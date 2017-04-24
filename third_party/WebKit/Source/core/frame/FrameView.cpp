/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *           (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "core/frame/FrameView.h"

#include <algorithm>
#include <memory>
#include "core/HTMLNames.h"
#include "core/MediaTypeNames.h"
#include "core/animation/DocumentAnimations.h"
#include "core/css/FontFaceSet.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/DOMNodeIds.h"
#include "core/dom/ElementVisibilityObserver.h"
#include "core/dom/Fullscreen.h"
#include "core/dom/IntersectionObserverCallback.h"
#include "core/dom/IntersectionObserverController.h"
#include "core/dom/IntersectionObserverInit.h"
#include "core/dom/ResizeObserverController.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/editing/DragCaret.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/RenderedPosition.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/events/ErrorEvent.h"
#include "core/frame/BrowserControls.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Location.h"
#include "core/frame/PageScaleConstraintsSet.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/RemoteFrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLFrameElement.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/html/TextControlElement.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/input/EventHandler.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/LayoutCounter.h"
#include "core/layout/LayoutEmbeddedObject.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/LayoutScrollbar.h"
#include "core/layout/LayoutScrollbarPart.h"
#include "core/layout/LayoutView.h"
#include "core/layout/ScrollAlignment.h"
#include "core/layout/TextAutosizer.h"
#include "core/layout/TracedLayoutObject.h"
#include "core/layout/api/LayoutBoxModel.h"
#include "core/layout/api/LayoutItem.h"
#include "core/layout/api/LayoutPartItem.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/layout/compositing/CompositedSelection.h"
#include "core/layout/compositing/CompositingInputsUpdater.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/page/AutoscrollController.h"
#include "core/page/ChromeClient.h"
#include "core/page/FocusController.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/page/PrintContext.h"
#include "core/page/scrolling/RootScrollerUtil.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/page/scrolling/TopDocumentRootScrollerController.h"
#include "core/paint/BlockPaintInvalidator.h"
#include "core/paint/FramePainter.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintTiming.h"
#include "core/paint/PrePaintTreeWalk.h"
#include "core/plugins/PluginView.h"
#include "core/style/ComputedStyle.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGSVGElement.h"
#include "platform/Histogram.h"
#include "platform/HostWindow.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/WebFrameScheduler.h"
#include "platform/fonts/FontCache.h"
#include "platform/geometry/DoubleRect.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/geometry/TransformState.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/GraphicsLayerDebugInfo.h"
#include "platform/graphics/compositing/PaintArtifactCompositor.h"
#include "platform/graphics/paint/CullRect.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/graphics/paint/ScopedPaintChunkProperties.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/instrumentation/tracing/TracedValue.h"
#include "platform/json/JSONValues.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/scroll/ScrollAnimatorBase.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/text/TextStream.h"
#include "public/platform/WebDisplayItemList.h"
#include "wtf/CurrentTime.h"
#include "wtf/PtrUtil.h"
#include "wtf/StdLibExtras.h"

// Used to check for dirty layouts violating document lifecycle rules.
// If arg evaluates to true, the program will continue. If arg evaluates to
// false, program will crash if DCHECK_IS_ON() or return false from the current
// function.
#define CHECK_FOR_DIRTY_LAYOUT(arg) \
  do {                              \
    if (!(arg)) {                   \
      NOTREACHED();                 \
      return false;                 \
    }                               \
  } while (false)

namespace blink {

// A4 Portrait dimensions in pixels
const int kA4PortraitPageWidth = 595;
const int kA4PortraitPageHeight = 842;

using namespace HTMLNames;

// The maximum number of updateWidgets iterations that should be done before
// returning.
static const unsigned maxUpdateWidgetsIterations = 2;
static const double resourcePriorityUpdateDelayAfterScroll = 0.250;

static bool s_initialTrackAllPaintInvalidations = false;

FrameView::FrameView(LocalFrame& frame)
    : m_frame(frame),
      m_displayMode(WebDisplayModeBrowser),
      m_canHaveScrollbars(true),
      m_hasPendingLayout(false),
      m_inSynchronousPostLayout(false),
      m_postLayoutTasksTimer(
          TaskRunnerHelper::get(TaskType::UnspecedTimer, &frame),
          this,
          &FrameView::postLayoutTimerFired),
      m_updateWidgetsTimer(
          TaskRunnerHelper::get(TaskType::UnspecedTimer, &frame),
          this,
          &FrameView::updateWidgetsTimerFired),
      m_isTransparent(false),
      m_baseBackgroundColor(Color::white),
      m_mediaType(MediaTypeNames::screen),
      m_safeToPropagateScrollToParent(true),
      m_scrollCorner(nullptr),
      m_stickyPositionObjectCount(0),
      m_inputEventsScaleFactorForEmulation(1),
      m_layoutSizeFixedToFrameSize(true),
      m_didScrollTimer(TaskRunnerHelper::get(TaskType::UnspecedTimer, &frame),
                       this,
                       &FrameView::didScrollTimerFired),
      m_needsUpdateWidgetGeometries(false),
      m_horizontalScrollbarMode(ScrollbarAuto),
      m_verticalScrollbarMode(ScrollbarAuto),
      m_horizontalScrollbarLock(false),
      m_verticalScrollbarLock(false),
      m_scrollbarsSuppressed(false),
      m_inUpdateScrollbars(false),
      m_frameTimingRequestsDirty(true),
      m_hiddenForThrottling(false),
      m_subtreeThrottled(false),
      m_lifecycleUpdatesThrottled(false),
      m_needsPaintPropertyUpdate(true),
      m_currentUpdateLifecyclePhasesTargetState(
          DocumentLifecycle::Uninitialized),
      m_scrollAnchor(this),
      m_scrollbarManager(*this),
      m_needsScrollbarsUpdate(false),
      m_suppressAdjustViewSize(false),
      m_allowsLayoutInvalidationAfterLayoutClean(true),
      m_mainThreadScrollingReasons(0),
      m_mainThreadScrollingReasonsCounter(
          MainThreadScrollingReason::kMainThreadScrollingReasonCount,
          0) {
  init();
}

FrameView* FrameView::create(LocalFrame& frame) {
  FrameView* view = new FrameView(frame);
  view->show();
  return view;
}

FrameView* FrameView::create(LocalFrame& frame, const IntSize& initialSize) {
  FrameView* view = new FrameView(frame);
  view->FrameViewBase::setFrameRect(IntRect(view->location(), initialSize));
  view->setLayoutSizeInternal(initialSize);

  view->show();
  return view;
}

FrameView::~FrameView() {
  ASSERT(m_hasBeenDisposed);
}

DEFINE_TRACE(FrameView) {
  visitor->trace(m_frame);
  visitor->trace(m_fragmentAnchor);
  visitor->trace(m_scrollableAreas);
  visitor->trace(m_animatingScrollableAreas);
  visitor->trace(m_autoSizeInfo);
  visitor->trace(m_children);
  visitor->trace(m_viewportScrollableArea);
  visitor->trace(m_visibilityObserver);
  visitor->trace(m_scrollAnchor);
  visitor->trace(m_anchoringAdjustmentQueue);
  visitor->trace(m_scrollbarManager);
  visitor->trace(m_printContext);
  FrameViewBase::trace(visitor);
  ScrollableArea::trace(visitor);
}

void FrameView::reset() {
  // The compositor throttles the main frame using deferred commits, we can't
  // throttle it here or it seems the root compositor doesn't get setup
  // properly.
  if (RuntimeEnabledFeatures::
          renderingPipelineThrottlingLoadingIframesEnabled())
    m_lifecycleUpdatesThrottled = !frame().isMainFrame();
  m_hasPendingLayout = false;
  m_layoutSchedulingEnabled = true;
  m_inSynchronousPostLayout = false;
  m_layoutCount = 0;
  m_nestedLayoutCount = 0;
  m_postLayoutTasksTimer.stop();
  m_updateWidgetsTimer.stop();
  m_firstLayout = true;
  m_safeToPropagateScrollToParent = true;
  m_lastViewportSize = IntSize();
  m_lastZoomFactor = 1.0f;
  m_trackedObjectPaintInvalidations = WTF::wrapUnique(
      s_initialTrackAllPaintInvalidations ? new Vector<ObjectPaintInvalidation>
                                          : nullptr);
  m_visuallyNonEmptyCharacterCount = 0;
  m_visuallyNonEmptyPixelCount = 0;
  m_isVisuallyNonEmpty = false;
  m_mainThreadScrollingReasons = 0;
  m_layoutObjectCounter.reset();
  clearFragmentAnchor();
  m_viewportConstrainedObjects.reset();
  m_layoutSubtreeRootList.clear();
  m_orthogonalWritingModeRootList.clear();
}

// Call function for each non-throttled frame view in pre tree order.
// Note it needs a null check of the frame's layoutView to access it in case of
// detached frames.
template <typename Function>
void FrameView::forAllNonThrottledFrameViews(const Function& function) {
  if (shouldThrottleRendering())
    return;

  function(*this);

  for (Frame* child = m_frame->tree().firstChild(); child;
       child = child->tree().nextSibling()) {
    if (!child->isLocalFrame())
      continue;
    if (FrameView* childView = toLocalFrame(child)->view())
      childView->forAllNonThrottledFrameViews(function);
  }
}

void FrameView::init() {
  reset();

  m_size = LayoutSize();

  // Propagate the marginwidth/height and scrolling modes to the view.
  if (m_frame->owner() &&
      m_frame->owner()->scrollingMode() == ScrollbarAlwaysOff)
    setCanHaveScrollbars(false);
}

void FrameView::setupRenderThrottling() {
  if (m_visibilityObserver)
    return;

  // We observe the frame owner element instead of the document element, because
  // if the document has no content we can falsely think the frame is invisible.
  // Note that this means we cannot throttle top-level frames or (currently)
  // frames whose owner element is remote.
  Element* targetElement = frame().deprecatedLocalOwner();
  if (!targetElement)
    return;

  m_visibilityObserver = new ElementVisibilityObserver(
      targetElement, WTF::bind(
                         [](FrameView* frameView, bool isVisible) {
                           if (!frameView)
                             return;
                           frameView->updateRenderThrottlingStatus(
                               !isVisible, frameView->m_subtreeThrottled);
                         },
                         wrapWeakPersistent(this)));
  m_visibilityObserver->start();
}

void FrameView::dispose() {
  RELEASE_ASSERT(!isInPerformLayout());

  if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator())
    scrollAnimator->cancelAnimation();
  cancelProgrammaticScrollAnimation();

  detachScrollbars();

  if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
    scrollingCoordinator->willDestroyScrollableArea(this);

  FrameHost* frameHost = m_frame->host();
  // TODO(dcheng): It's wrong that the frame can be detached before the
  // FrameView. Figure out what's going on and fix FrameView to be disposed with
  // the correct timing.
  if (frameHost)
    frameHost->globalRootScrollerController().didDisposeScrollableArea(*this);

  // We need to clear the RootFrameViewport's animator since it gets called
  // from non-GC'd objects and RootFrameViewport will still have a pointer to
  // this class.
  if (m_viewportScrollableArea)
    m_viewportScrollableArea->clearScrollableArea();

  clearScrollableArea();

  // Destroy |m_autoSizeInfo| as early as possible, to avoid dereferencing
  // partially destroyed |this| via |m_autoSizeInfo->m_frameView|.
  m_autoSizeInfo.clear();

  m_postLayoutTasksTimer.stop();
  m_didScrollTimer.stop();

  // FIXME: Do we need to do something here for OOPI?
  HTMLFrameOwnerElement* ownerElement = m_frame->deprecatedLocalOwner();
  // TODO(dcheng): It seems buggy that we can have an owner element that points
  // to another FrameViewBase. This can happen when a plugin element loads a
  // frame (FrameViewBase A of type FrameView) and then loads a plugin
  // (FrameViewBase B of type WebPluginContainerImpl). In this case, the frame's
  // view is A and the frame element's owned FrameViewBase is B. See
  // https://crbug.com/673170 for an example.
  if (ownerElement && ownerElement->ownedWidget() == this)
    ownerElement->setWidget(nullptr);

#if DCHECK_IS_ON()
  m_hasBeenDisposed = true;
#endif
}

void FrameView::detachScrollbars() {
  // Previously, we detached custom scrollbars as early as possible to prevent
  // Document::detachLayoutTree() from messing with the view such that its
  // scroll bars won't be torn down. However, scripting in
  // Document::detachLayoutTree() is forbidden
  // now, so it's not clear if these edge cases can still happen.
  // However, for Oilpan, we still need to remove the native scrollbars before
  // we lose the connection to the HostWindow, so we just unconditionally
  // detach any scrollbars now.
  m_scrollbarManager.dispose();

  if (m_scrollCorner) {
    m_scrollCorner->destroy();
    m_scrollCorner = nullptr;
  }
}

void FrameView::ScrollbarManager::setHasHorizontalScrollbar(bool hasScrollbar) {
  if (hasScrollbar == hasHorizontalScrollbar())
    return;

  if (hasScrollbar) {
    m_hBar = createScrollbar(HorizontalScrollbar);
    m_scrollableArea->layoutBox()->document().view()->addChild(m_hBar.get());
    m_hBarIsAttached = 1;
    m_scrollableArea->didAddScrollbar(*m_hBar, HorizontalScrollbar);
    m_hBar->styleChanged();
  } else {
    m_hBarIsAttached = 0;
    destroyScrollbar(HorizontalScrollbar);
  }

  m_scrollableArea->setScrollCornerNeedsPaintInvalidation();
}

void FrameView::ScrollbarManager::setHasVerticalScrollbar(bool hasScrollbar) {
  if (hasScrollbar == hasVerticalScrollbar())
    return;

  if (hasScrollbar) {
    m_vBar = createScrollbar(VerticalScrollbar);
    m_scrollableArea->layoutBox()->document().view()->addChild(m_vBar.get());
    m_vBarIsAttached = 1;
    m_scrollableArea->didAddScrollbar(*m_vBar, VerticalScrollbar);
    m_vBar->styleChanged();
  } else {
    m_vBarIsAttached = 0;
    destroyScrollbar(VerticalScrollbar);
  }

  m_scrollableArea->setScrollCornerNeedsPaintInvalidation();
}

Scrollbar* FrameView::ScrollbarManager::createScrollbar(
    ScrollbarOrientation orientation) {
  Element* customScrollbarElement = nullptr;
  LayoutBox* box = m_scrollableArea->layoutBox();
  if (box->document().view()->shouldUseCustomScrollbars(
          customScrollbarElement)) {
    return LayoutScrollbar::createCustomScrollbar(
        m_scrollableArea.get(), orientation, customScrollbarElement);
  }

  // Nobody set a custom style, so we just use a native scrollbar.
  return Scrollbar::create(m_scrollableArea.get(), orientation,
                           RegularScrollbar,
                           &box->frame()->page()->chromeClient());
}

void FrameView::ScrollbarManager::destroyScrollbar(
    ScrollbarOrientation orientation) {
  Member<Scrollbar>& scrollbar =
      orientation == HorizontalScrollbar ? m_hBar : m_vBar;
  DCHECK(orientation == HorizontalScrollbar ? !m_hBarIsAttached
                                            : !m_vBarIsAttached);
  if (!scrollbar)
    return;

  m_scrollableArea->willRemoveScrollbar(*scrollbar, orientation);
  m_scrollableArea->layoutBox()->document().view()->removeChild(
      scrollbar.get());
  scrollbar->disconnectFromScrollableArea();
  scrollbar = nullptr;
}

void FrameView::recalculateCustomScrollbarStyle() {
  bool didStyleChange = false;
  if (horizontalScrollbar() && horizontalScrollbar()->isCustomScrollbar()) {
    horizontalScrollbar()->styleChanged();
    didStyleChange = true;
  }
  if (verticalScrollbar() && verticalScrollbar()->isCustomScrollbar()) {
    verticalScrollbar()->styleChanged();
    didStyleChange = true;
  }
  if (didStyleChange) {
    updateScrollbarGeometry();
    updateScrollCorner();
    positionScrollbarLayers();
  }
}

void FrameView::invalidateAllCustomScrollbarsOnActiveChanged() {
  bool usesWindowInactiveSelector =
      m_frame->document()->styleEngine().usesWindowInactiveSelector();

  const ChildrenWidgetSet* viewChildren = children();
  for (const Member<FrameViewBase>& child : *viewChildren) {
    FrameViewBase* frameViewBase = child.get();
    if (frameViewBase->isFrameView()) {
      toFrameView(frameViewBase)
          ->invalidateAllCustomScrollbarsOnActiveChanged();
    } else if (usesWindowInactiveSelector && frameViewBase->isScrollbar() &&
               toScrollbar(frameViewBase)->isCustomScrollbar()) {
      toScrollbar(frameViewBase)->styleChanged();
    }
  }
  if (usesWindowInactiveSelector)
    recalculateCustomScrollbarStyle();
}

void FrameView::clear() {
  reset();
  setScrollbarsSuppressed(true);
}

bool FrameView::didFirstLayout() const {
  return !m_firstLayout;
}

void FrameView::invalidateRect(const IntRect& rect) {
  LayoutPartItem layoutItem = m_frame->ownerLayoutItem();
  if (layoutItem.isNull())
    return;

  IntRect paintInvalidationRect = rect;
  paintInvalidationRect.move(
      (layoutItem.borderLeft() + layoutItem.paddingLeft()).toInt(),
      (layoutItem.borderTop() + layoutItem.paddingTop()).toInt());
  // FIXME: We should not allow paint invalidation out of paint invalidation
  // state. crbug.com/457415
  DisablePaintInvalidationStateAsserts paintInvalidationAssertDisabler;
  layoutItem.invalidatePaintRectangle(LayoutRect(paintInvalidationRect));
}

void FrameView::setFrameRect(const IntRect& newRect) {
  IntRect oldRect = frameRect();
  if (newRect == oldRect)
    return;

  FrameViewBase::setFrameRect(newRect);

  const bool frameSizeChanged = oldRect.size() != newRect.size();

  m_needsScrollbarsUpdate = frameSizeChanged;
  // TODO(wjmaclean): find out why scrollbars fail to resize for complex
  // subframes after changing the zoom level. For now always calling
  // updateScrollbarsIfNeeded() here fixes the issue, but it would be good to
  // discover the deeper cause of this. http://crbug.com/607987.
  updateScrollbarsIfNeeded();

  frameRectsChanged();

  updateParentScrollableAreaSet();

  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled() &&
      !RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
    // The overflow clip property depends on the frame size and the pre
    // translation property depends on the frame location.
    setNeedsPaintPropertyUpdate();
  }

  if (auto layoutViewItem = this->layoutViewItem())
    layoutViewItem.setMayNeedPaintInvalidation();

  if (frameSizeChanged) {
    viewportSizeChanged(newRect.width() != oldRect.width(),
                        newRect.height() != oldRect.height());

    if (m_frame->isMainFrame())
      m_frame->host()->visualViewport().mainFrameDidChangeSize();

    frame().loader().restoreScrollPositionAndViewState();
  }
}

Page* FrameView::page() const {
  return frame().page();
}

LayoutView* FrameView::layoutView() const {
  return frame().contentLayoutObject();
}

LayoutViewItem FrameView::layoutViewItem() const {
  return LayoutViewItem(frame().contentLayoutObject());
}

ScrollingCoordinator* FrameView::scrollingCoordinator() const {
  Page* p = page();
  return p ? p->scrollingCoordinator() : 0;
}

CompositorAnimationHost* FrameView::compositorAnimationHost() const {
  // When m_animationHost is not nullptr, this is the FrameView for an OOPIF.
  if (m_animationHost)
    return m_animationHost.get();

  if (m_frame->localFrameRoot() != m_frame)
    return m_frame->localFrameRoot()->view()->compositorAnimationHost();

  if (!m_frame->isMainFrame())
    return nullptr;

  ScrollingCoordinator* c = scrollingCoordinator();
  return c ? c->compositorAnimationHost() : nullptr;
}

CompositorAnimationTimeline* FrameView::compositorAnimationTimeline() const {
  if (m_animationTimeline)
    return m_animationTimeline.get();

  if (m_frame->localFrameRoot() != m_frame)
    return m_frame->localFrameRoot()->view()->compositorAnimationTimeline();

  if (!m_frame->isMainFrame())
    return nullptr;

  ScrollingCoordinator* c = scrollingCoordinator();
  return c ? c->compositorAnimationTimeline() : nullptr;
}

LayoutBox* FrameView::layoutBox() const {
  return layoutView();
}

FloatQuad FrameView::localToVisibleContentQuad(
    const FloatQuad& quad,
    const LayoutObject* localObject,
    MapCoordinatesFlags flags) const {
  LayoutBox* box = layoutBox();
  if (!box)
    return quad;
  DCHECK(localObject);
  FloatQuad result = localObject->localToAncestorQuad(quad, box, flags);
  result.move(-getScrollOffset());
  return result;
}

RefPtr<WebTaskRunner> FrameView::getTimerTaskRunner() const {
  return TaskRunnerHelper::get(TaskType::UnspecedTimer, m_frame.get());
}

void FrameView::setCanHaveScrollbars(bool canHaveScrollbars) {
  m_canHaveScrollbars = canHaveScrollbars;

  ScrollbarMode newVerticalMode = m_verticalScrollbarMode;
  if (canHaveScrollbars && m_verticalScrollbarMode == ScrollbarAlwaysOff)
    newVerticalMode = ScrollbarAuto;
  else if (!canHaveScrollbars)
    newVerticalMode = ScrollbarAlwaysOff;

  ScrollbarMode newHorizontalMode = m_horizontalScrollbarMode;
  if (canHaveScrollbars && m_horizontalScrollbarMode == ScrollbarAlwaysOff)
    newHorizontalMode = ScrollbarAuto;
  else if (!canHaveScrollbars)
    newHorizontalMode = ScrollbarAlwaysOff;

  setScrollbarModes(newHorizontalMode, newVerticalMode);
}

bool FrameView::shouldUseCustomScrollbars(
    Element*& customScrollbarElement) const {
  customScrollbarElement = nullptr;

  if (Settings* settings = m_frame->settings()) {
    if (!settings->getAllowCustomScrollbarInMainFrame() &&
        m_frame->isMainFrame())
      return false;
  }
  Document* doc = m_frame->document();

  // Try the <body> element first as a scrollbar source.
  Element* body = doc ? doc->body() : 0;
  if (body && body->layoutObject() &&
      body->layoutObject()->style()->hasPseudoStyle(PseudoIdScrollbar)) {
    customScrollbarElement = body;
    return true;
  }

  // If the <body> didn't have a custom style, then the root element might.
  Element* docElement = doc ? doc->documentElement() : 0;
  if (docElement && docElement->layoutObject() &&
      docElement->layoutObject()->style()->hasPseudoStyle(PseudoIdScrollbar)) {
    customScrollbarElement = docElement;
    return true;
  }

  return false;
}

Scrollbar* FrameView::createScrollbar(ScrollbarOrientation orientation) {
  return m_scrollbarManager.createScrollbar(orientation);
}

void FrameView::setContentsSize(const IntSize& size) {
  if (size == contentsSize())
    return;

  m_contentsSize = size;
  updateScrollbars();
  ScrollableArea::contentsResized();

  Page* page = frame().page();
  if (!page)
    return;

  updateParentScrollableAreaSet();

  page->chromeClient().contentsSizeChanged(m_frame.get(), size);

  // Ensure the scrollToFragmentAnchor is called before
  // restoreScrollPositionAndViewState when reload
  scrollToFragmentAnchor();
  frame().loader().restoreScrollPositionAndViewState();
}

void FrameView::adjustViewSize() {
  if (m_suppressAdjustViewSize)
    return;

  LayoutViewItem layoutViewItem = this->layoutViewItem();
  if (layoutViewItem.isNull())
    return;

  ASSERT(m_frame->view() == this);

  const IntRect rect = layoutViewItem.documentRect();
  const IntSize& size = rect.size();

  const IntPoint origin(-rect.x(), -rect.y());
  if (scrollOrigin() != origin) {
    ScrollableArea::setScrollOrigin(origin);
    // setContentSize (below) also calls updateScrollbars so we can avoid
    // updating scrollbars twice by skipping the call here when the content
    // size does not change.
    if (!m_frame->document()->printing() && size == contentsSize())
      updateScrollbars();
  }

  setContentsSize(size);
}

void FrameView::adjustViewSizeAndLayout() {
  adjustViewSize();
  if (needsLayout()) {
    AutoReset<bool> suppressAdjustViewSize(&m_suppressAdjustViewSize, true);
    layout();
  }
}

void FrameView::calculateScrollbarModesFromOverflowStyle(
    const ComputedStyle* style,
    ScrollbarMode& hMode,
    ScrollbarMode& vMode) const {
  hMode = vMode = ScrollbarAuto;

  EOverflow overflowX = style->overflowX();
  EOverflow overflowY = style->overflowY();

  if (!shouldIgnoreOverflowHidden()) {
    if (overflowX == EOverflow::kHidden)
      hMode = ScrollbarAlwaysOff;
    if (overflowY == EOverflow::kHidden)
      vMode = ScrollbarAlwaysOff;
  }

  if (overflowX == EOverflow::kScroll)
    hMode = ScrollbarAlwaysOn;
  if (overflowY == EOverflow::kScroll)
    vMode = ScrollbarAlwaysOn;
}

void FrameView::calculateScrollbarModes(
    ScrollbarMode& hMode,
    ScrollbarMode& vMode,
    ScrollbarModesCalculationStrategy strategy) const {
#define RETURN_SCROLLBAR_MODE(mode) \
  {                                 \
    hMode = vMode = mode;           \
    return;                         \
  }

  // Setting scrolling="no" on an iframe element disables scrolling.
  if (m_frame->owner() &&
      m_frame->owner()->scrollingMode() == ScrollbarAlwaysOff)
    RETURN_SCROLLBAR_MODE(ScrollbarAlwaysOff);

  // Framesets can't scroll.
  Node* body = m_frame->document()->body();
  if (isHTMLFrameSetElement(body) && body->layoutObject())
    RETURN_SCROLLBAR_MODE(ScrollbarAlwaysOff);

  // Scrollbars can be disabled by FrameView::setCanHaveScrollbars.
  if (!m_canHaveScrollbars && strategy != RulesFromWebContentOnly)
    RETURN_SCROLLBAR_MODE(ScrollbarAlwaysOff);

  // This will be the LayoutObject for either the body element or the html
  // element (see Document::viewportDefiningElement).
  LayoutObject* viewport = viewportLayoutObject();
  if (!viewport || !viewport->style())
    RETURN_SCROLLBAR_MODE(ScrollbarAuto);

  if (viewport->isSVGRoot()) {
    // Don't allow overflow to affect <img> and css backgrounds
    if (toLayoutSVGRoot(viewport)->isEmbeddedThroughSVGImage())
      RETURN_SCROLLBAR_MODE(ScrollbarAuto);

    // FIXME: evaluate if we can allow overflow for these cases too.
    // Overflow is always hidden when stand-alone SVG documents are embedded.
    if (toLayoutSVGRoot(viewport)
            ->isEmbeddedThroughFrameContainingSVGDocument())
      RETURN_SCROLLBAR_MODE(ScrollbarAlwaysOff);
  }

  calculateScrollbarModesFromOverflowStyle(viewport->style(), hMode, vMode);

#undef RETURN_SCROLLBAR_MODE
}

void FrameView::updateAcceleratedCompositingSettings() {
  if (LayoutViewItem layoutViewItem = this->layoutViewItem())
    layoutViewItem.compositor()->updateAcceleratedCompositingSettings();
}

void FrameView::recalcOverflowAfterStyleChange() {
  LayoutViewItem layoutViewItem = this->layoutViewItem();
  RELEASE_ASSERT(!layoutViewItem.isNull());
  if (!layoutViewItem.needsOverflowRecalcAfterStyleChange())
    return;

  layoutViewItem.recalcOverflowAfterStyleChange();

  // Changing overflow should notify scrolling coordinator to ensures that it
  // updates non-fast scroll rects even if there is no layout.
  if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
    scrollingCoordinator->notifyOverflowUpdated();

  IntRect documentRect = layoutViewItem.documentRect();
  if (scrollOrigin() == -documentRect.location() &&
      contentsSize() == documentRect.size())
    return;

  if (needsLayout())
    return;

  // If the visualViewport supplies scrollbars, we won't get a paint
  // invalidation from computeScrollbarExistence so we need to force one.
  if (visualViewportSuppliesScrollbars())
    layoutViewItem.setMayNeedPaintInvalidation();

  // TODO(pdr): This should be refactored to just block scrollbar updates as
  // we are not in a scrollbar update here and m_inUpdateScrollbars has other
  // side effects. This scope is only for preventing a synchronous layout from
  // scroll origin changes which would not be allowed during style recalc.
  InUpdateScrollbarsScope inUpdateScrollbarsScope(this);

  bool shouldHaveHorizontalScrollbar = false;
  bool shouldHaveVerticalScrollbar = false;
  computeScrollbarExistence(shouldHaveHorizontalScrollbar,
                            shouldHaveVerticalScrollbar, documentRect.size());

  bool hasHorizontalScrollbar = horizontalScrollbar();
  bool hasVerticalScrollbar = verticalScrollbar();
  if (hasHorizontalScrollbar != shouldHaveHorizontalScrollbar ||
      hasVerticalScrollbar != shouldHaveVerticalScrollbar) {
    setNeedsLayout();
    return;
  }

  adjustViewSize();
  updateScrollbarGeometry();

  if (scrollOriginChanged())
    setNeedsLayout();
}

bool FrameView::usesCompositedScrolling() const {
  LayoutViewItem layoutView = this->layoutViewItem();
  if (layoutView.isNull())
    return false;
  if (m_frame->settings() &&
      m_frame->settings()->getPreferCompositingToLCDTextEnabled())
    return layoutView.compositor()->inCompositingMode();
  return false;
}

bool FrameView::shouldScrollOnMainThread() const {
  if (mainThreadScrollingReasons())
    return true;
  return ScrollableArea::shouldScrollOnMainThread();
}

GraphicsLayer* FrameView::layerForScrolling() const {
  LayoutViewItem layoutView = this->layoutViewItem();
  if (layoutView.isNull())
    return nullptr;
  return layoutView.compositor()->frameScrollLayer();
}

GraphicsLayer* FrameView::layerForHorizontalScrollbar() const {
  LayoutViewItem layoutView = this->layoutViewItem();
  if (layoutView.isNull())
    return nullptr;
  return layoutView.compositor()->layerForHorizontalScrollbar();
}

GraphicsLayer* FrameView::layerForVerticalScrollbar() const {
  LayoutViewItem layoutView = this->layoutViewItem();
  if (layoutView.isNull())
    return nullptr;
  return layoutView.compositor()->layerForVerticalScrollbar();
}

GraphicsLayer* FrameView::layerForScrollCorner() const {
  LayoutViewItem layoutView = this->layoutViewItem();
  if (layoutView.isNull())
    return nullptr;
  return layoutView.compositor()->layerForScrollCorner();
}

bool FrameView::isEnclosedInCompositingLayer() const {
  // FIXME: It's a bug that compositing state isn't always up to date when this
  // is called. crbug.com/366314
  DisableCompositingQueryAsserts disabler;

  LayoutItem frameOwnerLayoutItem = m_frame->ownerLayoutItem();
  return !frameOwnerLayoutItem.isNull() &&
         frameOwnerLayoutItem.enclosingLayer()
             ->enclosingLayerForPaintInvalidationCrossingFrameBoundaries();
}

void FrameView::countObjectsNeedingLayout(unsigned& needsLayoutObjects,
                                          unsigned& totalObjects,
                                          bool& isSubtree) {
  needsLayoutObjects = 0;
  totalObjects = 0;
  isSubtree = isSubtreeLayout();
  if (isSubtree)
    m_layoutSubtreeRootList.countObjectsNeedingLayout(needsLayoutObjects,
                                                      totalObjects);
  else
    LayoutSubtreeRootList::countObjectsNeedingLayoutInRoot(
        layoutView(), needsLayoutObjects, totalObjects);
}

inline void FrameView::forceLayoutParentViewIfNeeded() {
  LayoutPartItem ownerLayoutItem = m_frame->ownerLayoutItem();
  if (ownerLayoutItem.isNull() || !ownerLayoutItem.frame())
    return;

  LayoutReplaced* contentBox = embeddedReplacedContent();
  if (!contentBox)
    return;

  LayoutSVGRoot* svgRoot = toLayoutSVGRoot(contentBox);
  if (svgRoot->everHadLayout() && !svgRoot->needsLayout())
    return;

  // If the embedded SVG document appears the first time, the ownerLayoutObject
  // has already finished layout without knowing about the existence of the
  // embedded SVG document, because LayoutReplaced embeddedReplacedContent()
  // returns 0, as long as the embedded document isn't loaded yet. Before
  // bothering to lay out the SVG document, mark the ownerLayoutObject needing
  // layout and ask its FrameView for a layout. After that the
  // LayoutEmbeddedObject (ownerLayoutObject) carries the correct size, which
  // LayoutSVGRoot::computeReplacedLogicalWidth/Height rely on, when laying out
  // for the first time, or when the LayoutSVGRoot size has changed dynamically
  // (eg. via <script>).
  FrameView* frameView = ownerLayoutItem.frame()->view();

  // Mark the owner layoutObject as needing layout.
  ownerLayoutItem.setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
      LayoutInvalidationReason::Unknown);

  // Synchronously enter layout, to layout the view containing the host
  // object/embed/iframe.
  ASSERT(frameView);
  frameView->layout();
}

void FrameView::performPreLayoutTasks() {
  TRACE_EVENT0("blink,benchmark", "FrameView::performPreLayoutTasks");
  lifecycle().advanceTo(DocumentLifecycle::InPreLayout);

  // Don't schedule more layouts, we're in one.
  AutoReset<bool> changeSchedulingEnabled(&m_layoutSchedulingEnabled, false);

  if (!m_nestedLayoutCount && !m_inSynchronousPostLayout &&
      m_postLayoutTasksTimer.isActive()) {
    // This is a new top-level layout. If there are any remaining tasks from the
    // previous layout, finish them now.
    m_inSynchronousPostLayout = true;
    performPostLayoutTasks();
    m_inSynchronousPostLayout = false;
  }

  bool wasResized = wasViewportResized();
  Document* document = m_frame->document();
  if (wasResized)
    document->setResizedForViewportUnits();

  // Viewport-dependent or device-dependent media queries may cause us to need
  // completely different style information.
  bool mainFrameRotation =
      m_frame->isMainFrame() && m_frame->settings() &&
      m_frame->settings()->getMainFrameResizesAreOrientationChanges();
  if ((wasResized &&
       document->styleEngine().mediaQueryAffectedByViewportChange()) ||
      (wasResized && mainFrameRotation &&
       document->styleEngine().mediaQueryAffectedByDeviceChange())) {
    document->mediaQueryAffectingValueChanged();
  } else if (wasResized) {
    document->evaluateMediaQueryList();
  }

  document->updateStyleAndLayoutTree();
  lifecycle().advanceTo(DocumentLifecycle::StyleClean);

  if (wasResized)
    document->clearResizedForViewportUnits();

  if (shouldPerformScrollAnchoring())
    m_scrollAnchor.notifyBeforeLayout();
}

bool FrameView::shouldPerformScrollAnchoring() const {
  return RuntimeEnabledFeatures::scrollAnchoringEnabled() &&
         !RuntimeEnabledFeatures::rootLayerScrollingEnabled() &&
         m_scrollAnchor.hasScroller() &&
         layoutBox()->style()->overflowAnchor() != EOverflowAnchor::kNone &&
         !m_frame->document()->finishingOrIsPrinting();
}

static inline void layoutFromRootObject(LayoutObject& root) {
  LayoutState layoutState(root);
  root.layout();
}

void FrameView::prepareLayoutAnalyzer() {
  bool isTracing = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("blink.debug.layout"), &isTracing);
  if (!isTracing) {
    m_analyzer.reset();
    return;
  }
  if (!m_analyzer)
    m_analyzer = WTF::makeUnique<LayoutAnalyzer>();
  m_analyzer->reset();
}

std::unique_ptr<TracedValue> FrameView::analyzerCounters() {
  if (!m_analyzer)
    return TracedValue::create();
  std::unique_ptr<TracedValue> value = m_analyzer->toTracedValue();
  value->setString("host", layoutViewItem().document().location()->host());
  value->setString("frame",
                   String::format("0x%" PRIxPTR,
                                  reinterpret_cast<uintptr_t>(m_frame.get())));
  value->setInteger("contentsHeightAfterLayout",
                    layoutViewItem().documentRect().height());
  value->setInteger("visibleHeight", visibleHeight());
  value->setInteger(
      "approximateBlankCharacterCount",
      FontFaceSet::approximateBlankCharacterCount(*m_frame->document()));
  return value;
}

#define PERFORM_LAYOUT_TRACE_CATEGORIES \
  "blink,benchmark,rail," TRACE_DISABLED_BY_DEFAULT("blink.debug.layout")

void FrameView::performLayout(bool inSubtreeLayout) {
  ASSERT(inSubtreeLayout || m_layoutSubtreeRootList.isEmpty());

  int contentsHeightBeforeLayout = layoutViewItem().documentRect().height();
  TRACE_EVENT_BEGIN1(PERFORM_LAYOUT_TRACE_CATEGORIES,
                     "FrameView::performLayout", "contentsHeightBeforeLayout",
                     contentsHeightBeforeLayout);
  prepareLayoutAnalyzer();

  ScriptForbiddenScope forbidScript;

  if (inSubtreeLayout && hasOrthogonalWritingModeRoots()) {
    // If we're going to lay out from each subtree root, rather than once from
    // LayoutView, we need to merge the depth-ordered orthogonal writing mode
    // root list into the depth-ordered list of subtrees scheduled for
    // layout. Otherwise, during layout of one such subtree, we'd risk skipping
    // over a subtree of objects needing layout.
    DCHECK(!m_layoutSubtreeRootList.isEmpty());
    scheduleOrthogonalWritingModeRootsForLayout();
  }

  ASSERT(!isInPerformLayout());
  lifecycle().advanceTo(DocumentLifecycle::InPerformLayout);

  // performLayout is the actual guts of layout().
  // FIXME: The 300 other lines in layout() probably belong in other helper
  // functions so that a single human could understand what layout() is actually
  // doing.

  forceLayoutParentViewIfNeeded();

  if (inSubtreeLayout) {
    if (m_analyzer)
      m_analyzer->increment(LayoutAnalyzer::PerformLayoutRootLayoutObjects,
                            m_layoutSubtreeRootList.size());
    for (auto& root : m_layoutSubtreeRootList.ordered()) {
      if (!root->needsLayout())
        continue;
      layoutFromRootObject(*root);

      // We need to ensure that we mark up all layoutObjects up to the
      // LayoutView for paint invalidation. This simplifies our code as we just
      // always do a full tree walk.
      if (LayoutItem container = LayoutItem(root->container()))
        container.setMayNeedPaintInvalidation();
    }
    m_layoutSubtreeRootList.clear();
  } else {
    if (hasOrthogonalWritingModeRoots() &&
        !RuntimeEnabledFeatures::layoutNGEnabled())
      layoutOrthogonalWritingModeRoots();
    layoutView()->layout();
  }

  m_frame->document()->fetcher()->updateAllImageResourcePriorities();

  lifecycle().advanceTo(DocumentLifecycle::AfterPerformLayout);

  TRACE_EVENT_END1(PERFORM_LAYOUT_TRACE_CATEGORIES, "FrameView::performLayout",
                   "counters", analyzerCounters());
  FirstMeaningfulPaintDetector::from(*m_frame->document())
      .markNextPaintAsMeaningfulIfNeeded(
          m_layoutObjectCounter, contentsHeightBeforeLayout,
          layoutViewItem().documentRect().height(), visibleHeight());
}

void FrameView::scheduleOrPerformPostLayoutTasks() {
  if (m_postLayoutTasksTimer.isActive())
    return;

  if (!m_inSynchronousPostLayout) {
    m_inSynchronousPostLayout = true;
    // Calls resumeScheduledEvents()
    performPostLayoutTasks();
    m_inSynchronousPostLayout = false;
  }

  if (!m_postLayoutTasksTimer.isActive() &&
      (needsLayout() || m_inSynchronousPostLayout)) {
    // If we need layout or are already in a synchronous call to
    // postLayoutTasks(), defer FrameViewBase updates and event dispatch until
    // after we return.  postLayoutTasks() can make us need to update again, and
    // we can get stuck in a nasty cycle unless we call it through the timer
    // here.
    m_postLayoutTasksTimer.startOneShot(0, BLINK_FROM_HERE);
    if (needsLayout())
      layout();
  }
}

void FrameView::layout() {
  // We should never layout a Document which is not in a LocalFrame.
  ASSERT(m_frame);
  ASSERT(m_frame->view() == this);
  ASSERT(m_frame->page());

  ScriptForbiddenScope forbidScript;

  if (isInPerformLayout() || shouldThrottleRendering() ||
      !m_frame->document()->isActive())
    return;

  TRACE_EVENT0("blink,benchmark", "FrameView::layout");

  if (m_autoSizeInfo)
    m_autoSizeInfo->autoSizeIfNeeded();

  m_hasPendingLayout = false;
  DocumentLifecycle::Scope lifecycleScope(lifecycle(),
                                          DocumentLifecycle::LayoutClean);

  Document* document = m_frame->document();
  TRACE_EVENT_BEGIN1("devtools.timeline", "Layout", "beginData",
                     InspectorLayoutEvent::beginData(this));
  probe::willUpdateLayout(document);

  performPreLayoutTasks();

  // TODO(crbug.com/460956): The notion of a single root for layout is no longer
  // applicable. Remove or update this code.
  LayoutObject* rootForThisLayout = layoutView();

  FontCachePurgePreventer fontCachePurgePreventer;
  {
    AutoReset<bool> changeSchedulingEnabled(&m_layoutSchedulingEnabled, false);
    m_nestedLayoutCount++;

    updateCounters();

    // If the layout view was marked as needing layout after we added items in
    // the subtree roots we need to clear the roots and do the layout from the
    // layoutView.
    if (layoutViewItem().needsLayout())
      clearLayoutSubtreeRootsAndMarkContainingBlocks();
    layoutViewItem().clearHitTestCache();

    bool inSubtreeLayout = isSubtreeLayout();

    // TODO(crbug.com/460956): The notion of a single root for layout is no
    // longer applicable. Remove or update this code.
    if (inSubtreeLayout)
      rootForThisLayout = m_layoutSubtreeRootList.randomRoot();

    if (!rootForThisLayout) {
      // FIXME: Do we need to set m_size here?
      NOTREACHED();
      return;
    }

    if (!inSubtreeLayout) {
      clearLayoutSubtreeRootsAndMarkContainingBlocks();
      Node* body = document->body();
      if (body && body->layoutObject()) {
        if (isHTMLFrameSetElement(*body)) {
          body->layoutObject()->setChildNeedsLayout();
        } else if (isHTMLBodyElement(*body)) {
          if (!m_firstLayout && m_size.height() != layoutSize().height() &&
              body->layoutObject()->enclosingBox()->stretchesToViewport())
            body->layoutObject()->setChildNeedsLayout();
        }
      }

      ScrollbarMode hMode;
      ScrollbarMode vMode;
      calculateScrollbarModes(hMode, vMode);

      // Now set our scrollbar state for the layout.
      ScrollbarMode currentHMode = horizontalScrollbarMode();
      ScrollbarMode currentVMode = verticalScrollbarMode();

      if (m_firstLayout) {
        setScrollbarsSuppressed(true);

        m_firstLayout = false;
        m_lastViewportSize = layoutSize(IncludeScrollbars);
        m_lastZoomFactor = layoutViewItem().style()->zoom();

        // Set the initial vMode to AlwaysOn if we're auto.
        if (vMode == ScrollbarAuto) {
          // This causes a vertical scrollbar to appear.
          setVerticalScrollbarMode(ScrollbarAlwaysOn);
        }
        // Set the initial hMode to AlwaysOff if we're auto.
        if (hMode == ScrollbarAuto) {
          // This causes a horizontal scrollbar to disappear.
          setHorizontalScrollbarMode(ScrollbarAlwaysOff);
        }

        setScrollbarModes(hMode, vMode);
        setScrollbarsSuppressed(false);
      } else if (hMode != currentHMode || vMode != currentVMode) {
        setScrollbarModes(hMode, vMode);
      }

      updateScrollbarsIfNeeded();

      LayoutSize oldSize = m_size;

      m_size = LayoutSize(layoutSize());

      if (oldSize != m_size && !m_firstLayout) {
        LayoutBox* rootLayoutObject =
            document->documentElement()
                ? document->documentElement()->layoutBox()
                : 0;
        LayoutBox* bodyLayoutObject = rootLayoutObject && document->body()
                                          ? document->body()->layoutBox()
                                          : 0;
        if (bodyLayoutObject && bodyLayoutObject->stretchesToViewport())
          bodyLayoutObject->setChildNeedsLayout();
        else if (rootLayoutObject && rootLayoutObject->stretchesToViewport())
          rootLayoutObject->setChildNeedsLayout();
      }
    }

    TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
        TRACE_DISABLED_BY_DEFAULT("blink.debug.layout.trees"), "LayoutTree",
        this, TracedLayoutObject::create(*layoutView(), false));

    performLayout(inSubtreeLayout);

    if (!inSubtreeLayout && !document->printing())
      adjustViewSizeAndLayout();

    ASSERT(m_layoutSubtreeRootList.isEmpty());
  }  // Reset m_layoutSchedulingEnabled to its previous value.
  checkDoesNotNeedLayout();

  m_frameTimingRequestsDirty = true;

  // FIXME: Could find the common ancestor layer of all dirty subtrees and mark
  // from there. crbug.com/462719
  layoutViewItem().enclosingLayer()->updateLayerPositionsAfterLayout();

  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("blink.debug.layout.trees"), "LayoutTree", this,
      TracedLayoutObject::create(*layoutView(), true));

  layoutViewItem().compositor()->didLayout();

  m_layoutCount++;

  if (AXObjectCache* cache = document->axObjectCache()) {
    const KURL& url = document->url();
    if (url.isValid() && !url.isAboutBlankURL())
      cache->handleLayoutComplete(document);
  }
  updateDocumentAnnotatedRegions();
  checkDoesNotNeedLayout();

  scheduleOrPerformPostLayoutTasks();
  checkDoesNotNeedLayout();

  // FIXME: The notion of a single root for layout is no longer applicable.
  // Remove or update this code. crbug.com/460596
  TRACE_EVENT_END1("devtools.timeline", "Layout", "endData",
                   InspectorLayoutEvent::endData(rootForThisLayout));
  probe::didUpdateLayout(m_frame.get());

  m_nestedLayoutCount--;
  if (m_nestedLayoutCount)
    return;

#if DCHECK_IS_ON()
  // Post-layout assert that nobody was re-marked as needing layout during
  // layout.
  layoutView()->assertSubtreeIsLaidOut();
#endif

  frame().document()->layoutUpdated();
  checkDoesNotNeedLayout();
}

void FrameView::invalidateTreeIfNeeded(
    const PaintInvalidationState& paintInvalidationState) {
  DCHECK(!RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled());

  if (shouldThrottleRendering())
    return;

  lifecycle().advanceTo(DocumentLifecycle::InPaintInvalidation);

  RELEASE_ASSERT(!layoutViewItem().isNull());
  LayoutViewItem rootForPaintInvalidation = layoutViewItem();
  ASSERT(!rootForPaintInvalidation.needsLayout());

  TRACE_EVENT1("blink", "FrameView::invalidateTree", "root",
               rootForPaintInvalidation.debugName().ascii());

  invalidatePaintIfNeeded(paintInvalidationState);
  rootForPaintInvalidation.invalidateTreeIfNeeded(paintInvalidationState);

#if DCHECK_IS_ON()
  layoutView()->assertSubtreeClearedPaintInvalidationFlags();
#endif

  lifecycle().advanceTo(DocumentLifecycle::PaintInvalidationClean);
}

void FrameView::invalidatePaintIfNeeded(
    const PaintInvalidationState& paintInvalidationState) {
  RELEASE_ASSERT(!layoutViewItem().isNull());
  if (!RuntimeEnabledFeatures::rootLayerScrollingEnabled())
    invalidatePaintOfScrollControlsIfNeeded(paintInvalidationState);
}

void FrameView::setNeedsPaintPropertyUpdate() {
  m_needsPaintPropertyUpdate = true;
  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
    if (auto* layoutView = this->layoutView()) {
      layoutView->setNeedsPaintPropertyUpdate();
      return;
    }
  }
  if (LayoutObject* owner = frame().ownerLayoutObject())
    owner->setNeedsPaintPropertyUpdate();
}

void FrameView::setSubtreeNeedsPaintPropertyUpdate() {
  setNeedsPaintPropertyUpdate();
  layoutView()->setSubtreeNeedsPaintPropertyUpdate();
}

IntRect FrameView::computeVisibleArea() {
  // Return our clipping bounds in the root frame.
  IntRect us(frameRect());
  if (FrameView* parent = parentFrameView()) {
    us = parent->contentsToRootFrame(us);
    IntRect parentRect = parent->computeVisibleArea();
    if (parentRect.isEmpty())
      return IntRect();

    us.intersect(parentRect);
  }

  return us;
}

FloatSize FrameView::viewportSizeForViewportUnits() const {
  float zoom = 1;
  if (!m_frame->document() || !m_frame->document()->printing())
    zoom = frame().pageZoomFactor();

  LayoutViewItem layoutViewItem = this->layoutViewItem();
  if (layoutViewItem.isNull())
    return FloatSize();

  FloatSize layoutSize;
  layoutSize.setWidth(layoutViewItem.viewWidth(IncludeScrollbars) / zoom);
  layoutSize.setHeight(layoutViewItem.viewHeight(IncludeScrollbars) / zoom);

  BrowserControls& browserControls = m_frame->host()->browserControls();
  if (RuntimeEnabledFeatures::inertTopControlsEnabled() &&
      browserControls.permittedState() != WebBrowserControlsHidden) {
    // We use the layoutSize rather than frameRect to calculate viewport units
    // so that we get correct results on mobile where the page is laid out into
    // a rect that may be larger than the viewport (e.g. the 980px fallback
    // width for desktop pages). Since the layout height is statically set to
    // be the viewport with browser controls showing, we add the browser
    // controls height, compensating for page scale as well, since we want to
    // use the viewport with browser controls hidden for vh (to match Safari).
    int viewportWidth = m_frame->host()->visualViewport().size().width();
    if (m_frame->isMainFrame() && layoutSize.width() && viewportWidth) {
      float pageScaleAtLayoutWidth = viewportWidth / layoutSize.width();
      layoutSize.expand(0, browserControls.height() / pageScaleAtLayoutWidth);
    }
  }

  return layoutSize;
}

FloatSize FrameView::viewportSizeForMediaQueries() const {
  FloatSize viewportSize(layoutSize(IncludeScrollbars));
  if (!m_frame->document() || !m_frame->document()->printing())
    viewportSize.scale(1 / frame().pageZoomFactor());
  return viewportSize;
}

DocumentLifecycle& FrameView::lifecycle() const {
  DCHECK(m_frame);
  DCHECK(m_frame->document());
  return m_frame->document()->lifecycle();
}

LayoutReplaced* FrameView::embeddedReplacedContent() const {
  LayoutViewItem layoutViewItem = this->layoutViewItem();
  if (layoutViewItem.isNull())
    return nullptr;

  LayoutObject* firstChild = layoutView()->firstChild();
  if (!firstChild || !firstChild->isBox())
    return nullptr;

  // Currently only embedded SVG documents participate in the size-negotiation
  // logic.
  if (firstChild->isSVGRoot())
    return toLayoutSVGRoot(firstChild);

  return nullptr;
}

void FrameView::addPart(LayoutPart* object) {
  m_parts.insert(object);
}

void FrameView::removePart(LayoutPart* object) {
  m_parts.erase(object);
}

void FrameView::updateWidgetGeometries() {
  Vector<RefPtr<LayoutPart>> parts;
  copyToVector(m_parts, parts);

  for (auto part : parts) {
    // Script or plugins could detach the frame so abort processing if that
    // happens.
    if (layoutViewItem().isNull())
      break;

    if (FrameViewBase* frameViewBase = part->widget()) {
      if (frameViewBase->isFrameView()) {
        FrameView* frameView = toFrameView(frameViewBase);
        bool didNeedLayout = frameView->needsLayout();
        part->updateWidgetGeometry();
        if (!didNeedLayout && !frameView->shouldThrottleRendering())
          frameView->checkDoesNotNeedLayout();
      } else {
        part->updateWidgetGeometry();
      }
    }
  }
}

void FrameView::addPartToUpdate(LayoutEmbeddedObject& object) {
  ASSERT(isInPerformLayout());
  // Tell the DOM element that it needs a FrameViewBase update.
  Node* node = object.node();
  ASSERT(node);
  if (isHTMLObjectElement(*node) || isHTMLEmbedElement(*node))
    toHTMLPlugInElement(node)->setNeedsWidgetUpdate(true);

  m_partUpdateSet.insert(&object);
}

void FrameView::setDisplayMode(WebDisplayMode mode) {
  if (mode == m_displayMode)
    return;

  m_displayMode = mode;

  if (m_frame->document())
    m_frame->document()->mediaQueryAffectingValueChanged();
}

void FrameView::setDisplayShape(DisplayShape displayShape) {
  if (displayShape == m_displayShape)
    return;

  m_displayShape = displayShape;

  if (m_frame->document())
    m_frame->document()->mediaQueryAffectingValueChanged();
}

void FrameView::setMediaType(const AtomicString& mediaType) {
  DCHECK(m_frame->document());
  m_mediaType = mediaType;
  m_frame->document()->mediaQueryAffectingValueChanged();
}

AtomicString FrameView::mediaType() const {
  // See if we have an override type.
  if (m_frame->settings() &&
      !m_frame->settings()->getMediaTypeOverride().isEmpty())
    return AtomicString(m_frame->settings()->getMediaTypeOverride());
  return m_mediaType;
}

void FrameView::adjustMediaTypeForPrinting(bool printing) {
  if (printing) {
    if (m_mediaTypeWhenNotPrinting.isNull())
      m_mediaTypeWhenNotPrinting = mediaType();
    setMediaType(MediaTypeNames::print);
  } else {
    if (!m_mediaTypeWhenNotPrinting.isNull())
      setMediaType(m_mediaTypeWhenNotPrinting);
    m_mediaTypeWhenNotPrinting = nullAtom;
  }

  m_frame->document()->setNeedsStyleRecalc(
      SubtreeStyleChange,
      StyleChangeReasonForTracing::create(StyleChangeReason::StyleSheetChange));
}

bool FrameView::contentsInCompositedLayer() const {
  LayoutViewItem layoutViewItem = this->layoutViewItem();
  return !layoutViewItem.isNull() &&
         layoutViewItem.compositingState() == PaintsIntoOwnBacking;
}

void FrameView::addBackgroundAttachmentFixedObject(LayoutObject* object) {
  ASSERT(!m_backgroundAttachmentFixedObjects.contains(object));

  m_backgroundAttachmentFixedObjects.insert(object);
  if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
    scrollingCoordinator->frameViewHasBackgroundAttachmentFixedObjectsDidChange(
        this);

  // Ensure main thread scrolling reasons are recomputed.
  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled()) {
    setNeedsPaintPropertyUpdate();
    // The object's scroll properties are not affected by its own background.
    object->setAncestorsNeedPaintPropertyUpdateForMainThreadScrolling();
  }
}

void FrameView::removeBackgroundAttachmentFixedObject(LayoutObject* object) {
  ASSERT(m_backgroundAttachmentFixedObjects.contains(object));

  m_backgroundAttachmentFixedObjects.erase(object);
  if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
    scrollingCoordinator->frameViewHasBackgroundAttachmentFixedObjectsDidChange(
        this);

  // Ensure main thread scrolling reasons are recomputed.
  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled()) {
    setNeedsPaintPropertyUpdate();
    // The object's scroll properties are not affected by its own background.
    object->setAncestorsNeedPaintPropertyUpdateForMainThreadScrolling();
  }
}

void FrameView::addViewportConstrainedObject(LayoutObject& object) {
  if (!m_viewportConstrainedObjects) {
    m_viewportConstrainedObjects =
        WTF::wrapUnique(new ViewportConstrainedObjectSet);
  }

  if (!m_viewportConstrainedObjects->contains(&object)) {
    m_viewportConstrainedObjects->insert(&object);

    if (ScrollingCoordinator* scrollingCoordinator =
            this->scrollingCoordinator())
      scrollingCoordinator->frameViewFixedObjectsDidChange(this);
  }
}

void FrameView::removeViewportConstrainedObject(LayoutObject& object) {
  if (m_viewportConstrainedObjects &&
      m_viewportConstrainedObjects->contains(&object)) {
    m_viewportConstrainedObjects->erase(&object);

    if (ScrollingCoordinator* scrollingCoordinator =
            this->scrollingCoordinator())
      scrollingCoordinator->frameViewFixedObjectsDidChange(this);
  }
}

void FrameView::viewportSizeChanged(bool widthChanged, bool heightChanged) {
  DCHECK(widthChanged || heightChanged);
  DCHECK(m_frame->host());

  if (LayoutViewItem layoutView = this->layoutViewItem()) {
    if (layoutView.usesCompositing())
      layoutView.compositor()->frameViewDidChangeSize();
  }

  // Ensure the root scroller compositing layers update geometry in response to
  // the URL bar resizing.
  if (m_frame->isMainFrame())
    m_frame->host()->globalRootScrollerController().mainFrameViewResized();

  showOverlayScrollbars();

  bool rootLayerScrollingEnabled =
      RuntimeEnabledFeatures::rootLayerScrollingEnabled();
  if (rootLayerScrollingEnabled) {
    // The background must be repainted when the FrameView is resized, even if
    // the initial containing block does not change (so we can't rely on layout
    // to issue the invalidation).  This is because the background fills the
    // main GraphicsLayer, which takes the size of the layout viewport.
    // TODO(skobes): Paint non-fixed backgrounds into the scrolling contents
    // layer and avoid this invalidation (http://crbug.com/568847).
    LayoutViewItem lvi = layoutViewItem();
    if (!lvi.isNull())
      lvi.setShouldDoFullPaintInvalidation();
  }

  if (RuntimeEnabledFeatures::inertTopControlsEnabled() && layoutView() &&
      m_frame->isMainFrame() && m_frame->host()->browserControls().height()) {
    if (layoutView()->style()->hasFixedBackgroundImage()) {
      // In the case where we don't change layout size from top control resizes,
      // we wont perform a layout. If we have a fixed background image however,
      // the background layer needs to get resized so we should request a layout
      // explicitly.
      PaintLayer* layer = layoutView()->layer();
      if (layoutView()->compositor()->needsFixedRootBackgroundLayer(layer)) {
        setNeedsLayout();
      } else if (!rootLayerScrollingEnabled) {
        // If root layer scrolls is on, we've already issued a full invalidation
        // above.
        layoutView()->setShouldDoFullPaintInvalidationOnResizeIfNeeded(
            widthChanged, heightChanged);
      }
    } else if (heightChanged && !rootLayerScrollingEnabled) {
      // If the document rect doesn't fill the full view height, hiding the
      // URL bar will expose area outside the current LayoutView so we need to
      // paint additional background. If RLS is on, we've already invalidated
      // above.
      LayoutViewItem lvi = layoutViewItem();
      DCHECK(!lvi.isNull());
      if (lvi.documentRect().height() < lvi.viewRect().height())
        lvi.setShouldDoFullPaintInvalidation();
    }
  }

  if (!hasViewportConstrainedObjects())
    return;

  for (const auto& viewportConstrainedObject : *m_viewportConstrainedObjects) {
    LayoutObject* layoutObject = viewportConstrainedObject;
    const ComputedStyle& style = layoutObject->styleRef();
    if (widthChanged) {
      if (style.width().isFixed() &&
          (style.left().isAuto() || style.right().isAuto()))
        layoutObject->setNeedsPositionedMovementLayout();
      else
        layoutObject->setNeedsLayoutAndFullPaintInvalidation(
            LayoutInvalidationReason::SizeChanged);
    }
    if (heightChanged) {
      if (style.height().isFixed() &&
          (style.top().isAuto() || style.bottom().isAuto()))
        layoutObject->setNeedsPositionedMovementLayout();
      else
        layoutObject->setNeedsLayoutAndFullPaintInvalidation(
            LayoutInvalidationReason::SizeChanged);
    }
  }
}

IntPoint FrameView::lastKnownMousePosition() const {
  return m_frame->eventHandler().lastKnownMousePosition();
}

bool FrameView::shouldSetCursor() const {
  Page* page = frame().page();
  return page && page->visibilityState() != PageVisibilityStateHidden &&
         !m_frame->eventHandler().isMousePositionUnknown() &&
         page->focusController().isActive();
}

void FrameView::scrollContentsIfNeededRecursive() {
  forAllNonThrottledFrameViews(
      [](FrameView& frameView) { frameView.scrollContentsIfNeeded(); });
}

void FrameView::invalidateBackgroundAttachmentFixedObjects() {
  for (const auto& layoutObject : m_backgroundAttachmentFixedObjects)
    layoutObject->setShouldDoFullPaintInvalidation();
}

bool FrameView::hasBackgroundAttachmentFixedDescendants(
    const LayoutObject& object) const {
  for (const auto* potentialDescendant : m_backgroundAttachmentFixedObjects) {
    if (potentialDescendant == &object)
      continue;
    if (potentialDescendant->isDescendantOf(&object))
      return true;
  }
  return false;
}

bool FrameView::invalidateViewportConstrainedObjects() {
  bool fastPathAllowed = true;
  for (const auto& viewportConstrainedObject : *m_viewportConstrainedObjects) {
    LayoutObject* layoutObject = viewportConstrainedObject;
    LayoutItem layoutItem = LayoutItem(layoutObject);
    ASSERT(layoutItem.style()->hasViewportConstrainedPosition());
    ASSERT(layoutItem.hasLayer());
    PaintLayer* layer = LayoutBoxModel(layoutItem).layer();

    if (layer->isPaintInvalidationContainer())
      continue;

    if (layer->subtreeIsInvisible())
      continue;

    // invalidate even if there is an ancestor with a filter that moves pixels.
    layoutItem
        .setShouldDoFullPaintInvalidationIncludingNonCompositingDescendants();

    TRACE_EVENT_INSTANT1(
        TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"),
        "ScrollInvalidationTracking", TRACE_EVENT_SCOPE_THREAD, "data",
        InspectorScrollInvalidationTrackingEvent::data(*layoutObject));

    // If the fixed layer has a blur/drop-shadow filter applied on at least one
    // of its parents, we cannot scroll using the fast path, otherwise the
    // outsets of the filter will be moved around the page.
    if (layer->hasAncestorWithFilterThatMovesPixels())
      fastPathAllowed = false;
  }
  return fastPathAllowed;
}

bool FrameView::scrollContentsFastPath(const IntSize& scrollDelta) {
  if (!contentsInCompositedLayer())
    return false;

  invalidateBackgroundAttachmentFixedObjects();

  if (!m_viewportConstrainedObjects ||
      m_viewportConstrainedObjects->isEmpty()) {
    probe::didUpdateLayout(m_frame.get());
    return true;
  }

  if (!invalidateViewportConstrainedObjects())
    return false;

  probe::didUpdateLayout(m_frame.get());
  return true;
}

void FrameView::scrollContentsSlowPath() {
  TRACE_EVENT0("blink", "FrameView::scrollContentsSlowPath");
  // We need full invalidation during slow scrolling. For slimming paint, full
  // invalidation of the LayoutView is not enough. We also need to invalidate
  // all of the objects.
  // FIXME: Find out what are enough to invalidate in slow path scrolling.
  // crbug.com/451090#9.
  ASSERT(!layoutViewItem().isNull());
  if (contentsInCompositedLayer())
    layoutViewItem()
        .layer()
        ->compositedLayerMapping()
        ->setContentsNeedDisplay();
  else
    layoutViewItem()
        .setShouldDoFullPaintInvalidationIncludingNonCompositingDescendants();

  if (contentsInCompositedLayer()) {
    IntRect updateRect = visibleContentRect();
    ASSERT(!layoutViewItem().isNull());
    // FIXME: We should not allow paint invalidation out of paint invalidation
    // state. crbug.com/457415
    DisablePaintInvalidationStateAsserts disabler;
    layoutViewItem().invalidatePaintRectangle(LayoutRect(updateRect));
  }
  LayoutPartItem frameLayoutItem = m_frame->ownerLayoutItem();
  if (!frameLayoutItem.isNull()) {
    if (isEnclosedInCompositingLayer()) {
      LayoutRect rect(
          frameLayoutItem.borderLeft() + frameLayoutItem.paddingLeft(),
          frameLayoutItem.borderTop() + frameLayoutItem.paddingTop(),
          LayoutUnit(visibleWidth()), LayoutUnit(visibleHeight()));
      // FIXME: We should not allow paint invalidation out of paint invalidation
      // state. crbug.com/457415
      DisablePaintInvalidationStateAsserts disabler;
      frameLayoutItem.invalidatePaintRectangle(rect);
      return;
    }
  }
}

void FrameView::restoreScrollbar() {
  setScrollbarsSuppressed(false);
}

void FrameView::processUrlFragment(const KURL& url,
                                   UrlFragmentBehavior behavior) {
  // If our URL has no ref, then we have no place we need to jump to.
  // OTOH If CSS target was set previously, we want to set it to 0, recalc
  // and possibly paint invalidation because :target pseudo class may have been
  // set (see bug 11321).
  // Similarly for svg, if we had a previous svgView() then we need to reset
  // the initial view if we don't have a fragment.
  if (!url.hasFragmentIdentifier() && !m_frame->document()->cssTarget() &&
      !m_frame->document()->isSVGDocument())
    return;

  String fragmentIdentifier = url.fragmentIdentifier();
  if (processUrlFragmentHelper(fragmentIdentifier, behavior))
    return;

  // Try again after decoding the ref, based on the document's encoding.
  if (m_frame->document()->encoding().isValid())
    processUrlFragmentHelper(
        decodeURLEscapeSequences(fragmentIdentifier,
                                 m_frame->document()->encoding()),
        behavior);
}

bool FrameView::processUrlFragmentHelper(const String& name,
                                         UrlFragmentBehavior behavior) {
  ASSERT(m_frame->document());

  if (behavior == UrlFragmentScroll &&
      !m_frame->document()->isRenderingReady()) {
    m_frame->document()->setGotoAnchorNeededAfterStylesheetsLoad(true);
    return false;
  }

  m_frame->document()->setGotoAnchorNeededAfterStylesheetsLoad(false);

  Element* anchorNode = m_frame->document()->findAnchor(name);

  // Setting to null will clear the current target.
  m_frame->document()->setCSSTarget(anchorNode);

  if (m_frame->document()->isSVGDocument()) {
    if (SVGSVGElement* svg =
            SVGDocumentExtensions::rootElement(*m_frame->document())) {
      svg->setupInitialView(name, anchorNode);
      if (!anchorNode)
        return true;
    }
  }

  // Implement the rule that "" and "top" both mean top of page as in other
  // browsers.
  if (!anchorNode && !(name.isEmpty() || equalIgnoringCase(name, "top")))
    return false;

  if (behavior == UrlFragmentScroll)
    setFragmentAnchor(anchorNode ? static_cast<Node*>(anchorNode)
                                 : m_frame->document());

  // If the anchor accepts keyboard focus and fragment scrolling is allowed,
  // move focus there to aid users relying on keyboard navigation.
  // If anchorNode is not focusable or fragment scrolling is not allowed,
  // clear focus, which matches the behavior of other browsers.
  if (anchorNode) {
    m_frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();
    if (behavior == UrlFragmentScroll && anchorNode->isFocusable()) {
      anchorNode->focus();
    } else {
      if (behavior == UrlFragmentScroll)
        m_frame->document()->setSequentialFocusNavigationStartingPoint(
            anchorNode);
      m_frame->document()->clearFocusedElement();
    }
  }
  return true;
}

void FrameView::setFragmentAnchor(Node* anchorNode) {
  ASSERT(anchorNode);
  m_fragmentAnchor = anchorNode;

  // We need to update the layout tree before scrolling.
  m_frame->document()->updateStyleAndLayoutTree();

  // If layout is needed, we will scroll in performPostLayoutTasks. Otherwise,
  // scroll immediately.
  LayoutViewItem layoutViewItem = this->layoutViewItem();
  if (!layoutViewItem.isNull() && layoutViewItem.needsLayout())
    layout();
  else
    scrollToFragmentAnchor();
}

void FrameView::clearFragmentAnchor() {
  m_fragmentAnchor = nullptr;
}

void FrameView::didUpdateElasticOverscroll() {
  Page* page = frame().page();
  if (!page)
    return;
  FloatSize elasticOverscroll = page->chromeClient().elasticOverscroll();
  if (horizontalScrollbar()) {
    float delta =
        elasticOverscroll.width() - horizontalScrollbar()->elasticOverscroll();
    if (delta != 0) {
      horizontalScrollbar()->setElasticOverscroll(elasticOverscroll.width());
      scrollAnimator().notifyContentAreaScrolled(FloatSize(delta, 0));
      setScrollbarNeedsPaintInvalidation(HorizontalScrollbar);
    }
  }
  if (verticalScrollbar()) {
    float delta =
        elasticOverscroll.height() - verticalScrollbar()->elasticOverscroll();
    if (delta != 0) {
      verticalScrollbar()->setElasticOverscroll(elasticOverscroll.height());
      scrollAnimator().notifyContentAreaScrolled(FloatSize(0, delta));
      setScrollbarNeedsPaintInvalidation(VerticalScrollbar);
    }
  }
}

IntSize FrameView::layoutSize(
    IncludeScrollbarsInRect scrollbarInclusion) const {
  return scrollbarInclusion == ExcludeScrollbars
             ? excludeScrollbars(m_layoutSize)
             : m_layoutSize;
}

void FrameView::setLayoutSize(const IntSize& size) {
  ASSERT(!layoutSizeFixedToFrameSize());

  setLayoutSizeInternal(size);
}

void FrameView::didScrollTimerFired(TimerBase*) {
  if (m_frame->document() && !m_frame->document()->layoutViewItem().isNull())
    m_frame->document()->fetcher()->updateAllImageResourcePriorities();
}

void FrameView::updateLayersAndCompositingAfterScrollIfNeeded() {
  // Nothing to do after scrolling if there are no fixed position elements.
  if (!hasViewportConstrainedObjects())
    return;

  // Update sticky position objects which are stuck to the viewport. In order to
  // correctly compute the sticky position offsets the layers must be visited
  // top-down, so start at the 'root' sticky elements and recurse downwards.
  for (const auto& viewportConstrainedObject : *m_viewportConstrainedObjects) {
    LayoutObject* layoutObject = viewportConstrainedObject;
    if (layoutObject->style()->position() != EPosition::kSticky)
      continue;

    PaintLayer* layer = toLayoutBoxModelObject(layoutObject)->layer();

    // This method can be called during layout at which point the ancestor
    // overflow layer may not be set yet. We can safely skip such cases as we
    // will revisit this method during compositing inputs update.
    if (!layer->ancestorOverflowLayer())
      continue;

    StickyConstraintsMap constraintsMap = layer->ancestorOverflowLayer()
                                              ->getScrollableArea()
                                              ->stickyConstraintsMap();
    if (constraintsMap.contains(layer) &&
        !constraintsMap.at(layer).hasAncestorStickyElement()) {
      // TODO(skobes): Resolve circular dependency between scroll offset and
      // compositing state, and remove this disabler. https://crbug.com/420741
      DisableCompositingQueryAsserts disabler;
      layer->updateLayerPositionsAfterOverflowScroll();
      layoutObject->setMayNeedPaintInvalidationSubtree();
    }
  }

  // If there fixed position elements, scrolling may cause compositing layers to
  // change.  Update FrameViewBase and layer positions after scrolling, but only
  // if we're not inside of layout.
  if (!m_nestedLayoutCount) {
    updateWidgetGeometries();
    LayoutViewItem layoutViewItem = this->layoutViewItem();
    if (!layoutViewItem.isNull())
      layoutViewItem.layer()->setNeedsCompositingInputsUpdate();
  }
}

bool FrameView::computeCompositedSelection(LocalFrame& frame,
                                           CompositedSelection& selection) {
  if (!frame.view() || frame.view()->shouldThrottleRendering())
    return false;

  const VisibleSelection& visibleSelection =
      frame.selection().computeVisibleSelectionInDOMTree();
  if (visibleSelection.isNone() || !frame.selection().isHandleVisible())
    return false;

  // Non-editable caret selections lack any kind of UI affordance, and
  // needn't be tracked by the client.
  if (visibleSelection.isCaret() && !visibleSelection.isContentEditable())
    return false;

  VisiblePosition visibleStart(visibleSelection.visibleStart());
  RenderedPosition renderedStart(visibleStart);
  renderedStart.positionInGraphicsLayerBacking(selection.start, true);
  if (!selection.start.layer)
    return false;

  VisiblePosition visibleEnd(visibleSelection.visibleEnd());
  RenderedPosition renderedEnd(visibleEnd);
  renderedEnd.positionInGraphicsLayerBacking(selection.end, false);
  if (!selection.end.layer)
    return false;

  selection.type = visibleSelection.getSelectionType();
  selection.start.isTextDirectionRTL |=
      primaryDirectionOf(*visibleSelection.start().anchorNode()) ==
      TextDirection::kRtl;
  selection.end.isTextDirectionRTL |=
      primaryDirectionOf(*visibleSelection.end().anchorNode()) ==
      TextDirection::kRtl;

  return true;
}

void FrameView::updateCompositedSelectionIfNeeded() {
  if (!RuntimeEnabledFeatures::compositedSelectionUpdateEnabled())
    return;

  TRACE_EVENT0("blink", "FrameView::updateCompositedSelectionIfNeeded");

  Page* page = frame().page();
  ASSERT(page);

  CompositedSelection selection;
  LocalFrame* focusedFrame = page->focusController().focusedFrame();
  LocalFrame* localFrame = (focusedFrame && (focusedFrame->localFrameRoot() ==
                                             m_frame->localFrameRoot()))
                               ? focusedFrame
                               : nullptr;

  if (localFrame && computeCompositedSelection(*localFrame, selection)) {
    page->chromeClient().updateCompositedSelection(localFrame, selection);
  } else {
    if (!localFrame) {
      // Clearing the mainframe when there is no focused frame (and hence
      // no localFrame) is legacy behaviour, and implemented here to
      // satisfy ParameterizedWebFrameTest.CompositedSelectionBoundsCleared's
      // first check that the composited selection has been cleared even
      // though no frame has focus yet. If this is not desired, then the
      // expectation needs to be removed from the test.
      localFrame = m_frame->localFrameRoot();
    }

    if (localFrame)
      page->chromeClient().clearCompositedSelection(localFrame);
  }
}

HostWindow* FrameView::getHostWindow() const {
  Page* page = frame().page();
  if (!page)
    return nullptr;
  return &page->chromeClient();
}

void FrameView::contentsResized() {
  if (m_frame->isMainFrame() && m_frame->document()) {
    if (TextAutosizer* textAutosizer = m_frame->document()->textAutosizer())
      textAutosizer->updatePageInfoInAllFrames();
  }

  ScrollableArea::contentsResized();
  setNeedsLayout();
}

void FrameView::scrollbarExistenceDidChange() {
  // We check to make sure the view is attached to a frame() as this method can
  // be triggered before the view is attached by LocalFrame::createView(...)
  // setting various values such as setScrollBarModes(...) for example.  An
  // ASSERT is triggered when a view is layout before being attached to a
  // frame().
  if (!frame().view())
    return;

  bool usesOverlayScrollbars = ScrollbarTheme::theme().usesOverlayScrollbars();

  // FIXME: this call to layout() could be called within FrameView::layout(),
  // but before performLayout(), causing double-layout. See also
  // crbug.com/429242.
  if (!usesOverlayScrollbars && needsLayout())
    layout();

  if (!layoutViewItem().isNull() && layoutViewItem().usesCompositing()) {
    layoutViewItem().compositor()->frameViewScrollbarsExistenceDidChange();

    if (!usesOverlayScrollbars)
      layoutViewItem().compositor()->frameViewDidChangeSize();
  }
}

void FrameView::handleLoadCompleted() {
  // Once loading has completed, allow autoSize one last opportunity to
  // reduce the size of the frame.
  if (m_autoSizeInfo)
    m_autoSizeInfo->autoSizeIfNeeded();

  // If there is a pending layout, the fragment anchor will be cleared when it
  // finishes.
  if (!needsLayout())
    clearFragmentAnchor();
}

void FrameView::clearLayoutSubtreeRoot(const LayoutObject& root) {
  m_layoutSubtreeRootList.remove(const_cast<LayoutObject&>(root));
}

void FrameView::clearLayoutSubtreeRootsAndMarkContainingBlocks() {
  m_layoutSubtreeRootList.clearAndMarkContainingBlocksForLayout();
}

void FrameView::addOrthogonalWritingModeRoot(LayoutBox& root) {
  DCHECK(!root.isLayoutScrollbarPart());
  m_orthogonalWritingModeRootList.add(root);
}

void FrameView::removeOrthogonalWritingModeRoot(LayoutBox& root) {
  m_orthogonalWritingModeRootList.remove(root);
}

bool FrameView::hasOrthogonalWritingModeRoots() const {
  return !m_orthogonalWritingModeRootList.isEmpty();
}

static inline void removeFloatingObjectsForSubtreeRoot(LayoutObject& root) {
  // TODO(kojii): Under certain conditions, moveChildTo() defers
  // removeFloatingObjects() until the containing block layouts. For
  // instance, when descendants of the moving child is floating,
  // removeChildNode() does not clear them. In such cases, at this
  // point, FloatingObjects may contain old or even deleted objects.
  // Dealing this in markAllDescendantsWithFloatsForLayout() could
  // solve, but since that is likely to suffer the performance and
  // since the containing block of orthogonal writing mode roots
  // having floats is very rare, prefer to re-create
  // FloatingObjects.
  if (LayoutBlock* cb = root.containingBlock()) {
    if ((cb->normalChildNeedsLayout() || cb->selfNeedsLayout()) &&
        cb->isLayoutBlockFlow()) {
      toLayoutBlockFlow(cb)->removeFloatingObjectsFromDescendants();
    }
  }
}

static bool prepareOrthogonalWritingModeRootForLayout(LayoutObject& root) {
  DCHECK(root.isBox() && toLayoutBox(root).isOrthogonalWritingModeRoot());
  if (!root.needsLayout() || root.isOutOfFlowPositioned() ||
      root.isColumnSpanAll() ||
      !root.styleRef().logicalHeight().isIntrinsicOrAuto())
    return false;

  removeFloatingObjectsForSubtreeRoot(root);
  return true;
}

void FrameView::layoutOrthogonalWritingModeRoots() {
  for (auto& root : m_orthogonalWritingModeRootList.ordered()) {
    if (prepareOrthogonalWritingModeRootForLayout(*root))
      layoutFromRootObject(*root);
  }
}

void FrameView::scheduleOrthogonalWritingModeRootsForLayout() {
  for (auto& root : m_orthogonalWritingModeRootList.ordered()) {
    if (prepareOrthogonalWritingModeRootForLayout(*root))
      m_layoutSubtreeRootList.add(*root);
  }
}

bool FrameView::checkLayoutInvalidationIsAllowed() const {
  if (m_allowsLayoutInvalidationAfterLayoutClean)
    return true;

  // If we are updating all lifecycle phases beyond LayoutClean, we don't expect
  // dirty layout after LayoutClean.
  CHECK_FOR_DIRTY_LAYOUT(lifecycle().state() < DocumentLifecycle::LayoutClean);

  return true;
}

void FrameView::scheduleRelayout() {
  DCHECK(m_frame->view() == this);

  if (!m_layoutSchedulingEnabled)
    return;
  // TODO(crbug.com/590856): It's still broken when we choose not to crash when
  // the check fails.
  if (!checkLayoutInvalidationIsAllowed())
    return;
  if (!needsLayout())
    return;
  if (!m_frame->document()->shouldScheduleLayout())
    return;
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "InvalidateLayout", TRACE_EVENT_SCOPE_THREAD, "data",
                       InspectorInvalidateLayoutEvent::data(m_frame.get()));

  clearLayoutSubtreeRootsAndMarkContainingBlocks();

  if (m_hasPendingLayout)
    return;
  m_hasPendingLayout = true;

  if (!shouldThrottleRendering())
    page()->animator().scheduleVisualUpdate(m_frame.get());
}

void FrameView::scheduleRelayoutOfSubtree(LayoutObject* relayoutRoot) {
  DCHECK(m_frame->view() == this);

  // TODO(crbug.com/590856): It's still broken when we choose not to crash when
  // the check fails.
  if (!checkLayoutInvalidationIsAllowed())
    return;

  // FIXME: Should this call shouldScheduleLayout instead?
  if (!m_frame->document()->isActive())
    return;

  LayoutView* layoutView = this->layoutView();
  if (layoutView && layoutView->needsLayout()) {
    if (relayoutRoot)
      relayoutRoot->markContainerChainForLayout(false);
    return;
  }

  if (relayoutRoot == layoutView)
    m_layoutSubtreeRootList.clearAndMarkContainingBlocksForLayout();
  else
    m_layoutSubtreeRootList.add(*relayoutRoot);

  if (m_layoutSchedulingEnabled) {
    m_hasPendingLayout = true;

    if (!shouldThrottleRendering())
      page()->animator().scheduleVisualUpdate(m_frame.get());

    lifecycle().ensureStateAtMost(DocumentLifecycle::StyleClean);
  }
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "InvalidateLayout", TRACE_EVENT_SCOPE_THREAD, "data",
                       InspectorInvalidateLayoutEvent::data(m_frame.get()));
}

bool FrameView::layoutPending() const {
  // FIXME: This should check Document::lifecycle instead.
  return m_hasPendingLayout;
}

bool FrameView::isInPerformLayout() const {
  return lifecycle().state() == DocumentLifecycle::InPerformLayout;
}

bool FrameView::needsLayout() const {
  // This can return true in cases where the document does not have a body yet.
  // Document::shouldScheduleLayout takes care of preventing us from scheduling
  // layout in that case.

  LayoutViewItem layoutViewItem = this->layoutViewItem();
  return layoutPending() ||
         (!layoutViewItem.isNull() && layoutViewItem.needsLayout()) ||
         isSubtreeLayout();
}

NOINLINE bool FrameView::checkDoesNotNeedLayout() const {
  CHECK_FOR_DIRTY_LAYOUT(!layoutPending());
  CHECK_FOR_DIRTY_LAYOUT(layoutViewItem().isNull() ||
                         !layoutViewItem().needsLayout());
  CHECK_FOR_DIRTY_LAYOUT(!isSubtreeLayout());
  return true;
}

void FrameView::setNeedsLayout() {
  LayoutViewItem layoutViewItem = this->layoutViewItem();
  if (layoutViewItem.isNull())
    return;
  // TODO(crbug.com/590856): It's still broken if we choose not to crash when
  // the check fails.
  if (!checkLayoutInvalidationIsAllowed())
    return;
  layoutViewItem.setNeedsLayout(LayoutInvalidationReason::Unknown);
}

bool FrameView::isTransparent() const {
  return m_isTransparent;
}

void FrameView::setTransparent(bool isTransparent) {
  m_isTransparent = isTransparent;
  DisableCompositingQueryAsserts disabler;
  if (!layoutViewItem().isNull() &&
      layoutViewItem().layer()->hasCompositedLayerMapping())
    layoutViewItem().layer()->compositedLayerMapping()->updateContentsOpaque();
}

bool FrameView::hasOpaqueBackground() const {
  return !m_isTransparent && !m_baseBackgroundColor.hasAlpha();
}

Color FrameView::baseBackgroundColor() const {
  return m_baseBackgroundColor;
}

void FrameView::setBaseBackgroundColor(const Color& backgroundColor) {
  m_baseBackgroundColor = backgroundColor;

  if (!layoutViewItem().isNull() &&
      layoutViewItem().layer()->hasCompositedLayerMapping()) {
    CompositedLayerMapping* compositedLayerMapping =
        layoutViewItem().layer()->compositedLayerMapping();
    compositedLayerMapping->updateContentsOpaque();
    if (compositedLayerMapping->mainGraphicsLayer())
      compositedLayerMapping->mainGraphicsLayer()->setNeedsDisplay();
  }
  recalculateScrollbarOverlayColorTheme(documentBackgroundColor());

  if (!shouldThrottleRendering())
    page()->animator().scheduleVisualUpdate(m_frame.get());
}

void FrameView::updateBackgroundRecursively(const Color& backgroundColor,
                                            bool transparent) {
  forAllNonThrottledFrameViews(
      [backgroundColor, transparent](FrameView& frameView) {
        frameView.setTransparent(transparent);
        frameView.setBaseBackgroundColor(backgroundColor);
      });
}

void FrameView::scrollToFragmentAnchor() {
  Node* anchorNode = m_fragmentAnchor;
  if (!anchorNode)
    return;

  // Scrolling is disabled during updateScrollbars (see
  // isProgrammaticallyScrollable).  Bail now to avoid clearing m_fragmentAnchor
  // before we actually have a chance to scroll.
  if (m_inUpdateScrollbars)
    return;

  if (anchorNode->layoutObject()) {
    LayoutRect rect;
    if (anchorNode != m_frame->document()) {
      rect = anchorNode->boundingBox();
    } else if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
      if (Element* documentElement = m_frame->document()->documentElement())
        rect = documentElement->boundingBox();
    }

    Frame* boundaryFrame = m_frame->findUnsafeParentScrollPropagationBoundary();

    // FIXME: Handle RemoteFrames
    if (boundaryFrame && boundaryFrame->isLocalFrame())
      toLocalFrame(boundaryFrame)
          ->view()
          ->setSafeToPropagateScrollToParent(false);

    // Scroll nested layers and frames to reveal the anchor.
    // Align to the top and to the closest side (this matches other browsers).
    anchorNode->layoutObject()->scrollRectToVisible(
        rect, ScrollAlignment::alignToEdgeIfNeeded,
        ScrollAlignment::alignTopAlways);

    if (boundaryFrame && boundaryFrame->isLocalFrame())
      toLocalFrame(boundaryFrame)
          ->view()
          ->setSafeToPropagateScrollToParent(true);

    if (AXObjectCache* cache = m_frame->document()->existingAXObjectCache())
      cache->handleScrolledToAnchor(anchorNode);
  }

  // The fragment anchor should only be maintained while the frame is still
  // loading.  If the frame is done loading, clear the anchor now. Otherwise,
  // restore it since it may have been cleared during scrollRectToVisible.
  m_fragmentAnchor =
      m_frame->document()->isLoadCompleted() ? nullptr : anchorNode;
}

bool FrameView::updateWidgets() {
  // This is always called from updateWidgetsTimerFired.
  // m_updateWidgetsTimer should only be scheduled if we have FrameViewBases to
  // update. Thus I believe we can stop checking isEmpty here, and just ASSERT
  // isEmpty:
  // FIXME: This assert has been temporarily removed due to
  // https://crbug.com/430344
  if (m_nestedLayoutCount > 1 || m_partUpdateSet.isEmpty())
    return true;

  // Need to swap because script will run inside the below loop and invalidate
  // the iterator.
  EmbeddedObjectSet objects;
  objects.swap(m_partUpdateSet);

  for (const auto& embeddedObject : objects) {
    LayoutEmbeddedObject& object = *embeddedObject;
    HTMLPlugInElement* element = toHTMLPlugInElement(object.node());

    // The object may have already been destroyed (thus node cleared),
    // but FrameView holds a manual ref, so it won't have been deleted.
    if (!element)
      continue;

    // No need to update if it's already crashed or known to be missing.
    if (object.showsUnavailablePluginIndicator())
      continue;

    if (element->needsWidgetUpdate())
      element->updateWidget();
    object.updateWidgetGeometry();

    // Prevent plugins from causing infinite updates of themselves.
    // FIXME: Do we really need to prevent this?
    m_partUpdateSet.erase(&object);
  }

  return m_partUpdateSet.isEmpty();
}

void FrameView::updateWidgetsTimerFired(TimerBase*) {
  ASSERT(!isInPerformLayout());
  for (unsigned i = 0; i < maxUpdateWidgetsIterations; ++i) {
    if (updateWidgets())
      return;
  }
}

void FrameView::flushAnyPendingPostLayoutTasks() {
  ASSERT(!isInPerformLayout());
  if (m_postLayoutTasksTimer.isActive())
    performPostLayoutTasks();
  if (m_updateWidgetsTimer.isActive()) {
    m_updateWidgetsTimer.stop();
    updateWidgetsTimerFired(nullptr);
  }
}

void FrameView::scheduleUpdateWidgetsIfNecessary() {
  ASSERT(!isInPerformLayout());
  if (m_updateWidgetsTimer.isActive() || m_partUpdateSet.isEmpty())
    return;
  m_updateWidgetsTimer.startOneShot(0, BLINK_FROM_HERE);
}

void FrameView::performPostLayoutTasks() {
  // FIXME: We can reach here, even when the page is not active!
  // http/tests/inspector/elements/html-link-import.html and many other
  // tests hit that case.
  // We should ASSERT(isActive()); or at least return early if we can!

  // Always called before or after performLayout(), part of the highest-level
  // layout() call.
  ASSERT(!isInPerformLayout());
  TRACE_EVENT0("blink,benchmark", "FrameView::performPostLayoutTasks");

  m_postLayoutTasksTimer.stop();

  m_frame->selection().didLayout();

  ASSERT(m_frame->document());

  FontFaceSet::didLayout(*m_frame->document());
  // Cursor update scheduling is done by the local root, which is the main frame
  // if there are no RemoteFrame ancestors in the frame tree. Use of
  // localFrameRoot() is discouraged but will change when cursor update
  // scheduling is moved from EventHandler to PageEventHandler.
  frame().localFrameRoot()->eventHandler().scheduleCursorUpdate();

  updateWidgetGeometries();

  // Plugins could have torn down the page inside updateWidgetGeometries().
  if (layoutViewItem().isNull())
    return;

  scheduleUpdateWidgetsIfNecessary();

  if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
    scrollingCoordinator->notifyGeometryChanged();

  scrollToFragmentAnchor();
  sendResizeEventIfNeeded();
}

bool FrameView::wasViewportResized() {
  ASSERT(m_frame);
  LayoutViewItem layoutViewItem = this->layoutViewItem();
  if (layoutViewItem.isNull())
    return false;
  ASSERT(layoutViewItem.style());
  return (layoutSize(IncludeScrollbars) != m_lastViewportSize ||
          layoutViewItem.style()->zoom() != m_lastZoomFactor);
}

void FrameView::sendResizeEventIfNeeded() {
  ASSERT(m_frame);

  LayoutViewItem layoutViewItem = this->layoutViewItem();
  if (layoutViewItem.isNull() || layoutViewItem.document().printing())
    return;

  if (!wasViewportResized())
    return;

  m_lastViewportSize = layoutSize(IncludeScrollbars);
  m_lastZoomFactor = layoutViewItem.style()->zoom();

  if (RuntimeEnabledFeatures::visualViewportAPIEnabled())
    m_frame->document()->enqueueVisualViewportResizeEvent();

  m_frame->document()->enqueueResizeEvent();

  if (m_frame->isMainFrame())
    probe::didResizeMainFrame(m_frame.get());
}

void FrameView::postLayoutTimerFired(TimerBase*) {
  performPostLayoutTasks();
}

void FrameView::updateCounters() {
  LayoutView* view = layoutView();
  if (!view->hasLayoutCounters())
    return;

  for (LayoutObject* layoutObject = view; layoutObject;
       layoutObject = layoutObject->nextInPreOrder()) {
    if (!layoutObject->isCounter())
      continue;

    toLayoutCounter(layoutObject)->updateCounter();
  }
}

bool FrameView::shouldUseIntegerScrollOffset() const {
  if (m_frame->settings() &&
      !m_frame->settings()->getPreferCompositingToLCDTextEnabled())
    return true;

  return ScrollableArea::shouldUseIntegerScrollOffset();
}

bool FrameView::isActive() const {
  Page* page = frame().page();
  return page && page->focusController().isActive();
}

void FrameView::invalidatePaintForTickmarks() {
  if (Scrollbar* scrollbar = verticalScrollbar())
    scrollbar->setNeedsPaintInvalidation(
        static_cast<ScrollbarPart>(~ThumbPart));
}

void FrameView::getTickmarks(Vector<IntRect>& tickmarks) const {
  if (!m_tickmarks.isEmpty())
    tickmarks = m_tickmarks;
  else
    tickmarks = frame().document()->markers().renderedRectsForMarkers(
        DocumentMarker::TextMatch);
}

void FrameView::setInputEventsTransformForEmulation(const IntSize& offset,
                                                    float contentScaleFactor) {
  m_inputEventsOffsetForEmulation = offset;
  m_inputEventsScaleFactorForEmulation = contentScaleFactor;
}

IntSize FrameView::inputEventsOffsetForEmulation() const {
  return m_inputEventsOffsetForEmulation;
}

float FrameView::inputEventsScaleFactor() const {
  float pageScale = m_frame->host()->visualViewport().scale();
  return pageScale * m_inputEventsScaleFactorForEmulation;
}

bool FrameView::scrollbarsCanBeActive() const {
  if (m_frame->view() != this)
    return false;

  return !!m_frame->document();
}

void FrameView::scrollbarVisibilityChanged() {
  updateScrollbarEnabledState();
  LayoutViewItem viewItem = layoutViewItem();
  if (!viewItem.isNull())
    viewItem.clearHitTestCache();
}

void FrameView::scrollbarFrameRectChanged() {
  setNeedsPaintPropertyUpdate();
}

IntRect FrameView::scrollableAreaBoundingBox() const {
  LayoutPartItem ownerLayoutItem = frame().ownerLayoutItem();
  if (ownerLayoutItem.isNull())
    return frameRect();

  return ownerLayoutItem.absoluteContentQuad(TraverseDocumentBoundaries)
      .enclosingBoundingBox();
}

bool FrameView::isScrollable() const {
  return getScrollingReasons() == Scrollable;
}

bool FrameView::isProgrammaticallyScrollable() {
  return !m_inUpdateScrollbars;
}

FrameView::ScrollingReasons FrameView::getScrollingReasons() const {
  // Check for:
  // 1) If there an actual overflow.
  // 2) display:none or visibility:hidden set to self or inherited.
  // 3) overflow{-x,-y}: hidden;
  // 4) scrolling: no;

  // Covers #1
  IntSize contentsSize = this->contentsSize();
  IntSize visibleContentSize = visibleContentRect().size();
  if ((contentsSize.height() <= visibleContentSize.height() &&
       contentsSize.width() <= visibleContentSize.width()))
    return NotScrollableNoOverflow;

  // Covers #2.
  // FIXME: Do we need to fix this for OOPI?
  HTMLFrameOwnerElement* owner = m_frame->deprecatedLocalOwner();
  if (owner &&
      (!owner->layoutObject() || !owner->layoutObject()->visibleToHitTesting()))
    return NotScrollableNotVisible;

  // Cover #3 and #4.
  ScrollbarMode horizontalMode;
  ScrollbarMode verticalMode;
  calculateScrollbarModes(horizontalMode, verticalMode,
                          RulesFromWebContentOnly);
  if (horizontalMode == ScrollbarAlwaysOff &&
      verticalMode == ScrollbarAlwaysOff)
    return NotScrollableExplicitlyDisabled;

  return Scrollable;
}

void FrameView::updateParentScrollableAreaSet() {
  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled())
    return;

  // That ensures that only inner frames are cached.
  FrameView* parentFrameView = this->parentFrameView();
  if (!parentFrameView)
    return;

  if (!isScrollable()) {
    parentFrameView->removeScrollableArea(this);
    return;
  }

  parentFrameView->addScrollableArea(this);
}

bool FrameView::shouldSuspendScrollAnimations() const {
  return !m_frame->document()->loadEventFinished();
}

void FrameView::scrollbarStyleChanged() {
  // FIXME: Why does this only apply to the main frame?
  if (!m_frame->isMainFrame())
    return;
  adjustScrollbarOpacity();
  contentsResized();
  updateScrollbars();
  positionScrollbarLayers();
}

void FrameView::notifyPageThatContentAreaWillPaint() const {
  Page* page = m_frame->page();
  if (!page)
    return;

  contentAreaWillPaint();

  if (!m_scrollableAreas)
    return;

  for (const auto& scrollableArea : *m_scrollableAreas) {
    if (!scrollableArea->scrollbarsCanBeActive())
      continue;

    scrollableArea->contentAreaWillPaint();
  }
}

bool FrameView::scrollAnimatorEnabled() const {
  return m_frame->settings() && m_frame->settings()->getScrollAnimatorEnabled();
}

void FrameView::updateDocumentAnnotatedRegions() const {
  Document* document = m_frame->document();
  if (!document->hasAnnotatedRegions())
    return;
  Vector<AnnotatedRegionValue> newRegions;
  collectAnnotatedRegions(*(document->layoutBox()), newRegions);
  if (newRegions == document->annotatedRegions())
    return;
  document->setAnnotatedRegions(newRegions);
  if (Page* page = m_frame->page())
    page->chromeClient().annotatedRegionsChanged();
}

void FrameView::didAttachDocument() {
  FrameHost* frameHost = m_frame->host();
  DCHECK(frameHost);

  DCHECK(m_frame->document());

  if (m_frame->isMainFrame()) {
    ScrollableArea& visualViewport = frameHost->visualViewport();
    ScrollableArea* layoutViewport = layoutViewportScrollableArea();
    DCHECK(layoutViewport);

    RootFrameViewport* rootFrameViewport =
        RootFrameViewport::create(visualViewport, *layoutViewport);
    m_viewportScrollableArea = rootFrameViewport;

    frameHost->globalRootScrollerController().initializeViewportScrollCallback(
        *rootFrameViewport);
  }
}

void FrameView::updateScrollCorner() {
  RefPtr<ComputedStyle> cornerStyle;
  IntRect cornerRect = scrollCornerRect();
  Document* doc = m_frame->document();

  if (doc && !cornerRect.isEmpty()) {
    // Try the <body> element first as a scroll corner source.
    if (Element* body = doc->body()) {
      if (LayoutObject* layoutObject = body->layoutObject())
        cornerStyle = layoutObject->getUncachedPseudoStyle(
            PseudoStyleRequest(PseudoIdScrollbarCorner), layoutObject->style());
    }

    if (!cornerStyle) {
      // If the <body> didn't have a custom style, then the root element might.
      if (Element* docElement = doc->documentElement()) {
        if (LayoutObject* layoutObject = docElement->layoutObject())
          cornerStyle = layoutObject->getUncachedPseudoStyle(
              PseudoStyleRequest(PseudoIdScrollbarCorner),
              layoutObject->style());
      }
    }

    if (!cornerStyle) {
      // If we have an owning ipage/LocalFrame element, then it can set the
      // custom scrollbar also.
      LayoutPartItem layoutItem = m_frame->ownerLayoutItem();
      if (!layoutItem.isNull())
        cornerStyle = layoutItem.getUncachedPseudoStyle(
            PseudoStyleRequest(PseudoIdScrollbarCorner), layoutItem.style());
    }
  }

  if (cornerStyle) {
    if (!m_scrollCorner)
      m_scrollCorner = LayoutScrollbarPart::createAnonymous(doc, this);
    m_scrollCorner->setStyleWithWritingModeOfParent(std::move(cornerStyle));
    setScrollCornerNeedsPaintInvalidation();
  } else if (m_scrollCorner) {
    m_scrollCorner->destroy();
    m_scrollCorner = nullptr;
  }
}

Color FrameView::documentBackgroundColor() const {
  // The LayoutView's background color is set in
  // Document::inheritHtmlAndBodyElementStyles.  Blend this with the base
  // background color of the FrameView. This should match the color drawn by
  // ViewPainter::paintBoxDecorationBackground.
  Color result = baseBackgroundColor();
  LayoutItem documentLayoutObject = layoutViewItem();
  if (!documentLayoutObject.isNull())
    result = result.blend(
        documentLayoutObject.resolveColor(CSSPropertyBackgroundColor));
  return result;
}

FrameView* FrameView::parentFrameView() const {
  if (!parent())
    return nullptr;

  Frame* parentFrame = m_frame->tree().parent();
  if (parentFrame && parentFrame->isLocalFrame())
    return toLocalFrame(parentFrame)->view();

  return nullptr;
}

void FrameView::didChangeGlobalRootScroller() {
  if (!m_frame->settings() || !m_frame->settings()->getViewportEnabled())
    return;

  // Avoid drawing two sets of scrollbars when visual viewport is enabled.
  visualViewportScrollbarsChanged();
}

// TODO(pdr): This logic is similar to adjustScrollbarExistence and the common
// logic should be factored into a helper.
void FrameView::visualViewportScrollbarsChanged() {
  bool hasHorizontalScrollbar = horizontalScrollbar();
  bool hasVerticalScrollbar = verticalScrollbar();
  bool shouldHaveHorizontalScrollbar = false;
  bool shouldHaveVerticalScrollbar = false;
  computeScrollbarExistence(shouldHaveHorizontalScrollbar,
                            shouldHaveVerticalScrollbar, contentsSize());
  m_scrollbarManager.setHasHorizontalScrollbar(shouldHaveHorizontalScrollbar);
  m_scrollbarManager.setHasVerticalScrollbar(shouldHaveVerticalScrollbar);

  if (hasHorizontalScrollbar != shouldHaveHorizontalScrollbar ||
      hasVerticalScrollbar != shouldHaveVerticalScrollbar) {
    scrollbarExistenceDidChange();

    if (!visualViewportSuppliesScrollbars())
      updateScrollbarGeometry();
  }
}

void FrameView::updateWidgetGeometriesIfNeeded() {
  if (!m_needsUpdateWidgetGeometries)
    return;

  m_needsUpdateWidgetGeometries = false;

  updateWidgetGeometries();
}

GeometryMapper& FrameView::geometryMapper() {
  DCHECK(RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled());
  DCHECK(lifecycle().state() >= DocumentLifecycle::InPrePaint);

  if (m_frame->isLocalRoot()) {
    if (!m_geometryMapper)
      m_geometryMapper = GeometryMapper::create();
    return *m_geometryMapper.get();
  }

  return frame().localFrameRoot()->view()->geometryMapper();
}

void FrameView::updateAllLifecyclePhases() {
  frame().localFrameRoot()->view()->updateLifecyclePhasesInternal(
      DocumentLifecycle::PaintClean);
}

// TODO(chrishtr): add a scrolling update lifecycle phase.
void FrameView::updateLifecycleToCompositingCleanPlusScrolling() {
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    updateAllLifecyclePhasesExceptPaint();
  } else {
    frame().localFrameRoot()->view()->updateLifecyclePhasesInternal(
        DocumentLifecycle::CompositingClean);
  }
}

void FrameView::updateAllLifecyclePhasesExceptPaint() {
  frame().localFrameRoot()->view()->updateLifecyclePhasesInternal(
      DocumentLifecycle::PrePaintClean);
}

void FrameView::updateLifecycleToLayoutClean() {
  frame().localFrameRoot()->view()->updateLifecyclePhasesInternal(
      DocumentLifecycle::LayoutClean);
}

void FrameView::scheduleVisualUpdateForPaintInvalidationIfNeeded() {
  LocalFrame* localFrameRoot = frame().localFrameRoot();
  if (localFrameRoot->view()->m_currentUpdateLifecyclePhasesTargetState <
          DocumentLifecycle::PaintInvalidationClean ||
      lifecycle().state() >= DocumentLifecycle::PrePaintClean) {
    // Schedule visual update to process the paint invalidation in the next
    // cycle.
    localFrameRoot->scheduleVisualUpdateUnlessThrottled();
  }
  // Otherwise the paint invalidation will be handled in paint invalidation
  // phase of this cycle.
}

void FrameView::notifyResizeObservers() {
  // Controller exists only if ResizeObserver was created.
  if (!frame().document()->resizeObserverController())
    return;

  ResizeObserverController& resizeController =
      m_frame->document()->ensureResizeObserverController();

  DCHECK(lifecycle().state() >= DocumentLifecycle::LayoutClean);

  size_t minDepth = 0;
  for (minDepth = resizeController.gatherObservations(0);
       minDepth != ResizeObserverController::kDepthBottom;
       minDepth = resizeController.gatherObservations(minDepth)) {
    resizeController.deliverObservations();
    frame().document()->updateStyleAndLayout();
  }

  if (resizeController.skippedObservations()) {
    resizeController.clearObservations();
    ErrorEvent* error = ErrorEvent::create(
        "ResizeObserver loop limit exceeded",
        SourceLocation::capture(m_frame->document()), nullptr);
    m_frame->document()->dispatchErrorEvent(error, NotSharableCrossOrigin);
    // Ensure notifications will get delivered in next cycle.
    if (FrameView* frameView = m_frame->view())
      frameView->scheduleAnimation();
  }

  DCHECK(!layoutView()->needsLayout());
}

void FrameView::dispatchEventsForPrintingOnAllFrames() {
  DCHECK(m_frame->isMainFrame());
  for (Frame* currentFrame = m_frame; currentFrame;
       currentFrame = currentFrame->tree().traverseNext(m_frame)) {
    if (currentFrame->isLocalFrame())
      toLocalFrame(currentFrame)->document()->dispatchEventsForPrinting();
  }
}

void FrameView::setupPrintContext() {
  if (m_frame->document()->printing())
    return;
  if (!m_printContext)
    m_printContext = new PrintContext(m_frame);
  if (m_frame->settings())
    m_frame->settings()->setShouldPrintBackgrounds(true);
  FloatRect pageRect(0, 0, kA4PortraitPageWidth, kA4PortraitPageHeight);
  m_printContext->begin(pageRect.width(), pageRect.height());
  float height;
  m_printContext->computePageRects(pageRect, 0, 0, 1.0, height);
  dispatchEventsForPrintingOnAllFrames();
}

void FrameView::clearPrintContext() {
  if (!m_printContext)
    return;
  m_printContext->end();
  m_printContext.clear();
}

// TODO(leviw): We don't assert lifecycle information from documents in child
// PluginViews.
void FrameView::updateLifecyclePhasesInternal(
    DocumentLifecycle::LifecycleState targetState) {
  if (m_currentUpdateLifecyclePhasesTargetState !=
      DocumentLifecycle::Uninitialized) {
    NOTREACHED() << "FrameView::updateLifecyclePhasesInternal() reentrance";
    return;
  }

  // This must be called from the root frame, since it recurses down, not up.
  // Otherwise the lifecycles of the frames might be out of sync.
  DCHECK(m_frame->isLocalRoot());

  // Only the following target states are supported.
  DCHECK(targetState == DocumentLifecycle::LayoutClean ||
         targetState == DocumentLifecycle::CompositingClean ||
         targetState == DocumentLifecycle::PrePaintClean ||
         targetState == DocumentLifecycle::PaintClean);

  if (!m_frame->document()->isActive())
    return;

  AutoReset<DocumentLifecycle::LifecycleState> targetStateScope(
      &m_currentUpdateLifecyclePhasesTargetState, targetState);

  if (shouldThrottleRendering()) {
    updateViewportIntersectionsForSubtree(
        std::min(targetState, DocumentLifecycle::CompositingClean));
    return;
  }

  if (RuntimeEnabledFeatures::printBrowserEnabled())
    setupPrintContext();
  else
    clearPrintContext();

  updateStyleAndLayoutIfNeededRecursive();
  DCHECK(lifecycle().state() >= DocumentLifecycle::LayoutClean);

  if (targetState == DocumentLifecycle::LayoutClean) {
    updateViewportIntersectionsForSubtree(targetState);
    return;
  }

  forAllNonThrottledFrameViews([](FrameView& frameView) {
    frameView.performScrollAnchoringAdjustments();
  });

  if (targetState == DocumentLifecycle::PaintClean) {
    forAllNonThrottledFrameViews(
        [](FrameView& frameView) { frameView.notifyResizeObservers(); });
  }

  if (LayoutViewItem view = layoutViewItem()) {
    forAllNonThrottledFrameViews([](FrameView& frameView) {
      frameView.checkDoesNotNeedLayout();
      frameView.m_allowsLayoutInvalidationAfterLayoutClean = false;
    });

    {
      TRACE_EVENT1("devtools.timeline", "UpdateLayerTree", "data",
                   InspectorUpdateLayerTreeEvent::data(m_frame.get()));

      if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
        view.compositor()->updateIfNeededRecursive();
      } else {
        forAllNonThrottledFrameViews([](FrameView& frameView) {
          frameView.layoutView()->layer()->updateDescendantDependentFlags();
          frameView.layoutView()->commitPendingSelection();
        });
      }

      scrollContentsIfNeededRecursive();
      DCHECK(RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled() ||
             lifecycle().state() >= DocumentLifecycle::CompositingClean);

      m_frame->host()->globalRootScrollerController().didUpdateCompositing();

      if (targetState >= DocumentLifecycle::PrePaintClean) {
        if (!RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
          invalidateTreeIfNeededRecursive();

        if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
          if (view.compositor()->inCompositingMode())
            scrollingCoordinator()->updateAfterCompositingChangeIfNeeded();
        }

        if (LocalFrame* localFrame = m_frame->localFrameRoot()) {
          // This is needed since, at present, the ScrollingCoordinator doesn't
          // send rects for oopif sub-frames.
          // TODO(wjmaclean): Remove this pathway when ScrollingCoordinator
          // operates on a per-frame basis. https://crbug.com/680606
          frame().page()->chromeClient().updateEventRectsForSubframeIfNecessary(
              localFrame);
        }
        updateCompositedSelectionIfNeeded();
      }

      // TODO(pdr): prePaint should be under the "Paint" devtools timeline step
      // for slimming paint v2.
      if (targetState >= DocumentLifecycle::PrePaintClean)
        prePaint();
    }

    if (targetState == DocumentLifecycle::PaintClean) {
      if (!m_frame->document()->printing() ||
          RuntimeEnabledFeatures::printBrowserEnabled())
        paintTree();

      if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
        pushPaintArtifactToCompositor();

      DCHECK(!view.hasPendingSelection());
      DCHECK((m_frame->document()->printing() &&
              lifecycle().state() == DocumentLifecycle::PrePaintClean) ||
             lifecycle().state() == DocumentLifecycle::PaintClean);

      if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
        DocumentAnimations::updateAnimations(layoutView()->document(),
                                             DocumentLifecycle::PaintClean);
      }
    }

    forAllNonThrottledFrameViews([](FrameView& frameView) {
      frameView.checkDoesNotNeedLayout();
      frameView.m_allowsLayoutInvalidationAfterLayoutClean = true;
    });
  }

  updateViewportIntersectionsForSubtree(targetState);
}

void FrameView::enqueueScrollAnchoringAdjustment(
    ScrollableArea* scrollableArea) {
  m_anchoringAdjustmentQueue.insert(scrollableArea);
}

void FrameView::performScrollAnchoringAdjustments() {
  for (WeakMember<ScrollableArea>& scroller : m_anchoringAdjustmentQueue) {
    if (scroller) {
      DCHECK(scroller->scrollAnchor());
      scroller->scrollAnchor()->adjust();
    }
  }
  m_anchoringAdjustmentQueue.clear();
}

void FrameView::prePaint() {
  TRACE_EVENT0("blink", "FrameView::prePaint");

  if (!m_paintController)
    m_paintController = PaintController::create();

  forAllNonThrottledFrameViews([](FrameView& frameView) {
    frameView.lifecycle().advanceTo(DocumentLifecycle::InPrePaint);
    if (frameView.canThrottleRendering()) {
      // This frame can be throttled but not throttled, meaning we are not in an
      // AllowThrottlingScope. Now this frame may contain dirty paint flags, and
      // we need to propagate the flags into the ancestor chain so that
      // PrePaintTreeWalk can reach this frame.
      frameView.setNeedsPaintPropertyUpdate();
    }
  });

  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled()) {
    // TODO(chrishtr): the cache only needs to be invalidated if one or more of
    // the property tree nodes changed.
    geometryMapper().clearCache();
  }

  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled()) {
    SCOPED_BLINK_UMA_HISTOGRAM_TIMER("Blink.PrePaint.UpdateTime");
    PrePaintTreeWalk(geometryMapper()).walk(*this);
  }

  forAllNonThrottledFrameViews([](FrameView& frameView) {
    frameView.lifecycle().advanceTo(DocumentLifecycle::PrePaintClean);
  });
}

void FrameView::notifyPaint(const PaintController& paintController) const {
  DCHECK(m_frame->document());
  PaintTiming::from(*m_frame->document())
      .notifyPaint(paintController.firstPainted(),
                   paintController.textPainted(),
                   paintController.imagePainted());
}

void FrameView::paintTree() {
  TRACE_EVENT0("blink", "FrameView::paintTree");
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER("Blink.Paint.UpdateTime");

  ASSERT(frame() == page()->mainFrame() ||
         (!frame().tree().parent()->isLocalFrame()));

  LayoutViewItem view = layoutViewItem();
  ASSERT(!view.isNull());
  forAllNonThrottledFrameViews([](FrameView& frameView) {
    frameView.lifecycle().advanceTo(DocumentLifecycle::InPaint);
  });

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    if (layoutView()->layer()->needsRepaint()) {
      GraphicsContext graphicsContext(*m_paintController);
      if (RuntimeEnabledFeatures::printBrowserEnabled())
        graphicsContext.setPrinting(true);
      paint(graphicsContext, CullRect(LayoutRect::infiniteIntRect()));
      m_paintController->commitNewDisplayItems(LayoutSize());
      notifyPaint(*m_paintController);
    }
  } else {
    // A null graphics layer can occur for painting of SVG images that are not
    // parented into the main frame tree, or when the FrameView is the main
    // frame view of a page overlay. The page overlay is in the layer tree of
    // the host page and will be painted during painting of the host page.
    if (GraphicsLayer* rootGraphicsLayer =
            view.compositor()->rootGraphicsLayer()) {
      paintGraphicsLayerRecursively(rootGraphicsLayer);
    }

    // TODO(sataya.m):Main frame doesn't create RootFrameViewport in some
    // webkit_unit_tests (http://crbug.com/644788).
    if (m_viewportScrollableArea) {
      if (GraphicsLayer* layerForHorizontalScrollbar =
              m_viewportScrollableArea->layerForHorizontalScrollbar()) {
        paintGraphicsLayerRecursively(layerForHorizontalScrollbar);
      }
      if (GraphicsLayer* layerForVerticalScrollbar =
              m_viewportScrollableArea->layerForVerticalScrollbar()) {
        paintGraphicsLayerRecursively(layerForVerticalScrollbar);
      }
      if (GraphicsLayer* layerForScrollCorner =
              m_viewportScrollableArea->layerForScrollCorner()) {
        paintGraphicsLayerRecursively(layerForScrollCorner);
      }
    }
  }

  forAllNonThrottledFrameViews([](FrameView& frameView) {
    frameView.lifecycle().advanceTo(DocumentLifecycle::PaintClean);
    LayoutViewItem layoutViewItem = frameView.layoutViewItem();
    if (!layoutViewItem.isNull())
      layoutViewItem.layer()->clearNeedsRepaintRecursively();
  });
}

void FrameView::paintGraphicsLayerRecursively(GraphicsLayer* graphicsLayer) {
  if (graphicsLayer->drawsContent()) {
    graphicsLayer->paint(nullptr);
    notifyPaint(graphicsLayer->getPaintController());
  }

  if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    if (GraphicsLayer* maskLayer = graphicsLayer->maskLayer())
      paintGraphicsLayerRecursively(maskLayer);
    if (GraphicsLayer* contentsClippingMaskLayer =
            graphicsLayer->contentsClippingMaskLayer())
      paintGraphicsLayerRecursively(contentsClippingMaskLayer);
  }

  for (auto& child : graphicsLayer->children())
    paintGraphicsLayerRecursively(child);
}

void FrameView::pushPaintArtifactToCompositor() {
  TRACE_EVENT0("blink", "FrameView::pushPaintArtifactToCompositor");

  DCHECK(RuntimeEnabledFeatures::slimmingPaintV2Enabled());

  Page* page = frame().page();
  if (!page)
    return;

  if (!m_paintArtifactCompositor) {
    m_paintArtifactCompositor = PaintArtifactCompositor::create();
    page->chromeClient().attachRootLayer(
        m_paintArtifactCompositor->getWebLayer(), &frame());
  }

  SCOPED_BLINK_UMA_HISTOGRAM_TIMER("Blink.Compositing.UpdateTime");

  DCHECK(m_geometryMapper.get());
  m_paintArtifactCompositor->update(
      m_paintController->paintArtifact(),
      m_paintController->paintChunksRasterInvalidationTrackingMap(),
      m_isStoringCompositedLayerDebugInfo, *m_geometryMapper);
}

std::unique_ptr<JSONObject> FrameView::compositedLayersAsJSON(
    LayerTreeFlags flags) {
  return frame()
      .localFrameRoot()
      ->view()
      ->m_paintArtifactCompositor->layersAsJSON(flags);
}

void FrameView::updateStyleAndLayoutIfNeededRecursive() {
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER("Blink.StyleAndLayout.UpdateTime");
  updateStyleAndLayoutIfNeededRecursiveInternal();
}

void FrameView::updateStyleAndLayoutIfNeededRecursiveInternal() {
  if (shouldThrottleRendering() || !m_frame->document()->isActive())
    return;

  ScopedFrameBlamer frameBlamer(m_frame);
  TRACE_EVENT0("blink", "FrameView::updateStyleAndLayoutIfNeededRecursive");

  // We have to crawl our entire subtree looking for any FrameViews that need
  // layout and make sure they are up to date.
  // Mac actually tests for intersection with the dirty region and tries not to
  // update layout for frames that are outside the dirty region.  Not only does
  // this seem pointless (since those frames will have set a zero timer to
  // layout anyway), but it is also incorrect, since if two frames overlap, the
  // first could be excluded from the dirty region but then become included
  // later by the second frame adding rects to the dirty region when it lays
  // out.

  m_frame->document()->updateStyleAndLayoutTree();

  CHECK(!shouldThrottleRendering());
  CHECK(m_frame->document()->isActive());
  CHECK(!m_nestedLayoutCount);

  if (needsLayout())
    layout();

  checkDoesNotNeedLayout();

  // WebView plugins need to update regardless of whether the
  // LayoutEmbeddedObject that owns them needed layout.
  // TODO(leviw): This currently runs the entire lifecycle on plugin WebViews.
  // We should have a way to only run these other Documents to the same
  // lifecycle stage as this frame.
  const ChildrenWidgetSet* viewChildren = children();
  for (const Member<FrameViewBase>& child : *viewChildren) {
    if ((*child).isPluginContainer())
      toPluginView(child.get())->updateAllLifecyclePhases();
  }
  checkDoesNotNeedLayout();

  // FIXME: Calling layout() shouldn't trigger script execution or have any
  // observable effects on the frame tree but we're not quite there yet.
  HeapVector<Member<FrameView>> frameViews;
  for (Frame* child = m_frame->tree().firstChild(); child;
       child = child->tree().nextSibling()) {
    if (!child->isLocalFrame())
      continue;
    if (FrameView* view = toLocalFrame(child)->view())
      frameViews.push_back(view);
  }

  for (const auto& frameView : frameViews)
    frameView->updateStyleAndLayoutIfNeededRecursiveInternal();

  // These asserts ensure that parent frames are clean, when child frames
  // finished updating layout and style.
  checkDoesNotNeedLayout();
#if DCHECK_IS_ON()
  m_frame->document()->layoutView()->assertLaidOut();
#endif

  updateWidgetGeometriesIfNeeded();

  if (lifecycle().state() < DocumentLifecycle::LayoutClean)
    lifecycle().advanceTo(DocumentLifecycle::LayoutClean);

  // Ensure that we become visually non-empty eventually.
  // TODO(esprehn): This should check isRenderingReady() instead.
  if (frame().document()->hasFinishedParsing() &&
      frame().loader().stateMachine()->committedFirstRealDocumentLoad())
    m_isVisuallyNonEmpty = true;

  frame().selection().updateStyleAndLayoutIfNeeded();
  frame().page()->dragCaret().updateStyleAndLayoutIfNeeded();
}

void FrameView::invalidateTreeIfNeededRecursive() {
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER("Blink.PaintInvalidation.UpdateTime");
  {
    // For comparison to SlimmingPaintInvalidation.
    SCOPED_BLINK_UMA_HISTOGRAM_TIMER("Blink.PrePaint.UpdateTime");
    invalidateTreeIfNeededRecursiveInternal();
  }
}

void FrameView::invalidateTreeIfNeededRecursiveInternal() {
  DCHECK(!RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled());
  CHECK(layoutView());

  // We need to stop recursing here since a child frame view might not be
  // throttled even though we are (e.g., it didn't compute its visibility yet).
  if (shouldThrottleRendering())
    return;
  TRACE_EVENT0("blink", "FrameView::invalidateTreeIfNeededRecursiveInternal");

  Vector<const LayoutObject*> pendingDelayedPaintInvalidations;
  PaintInvalidationState rootPaintInvalidationState(
      *layoutView(), pendingDelayedPaintInvalidations);

  if (lifecycle().state() < DocumentLifecycle::PaintInvalidationClean)
    invalidateTreeIfNeeded(rootPaintInvalidationState);

  // Some frames may be not reached during the above invalidateTreeIfNeeded
  // because
  // - the frame is a detached frame; or
  // - it didn't need paint invalidation.
  // We need to call invalidateTreeIfNeededRecursiveInternal() for such frames
  // to finish required paint invalidation and advance their life cycle state.
  for (Frame* child = m_frame->tree().firstChild(); child;
       child = child->tree().nextSibling()) {
    if (child->isLocalFrame()) {
      FrameView& childFrameView = *toLocalFrame(child)->view();
      // The children frames can be in any state, including stopping.
      // Thus we have to check that it makes sense to do paint
      // invalidation onto them here.
      if (!childFrameView.layoutView())
        continue;
      childFrameView.invalidateTreeIfNeededRecursiveInternal();
    }
  }

  // Process objects needing paint invalidation on the next frame. See the
  // definition of PaintInvalidationDelayedFull for more details.
  for (auto& target : pendingDelayedPaintInvalidations)
    target->getMutableForPainting().setShouldDoFullPaintInvalidation(
        PaintInvalidationDelayedFull);
}

void FrameView::enableAutoSizeMode(const IntSize& minSize,
                                   const IntSize& maxSize) {
  if (!m_autoSizeInfo)
    m_autoSizeInfo = FrameViewAutoSizeInfo::create(this);

  m_autoSizeInfo->configureAutoSizeMode(minSize, maxSize);
  setLayoutSizeFixedToFrameSize(true);
  setNeedsLayout();
  scheduleRelayout();
}

void FrameView::disableAutoSizeMode() {
  if (!m_autoSizeInfo)
    return;

  setLayoutSizeFixedToFrameSize(false);
  setNeedsLayout();
  scheduleRelayout();

  // Since autosize mode forces the scrollbar mode, change them to being auto.
  setVerticalScrollbarLock(false);
  setHorizontalScrollbarLock(false);
  setScrollbarModes(ScrollbarAuto, ScrollbarAuto);
  m_autoSizeInfo.clear();
}

void FrameView::forceLayoutForPagination(const FloatSize& pageSize,
                                         const FloatSize& originalPageSize,
                                         float maximumShrinkFactor) {
  // Dumping externalRepresentation(m_frame->layoutObject()).ascii() is a good
  // trick to see the state of things before and after the layout
  if (LayoutView* layoutView = this->layoutView()) {
    float pageLogicalWidth = layoutView->style()->isHorizontalWritingMode()
                                 ? pageSize.width()
                                 : pageSize.height();
    float pageLogicalHeight = layoutView->style()->isHorizontalWritingMode()
                                  ? pageSize.height()
                                  : pageSize.width();

    LayoutUnit flooredPageLogicalWidth =
        static_cast<LayoutUnit>(pageLogicalWidth);
    LayoutUnit flooredPageLogicalHeight =
        static_cast<LayoutUnit>(pageLogicalHeight);
    layoutView->setLogicalWidth(flooredPageLogicalWidth);
    layoutView->setPageLogicalHeight(flooredPageLogicalHeight);
    layoutView->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
        LayoutInvalidationReason::PrintingChanged);
    layout();

    // If we don't fit in the given page width, we'll lay out again. If we don't
    // fit in the page width when shrunk, we will lay out at maximum shrink and
    // clip extra content.
    // FIXME: We are assuming a shrink-to-fit printing implementation.  A
    // cropping implementation should not do this!
    bool horizontalWritingMode = layoutView->style()->isHorizontalWritingMode();
    const LayoutRect& documentRect = LayoutRect(layoutView->documentRect());
    LayoutUnit docLogicalWidth =
        horizontalWritingMode ? documentRect.width() : documentRect.height();
    if (docLogicalWidth > pageLogicalWidth) {
      FloatSize expectedPageSize(
          std::min<float>(documentRect.width().toFloat(),
                          pageSize.width() * maximumShrinkFactor),
          std::min<float>(documentRect.height().toFloat(),
                          pageSize.height() * maximumShrinkFactor));
      FloatSize maxPageSize = m_frame->resizePageRectsKeepingRatio(
          FloatSize(originalPageSize.width(), originalPageSize.height()),
          expectedPageSize);
      pageLogicalWidth =
          horizontalWritingMode ? maxPageSize.width() : maxPageSize.height();
      pageLogicalHeight =
          horizontalWritingMode ? maxPageSize.height() : maxPageSize.width();

      flooredPageLogicalWidth = static_cast<LayoutUnit>(pageLogicalWidth);
      flooredPageLogicalHeight = static_cast<LayoutUnit>(pageLogicalHeight);
      layoutView->setLogicalWidth(flooredPageLogicalWidth);
      layoutView->setPageLogicalHeight(flooredPageLogicalHeight);
      layoutView->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
          LayoutInvalidationReason::PrintingChanged);
      layout();

      const LayoutRect& updatedDocumentRect =
          LayoutRect(layoutView->documentRect());
      LayoutUnit docLogicalHeight = horizontalWritingMode
                                        ? updatedDocumentRect.height()
                                        : updatedDocumentRect.width();
      LayoutUnit docLogicalTop = horizontalWritingMode
                                     ? updatedDocumentRect.y()
                                     : updatedDocumentRect.x();
      LayoutUnit docLogicalRight = horizontalWritingMode
                                       ? updatedDocumentRect.maxX()
                                       : updatedDocumentRect.maxY();
      LayoutUnit clippedLogicalLeft;
      if (!layoutView->style()->isLeftToRightDirection())
        clippedLogicalLeft = LayoutUnit(docLogicalRight - pageLogicalWidth);
      LayoutRect overflow(clippedLogicalLeft, docLogicalTop,
                          LayoutUnit(pageLogicalWidth), docLogicalHeight);

      if (!horizontalWritingMode)
        overflow = overflow.transposedRect();
      layoutView->clearLayoutOverflow();
      layoutView->addLayoutOverflow(
          overflow);  // This is how we clip in case we overflow again.
    }
  }

  adjustViewSizeAndLayout();
}

IntRect FrameView::convertFromLayoutItem(
    const LayoutItem& layoutItem,
    const IntRect& layoutObjectRect) const {
  // Convert from page ("absolute") to FrameView coordinates.
  LayoutRect rect = enclosingLayoutRect(
      layoutItem.localToAbsoluteQuad(FloatRect(layoutObjectRect))
          .boundingBox());
  rect.move(LayoutSize(-getScrollOffset()));
  return pixelSnappedIntRect(rect);
}

IntRect FrameView::convertToLayoutItem(const LayoutItem& layoutItem,
                                       const IntRect& frameRect) const {
  IntRect rectInContent = frameToContents(frameRect);

  // Convert from FrameView coords into page ("absolute") coordinates.
  rectInContent.move(scrollOffsetInt());

  // FIXME: we don't have a way to map an absolute rect down to a local quad, so
  // just move the rect for now.
  rectInContent.setLocation(roundedIntPoint(
      layoutItem.absoluteToLocal(rectInContent.location(), UseTransforms)));
  return rectInContent;
}

IntPoint FrameView::convertFromLayoutItem(
    const LayoutItem& layoutItem,
    const IntPoint& layoutObjectPoint) const {
  IntPoint point = roundedIntPoint(
      layoutItem.localToAbsolute(layoutObjectPoint, UseTransforms));

  // Convert from page ("absolute") to FrameView coordinates.
  point.move(-scrollOffsetInt());
  return point;
}

IntPoint FrameView::convertToLayoutItem(const LayoutItem& layoutItem,
                                        const IntPoint& framePoint) const {
  IntPoint point = framePoint;

  // Convert from FrameView coords into page ("absolute") coordinates.
  point += IntSize(scrollX(), scrollY());

  return roundedIntPoint(layoutItem.absoluteToLocal(point, UseTransforms));
}

IntRect FrameView::convertToContainingWidget(const IntRect& localRect) const {
  if (const FrameView* parentView = toFrameView(parent())) {
    // Get our layoutObject in the parent view
    LayoutPartItem layoutItem = m_frame->ownerLayoutItem();
    if (layoutItem.isNull())
      return localRect;

    IntRect rect(localRect);
    // Add borders and padding??
    rect.move((layoutItem.borderLeft() + layoutItem.paddingLeft()).toInt(),
              (layoutItem.borderTop() + layoutItem.paddingTop()).toInt());
    return parentView->convertFromLayoutItem(layoutItem, rect);
  }

  return localRect;
}

IntRect FrameView::convertFromContainingWidget(
    const IntRect& parentRect) const {
  if (const FrameView* parentView = toFrameView(parent())) {
    // Get our layoutObject in the parent view
    LayoutPartItem layoutItem = m_frame->ownerLayoutItem();
    if (layoutItem.isNull())
      return parentRect;

    IntRect rect = parentView->convertToLayoutItem(layoutItem, parentRect);
    // Subtract borders and padding
    rect.move((-layoutItem.borderLeft() - layoutItem.paddingLeft()).toInt(),
              (-layoutItem.borderTop() - layoutItem.paddingTop()).toInt());
    return rect;
  }

  return parentRect;
}

IntPoint FrameView::convertToContainingWidget(
    const IntPoint& localPoint) const {
  if (const FrameView* parentView = toFrameView(parent())) {
    // Get our layoutObject in the parent view
    LayoutPartItem layoutItem = m_frame->ownerLayoutItem();
    if (layoutItem.isNull())
      return localPoint;

    IntPoint point(localPoint);

    // Add borders and padding
    point.move((layoutItem.borderLeft() + layoutItem.paddingLeft()).toInt(),
               (layoutItem.borderTop() + layoutItem.paddingTop()).toInt());
    return parentView->convertFromLayoutItem(layoutItem, point);
  }

  return localPoint;
}

IntPoint FrameView::convertFromContainingWidget(
    const IntPoint& parentPoint) const {
  if (const FrameView* parentView = toFrameView(parent())) {
    // Get our layoutObject in the parent view
    LayoutPartItem layoutItem = m_frame->ownerLayoutItem();
    if (layoutItem.isNull())
      return parentPoint;

    IntPoint point = parentView->convertToLayoutItem(layoutItem, parentPoint);
    // Subtract borders and padding
    point.move((-layoutItem.borderLeft() - layoutItem.paddingLeft()).toInt(),
               (-layoutItem.borderTop() - layoutItem.paddingTop()).toInt());
    return point;
  }

  return parentPoint;
}

void FrameView::setInitialTracksPaintInvalidationsForTesting(
    bool trackPaintInvalidations) {
  s_initialTrackAllPaintInvalidations = trackPaintInvalidations;
}

void FrameView::setTracksPaintInvalidations(bool trackPaintInvalidations) {
  if (trackPaintInvalidations == isTrackingPaintInvalidations())
    return;

  for (Frame* frame = m_frame->tree().top(); frame;
       frame = frame->tree().traverseNext()) {
    if (!frame->isLocalFrame())
      continue;
    if (LayoutViewItem layoutView = toLocalFrame(frame)->contentLayoutItem()) {
      layoutView.frameView()->m_trackedObjectPaintInvalidations =
          WTF::wrapUnique(trackPaintInvalidations
                              ? new Vector<ObjectPaintInvalidation>
                              : nullptr);
      if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
        m_paintController->setTracksRasterInvalidations(
            trackPaintInvalidations);
        m_paintArtifactCompositor->setTracksRasterInvalidations(
            trackPaintInvalidations);
      } else {
        layoutView.compositor()->setTracksRasterInvalidations(
            trackPaintInvalidations);
      }
    }
  }

  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("blink.invalidation"),
                       "FrameView::setTracksPaintInvalidations",
                       TRACE_EVENT_SCOPE_GLOBAL, "enabled",
                       trackPaintInvalidations);
}

void FrameView::trackObjectPaintInvalidation(const DisplayItemClient& client,
                                             PaintInvalidationReason reason) {
  if (!m_trackedObjectPaintInvalidations)
    return;

  ObjectPaintInvalidation invalidation = {client.debugName(), reason};
  m_trackedObjectPaintInvalidations->push_back(invalidation);
}

std::unique_ptr<JSONArray> FrameView::trackedObjectPaintInvalidationsAsJSON()
    const {
  if (!m_trackedObjectPaintInvalidations)
    return nullptr;

  std::unique_ptr<JSONArray> result = JSONArray::create();
  for (Frame* frame = m_frame->tree().top(); frame;
       frame = frame->tree().traverseNext()) {
    if (!frame->isLocalFrame())
      continue;
    if (LayoutViewItem layoutView = toLocalFrame(frame)->contentLayoutItem()) {
      if (!layoutView.frameView()->m_trackedObjectPaintInvalidations)
        continue;
      for (const auto& item :
           *layoutView.frameView()->m_trackedObjectPaintInvalidations) {
        std::unique_ptr<JSONObject> itemJSON = JSONObject::create();
        itemJSON->setString("object", item.name);
        itemJSON->setString("reason",
                            paintInvalidationReasonToString(item.reason));
        result->pushObject(std::move(itemJSON));
      }
    }
  }
  return result;
}

void FrameView::addResizerArea(LayoutBox& resizerBox) {
  if (!m_resizerAreas)
    m_resizerAreas = WTF::wrapUnique(new ResizerAreaSet);
  m_resizerAreas->insert(&resizerBox);
}

void FrameView::removeResizerArea(LayoutBox& resizerBox) {
  if (!m_resizerAreas)
    return;

  ResizerAreaSet::iterator it = m_resizerAreas->find(&resizerBox);
  if (it != m_resizerAreas->end())
    m_resizerAreas->erase(it);
}

void FrameView::addScrollableArea(ScrollableArea* scrollableArea) {
  ASSERT(scrollableArea);
  if (!m_scrollableAreas)
    m_scrollableAreas = new ScrollableAreaSet;
  m_scrollableAreas->insert(scrollableArea);

  if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
    scrollingCoordinator->scrollableAreasDidChange();
}

void FrameView::removeScrollableArea(ScrollableArea* scrollableArea) {
  if (!m_scrollableAreas)
    return;
  m_scrollableAreas->erase(scrollableArea);

  if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
    scrollingCoordinator->scrollableAreasDidChange();
}

void FrameView::addAnimatingScrollableArea(ScrollableArea* scrollableArea) {
  ASSERT(scrollableArea);
  if (!m_animatingScrollableAreas)
    m_animatingScrollableAreas = new ScrollableAreaSet;
  m_animatingScrollableAreas->insert(scrollableArea);
}

void FrameView::removeAnimatingScrollableArea(ScrollableArea* scrollableArea) {
  if (!m_animatingScrollableAreas)
    return;
  m_animatingScrollableAreas->erase(scrollableArea);
}

void FrameView::setParent(FrameViewBase* parentView) {
  if (parentView == parent())
    return;

  FrameViewBase::setParent(parentView);

  updateParentScrollableAreaSet();
  setupRenderThrottling();

  if (parentFrameView())
    m_subtreeThrottled = parentFrameView()->canThrottleRendering();
}

void FrameView::removeChild(FrameViewBase* child) {
  ASSERT(child->parent() == this);

  if (child->isFrameView() &&
      !RuntimeEnabledFeatures::rootLayerScrollingEnabled())
    removeScrollableArea(toFrameView(child));

  child->setParent(0);
  m_children.erase(child);
}

bool FrameView::visualViewportSuppliesScrollbars() {
  // On desktop, we always use the layout viewport's scrollbars.
  if (!m_frame->settings() || !m_frame->settings()->getViewportEnabled() ||
      !m_frame->document() || !m_frame->host())
    return false;

  const TopDocumentRootScrollerController& controller =
      m_frame->host()->globalRootScrollerController();

  if (!layoutViewportScrollableArea())
    return false;

  return RootScrollerUtil::scrollableAreaForRootScroller(
             controller.globalRootScroller()) == layoutViewportScrollableArea();
}

AXObjectCache* FrameView::axObjectCache() const {
  if (frame().document())
    return frame().document()->existingAXObjectCache();
  return nullptr;
}

void FrameView::setCursor(const Cursor& cursor) {
  Page* page = frame().page();
  if (!page || m_frame->eventHandler().isMousePositionUnknown())
    return;
  page->chromeClient().setCursor(cursor, m_frame);
}

void FrameView::frameRectsChanged() {
  TRACE_EVENT0("blink", "FrameView::frameRectsChanged");
  if (layoutSizeFixedToFrameSize())
    setLayoutSizeInternal(frameRect().size());

  for (const auto& child : m_children)
    child->frameRectsChanged();
}

void FrameView::setLayoutSizeInternal(const IntSize& size) {
  if (m_layoutSize == size)
    return;

  m_layoutSize = size;
  contentsResized();
}

void FrameView::didAddScrollbar(Scrollbar& scrollbar,
                                ScrollbarOrientation orientation) {
  ScrollableArea::didAddScrollbar(scrollbar, orientation);
}

PaintLayer* FrameView::layer() const {
  LayoutViewItem layoutView = layoutViewItem();
  if (layoutView.isNull() || !layoutView.compositor())
    return nullptr;

  return layoutView.compositor()->rootLayer();
}

IntSize FrameView::maximumScrollOffsetInt() const {
  // Make the same calculation as in CC's LayerImpl::MaxScrollOffset()
  // FIXME: We probably shouldn't be storing the bounds in a float.
  // crbug.com/422331.
  IntSize visibleSize = visibleContentSize(ExcludeScrollbars);
  IntSize contentBounds = contentsSize();

  FrameHost* host = m_frame->host();
  DCHECK(host);

  // We need to perform this const_cast since maximumScrollOffsetInt is a const
  // method but we can't make layoutViewportScrollableArea const since it can
  // return |this|. Once root-layer-scrolls ships layoutViewportScrollableArea
  // can be made const.
  const ScrollableArea* layoutViewport =
      const_cast<FrameView*>(this)->layoutViewportScrollableArea();
  TopDocumentRootScrollerController& controller =
      host->globalRootScrollerController();
  if (layoutViewport == controller.rootScrollerArea())
    visibleSize = controller.rootScrollerVisibleArea();

  IntSize maximumOffset =
      toIntSize(-scrollOrigin() + (contentBounds - visibleSize));
  return maximumOffset.expandedTo(minimumScrollOffsetInt());
}

void FrameView::addChild(FrameViewBase* child) {
  ASSERT(child != this && !child->parent());
  child->setParent(this);
  m_children.insert(child);
}

void FrameView::setScrollbarModes(ScrollbarMode horizontalMode,
                                  ScrollbarMode verticalMode,
                                  bool horizontalLock,
                                  bool verticalLock) {
  bool needsUpdate = false;

  // If the page's overflow setting has disabled scrolling, do not allow
  // anything to override that setting, http://crbug.com/426447
  LayoutObject* viewport = viewportLayoutObject();
  if (viewport && !shouldIgnoreOverflowHidden()) {
    if (viewport->style()->overflowX() == EOverflow::kHidden)
      horizontalMode = ScrollbarAlwaysOff;
    if (viewport->style()->overflowY() == EOverflow::kHidden)
      verticalMode = ScrollbarAlwaysOff;
  }

  if (horizontalMode != horizontalScrollbarMode() &&
      !m_horizontalScrollbarLock) {
    m_horizontalScrollbarMode = horizontalMode;
    needsUpdate = true;
  }

  if (verticalMode != verticalScrollbarMode() && !m_verticalScrollbarLock) {
    m_verticalScrollbarMode = verticalMode;
    needsUpdate = true;
  }

  if (horizontalLock)
    setHorizontalScrollbarLock();

  if (verticalLock)
    setVerticalScrollbarLock();

  if (!needsUpdate)
    return;

  updateScrollbars();

  if (!layerForScrolling())
    return;
  WebLayer* layer = layerForScrolling()->platformLayer();
  if (!layer)
    return;
  layer->setUserScrollable(userInputScrollable(HorizontalScrollbar),
                           userInputScrollable(VerticalScrollbar));
}

IntSize FrameView::visibleContentSize(
    IncludeScrollbarsInRect scrollbarInclusion) const {
  return scrollbarInclusion == ExcludeScrollbars
             ? excludeScrollbars(frameRect().size())
             : frameRect().size();
}

IntRect FrameView::visibleContentRect(
    IncludeScrollbarsInRect scrollbarInclusion) const {
  return IntRect(IntPoint(flooredIntSize(m_scrollOffset)),
                 visibleContentSize(scrollbarInclusion));
}

IntSize FrameView::contentsSize() const {
  return m_contentsSize;
}

void FrameView::clipPaintRect(FloatRect* paintRect) const {
  // Paint the whole rect if "mainFrameClipsContent" is false, meaning that
  // WebPreferences::record_whole_document is true.
  if (!m_frame->settings()->getMainFrameClipsContent())
    return;

  paintRect->intersect(
      page()->chromeClient().visibleContentRectForPainting().value_or(
          visibleContentRect()));
}

IntSize FrameView::minimumScrollOffsetInt() const {
  return IntSize(-scrollOrigin().x(), -scrollOrigin().y());
}

void FrameView::adjustScrollbarOpacity() {
  if (horizontalScrollbar() && layerForHorizontalScrollbar()) {
    bool isOpaqueScrollbar = !horizontalScrollbar()->isOverlayScrollbar();
    layerForHorizontalScrollbar()->setContentsOpaque(isOpaqueScrollbar);
  }
  if (verticalScrollbar() && layerForVerticalScrollbar()) {
    bool isOpaqueScrollbar = !verticalScrollbar()->isOverlayScrollbar();
    layerForVerticalScrollbar()->setContentsOpaque(isOpaqueScrollbar);
  }
}

int FrameView::scrollSize(ScrollbarOrientation orientation) const {
  Scrollbar* scrollbar =
      ((orientation == HorizontalScrollbar) ? horizontalScrollbar()
                                            : verticalScrollbar());

  // If no scrollbars are present, the content may still be scrollable.
  if (!scrollbar) {
    IntSize scrollSize = m_contentsSize - visibleContentRect().size();
    scrollSize.clampNegativeToZero();
    return orientation == HorizontalScrollbar ? scrollSize.width()
                                              : scrollSize.height();
  }

  return scrollbar->totalSize() - scrollbar->visibleSize();
}

void FrameView::updateScrollOffset(const ScrollOffset& offset,
                                   ScrollType scrollType) {
  ScrollOffset scrollDelta = offset - m_scrollOffset;
  if (scrollDelta.isZero())
    return;

  showOverlayScrollbars();

  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
    // Don't scroll the FrameView!
    ASSERT_NOT_REACHED();
  }

  m_scrollOffset = offset;

  if (!scrollbarsSuppressed())
    m_pendingScrollDelta += scrollDelta;

  if (scrollTypeClearsFragmentAnchor(scrollType))
    clearFragmentAnchor();
  updateLayersAndCompositingAfterScrollIfNeeded();

  Document* document = m_frame->document();
  document->enqueueScrollEventForNode(document);

  m_frame->eventHandler().dispatchFakeMouseMoveEventSoon();
  if (scrollType == UserScroll || scrollType == CompositorScroll) {
    Page* page = frame().page();
    if (page)
      page->chromeClient().clearToolTip(*m_frame);
  }

  LayoutViewItem layoutViewItem = document->layoutViewItem();
  if (!layoutViewItem.isNull()) {
    if (layoutViewItem.usesCompositing())
      layoutViewItem.compositor()->frameViewDidScroll();
    layoutViewItem.clearHitTestCache();
  }

  m_didScrollTimer.startOneShot(resourcePriorityUpdateDelayAfterScroll,
                                BLINK_FROM_HERE);

  if (AXObjectCache* cache = m_frame->document()->existingAXObjectCache())
    cache->handleScrollPositionChanged(this);

  frame().loader().saveScrollState();
  didChangeScrollOffset();

  if (scrollType == CompositorScroll && m_frame->isMainFrame()) {
    if (DocumentLoader* documentLoader = m_frame->loader().documentLoader())
      documentLoader->initialScrollState().wasScrolledByUser = true;
  }

  if (scrollType != AnchoringScroll && scrollType != ClampingScroll)
    clearScrollAnchor();
}

void FrameView::didChangeScrollOffset() {
  frame().loader().client()->didChangeScrollOffset();
  if (frame().isMainFrame())
    frame().page()->chromeClient().mainFrameScrollOffsetChanged();
}

void FrameView::clearScrollAnchor() {
  if (!RuntimeEnabledFeatures::scrollAnchoringEnabled())
    return;
  m_scrollAnchor.clear();
}

bool FrameView::hasOverlayScrollbars() const {
  return (horizontalScrollbar() &&
          horizontalScrollbar()->isOverlayScrollbar()) ||
         (verticalScrollbar() && verticalScrollbar()->isOverlayScrollbar());
}

void FrameView::computeScrollbarExistence(
    bool& newHasHorizontalScrollbar,
    bool& newHasVerticalScrollbar,
    const IntSize& docSize,
    ComputeScrollbarExistenceOption option) {
  if ((m_frame->settings() && m_frame->settings()->getHideScrollbars()) ||
      visualViewportSuppliesScrollbars()) {
    newHasHorizontalScrollbar = false;
    newHasVerticalScrollbar = false;
    return;
  }

  bool hasHorizontalScrollbar = horizontalScrollbar();
  bool hasVerticalScrollbar = verticalScrollbar();

  newHasHorizontalScrollbar = hasHorizontalScrollbar;
  newHasVerticalScrollbar = hasVerticalScrollbar;

  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled())
    return;

  ScrollbarMode hScroll = m_horizontalScrollbarMode;
  ScrollbarMode vScroll = m_verticalScrollbarMode;

  if (hScroll != ScrollbarAuto)
    newHasHorizontalScrollbar = (hScroll == ScrollbarAlwaysOn);
  if (vScroll != ScrollbarAuto)
    newHasVerticalScrollbar = (vScroll == ScrollbarAlwaysOn);

  if (m_scrollbarsSuppressed ||
      (hScroll != ScrollbarAuto && vScroll != ScrollbarAuto))
    return;

  if (hScroll == ScrollbarAuto)
    newHasHorizontalScrollbar = docSize.width() > visibleWidth();
  if (vScroll == ScrollbarAuto)
    newHasVerticalScrollbar = docSize.height() > visibleHeight();

  if (hasOverlayScrollbars())
    return;

  IntSize fullVisibleSize = visibleContentRect(IncludeScrollbars).size();

  bool attemptToRemoveScrollbars =
      (option == FirstPass && docSize.width() <= fullVisibleSize.width() &&
       docSize.height() <= fullVisibleSize.height());
  if (attemptToRemoveScrollbars) {
    if (hScroll == ScrollbarAuto)
      newHasHorizontalScrollbar = false;
    if (vScroll == ScrollbarAuto)
      newHasVerticalScrollbar = false;
  }
}

void FrameView::updateScrollbarEnabledState() {
  bool forceDisabled =
      ScrollbarTheme::theme().shouldDisableInvisibleScrollbars() &&
      scrollbarsHidden();

  if (horizontalScrollbar()) {
    horizontalScrollbar()->setEnabled(contentsWidth() > visibleWidth() &&
                                      !forceDisabled);
  }
  if (verticalScrollbar()) {
    verticalScrollbar()->setEnabled(contentsHeight() > visibleHeight() &&
                                    !forceDisabled);
  }
}

void FrameView::updateScrollbarGeometry() {
  updateScrollbarEnabledState();
  if (horizontalScrollbar()) {
    int thickness = horizontalScrollbar()->scrollbarThickness();
    IntRect oldRect(horizontalScrollbar()->frameRect());
    IntRect hBarRect(
        (shouldPlaceVerticalScrollbarOnLeft() && verticalScrollbar())
            ? verticalScrollbar()->width()
            : 0,
        height() - thickness,
        width() - (verticalScrollbar() ? verticalScrollbar()->width() : 0),
        thickness);
    horizontalScrollbar()->setFrameRect(hBarRect);
    if (oldRect != horizontalScrollbar()->frameRect())
      setScrollbarNeedsPaintInvalidation(HorizontalScrollbar);

    horizontalScrollbar()->setProportion(visibleWidth(), contentsWidth());
    horizontalScrollbar()->offsetDidChange();
  }

  if (verticalScrollbar()) {
    int thickness = verticalScrollbar()->scrollbarThickness();
    IntRect oldRect(verticalScrollbar()->frameRect());
    IntRect vBarRect(
        shouldPlaceVerticalScrollbarOnLeft() ? 0 : (width() - thickness), 0,
        thickness,
        height() -
            (horizontalScrollbar() ? horizontalScrollbar()->height() : 0));
    verticalScrollbar()->setFrameRect(vBarRect);
    if (oldRect != verticalScrollbar()->frameRect())
      setScrollbarNeedsPaintInvalidation(VerticalScrollbar);

    verticalScrollbar()->setProportion(visibleHeight(), contentsHeight());
    verticalScrollbar()->offsetDidChange();
  }
}

bool FrameView::adjustScrollbarExistence(
    ComputeScrollbarExistenceOption option) {
  ASSERT(m_inUpdateScrollbars);

  // If we came in here with the view already needing a layout, then go ahead
  // and do that first.  (This will be the common case, e.g., when the page
  // changes due to window resizing for example).  This layout will not re-enter
  // updateScrollbars and does not count towards our max layout pass total.
  if (!m_scrollbarsSuppressed)
    scrollbarExistenceDidChange();

  bool hasHorizontalScrollbar = horizontalScrollbar();
  bool hasVerticalScrollbar = verticalScrollbar();

  bool newHasHorizontalScrollbar = false;
  bool newHasVerticalScrollbar = false;
  computeScrollbarExistence(newHasHorizontalScrollbar, newHasVerticalScrollbar,
                            contentsSize(), option);

  bool scrollbarExistenceChanged =
      hasHorizontalScrollbar != newHasHorizontalScrollbar ||
      hasVerticalScrollbar != newHasVerticalScrollbar;
  if (!scrollbarExistenceChanged)
    return false;

  m_scrollbarManager.setHasHorizontalScrollbar(newHasHorizontalScrollbar);
  m_scrollbarManager.setHasVerticalScrollbar(newHasVerticalScrollbar);

  if (m_scrollbarsSuppressed)
    return true;

  if (!hasOverlayScrollbars())
    contentsResized();
  scrollbarExistenceDidChange();
  return true;
}

bool FrameView::needsScrollbarReconstruction() const {
  Scrollbar* scrollbar = horizontalScrollbar();
  if (!scrollbar)
    scrollbar = verticalScrollbar();
  if (!scrollbar) {
    // We have no scrollbar to reconstruct.
    return false;
  }
  Element* styleSource = nullptr;
  bool needsCustom = shouldUseCustomScrollbars(styleSource);
  bool isCustom = scrollbar->isCustomScrollbar();
  if (needsCustom != isCustom) {
    // We have a native scrollbar that should be custom, or vice versa.
    return true;
  }
  if (!needsCustom) {
    // We have a native scrollbar that should remain native.
    return false;
  }
  DCHECK(needsCustom && isCustom);
  DCHECK(styleSource);
  if (toLayoutScrollbar(scrollbar)->styleSource() !=
      styleSource->layoutObject()) {
    // We have a custom scrollbar with a stale m_owner.
    return true;
  }
  return false;
}

bool FrameView::shouldIgnoreOverflowHidden() const {
  return m_frame->settings()->getIgnoreMainFrameOverflowHiddenQuirk() &&
         m_frame->isMainFrame();
}

void FrameView::updateScrollbarsIfNeeded() {
  if (m_needsScrollbarsUpdate || needsScrollbarReconstruction() ||
      scrollOriginChanged())
    updateScrollbars();
}

void FrameView::updateScrollbars() {
  m_needsScrollbarsUpdate = false;

  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled())
    return;

  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
    setNeedsPaintPropertyUpdate();

  // Avoid drawing two sets of scrollbars when visual viewport is enabled.
  if (visualViewportSuppliesScrollbars()) {
    m_scrollbarManager.setHasHorizontalScrollbar(false);
    m_scrollbarManager.setHasVerticalScrollbar(false);
    adjustScrollOffsetFromUpdateScrollbars();
    return;
  }

  if (m_inUpdateScrollbars)
    return;
  InUpdateScrollbarsScope inUpdateScrollbarsScope(this);

  bool scrollbarExistenceChanged = false;

  if (needsScrollbarReconstruction()) {
    m_scrollbarManager.setHasHorizontalScrollbar(false);
    m_scrollbarManager.setHasVerticalScrollbar(false);
    scrollbarExistenceChanged = true;
  }

  int maxUpdateScrollbarsPass =
      hasOverlayScrollbars() || m_scrollbarsSuppressed ? 1 : 3;
  for (int updateScrollbarsPass = 0;
       updateScrollbarsPass < maxUpdateScrollbarsPass; updateScrollbarsPass++) {
    if (!adjustScrollbarExistence(updateScrollbarsPass ? Incremental
                                                       : FirstPass))
      break;
    scrollbarExistenceChanged = true;
  }

  updateScrollbarGeometry();

  if (scrollbarExistenceChanged) {
    // FIXME: Is frameRectsChanged really necessary here? Have any frame rects
    // changed?
    frameRectsChanged();
    positionScrollbarLayers();
    updateScrollCorner();
  }

  adjustScrollOffsetFromUpdateScrollbars();
}

void FrameView::adjustScrollOffsetFromUpdateScrollbars() {
  ScrollOffset clamped = clampScrollOffset(getScrollOffset());
  if (clamped != getScrollOffset() || scrollOriginChanged()) {
    ScrollableArea::setScrollOffset(clamped, ClampingScroll);
    resetScrollOriginChanged();
  }
}

void FrameView::scrollContentsIfNeeded() {
  if (m_pendingScrollDelta.isZero())
    return;
  ScrollOffset scrollDelta = m_pendingScrollDelta;
  m_pendingScrollDelta = ScrollOffset();
  // FIXME: Change scrollContents() to take DoubleSize. crbug.com/414283.
  scrollContents(flooredIntSize(scrollDelta));
}

void FrameView::scrollContents(const IntSize& scrollDelta) {
  HostWindow* window = getHostWindow();
  if (!window)
    return;

  TRACE_EVENT0("blink", "FrameView::scrollContents");

  if (!scrollContentsFastPath(-scrollDelta))
    scrollContentsSlowPath();

  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled() &&
      !RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
    // Need to update scroll translation property.
    setNeedsPaintPropertyUpdate();
  }

  // This call will move children with native FrameViewBases (plugins) and
  // invalidate them as well.
  frameRectsChanged();
}

IntPoint FrameView::contentsToFrame(const IntPoint& pointInContentSpace) const {
  return pointInContentSpace - scrollOffsetInt();
}

IntRect FrameView::contentsToFrame(const IntRect& rectInContentSpace) const {
  return IntRect(contentsToFrame(rectInContentSpace.location()),
                 rectInContentSpace.size());
}

FloatPoint FrameView::frameToContents(const FloatPoint& pointInFrame) const {
  return pointInFrame + getScrollOffset();
}

IntPoint FrameView::frameToContents(const IntPoint& pointInFrame) const {
  return pointInFrame + scrollOffsetInt();
}

IntRect FrameView::frameToContents(const IntRect& rectInFrame) const {
  return IntRect(frameToContents(rectInFrame.location()), rectInFrame.size());
}

IntPoint FrameView::rootFrameToContents(const IntPoint& rootFramePoint) const {
  IntPoint framePoint = convertFromRootFrame(rootFramePoint);
  return frameToContents(framePoint);
}

IntRect FrameView::rootFrameToContents(const IntRect& rootFrameRect) const {
  return IntRect(rootFrameToContents(rootFrameRect.location()),
                 rootFrameRect.size());
}

IntPoint FrameView::contentsToRootFrame(const IntPoint& contentsPoint) const {
  IntPoint framePoint = contentsToFrame(contentsPoint);
  return convertToRootFrame(framePoint);
}

IntRect FrameView::contentsToRootFrame(const IntRect& contentsRect) const {
  IntRect rectInFrame = contentsToFrame(contentsRect);
  return convertToRootFrame(rectInFrame);
}

FloatPoint FrameView::rootFrameToContents(
    const FloatPoint& pointInRootFrame) const {
  FloatPoint framePoint = convertFromRootFrame(pointInRootFrame);
  return frameToContents(framePoint);
}

IntRect FrameView::viewportToContents(const IntRect& rectInViewport) const {
  IntRect rectInRootFrame =
      m_frame->host()->visualViewport().viewportToRootFrame(rectInViewport);
  IntRect frameRect = convertFromRootFrame(rectInRootFrame);
  return frameToContents(frameRect);
}

IntPoint FrameView::viewportToContents(const IntPoint& pointInViewport) const {
  IntPoint pointInRootFrame =
      m_frame->host()->visualViewport().viewportToRootFrame(pointInViewport);
  IntPoint pointInFrame = convertFromRootFrame(pointInRootFrame);
  return frameToContents(pointInFrame);
}

IntRect FrameView::contentsToViewport(const IntRect& rectInContents) const {
  IntRect rectInFrame = contentsToFrame(rectInContents);
  IntRect rectInRootFrame = convertToRootFrame(rectInFrame);
  return m_frame->host()->visualViewport().rootFrameToViewport(rectInRootFrame);
}

IntPoint FrameView::contentsToViewport(const IntPoint& pointInContents) const {
  IntPoint pointInFrame = contentsToFrame(pointInContents);
  IntPoint pointInRootFrame = convertToRootFrame(pointInFrame);
  return m_frame->host()->visualViewport().rootFrameToViewport(
      pointInRootFrame);
}

IntRect FrameView::contentsToScreen(const IntRect& rect) const {
  HostWindow* window = getHostWindow();
  if (!window)
    return IntRect();
  return window->viewportToScreen(contentsToViewport(rect), this);
}

IntPoint FrameView::soonToBeRemovedUnscaledViewportToContents(
    const IntPoint& pointInViewport) const {
  IntPoint pointInRootFrame = flooredIntPoint(
      m_frame->host()->visualViewport().viewportCSSPixelsToRootFrame(
          pointInViewport));
  IntPoint pointInThisFrame = convertFromRootFrame(pointInRootFrame);
  return frameToContents(pointInThisFrame);
}

Scrollbar* FrameView::scrollbarAtFramePoint(const IntPoint& pointInFrame) {
  if (horizontalScrollbar() &&
      horizontalScrollbar()->shouldParticipateInHitTesting() &&
      horizontalScrollbar()->frameRect().contains(pointInFrame))
    return horizontalScrollbar();
  if (verticalScrollbar() &&
      verticalScrollbar()->shouldParticipateInHitTesting() &&
      verticalScrollbar()->frameRect().contains(pointInFrame))
    return verticalScrollbar();
  return nullptr;
}

static void positionScrollbarLayer(GraphicsLayer* graphicsLayer,
                                   Scrollbar* scrollbar) {
  if (!graphicsLayer || !scrollbar)
    return;

  IntRect scrollbarRect = scrollbar->frameRect();
  graphicsLayer->setPosition(scrollbarRect.location());

  if (scrollbarRect.size() == graphicsLayer->size())
    return;

  graphicsLayer->setSize(FloatSize(scrollbarRect.size()));

  if (graphicsLayer->hasContentsLayer()) {
    graphicsLayer->setContentsRect(
        IntRect(0, 0, scrollbarRect.width(), scrollbarRect.height()));
    return;
  }

  graphicsLayer->setDrawsContent(true);
  graphicsLayer->setNeedsDisplay();
}

static void positionScrollCornerLayer(GraphicsLayer* graphicsLayer,
                                      const IntRect& cornerRect) {
  if (!graphicsLayer)
    return;
  graphicsLayer->setDrawsContent(!cornerRect.isEmpty());
  graphicsLayer->setPosition(cornerRect.location());
  if (cornerRect.size() != graphicsLayer->size())
    graphicsLayer->setNeedsDisplay();
  graphicsLayer->setSize(FloatSize(cornerRect.size()));
}

void FrameView::positionScrollbarLayers() {
  positionScrollbarLayer(layerForHorizontalScrollbar(), horizontalScrollbar());
  positionScrollbarLayer(layerForVerticalScrollbar(), verticalScrollbar());
  positionScrollCornerLayer(layerForScrollCorner(), scrollCornerRect());
}

bool FrameView::userInputScrollable(ScrollbarOrientation orientation) const {
  Document* document = frame().document();
  Element* fullscreenElement = Fullscreen::fullscreenElementFrom(*document);
  if (fullscreenElement && fullscreenElement != document->documentElement())
    return false;

  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled())
    return false;

  ScrollbarMode mode = (orientation == HorizontalScrollbar)
                           ? m_horizontalScrollbarMode
                           : m_verticalScrollbarMode;

  return mode == ScrollbarAuto || mode == ScrollbarAlwaysOn;
}

bool FrameView::shouldPlaceVerticalScrollbarOnLeft() const {
  return false;
}

FrameViewBase* FrameView::getWidget() {
  return this;
}

LayoutRect FrameView::scrollIntoView(const LayoutRect& rectInContent,
                                     const ScrollAlignment& alignX,
                                     const ScrollAlignment& alignY,
                                     ScrollType scrollType) {
  LayoutRect viewRect(visibleContentRect());
  LayoutRect exposeRect =
      ScrollAlignment::getRectToExpose(viewRect, rectInContent, alignX, alignY);
  if (exposeRect != viewRect) {
    setScrollOffset(
        ScrollOffset(exposeRect.x().toFloat(), exposeRect.y().toFloat()),
        scrollType);
  }

  // Scrolling the FrameView cannot change the input rect's location relative to
  // the document.
  return rectInContent;
}

IntRect FrameView::scrollCornerRect() const {
  IntRect cornerRect;

  if (hasOverlayScrollbars())
    return cornerRect;

  if (horizontalScrollbar() && width() - horizontalScrollbar()->width() > 0) {
    cornerRect.unite(IntRect(shouldPlaceVerticalScrollbarOnLeft()
                                 ? 0
                                 : horizontalScrollbar()->width(),
                             height() - horizontalScrollbar()->height(),
                             width() - horizontalScrollbar()->width(),
                             horizontalScrollbar()->height()));
  }

  if (verticalScrollbar() && height() - verticalScrollbar()->height() > 0) {
    cornerRect.unite(IntRect(shouldPlaceVerticalScrollbarOnLeft()
                                 ? 0
                                 : (width() - verticalScrollbar()->width()),
                             verticalScrollbar()->height(),
                             verticalScrollbar()->width(),
                             height() - verticalScrollbar()->height()));
  }

  return cornerRect;
}

bool FrameView::isScrollCornerVisible() const {
  return !scrollCornerRect().isEmpty();
}

ScrollBehavior FrameView::scrollBehaviorStyle() const {
  Element* scrollElement = m_frame->document()->scrollingElement();
  LayoutObject* layoutObject =
      scrollElement ? scrollElement->layoutObject() : nullptr;
  if (layoutObject &&
      layoutObject->style()->getScrollBehavior() == ScrollBehaviorSmooth)
    return ScrollBehaviorSmooth;

  return ScrollBehaviorInstant;
}

void FrameView::paint(GraphicsContext& context,
                      const CullRect& cullRect) const {
  paint(context, GlobalPaintNormalPhase, cullRect);
}

void FrameView::paint(GraphicsContext& context,
                      const GlobalPaintFlags globalPaintFlags,
                      const CullRect& cullRect) const {
  FramePainter(*this).paint(context, globalPaintFlags, cullRect);
}

void FrameView::paintContents(GraphicsContext& context,
                              const GlobalPaintFlags globalPaintFlags,
                              const IntRect& damageRect) const {
  FramePainter(*this).paintContents(context, globalPaintFlags, damageRect);
}

bool FrameView::isPointInScrollbarCorner(const IntPoint& pointInRootFrame) {
  if (!scrollbarCornerPresent())
    return false;

  IntPoint framePoint = convertFromRootFrame(pointInRootFrame);

  if (horizontalScrollbar()) {
    int horizontalScrollbarYMin = horizontalScrollbar()->frameRect().y();
    int horizontalScrollbarYMax = horizontalScrollbar()->frameRect().y() +
                                  horizontalScrollbar()->frameRect().height();
    int horizontalScrollbarXMin = horizontalScrollbar()->frameRect().x() +
                                  horizontalScrollbar()->frameRect().width();

    return framePoint.y() > horizontalScrollbarYMin &&
           framePoint.y() < horizontalScrollbarYMax &&
           framePoint.x() > horizontalScrollbarXMin;
  }

  int verticalScrollbarXMin = verticalScrollbar()->frameRect().x();
  int verticalScrollbarXMax = verticalScrollbar()->frameRect().x() +
                              verticalScrollbar()->frameRect().width();
  int verticalScrollbarYMin = verticalScrollbar()->frameRect().y() +
                              verticalScrollbar()->frameRect().height();

  return framePoint.x() > verticalScrollbarXMin &&
         framePoint.x() < verticalScrollbarXMax &&
         framePoint.y() > verticalScrollbarYMin;
}

bool FrameView::scrollbarCornerPresent() const {
  return (horizontalScrollbar() &&
          width() - horizontalScrollbar()->width() > 0) ||
         (verticalScrollbar() && height() - verticalScrollbar()->height() > 0);
}

IntRect FrameView::convertFromScrollbarToContainingWidget(
    const Scrollbar& scrollbar,
    const IntRect& localRect) const {
  // Scrollbars won't be transformed within us
  IntRect newRect = localRect;
  newRect.moveBy(scrollbar.location());
  return newRect;
}

IntRect FrameView::convertFromContainingWidgetToScrollbar(
    const Scrollbar& scrollbar,
    const IntRect& parentRect) const {
  IntRect newRect = parentRect;
  // Scrollbars won't be transformed within us
  newRect.moveBy(-scrollbar.location());
  return newRect;
}

// FIXME: test these on windows
IntPoint FrameView::convertFromScrollbarToContainingWidget(
    const Scrollbar& scrollbar,
    const IntPoint& localPoint) const {
  // Scrollbars won't be transformed within us
  IntPoint newPoint = localPoint;
  newPoint.moveBy(scrollbar.location());
  return newPoint;
}

IntPoint FrameView::convertFromContainingWidgetToScrollbar(
    const Scrollbar& scrollbar,
    const IntPoint& parentPoint) const {
  IntPoint newPoint = parentPoint;
  // Scrollbars won't be transformed within us
  newPoint.moveBy(-scrollbar.location());
  return newPoint;
}

static void setNeedsCompositingUpdate(LayoutViewItem layoutViewItem,
                                      CompositingUpdateType updateType) {
  if (PaintLayerCompositor* compositor =
          !layoutViewItem.isNull() ? layoutViewItem.compositor() : nullptr)
    compositor->setNeedsCompositingUpdate(updateType);
}

void FrameView::setParentVisible(bool visible) {
  if (isParentVisible() == visible)
    return;

  // As parent visibility changes, we may need to recomposite this frame view
  // and potentially child frame views.
  setNeedsCompositingUpdate(layoutViewItem(), CompositingUpdateRebuildTree);

  FrameViewBase::setParentVisible(visible);

  if (!isSelfVisible())
    return;

  for (const auto& child : m_children)
    child->setParentVisible(visible);
}

void FrameView::show() {
  if (!isSelfVisible()) {
    setSelfVisible(true);
    if (ScrollingCoordinator* scrollingCoordinator =
            this->scrollingCoordinator())
      scrollingCoordinator->frameViewVisibilityDidChange();
    setNeedsCompositingUpdate(layoutViewItem(), CompositingUpdateRebuildTree);
    updateParentScrollableAreaSet();
    if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled() &&
        !RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
      // The existance of scrolling properties depends on visibility through
      // isScrollable() so ensure properties are updated if visibility changes.
      setNeedsPaintPropertyUpdate();
    }
    if (isParentVisible()) {
      for (const auto& child : m_children)
        child->setParentVisible(true);
    }
  }

  FrameViewBase::show();
}

void FrameView::hide() {
  if (isSelfVisible()) {
    if (isParentVisible()) {
      for (const auto& child : m_children)
        child->setParentVisible(false);
    }
    setSelfVisible(false);
    if (ScrollingCoordinator* scrollingCoordinator =
            this->scrollingCoordinator())
      scrollingCoordinator->frameViewVisibilityDidChange();
    setNeedsCompositingUpdate(layoutViewItem(), CompositingUpdateRebuildTree);
    updateParentScrollableAreaSet();
    if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled() &&
        !RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
      // The existance of scrolling properties depends on visibility through
      // isScrollable() so ensure properties are updated if visibility changes.
      setNeedsPaintPropertyUpdate();
    }
  }

  FrameViewBase::hide();
}

int FrameView::viewportWidth() const {
  int viewportWidth = layoutSize(IncludeScrollbars).width();
  return adjustForAbsoluteZoom(viewportWidth, layoutView());
}

ScrollableArea* FrameView::getScrollableArea() {
  if (m_viewportScrollableArea)
    return m_viewportScrollableArea.get();

  return layoutViewportScrollableArea();
}

ScrollableArea* FrameView::layoutViewportScrollableArea() {
  if (!RuntimeEnabledFeatures::rootLayerScrollingEnabled())
    return this;

  LayoutViewItem layoutViewItem = this->layoutViewItem();
  return layoutViewItem.isNull() ? nullptr : layoutViewItem.getScrollableArea();
}

RootFrameViewport* FrameView::getRootFrameViewport() {
  return m_viewportScrollableArea.get();
}

LayoutObject* FrameView::viewportLayoutObject() const {
  if (Document* document = frame().document()) {
    if (Element* element = document->viewportDefiningElement())
      return element->layoutObject();
  }
  return nullptr;
}

void FrameView::collectAnnotatedRegions(
    LayoutObject& layoutObject,
    Vector<AnnotatedRegionValue>& regions) const {
  // LayoutTexts don't have their own style, they just use their parent's style,
  // so we don't want to include them.
  if (layoutObject.isText())
    return;

  layoutObject.addAnnotatedRegions(regions);
  for (LayoutObject* curr = layoutObject.slowFirstChild(); curr;
       curr = curr->nextSibling())
    collectAnnotatedRegions(*curr, regions);
}

void FrameView::updateViewportIntersectionsForSubtree(
    DocumentLifecycle::LifecycleState targetState) {
  // TODO(dcheng): Since FrameViewBase tree updates are deferred, FrameViews
  // might still be in the FrameViewBase hierarchy even though the associated
  // Document is already detached. Investigate if this check and a similar check
  // in lifecycle updates are still needed when there are no more deferred
  // FrameViewBase updates: https://crbug.com/561683
  if (!frame().document()->isActive())
    return;

  if (targetState == DocumentLifecycle::PaintClean) {
    recordDeferredLoadingStats();
    // Notify javascript IntersectionObservers
    if (frame().document()->intersectionObserverController()) {
      frame()
          .document()
          ->intersectionObserverController()
          ->computeTrackedIntersectionObservations();
    }
  }

  // Don't throttle display:none frames (see updateRenderThrottlingStatus).
  HTMLFrameOwnerElement* ownerElement = m_frame->deprecatedLocalOwner();
  if (m_hiddenForThrottling && ownerElement && !ownerElement->layoutObject()) {
    // No need to notify children because descendants of display:none frames
    // should remain throttled.
    updateRenderThrottlingStatus(m_hiddenForThrottling, m_subtreeThrottled,
                                 DontForceThrottlingInvalidation,
                                 DontNotifyChildren);
    DCHECK(!canThrottleRendering());
  }

  for (Frame* child = m_frame->tree().firstChild(); child;
       child = child->tree().nextSibling()) {
    if (!child->isLocalFrame())
      continue;
    if (FrameView* view = toLocalFrame(child)->view())
      view->updateViewportIntersectionsForSubtree(targetState);
  }
}

void FrameView::updateRenderThrottlingStatusForTesting() {
  m_visibilityObserver->deliverObservationsForTesting();
}

void FrameView::crossOriginStatusChanged() {
  // Cross-domain status is not stored as a dirty bit within FrameView,
  // so force-invalidate throttling status when it changes regardless of
  // previous or new value.
  updateRenderThrottlingStatus(m_hiddenForThrottling, m_subtreeThrottled,
                               ForceThrottlingInvalidation);
}

void FrameView::updateRenderThrottlingStatus(
    bool hidden,
    bool subtreeThrottled,
    ForceThrottlingInvalidationBehavior forceThrottlingInvalidationBehavior,
    NotifyChildrenBehavior notifyChildrenBehavior) {
  TRACE_EVENT0("blink", "FrameView::updateRenderThrottlingStatus");
  DCHECK(!isInPerformLayout());
  DCHECK(!m_frame->document() || !m_frame->document()->inStyleRecalc());
  bool wasThrottled = canThrottleRendering();

  // Note that we disallow throttling of 0x0 and display:none frames because
  // some sites use them to drive UI logic.
  HTMLFrameOwnerElement* ownerElement = m_frame->deprecatedLocalOwner();
  m_hiddenForThrottling = hidden && !frameRect().isEmpty() &&
                          (ownerElement && ownerElement->layoutObject());
  m_subtreeThrottled = subtreeThrottled;

  bool isThrottled = canThrottleRendering();
  bool becameUnthrottled = wasThrottled && !isThrottled;

  // If this FrameView became unthrottled or throttled, we must make sure all
  // its children are notified synchronously. Otherwise we 1) might attempt to
  // paint one of the children with an out-of-date layout before
  // |updateRenderThrottlingStatus| has made it throttled or 2) fail to
  // unthrottle a child whose parent is unthrottled by a later notification.
  if (notifyChildrenBehavior == NotifyChildren &&
      (wasThrottled != isThrottled ||
       forceThrottlingInvalidationBehavior == ForceThrottlingInvalidation)) {
    for (const Member<FrameViewBase>& child : *children()) {
      if (child->isFrameView()) {
        FrameView* childView = toFrameView(child);
        childView->updateRenderThrottlingStatus(
            childView->m_hiddenForThrottling, isThrottled);
      }
    }
  }

  ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator();
  if (becameUnthrottled ||
      forceThrottlingInvalidationBehavior == ForceThrottlingInvalidation) {
    // ScrollingCoordinator needs to update according to the new throttling
    // status.
    if (scrollingCoordinator)
      scrollingCoordinator->notifyGeometryChanged();
    // Start ticking animation frames again if necessary.
    if (page())
      page()->animator().scheduleVisualUpdate(m_frame.get());
    // Force a full repaint of this frame to ensure we are not left with a
    // partially painted version of this frame's contents if we skipped
    // painting them while the frame was throttled.
    LayoutViewItem layoutViewItem = this->layoutViewItem();
    if (!layoutViewItem.isNull())
      layoutViewItem.invalidatePaintForViewAndCompositedLayers();
    // Also need to update all paint properties that might be skipped while
    // the frame was throttled.
    setSubtreeNeedsPaintPropertyUpdate();
  }

  bool hasHandlers = m_frame->host() &&
                     m_frame->host()->eventHandlerRegistry().hasEventHandlers(
                         EventHandlerRegistry::TouchStartOrMoveEventBlocking);
  if (wasThrottled != canThrottleRendering() && scrollingCoordinator &&
      hasHandlers)
    scrollingCoordinator->touchEventTargetRectsDidChange();

  if (m_frame->frameScheduler()) {
    m_frame->frameScheduler()->setFrameVisible(!m_hiddenForThrottling);
    m_frame->frameScheduler()->setCrossOrigin(m_frame->isCrossOriginSubframe());
  }

#if DCHECK_IS_ON()
  // Make sure we never have an unthrottled frame inside a throttled one.
  FrameView* parent = parentFrameView();
  while (parent) {
    DCHECK(canThrottleRendering() || !parent->canThrottleRendering());
    parent = parent->parentFrameView();
  }
#endif
}

void FrameView::recordDeferredLoadingStats() {
  if (!frame().document()->frame() || !frame().isCrossOriginSubframe())
    return;

  FrameView* parent = parentFrameView();
  if (!parent) {
    HTMLFrameOwnerElement* element = frame().deprecatedLocalOwner();
    // We would fall into an else block on some teardowns and other weird cases.
    if (!element || !element->layoutObject())
      frame().document()->recordDeferredLoadReason(WouldLoadNoParent);
    return;
  }
  // Small inaccuracy: frames with origins that match the top level might be
  // nested in a cross-origin frame. To keep code simpler, count such frames as
  // WouldLoadVisible, even when their parent is offscreen.
  WouldLoadReason whyParentLoaded = WouldLoadVisible;
  if (parent->parentFrameView() && parent->frame().isCrossOriginSubframe())
    whyParentLoaded = parent->frame().document()->deferredLoadReason();

  // If the parent wasn't loaded, the children won't be either.
  if (whyParentLoaded == Created)
    return;
  // These frames are never meant to be seen so we will need to load them.
  if (frameRect().isEmpty() || frameRect().maxY() < 0 ||
      frameRect().maxX() < 0) {
    frame().document()->recordDeferredLoadReason(whyParentLoaded);
    return;
  }

  IntRect parentRect = parent->frameRect();
  // First clause: for this rough data collection we assume the user never
  // scrolls right.
  if (frameRect().x() >= parentRect.width() || parentRect.height() <= 0)
    return;

  int thisFrameScreensAway = 0;
  // If an frame is created above the current scoll position, this logic counts
  // it as visible.
  if (frameRect().y() > parent->getScrollOffset().height()) {
    thisFrameScreensAway =
        (frameRect().y() - parent->getScrollOffset().height()) /
        parentRect.height();
  }
  DCHECK_GE(thisFrameScreensAway, 0);

  int parentScreensAway = 0;
  if (whyParentLoaded <= WouldLoadVisible)
    parentScreensAway = WouldLoadVisible - whyParentLoaded;

  int totalScreensAway = thisFrameScreensAway + parentScreensAway;

  // We're collecting data for frames that are at most 3 screens away.
  if (totalScreensAway > 3)
    return;

  frame().document()->recordDeferredLoadReason(
      static_cast<WouldLoadReason>(WouldLoadVisible - totalScreensAway));
}

bool FrameView::shouldThrottleRendering() const {
  return canThrottleRendering() && m_frame->document() &&
         lifecycle().throttlingAllowed();
}

bool FrameView::canThrottleRendering() const {
  if (m_lifecycleUpdatesThrottled)
    return true;
  if (!RuntimeEnabledFeatures::renderingPipelineThrottlingEnabled())
    return false;
  if (m_subtreeThrottled)
    return true;
  // We only throttle hidden cross-origin frames. This is to avoid a situation
  // where an ancestor frame directly depends on the pipeline timing of a
  // descendant and breaks as a result of throttling. The rationale is that
  // cross-origin frames must already communicate with asynchronous messages,
  // so they should be able to tolerate some delay in receiving replies from a
  // throttled peer.
  return m_hiddenForThrottling && m_frame->isCrossOriginSubframe();
}

void FrameView::beginLifecycleUpdates() {
  // Avoid pumping frames for the initially empty document.
  if (!frame().loader().stateMachine()->committedFirstRealDocumentLoad())
    return;
  m_lifecycleUpdatesThrottled = false;
  setupRenderThrottling();
  updateRenderThrottlingStatus(m_hiddenForThrottling, m_subtreeThrottled);
  // The compositor will "defer commits" for the main frame until we
  // explicitly request them.
  if (frame().isMainFrame())
    frame().page()->chromeClient().beginLifecycleUpdates();
}

void FrameView::setInitialViewportSize(const IntSize& viewportSize) {
  if (viewportSize == m_initialViewportSize)
    return;

  m_initialViewportSize = viewportSize;
  if (Document* document = m_frame->document())
    document->styleEngine().initialViewportChanged();
}

int FrameView::initialViewportWidth() const {
  DCHECK(m_frame->isMainFrame());
  return m_initialViewportSize.width();
}

int FrameView::initialViewportHeight() const {
  DCHECK(m_frame->isMainFrame());
  return m_initialViewportSize.height();
}

bool FrameView::hasVisibleSlowRepaintViewportConstrainedObjects() const {
  if (!viewportConstrainedObjects())
    return false;

  for (const LayoutObject* layoutObject : *viewportConstrainedObjects()) {
    DCHECK(layoutObject->isBoxModelObject() && layoutObject->hasLayer());
    DCHECK(layoutObject->style()->position() == EPosition::kFixed ||
           layoutObject->style()->position() == EPosition::kSticky);
    PaintLayer* layer = toLayoutBoxModelObject(layoutObject)->layer();

    // Whether the Layer sticks to the viewport is a tree-depenent
    // property and our viewportConstrainedObjects collection is maintained
    // with only LayoutObject-level information.
    if (!layer->sticksToViewport())
      continue;

    // If the whole subtree is invisible, there's no reason to scroll on
    // the main thread because we don't need to generate invalidations
    // for invisible content.
    if (layer->subtreeIsInvisible())
      continue;

    // We're only smart enough to scroll viewport-constrainted objects
    // in the compositor if they have their own backing or they paint
    // into a grouped back (which necessarily all have the same viewport
    // constraints).
    CompositingState compositingState = layer->compositingState();
    if (compositingState != PaintsIntoOwnBacking &&
        compositingState != PaintsIntoGroupedBacking)
      return true;
  }
  return false;
}

void FrameView::updateSubFrameScrollOnMainReason(
    const Frame& frame,
    MainThreadScrollingReasons parentReason) {
  MainThreadScrollingReasons reasons = parentReason;

  if (!page()->settings().getThreadedScrollingEnabled())
    reasons |= MainThreadScrollingReason::kThreadedScrollingDisabled;

  if (!frame.isLocalFrame())
    return;

  FrameView& frameView = *toLocalFrame(frame).view();
  if (frameView.shouldThrottleRendering())
    return;
  if (!frameView.layerForScrolling())
    return;

  reasons |= frameView.mainThreadScrollingReasonsPerFrame();
  if (GraphicsLayer* layerForScrolling = toLocalFrame(frame)
                                             .view()
                                             ->layoutViewportScrollableArea()
                                             ->layerForScrolling()) {
    if (WebLayer* platformLayerForScrolling =
            layerForScrolling->platformLayer()) {
      if (reasons) {
        platformLayerForScrolling->addMainThreadScrollingReasons(reasons);
      } else {
        // Clear all main thread scrolling reasons except the one that's set
        // if there is a running scroll animation.
        platformLayerForScrolling->clearMainThreadScrollingReasons(
            ~MainThreadScrollingReason::kHandlingScrollFromMainThread);
      }
    }
  }

  Frame* child = frame.tree().firstChild();
  while (child) {
    updateSubFrameScrollOnMainReason(*child, reasons);
    child = child->tree().nextSibling();
  }

  if (frame.isMainFrame())
    m_mainThreadScrollingReasons = reasons;
}

MainThreadScrollingReasons FrameView::mainThreadScrollingReasonsPerFrame()
    const {
  MainThreadScrollingReasons reasons =
      static_cast<MainThreadScrollingReasons>(0);

  if (shouldThrottleRendering())
    return reasons;

  if (hasBackgroundAttachmentFixedObjects())
    reasons |= MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects;

  ScrollingReasons scrollingReasons = getScrollingReasons();
  const bool mayBeScrolledByInput = (scrollingReasons == Scrollable);
  const bool mayBeScrolledByScript =
      mayBeScrolledByInput ||
      (scrollingReasons == NotScrollableExplicitlyDisabled);

  // TODO(awoloszyn) Currently crbug.com/304810 will let certain
  // overflow:hidden elements scroll on the compositor thread, so we should
  // not let this move there path as an optimization, when we have
  // slow-repaint elements.
  if (mayBeScrolledByScript &&
      hasVisibleSlowRepaintViewportConstrainedObjects()) {
    reasons |=
        MainThreadScrollingReason::kHasNonLayerViewportConstrainedObjects;
  }
  return reasons;
}

MainThreadScrollingReasons FrameView::mainThreadScrollingReasons() const {
  MainThreadScrollingReasons reasons =
      static_cast<MainThreadScrollingReasons>(0);

  if (!page()->settings().getThreadedScrollingEnabled())
    reasons |= MainThreadScrollingReason::kThreadedScrollingDisabled;

  if (!page()->mainFrame()->isLocalFrame())
    return reasons;

  // TODO(alexmos,kenrb): For OOPIF, local roots that are different from
  // the main frame can't be used in the calculation, since they use
  // different compositors with unrelated state, which breaks some of the
  // calculations below.
  if (m_frame->localFrameRoot() != page()->mainFrame())
    return reasons;

  // Walk the tree to the root. Use the gathered reasons to determine
  // whether the target frame should be scrolled on main thread regardless
  // other subframes on the same page.
  for (Frame* frame = m_frame; frame; frame = frame->tree().parent()) {
    if (!frame->isLocalFrame())
      continue;
    reasons |=
        toLocalFrame(frame)->view()->mainThreadScrollingReasonsPerFrame();
  }

  return reasons;
}

String FrameView::mainThreadScrollingReasonsAsText() const {
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    DCHECK(lifecycle().state() >= DocumentLifecycle::PrePaintClean);

    // Slimming paint v2 stores main thread scrolling reasons on property
    // trees instead of in |m_mainThreadScrollingReasons|.
    MainThreadScrollingReasons reasons = 0;
    if (const auto* scrollTranslation = this->scrollTranslation())
      reasons |= scrollTranslation->scrollNode()->mainThreadScrollingReasons();
    return String(
        MainThreadScrollingReason::mainThreadScrollingReasonsAsText(reasons)
            .c_str());
  }

  DCHECK(lifecycle().state() >= DocumentLifecycle::CompositingClean);
  if (layerForScrolling() && layerForScrolling()->platformLayer()) {
    String result(
        MainThreadScrollingReason::mainThreadScrollingReasonsAsText(
            layerForScrolling()->platformLayer()->mainThreadScrollingReasons())
            .c_str());
    return result;
  }

  String result(MainThreadScrollingReason::mainThreadScrollingReasonsAsText(
                    m_mainThreadScrollingReasons)
                    .c_str());
  return result;
}

void FrameView::adjustStyleRelatedMainThreadScrollingReasons(
    const uint32_t reason,
    bool increase) {
  int index = MainThreadScrollingReason::getReasonIndex(reason);
  DCHECK_GE(index, 0);
  m_mainThreadScrollingReasonsCounter[index] += increase ? 1 : -1;
  DCHECK_GE(m_mainThreadScrollingReasonsCounter[index], 0);
}

MainThreadScrollingReasons
FrameView::getStyleRelatedMainThreadScrollingReasons() const {
  MainThreadScrollingReasons reasons =
      static_cast<MainThreadScrollingReasons>(0);
  for (uint32_t reason = 0;
       reason < MainThreadScrollingReason::kMainThreadScrollingReasonCount;
       ++reason) {
    if (m_mainThreadScrollingReasonsCounter[reason] > 0) {
      reasons |= 1 << reason;
    }
  }
  return reasons;
}

void FrameView::setViewportIntersectionFromParent(
    const IntRect& viewportIntersection) {
  if (m_remoteViewportIntersection != viewportIntersection) {
    m_remoteViewportIntersection = viewportIntersection;
    scheduleAnimation();
  }
}

IntRect FrameView::remoteViewportIntersection() {
  IntRect intersection(m_remoteViewportIntersection);
  intersection.move(scrollOffsetInt());
  return intersection;
}

void FrameView::mapQuadToAncestorFrameIncludingScrollOffset(
    LayoutRect& rect,
    const LayoutObject* descendant,
    const LayoutView* ancestor,
    MapCoordinatesFlags mode) {
  FloatQuad mappedQuad = descendant->localToAncestorQuad(
      FloatQuad(FloatRect(rect)), ancestor, mode);
  rect = LayoutRect(mappedQuad.boundingBox());

  // localToAncestorQuad accounts for scroll offset if it encounters a remote
  // frame in the ancestor chain, otherwise it needs to be added explicitly.
  if (frame().localFrameRoot() == frame().tree().top() ||
      (ancestor &&
       ancestor->frame()->localFrameRoot() == frame().localFrameRoot())) {
    FrameView* ancestorView =
        (ancestor ? ancestor->frameView()
                  : toLocalFrame(frame().tree().top())->view());
    LayoutSize scrollPosition = LayoutSize(ancestorView->getScrollOffset());
    rect.move(-scrollPosition);
  }
}

bool FrameView::mapToVisualRectInTopFrameSpace(LayoutRect& rect) {
  // This is the top-level frame, so no mapping necessary.
  if (m_frame->isMainFrame())
    return true;

  LayoutRect viewportIntersectionRect(remoteViewportIntersection());
  rect.intersect(viewportIntersectionRect);
  if (rect.isEmpty())
    return false;
  return true;
}

void FrameView::applyTransformForTopFrameSpace(TransformState& transformState) {
  // This is the top-level frame, so no mapping necessary.
  if (m_frame->isMainFrame())
    return;

  LayoutRect viewportIntersectionRect(remoteViewportIntersection());
  transformState.move(
      LayoutSize(-viewportIntersectionRect.x(), -viewportIntersectionRect.y()));
}

void FrameView::setAnimationTimeline(
    std::unique_ptr<CompositorAnimationTimeline> timeline) {
  m_animationTimeline = std::move(timeline);
}

void FrameView::setAnimationHost(
    std::unique_ptr<CompositorAnimationHost> host) {
  m_animationHost = std::move(host);
}

LayoutUnit FrameView::caretWidth() const {
  return LayoutUnit(getHostWindow()->windowToViewportScalar(1));
}

}  // namespace blink
