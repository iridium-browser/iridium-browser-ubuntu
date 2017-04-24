/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "core/html/HTMLImageElement.h"

#include "bindings/core/v8/ScriptEventListener.h"
#include "core/CSSPropertyNames.h"
#include "core/HTMLNames.h"
#include "core/MediaTypeNames.h"
#include "core/css/MediaQueryMatcher.h"
#include "core/css/MediaValuesDynamic.h"
#include "core/css/parser/SizesAttributeParser.h"
#include "core/dom/Attribute.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/frame/Deprecation.h"
#include "core/frame/ImageBitmap.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/html/FormAssociated.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLImageFallbackHelper.h"
#include "core/html/HTMLPictureElement.h"
#include "core/html/HTMLSourceElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/parser/HTMLSrcsetParser.h"
#include "core/imagebitmap/ImageBitmapOptions.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutImage.h"
#include "core/layout/api/LayoutImageItem.h"
#include "core/loader/resource/ImageResourceContent.h"
#include "core/page/Page.h"
#include "core/style/ContentData.h"
#include "core/svg/graphics/SVGImageForContainer.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/network/mime/ContentType.h"
#include "platform/network/mime/MIMETypeRegistry.h"
#include "platform/weborigin/SecurityPolicy.h"

namespace blink {

using namespace HTMLNames;

class HTMLImageElement::ViewportChangeListener final
    : public MediaQueryListListener {
 public:
  static ViewportChangeListener* create(HTMLImageElement* element) {
    return new ViewportChangeListener(element);
  }

  void notifyMediaQueryChanged() override {
    if (m_element)
      m_element->notifyViewportChanged();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_element);
    MediaQueryListListener::trace(visitor);
  }

 private:
  explicit ViewportChangeListener(HTMLImageElement* element)
      : m_element(element) {}
  Member<HTMLImageElement> m_element;
};

HTMLImageElement::HTMLImageElement(Document& document, bool createdByParser)
    : HTMLElement(imgTag, document),
      m_imageLoader(HTMLImageLoader::create(this)),
      m_imageDevicePixelRatio(1.0f),
      m_source(nullptr),
      m_layoutDisposition(LayoutDisposition::PrimaryContent),
      m_formWasSetByParser(false),
      m_elementCreatedByParser(createdByParser),
      m_isFallbackImage(false),
      m_referrerPolicy(ReferrerPolicyDefault) {
  setHasCustomStyleCallbacks();
}

HTMLImageElement* HTMLImageElement::create(Document& document) {
  return new HTMLImageElement(document);
}

HTMLImageElement* HTMLImageElement::create(Document& document,
                                           bool createdByParser) {
  return new HTMLImageElement(document, createdByParser);
}

HTMLImageElement::~HTMLImageElement() {}

DEFINE_TRACE(HTMLImageElement) {
  visitor->trace(m_imageLoader);
  visitor->trace(m_listener);
  visitor->trace(m_form);
  visitor->trace(m_source);
  HTMLElement::trace(visitor);
}

void HTMLImageElement::notifyViewportChanged() {
  // Re-selecting the source URL in order to pick a more fitting resource
  // And update the image's intrinsic dimensions when the viewport changes.
  // Picking of a better fitting resource is UA dependant, not spec required.
  selectSourceURL(ImageLoader::UpdateSizeChanged);
}

HTMLImageElement* HTMLImageElement::createForJSConstructor(Document& document) {
  HTMLImageElement* image = new HTMLImageElement(document);
  image->m_elementCreatedByParser = false;
  return image;
}

HTMLImageElement* HTMLImageElement::createForJSConstructor(Document& document,
                                                           unsigned width) {
  HTMLImageElement* image = new HTMLImageElement(document);
  image->setWidth(width);
  image->m_elementCreatedByParser = false;
  return image;
}

HTMLImageElement* HTMLImageElement::createForJSConstructor(Document& document,
                                                           unsigned width,
                                                           unsigned height) {
  HTMLImageElement* image = new HTMLImageElement(document);
  image->setWidth(width);
  image->setHeight(height);
  image->m_elementCreatedByParser = false;
  return image;
}

bool HTMLImageElement::isPresentationAttribute(
    const QualifiedName& name) const {
  if (name == widthAttr || name == heightAttr || name == borderAttr ||
      name == vspaceAttr || name == hspaceAttr || name == alignAttr ||
      name == valignAttr)
    return true;
  return HTMLElement::isPresentationAttribute(name);
}

void HTMLImageElement::collectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableStylePropertySet* style) {
  if (name == widthAttr) {
    addHTMLLengthToStyle(style, CSSPropertyWidth, value);
  } else if (name == heightAttr) {
    addHTMLLengthToStyle(style, CSSPropertyHeight, value);
  } else if (name == borderAttr) {
    applyBorderAttributeToStyle(value, style);
  } else if (name == vspaceAttr) {
    addHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
    addHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
  } else if (name == hspaceAttr) {
    addHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
    addHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
  } else if (name == alignAttr) {
    applyAlignmentAttributeToStyle(value, style);
  } else if (name == valignAttr) {
    addPropertyToPresentationAttributeStyle(style, CSSPropertyVerticalAlign,
                                            value);
  } else {
    HTMLElement::collectStyleForPresentationAttribute(name, value, style);
  }
}

const AtomicString HTMLImageElement::imageSourceURL() const {
  return m_bestFitImageURL.isNull() ? fastGetAttribute(srcAttr)
                                    : m_bestFitImageURL;
}

HTMLFormElement* HTMLImageElement::formOwner() const {
  return m_form.get();
}

void HTMLImageElement::formRemovedFromTree(const Node& formRoot) {
  DCHECK(m_form);
  if (NodeTraversal::highestAncestorOrSelf(*this) != formRoot)
    resetFormOwner();
}

void HTMLImageElement::resetFormOwner() {
  m_formWasSetByParser = false;
  HTMLFormElement* nearestForm = findFormAncestor();
  if (m_form) {
    if (nearestForm == m_form.get())
      return;
    m_form->disassociate(*this);
  }
  if (nearestForm) {
    m_form = nearestForm;
    m_form->associate(*this);
  } else {
    m_form = nullptr;
  }
}

void HTMLImageElement::setBestFitURLAndDPRFromImageCandidate(
    const ImageCandidate& candidate) {
  m_bestFitImageURL = candidate.url();
  float candidateDensity = candidate.density();
  float oldImageDevicePixelRatio = m_imageDevicePixelRatio;
  if (candidateDensity >= 0)
    m_imageDevicePixelRatio = 1.0 / candidateDensity;

  bool intrinsicSizingViewportDependant = false;
  if (candidate.getResourceWidth() > 0) {
    intrinsicSizingViewportDependant = true;
    UseCounter::count(document(), UseCounter::SrcsetWDescriptor);
  } else if (!candidate.srcOrigin()) {
    UseCounter::count(document(), UseCounter::SrcsetXDescriptor);
  }
  if (layoutObject() && layoutObject()->isImage()) {
    LayoutImageItem(toLayoutImage(layoutObject()))
        .setImageDevicePixelRatio(m_imageDevicePixelRatio);

    if (oldImageDevicePixelRatio != m_imageDevicePixelRatio)
      toLayoutImage(layoutObject())->intrinsicSizeChanged();
  }

  if (intrinsicSizingViewportDependant) {
    if (!m_listener)
      m_listener = ViewportChangeListener::create(this);

    document().mediaQueryMatcher().addViewportListener(m_listener);
  } else if (m_listener) {
    document().mediaQueryMatcher().removeViewportListener(m_listener);
  }
}

void HTMLImageElement::parseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  if (name == altAttr || name == titleAttr) {
    if (userAgentShadowRoot()) {
      Element* text = userAgentShadowRoot()->getElementById("alttext");
      String value = altText();
      if (text && text->textContent() != params.newValue)
        text->setTextContent(altText());
    }
  } else if (name == srcAttr || name == srcsetAttr || name == sizesAttr) {
    selectSourceURL(ImageLoader::UpdateIgnorePreviousError);
  } else if (name == usemapAttr) {
    setIsLink(!params.newValue.isNull());
  } else if (name == referrerpolicyAttr) {
    m_referrerPolicy = ReferrerPolicyDefault;
    if (!params.newValue.isNull()) {
      SecurityPolicy::referrerPolicyFromString(
          params.newValue, SupportReferrerPolicyLegacyKeywords,
          &m_referrerPolicy);
      UseCounter::count(document(),
                        UseCounter::HTMLImageElementReferrerPolicyAttribute);
    }
  } else {
    HTMLElement::parseAttribute(params);
  }
}

String HTMLImageElement::altText() const {
  // lets figure out the alt text.. magic stuff
  // http://www.w3.org/TR/1998/REC-html40-19980424/appendix/notes.html#altgen
  // also heavily discussed by Hixie on bugzilla
  const AtomicString& alt = fastGetAttribute(altAttr);
  if (!alt.isNull())
    return alt;
  // fall back to title attribute
  return fastGetAttribute(titleAttr);
}

static bool supportedImageType(const String& type) {
  String trimmedType = ContentType(type).type();
  // An empty type attribute is implicitly supported.
  if (trimmedType.isEmpty())
    return true;
  return MIMETypeRegistry::isSupportedImagePrefixedMIMEType(trimmedType);
}

// http://picture.responsiveimages.org/#update-source-set
ImageCandidate HTMLImageElement::findBestFitImageFromPictureParent() {
  DCHECK(isMainThread());
  Node* parent = parentNode();
  m_source = nullptr;
  if (!parent || !isHTMLPictureElement(*parent))
    return ImageCandidate();
  for (Node* child = parent->firstChild(); child;
       child = child->nextSibling()) {
    if (child == this)
      return ImageCandidate();

    if (!isHTMLSourceElement(*child))
      continue;

    HTMLSourceElement* source = toHTMLSourceElement(child);
    if (!source->fastGetAttribute(srcAttr).isNull())
      Deprecation::countDeprecation(document(), UseCounter::PictureSourceSrc);
    String srcset = source->fastGetAttribute(srcsetAttr);
    if (srcset.isEmpty())
      continue;
    String type = source->fastGetAttribute(typeAttr);
    if (!type.isEmpty() && !supportedImageType(type))
      continue;

    if (!source->mediaQueryMatches())
      continue;

    ImageCandidate candidate = bestFitSourceForSrcsetAttribute(
        document().devicePixelRatio(), sourceSize(*source),
        source->fastGetAttribute(srcsetAttr), &document());
    if (candidate.isEmpty())
      continue;
    m_source = source;
    return candidate;
  }
  return ImageCandidate();
}

LayoutObject* HTMLImageElement::createLayoutObject(const ComputedStyle& style) {
  const ContentData* contentData = style.contentData();
  if (contentData && contentData->isImage()) {
    const StyleImage* contentImage = toImageContentData(contentData)->image();
    bool errorOccurred = contentImage && contentImage->cachedImage() &&
                         contentImage->cachedImage()->errorOccurred();
    if (!errorOccurred)
      return LayoutObject::createObject(this, style);
  }

  switch (m_layoutDisposition) {
    case LayoutDisposition::FallbackContent:
      return new LayoutBlockFlow(this);
    case LayoutDisposition::PrimaryContent: {
      LayoutImage* image = new LayoutImage(this);
      image->setImageResource(LayoutImageResource::create());
      image->setImageDevicePixelRatio(m_imageDevicePixelRatio);
      return image;
    }
    case LayoutDisposition::Collapsed:  // Falls through.
    default:
      NOTREACHED();
      return nullptr;
  }
}

void HTMLImageElement::attachLayoutTree(const AttachContext& context) {
  HTMLElement::attachLayoutTree(context);
  if (layoutObject() && layoutObject()->isImage()) {
    LayoutImage* layoutImage = toLayoutImage(layoutObject());
    LayoutImageResource* layoutImageResource = layoutImage->imageResource();
    if (m_isFallbackImage) {
      float deviceScaleFactor =
          blink::deviceScaleFactorDeprecated(layoutImage->frame());
      std::pair<Image*, float> brokenImageAndImageScaleFactor =
          ImageResourceContent::brokenImage(deviceScaleFactor);
      ImageResourceContent* newImageResource =
          ImageResourceContent::create(brokenImageAndImageScaleFactor.first);
      layoutImage->imageResource()->setImageResource(newImageResource);
    }
    if (layoutImageResource->hasImage())
      return;

    if (!imageLoader().image() && !layoutImageResource->cachedImage())
      return;
    layoutImageResource->setImageResource(imageLoader().image());
  }
}

Node::InsertionNotificationRequest HTMLImageElement::insertedInto(
    ContainerNode* insertionPoint) {
  if (!m_formWasSetByParser ||
      NodeTraversal::highestAncestorOrSelf(*insertionPoint) !=
          NodeTraversal::highestAncestorOrSelf(*m_form.get()))
    resetFormOwner();
  if (m_listener)
    document().mediaQueryMatcher().addViewportListener(m_listener);
  Node* parent = parentNode();
  if (parent && isHTMLPictureElement(*parent))
    toHTMLPictureElement(parent)->addListenerToSourceChildren();

  bool imageWasModified = false;
  if (document().isActive()) {
    ImageCandidate candidate = findBestFitImageFromPictureParent();
    if (!candidate.isEmpty()) {
      setBestFitURLAndDPRFromImageCandidate(candidate);
      imageWasModified = true;
    }
  }

  // If we have been inserted from a layoutObject-less document,
  // our loader may have not fetched the image, so do it now.
  if ((insertionPoint->isConnected() && !imageLoader().image()) ||
      imageWasModified)
    imageLoader().updateFromElement(ImageLoader::UpdateNormal,
                                    m_referrerPolicy);

  return HTMLElement::insertedInto(insertionPoint);
}

void HTMLImageElement::removedFrom(ContainerNode* insertionPoint) {
  if (!m_form ||
      NodeTraversal::highestAncestorOrSelf(*m_form.get()) !=
          NodeTraversal::highestAncestorOrSelf(*this))
    resetFormOwner();
  if (m_listener) {
    document().mediaQueryMatcher().removeViewportListener(m_listener);
    Node* parent = parentNode();
    if (parent && isHTMLPictureElement(*parent))
      toHTMLPictureElement(parent)->removeListenerFromSourceChildren();
  }
  HTMLElement::removedFrom(insertionPoint);
}

unsigned HTMLImageElement::width() {
  if (inActiveDocument())
    document().updateStyleAndLayoutIgnorePendingStylesheets();

  if (!layoutObject()) {
    // check the attribute first for an explicit pixel value
    unsigned width = 0;
    if (parseHTMLNonNegativeInteger(getAttribute(widthAttr), width))
      return width;

    // if the image is available, use its width
    if (imageLoader().image()) {
      return imageLoader()
          .image()
          ->imageSize(LayoutObject::shouldRespectImageOrientation(nullptr),
                      1.0f)
          .width()
          .toUnsigned();
    }
  }

  return layoutBoxWidth();
}

unsigned HTMLImageElement::height() {
  if (inActiveDocument())
    document().updateStyleAndLayoutIgnorePendingStylesheets();

  if (!layoutObject()) {
    // check the attribute first for an explicit pixel value
    unsigned height = 0;
    if (parseHTMLNonNegativeInteger(getAttribute(heightAttr), height))
      return height;

    // if the image is available, use its height
    if (imageLoader().image()) {
      return imageLoader()
          .image()
          ->imageSize(LayoutObject::shouldRespectImageOrientation(nullptr),
                      1.0f)
          .height()
          .toUnsigned();
    }
  }

  return layoutBoxHeight();
}

unsigned HTMLImageElement::naturalWidth() const {
  if (!imageLoader().image())
    return 0;

  return imageLoader()
      .image()
      ->imageSize(LayoutObject::shouldRespectImageOrientation(layoutObject()),
                  m_imageDevicePixelRatio,
                  ImageResourceContent::IntrinsicCorrectedToDPR)
      .width()
      .toUnsigned();
}

unsigned HTMLImageElement::naturalHeight() const {
  if (!imageLoader().image())
    return 0;

  return imageLoader()
      .image()
      ->imageSize(LayoutObject::shouldRespectImageOrientation(layoutObject()),
                  m_imageDevicePixelRatio,
                  ImageResourceContent::IntrinsicCorrectedToDPR)
      .height()
      .toUnsigned();
}

unsigned HTMLImageElement::layoutBoxWidth() const {
  LayoutBox* box = layoutBox();
  return box ? adjustForAbsoluteZoom(box->contentBoxRect().pixelSnappedWidth(),
                                     box)
             : 0;
}

unsigned HTMLImageElement::layoutBoxHeight() const {
  LayoutBox* box = layoutBox();
  return box ? adjustForAbsoluteZoom(box->contentBoxRect().pixelSnappedHeight(),
                                     box)
             : 0;
}

const String& HTMLImageElement::currentSrc() const {
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/edits.html#dom-img-currentsrc
  // The currentSrc IDL attribute must return the img element's current
  // request's current URL.

  // Return the picked URL string in case of load error.
  if (imageLoader().hadError())
    return m_bestFitImageURL;
  // Initially, the pending request turns into current request when it is either
  // available or broken.  We use the image's dimensions as a proxy to it being
  // in any of these states.
  if (!imageLoader().image() || !imageLoader().image()->getImage() ||
      !imageLoader().image()->getImage()->width())
    return emptyAtom;

  return imageLoader().image()->url().getString();
}

bool HTMLImageElement::isURLAttribute(const Attribute& attribute) const {
  return attribute.name() == srcAttr || attribute.name() == lowsrcAttr ||
         attribute.name() == longdescAttr ||
         (attribute.name() == usemapAttr && attribute.value()[0] != '#') ||
         HTMLElement::isURLAttribute(attribute);
}

bool HTMLImageElement::hasLegalLinkAttribute(const QualifiedName& name) const {
  return name == srcAttr || HTMLElement::hasLegalLinkAttribute(name);
}

const QualifiedName& HTMLImageElement::subResourceAttributeName() const {
  return srcAttr;
}

bool HTMLImageElement::draggable() const {
  // Image elements are draggable by default.
  return !equalIgnoringCase(getAttribute(draggableAttr), "false");
}

void HTMLImageElement::setHeight(unsigned value) {
  setUnsignedIntegralAttribute(heightAttr, value);
}

KURL HTMLImageElement::src() const {
  return document().completeURL(getAttribute(srcAttr));
}

void HTMLImageElement::setSrc(const String& value) {
  setAttribute(srcAttr, AtomicString(value));
}

void HTMLImageElement::setWidth(unsigned value) {
  setUnsignedIntegralAttribute(widthAttr, value);
}

int HTMLImageElement::x() const {
  document().updateStyleAndLayoutIgnorePendingStylesheets();
  LayoutObject* r = layoutObject();
  if (!r)
    return 0;

  // FIXME: This doesn't work correctly with transforms.
  FloatPoint absPos = r->localToAbsolute();
  return absPos.x();
}

int HTMLImageElement::y() const {
  document().updateStyleAndLayoutIgnorePendingStylesheets();
  LayoutObject* r = layoutObject();
  if (!r)
    return 0;

  // FIXME: This doesn't work correctly with transforms.
  FloatPoint absPos = r->localToAbsolute();
  return absPos.y();
}

bool HTMLImageElement::complete() const {
  return imageLoader().imageComplete();
}

void HTMLImageElement::didMoveToNewDocument(Document& oldDocument) {
  selectSourceURL(ImageLoader::UpdateIgnorePreviousError);
  imageLoader().elementDidMoveToNewDocument();
  HTMLElement::didMoveToNewDocument(oldDocument);
}

bool HTMLImageElement::isServerMap() const {
  if (!fastHasAttribute(ismapAttr))
    return false;

  const AtomicString& usemap = fastGetAttribute(usemapAttr);

  // If the usemap attribute starts with '#', it refers to a map element in the
  // document.
  if (usemap[0] == '#')
    return false;

  return document()
      .completeURL(stripLeadingAndTrailingHTMLSpaces(usemap))
      .isEmpty();
}

Image* HTMLImageElement::imageContents() {
  if (!imageLoader().imageComplete())
    return nullptr;

  return imageLoader().image()->getImage();
}

bool HTMLImageElement::isInteractiveContent() const {
  return fastHasAttribute(usemapAttr);
}

PassRefPtr<Image> HTMLImageElement::getSourceImageForCanvas(
    SourceImageStatus* status,
    AccelerationHint,
    SnapshotReason,
    const FloatSize& defaultObjectSize) const {
  if (!complete() || !cachedImage()) {
    *status = IncompleteSourceImageStatus;
    return nullptr;
  }

  if (cachedImage()->errorOccurred()) {
    *status = UndecodableSourceImageStatus;
    return nullptr;
  }

  RefPtr<Image> sourceImage;
  if (cachedImage()->getImage()->isSVGImage()) {
    UseCounter::count(document(), UseCounter::SVGInCanvas2D);
    SVGImage* svgImage = toSVGImage(cachedImage()->getImage());
    IntSize imageSize =
        roundedIntSize(svgImage->concreteObjectSize(defaultObjectSize));
    sourceImage = SVGImageForContainer::create(
        svgImage, imageSize, 1, document().completeURL(imageSourceURL()));
  } else {
    sourceImage = cachedImage()->getImage();
  }

  *status = NormalSourceImageStatus;
  return sourceImage->imageForDefaultFrame();
}

bool HTMLImageElement::isSVGSource() const {
  return cachedImage() && cachedImage()->getImage()->isSVGImage();
}

bool HTMLImageElement::wouldTaintOrigin(
    SecurityOrigin* destinationSecurityOrigin) const {
  ImageResourceContent* image = cachedImage();
  if (!image)
    return false;
  return !image->isAccessAllowed(destinationSecurityOrigin);
}

FloatSize HTMLImageElement::elementSize(
    const FloatSize& defaultObjectSize) const {
  ImageResourceContent* image = cachedImage();
  if (!image)
    return FloatSize();

  if (image->getImage() && image->getImage()->isSVGImage())
    return toSVGImage(cachedImage()->getImage())
        ->concreteObjectSize(defaultObjectSize);

  return FloatSize(image->imageSize(
      LayoutObject::shouldRespectImageOrientation(layoutObject()), 1.0f));
}

FloatSize HTMLImageElement::defaultDestinationSize(
    const FloatSize& defaultObjectSize) const {
  ImageResourceContent* image = cachedImage();
  if (!image)
    return FloatSize();

  if (image->getImage() && image->getImage()->isSVGImage())
    return toSVGImage(cachedImage()->getImage())
        ->concreteObjectSize(defaultObjectSize);

  LayoutSize size;
  size = image->imageSize(
      LayoutObject::shouldRespectImageOrientation(layoutObject()), 1.0f);
  if (layoutObject() && layoutObject()->isLayoutImage() && image->getImage() &&
      !image->getImage()->hasRelativeSize())
    size.scale(toLayoutImage(layoutObject())->imageDevicePixelRatio());
  return FloatSize(size);
}

static bool sourceSizeValue(Element& element,
                            Document& currentDocument,
                            float& sourceSize) {
  String sizes = element.fastGetAttribute(sizesAttr);
  bool exists = !sizes.isNull();
  if (exists)
    UseCounter::count(currentDocument, UseCounter::Sizes);
  sourceSize =
      SizesAttributeParser(MediaValuesDynamic::create(currentDocument), sizes)
          .length();
  return exists;
}

FetchRequest::ResourceWidth HTMLImageElement::getResourceWidth() {
  FetchRequest::ResourceWidth resourceWidth;
  Element* element = m_source.get();
  if (!element)
    element = this;
  resourceWidth.isSet =
      sourceSizeValue(*element, document(), resourceWidth.width);
  return resourceWidth;
}

float HTMLImageElement::sourceSize(Element& element) {
  float value;
  // We don't care here if the sizes attribute exists, so we ignore the return
  // value.  If it doesn't exist, we just return the default.
  sourceSizeValue(element, document(), value);
  return value;
}

void HTMLImageElement::forceReload() const {
  imageLoader().updateFromElement(ImageLoader::UpdateForcedReload,
                                  m_referrerPolicy);
}

ScriptPromise HTMLImageElement::createImageBitmap(
    ScriptState* scriptState,
    EventTarget& eventTarget,
    Optional<IntRect> cropRect,
    const ImageBitmapOptions& options,
    ExceptionState& exceptionState) {
  DCHECK(eventTarget.toLocalDOMWindow());
  if ((cropRect &&
       !ImageBitmap::isSourceSizeValid(cropRect->width(), cropRect->height(),
                                       exceptionState)) ||
      !ImageBitmap::isSourceSizeValid(bitmapSourceSize().width(),
                                      bitmapSourceSize().height(),
                                      exceptionState))
    return ScriptPromise();
  if (!ImageBitmap::isResizeOptionValid(options, exceptionState))
    return ScriptPromise();
  return ImageBitmapSource::fulfillImageBitmap(
      scriptState,
      ImageBitmap::create(this, cropRect,
                          eventTarget.toLocalDOMWindow()->document(), options));
}

void HTMLImageElement::selectSourceURL(
    ImageLoader::UpdateFromElementBehavior behavior) {
  if (!document().isActive())
    return;

  bool foundURL = false;
  ImageCandidate candidate = findBestFitImageFromPictureParent();
  if (!candidate.isEmpty()) {
    setBestFitURLAndDPRFromImageCandidate(candidate);
    foundURL = true;
  }

  if (!foundURL) {
    candidate = bestFitSourceForImageAttributes(
        document().devicePixelRatio(), sourceSize(*this),
        fastGetAttribute(srcAttr), fastGetAttribute(srcsetAttr), &document());
    setBestFitURLAndDPRFromImageCandidate(candidate);
  }

  imageLoader().updateFromElement(behavior, m_referrerPolicy);

  // Images such as data: uri's can return immediately and may already have
  // errored out.
  bool imageHasLoaded = imageLoader().image() &&
                        !imageLoader().image()->isLoading() &&
                        !imageLoader().image()->errorOccurred();
  bool imageStillLoading =
      !imageHasLoaded && imageLoader().hasPendingActivity() &&
      !imageLoader().hasPendingError() && !imageSourceURL().isEmpty();
  bool imageHasImage =
      imageLoader().image() && imageLoader().image()->hasImage();
  bool imageIsDocument = imageLoader().isLoadingImageDocument() &&
                         imageLoader().image() &&
                         !imageLoader().image()->errorOccurred();

  // Icky special case for deferred images:
  // A deferred image is not loading, does have pending activity, does not
  // have an error, but it does have an ImageResourceContent associated
  // with it, so imageHasLoaded will be true even though the image hasn't
  // actually loaded. Fixing the definition of imageHasLoaded isn't
  // sufficient, because a deferred image does have pending activity, does not
  // have a pending error, and does have a source URL, so if imageHasLoaded
  // was correct, imageStillLoading would become wrong.
  //
  // Instead of dealing with that, there's a separate check that the
  // ImageResourceContent has non-null image data associated with it, which
  // isn't folded into imageHasLoaded above.
  if ((imageHasLoaded && imageHasImage) || imageStillLoading || imageIsDocument)
    ensurePrimaryContent();
  else
    ensureCollapsedOrFallbackContent();
}

const KURL& HTMLImageElement::sourceURL() const {
  return cachedImage()->response().url();
}

void HTMLImageElement::didAddUserAgentShadowRoot(ShadowRoot&) {
  HTMLImageFallbackHelper::createAltTextShadowTree(*this);
}

void HTMLImageElement::ensureFallbackForGeneratedContent() {
  // The special casing for generated content in createLayoutObject breaks the
  // invariant that the layout object attached to this element will always be
  // appropriate for |m_layoutDisposition|. Force recreate it.
  // TODO(engedy): Remove this hack. See: https://crbug.com/671953.
  setLayoutDisposition(LayoutDisposition::FallbackContent,
                       true /* forceReattach */);
}

void HTMLImageElement::ensureCollapsedOrFallbackContent() {
  if (m_isFallbackImage)
    return;

  bool resourceErrorIndicatesElementShouldBeCollapsed =
      imageLoader().image() &&
      imageLoader().image()->resourceError().shouldCollapseInitiator();
  setLayoutDisposition(resourceErrorIndicatesElementShouldBeCollapsed
                           ? LayoutDisposition::Collapsed
                           : LayoutDisposition::FallbackContent);
}

void HTMLImageElement::ensurePrimaryContent() {
  setLayoutDisposition(LayoutDisposition::PrimaryContent);
}

bool HTMLImageElement::isCollapsed() const {
  return m_layoutDisposition == LayoutDisposition::Collapsed;
}

void HTMLImageElement::setLayoutDisposition(LayoutDisposition layoutDisposition,
                                            bool forceReattach) {
  if (m_layoutDisposition == layoutDisposition && !forceReattach)
    return;

  m_layoutDisposition = layoutDisposition;

  // This can happen inside of attachLayoutTree() in the middle of a recalcStyle
  // so we need to reattach synchronously here.
  if (document().inStyleRecalc()) {
    reattachLayoutTree();
  } else {
    if (m_layoutDisposition == LayoutDisposition::FallbackContent) {
      EventDispatchForbiddenScope::AllowUserAgentEvents allowEvents;
      ensureUserAgentShadowRoot();
    }
    lazyReattachIfAttached();
  }
}

PassRefPtr<ComputedStyle> HTMLImageElement::customStyleForLayoutObject() {
  switch (m_layoutDisposition) {
    case LayoutDisposition::PrimaryContent:  // Fall through.
    case LayoutDisposition::Collapsed:
      return originalStyleForLayoutObject();
    case LayoutDisposition::FallbackContent:
      return HTMLImageFallbackHelper::customStyleForAltText(
          *this, ComputedStyle::clone(*originalStyleForLayoutObject()));
    default:
      NOTREACHED();
      return nullptr;
  }
}

bool HTMLImageElement::isOpaque() const {
  Image* image = const_cast<HTMLImageElement*>(this)->imageContents();
  return image && image->currentFrameKnownToBeOpaque();
}

int HTMLImageElement::sourceWidth() {
  SourceImageStatus status;
  FloatSize defaultObjectSize(width(), height());
  RefPtr<Image> image = getSourceImageForCanvas(
      &status, PreferNoAcceleration, SnapshotReasonUnknown, defaultObjectSize);
  return image->width();
}

int HTMLImageElement::sourceHeight() {
  SourceImageStatus status;
  FloatSize defaultObjectSize(width(), height());
  RefPtr<Image> image = getSourceImageForCanvas(
      &status, PreferNoAcceleration, SnapshotReasonUnknown, defaultObjectSize);
  return image->height();
}

IntSize HTMLImageElement::bitmapSourceSize() const {
  ImageResourceContent* image = cachedImage();
  if (!image)
    return IntSize();
  LayoutSize lSize = image->imageSize(
      LayoutObject::shouldRespectImageOrientation(layoutObject()), 1.0f);
  DCHECK(lSize.fraction().isZero());
  return IntSize(lSize.width().toInt(), lSize.height().toInt());
}

void HTMLImageElement::associateWith(HTMLFormElement* form) {
  if (form && form->isConnected()) {
    m_form = form;
    m_formWasSetByParser = true;
    m_form->associate(*this);
    m_form->didAssociateByParser();
  }
};

}  // namespace blink
