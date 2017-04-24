/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010 Apple Inc. All rights
 * reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/loader/ImageLoader.h"

#include "bindings/core/v8/Microtask.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/IncrementLoadEventDelayCount.h"
#include "core/events/Event.h"
#include "core/events/EventSender.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/CrossOriginAttribute.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/layout/LayoutImage.h"
#include "core/layout/LayoutVideo.h"
#include "core/layout/svg/LayoutSVGImage.h"
#include "core/svg/graphics/SVGImage.h"
#include "platform/loader/fetch/FetchRequest.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoadingLog.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

static ImageEventSender& loadEventSender() {
  DEFINE_STATIC_LOCAL(ImageEventSender, sender,
                      (ImageEventSender::create(EventTypeNames::load)));
  return sender;
}

static ImageEventSender& errorEventSender() {
  DEFINE_STATIC_LOCAL(ImageEventSender, sender,
                      (ImageEventSender::create(EventTypeNames::error)));
  return sender;
}

static inline bool pageIsBeingDismissed(Document* document) {
  return document->pageDismissalEventBeingDispatched() != Document::NoDismissal;
}

static ImageLoader::BypassMainWorldBehavior shouldBypassMainWorldCSP(
    ImageLoader* loader) {
  DCHECK(loader);
  DCHECK(loader->element());
  if (loader->element()->document().frame() &&
      loader->element()
          ->document()
          .frame()
          ->script()
          .shouldBypassMainWorldCSP())
    return ImageLoader::BypassMainWorldCSP;
  return ImageLoader::DoNotBypassMainWorldCSP;
}

class ImageLoader::Task {
 public:
  static std::unique_ptr<Task> create(ImageLoader* loader,
                                      UpdateFromElementBehavior updateBehavior,
                                      ReferrerPolicy referrerPolicy) {
    return WTF::makeUnique<Task>(loader, updateBehavior, referrerPolicy);
  }

  Task(ImageLoader* loader,
       UpdateFromElementBehavior updateBehavior,
       ReferrerPolicy referrerPolicy)
      : m_loader(loader),
        m_shouldBypassMainWorldCSP(shouldBypassMainWorldCSP(loader)),
        m_updateBehavior(updateBehavior),
        m_weakFactory(this),
        m_referrerPolicy(referrerPolicy) {
    ExecutionContext& context = m_loader->element()->document();
    probe::asyncTaskScheduled(&context, "Image", this);
    v8::Isolate* isolate = V8PerIsolateData::mainThreadIsolate();
    v8::HandleScope scope(isolate);
    // If we're invoked from C++ without a V8 context on the stack, we should
    // run the microtask in the context of the element's document's main world.
    if (!isolate->GetCurrentContext().IsEmpty()) {
      m_scriptState = ScriptState::current(isolate);
    } else {
      m_scriptState =
          ScriptState::forMainWorld(loader->element()->document().frame());
      DCHECK(m_scriptState);
    }
    m_requestURL =
        loader->imageSourceToKURL(loader->element()->imageSourceURL());
  }

  void run() {
    if (!m_loader)
      return;
    ExecutionContext& context = m_loader->element()->document();
    probe::AsyncTask asyncTask(&context, this);
    if (m_scriptState->contextIsValid()) {
      ScriptState::Scope scope(m_scriptState.get());
      m_loader->doUpdateFromElement(m_shouldBypassMainWorldCSP,
                                    m_updateBehavior, m_requestURL,
                                    m_referrerPolicy);
    } else {
      m_loader->doUpdateFromElement(m_shouldBypassMainWorldCSP,
                                    m_updateBehavior, m_requestURL,
                                    m_referrerPolicy);
    }
  }

  void clearLoader() {
    m_loader = nullptr;
    m_scriptState.clear();
  }

  WeakPtr<Task> createWeakPtr() { return m_weakFactory.createWeakPtr(); }

 private:
  WeakPersistent<ImageLoader> m_loader;
  BypassMainWorldBehavior m_shouldBypassMainWorldCSP;
  UpdateFromElementBehavior m_updateBehavior;
  RefPtr<ScriptState> m_scriptState;
  WeakPtrFactory<Task> m_weakFactory;
  ReferrerPolicy m_referrerPolicy;
  KURL m_requestURL;
};

ImageLoader::ImageLoader(Element* element)
    : m_element(element),
      m_derefElementTimer(this, &ImageLoader::timerFired),
      m_hasPendingLoadEvent(false),
      m_hasPendingErrorEvent(false),
      m_imageComplete(true),
      m_loadingImageDocument(false),
      m_elementIsProtected(false),
      m_suppressErrorEvents(false) {
  RESOURCE_LOADING_DVLOG(1) << "new ImageLoader " << this;
}

ImageLoader::~ImageLoader() {}

void ImageLoader::dispose() {
  RESOURCE_LOADING_DVLOG(1)
      << "~ImageLoader " << this
      << "; m_hasPendingLoadEvent=" << m_hasPendingLoadEvent
      << ", m_hasPendingErrorEvent=" << m_hasPendingErrorEvent;

  if (m_image) {
    m_image->removeObserver(this);
    m_image = nullptr;
  }
}

DEFINE_TRACE(ImageLoader) {
  visitor->trace(m_image);
  visitor->trace(m_imageResourceForImageDocument);
  visitor->trace(m_element);
}

void ImageLoader::setImage(ImageResourceContent* newImage) {
  setImageWithoutConsideringPendingLoadEvent(newImage);

  // Only consider updating the protection ref-count of the Element immediately
  // before returning from this function as doing so might result in the
  // destruction of this ImageLoader.
  updatedHasPendingEvent();
}

void ImageLoader::setImageWithoutConsideringPendingLoadEvent(
    ImageResourceContent* newImage) {
  DCHECK(m_failedLoadURL.isEmpty());
  ImageResourceContent* oldImage = m_image.get();
  if (newImage != oldImage) {
    m_image = newImage;
    if (m_hasPendingLoadEvent) {
      loadEventSender().cancelEvent(this);
      m_hasPendingLoadEvent = false;
    }
    if (m_hasPendingErrorEvent) {
      errorEventSender().cancelEvent(this);
      m_hasPendingErrorEvent = false;
    }
    m_imageComplete = true;
    if (newImage) {
      newImage->addObserver(this);
    }
    if (oldImage) {
      oldImage->removeObserver(this);
    }
  }

  if (LayoutImageResource* imageResource = layoutImageResource())
    imageResource->resetAnimation();
}

static void configureRequest(
    FetchRequest& request,
    ImageLoader::BypassMainWorldBehavior bypassBehavior,
    Element& element,
    const ClientHintsPreferences& clientHintsPreferences) {
  if (bypassBehavior == ImageLoader::BypassMainWorldCSP)
    request.setContentSecurityCheck(DoNotCheckContentSecurityPolicy);

  CrossOriginAttributeValue crossOrigin = crossOriginAttributeValue(
      element.fastGetAttribute(HTMLNames::crossoriginAttr));
  if (crossOrigin != CrossOriginAttributeNotSet) {
    request.setCrossOriginAccessControl(element.document().getSecurityOrigin(),
                                        crossOrigin);
  }

  if (clientHintsPreferences.shouldSendResourceWidth() &&
      isHTMLImageElement(element))
    request.setResourceWidth(toHTMLImageElement(element).getResourceWidth());
}

inline void ImageLoader::dispatchErrorEvent() {
  m_hasPendingErrorEvent = true;
  errorEventSender().dispatchEventSoon(this);
}

inline void ImageLoader::crossSiteOrCSPViolationOccurred(
    AtomicString imageSourceURL) {
  m_failedLoadURL = imageSourceURL;
}

inline void ImageLoader::clearFailedLoadURL() {
  m_failedLoadURL = AtomicString();
}

inline void ImageLoader::enqueueImageLoadingMicroTask(
    UpdateFromElementBehavior updateBehavior,
    ReferrerPolicy referrerPolicy) {
  std::unique_ptr<Task> task =
      Task::create(this, updateBehavior, referrerPolicy);
  m_pendingTask = task->createWeakPtr();
  Microtask::enqueueMicrotask(
      WTF::bind(&Task::run, WTF::passed(std::move(task))));
  m_loadDelayCounter =
      IncrementLoadEventDelayCount::create(m_element->document());
}

void ImageLoader::doUpdateFromElement(BypassMainWorldBehavior bypassBehavior,
                                      UpdateFromElementBehavior updateBehavior,
                                      const KURL& url,
                                      ReferrerPolicy referrerPolicy) {
  // FIXME: According to
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/embedded-content.html#the-img-element:the-img-element-55
  // When "update image" is called due to environment changes and the load
  // fails, onerror should not be called. That is currently not the case.
  //
  // We don't need to call clearLoader here: Either we were called from the
  // task, or our caller updateFromElement cleared the task's loader (and set
  // m_pendingTask to null).
  m_pendingTask.reset();
  // Make sure to only decrement the count when we exit this function
  std::unique_ptr<IncrementLoadEventDelayCount> loadDelayCounter;
  loadDelayCounter.swap(m_loadDelayCounter);

  Document& document = m_element->document();
  if (!document.isActive())
    return;

  AtomicString imageSourceURL = m_element->imageSourceURL();
  ImageResourceContent* newImage = nullptr;
  if (!url.isNull()) {
    // Unlike raw <img>, we block mixed content inside of <picture> or
    // <img srcset>.
    ResourceLoaderOptions resourceLoaderOptions =
        ResourceFetcher::defaultResourceOptions();
    ResourceRequest resourceRequest(url);
    if (updateBehavior == UpdateForcedReload) {
      resourceRequest.setCachePolicy(WebCachePolicy::BypassingCache);
      resourceRequest.setPreviewsState(WebURLRequest::PreviewsNoTransform);
    }

    if (referrerPolicy != ReferrerPolicyDefault) {
      resourceRequest.setHTTPReferrer(SecurityPolicy::generateReferrer(
          referrerPolicy, url, document.outgoingReferrer()));
    }

    if (isHTMLPictureElement(element()->parentNode()) ||
        !element()->fastGetAttribute(HTMLNames::srcsetAttr).isNull())
      resourceRequest.setRequestContext(WebURLRequest::RequestContextImageSet);
    FetchRequest request(resourceRequest, element()->localName(),
                         resourceLoaderOptions);
    configureRequest(request, bypassBehavior, *m_element,
                     document.clientHintsPreferences());

    if (updateBehavior != UpdateForcedReload && document.settings() &&
        document.settings()->getFetchImagePlaceholders()) {
      request.setAllowImagePlaceholder();
    }

    newImage = ImageResourceContent::fetch(request, document.fetcher());

    if (!newImage && !pageIsBeingDismissed(&document)) {
      crossSiteOrCSPViolationOccurred(imageSourceURL);
      dispatchErrorEvent();
    } else {
      clearFailedLoadURL();
    }
  } else {
    if (!imageSourceURL.isNull()) {
      // Fire an error event if the url string is not empty, but the KURL is.
      dispatchErrorEvent();
    }
    noImageResourceToLoad();
  }

  ImageResourceContent* oldImage = m_image.get();
  if (updateBehavior == UpdateSizeChanged && m_element->layoutObject() &&
      m_element->layoutObject()->isImage() && newImage == oldImage) {
    toLayoutImage(m_element->layoutObject())->intrinsicSizeChanged();
  } else {
    if (m_hasPendingLoadEvent) {
      loadEventSender().cancelEvent(this);
      m_hasPendingLoadEvent = false;
    }

    // Cancel error events that belong to the previous load, which is now
    // cancelled by changing the src attribute. If newImage is null and
    // m_hasPendingErrorEvent is true, we know the error event has been just
    // posted by this load and we should not cancel the event.
    // FIXME: If both previous load and this one got blocked with an error, we
    // can receive one error event instead of two.
    if (m_hasPendingErrorEvent && newImage) {
      errorEventSender().cancelEvent(this);
      m_hasPendingErrorEvent = false;
    }

    m_image = newImage;
    m_hasPendingLoadEvent = newImage;
    m_imageComplete = !newImage;

    updateLayoutObject();
    // If newImage exists and is cached, addObserver() will result in the load
    // event being queued to fire. Ensure this happens after beforeload is
    // dispatched.
    if (newImage) {
      newImage->addObserver(this);
    }
    if (oldImage) {
      oldImage->removeObserver(this);
    }
  }

  if (LayoutImageResource* imageResource = layoutImageResource())
    imageResource->resetAnimation();

  // Only consider updating the protection ref-count of the Element immediately
  // before returning from this function as doing so might result in the
  // destruction of this ImageLoader.
  updatedHasPendingEvent();
}

void ImageLoader::updateFromElement(UpdateFromElementBehavior updateBehavior,
                                    ReferrerPolicy referrerPolicy) {
  AtomicString imageSourceURL = m_element->imageSourceURL();
  m_suppressErrorEvents = (updateBehavior == UpdateSizeChanged);

  if (updateBehavior == UpdateIgnorePreviousError)
    clearFailedLoadURL();

  if (!m_failedLoadURL.isEmpty() && imageSourceURL == m_failedLoadURL)
    return;

  // Prevent the creation of a ResourceLoader (and therefore a network request)
  // for ImageDocument loads. In this case, the image contents have already been
  // requested as a main resource and ImageDocumentParser will take care of
  // funneling the main resource bytes into m_image, so just create an
  // ImageResource to be populated later.
  if (m_loadingImageDocument && updateBehavior != UpdateForcedReload) {
    ImageResource* imageResource =
        ImageResource::create(imageSourceToKURL(m_element->imageSourceURL()));
    imageResource->setStatus(ResourceStatus::Pending);
    m_imageResourceForImageDocument = imageResource;
    setImage(imageResource->getContent());
    return;
  }

  // If we have a pending task, we have to clear it -- either we're now loading
  // immediately, or we need to reset the task's state.
  if (m_pendingTask) {
    m_pendingTask->clearLoader();
    m_pendingTask.reset();
  }

  KURL url = imageSourceToKURL(imageSourceURL);
  if (shouldLoadImmediately(url)) {
    doUpdateFromElement(DoNotBypassMainWorldCSP, updateBehavior, url,
                        referrerPolicy);
    return;
  }
  // Allow the idiom "img.src=''; img.src='.." to clear down the image before an
  // asynchronous load completes.
  if (imageSourceURL.isEmpty()) {
    ImageResourceContent* image = m_image.get();
    if (image) {
      image->removeObserver(this);
    }
    m_image = nullptr;
  }

  // Don't load images for inactive documents. We don't want to slow down the
  // raw HTML parsing case by loading images we don't intend to display.
  Document& document = m_element->document();
  if (document.isActive())
    enqueueImageLoadingMicroTask(updateBehavior, referrerPolicy);
}

KURL ImageLoader::imageSourceToKURL(AtomicString imageSourceURL) const {
  KURL url;

  // Don't load images for inactive documents. We don't want to slow down the
  // raw HTML parsing case by loading images we don't intend to display.
  Document& document = m_element->document();
  if (!document.isActive())
    return url;

  // Do not load any image if the 'src' attribute is missing or if it is
  // an empty string.
  if (!imageSourceURL.isNull()) {
    String strippedImageSourceURL =
        stripLeadingAndTrailingHTMLSpaces(imageSourceURL);
    if (!strippedImageSourceURL.isEmpty())
      url = document.completeURL(strippedImageSourceURL);
  }
  return url;
}

bool ImageLoader::shouldLoadImmediately(const KURL& url) const {
  // We force any image loads which might require alt content through the
  // asynchronous path so that we can add the shadow DOM for the alt-text
  // content when style recalc is over and DOM mutation is allowed again.
  if (!url.isNull()) {
    Resource* resource = memoryCache()->resourceForURL(
        url, m_element->document().fetcher()->getCacheIdentifier());
    if (resource && !resource->errorOccurred())
      return true;
  }
  return (isHTMLObjectElement(m_element) || isHTMLEmbedElement(m_element));
}

void ImageLoader::imageNotifyFinished(ImageResourceContent* resource) {
  RESOURCE_LOADING_DVLOG(1)
      << "ImageLoader::imageNotifyFinished " << this
      << "; m_hasPendingLoadEvent=" << m_hasPendingLoadEvent;

  DCHECK(m_failedLoadURL.isEmpty());
  DCHECK_EQ(resource, m_image.get());

  m_imageComplete = true;

  // Update ImageAnimationPolicy for m_image.
  if (m_image)
    m_image->updateImageAnimationPolicy();

  updateLayoutObject();

  if (m_image && m_image->getImage() && m_image->getImage()->isSVGImage())
    toSVGImage(m_image->getImage())->updateUseCounters(element()->document());

  if (!m_hasPendingLoadEvent)
    return;

  if (resource->errorOccurred()) {
    loadEventSender().cancelEvent(this);
    m_hasPendingLoadEvent = false;

    if (resource->resourceError().isAccessCheck()) {
      crossSiteOrCSPViolationOccurred(
          AtomicString(resource->resourceError().failingURL()));
    }

    // The error event should not fire if the image data update is a result of
    // environment change.
    // https://html.spec.whatwg.org/multipage/embedded-content.html#the-img-element:the-img-element-55
    if (!m_suppressErrorEvents)
      dispatchErrorEvent();

    // Only consider updating the protection ref-count of the Element
    // immediately before returning from this function as doing so might result
    // in the destruction of this ImageLoader.
    updatedHasPendingEvent();
    return;
  }
  loadEventSender().dispatchEventSoon(this);
}

LayoutImageResource* ImageLoader::layoutImageResource() {
  LayoutObject* layoutObject = m_element->layoutObject();

  if (!layoutObject)
    return 0;

  // We don't return style generated image because it doesn't belong to the
  // ImageLoader. See <https://bugs.webkit.org/show_bug.cgi?id=42840>
  if (layoutObject->isImage() &&
      !static_cast<LayoutImage*>(layoutObject)->isGeneratedContent())
    return toLayoutImage(layoutObject)->imageResource();

  if (layoutObject->isSVGImage())
    return toLayoutSVGImage(layoutObject)->imageResource();

  if (layoutObject->isVideo())
    return toLayoutVideo(layoutObject)->imageResource();

  return 0;
}

void ImageLoader::updateLayoutObject() {
  LayoutImageResource* imageResource = layoutImageResource();

  if (!imageResource)
    return;

  // Only update the layoutObject if it doesn't have an image or if what we have
  // is a complete image.  This prevents flickering in the case where a dynamic
  // change is happening between two images.
  ImageResourceContent* cachedImage = imageResource->cachedImage();
  if (m_image != cachedImage && (m_imageComplete || !cachedImage))
    imageResource->setImageResource(m_image.get());
}

void ImageLoader::updatedHasPendingEvent() {
  // If an Element that does image loading is removed from the DOM the
  // load/error event for the image is still observable. As long as the
  // ImageLoader is actively loading, the Element itself needs to be ref'ed to
  // keep it from being destroyed by DOM manipulation or garbage collection. If
  // such an Element wishes for the load to stop when removed from the DOM it
  // needs to stop the ImageLoader explicitly.
  bool wasProtected = m_elementIsProtected;
  m_elementIsProtected = m_hasPendingLoadEvent || m_hasPendingErrorEvent;
  if (wasProtected == m_elementIsProtected)
    return;

  if (m_elementIsProtected) {
    if (m_derefElementTimer.isActive())
      m_derefElementTimer.stop();
    else
      m_keepAlive = m_element;
  } else {
    DCHECK(!m_derefElementTimer.isActive());
    m_derefElementTimer.startOneShot(0, BLINK_FROM_HERE);
  }
}

void ImageLoader::timerFired(TimerBase*) {
  m_keepAlive.clear();
}

void ImageLoader::dispatchPendingEvent(ImageEventSender* eventSender) {
  RESOURCE_LOADING_DVLOG(1) << "ImageLoader::dispatchPendingEvent " << this;
  DCHECK(eventSender == &loadEventSender() ||
         eventSender == &errorEventSender());
  const AtomicString& eventType = eventSender->eventType();
  if (eventType == EventTypeNames::load)
    dispatchPendingLoadEvent();
  if (eventType == EventTypeNames::error)
    dispatchPendingErrorEvent();
}

void ImageLoader::dispatchPendingLoadEvent() {
  if (!m_hasPendingLoadEvent)
    return;
  if (!m_image)
    return;
  m_hasPendingLoadEvent = false;
  if (element()->document().frame())
    dispatchLoadEvent();

  // Only consider updating the protection ref-count of the Element immediately
  // before returning from this function as doing so might result in the
  // destruction of this ImageLoader.
  updatedHasPendingEvent();
}

void ImageLoader::dispatchPendingErrorEvent() {
  if (!m_hasPendingErrorEvent)
    return;
  m_hasPendingErrorEvent = false;

  if (element()->document().frame())
    element()->dispatchEvent(Event::create(EventTypeNames::error));

  // Only consider updating the protection ref-count of the Element immediately
  // before returning from this function as doing so might result in the
  // destruction of this ImageLoader.
  updatedHasPendingEvent();
}

bool ImageLoader::getImageAnimationPolicy(ImageAnimationPolicy& policy) {
  if (!element()->document().settings())
    return false;

  policy = element()->document().settings()->getImageAnimationPolicy();
  return true;
}

void ImageLoader::dispatchPendingLoadEvents() {
  loadEventSender().dispatchPendingEvents();
}

void ImageLoader::dispatchPendingErrorEvents() {
  errorEventSender().dispatchPendingEvents();
}

void ImageLoader::elementDidMoveToNewDocument() {
  if (m_loadDelayCounter)
    m_loadDelayCounter->documentChanged(m_element->document());
  clearFailedLoadURL();
  setImage(0);
}

}  // namespace blink
