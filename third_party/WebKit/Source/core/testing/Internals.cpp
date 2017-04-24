/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/testing/Internals.h"

#include <deque>
#include <memory>

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "bindings/core/v8/SerializedScriptValueFactory.h"
#include "bindings/core/v8/V8IteratorResultValue.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/HTMLNames.h"
#include "core/SVGNames.h"
#include "core/animation/DocumentTimeline.h"
#include "core/dom/ClientRect.h"
#include "core/dom/ClientRectList.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMNodeIds.h"
#include "core/dom/DOMPoint.h"
#include "core/dom/DOMStringList.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/Iterator.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/PseudoElement.h"
#include "core/dom/Range.h"
#include "core/dom/StaticNodeList.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/TreeScope.h"
#include "core/dom/ViewportDescription.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ElementShadowV0.h"
#include "core/dom/shadow/FlatTreeTraversal.h"
#include "core/dom/shadow/SelectRuleFeatureSet.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/Editor.h"
#include "core/editing/PlainTextRange.h"
#include "core/editing/SurroundingText.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/editing/markers/DocumentMarker.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/editing/serializers/Serialization.h"
#include "core/editing/spellcheck/SpellCheckRequester.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLContentElement.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/canvas/CanvasFontCache.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/html/forms/FormController.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/html/shadow/TextControlInnerElements.h"
#include "core/input/EventHandler.h"
#include "core/input/KeyboardEventManager.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/layout/LayoutMenuList.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTreeAsText.h"
#include "core/layout/api/LayoutMenuListItem.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/HistoryItem.h"
#include "core/page/ChromeClient.h"
#include "core/page/FocusController.h"
#include "core/page/NetworkStateNotifier.h"
#include "core/page/Page.h"
#include "core/page/PrintContext.h"
#include "core/page/scrolling/ScrollState.h"
#include "core/paint/PaintLayer.h"
#include "core/svg/SVGImageElement.h"
#include "core/testing/CallbackFunctionTest.h"
#include "core/testing/DictionaryTest.h"
#include "core/testing/GCObservation.h"
#include "core/testing/InternalRuntimeFlags.h"
#include "core/testing/InternalSettings.h"
#include "core/testing/LayerRect.h"
#include "core/testing/LayerRectList.h"
#include "core/testing/MockHyphenation.h"
#include "core/testing/OriginTrialsTest.h"
#include "core/testing/TypeConversions.h"
#include "core/testing/UnionTypesTest.h"
#include "core/workers/WorkerThread.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "platform/Cursor.h"
#include "platform/InstanceCounters.h"
#include "platform/Language.h"
#include "platform/LayoutLocale.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/heap/Handle.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/network/ResourceLoadPriority.h"
#include "platform/scroll/ProgrammaticScrollAnimator.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "public/platform/Platform.h"
#include "public/platform/WebConnectionType.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "public/platform/WebLayer.h"
#include "public/platform/modules/remoteplayback/WebRemotePlaybackAvailability.h"
#include "v8/include/v8.h"
#include "wtf/InstanceCounter.h"
#include "wtf/Optional.h"
#include "wtf/PtrUtil.h"
#include "wtf/dtoa.h"
#include "wtf/text/StringBuffer.h"

namespace blink {

namespace {

class UseCounterObserverImpl final : public UseCounter::Observer {
  WTF_MAKE_NONCOPYABLE(UseCounterObserverImpl);

 public:
  UseCounterObserverImpl(ScriptPromiseResolver* resolver,
                         UseCounter::Feature feature)
      : m_resolver(resolver), m_feature(feature) {}

  bool onCountFeature(UseCounter::Feature feature) final {
    if (m_feature != feature)
      return false;
    m_resolver->resolve(feature);
    return true;
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    UseCounter::Observer::trace(visitor);
    visitor->trace(m_resolver);
  }

 private:
  Member<ScriptPromiseResolver> m_resolver;
  UseCounter::Feature m_feature;
};

}  // namespace

static WTF::Optional<DocumentMarker::MarkerType> markerTypeFrom(
    const String& markerType) {
  if (equalIgnoringCase(markerType, "Spelling"))
    return DocumentMarker::Spelling;
  if (equalIgnoringCase(markerType, "Grammar"))
    return DocumentMarker::Grammar;
  if (equalIgnoringCase(markerType, "TextMatch"))
    return DocumentMarker::TextMatch;
  return WTF::nullopt;
}

static WTF::Optional<DocumentMarker::MarkerTypes> markerTypesFrom(
    const String& markerType) {
  if (markerType.isEmpty() || equalIgnoringCase(markerType, "all"))
    return DocumentMarker::AllMarkers();
  WTF::Optional<DocumentMarker::MarkerType> type = markerTypeFrom(markerType);
  if (!type)
    return WTF::nullopt;
  return DocumentMarker::MarkerTypes(type.value());
}

static SpellCheckRequester* spellCheckRequester(Document* document) {
  if (!document || !document->frame())
    return 0;
  return &document->frame()->spellChecker().spellCheckRequester();
}

static ScrollableArea* scrollableAreaForNode(Node* node) {
  if (!node)
    return nullptr;

  if (node->isDocumentNode()) {
    // This can be removed after root layer scrolling is enabled.
    if (FrameView* frameView = toDocument(node)->view())
      return frameView->layoutViewportScrollableArea();
  }

  LayoutObject* layoutObject = node->layoutObject();
  if (!layoutObject || !layoutObject->isBox())
    return nullptr;

  return toLayoutBox(layoutObject)->getScrollableArea();
}

static RuntimeEnabledFeatures::Backup* sFeaturesBackup = nullptr;

void Internals::resetToConsistentState(Page* page) {
  DCHECK(page);

  if (!sFeaturesBackup)
    sFeaturesBackup = new RuntimeEnabledFeatures::Backup;
  sFeaturesBackup->restore();
  page->setIsCursorVisible(true);
  page->setPageScaleFactor(1);
  page->deprecatedLocalMainFrame()
      ->view()
      ->layoutViewportScrollableArea()
      ->setScrollOffset(ScrollOffset(), ProgrammaticScroll);
  overrideUserPreferredLanguages(Vector<AtomicString>());
  if (!page->deprecatedLocalMainFrame()
           ->spellChecker()
           .isSpellCheckingEnabled())
    page->deprecatedLocalMainFrame()
        ->spellChecker()
        .toggleSpellCheckingEnabled();
  if (page->deprecatedLocalMainFrame()->editor().isOverwriteModeEnabled())
    page->deprecatedLocalMainFrame()->editor().toggleOverwriteModeEnabled();

  if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
    scrollingCoordinator->reset();

  page->deprecatedLocalMainFrame()->view()->clear();
  KeyboardEventManager::setCurrentCapsLockState(OverrideCapsLockState::Default);
}

Internals::Internals(ExecutionContext* context)
    : m_runtimeFlags(InternalRuntimeFlags::create()),
      m_document(toDocument(context)) {
  m_document->fetcher()->enableIsPreloadedForTest();
}

LocalFrame* Internals::frame() const {
  if (!m_document)
    return nullptr;
  return m_document->frame();
}

InternalSettings* Internals::settings() const {
  if (!m_document)
    return 0;
  Page* page = m_document->page();
  if (!page)
    return 0;
  return InternalSettings::from(*page);
}

InternalRuntimeFlags* Internals::runtimeFlags() const {
  return m_runtimeFlags.get();
}

unsigned Internals::workerThreadCount() const {
  return WorkerThread::workerThreadCount();
}

GCObservation* Internals::observeGC(ScriptValue scriptValue) {
  v8::Local<v8::Value> observedValue = scriptValue.v8Value();
  DCHECK(!observedValue.IsEmpty());
  if (observedValue->IsNull() || observedValue->IsUndefined()) {
    V8ThrowException::throwTypeError(v8::Isolate::GetCurrent(),
                                     "value to observe is null or undefined");
    return nullptr;
  }

  return GCObservation::create(observedValue);
}

unsigned Internals::updateStyleAndReturnAffectedElementCount(
    ExceptionState& exceptionState) const {
  if (!m_document) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "No context document is available.");
    return 0;
  }

  unsigned beforeCount = m_document->styleEngine().styleForElementCount();
  m_document->updateStyleAndLayoutTree();
  return m_document->styleEngine().styleForElementCount() - beforeCount;
}

unsigned Internals::needsLayoutCount(ExceptionState& exceptionState) const {
  LocalFrame* contextFrame = frame();
  if (!contextFrame) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "No context frame is available.");
    return 0;
  }

  bool isPartial;
  unsigned needsLayoutObjects;
  unsigned totalObjects;
  contextFrame->view()->countObjectsNeedingLayout(needsLayoutObjects,
                                                  totalObjects, isPartial);
  return needsLayoutObjects;
}

unsigned Internals::hitTestCount(Document* doc,
                                 ExceptionState& exceptionState) const {
  if (!doc) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "Must supply document to check");
    return 0;
  }

  return doc->layoutViewItem().hitTestCount();
}

unsigned Internals::hitTestCacheHits(Document* doc,
                                     ExceptionState& exceptionState) const {
  if (!doc) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "Must supply document to check");
    return 0;
  }

  return doc->layoutViewItem().hitTestCacheHits();
}

Element* Internals::elementFromPoint(Document* doc,
                                     double x,
                                     double y,
                                     bool ignoreClipping,
                                     bool allowChildFrameContent,
                                     ExceptionState& exceptionState) const {
  if (!doc) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "Must supply document to check");
    return 0;
  }

  if (doc->layoutViewItem().isNull())
    return 0;

  HitTestRequest::HitTestRequestType hitType =
      HitTestRequest::ReadOnly | HitTestRequest::Active;
  if (ignoreClipping)
    hitType |= HitTestRequest::IgnoreClipping;
  if (allowChildFrameContent)
    hitType |= HitTestRequest::AllowChildFrameContent;

  HitTestRequest request(hitType);

  return doc->hitTestPoint(x, y, request);
}

void Internals::clearHitTestCache(Document* doc,
                                  ExceptionState& exceptionState) const {
  if (!doc) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "Must supply document to check");
    return;
  }

  if (doc->layoutViewItem().isNull())
    return;

  doc->layoutViewItem().clearHitTestCache();
}

bool Internals::isPreloaded(const String& url) {
  return isPreloadedBy(url, m_document);
}

bool Internals::isPreloadedBy(const String& url, Document* document) {
  if (!document)
    return false;
  return document->fetcher()->isPreloadedForTest(document->completeURL(url));
}

bool Internals::isLoading(const String& url) {
  if (!m_document)
    return false;
  const String cacheIdentifier = m_document->fetcher()->getCacheIdentifier();
  Resource* resource = memoryCache()->resourceForURL(
      m_document->completeURL(url), cacheIdentifier);
  // We check loader() here instead of isLoading(), because a multipart
  // ImageResource lies isLoading() == false after the first part is loaded.
  return resource && resource->loader();
}

bool Internals::isLoadingFromMemoryCache(const String& url) {
  if (!m_document)
    return false;
  const String cacheIdentifier = m_document->fetcher()->getCacheIdentifier();
  Resource* resource = memoryCache()->resourceForURL(
      m_document->completeURL(url), cacheIdentifier);
  return resource && resource->getStatus() == ResourceStatus::Cached;
}

int Internals::getResourcePriority(const String& url, Document* document) {
  if (!document)
    return ResourceLoadPriority::ResourceLoadPriorityUnresolved;

  Resource* resource = document->fetcher()->allResources().at(
      URLTestHelpers::toKURL(url.utf8().data()));

  if (!resource)
    return ResourceLoadPriority::ResourceLoadPriorityUnresolved;

  return resource->resourceRequest().priority();
}

String Internals::getResourceHeader(const String& url,
                                    const String& header,
                                    Document* document) {
  if (!document)
    return String();
  Resource* resource = document->fetcher()->allResources().at(
      URLTestHelpers::toKURL(url.utf8().data()));
  if (!resource)
    return String();
  return resource->resourceRequest().httpHeaderField(header.utf8().data());
}

bool Internals::isSharingStyle(Element* element1, Element* element2) const {
  DCHECK(element1 && element2);
  return element1->computedStyle() == element2->computedStyle();
}

bool Internals::isValidContentSelect(Element* insertionPoint,
                                     ExceptionState& exceptionState) {
  DCHECK(insertionPoint);
  if (!insertionPoint->isInsertionPoint()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The element is not an insertion point.");
    return false;
  }

  return isHTMLContentElement(*insertionPoint) &&
         toHTMLContentElement(*insertionPoint).isSelectValid();
}

Node* Internals::treeScopeRootNode(Node* node) {
  DCHECK(node);
  return &node->treeScope().rootNode();
}

Node* Internals::parentTreeScope(Node* node) {
  DCHECK(node);
  const TreeScope* parentTreeScope = node->treeScope().parentTreeScope();
  return parentTreeScope ? &parentTreeScope->rootNode() : 0;
}

bool Internals::hasSelectorForIdInShadow(Element* host,
                                         const AtomicString& idValue,
                                         ExceptionState& exceptionState) {
  DCHECK(host);
  if (!host->shadow() || host->shadow()->isV1()) {
    exceptionState.throwDOMException(
        InvalidAccessError, "The host element does not have a v0 shadow.");
    return false;
  }

  return host->shadow()->v0().ensureSelectFeatureSet().hasSelectorForId(
      idValue);
}

bool Internals::hasSelectorForClassInShadow(Element* host,
                                            const AtomicString& className,
                                            ExceptionState& exceptionState) {
  DCHECK(host);
  if (!host->shadow() || host->shadow()->isV1()) {
    exceptionState.throwDOMException(
        InvalidAccessError, "The host element does not have a v0 shadow.");
    return false;
  }

  return host->shadow()->v0().ensureSelectFeatureSet().hasSelectorForClass(
      className);
}

bool Internals::hasSelectorForAttributeInShadow(
    Element* host,
    const AtomicString& attributeName,
    ExceptionState& exceptionState) {
  DCHECK(host);
  if (!host->shadow() || host->shadow()->isV1()) {
    exceptionState.throwDOMException(
        InvalidAccessError, "The host element does not have a v0 shadow.");
    return false;
  }

  return host->shadow()->v0().ensureSelectFeatureSet().hasSelectorForAttribute(
      attributeName);
}

unsigned short Internals::compareTreeScopePosition(
    const Node* node1,
    const Node* node2,
    ExceptionState& exceptionState) const {
  DCHECK(node1 && node2);
  const TreeScope* treeScope1 =
      node1->isDocumentNode()
          ? static_cast<const TreeScope*>(toDocument(node1))
          : node1->isShadowRoot()
                ? static_cast<const TreeScope*>(toShadowRoot(node1))
                : 0;
  const TreeScope* treeScope2 =
      node2->isDocumentNode()
          ? static_cast<const TreeScope*>(toDocument(node2))
          : node2->isShadowRoot()
                ? static_cast<const TreeScope*>(toShadowRoot(node2))
                : 0;
  if (!treeScope1 || !treeScope2) {
    exceptionState.throwDOMException(
        InvalidAccessError,
        String::format(
            "The %s node is neither a document node, nor a shadow root.",
            treeScope1 ? "second" : "first"));
    return 0;
  }
  return treeScope1->comparePosition(*treeScope2);
}

void Internals::pauseAnimations(double pauseTime,
                                ExceptionState& exceptionState) {
  if (pauseTime < 0) {
    exceptionState.throwDOMException(
        InvalidAccessError, ExceptionMessages::indexExceedsMinimumBound(
                                "pauseTime", pauseTime, 0.0));
    return;
  }

  if (!frame())
    return;

  frame()->view()->updateAllLifecyclePhases();
  frame()->document()->timeline().pauseAnimationsForTesting(pauseTime);
}

bool Internals::isCompositedAnimation(Animation* animation) {
  return animation->hasActiveAnimationsOnCompositor();
}

void Internals::disableCompositedAnimation(Animation* animation) {
  animation->disableCompositedAnimationForTesting();
}

void Internals::disableCSSAdditiveAnimations() {
  RuntimeEnabledFeatures::setCSSAdditiveAnimationsEnabled(false);
}

void Internals::advanceTimeForImage(Element* image,
                                    double deltaTimeInSeconds,
                                    ExceptionState& exceptionState) {
  DCHECK(image);
  if (deltaTimeInSeconds < 0) {
    exceptionState.throwDOMException(
        InvalidAccessError, ExceptionMessages::indexExceedsMinimumBound(
                                "deltaTimeInSeconds", deltaTimeInSeconds, 0.0));
    return;
  }

  ImageResourceContent* resource = nullptr;
  if (isHTMLImageElement(*image)) {
    resource = toHTMLImageElement(*image).cachedImage();
  } else if (isSVGImageElement(*image)) {
    resource = toSVGImageElement(*image).cachedImage();
  } else {
    exceptionState.throwDOMException(
        InvalidAccessError, "The element provided is not a image element.");
    return;
  }

  if (!resource || !resource->hasImage()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The image resource is not available.");
    return;
  }

  Image* imageData = resource->getImage();
  if (!imageData->isBitmapImage()) {
    exceptionState.throwDOMException(
        InvalidAccessError, "The image resource is not a BitmapImage type.");
    return;
  }

  imageData->advanceTime(deltaTimeInSeconds);
}

void Internals::advanceImageAnimation(Element* image,
                                      ExceptionState& exceptionState) {
  DCHECK(image);

  ImageResourceContent* resource = nullptr;
  if (isHTMLImageElement(*image)) {
    resource = toHTMLImageElement(*image).cachedImage();
  } else if (isSVGImageElement(*image)) {
    resource = toSVGImageElement(*image).cachedImage();
  } else {
    exceptionState.throwDOMException(
        InvalidAccessError, "The element provided is not a image element.");
    return;
  }

  if (!resource || !resource->hasImage()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The image resource is not available.");
    return;
  }

  Image* imageData = resource->getImage();
  imageData->advanceAnimationForTesting();
}

bool Internals::hasShadowInsertionPoint(const Node* root,
                                        ExceptionState& exceptionState) const {
  DCHECK(root);
  if (!root->isShadowRoot()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The node argument is not a shadow root.");
    return false;
  }
  return toShadowRoot(root)->containsShadowElements();
}

bool Internals::hasContentElement(const Node* root,
                                  ExceptionState& exceptionState) const {
  DCHECK(root);
  if (!root->isShadowRoot()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The node argument is not a shadow root.");
    return false;
  }
  return toShadowRoot(root)->containsContentElements();
}

size_t Internals::countElementShadow(const Node* root,
                                     ExceptionState& exceptionState) const {
  DCHECK(root);
  if (!root->isShadowRoot()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The node argument is not a shadow root.");
    return 0;
  }
  return toShadowRoot(root)->childShadowRootCount();
}

Node* Internals::nextSiblingInFlatTree(Node* node,
                                       ExceptionState& exceptionState) {
  DCHECK(node);
  if (!node->canParticipateInFlatTree()) {
    exceptionState.throwDOMException(
        InvalidAccessError,
        "The node argument doesn't particite in the flat tree.");
    return 0;
  }
  return FlatTreeTraversal::nextSibling(*node);
}

Node* Internals::firstChildInFlatTree(Node* node,
                                      ExceptionState& exceptionState) {
  DCHECK(node);
  if (!node->canParticipateInFlatTree()) {
    exceptionState.throwDOMException(
        InvalidAccessError,
        "The node argument doesn't particite in the flat tree");
    return 0;
  }
  return FlatTreeTraversal::firstChild(*node);
}

Node* Internals::lastChildInFlatTree(Node* node,
                                     ExceptionState& exceptionState) {
  DCHECK(node);
  if (!node->canParticipateInFlatTree()) {
    exceptionState.throwDOMException(
        InvalidAccessError,
        "The node argument doesn't particite in the flat tree.");
    return 0;
  }
  return FlatTreeTraversal::lastChild(*node);
}

Node* Internals::nextInFlatTree(Node* node, ExceptionState& exceptionState) {
  DCHECK(node);
  if (!node->canParticipateInFlatTree()) {
    exceptionState.throwDOMException(
        InvalidAccessError,
        "The node argument doesn't particite in the flat tree.");
    return 0;
  }
  return FlatTreeTraversal::next(*node);
}

Node* Internals::previousInFlatTree(Node* node,
                                    ExceptionState& exceptionState) {
  DCHECK(node);
  if (!node->canParticipateInFlatTree()) {
    exceptionState.throwDOMException(
        InvalidAccessError,
        "The node argument doesn't particite in the flat tree.");
    return 0;
  }
  return FlatTreeTraversal::previous(*node);
}

String Internals::elementLayoutTreeAsText(Element* element,
                                          ExceptionState& exceptionState) {
  DCHECK(element);
  element->document().view()->updateAllLifecyclePhases();

  String representation = externalRepresentation(element);
  if (representation.isEmpty()) {
    exceptionState.throwDOMException(
        InvalidAccessError,
        "The element provided has no external representation.");
    return String();
  }

  return representation;
}

CSSStyleDeclaration* Internals::computedStyleIncludingVisitedInfo(
    Node* node) const {
  DCHECK(node);
  bool allowVisitedStyle = true;
  return CSSComputedStyleDeclaration::create(node, allowVisitedStyle);
}

ShadowRoot* Internals::createUserAgentShadowRoot(Element* host) {
  DCHECK(host);
  return &host->ensureUserAgentShadowRoot();
}

ShadowRoot* Internals::shadowRoot(Element* host) {
  // FIXME: Internals::shadowRoot() in tests should be converted to
  // youngestShadowRoot() or oldestShadowRoot().
  // https://bugs.webkit.org/show_bug.cgi?id=78465
  return youngestShadowRoot(host);
}

ShadowRoot* Internals::youngestShadowRoot(Element* host) {
  DCHECK(host);
  if (ElementShadow* shadow = host->shadow())
    return &shadow->youngestShadowRoot();
  return 0;
}

ShadowRoot* Internals::oldestShadowRoot(Element* host) {
  DCHECK(host);
  if (ElementShadow* shadow = host->shadow())
    return &shadow->oldestShadowRoot();
  return 0;
}

ShadowRoot* Internals::youngerShadowRoot(Node* shadow,
                                         ExceptionState& exceptionState) {
  DCHECK(shadow);
  if (!shadow->isShadowRoot()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The node provided is not a shadow root.");
    return 0;
  }

  return toShadowRoot(shadow)->youngerShadowRoot();
}

String Internals::shadowRootType(const Node* root,
                                 ExceptionState& exceptionState) const {
  DCHECK(root);
  if (!root->isShadowRoot()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The node provided is not a shadow root.");
    return String();
  }

  switch (toShadowRoot(root)->type()) {
    case ShadowRootType::UserAgent:
      return String("UserAgentShadowRoot");
    case ShadowRootType::V0:
      return String("V0ShadowRoot");
    case ShadowRootType::Open:
      return String("OpenShadowRoot");
    case ShadowRootType::Closed:
      return String("ClosedShadowRoot");
    default:
      NOTREACHED();
      return String("Unknown");
  }
}

const AtomicString& Internals::shadowPseudoId(Element* element) {
  DCHECK(element);
  return element->shadowPseudoId();
}

String Internals::visiblePlaceholder(Element* element) {
  if (element && isTextControlElement(*element)) {
    const TextControlElement& textControlElement =
        toTextControlElement(*element);
    if (!textControlElement.isPlaceholderVisible())
      return String();
    if (HTMLElement* placeholderElement =
            textControlElement.placeholderElement())
      return placeholderElement->textContent();
  }

  return String();
}

void Internals::selectColorInColorChooser(Element* element,
                                          const String& colorValue) {
  DCHECK(element);
  if (!isHTMLInputElement(*element))
    return;
  Color color;
  if (!color.setFromString(colorValue))
    return;
  toHTMLInputElement(*element).selectColorInColorChooser(color);
}

void Internals::endColorChooser(Element* element) {
  DCHECK(element);
  if (!isHTMLInputElement(*element))
    return;
  toHTMLInputElement(*element).endColorChooser();
}

bool Internals::hasAutofocusRequest(Document* document) {
  if (!document)
    document = m_document;
  return document->autofocusElement();
}

bool Internals::hasAutofocusRequest() {
  return hasAutofocusRequest(0);
}

Vector<String> Internals::formControlStateOfHistoryItem(
    ExceptionState& exceptionState) {
  HistoryItem* mainItem = nullptr;
  if (frame())
    mainItem = frame()->loader().currentItem();
  if (!mainItem) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "No history item is available.");
    return Vector<String>();
  }
  return mainItem->getDocumentState();
}

void Internals::setFormControlStateOfHistoryItem(
    const Vector<String>& state,
    ExceptionState& exceptionState) {
  HistoryItem* mainItem = nullptr;
  if (frame())
    mainItem = frame()->loader().currentItem();
  if (!mainItem) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "No history item is available.");
    return;
  }
  mainItem->clearDocumentState();
  mainItem->setDocumentState(state);
}

DOMWindow* Internals::pagePopupWindow() const {
  if (!m_document)
    return nullptr;
  if (Page* page = m_document->page()) {
    LocalDOMWindow* popup =
        toLocalDOMWindow(page->chromeClient().pagePopupWindowForTesting());
    if (popup) {
      // We need to make the popup same origin so layout tests can access it.
      popup->document()->updateSecurityOrigin(m_document->getSecurityOrigin());
    }
    return popup;
  }
  return nullptr;
}

ClientRect* Internals::absoluteCaretBounds(ExceptionState& exceptionState) {
  if (!frame()) {
    exceptionState.throwDOMException(
        InvalidAccessError, "The document's frame cannot be retrieved.");
    return ClientRect::create();
  }

  m_document->updateStyleAndLayoutIgnorePendingStylesheets();
  return ClientRect::create(frame()->selection().absoluteCaretBounds());
}

String Internals::textAffinity() {
  if (frame()
          ->page()
          ->focusController()
          .focusedFrame()
          ->selection()
          .selectionInDOMTree()
          .affinity() == TextAffinity::Upstream) {
    return "Upstream";
  }
  return "Downstream";
}

ClientRect* Internals::boundingBox(Element* element) {
  DCHECK(element);

  element->document().updateStyleAndLayoutIgnorePendingStylesheets();
  LayoutObject* layoutObject = element->layoutObject();
  if (!layoutObject)
    return ClientRect::create();
  return ClientRect::create(
      layoutObject->absoluteBoundingBoxRectIgnoringTransforms());
}

void Internals::setMarker(Document* document,
                          const Range* range,
                          const String& markerType,
                          ExceptionState& exceptionState) {
  if (!document) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "No context document is available.");
    return;
  }

  WTF::Optional<DocumentMarker::MarkerType> type = markerTypeFrom(markerType);
  if (!type) {
    exceptionState.throwDOMException(
        SyntaxError,
        "The marker type provided ('" + markerType + "') is invalid.");
    return;
  }

  document->updateStyleAndLayoutIgnorePendingStylesheets();
  document->markers().addMarker(range->startPosition(), range->endPosition(),
                                type.value());
}

unsigned Internals::markerCountForNode(Node* node,
                                       const String& markerType,
                                       ExceptionState& exceptionState) {
  DCHECK(node);
  WTF::Optional<DocumentMarker::MarkerTypes> markerTypes =
      markerTypesFrom(markerType);
  if (!markerTypes) {
    exceptionState.throwDOMException(
        SyntaxError,
        "The marker type provided ('" + markerType + "') is invalid.");
    return 0;
  }

  return node->document()
      .markers()
      .markersFor(node, markerTypes.value())
      .size();
}

unsigned Internals::activeMarkerCountForNode(Node* node) {
  DCHECK(node);

  // Only TextMatch markers can be active.
  DocumentMarker::MarkerType markerType = DocumentMarker::TextMatch;
  DocumentMarkerVector markers =
      node->document().markers().markersFor(node, markerType);

  unsigned activeMarkerCount = 0;
  for (const auto& marker : markers) {
    if (marker->activeMatch())
      activeMarkerCount++;
  }

  return activeMarkerCount;
}

DocumentMarker* Internals::markerAt(Node* node,
                                    const String& markerType,
                                    unsigned index,
                                    ExceptionState& exceptionState) {
  DCHECK(node);
  WTF::Optional<DocumentMarker::MarkerTypes> markerTypes =
      markerTypesFrom(markerType);
  if (!markerTypes) {
    exceptionState.throwDOMException(
        SyntaxError,
        "The marker type provided ('" + markerType + "') is invalid.");
    return 0;
  }

  DocumentMarkerVector markers =
      node->document().markers().markersFor(node, markerTypes.value());
  if (markers.size() <= index)
    return 0;
  return markers[index];
}

Range* Internals::markerRangeForNode(Node* node,
                                     const String& markerType,
                                     unsigned index,
                                     ExceptionState& exceptionState) {
  DCHECK(node);
  DocumentMarker* marker = markerAt(node, markerType, index, exceptionState);
  if (!marker)
    return nullptr;
  return Range::create(node->document(), node, marker->startOffset(), node,
                       marker->endOffset());
}

String Internals::markerDescriptionForNode(Node* node,
                                           const String& markerType,
                                           unsigned index,
                                           ExceptionState& exceptionState) {
  DocumentMarker* marker = markerAt(node, markerType, index, exceptionState);
  if (!marker)
    return String();
  return marker->description();
}

void Internals::addTextMatchMarker(const Range* range, bool isActive) {
  DCHECK(range);
  if (!range->ownerDocument().view())
    return;

  range->ownerDocument().updateStyleAndLayoutIgnorePendingStylesheets();
  range->ownerDocument().markers().addTextMatchMarker(EphemeralRange(range),
                                                      isActive);

  // This simulates what the production code does after
  // DocumentMarkerController::addTextMatchMarker().
  range->ownerDocument().view()->invalidatePaintForTickmarks();
}

static bool parseColor(const String& value,
                       Color& color,
                       ExceptionState& exceptionState,
                       String errorMessage) {
  if (!color.setFromString(value)) {
    exceptionState.throwDOMException(InvalidAccessError, errorMessage);
    return false;
  }
  return true;
}

void Internals::addCompositionMarker(const Range* range,
                                     const String& underlineColorValue,
                                     bool thick,
                                     const String& backgroundColorValue,
                                     ExceptionState& exceptionState) {
  DCHECK(range);
  range->ownerDocument().updateStyleAndLayoutIgnorePendingStylesheets();

  Color underlineColor;
  Color backgroundColor;
  if (parseColor(underlineColorValue, underlineColor, exceptionState,
                 "Invalid underline color.") &&
      parseColor(backgroundColorValue, backgroundColor, exceptionState,
                 "Invalid background color.")) {
    range->ownerDocument().markers().addCompositionMarker(
        range->startPosition(), range->endPosition(), underlineColor, thick,
        backgroundColor);
  }
}

void Internals::setMarkersActive(Node* node,
                                 unsigned startOffset,
                                 unsigned endOffset,
                                 bool active) {
  DCHECK(node);
  node->document().markers().setMarkersActive(node, startOffset, endOffset,
                                              active);
}

void Internals::setMarkedTextMatchesAreHighlighted(Document* document,
                                                   bool highlight) {
  if (!document || !document->frame())
    return;

  document->frame()->editor().setMarkedTextMatchesAreHighlighted(highlight);
}

void Internals::setFrameViewPosition(Document* document,
                                     long x,
                                     long y,
                                     ExceptionState& exceptionState) {
  DCHECK(document);
  if (!document->view()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The document provided is invalid.");
    return;
  }

  FrameView* frameView = document->view();
  bool scrollbarsSuppressedOldValue = frameView->scrollbarsSuppressed();

  frameView->setScrollbarsSuppressed(false);
  frameView->updateScrollOffsetFromInternals(IntSize(x, y));
  frameView->setScrollbarsSuppressed(scrollbarsSuppressedOldValue);
}

String Internals::viewportAsText(Document* document,
                                 float,
                                 int availableWidth,
                                 int availableHeight,
                                 ExceptionState& exceptionState) {
  DCHECK(document);
  if (!document->page()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The document provided is invalid.");
    return String();
  }

  document->updateStyleAndLayoutIgnorePendingStylesheets();

  Page* page = document->page();

  // Update initial viewport size.
  IntSize initialViewportSize(availableWidth, availableHeight);
  document->page()->deprecatedLocalMainFrame()->view()->setFrameRect(
      IntRect(IntPoint::zero(), initialViewportSize));

  ViewportDescription description = page->viewportDescription();
  PageScaleConstraints constraints =
      description.resolve(FloatSize(initialViewportSize), Length());

  constraints.fitToContentsWidth(constraints.layoutSize.width(),
                                 availableWidth);
  constraints.resolveAutoInitialScale();

  StringBuilder builder;

  builder.append("viewport size ");
  builder.append(String::number(constraints.layoutSize.width()));
  builder.append('x');
  builder.append(String::number(constraints.layoutSize.height()));

  builder.append(" scale ");
  builder.append(String::number(constraints.initialScale));
  builder.append(" with limits [");
  builder.append(String::number(constraints.minimumScale));
  builder.append(", ");
  builder.append(String::number(constraints.maximumScale));

  builder.append("] and userScalable ");
  builder.append(description.userZoom ? "true" : "false");

  return builder.toString();
}

bool Internals::elementShouldAutoComplete(Element* element,
                                          ExceptionState& exceptionState) {
  DCHECK(element);
  if (isHTMLInputElement(*element))
    return toHTMLInputElement(*element).shouldAutocomplete();

  exceptionState.throwDOMException(InvalidNodeTypeError,
                                   "The element provided is not an INPUT.");
  return false;
}

String Internals::suggestedValue(Element* element,
                                 ExceptionState& exceptionState) {
  DCHECK(element);
  if (!element->isFormControlElement()) {
    exceptionState.throwDOMException(
        InvalidNodeTypeError,
        "The element provided is not a form control element.");
    return String();
  }

  String suggestedValue;
  if (isHTMLInputElement(*element))
    suggestedValue = toHTMLInputElement(*element).suggestedValue();

  if (isHTMLTextAreaElement(*element))
    suggestedValue = toHTMLTextAreaElement(*element).suggestedValue();

  if (isHTMLSelectElement(*element))
    suggestedValue = toHTMLSelectElement(*element).suggestedValue();

  return suggestedValue;
}

void Internals::setSuggestedValue(Element* element,
                                  const String& value,
                                  ExceptionState& exceptionState) {
  DCHECK(element);
  if (!element->isFormControlElement()) {
    exceptionState.throwDOMException(
        InvalidNodeTypeError,
        "The element provided is not a form control element.");
    return;
  }

  if (isHTMLInputElement(*element))
    toHTMLInputElement(*element).setSuggestedValue(value);

  if (isHTMLTextAreaElement(*element))
    toHTMLTextAreaElement(*element).setSuggestedValue(value);

  if (isHTMLSelectElement(*element))
    toHTMLSelectElement(*element).setSuggestedValue(value);
}

void Internals::setEditingValue(Element* element,
                                const String& value,
                                ExceptionState& exceptionState) {
  DCHECK(element);
  if (!isHTMLInputElement(*element)) {
    exceptionState.throwDOMException(InvalidNodeTypeError,
                                     "The element provided is not an INPUT.");
    return;
  }

  toHTMLInputElement(*element).setEditingValue(value);
}

void Internals::setAutofilled(Element* element,
                              bool enabled,
                              ExceptionState& exceptionState) {
  DCHECK(element);
  if (!element->isFormControlElement()) {
    exceptionState.throwDOMException(
        InvalidNodeTypeError,
        "The element provided is not a form control element.");
    return;
  }
  toHTMLFormControlElement(element)->setAutofilled(enabled);
}

Range* Internals::rangeFromLocationAndLength(Element* scope,
                                             int rangeLocation,
                                             int rangeLength) {
  DCHECK(scope);

  // TextIterator depends on Layout information, make sure layout it up to date.
  scope->document().updateStyleAndLayoutIgnorePendingStylesheets();

  return createRange(PlainTextRange(rangeLocation, rangeLocation + rangeLength)
                         .createRange(*scope));
}

unsigned Internals::locationFromRange(Element* scope, const Range* range) {
  DCHECK(scope && range);
  // PlainTextRange depends on Layout information, make sure layout it up to
  // date.
  scope->document().updateStyleAndLayoutIgnorePendingStylesheets();

  return PlainTextRange::create(*scope, *range).start();
}

unsigned Internals::lengthFromRange(Element* scope, const Range* range) {
  DCHECK(scope && range);
  // PlainTextRange depends on Layout information, make sure layout it up to
  // date.
  scope->document().updateStyleAndLayoutIgnorePendingStylesheets();

  return PlainTextRange::create(*scope, *range).length();
}

String Internals::rangeAsText(const Range* range) {
  DCHECK(range);
  // Clean layout is required by plain text extraction.
  range->ownerDocument().updateStyleAndLayoutIgnorePendingStylesheets();

  return range->text();
}

// FIXME: The next four functions are very similar - combine them once
// bestClickableNode/bestContextMenuNode have been combined..

DOMPoint* Internals::touchPositionAdjustedToBestClickableNode(
    long x,
    long y,
    long width,
    long height,
    Document* document,
    ExceptionState& exceptionState) {
  DCHECK(document);
  if (!document->frame()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The document provided is invalid.");
    return 0;
  }

  document->updateStyleAndLayout();

  IntSize radius(width / 2, height / 2);
  IntPoint point(x + radius.width(), y + radius.height());

  EventHandler& eventHandler = document->frame()->eventHandler();
  IntPoint hitTestPoint = document->frame()->view()->rootFrameToContents(point);
  HitTestResult result = eventHandler.hitTestResultAtPoint(
      hitTestPoint, HitTestRequest::ReadOnly | HitTestRequest::Active |
                        HitTestRequest::ListBased,
      LayoutSize(radius));

  Node* targetNode = 0;
  IntPoint adjustedPoint;

  bool foundNode = eventHandler.bestClickableNodeForHitTestResult(
      result, adjustedPoint, targetNode);
  if (foundNode)
    return DOMPoint::create(adjustedPoint.x(), adjustedPoint.y());

  return 0;
}

Node* Internals::touchNodeAdjustedToBestClickableNode(
    long x,
    long y,
    long width,
    long height,
    Document* document,
    ExceptionState& exceptionState) {
  DCHECK(document);
  if (!document->frame()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The document provided is invalid.");
    return 0;
  }

  document->updateStyleAndLayout();

  IntSize radius(width / 2, height / 2);
  IntPoint point(x + radius.width(), y + radius.height());

  EventHandler& eventHandler = document->frame()->eventHandler();
  IntPoint hitTestPoint = document->frame()->view()->rootFrameToContents(point);
  HitTestResult result = eventHandler.hitTestResultAtPoint(
      hitTestPoint, HitTestRequest::ReadOnly | HitTestRequest::Active |
                        HitTestRequest::ListBased,
      LayoutSize(radius));

  Node* targetNode = 0;
  IntPoint adjustedPoint;
  document->frame()->eventHandler().bestClickableNodeForHitTestResult(
      result, adjustedPoint, targetNode);
  return targetNode;
}

DOMPoint* Internals::touchPositionAdjustedToBestContextMenuNode(
    long x,
    long y,
    long width,
    long height,
    Document* document,
    ExceptionState& exceptionState) {
  DCHECK(document);
  if (!document->frame()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The document provided is invalid.");
    return 0;
  }

  document->updateStyleAndLayout();

  IntSize radius(width / 2, height / 2);
  IntPoint point(x + radius.width(), y + radius.height());

  EventHandler& eventHandler = document->frame()->eventHandler();
  IntPoint hitTestPoint = document->frame()->view()->rootFrameToContents(point);
  HitTestResult result = eventHandler.hitTestResultAtPoint(
      hitTestPoint, HitTestRequest::ReadOnly | HitTestRequest::Active |
                        HitTestRequest::ListBased,
      LayoutSize(radius));

  Node* targetNode = 0;
  IntPoint adjustedPoint;

  bool foundNode = eventHandler.bestContextMenuNodeForHitTestResult(
      result, adjustedPoint, targetNode);
  if (foundNode)
    return DOMPoint::create(adjustedPoint.x(), adjustedPoint.y());

  return DOMPoint::create(x, y);
}

Node* Internals::touchNodeAdjustedToBestContextMenuNode(
    long x,
    long y,
    long width,
    long height,
    Document* document,
    ExceptionState& exceptionState) {
  DCHECK(document);
  if (!document->frame()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The document provided is invalid.");
    return 0;
  }

  document->updateStyleAndLayout();

  IntSize radius(width / 2, height / 2);
  IntPoint point(x + radius.width(), y + radius.height());

  EventHandler& eventHandler = document->frame()->eventHandler();
  IntPoint hitTestPoint = document->frame()->view()->rootFrameToContents(point);
  HitTestResult result = eventHandler.hitTestResultAtPoint(
      hitTestPoint, HitTestRequest::ReadOnly | HitTestRequest::Active |
                        HitTestRequest::ListBased,
      LayoutSize(radius));

  Node* targetNode = 0;
  IntPoint adjustedPoint;
  eventHandler.bestContextMenuNodeForHitTestResult(result, adjustedPoint,
                                                   targetNode);
  return targetNode;
}

ClientRect* Internals::bestZoomableAreaForTouchPoint(
    long x,
    long y,
    long width,
    long height,
    Document* document,
    ExceptionState& exceptionState) {
  DCHECK(document);
  if (!document->frame()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The document provided is invalid.");
    return nullptr;
  }

  document->updateStyleAndLayout();

  IntSize radius(width / 2, height / 2);
  IntPoint point(x + radius.width(), y + radius.height());

  Node* targetNode = 0;
  IntRect zoomableArea;
  bool foundNode =
      document->frame()->eventHandler().bestZoomableAreaForTouchPoint(
          point, radius, zoomableArea, targetNode);
  if (foundNode)
    return ClientRect::create(zoomableArea);

  return nullptr;
}

int Internals::lastSpellCheckRequestSequence(Document* document,
                                             ExceptionState& exceptionState) {
  SpellCheckRequester* requester = spellCheckRequester(document);

  if (!requester) {
    exceptionState.throwDOMException(
        InvalidAccessError,
        "No spell check requestor can be obtained for the provided document.");
    return -1;
  }

  return requester->lastRequestSequence();
}

int Internals::lastSpellCheckProcessedSequence(Document* document,
                                               ExceptionState& exceptionState) {
  SpellCheckRequester* requester = spellCheckRequester(document);

  if (!requester) {
    exceptionState.throwDOMException(
        InvalidAccessError,
        "No spell check requestor can be obtained for the provided document.");
    return -1;
  }

  return requester->lastProcessedSequence();
}

Vector<AtomicString> Internals::userPreferredLanguages() const {
  return blink::userPreferredLanguages();
}

// Optimally, the bindings generator would pass a Vector<AtomicString> here but
// this is not supported yet.
void Internals::setUserPreferredLanguages(const Vector<String>& languages) {
  Vector<AtomicString> atomicLanguages;
  for (size_t i = 0; i < languages.size(); ++i)
    atomicLanguages.push_back(AtomicString(languages[i]));
  overrideUserPreferredLanguages(atomicLanguages);
}

unsigned Internals::mediaKeysCount() {
  return InstanceCounters::counterValue(InstanceCounters::MediaKeysCounter);
}

unsigned Internals::mediaKeySessionCount() {
  return InstanceCounters::counterValue(
      InstanceCounters::MediaKeySessionCounter);
}

unsigned Internals::suspendableObjectCount(Document* document) {
  DCHECK(document);
  return document->suspendableObjectCount();
}

static unsigned eventHandlerCount(
    Document& document,
    EventHandlerRegistry::EventHandlerClass handlerClass) {
  if (!document.frameHost())
    return 0;
  EventHandlerRegistry* registry =
      &document.frameHost()->eventHandlerRegistry();
  unsigned count = 0;
  const EventTargetSet* targets = registry->eventHandlerTargets(handlerClass);
  if (targets) {
    for (const auto& target : *targets)
      count += target.value;
  }
  return count;
}

unsigned Internals::wheelEventHandlerCount(Document* document) {
  DCHECK(document);
  return eventHandlerCount(*document, EventHandlerRegistry::WheelEventBlocking);
}

unsigned Internals::scrollEventHandlerCount(Document* document) {
  DCHECK(document);
  return eventHandlerCount(*document, EventHandlerRegistry::ScrollEvent);
}

unsigned Internals::touchStartOrMoveEventHandlerCount(Document* document) {
  DCHECK(document);
  return eventHandlerCount(
             *document, EventHandlerRegistry::TouchStartOrMoveEventBlocking) +
         eventHandlerCount(*document,
                           EventHandlerRegistry::TouchStartOrMoveEventPassive);
}

unsigned Internals::touchEndOrCancelEventHandlerCount(Document* document) {
  DCHECK(document);
  return eventHandlerCount(
             *document, EventHandlerRegistry::TouchEndOrCancelEventBlocking) +
         eventHandlerCount(*document,
                           EventHandlerRegistry::TouchEndOrCancelEventPassive);
}

static PaintLayer* findLayerForGraphicsLayer(PaintLayer* searchRoot,
                                             GraphicsLayer* graphicsLayer,
                                             IntSize* layerOffset,
                                             String* layerType) {
  *layerOffset = IntSize();
  if (searchRoot->hasCompositedLayerMapping() &&
      graphicsLayer ==
          searchRoot->compositedLayerMapping()->mainGraphicsLayer()) {
    // If the |graphicsLayer| sets the scrollingContent layer as its
    // scroll parent, consider it belongs to the scrolling layer and
    // mark the layer type as "scrolling".
    if (!searchRoot->layoutObject().hasTransformRelatedProperty() &&
        searchRoot->scrollParent() &&
        searchRoot->parent() == searchRoot->scrollParent()) {
      *layerType = "scrolling";
      // For hit-test rect visualization to work, the hit-test rect should
      // be relative to the scrolling layer and in this case the hit-test
      // rect is relative to the element's own GraphicsLayer. So we will have
      // to adjust the rect to be relative to the scrolling layer here.
      // Only when the element's offsetParent == scroller's offsetParent we
      // can compute the element's relative position to the scrolling content
      // in this way.
      if (searchRoot->layoutObject().offsetParent() ==
          searchRoot->parent()->layoutObject().offsetParent()) {
        LayoutBoxModelObject& current = searchRoot->layoutObject();
        LayoutBoxModelObject& parent = searchRoot->parent()->layoutObject();
        layerOffset->setWidth((parent.offsetLeft(parent.offsetParent()) -
                               current.offsetLeft(parent.offsetParent()))
                                  .toInt());
        layerOffset->setHeight((parent.offsetTop(parent.offsetParent()) -
                                current.offsetTop(parent.offsetParent()))
                                   .toInt());
        return searchRoot->parent();
      }
    }

    LayoutRect rect;
    PaintLayer::mapRectInPaintInvalidationContainerToBacking(
        searchRoot->layoutObject(), rect);
    rect.move(searchRoot->compositedLayerMapping()
                  ->contentOffsetInCompositingLayer());

    *layerOffset = IntSize(rect.x().toInt(), rect.y().toInt());
    return searchRoot;
  }

  // If the |graphicsLayer| is a scroller's scrollingContent layer,
  // consider this is a scrolling layer.
  GraphicsLayer* layerForScrolling =
      searchRoot->getScrollableArea()
          ? searchRoot->getScrollableArea()->layerForScrolling()
          : 0;
  if (graphicsLayer == layerForScrolling) {
    *layerType = "scrolling";
    return searchRoot;
  }

  if (searchRoot->compositingState() == PaintsIntoGroupedBacking) {
    GraphicsLayer* squashingLayer =
        searchRoot->groupedMapping()->squashingLayer();
    if (graphicsLayer == squashingLayer) {
      *layerType = "squashing";
      LayoutRect rect;
      PaintLayer::mapRectInPaintInvalidationContainerToBacking(
          searchRoot->layoutObject(), rect);
      *layerOffset = IntSize(rect.x().toInt(), rect.y().toInt());
      return searchRoot;
    }
  }

  GraphicsLayer* layerForHorizontalScrollbar =
      searchRoot->getScrollableArea()
          ? searchRoot->getScrollableArea()->layerForHorizontalScrollbar()
          : 0;
  if (graphicsLayer == layerForHorizontalScrollbar) {
    *layerType = "horizontalScrollbar";
    return searchRoot;
  }

  GraphicsLayer* layerForVerticalScrollbar =
      searchRoot->getScrollableArea()
          ? searchRoot->getScrollableArea()->layerForVerticalScrollbar()
          : 0;
  if (graphicsLayer == layerForVerticalScrollbar) {
    *layerType = "verticalScrollbar";
    return searchRoot;
  }

  GraphicsLayer* layerForScrollCorner =
      searchRoot->getScrollableArea()
          ? searchRoot->getScrollableArea()->layerForScrollCorner()
          : 0;
  if (graphicsLayer == layerForScrollCorner) {
    *layerType = "scrollCorner";
    return searchRoot;
  }

  // Search right to left to increase the chances that we'll choose the top-most
  // layers in a grouped mapping for squashing.
  for (PaintLayer* child = searchRoot->lastChild(); child;
       child = child->previousSibling()) {
    PaintLayer* foundLayer =
        findLayerForGraphicsLayer(child, graphicsLayer, layerOffset, layerType);
    if (foundLayer)
      return foundLayer;
  }

  return 0;
}

// Given a vector of rects, merge those that are adjacent, leaving empty rects
// in the place of no longer used slots. This is intended to simplify the list
// of rects returned by an SkRegion (which have been split apart for sorting
// purposes). No attempt is made to do this efficiently (eg. by relying on the
// sort criteria of SkRegion).
static void mergeRects(WebVector<blink::WebRect>& rects) {
  for (size_t i = 0; i < rects.size(); ++i) {
    if (rects[i].isEmpty())
      continue;
    bool updated;
    do {
      updated = false;
      for (size_t j = i + 1; j < rects.size(); ++j) {
        if (rects[j].isEmpty())
          continue;
        // Try to merge rects[j] into rects[i] along the 4 possible edges.
        if (rects[i].y == rects[j].y && rects[i].height == rects[j].height) {
          if (rects[i].x + rects[i].width == rects[j].x) {
            rects[i].width += rects[j].width;
            rects[j] = blink::WebRect();
            updated = true;
          } else if (rects[i].x == rects[j].x + rects[j].width) {
            rects[i].x = rects[j].x;
            rects[i].width += rects[j].width;
            rects[j] = blink::WebRect();
            updated = true;
          }
        } else if (rects[i].x == rects[j].x &&
                   rects[i].width == rects[j].width) {
          if (rects[i].y + rects[i].height == rects[j].y) {
            rects[i].height += rects[j].height;
            rects[j] = blink::WebRect();
            updated = true;
          } else if (rects[i].y == rects[j].y + rects[j].height) {
            rects[i].y = rects[j].y;
            rects[i].height += rects[j].height;
            rects[j] = blink::WebRect();
            updated = true;
          }
        }
      }
    } while (updated);
  }
}

static void accumulateLayerRectList(PaintLayerCompositor* compositor,
                                    GraphicsLayer* graphicsLayer,
                                    LayerRectList* rects) {
  WebVector<blink::WebRect> layerRects =
      graphicsLayer->platformLayer()->touchEventHandlerRegion();
  if (!layerRects.isEmpty()) {
    mergeRects(layerRects);
    String layerType;
    IntSize layerOffset;
    PaintLayer* paintLayer = findLayerForGraphicsLayer(
        compositor->rootLayer(), graphicsLayer, &layerOffset, &layerType);
    Node* node = paintLayer ? paintLayer->layoutObject().node() : 0;
    for (size_t i = 0; i < layerRects.size(); ++i) {
      if (!layerRects[i].isEmpty()) {
        rects->append(node, layerType, layerOffset.width(),
                      layerOffset.height(), ClientRect::create(layerRects[i]));
      }
    }
  }

  size_t numChildren = graphicsLayer->children().size();
  for (size_t i = 0; i < numChildren; ++i)
    accumulateLayerRectList(compositor, graphicsLayer->children()[i], rects);
}

LayerRectList* Internals::touchEventTargetLayerRects(
    Document* document,
    ExceptionState& exceptionState) {
  DCHECK(document);
  if (!document->view() || !document->page() || document != m_document) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The document provided is invalid.");
    return nullptr;
  }

  if (ScrollingCoordinator* scrollingCoordinator =
          document->page()->scrollingCoordinator())
    scrollingCoordinator->updateAfterCompositingChangeIfNeeded();

  LayoutViewItem view = document->layoutViewItem();
  if (!view.isNull()) {
    if (PaintLayerCompositor* compositor = view.compositor()) {
      if (GraphicsLayer* rootLayer = compositor->rootGraphicsLayer()) {
        LayerRectList* rects = LayerRectList::create();
        accumulateLayerRectList(compositor, rootLayer, rects);
        return rects;
      }
    }
  }

  return nullptr;
}

bool Internals::executeCommand(Document* document,
                               const String& name,
                               const String& value,
                               ExceptionState& exceptionState) {
  DCHECK(document);
  if (!document->frame()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The document provided is invalid.");
    return false;
  }

  LocalFrame* frame = document->frame();
  return frame->editor().executeCommand(name, value);
}

AtomicString Internals::htmlNamespace() {
  return HTMLNames::xhtmlNamespaceURI;
}

Vector<AtomicString> Internals::htmlTags() {
  Vector<AtomicString> tags(HTMLNames::HTMLTagsCount);
  std::unique_ptr<const HTMLQualifiedName* []> qualifiedNames =
      HTMLNames::getHTMLTags();
  for (size_t i = 0; i < HTMLNames::HTMLTagsCount; ++i)
    tags[i] = qualifiedNames[i]->localName();
  return tags;
}

AtomicString Internals::svgNamespace() {
  return SVGNames::svgNamespaceURI;
}

Vector<AtomicString> Internals::svgTags() {
  Vector<AtomicString> tags(SVGNames::SVGTagsCount);
  std::unique_ptr<const SVGQualifiedName* []> qualifiedNames =
      SVGNames::getSVGTags();
  for (size_t i = 0; i < SVGNames::SVGTagsCount; ++i)
    tags[i] = qualifiedNames[i]->localName();
  return tags;
}

StaticNodeList* Internals::nodesFromRect(Document* document,
                                         int centerX,
                                         int centerY,
                                         unsigned topPadding,
                                         unsigned rightPadding,
                                         unsigned bottomPadding,
                                         unsigned leftPadding,
                                         bool ignoreClipping,
                                         bool allowChildFrameContent,
                                         ExceptionState& exceptionState) const {
  DCHECK(document);
  if (!document->frame() || !document->frame()->view()) {
    exceptionState.throwDOMException(
        InvalidAccessError,
        "No view can be obtained from the provided document.");
    return nullptr;
  }

  LocalFrame* frame = document->frame();
  FrameView* frameView = document->view();
  LayoutViewItem layoutViewItem = document->layoutViewItem();

  if (layoutViewItem.isNull())
    return nullptr;

  float zoomFactor = frame->pageZoomFactor();
  LayoutPoint point =
      LayoutPoint(FloatPoint(centerX * zoomFactor + frameView->scrollX(),
                             centerY * zoomFactor + frameView->scrollY()));

  HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly |
                                               HitTestRequest::Active |
                                               HitTestRequest::ListBased;
  if (ignoreClipping)
    hitType |= HitTestRequest::IgnoreClipping;
  if (allowChildFrameContent)
    hitType |= HitTestRequest::AllowChildFrameContent;

  HitTestRequest request(hitType);

  // When ignoreClipping is false, this method returns null for coordinates
  // outside of the viewport.
  if (!request.ignoreClipping() &&
      !frameView->visibleContentRect().intersects(HitTestLocation::rectForPoint(
          point, topPadding, rightPadding, bottomPadding, leftPadding)))
    return nullptr;

  HeapVector<Member<Node>> matches;
  HitTestResult result(request, point, topPadding, rightPadding, bottomPadding,
                       leftPadding);
  layoutViewItem.hitTest(result);
  copyToVector(result.listBasedTestResult(), matches);

  return StaticNodeList::adopt(matches);
}

bool Internals::hasSpellingMarker(Document* document,
                                  int from,
                                  int length,
                                  ExceptionState& exceptionState) {
  if (!document || !document->frame()) {
    exceptionState.throwDOMException(
        InvalidAccessError,
        "No frame can be obtained from the provided document.");
    return false;
  }

  document->updateStyleAndLayoutIgnorePendingStylesheets();
  return document->frame()->spellChecker().selectionStartHasMarkerFor(
      DocumentMarker::Spelling, from, length);
}

void Internals::setSpellCheckingEnabled(bool enabled,
                                        ExceptionState& exceptionState) {
  if (!frame()) {
    exceptionState.throwDOMException(
        InvalidAccessError,
        "No frame can be obtained from the provided document.");
    return;
  }

  if (enabled != frame()->spellChecker().isSpellCheckingEnabled())
    frame()->spellChecker().toggleSpellCheckingEnabled();
}

void Internals::replaceMisspelled(Document* document,
                                  const String& replacement,
                                  ExceptionState& exceptionState) {
  if (!document || !document->frame()) {
    exceptionState.throwDOMException(
        InvalidAccessError,
        "No frame can be obtained from the provided document.");
    return;
  }

  document->updateStyleAndLayoutIgnorePendingStylesheets();
  document->frame()->spellChecker().replaceMisspelledRange(replacement);
}

bool Internals::canHyphenate(const AtomicString& locale) {
  return LayoutLocale::valueOrDefault(LayoutLocale::get(locale))
      .getHyphenation();
}

void Internals::setMockHyphenation(const AtomicString& locale) {
  LayoutLocale::setHyphenationForTesting(locale, adoptRef(new MockHyphenation));
}

bool Internals::isOverwriteModeEnabled(Document* document) {
  DCHECK(document);
  if (!document->frame())
    return false;

  return document->frame()->editor().isOverwriteModeEnabled();
}

void Internals::toggleOverwriteModeEnabled(Document* document) {
  DCHECK(document);
  if (!document->frame())
    return;

  document->frame()->editor().toggleOverwriteModeEnabled();
}

unsigned Internals::numberOfLiveNodes() const {
  return InstanceCounters::counterValue(InstanceCounters::NodeCounter);
}

unsigned Internals::numberOfLiveDocuments() const {
  return InstanceCounters::counterValue(InstanceCounters::DocumentCounter);
}

String Internals::dumpRefCountedInstanceCounts() const {
  return WTF::dumpRefCountedInstanceCounts();
}

bool Internals::hasGrammarMarker(Document* document,
                                 int from,
                                 int length,
                                 ExceptionState& exceptionState) {
  if (!document || !document->frame()) {
    exceptionState.throwDOMException(
        InvalidAccessError,
        "No frame can be obtained from the provided document.");
    return false;
  }

  document->updateStyleAndLayoutIgnorePendingStylesheets();
  return document->frame()->spellChecker().selectionStartHasMarkerFor(
      DocumentMarker::Grammar, from, length);
}

unsigned Internals::numberOfScrollableAreas(Document* document) {
  DCHECK(document);
  if (!document->frame())
    return 0;

  unsigned count = 0;
  LocalFrame* frame = document->frame();
  if (frame->view()->scrollableAreas())
    count += frame->view()->scrollableAreas()->size();

  for (Frame* child = frame->tree().firstChild(); child;
       child = child->tree().nextSibling()) {
    if (child->isLocalFrame() && toLocalFrame(child)->view() &&
        toLocalFrame(child)->view()->scrollableAreas())
      count += toLocalFrame(child)->view()->scrollableAreas()->size();
  }

  return count;
}

bool Internals::isPageBoxVisible(Document* document, int pageNumber) {
  DCHECK(document);
  return document->isPageBoxVisible(pageNumber);
}

String Internals::layerTreeAsText(Document* document,
                                  ExceptionState& exceptionState) const {
  return layerTreeAsText(document, 0, exceptionState);
}

String Internals::elementLayerTreeAsText(Element* element,
                                         ExceptionState& exceptionState) const {
  DCHECK(element);
  FrameView* frameView = element->document().view();
  frameView->updateAllLifecyclePhases();

  return elementLayerTreeAsText(element, 0, exceptionState);
}

bool Internals::scrollsWithRespectTo(Element* element1,
                                     Element* element2,
                                     ExceptionState& exceptionState) {
  DCHECK(element1 && element2);
  element1->document().view()->updateAllLifecyclePhases();

  LayoutObject* layoutObject1 = element1->layoutObject();
  LayoutObject* layoutObject2 = element2->layoutObject();
  if (!layoutObject1 || !layoutObject1->isBox()) {
    exceptionState.throwDOMException(
        InvalidAccessError,
        layoutObject1
            ? "The first provided element's layoutObject is not a box."
            : "The first provided element has no layoutObject.");
    return false;
  }
  if (!layoutObject2 || !layoutObject2->isBox()) {
    exceptionState.throwDOMException(
        InvalidAccessError,
        layoutObject2
            ? "The second provided element's layoutObject is not a box."
            : "The second provided element has no layoutObject.");
    return false;
  }

  PaintLayer* layer1 = toLayoutBox(layoutObject1)->layer();
  PaintLayer* layer2 = toLayoutBox(layoutObject2)->layer();
  if (!layer1 || !layer2) {
    exceptionState.throwDOMException(
        InvalidAccessError,
        String::format(
            "No PaintLayer can be obtained from the %s provided element.",
            layer1 ? "second" : "first"));
    return false;
  }

  return layer1->scrollsWithRespectTo(layer2);
}

String Internals::layerTreeAsText(Document* document,
                                  unsigned flags,
                                  ExceptionState& exceptionState) const {
  DCHECK(document);
  if (!document->frame()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The document provided is invalid.");
    return String();
  }

  document->view()->updateAllLifecyclePhases();

  return document->frame()->layerTreeAsText(flags);
}

String Internals::elementLayerTreeAsText(Element* element,
                                         unsigned flags,
                                         ExceptionState& exceptionState) const {
  DCHECK(element);
  element->document().updateStyleAndLayout();

  LayoutObject* layoutObject = element->layoutObject();
  if (!layoutObject || !layoutObject->isBox()) {
    exceptionState.throwDOMException(
        InvalidAccessError,
        layoutObject ? "The provided element's layoutObject is not a box."
                     : "The provided element has no layoutObject.");
    return String();
  }

  PaintLayer* layer = toLayoutBox(layoutObject)->layer();
  if (!layer || !layer->hasCompositedLayerMapping() ||
      !layer->compositedLayerMapping()->mainGraphicsLayer()) {
    // Don't raise exception in these cases which may be normally used in tests.
    return String();
  }

  return layer->compositedLayerMapping()->mainGraphicsLayer()->layerTreeAsText(
      flags);
}

String Internals::scrollingStateTreeAsText(Document*) const {
  return String();
}

String Internals::mainThreadScrollingReasons(
    Document* document,
    ExceptionState& exceptionState) const {
  DCHECK(document);
  if (!document->frame()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The document provided is invalid.");
    return String();
  }

  document->frame()->view()->updateAllLifecyclePhases();

  return document->frame()->view()->mainThreadScrollingReasonsAsText();
}

ClientRectList* Internals::nonFastScrollableRects(
    Document* document,
    ExceptionState& exceptionState) const {
  DCHECK(document);
  if (!document->frame()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The document provided is invalid.");
    return nullptr;
  }

  Page* page = document->page();
  if (!page)
    return nullptr;

  return page->nonFastScrollableRects(document->frame());
}

void Internals::evictAllResources() const {
  memoryCache()->evictResources();
}

String Internals::counterValue(Element* element) {
  if (!element)
    return String();

  return counterValueForElement(element);
}

int Internals::pageNumber(Element* element,
                          float pageWidth,
                          float pageHeight,
                          ExceptionState& exceptionState) {
  if (!element)
    return 0;

  if (pageWidth <= 0 || pageHeight <= 0) {
    exceptionState.throwDOMException(
        V8TypeError, "Page width and height must be larger than 0.");
    return 0;
  }

  return PrintContext::pageNumberForElement(element,
                                            FloatSize(pageWidth, pageHeight));
}

Vector<String> Internals::iconURLs(Document* document,
                                   int iconTypesMask) const {
  Vector<IconURL> iconURLs = document->iconURLs(iconTypesMask);
  Vector<String> array;

  for (auto& iconURL : iconURLs)
    array.push_back(iconURL.m_iconURL.getString());

  return array;
}

Vector<String> Internals::shortcutIconURLs(Document* document) const {
  return iconURLs(document, Favicon);
}

Vector<String> Internals::allIconURLs(Document* document) const {
  return iconURLs(document, Favicon | TouchIcon | TouchPrecomposedIcon);
}

int Internals::numberOfPages(float pageWidth,
                             float pageHeight,
                             ExceptionState& exceptionState) {
  if (!frame())
    return -1;

  if (pageWidth <= 0 || pageHeight <= 0) {
    exceptionState.throwDOMException(
        V8TypeError, "Page width and height must be larger than 0.");
    return -1;
  }

  return PrintContext::numberOfPages(frame(), FloatSize(pageWidth, pageHeight));
}

String Internals::pageProperty(String propertyName,
                               int pageNumber,
                               ExceptionState& exceptionState) const {
  if (!frame()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "No frame is available.");
    return String();
  }

  return PrintContext::pageProperty(frame(), propertyName.utf8().data(),
                                    pageNumber);
}

String Internals::pageSizeAndMarginsInPixels(
    int pageNumber,
    int width,
    int height,
    int marginTop,
    int marginRight,
    int marginBottom,
    int marginLeft,
    ExceptionState& exceptionState) const {
  if (!frame()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "No frame is available.");
    return String();
  }

  return PrintContext::pageSizeAndMarginsInPixels(
      frame(), pageNumber, width, height, marginTop, marginRight, marginBottom,
      marginLeft);
}

float Internals::pageScaleFactor(ExceptionState& exceptionState) {
  if (!m_document->page()) {
    exceptionState.throwDOMException(
        InvalidAccessError, "The document's page cannot be retrieved.");
    return 0;
  }
  Page* page = m_document->page();
  return page->frameHost().visualViewport().pageScale();
}

void Internals::setPageScaleFactor(float scaleFactor,
                                   ExceptionState& exceptionState) {
  if (scaleFactor <= 0)
    return;
  if (!m_document->page()) {
    exceptionState.throwDOMException(
        InvalidAccessError, "The document's page cannot be retrieved.");
    return;
  }
  Page* page = m_document->page();
  page->frameHost().visualViewport().setScale(scaleFactor);
}

void Internals::setPageScaleFactorLimits(float minScaleFactor,
                                         float maxScaleFactor,
                                         ExceptionState& exceptionState) {
  if (!m_document->page()) {
    exceptionState.throwDOMException(
        InvalidAccessError, "The document's page cannot be retrieved.");
    return;
  }

  Page* page = m_document->page();
  page->frameHost().setDefaultPageScaleLimits(minScaleFactor, maxScaleFactor);
}

bool Internals::magnifyScaleAroundAnchor(float scaleFactor, float x, float y) {
  if (!frame())
    return false;

  return frame()->host()->visualViewport().magnifyScaleAroundAnchor(
      scaleFactor, FloatPoint(x, y));
}

void Internals::setIsCursorVisible(Document* document,
                                   bool isVisible,
                                   ExceptionState& exceptionState) {
  DCHECK(document);
  if (!document->page()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "No context document can be obtained.");
    return;
  }
  document->page()->setIsCursorVisible(isVisible);
}

String Internals::effectivePreload(HTMLMediaElement* mediaElement) {
  DCHECK(mediaElement);
  return mediaElement->effectivePreload();
}

void Internals::mediaPlayerRemoteRouteAvailabilityChanged(
    HTMLMediaElement* mediaElement,
    bool available) {
  DCHECK(mediaElement);
  mediaElement->remoteRouteAvailabilityChanged(
      available ? WebRemotePlaybackAvailability::DeviceAvailable
                : WebRemotePlaybackAvailability::SourceNotSupported);
}

void Internals::mediaPlayerPlayingRemotelyChanged(
    HTMLMediaElement* mediaElement,
    bool remote) {
  DCHECK(mediaElement);
  if (remote)
    mediaElement->connectedToRemoteDevice();
  else
    mediaElement->disconnectedFromRemoteDevice();
}

void Internals::setMediaElementNetworkState(HTMLMediaElement* mediaElement,
                                            int state) {
  DCHECK(mediaElement);
  DCHECK(state >= WebMediaPlayer::NetworkState::NetworkStateEmpty);
  DCHECK(state <= WebMediaPlayer::NetworkState::NetworkStateDecodeError);
  mediaElement->setNetworkState(
      static_cast<WebMediaPlayer::NetworkState>(state));
}

void Internals::setPersistent(HTMLVideoElement* videoElement, bool persistent) {
  DCHECK(videoElement);
  videoElement->onBecamePersistentVideo(persistent);
}

void Internals::registerURLSchemeAsBypassingContentSecurityPolicy(
    const String& scheme) {
  SchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy(scheme);
}

void Internals::registerURLSchemeAsBypassingContentSecurityPolicy(
    const String& scheme,
    const Vector<String>& policyAreas) {
  uint32_t policyAreasEnum = SchemeRegistry::PolicyAreaNone;
  for (const auto& policyArea : policyAreas) {
    if (policyArea == "img")
      policyAreasEnum |= SchemeRegistry::PolicyAreaImage;
    else if (policyArea == "style")
      policyAreasEnum |= SchemeRegistry::PolicyAreaStyle;
  }
  SchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy(
      scheme, static_cast<SchemeRegistry::PolicyAreas>(policyAreasEnum));
}

void Internals::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(
    const String& scheme) {
  SchemeRegistry::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(
      scheme);
}

TypeConversions* Internals::typeConversions() const {
  return TypeConversions::create();
}

DictionaryTest* Internals::dictionaryTest() const {
  return DictionaryTest::create();
}

UnionTypesTest* Internals::unionTypesTest() const {
  return UnionTypesTest::create();
}

OriginTrialsTest* Internals::originTrialsTest() const {
  return OriginTrialsTest::create();
}

CallbackFunctionTest* Internals::callbackFunctionTest() const {
  return CallbackFunctionTest::create();
}

Vector<String> Internals::getReferencedFilePaths() const {
  if (!frame())
    return Vector<String>();

  return frame()->loader().currentItem()->getReferencedFilePaths();
}

void Internals::startStoringCompositedLayerDebugInfo(
    Document* document,
    ExceptionState& exceptionState) {
  DCHECK(document);
  if (!document->view()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The document provided is invalid.");
    return;
  }

  FrameView* frameView = document->view();
  frameView->setIsStoringCompositedLayerDebugInfo(true);
  frameView->updateAllLifecyclePhases();
}

void Internals::stopStoringCompositedLayerDebugInfo(
    Document* document,
    ExceptionState& exceptionState) {
  DCHECK(document);
  if (!document->view()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The document provided is invalid.");
    return;
  }

  FrameView* frameView = document->view();
  frameView->setIsStoringCompositedLayerDebugInfo(false);
  frameView->updateAllLifecyclePhases();
}

void Internals::startTrackingRepaints(Document* document,
                                      ExceptionState& exceptionState) {
  DCHECK(document);
  if (!document->view()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The document provided is invalid.");
    return;
  }

  FrameView* frameView = document->view();
  frameView->updateAllLifecyclePhases();
  frameView->setTracksPaintInvalidations(true);
}

void Internals::stopTrackingRepaints(Document* document,
                                     ExceptionState& exceptionState) {
  DCHECK(document);
  if (!document->view()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The document provided is invalid.");
    return;
  }

  FrameView* frameView = document->view();
  frameView->updateAllLifecyclePhases();
  frameView->setTracksPaintInvalidations(false);
}

void Internals::updateLayoutIgnorePendingStylesheetsAndRunPostLayoutTasks(
    Node* node,
    ExceptionState& exceptionState) {
  Document* document = nullptr;
  if (!node) {
    document = m_document;
  } else if (node->isDocumentNode()) {
    document = toDocument(node);
  } else if (isHTMLIFrameElement(*node)) {
    document = toHTMLIFrameElement(*node).contentDocument();
  }

  if (!document) {
    exceptionState.throwTypeError(
        "The node provided is neither a document nor an IFrame.");
    return;
  }
  document->updateStyleAndLayoutIgnorePendingStylesheets(
      Document::RunPostLayoutTasksSynchronously);
}

void Internals::forceFullRepaint(Document* document,
                                 ExceptionState& exceptionState) {
  DCHECK(document);
  if (!document->view()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The document provided is invalid.");
    return;
  }

  LayoutViewItem layoutViewItem = document->layoutViewItem();
  if (!layoutViewItem.isNull())
    layoutViewItem.invalidatePaintForViewAndCompositedLayers();
}

ClientRectList* Internals::draggableRegions(Document* document,
                                            ExceptionState& exceptionState) {
  return annotatedRegions(document, true, exceptionState);
}

ClientRectList* Internals::nonDraggableRegions(Document* document,
                                               ExceptionState& exceptionState) {
  return annotatedRegions(document, false, exceptionState);
}

ClientRectList* Internals::annotatedRegions(Document* document,
                                            bool draggable,
                                            ExceptionState& exceptionState) {
  DCHECK(document);
  if (!document->view()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The document provided is invalid.");
    return ClientRectList::create();
  }

  document->updateStyleAndLayout();
  document->view()->updateDocumentAnnotatedRegions();
  Vector<AnnotatedRegionValue> regions = document->annotatedRegions();

  Vector<FloatQuad> quads;
  for (size_t i = 0; i < regions.size(); ++i) {
    if (regions[i].draggable == draggable)
      quads.push_back(FloatQuad(FloatRect(regions[i].bounds)));
  }
  return ClientRectList::create(quads);
}

static const char* cursorTypeToString(Cursor::Type cursorType) {
  switch (cursorType) {
    case Cursor::Pointer:
      return "Pointer";
    case Cursor::Cross:
      return "Cross";
    case Cursor::Hand:
      return "Hand";
    case Cursor::IBeam:
      return "IBeam";
    case Cursor::Wait:
      return "Wait";
    case Cursor::Help:
      return "Help";
    case Cursor::EastResize:
      return "EastResize";
    case Cursor::NorthResize:
      return "NorthResize";
    case Cursor::NorthEastResize:
      return "NorthEastResize";
    case Cursor::NorthWestResize:
      return "NorthWestResize";
    case Cursor::SouthResize:
      return "SouthResize";
    case Cursor::SouthEastResize:
      return "SouthEastResize";
    case Cursor::SouthWestResize:
      return "SouthWestResize";
    case Cursor::WestResize:
      return "WestResize";
    case Cursor::NorthSouthResize:
      return "NorthSouthResize";
    case Cursor::EastWestResize:
      return "EastWestResize";
    case Cursor::NorthEastSouthWestResize:
      return "NorthEastSouthWestResize";
    case Cursor::NorthWestSouthEastResize:
      return "NorthWestSouthEastResize";
    case Cursor::ColumnResize:
      return "ColumnResize";
    case Cursor::RowResize:
      return "RowResize";
    case Cursor::MiddlePanning:
      return "MiddlePanning";
    case Cursor::EastPanning:
      return "EastPanning";
    case Cursor::NorthPanning:
      return "NorthPanning";
    case Cursor::NorthEastPanning:
      return "NorthEastPanning";
    case Cursor::NorthWestPanning:
      return "NorthWestPanning";
    case Cursor::SouthPanning:
      return "SouthPanning";
    case Cursor::SouthEastPanning:
      return "SouthEastPanning";
    case Cursor::SouthWestPanning:
      return "SouthWestPanning";
    case Cursor::WestPanning:
      return "WestPanning";
    case Cursor::Move:
      return "Move";
    case Cursor::VerticalText:
      return "VerticalText";
    case Cursor::Cell:
      return "Cell";
    case Cursor::ContextMenu:
      return "ContextMenu";
    case Cursor::Alias:
      return "Alias";
    case Cursor::Progress:
      return "Progress";
    case Cursor::NoDrop:
      return "NoDrop";
    case Cursor::Copy:
      return "Copy";
    case Cursor::None:
      return "None";
    case Cursor::NotAllowed:
      return "NotAllowed";
    case Cursor::ZoomIn:
      return "ZoomIn";
    case Cursor::ZoomOut:
      return "ZoomOut";
    case Cursor::Grab:
      return "Grab";
    case Cursor::Grabbing:
      return "Grabbing";
    case Cursor::Custom:
      return "Custom";
  }

  NOTREACHED();
  return "UNKNOWN";
}

String Internals::getCurrentCursorInfo() {
  if (!frame())
    return String();

  Cursor cursor = frame()->page()->chromeClient().lastSetCursorForTesting();

  StringBuilder result;
  result.append("type=");
  result.append(cursorTypeToString(cursor.getType()));
  result.append(" hotSpot=");
  result.appendNumber(cursor.hotSpot().x());
  result.append(',');
  result.appendNumber(cursor.hotSpot().y());
  if (cursor.getImage()) {
    IntSize size = cursor.getImage()->size();
    result.append(" image=");
    result.appendNumber(size.width());
    result.append('x');
    result.appendNumber(size.height());
  }
  if (cursor.imageScaleFactor() != 1) {
    result.append(" scale=");
    result.appendNumber(cursor.imageScaleFactor(), 8);
  }

  return result.toString();
}

bool Internals::cursorUpdatePending() const {
  if (!frame())
    return false;

  return frame()->eventHandler().cursorUpdatePending();
}

DOMArrayBuffer* Internals::serializeObject(
    PassRefPtr<SerializedScriptValue> value) const {
  String stringValue = value->toWireString();
  DOMArrayBuffer* buffer = DOMArrayBuffer::createUninitializedOrNull(
      stringValue.length(), sizeof(UChar));
  if (buffer) {
    stringValue.copyTo(static_cast<UChar*>(buffer->data()), 0,
                       stringValue.length());
  }
  return buffer;
}

PassRefPtr<SerializedScriptValue> Internals::deserializeBuffer(
    DOMArrayBuffer* buffer) const {
  String value(static_cast<const UChar*>(buffer->data()),
               buffer->byteLength() / sizeof(UChar));
  return SerializedScriptValue::create(value);
}

void Internals::forceReload(bool bypassCache) {
  if (!frame())
    return;

  frame()->reload(bypassCache ? FrameLoadTypeReloadBypassingCache
                              : FrameLoadTypeReloadMainResource,
                  ClientRedirectPolicy::NotClientRedirect);
}

Node* Internals::visibleSelectionAnchorNode() {
  if (!frame())
    return nullptr;
  Position position =
      frame()->selection().computeVisibleSelectionInDOMTreeDeprecated().base();
  return position.isNull() ? nullptr : position.computeContainerNode();
}

unsigned Internals::visibleSelectionAnchorOffset() {
  if (!frame())
    return 0;
  Position position =
      frame()->selection().computeVisibleSelectionInDOMTreeDeprecated().base();
  return position.isNull() ? 0 : position.computeOffsetInContainerNode();
}

Node* Internals::visibleSelectionFocusNode() {
  if (!frame())
    return nullptr;
  Position position = frame()
                          ->selection()
                          .computeVisibleSelectionInDOMTreeDeprecated()
                          .extent();
  return position.isNull() ? nullptr : position.computeContainerNode();
}

unsigned Internals::visibleSelectionFocusOffset() {
  if (!frame())
    return 0;
  Position position = frame()
                          ->selection()
                          .computeVisibleSelectionInDOMTreeDeprecated()
                          .extent();
  return position.isNull() ? 0 : position.computeOffsetInContainerNode();
}

ClientRect* Internals::selectionBounds(ExceptionState& exceptionState) {
  if (!frame()) {
    exceptionState.throwDOMException(
        InvalidAccessError, "The document's frame cannot be retrieved.");
    return nullptr;
  }

  return ClientRect::create(FloatRect(frame()->selection().bounds()));
}

String Internals::markerTextForListItem(Element* element) {
  DCHECK(element);
  return blink::markerTextForListItem(element);
}

String Internals::getImageSourceURL(Element* element) {
  DCHECK(element);
  return element->imageSourceURL();
}

String Internals::selectMenuListText(HTMLSelectElement* select) {
  DCHECK(select);
  LayoutObject* layoutObject = select->layoutObject();
  if (!layoutObject || !layoutObject->isMenuList())
    return String();

  LayoutMenuListItem menuListItem =
      LayoutMenuListItem(toLayoutMenuList(layoutObject));
  return menuListItem.text();
}

bool Internals::isSelectPopupVisible(Node* node) {
  DCHECK(node);
  if (!isHTMLSelectElement(*node))
    return false;
  return toHTMLSelectElement(*node).popupIsVisible();
}

bool Internals::selectPopupItemStyleIsRtl(Node* node, int itemIndex) {
  if (!node || !isHTMLSelectElement(*node))
    return false;

  HTMLSelectElement& select = toHTMLSelectElement(*node);
  if (itemIndex < 0 ||
      static_cast<size_t>(itemIndex) >= select.listItems().size())
    return false;
  const ComputedStyle* itemStyle =
      select.itemComputedStyle(*select.listItems()[itemIndex]);
  return itemStyle && itemStyle->direction() == TextDirection::kRtl;
}

int Internals::selectPopupItemStyleFontHeight(Node* node, int itemIndex) {
  if (!node || !isHTMLSelectElement(*node))
    return false;

  HTMLSelectElement& select = toHTMLSelectElement(*node);
  if (itemIndex < 0 ||
      static_cast<size_t>(itemIndex) >= select.listItems().size())
    return false;
  const ComputedStyle* itemStyle =
      select.itemComputedStyle(*select.listItems()[itemIndex]);

  if (itemStyle) {
    const SimpleFontData* fontData = itemStyle->font().primaryFont();
    DCHECK(fontData);
    return fontData ? fontData->getFontMetrics().height() : 0;
  }
  return 0;
}

void Internals::resetTypeAheadSession(HTMLSelectElement* select) {
  DCHECK(select);
  select->resetTypeAheadSessionForTesting();
}

bool Internals::loseSharedGraphicsContext3D() {
  std::unique_ptr<WebGraphicsContext3DProvider> sharedProvider =
      WTF::wrapUnique(Platform::current()
                          ->createSharedOffscreenGraphicsContext3DProvider());
  if (!sharedProvider)
    return false;
  gpu::gles2::GLES2Interface* sharedGL = sharedProvider->contextGL();
  sharedGL->LoseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_EXT,
                                GL_INNOCENT_CONTEXT_RESET_EXT);
  // To prevent tests that call loseSharedGraphicsContext3D from being
  // flaky, we call finish so that the context is guaranteed to be lost
  // synchronously (i.e. before returning).
  sharedGL->Finish();
  return true;
}

void Internals::forceCompositingUpdate(Document* document,
                                       ExceptionState& exceptionState) {
  DCHECK(document);
  if (document->layoutViewItem().isNull()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The document provided is invalid.");
    return;
  }

  document->frame()->view()->updateAllLifecyclePhases();
}

void Internals::setZoomFactor(float factor) {
  if (!frame())
    return;

  frame()->setPageZoomFactor(factor);
}

void Internals::setShouldRevealPassword(Element* element,
                                        bool reveal,
                                        ExceptionState& exceptionState) {
  DCHECK(element);
  if (!isHTMLInputElement(element)) {
    exceptionState.throwDOMException(InvalidNodeTypeError,
                                     "The element provided is not an INPUT.");
    return;
  }

  return toHTMLInputElement(*element).setShouldRevealPassword(reveal);
}

namespace {

class AddOneFunction : public ScriptFunction {
 public:
  static v8::Local<v8::Function> createFunction(ScriptState* scriptState) {
    AddOneFunction* self = new AddOneFunction(scriptState);
    return self->bindToV8Function();
  }

 private:
  explicit AddOneFunction(ScriptState* scriptState)
      : ScriptFunction(scriptState) {}

  ScriptValue call(ScriptValue value) override {
    v8::Local<v8::Value> v8Value = value.v8Value();
    DCHECK(v8Value->IsNumber());
    int intValue = v8Value.As<v8::Integer>()->Value();
    return ScriptValue(
        getScriptState(),
        v8::Integer::New(getScriptState()->isolate(), intValue + 1));
  }
};

}  // namespace

ScriptPromise Internals::createResolvedPromise(ScriptState* scriptState,
                                               ScriptValue value) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  resolver->resolve(value);
  return promise;
}

ScriptPromise Internals::createRejectedPromise(ScriptState* scriptState,
                                               ScriptValue value) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  resolver->reject(value);
  return promise;
}

ScriptPromise Internals::addOneToPromise(ScriptState* scriptState,
                                         ScriptPromise promise) {
  return promise.then(AddOneFunction::createFunction(scriptState));
}

ScriptPromise Internals::promiseCheck(ScriptState* scriptState,
                                      long arg1,
                                      bool arg2,
                                      const Dictionary& arg3,
                                      const String& arg4,
                                      const Vector<String>& arg5,
                                      ExceptionState& exceptionState) {
  if (arg2)
    return ScriptPromise::cast(scriptState,
                               v8String(scriptState->isolate(), "done"));
  exceptionState.throwDOMException(InvalidStateError,
                                   "Thrown from the native implementation.");
  return ScriptPromise();
}

ScriptPromise Internals::promiseCheckWithoutExceptionState(
    ScriptState* scriptState,
    const Dictionary& arg1,
    const String& arg2,
    const Vector<String>& arg3) {
  return ScriptPromise::cast(scriptState,
                             v8String(scriptState->isolate(), "done"));
}

ScriptPromise Internals::promiseCheckRange(ScriptState* scriptState,
                                           long arg1) {
  return ScriptPromise::cast(scriptState,
                             v8String(scriptState->isolate(), "done"));
}

ScriptPromise Internals::promiseCheckOverload(ScriptState* scriptState,
                                              Location*) {
  return ScriptPromise::cast(scriptState,
                             v8String(scriptState->isolate(), "done"));
}

ScriptPromise Internals::promiseCheckOverload(ScriptState* scriptState,
                                              Document*) {
  return ScriptPromise::cast(scriptState,
                             v8String(scriptState->isolate(), "done"));
}

ScriptPromise Internals::promiseCheckOverload(ScriptState* scriptState,
                                              Location*,
                                              long,
                                              long) {
  return ScriptPromise::cast(scriptState,
                             v8String(scriptState->isolate(), "done"));
}

DEFINE_TRACE(Internals) {
  visitor->trace(m_runtimeFlags);
  visitor->trace(m_document);
}

void Internals::setValueForUser(HTMLInputElement* element,
                                const String& value) {
  element->setValueForUser(value);
}

String Internals::textSurroundingNode(Node* node,
                                      int x,
                                      int y,
                                      unsigned long maxLength) {
  if (!node)
    return String();

  // VisiblePosition and SurroundingText must be created with clean layout.
  node->document().updateStyleAndLayoutIgnorePendingStylesheets();
  DocumentLifecycle::DisallowTransitionScope disallowTransition(
      node->document().lifecycle());

  if (!node->layoutObject())
    return String();
  blink::WebPoint point(x, y);
  SurroundingText surroundingText(
      createVisiblePosition(
          node->layoutObject()->positionForPoint(static_cast<IntPoint>(point)))
          .deepEquivalent()
          .parentAnchoredEquivalent(),
      maxLength);
  return surroundingText.content();
}

void Internals::setFocused(bool focused) {
  if (!frame())
    return;

  frame()->page()->focusController().setFocused(focused);
}

void Internals::setInitialFocus(bool reverse) {
  if (!frame())
    return;

  frame()->document()->clearFocusedElement();
  frame()->page()->focusController().setInitialFocus(
      reverse ? WebFocusTypeBackward : WebFocusTypeForward);
}

bool Internals::ignoreLayoutWithPendingStylesheets(Document* document) {
  DCHECK(document);
  return document->ignoreLayoutWithPendingStylesheets();
}

void Internals::setNetworkConnectionInfoOverride(
    bool onLine,
    const String& type,
    double downlinkMaxMbps,
    ExceptionState& exceptionState) {
  WebConnectionType webtype;
  if (type == "cellular2g") {
    webtype = WebConnectionTypeCellular2G;
  } else if (type == "cellular3g") {
    webtype = WebConnectionTypeCellular3G;
  } else if (type == "cellular4g") {
    webtype = WebConnectionTypeCellular4G;
  } else if (type == "bluetooth") {
    webtype = WebConnectionTypeBluetooth;
  } else if (type == "ethernet") {
    webtype = WebConnectionTypeEthernet;
  } else if (type == "wifi") {
    webtype = WebConnectionTypeWifi;
  } else if (type == "wimax") {
    webtype = WebConnectionTypeWimax;
  } else if (type == "other") {
    webtype = WebConnectionTypeOther;
  } else if (type == "none") {
    webtype = WebConnectionTypeNone;
  } else if (type == "unknown") {
    webtype = WebConnectionTypeUnknown;
  } else {
    exceptionState.throwDOMException(
        NotFoundError,
        ExceptionMessages::failedToEnumerate("connection type", type));
    return;
  }
  networkStateNotifier().setOverride(onLine, webtype, downlinkMaxMbps);
}

void Internals::clearNetworkConnectionInfoOverride() {
  networkStateNotifier().clearOverride();
}

unsigned Internals::countHitRegions(CanvasRenderingContext* context) {
  return context->hitRegionsCount();
}

bool Internals::isInCanvasFontCache(Document* document,
                                    const String& fontString) {
  return document->canvasFontCache()->isInCache(fontString);
}

unsigned Internals::canvasFontCacheMaxFonts() {
  return CanvasFontCache::maxFonts();
}

void Internals::setScrollChain(ScrollState* scrollState,
                               const HeapVector<Member<Element>>& elements,
                               ExceptionState&) {
  std::deque<int> scrollChain;
  for (size_t i = 0; i < elements.size(); ++i)
    scrollChain.push_back(DOMNodeIds::idForNode(elements[i].get()));
  scrollState->setScrollChain(scrollChain);
}

void Internals::forceBlinkGCWithoutV8GC() {
  ThreadState::current()->setGCState(ThreadState::FullGCScheduled);
}

String Internals::selectedHTMLForClipboard() {
  if (!frame())
    return String();

  // Selection normalization and markup generation require clean layout.
  frame()->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  return frame()->selection().selectedHTMLForClipboard();
}

String Internals::selectedTextForClipboard() {
  if (!frame() || !frame()->document())
    return String();

  // Clean layout is required for extracting plain text from selection.
  frame()->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  return frame()->selection().selectedTextForClipboard();
}

void Internals::setVisualViewportOffset(int x, int y) {
  if (!frame())
    return;

  frame()->host()->visualViewport().setLocation(FloatPoint(x, y));
}

int Internals::visualViewportHeight() {
  if (!frame())
    return 0;

  return expandedIntSize(frame()->host()->visualViewport().visibleRect().size())
      .height();
}

int Internals::visualViewportWidth() {
  if (!frame())
    return 0;

  return expandedIntSize(frame()->host()->visualViewport().visibleRect().size())
      .width();
}

float Internals::visualViewportScrollX() {
  if (!frame())
    return 0;

  return frame()->view()->getScrollableArea()->getScrollOffset().width();
}

float Internals::visualViewportScrollY() {
  if (!frame())
    return 0;

  return frame()->view()->getScrollableArea()->getScrollOffset().height();
}

bool Internals::isUseCounted(Document* document, uint32_t feature) {
  if (feature >= UseCounter::NumberOfFeatures)
    return false;
  return UseCounter::isCounted(*document,
                               static_cast<UseCounter::Feature>(feature));
}

bool Internals::isCSSPropertyUseCounted(Document* document,
                                        const String& propertyName) {
  return UseCounter::isCounted(*document, propertyName);
}

ScriptPromise Internals::observeUseCounter(ScriptState* scriptState,
                                           Document* document,
                                           uint32_t feature) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  if (feature >= UseCounter::NumberOfFeatures) {
    resolver->reject();
    return promise;
  }

  UseCounter::Feature useCounterFeature =
      static_cast<UseCounter::Feature>(feature);
  if (UseCounter::isCounted(*document, useCounterFeature)) {
    resolver->resolve();
    return promise;
  }

  Page* page = document->page();
  if (!page) {
    resolver->reject();
    return promise;
  }

  page->useCounter().addObserver(
      new UseCounterObserverImpl(resolver, useCounterFeature));
  return promise;
}

String Internals::unscopableAttribute() {
  return "unscopableAttribute";
}

String Internals::unscopableMethod() {
  return "unscopableMethod";
}

ClientRectList* Internals::focusRingRects(Element* element) {
  Vector<LayoutRect> rects;
  if (element && element->layoutObject())
    element->layoutObject()->addOutlineRects(
        rects, LayoutPoint(), LayoutObject::IncludeBlockVisualOverflow);
  return ClientRectList::create(rects);
}

ClientRectList* Internals::outlineRects(Element* element) {
  Vector<LayoutRect> rects;
  if (element && element->layoutObject())
    element->layoutObject()->addOutlineRects(
        rects, LayoutPoint(), LayoutObject::DontIncludeBlockVisualOverflow);
  return ClientRectList::create(rects);
}

void Internals::setCapsLockState(bool enabled) {
  KeyboardEventManager::setCurrentCapsLockState(
      enabled ? OverrideCapsLockState::On : OverrideCapsLockState::Off);
}

bool Internals::setScrollbarVisibilityInScrollableArea(Node* node,
                                                       bool visible) {
  if (ScrollableArea* scrollableArea = scrollableAreaForNode(node)) {
    scrollableArea->setScrollbarsHidden(!visible);
    scrollableArea->scrollAnimator().setScrollbarsVisibleForTesting(visible);
    return ScrollbarTheme::theme().usesOverlayScrollbars();
  }
  return false;
}

double Internals::monotonicTimeToZeroBasedDocumentTime(
    double platformTime,
    ExceptionState& exceptionState) {
  return m_document->loader()->timing().monotonicTimeToZeroBasedDocumentTime(
      platformTime);
}

String Internals::getScrollAnimationState(Node* node) const {
  if (ScrollableArea* scrollableArea = scrollableAreaForNode(node))
    return scrollableArea->scrollAnimator().runStateAsText();
  return String();
}

String Internals::getProgrammaticScrollAnimationState(Node* node) const {
  if (ScrollableArea* scrollableArea = scrollableAreaForNode(node))
    return scrollableArea->programmaticScrollAnimator().runStateAsText();
  return String();
}

ClientRect* Internals::visualRect(Node* node) {
  if (!node || !node->layoutObject())
    return ClientRect::create();

  return ClientRect::create(FloatRect(node->layoutObject()->visualRect()));
}

void Internals::crash() {
  CHECK(false) << "Intentional crash";
}

void Internals::setIsLowEndDevice(bool isLowEndDevice) {
  MemoryCoordinator::setIsLowEndDeviceForTesting(isLowEndDevice);
}

}  // namespace blink
