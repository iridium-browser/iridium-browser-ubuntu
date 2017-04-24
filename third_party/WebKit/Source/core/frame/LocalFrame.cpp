/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Simon Hausmann <hausmann@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 *                     2001 George Staikos <staikos@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov <ap@nypop.com>
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Google Inc.
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

#include "core/frame/LocalFrame.h"

#include <memory>

#include "bindings/core/v8/ScriptController.h"
#include "core/InstrumentingAgents.h"
#include "core/dom/ChildFrameDisconnector.h"
#include "core/dom/DocumentType.h"
#include "core/dom/StyleChangeReason.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/InputMethodController.h"
#include "core/editing/serializers/Serialization.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/events/Event.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/PerformanceMonitor.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/input/EventHandler.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutPartItem.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/NavigationScheduler.h"
#include "core/page/ChromeClient.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintLayerPainter.h"
#include "core/paint/TransformRecorder.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/timing/Performance.h"
#include "platform/DragImage.h"
#include "platform/PluginScriptForbiddenScope.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/WebFrameScheduler.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/graphics/paint/PaintRecordBuilder.h"
#include "platform/graphics/paint/PaintSurface.h"
#include "platform/graphics/paint/TransformDisplayItem.h"
#include "platform/json/JSONValues.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/plugins/PluginData.h"
#include "platform/text/TextStream.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/InterfaceRegistry.h"
#include "public/platform/WebScreenInfo.h"
#include "public/platform/WebViewScheduler.h"
#include "third_party/skia/include/core/SkImage.h"
#include "wtf/PtrUtil.h"
#include "wtf/StdLibExtras.h"

namespace blink {

using namespace HTMLNames;

namespace {

// Convenience class for initializing a GraphicsContext to build a DragImage
// from a specific region specified by |bounds|. After painting the using
// context(), the DragImage returned from createImage() will only contain the
// content in |bounds| with the appropriate device scale factor included.
class DragImageBuilder {
  STACK_ALLOCATED();

 public:
  DragImageBuilder(const LocalFrame& localFrame, const FloatRect& bounds)
      : m_localFrame(&localFrame), m_bounds(bounds) {
    // TODO(oshima): Remove this when all platforms are migrated to
    // use-zoom-for-dsf.
    float deviceScaleFactor =
        m_localFrame->page()->deviceScaleFactorDeprecated();
    float pageScaleFactor = m_localFrame->host()->visualViewport().scale();
    m_bounds.setWidth(m_bounds.width() * deviceScaleFactor * pageScaleFactor);
    m_bounds.setHeight(m_bounds.height() * deviceScaleFactor * pageScaleFactor);
    m_builder = WTF::wrapUnique(new PaintRecordBuilder(
        SkRect::MakeIWH(m_bounds.width(), m_bounds.height())));

    AffineTransform transform;
    transform.scale(deviceScaleFactor * pageScaleFactor,
                    deviceScaleFactor * pageScaleFactor);
    transform.translate(-m_bounds.x(), -m_bounds.y());
    context().getPaintController().createAndAppend<BeginTransformDisplayItem>(
        *m_builder, transform);
  }

  GraphicsContext& context() { return m_builder->context(); }

  std::unique_ptr<DragImage> createImage(
      float opacity,
      RespectImageOrientationEnum imageOrientation =
          DoNotRespectImageOrientation) {
    context().getPaintController().endItem<EndTransformDisplayItem>(*m_builder);
    // TODO(fmalita): endRecording() should return a non-const SKP.
    sk_sp<PaintRecord> record(
        const_cast<PaintRecord*>(m_builder->endRecording().release()));

    // Rasterize upfront, since DragImage::create() is going to do it anyway
    // (SkImage::asLegacyBitmap).
    SkSurfaceProps surfaceProps(0, kUnknown_SkPixelGeometry);
    sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(
        m_bounds.width(), m_bounds.height(), &surfaceProps);
    if (!surface)
      return nullptr;

    record->playback(surface->getCanvas());
    RefPtr<Image> image =
        StaticBitmapImage::create(surface->makeImageSnapshot());

    float screenDeviceScaleFactor =
        m_localFrame->page()->chromeClient().screenInfo().deviceScaleFactor;

    return DragImage::create(image.get(), imageOrientation,
                             screenDeviceScaleFactor, InterpolationHigh,
                             opacity);
  }

 private:
  const Member<const LocalFrame> m_localFrame;
  FloatRect m_bounds;
  std::unique_ptr<PaintRecordBuilder> m_builder;
};

class DraggedNodeImageBuilder {
  STACK_ALLOCATED();

 public:
  DraggedNodeImageBuilder(const LocalFrame& localFrame, Node& node)
      : m_localFrame(&localFrame),
        m_node(&node)
#if DCHECK_IS_ON()
        ,
        m_domTreeVersion(node.document().domTreeVersion())
#endif
  {
    for (Node& descendant : NodeTraversal::inclusiveDescendantsOf(*m_node))
      descendant.setDragged(true);
  }

  ~DraggedNodeImageBuilder() {
#if DCHECK_IS_ON()
    DCHECK_EQ(m_domTreeVersion, m_node->document().domTreeVersion());
#endif
    for (Node& descendant : NodeTraversal::inclusiveDescendantsOf(*m_node))
      descendant.setDragged(false);
  }

  std::unique_ptr<DragImage> createImage() {
#if DCHECK_IS_ON()
    DCHECK_EQ(m_domTreeVersion, m_node->document().domTreeVersion());
#endif
    // Construct layout object for |m_node| with pseudo class "-webkit-drag"
    m_localFrame->view()->updateAllLifecyclePhasesExceptPaint();
    LayoutObject* const draggedLayoutObject = m_node->layoutObject();
    if (!draggedLayoutObject)
      return nullptr;
    // Paint starting at the nearest stacking context, clipped to the object
    // itself. This will also paint the contents behind the object if the
    // object contains transparency and there are other elements in the same
    // stacking context which stacked below.
    PaintLayer* layer = draggedLayoutObject->enclosingLayer();
    if (!layer->stackingNode()->isStackingContext())
      layer = layer->stackingNode()->ancestorStackingContextNode()->layer();
    IntRect absoluteBoundingBox =
        draggedLayoutObject->absoluteBoundingBoxRectIncludingDescendants();
    FloatRect boundingBox =
        layer->layoutObject()
            .absoluteToLocalQuad(FloatQuad(absoluteBoundingBox), UseTransforms)
            .boundingBox();
    DragImageBuilder dragImageBuilder(*m_localFrame, boundingBox);
    {
      PaintLayerPaintingInfo paintingInfo(layer, LayoutRect(boundingBox),
                                          GlobalPaintFlattenCompositingLayers,
                                          LayoutSize());
      PaintLayerFlags flags = PaintLayerHaveTransparency |
                              PaintLayerAppliedTransform |
                              PaintLayerUncachedClipRects;
      PaintLayerPainter(*layer).paint(dragImageBuilder.context(), paintingInfo,
                                      flags);
    }
    return dragImageBuilder.createImage(
        1.0f, LayoutObject::shouldRespectImageOrientation(draggedLayoutObject));
  }

 private:
  const Member<const LocalFrame> m_localFrame;
  const Member<Node> m_node;
#if DCHECK_IS_ON()
  const uint64_t m_domTreeVersion;
#endif
};

inline float parentPageZoomFactor(LocalFrame* frame) {
  Frame* parent = frame->tree().parent();
  if (!parent || !parent->isLocalFrame())
    return 1;
  return toLocalFrame(parent)->pageZoomFactor();
}

inline float parentTextZoomFactor(LocalFrame* frame) {
  Frame* parent = frame->tree().parent();
  if (!parent || !parent->isLocalFrame())
    return 1;
  return toLocalFrame(parent)->textZoomFactor();
}

}  // namespace

template class CORE_TEMPLATE_EXPORT Supplement<LocalFrame>;

LocalFrame* LocalFrame::create(LocalFrameClient* client,
                               FrameHost* host,
                               FrameOwner* owner,
                               InterfaceProvider* interfaceProvider,
                               InterfaceRegistry* interfaceRegistry) {
  LocalFrame* frame = new LocalFrame(
      client, host, owner,
      interfaceProvider ? interfaceProvider
                        : InterfaceProvider::getEmptyInterfaceProvider(),
      interfaceRegistry ? interfaceRegistry
                        : InterfaceRegistry::getEmptyInterfaceRegistry());
  probe::frameAttachedToParent(frame);
  return frame;
}

void LocalFrame::setView(FrameView* view) {
  ASSERT(!m_view || m_view != view);
  ASSERT(!document() || !document()->isActive());

  eventHandler().clear();

  m_view = view;
}

void LocalFrame::createView(const IntSize& viewportSize,
                            const Color& backgroundColor,
                            bool transparent,
                            ScrollbarMode horizontalScrollbarMode,
                            bool horizontalLock,
                            ScrollbarMode verticalScrollbarMode,
                            bool verticalLock) {
  ASSERT(this);
  ASSERT(page());

  bool isLocalRoot = this->isLocalRoot();

  if (isLocalRoot && view())
    view()->setParentVisible(false);

  setView(nullptr);

  FrameView* frameView = nullptr;
  if (isLocalRoot) {
    frameView = FrameView::create(*this, viewportSize);

    // The layout size is set by WebViewImpl to support @viewport
    frameView->setLayoutSizeFixedToFrameSize(false);
  } else {
    frameView = FrameView::create(*this);
  }

  frameView->setScrollbarModes(horizontalScrollbarMode, verticalScrollbarMode,
                               horizontalLock, verticalLock);

  setView(frameView);

  frameView->updateBackgroundRecursively(backgroundColor, transparent);

  if (isLocalRoot)
    frameView->setParentVisible(true);

  // FIXME: Not clear what the right thing for OOPI is here.
  if (!ownerLayoutItem().isNull()) {
    HTMLFrameOwnerElement* owner = deprecatedLocalOwner();
    ASSERT(owner);
    // FIXME: OOPI might lead to us temporarily lying to a frame and telling it
    // that it's owned by a FrameOwner that knows nothing about it. If we're
    // lying to this frame, don't let it clobber the existing widget.
    if (owner->contentFrame() == this)
      owner->setWidget(frameView);
  }

  if (owner())
    view()->setCanHaveScrollbars(owner()->scrollingMode() !=
                                 ScrollbarAlwaysOff);
}

LocalFrame::~LocalFrame() {
  // Verify that the FrameView has been cleared as part of detaching
  // the frame owner.
  ASSERT(!m_view);
}

DEFINE_TRACE(LocalFrame) {
  visitor->trace(m_instrumentingAgents);
  visitor->trace(m_performanceMonitor);
  visitor->trace(m_loader);
  visitor->trace(m_navigationScheduler);
  visitor->trace(m_view);
  visitor->trace(m_domWindow);
  visitor->trace(m_pagePopupOwner);
  visitor->trace(m_script);
  visitor->trace(m_editor);
  visitor->trace(m_spellChecker);
  visitor->trace(m_selection);
  visitor->trace(m_eventHandler);
  visitor->trace(m_console);
  visitor->trace(m_inputMethodController);
  Frame::trace(visitor);
  Supplementable<LocalFrame>::trace(visitor);
}

WindowProxy* LocalFrame::windowProxy(DOMWrapperWorld& world) {
  return m_script->windowProxy(world);
}

void LocalFrame::navigate(Document& originDocument,
                          const KURL& url,
                          bool replaceCurrentItem,
                          UserGestureStatus userGestureStatus) {
  m_navigationScheduler->scheduleLocationChange(&originDocument, url,
                                                replaceCurrentItem);
}

void LocalFrame::navigate(const FrameLoadRequest& request) {
  m_loader.load(request);
}

void LocalFrame::reload(FrameLoadType loadType,
                        ClientRedirectPolicy clientRedirectPolicy) {
  DCHECK(isReloadLoadType(loadType));
  if (clientRedirectPolicy == ClientRedirectPolicy::NotClientRedirect) {
    if (!m_loader.currentItem())
      return;
    FrameLoadRequest request =
        FrameLoadRequest(nullptr, m_loader.resourceRequestForReload(
                                      loadType, KURL(), clientRedirectPolicy));
    request.setClientRedirect(clientRedirectPolicy);
    m_loader.load(request, loadType);
  } else {
    if (RuntimeEnabledFeatures::fasterLocationReloadEnabled())
      DCHECK_EQ(FrameLoadTypeReloadMainResource, loadType);
    else
      DCHECK_EQ(FrameLoadTypeReload, loadType);
    m_navigationScheduler->scheduleReload();
  }
}

void LocalFrame::detach(FrameDetachType type) {
  // Note that detach() can be re-entered, so it's not possible to
  // DCHECK(!m_isDetaching) here.
  m_isDetaching = true;

  if (isLocalRoot())
    m_performanceMonitor->shutdown();

  PluginScriptForbiddenScope forbidPluginDestructorScripting;
  m_loader.stopAllLoaders();
  // Don't allow any new child frames to load in this frame: attaching a new
  // child frame during or after detaching children results in an attached
  // frame on a detached DOM tree, which is bad.
  SubframeLoadingDisabler disabler(*document());
  m_loader.dispatchUnloadEvent();
  detachChildren();

  // All done if detaching the subframes brought about a detach of this frame
  // also.
  if (!client())
    return;

  // stopAllLoaders() needs to be called after detachChildren(), because
  // detachChildren() will trigger the unload event handlers of any child
  // frames, and those event handlers might start a new subresource load in this
  // frame.
  m_loader.stopAllLoaders();
  m_loader.detach();
  document()->shutdown();
  // This is the earliest that scripting can be disabled:
  // - FrameLoader::detach() can fire XHR abort events
  // - Document::shutdown()'s deferred widget updates can run script.
  ScriptForbiddenScope forbidScript;
  m_loader.clear();
  if (!client())
    return;

  client()->willBeDetached();
  // Notify ScriptController that the frame is closing, since its cleanup ends
  // up calling back to LocalFrameClient via WindowProxy.
  script().clearForClose();
  setView(nullptr);

  m_host->eventHandlerRegistry().didRemoveAllEventHandlers(*domWindow());

  domWindow()->frameDestroyed();

  // TODO: Page should take care of updating focus/scrolling instead of Frame.
  // TODO: It's unclear as to why this is called more than once, but it is,
  // so page() could be null.
  if (page() && page()->focusController().focusedFrame() == this)
    page()->focusController().setFocusedFrame(nullptr);

  if (page() && page()->scrollingCoordinator() && m_view)
    page()->scrollingCoordinator()->willDestroyScrollableArea(m_view.get());

  probe::frameDetachedFromParent(this);
  Frame::detach(type);

  m_supplements.clear();
  m_frameScheduler.reset();
  WeakIdentifierMap<LocalFrame>::notifyObjectDestroyed(this);
}

bool LocalFrame::prepareForCommit() {
  return loader().prepareForCommit();
}

SecurityContext* LocalFrame::securityContext() const {
  return document();
}

void LocalFrame::printNavigationErrorMessage(const Frame& targetFrame,
                                             const char* reason) {
  // URLs aren't available for RemoteFrames, so the error message uses their
  // origin instead.
  String targetFrameDescription =
      targetFrame.isLocalFrame()
          ? "with URL '" +
                toLocalFrame(targetFrame).document()->url().getString() + "'"
          : "with origin '" +
                targetFrame.securityContext()->getSecurityOrigin()->toString() +
                "'";
  String message =
      "Unsafe JavaScript attempt to initiate navigation for frame " +
      targetFrameDescription + " from frame with URL '" +
      document()->url().getString() + "'. " + reason + "\n";

  domWindow()->printErrorMessage(message);
}

void LocalFrame::printNavigationWarning(const String& message) {
  m_console->addMessage(
      ConsoleMessage::create(JSMessageSource, WarningMessageLevel, message));
}

WindowProxyManagerBase* LocalFrame::getWindowProxyManager() const {
  return m_script->getWindowProxyManager();
}

bool LocalFrame::shouldClose() {
  // TODO(dcheng): This should be fixed to dispatch beforeunload events to
  // both local and remote frames.
  return m_loader.shouldClose();
}

void LocalFrame::detachChildren() {
  DCHECK(m_loader.stateMachine()->creatingInitialEmptyDocument() || document());

  if (Document* document = this->document())
    ChildFrameDisconnector(*document).disconnect();
}

void LocalFrame::documentAttached() {
  DCHECK(document());
  selection().documentAttached(document());
  inputMethodController().documentAttached(document());
  spellChecker().documentAttached(document());
  if (isMainFrame())
    m_hasReceivedUserGesture = false;
}

LocalDOMWindow* LocalFrame::domWindow() const {
  return toLocalDOMWindow(m_domWindow);
}

void LocalFrame::setDOMWindow(LocalDOMWindow* domWindow) {
  if (domWindow)
    script().clearWindowProxy();

  if (this->domWindow())
    this->domWindow()->reset();
  m_domWindow = domWindow;
}

Document* LocalFrame::document() const {
  return domWindow() ? domWindow()->document() : nullptr;
}

void LocalFrame::setPagePopupOwner(Element& owner) {
  m_pagePopupOwner = &owner;
}

LayoutView* LocalFrame::contentLayoutObject() const {
  return document() ? document()->layoutView() : nullptr;
}

LayoutViewItem LocalFrame::contentLayoutItem() const {
  return LayoutViewItem(contentLayoutObject());
}

void LocalFrame::didChangeVisibilityState() {
  if (document())
    document()->didChangeVisibilityState();

  Frame::didChangeVisibilityState();
}

LocalFrame* LocalFrame::localFrameRoot() {
  LocalFrame* curFrame = this;
  while (curFrame && curFrame->tree().parent() &&
         curFrame->tree().parent()->isLocalFrame())
    curFrame = toLocalFrame(curFrame->tree().parent());

  return curFrame;
}

bool LocalFrame::isCrossOriginSubframe() const {
  const SecurityOrigin* securityOrigin = securityContext()->getSecurityOrigin();
  Frame* top = tree().top();
  return top &&
         !securityOrigin->canAccess(
             top->securityContext()->getSecurityOrigin());
}

void LocalFrame::setPrinting(bool printing,
                             const FloatSize& pageSize,
                             const FloatSize& originalPageSize,
                             float maximumShrinkRatio) {
  // In setting printing, we should not validate resources already cached for
  // the document.  See https://bugs.webkit.org/show_bug.cgi?id=43704
  ResourceCacheValidationSuppressor validationSuppressor(document()->fetcher());

  document()->setPrinting(printing ? Document::Printing
                                   : Document::FinishingPrinting);
  view()->adjustMediaTypeForPrinting(printing);

  if (shouldUsePrintingLayout()) {
    view()->forceLayoutForPagination(pageSize, originalPageSize,
                                     maximumShrinkRatio);
  } else {
    if (LayoutView* layoutView = view()->layoutView()) {
      layoutView->setPreferredLogicalWidthsDirty();
      layoutView->setNeedsLayout(LayoutInvalidationReason::PrintingChanged);
      layoutView->setShouldDoFullPaintInvalidationForViewAndAllDescendants();
    }
    view()->layout();
    view()->adjustViewSize();
  }

  // Subframes of the one we're printing don't lay out to the page size.
  for (Frame* child = tree().firstChild(); child;
       child = child->tree().nextSibling()) {
    if (child->isLocalFrame())
      toLocalFrame(child)->setPrinting(printing, FloatSize(), FloatSize(), 0);
  }

  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
    view()->setSubtreeNeedsPaintPropertyUpdate();

  if (!printing)
    document()->setPrinting(Document::NotPrinting);
}

bool LocalFrame::shouldUsePrintingLayout() const {
  // Only top frame being printed should be fit to page size.
  // Subframes should be constrained by parents only.
  return document()->printing() &&
         (!tree().parent() || !tree().parent()->isLocalFrame() ||
          !toLocalFrame(tree().parent())->document()->printing());
}

FloatSize LocalFrame::resizePageRectsKeepingRatio(
    const FloatSize& originalSize,
    const FloatSize& expectedSize) {
  FloatSize resultSize;
  if (contentLayoutItem().isNull())
    return FloatSize();

  if (contentLayoutItem().style()->isHorizontalWritingMode()) {
    ASSERT(fabs(originalSize.width()) > std::numeric_limits<float>::epsilon());
    float ratio = originalSize.height() / originalSize.width();
    resultSize.setWidth(floorf(expectedSize.width()));
    resultSize.setHeight(floorf(resultSize.width() * ratio));
  } else {
    ASSERT(fabs(originalSize.height()) > std::numeric_limits<float>::epsilon());
    float ratio = originalSize.width() / originalSize.height();
    resultSize.setHeight(floorf(expectedSize.height()));
    resultSize.setWidth(floorf(resultSize.height() * ratio));
  }
  return resultSize;
}

void LocalFrame::setPageZoomFactor(float factor) {
  setPageAndTextZoomFactors(factor, m_textZoomFactor);
}

void LocalFrame::setTextZoomFactor(float factor) {
  setPageAndTextZoomFactors(m_pageZoomFactor, factor);
}

void LocalFrame::setPageAndTextZoomFactors(float pageZoomFactor,
                                           float textZoomFactor) {
  if (m_pageZoomFactor == pageZoomFactor && m_textZoomFactor == textZoomFactor)
    return;

  Page* page = this->page();
  if (!page)
    return;

  Document* document = this->document();
  if (!document)
    return;

  // Respect SVGs zoomAndPan="disabled" property in standalone SVG documents.
  // FIXME: How to handle compound documents + zoomAndPan="disabled"? Needs SVG
  // WG clarification.
  if (document->isSVGDocument()) {
    if (!document->accessSVGExtensions().zoomAndPanEnabled())
      return;
  }

  if (m_pageZoomFactor != pageZoomFactor) {
    if (FrameView* view = this->view()) {
      // Update the scroll position when doing a full page zoom, so the content
      // stays in relatively the same position.
      ScrollableArea* scrollableArea = view->layoutViewportScrollableArea();
      ScrollOffset scrollOffset = scrollableArea->getScrollOffset();
      float percentDifference = (pageZoomFactor / m_pageZoomFactor);
      scrollableArea->setScrollOffset(
          ScrollOffset(scrollOffset.width() * percentDifference,
                       scrollOffset.height() * percentDifference),
          ProgrammaticScroll);
    }
  }

  m_pageZoomFactor = pageZoomFactor;
  m_textZoomFactor = textZoomFactor;

  for (Frame* child = tree().firstChild(); child;
       child = child->tree().nextSibling()) {
    if (child->isLocalFrame())
      toLocalFrame(child)->setPageAndTextZoomFactors(m_pageZoomFactor,
                                                     m_textZoomFactor);
  }

  document->mediaQueryAffectingValueChanged();
  document->setNeedsStyleRecalc(
      SubtreeStyleChange,
      StyleChangeReasonForTracing::create(StyleChangeReason::Zoom));
  document->updateStyleAndLayoutIgnorePendingStylesheets();
}

void LocalFrame::deviceScaleFactorChanged() {
  document()->mediaQueryAffectingValueChanged();
  document()->setNeedsStyleRecalc(
      SubtreeStyleChange,
      StyleChangeReasonForTracing::create(StyleChangeReason::Zoom));
  for (Frame* child = tree().firstChild(); child;
       child = child->tree().nextSibling()) {
    if (child->isLocalFrame())
      toLocalFrame(child)->deviceScaleFactorChanged();
  }
}

double LocalFrame::devicePixelRatio() const {
  if (!m_host)
    return 0;

  double ratio = m_host->page().deviceScaleFactorDeprecated();
  ratio *= pageZoomFactor();
  return ratio;
}

std::unique_ptr<DragImage> LocalFrame::nodeImage(Node& node) {
  DraggedNodeImageBuilder imageNode(*this, node);
  return imageNode.createImage();
}

std::unique_ptr<DragImage> LocalFrame::dragImageForSelection(float opacity) {
  if (!selection().computeVisibleSelectionInDOMTreeDeprecated().isRange())
    return nullptr;

  m_view->updateAllLifecyclePhasesExceptPaint();
  ASSERT(document()->isActive());

  FloatRect paintingRect = FloatRect(selection().bounds());
  DragImageBuilder dragImageBuilder(*this, paintingRect);
  GlobalPaintFlags paintFlags =
      GlobalPaintSelectionOnly | GlobalPaintFlattenCompositingLayers;
  m_view->paintContents(dragImageBuilder.context(), paintFlags,
                        enclosingIntRect(paintingRect));
  return dragImageBuilder.createImage(opacity);
}

String LocalFrame::selectedText() const {
  return selection().selectedText();
}

String LocalFrame::selectedTextForClipboard() const {
  if (!document())
    return emptyString;
  DCHECK(!document()->needsLayoutTreeUpdate());
  return selection().selectedTextForClipboard();
}

PositionWithAffinity LocalFrame::positionForPoint(const IntPoint& framePoint) {
  HitTestResult result = eventHandler().hitTestResultAtPoint(framePoint);
  Node* node = result.innerNodeOrImageMapImage();
  if (!node)
    return PositionWithAffinity();
  LayoutObject* layoutObject = node->layoutObject();
  if (!layoutObject)
    return PositionWithAffinity();
  const PositionWithAffinity position =
      layoutObject->positionForPoint(result.localPoint());
  if (position.isNull())
    return PositionWithAffinity(firstPositionInOrBeforeNode(node));
  return position;
}

Document* LocalFrame::documentAtPoint(const IntPoint& pointInRootFrame) {
  if (!view())
    return nullptr;

  IntPoint pt = view()->rootFrameToContents(pointInRootFrame);

  if (contentLayoutItem().isNull())
    return nullptr;
  HitTestResult result = eventHandler().hitTestResultAtPoint(
      pt, HitTestRequest::ReadOnly | HitTestRequest::Active);
  return result.innerNode() ? &result.innerNode()->document() : nullptr;
}

EphemeralRange LocalFrame::rangeForPoint(const IntPoint& framePoint) {
  const PositionWithAffinity positionWithAffinity =
      positionForPoint(framePoint);
  if (positionWithAffinity.isNull())
    return EphemeralRange();

  VisiblePosition position = createVisiblePosition(positionWithAffinity);
  VisiblePosition previous = previousPositionOf(position);
  if (previous.isNotNull()) {
    const EphemeralRange previousCharacterRange = makeRange(previous, position);
    IntRect rect = editor().firstRectForRange(previousCharacterRange);
    if (rect.contains(framePoint))
      return EphemeralRange(previousCharacterRange);
  }

  VisiblePosition next = nextPositionOf(position);
  const EphemeralRange nextCharacterRange = makeRange(position, next);
  if (nextCharacterRange.isNotNull()) {
    IntRect rect = editor().firstRectForRange(nextCharacterRange);
    if (rect.contains(framePoint))
      return EphemeralRange(nextCharacterRange);
  }

  return EphemeralRange();
}

bool LocalFrame::shouldReuseDefaultView(const KURL& url) const {
  // Secure transitions can only happen when navigating from the initial empty
  // document.
  if (!loader().stateMachine()->isDisplayingInitialEmptyDocument())
    return false;

  return document()->isSecureTransitionTo(url);
}

void LocalFrame::removeSpellingMarkersUnderWords(const Vector<String>& words) {
  spellChecker().removeSpellingMarkersUnderWords(words);
}

String LocalFrame::layerTreeAsText(unsigned flags) const {
  if (contentLayoutItem().isNull())
    return String();

  std::unique_ptr<JSONObject> layers;
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    layers = view()->compositedLayersAsJSON(static_cast<LayerTreeFlags>(flags));
  } else {
    layers = contentLayoutItem().compositor()->layerTreeAsJSON(
        static_cast<LayerTreeFlags>(flags));
  }

  if (flags & LayerTreeIncludesPaintInvalidations) {
    std::unique_ptr<JSONArray> objectPaintInvalidations =
        m_view->trackedObjectPaintInvalidationsAsJSON();
    if (objectPaintInvalidations && objectPaintInvalidations->size()) {
      if (!layers)
        layers = JSONObject::create();
      layers->setArray("objectPaintInvalidations",
                       std::move(objectPaintInvalidations));
    }
  }

  return layers ? layers->toPrettyJSONString() : String();
}

bool LocalFrame::shouldThrottleRendering() const {
  return view() && view()->shouldThrottleRendering();
}

inline LocalFrame::LocalFrame(LocalFrameClient* client,
                              FrameHost* host,
                              FrameOwner* owner,
                              InterfaceProvider* interfaceProvider,
                              InterfaceRegistry* interfaceRegistry)
    : Frame(client, host, owner),
      m_frameScheduler(page()->chromeClient().createFrameScheduler(
          client->frameBlameContext())),
      m_loader(this),
      m_navigationScheduler(NavigationScheduler::create(this)),
      m_script(ScriptController::create(this)),
      m_editor(Editor::create(*this)),
      m_spellChecker(SpellChecker::create(*this)),
      m_selection(FrameSelection::create(*this)),
      m_eventHandler(new EventHandler(*this)),
      m_console(FrameConsole::create(*this)),
      m_inputMethodController(InputMethodController::create(*this)),
      m_navigationDisableCount(0),
      m_pageZoomFactor(parentPageZoomFactor(this)),
      m_textZoomFactor(parentTextZoomFactor(this)),
      m_inViewSourceMode(false),
      m_interfaceProvider(interfaceProvider),
      m_interfaceRegistry(interfaceRegistry) {
  if (isLocalRoot()) {
    m_instrumentingAgents = new InstrumentingAgents();
    m_performanceMonitor = new PerformanceMonitor(this);
  } else {
    m_instrumentingAgents = localFrameRoot()->m_instrumentingAgents;
    m_performanceMonitor = localFrameRoot()->m_performanceMonitor;
  }
}

WebFrameScheduler* LocalFrame::frameScheduler() {
  return m_frameScheduler.get();
}

void LocalFrame::scheduleVisualUpdateUnlessThrottled() {
  if (shouldThrottleRendering())
    return;
  page()->animator().scheduleVisualUpdate(this);
}

LocalFrameClient* LocalFrame::client() const {
  return static_cast<LocalFrameClient*>(Frame::client());
}

PluginData* LocalFrame::pluginData() const {
  if (!loader().allowPlugins(NotAboutToInstantiatePlugin))
    return nullptr;
  return page()->pluginData(
      tree().top()->securityContext()->getSecurityOrigin());
}

DEFINE_WEAK_IDENTIFIER_MAP(LocalFrame);

FrameNavigationDisabler::FrameNavigationDisabler(LocalFrame& frame)
    : m_frame(&frame) {
  m_frame->disableNavigation();
}

FrameNavigationDisabler::~FrameNavigationDisabler() {
  m_frame->enableNavigation();
}

ScopedFrameBlamer::ScopedFrameBlamer(LocalFrame* frame) : m_frame(frame) {
  if (m_frame && m_frame->client() && m_frame->client()->frameBlameContext())
    m_frame->client()->frameBlameContext()->Enter();
}

ScopedFrameBlamer::~ScopedFrameBlamer() {
  if (m_frame && m_frame->client() && m_frame->client()->frameBlameContext())
    m_frame->client()->frameBlameContext()->Leave();
}

}  // namespace blink
