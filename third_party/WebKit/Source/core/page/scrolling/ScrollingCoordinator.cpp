/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/page/scrolling/ScrollingCoordinator.h"

#include "core/dom/Document.h"
#include "core/dom/Fullscreen.h"
#include "core/dom/Node.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLElement.h"
#include "core/layout/LayoutGeometryMap.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/api/LayoutPartItem.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/plugins/PluginView.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/animation/CompositorAnimationHost.h"
#include "platform/animation/CompositorAnimationTimeline.h"
#include "platform/exported/WebScrollbarImpl.h"
#include "platform/exported/WebScrollbarThemeGeometryNative.h"
#include "platform/geometry/Region.h"
#include "platform/geometry/TransformState.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#if OS(MACOSX)
#include "platform/mac/ScrollAnimatorMac.h"
#endif
#include "platform/scroll/MainThreadScrollingReason.h"
#include "platform/scroll/ScrollAnimatorBase.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebLayerPositionConstraint.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebScrollbarLayer.h"
#include "public/platform/WebScrollbarThemeGeometry.h"
#include "public/platform/WebScrollbarThemePainter.h"
#include "wtf/text/StringBuilder.h"
#include <memory>
#include <utility>

using blink::WebLayer;
using blink::WebLayerPositionConstraint;
using blink::WebRect;
using blink::WebScrollbarLayer;
using blink::WebVector;

namespace {

WebLayer* toWebLayer(blink::GraphicsLayer* layer) {
  return layer ? layer->platformLayer() : nullptr;
}

}  // namespace

namespace blink {

ScrollingCoordinator* ScrollingCoordinator::create(Page* page) {
  return new ScrollingCoordinator(page);
}

ScrollingCoordinator::ScrollingCoordinator(Page* page)
    : m_page(page),
      m_scrollGestureRegionIsDirty(false),
      m_touchEventTargetRectsAreDirty(false),
      m_shouldScrollOnMainThreadDirty(false),
      m_wasFrameScrollable(false),
      m_lastMainThreadScrollingReasons(0) {}

ScrollingCoordinator::~ScrollingCoordinator() {
  DCHECK(!m_page);
}

DEFINE_TRACE(ScrollingCoordinator) {
  visitor->trace(m_page);
  visitor->trace(m_horizontalScrollbars);
  visitor->trace(m_verticalScrollbars);
}

void ScrollingCoordinator::setShouldHandleScrollGestureOnMainThreadRegion(
    const Region& region) {
  if (!m_page->mainFrame()->isLocalFrame() ||
      !m_page->deprecatedLocalMainFrame()->view())
    return;
  if (WebLayer* scrollLayer = toWebLayer(
          m_page->deprecatedLocalMainFrame()->view()->layerForScrolling())) {
    Vector<IntRect> rects = region.rects();
    WebVector<WebRect> webRects(rects.size());
    for (size_t i = 0; i < rects.size(); ++i)
      webRects[i] = rects[i];
    scrollLayer->setNonFastScrollableRegion(webRects);
  }
}

void ScrollingCoordinator::notifyGeometryChanged() {
  m_scrollGestureRegionIsDirty = true;
  m_touchEventTargetRectsAreDirty = true;
  m_shouldScrollOnMainThreadDirty = true;
}

void ScrollingCoordinator::notifyTransformChanged(const LayoutBox& box) {
  DCHECK(m_page);
  if (!m_page->mainFrame()->isLocalFrame() ||
      !m_page->deprecatedLocalMainFrame()->view())
    return;

  if (m_page->deprecatedLocalMainFrame()->view()->needsLayout())
    return;

  for (PaintLayer* layer = box.enclosingLayer(); layer;
       layer = layer->parent()) {
    if (m_layersWithTouchRects.contains(layer)) {
      m_touchEventTargetRectsAreDirty = true;
      return;
    }
  }
}
void ScrollingCoordinator::notifyOverflowUpdated() {
  m_scrollGestureRegionIsDirty = true;
}

void ScrollingCoordinator::frameViewVisibilityDidChange() {
  m_scrollGestureRegionIsDirty = true;
}

void ScrollingCoordinator::scrollableAreasDidChange() {
  DCHECK(m_page);
  if (!m_page->mainFrame()->isLocalFrame() ||
      !m_page->deprecatedLocalMainFrame()->view())
    return;

  // Layout may update scrollable area bounding boxes. It also sets the same
  // dirty flag making this one redundant (See
  // |ScrollingCoordinator::notifyGeometryChanged|).
  // So if layout is expected, ignore this call allowing scrolling coordinator
  // to be notified post-layout to recompute gesture regions.
  if (m_page->deprecatedLocalMainFrame()->view()->needsLayout())
    return;

  m_scrollGestureRegionIsDirty = true;
}

void ScrollingCoordinator::updateAfterCompositingChangeIfNeeded() {
  if (!m_page->mainFrame()->isLocalFrame())
    return;

  if (!shouldUpdateAfterCompositingChange())
    return;

  TRACE_EVENT0("input",
               "ScrollingCoordinator::updateAfterCompositingChangeIfNeeded");

  if (m_scrollGestureRegionIsDirty) {
    // Compute the region of the page where we can't handle scroll gestures and
    // mousewheel events
    // on the impl thread. This currently includes:
    // 1. All scrollable areas, such as subframes, overflow divs and list boxes,
    //    whose composited scrolling are not enabled. We need to do this even if
    //    the frame view whose layout was updated is not the main frame.
    // 2. Resize control areas, e.g. the small rect at the right bottom of
    //    div/textarea/iframe when CSS property "resize" is enabled.
    // 3. Plugin areas.
    Region shouldHandleScrollGestureOnMainThreadRegion =
        computeShouldHandleScrollGestureOnMainThreadRegion(
            m_page->deprecatedLocalMainFrame(), IntPoint());
    setShouldHandleScrollGestureOnMainThreadRegion(
        shouldHandleScrollGestureOnMainThreadRegion);
    m_scrollGestureRegionIsDirty = false;
  }

  if (m_touchEventTargetRectsAreDirty) {
    updateTouchEventTargetRectsIfNeeded();
    m_touchEventTargetRectsAreDirty = false;
  }

  FrameView* frameView = toLocalFrame(m_page->mainFrame())->view();
  bool frameIsScrollable = frameView && frameView->isScrollable();
  if (m_shouldScrollOnMainThreadDirty ||
      m_wasFrameScrollable != frameIsScrollable) {
    setShouldUpdateScrollLayerPositionOnMainThread(
        frameView->mainThreadScrollingReasons());

    // Need to update scroll on main thread reasons for subframe because
    // subframe (e.g. iframe with background-attachment:fixed) should
    // scroll on main thread while the main frame scrolls on impl.
    frameView->updateSubFrameScrollOnMainReason(*(m_page->mainFrame()), 0);
    m_shouldScrollOnMainThreadDirty = false;
  }
  m_wasFrameScrollable = frameIsScrollable;

  if (WebLayer* layoutViewportScrollLayer =
          frameView ? toWebLayer(frameView->layerForScrolling()) : nullptr) {
    layoutViewportScrollLayer->setBounds(frameView->contentsSize());

    // If there is a non-root fullscreen element, prevent the viewport from
    // scrolling.
    Document* mainFrameDocument =
        m_page->deprecatedLocalMainFrame()->document();
    Element* fullscreenElement =
        Fullscreen::fullscreenElementFrom(*mainFrameDocument);
    WebLayer* visualViewportScrollLayer =
        toWebLayer(m_page->frameHost().visualViewport().scrollLayer());

    if (visualViewportScrollLayer) {
      if (fullscreenElement &&
          fullscreenElement != mainFrameDocument->documentElement())
        visualViewportScrollLayer->setUserScrollable(false, false);
      else
        visualViewportScrollLayer->setUserScrollable(true, true);
    }

    layoutViewportScrollLayer->setUserScrollable(
        frameView->userInputScrollable(HorizontalScrollbar),
        frameView->userInputScrollable(VerticalScrollbar));
  }

  const FrameTree& tree = m_page->mainFrame()->tree();
  for (const Frame* child = tree.firstChild(); child;
       child = child->tree().nextSibling()) {
    if (!child->isLocalFrame())
      continue;
    FrameView* frameView = toLocalFrame(child)->view();
    if (!frameView || frameView->shouldThrottleRendering())
      continue;
    if (WebLayer* scrollLayer = toWebLayer(frameView->layerForScrolling()))
      scrollLayer->setBounds(frameView->contentsSize());
  }
}

void ScrollingCoordinator::setLayerIsContainerForFixedPositionLayers(
    GraphicsLayer* layer,
    bool enable) {
  if (WebLayer* scrollableLayer = toWebLayer(layer))
    scrollableLayer->setIsContainerForFixedPositionLayers(enable);
}

static void clearPositionConstraintExceptForLayer(GraphicsLayer* layer,
                                                  GraphicsLayer* except) {
  if (layer && layer != except && toWebLayer(layer))
    toWebLayer(layer)->setPositionConstraint(WebLayerPositionConstraint());
}

static WebLayerPositionConstraint computePositionConstraint(
    const PaintLayer* layer) {
  DCHECK(layer->hasCompositedLayerMapping());
  do {
    if (layer->layoutObject()->style()->position() == FixedPosition) {
      const LayoutObject* fixedPositionObject = layer->layoutObject();
      bool fixedToRight = !fixedPositionObject->style()->right().isAuto();
      bool fixedToBottom = !fixedPositionObject->style()->bottom().isAuto();
      return WebLayerPositionConstraint::fixedPosition(fixedToRight,
                                                       fixedToBottom);
    }

    layer = layer->parent();

    // Composited layers that inherit a fixed position state will be positioned
    // with respect to the nearest compositedLayerMapping's GraphicsLayer.
    // So, once we find a layer that has its own compositedLayerMapping, we can
    // stop searching for a fixed position LayoutObject.
  } while (layer && !layer->hasCompositedLayerMapping());
  return WebLayerPositionConstraint();
}

void ScrollingCoordinator::updateLayerPositionConstraint(PaintLayer* layer) {
  DCHECK(layer->hasCompositedLayerMapping());
  CompositedLayerMapping* compositedLayerMapping =
      layer->compositedLayerMapping();
  GraphicsLayer* mainLayer = compositedLayerMapping->childForSuperlayers();

  // Avoid unnecessary commits
  clearPositionConstraintExceptForLayer(
      compositedLayerMapping->squashingContainmentLayer(), mainLayer);
  clearPositionConstraintExceptForLayer(
      compositedLayerMapping->ancestorClippingLayer(), mainLayer);
  clearPositionConstraintExceptForLayer(
      compositedLayerMapping->mainGraphicsLayer(), mainLayer);

  if (WebLayer* scrollableLayer = toWebLayer(mainLayer))
    scrollableLayer->setPositionConstraint(computePositionConstraint(layer));
}

void ScrollingCoordinator::willDestroyScrollableArea(
    ScrollableArea* scrollableArea) {
  removeWebScrollbarLayer(scrollableArea, HorizontalScrollbar);
  removeWebScrollbarLayer(scrollableArea, VerticalScrollbar);
}

void ScrollingCoordinator::removeWebScrollbarLayer(
    ScrollableArea* scrollableArea,
    ScrollbarOrientation orientation) {
  ScrollbarMap& scrollbars = orientation == HorizontalScrollbar
                                 ? m_horizontalScrollbars
                                 : m_verticalScrollbars;
  if (std::unique_ptr<WebScrollbarLayer> scrollbarLayer =
          scrollbars.take(scrollableArea))
    GraphicsLayer::unregisterContentsLayer(scrollbarLayer->layer());
}

static std::unique_ptr<WebScrollbarLayer> createScrollbarLayer(
    Scrollbar& scrollbar,
    float deviceScaleFactor) {
  ScrollbarTheme& theme = scrollbar.theme();
  WebScrollbarThemePainter painter(theme, scrollbar, deviceScaleFactor);
  std::unique_ptr<WebScrollbarThemeGeometry> geometry(
      WebScrollbarThemeGeometryNative::create(theme));

  std::unique_ptr<WebScrollbarLayer> scrollbarLayer =
      Platform::current()->compositorSupport()->createScrollbarLayer(
          WebScrollbarImpl::create(&scrollbar), painter, std::move(geometry));
  GraphicsLayer::registerContentsLayer(scrollbarLayer->layer());
  return scrollbarLayer;
}

std::unique_ptr<WebScrollbarLayer>
ScrollingCoordinator::createSolidColorScrollbarLayer(
    ScrollbarOrientation orientation,
    int thumbThickness,
    int trackStart,
    bool isLeftSideVerticalScrollbar) {
  WebScrollbar::Orientation webOrientation =
      (orientation == HorizontalScrollbar) ? WebScrollbar::Horizontal
                                           : WebScrollbar::Vertical;
  std::unique_ptr<WebScrollbarLayer> scrollbarLayer =
      Platform::current()->compositorSupport()->createSolidColorScrollbarLayer(
          webOrientation, thumbThickness, trackStart,
          isLeftSideVerticalScrollbar);
  GraphicsLayer::registerContentsLayer(scrollbarLayer->layer());
  return scrollbarLayer;
}

static void detachScrollbarLayer(GraphicsLayer* scrollbarGraphicsLayer) {
  DCHECK(scrollbarGraphicsLayer);

  scrollbarGraphicsLayer->setContentsToPlatformLayer(nullptr);
  scrollbarGraphicsLayer->setDrawsContent(true);
}

static void setupScrollbarLayer(GraphicsLayer* scrollbarGraphicsLayer,
                                WebScrollbarLayer* scrollbarLayer,
                                WebLayer* scrollLayer) {
  DCHECK(scrollbarGraphicsLayer);
  DCHECK(scrollbarLayer);

  if (!scrollLayer) {
    detachScrollbarLayer(scrollbarGraphicsLayer);
    return;
  }
  scrollbarLayer->setScrollLayer(scrollLayer);
  scrollbarGraphicsLayer->setContentsToPlatformLayer(scrollbarLayer->layer());
  scrollbarGraphicsLayer->setDrawsContent(false);
}

WebScrollbarLayer* ScrollingCoordinator::addWebScrollbarLayer(
    ScrollableArea* scrollableArea,
    ScrollbarOrientation orientation,
    std::unique_ptr<WebScrollbarLayer> scrollbarLayer) {
  ScrollbarMap& scrollbars = orientation == HorizontalScrollbar
                                 ? m_horizontalScrollbars
                                 : m_verticalScrollbars;
  return scrollbars.add(scrollableArea, std::move(scrollbarLayer))
      .storedValue->value.get();
}

WebScrollbarLayer* ScrollingCoordinator::getWebScrollbarLayer(
    ScrollableArea* scrollableArea,
    ScrollbarOrientation orientation) {
  ScrollbarMap& scrollbars = orientation == HorizontalScrollbar
                                 ? m_horizontalScrollbars
                                 : m_verticalScrollbars;
  return scrollbars.get(scrollableArea);
}

void ScrollingCoordinator::scrollableAreaScrollbarLayerDidChange(
    ScrollableArea* scrollableArea,
    ScrollbarOrientation orientation) {
  if (!m_page || !m_page->mainFrame())
    return;

  bool isMainFrame = isForMainFrame(scrollableArea);
  GraphicsLayer* scrollbarGraphicsLayer =
      orientation == HorizontalScrollbar
          ? scrollableArea->layerForHorizontalScrollbar()
          : scrollableArea->layerForVerticalScrollbar();

  if (scrollbarGraphicsLayer) {
    Scrollbar& scrollbar = orientation == HorizontalScrollbar
                               ? *scrollableArea->horizontalScrollbar()
                               : *scrollableArea->verticalScrollbar();
    if (scrollbar.isCustomScrollbar()) {
      detachScrollbarLayer(scrollbarGraphicsLayer);
      scrollbarGraphicsLayer->platformLayer()->addMainThreadScrollingReasons(
          MainThreadScrollingReason::kCustomScrollbarScrolling);
      return;
    }

    // Invalidate custom scrollbar scrolling reason in case a custom
    // scrollbar becomes a non-custom one.
    scrollbarGraphicsLayer->platformLayer()->clearMainThreadScrollingReasons(
        MainThreadScrollingReason::kCustomScrollbarScrolling);
    WebScrollbarLayer* scrollbarLayer =
        getWebScrollbarLayer(scrollableArea, orientation);
    if (!scrollbarLayer) {
      Settings* settings = m_page->mainFrame()->settings();

      std::unique_ptr<WebScrollbarLayer> webScrollbarLayer;
      if (settings->getUseSolidColorScrollbars()) {
        DCHECK(RuntimeEnabledFeatures::overlayScrollbarsEnabled());
        webScrollbarLayer = createSolidColorScrollbarLayer(
            orientation, scrollbar.theme().thumbThickness(scrollbar),
            scrollbar.theme().trackPosition(scrollbar),
            scrollableArea->shouldPlaceVerticalScrollbarOnLeft());
      } else {
        webScrollbarLayer =
            createScrollbarLayer(scrollbar, m_page->deviceScaleFactor());
      }
      scrollbarLayer = addWebScrollbarLayer(scrollableArea, orientation,
                                            std::move(webScrollbarLayer));
    }

    WebLayer* scrollLayer = toWebLayer(scrollableArea->layerForScrolling());
    setupScrollbarLayer(scrollbarGraphicsLayer, scrollbarLayer, scrollLayer);

    // Root layer non-overlay scrollbars should be marked opaque to disable
    // blending.
    bool isOpaqueScrollbar = !scrollbar.isOverlayScrollbar();
    scrollbarGraphicsLayer->setContentsOpaque(isMainFrame && isOpaqueScrollbar);
  } else {
    removeWebScrollbarLayer(scrollableArea, orientation);
  }
}

bool ScrollingCoordinator::scrollableAreaScrollLayerDidChange(
    ScrollableArea* scrollableArea) {
  if (!m_page || !m_page->mainFrame())
    return false;

  GraphicsLayer* scrollLayer = scrollableArea->layerForScrolling();

  if (scrollLayer) {
    bool isForVisualViewport =
        scrollableArea == &m_page->frameHost().visualViewport();
    scrollLayer->setScrollableArea(scrollableArea, isForVisualViewport);
  }

  WebLayer* webLayer = toWebLayer(scrollableArea->layerForScrolling());
  WebLayer* containerLayer = toWebLayer(scrollableArea->layerForContainer());
  if (webLayer) {
    webLayer->setScrollClipLayer(containerLayer);
    DoublePoint scrollPosition(FloatPoint(scrollableArea->scrollOrigin()) +
                               scrollableArea->getScrollOffset());
    webLayer->setScrollPositionDouble(scrollPosition);

    webLayer->setBounds(scrollableArea->contentsSize());
    bool canScrollX = scrollableArea->userInputScrollable(HorizontalScrollbar);
    bool canScrollY = scrollableArea->userInputScrollable(VerticalScrollbar);
    webLayer->setUserScrollable(canScrollX, canScrollY);
  }
  if (WebScrollbarLayer* scrollbarLayer =
          getWebScrollbarLayer(scrollableArea, HorizontalScrollbar)) {
    GraphicsLayer* horizontalScrollbarLayer =
        scrollableArea->layerForHorizontalScrollbar();
    if (horizontalScrollbarLayer)
      setupScrollbarLayer(horizontalScrollbarLayer, scrollbarLayer, webLayer);
  }
  if (WebScrollbarLayer* scrollbarLayer =
          getWebScrollbarLayer(scrollableArea, VerticalScrollbar)) {
    GraphicsLayer* verticalScrollbarLayer =
        scrollableArea->layerForVerticalScrollbar();

    if (verticalScrollbarLayer)
      setupScrollbarLayer(verticalScrollbarLayer, scrollbarLayer, webLayer);
  }

  // Update the viewport layer registration if the outer viewport may have
  // changed.
  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled() &&
      isForRootLayer(scrollableArea))
    m_page->chromeClient().registerViewportLayers();

  CompositorAnimationTimeline* timeline;
  // FrameView::compositorAnimationTimeline() can indirectly return
  // m_programmaticScrollAnimatorTimeline if it does not have its own
  // timeline.
  if (scrollableArea->isFrameView()) {
    timeline = toFrameView(scrollableArea)->compositorAnimationTimeline();
  } else if (scrollableArea->isPaintLayerScrollableArea()) {
    timeline = toPaintLayerScrollableArea(scrollableArea)
                   ->compositorAnimationTimeline();
  } else {
    timeline = m_programmaticScrollAnimatorTimeline.get();
  }
  scrollableArea->layerForScrollingDidChange(timeline);

  return !!webLayer;
}

using GraphicsLayerHitTestRects =
    WTF::HashMap<const GraphicsLayer*, Vector<LayoutRect>>;

// In order to do a DFS cross-frame walk of the Layer tree, we need to know
// which Layers have child frames inside of them. This computes a mapping for
// the current frame which we can consult while walking the layers of that
// frame.  Whenever we descend into a new frame, a new map will be created.
using LayerFrameMap =
    HeapHashMap<const PaintLayer*, HeapVector<Member<const LocalFrame>>>;
static void makeLayerChildFrameMap(const LocalFrame* currentFrame,
                                   LayerFrameMap* map) {
  map->clear();
  const FrameTree& tree = currentFrame->tree();
  for (const Frame* child = tree.firstChild(); child;
       child = child->tree().nextSibling()) {
    if (!child->isLocalFrame())
      continue;
    const LayoutItem ownerLayoutItem = toLocalFrame(child)->ownerLayoutItem();
    if (ownerLayoutItem.isNull())
      continue;
    const PaintLayer* containingLayer = ownerLayoutItem.enclosingLayer();
    LayerFrameMap::iterator iter = map->find(containingLayer);
    if (iter == map->end())
      map->add(containingLayer, HeapVector<Member<const LocalFrame>>())
          .storedValue->value.push_back(toLocalFrame(child));
    else
      iter->value.push_back(toLocalFrame(child));
  }
}

static void projectRectsToGraphicsLayerSpaceRecursive(
    const PaintLayer* curLayer,
    const LayerHitTestRects& layerRects,
    GraphicsLayerHitTestRects& graphicsRects,
    LayoutGeometryMap& geometryMap,
    HashSet<const PaintLayer*>& layersWithRects,
    LayerFrameMap& layerChildFrameMap) {
  // If this layer is throttled, ignore it.
  if (curLayer->layoutObject()->frameView() &&
      curLayer->layoutObject()->frameView()->shouldThrottleRendering())
    return;
  // Project any rects for the current layer
  LayerHitTestRects::const_iterator layerIter = layerRects.find(curLayer);
  if (layerIter != layerRects.end()) {
    // Find the enclosing composited layer when it's in another document (for
    // non-composited iframes).
    const PaintLayer* compositedLayer =
        layerIter->key
            ->enclosingLayerForPaintInvalidationCrossingFrameBoundaries();
    DCHECK(compositedLayer);

    // Find the appropriate GraphicsLayer for the composited Layer.
    GraphicsLayer* graphicsLayer =
        compositedLayer->graphicsLayerBacking(curLayer->layoutObject());

    GraphicsLayerHitTestRects::iterator glIter =
        graphicsRects.find(graphicsLayer);
    Vector<LayoutRect>* glRects;
    if (glIter == graphicsRects.end())
      glRects = &graphicsRects.add(graphicsLayer, Vector<LayoutRect>())
                     .storedValue->value;
    else
      glRects = &glIter->value;

    // Transform each rect to the co-ordinate space of the graphicsLayer.
    for (size_t i = 0; i < layerIter->value.size(); ++i) {
      LayoutRect rect = layerIter->value[i];
      if (compositedLayer != curLayer) {
        FloatQuad compositorQuad = geometryMap.mapToAncestor(
            FloatRect(rect), compositedLayer->layoutObject());
        rect = LayoutRect(compositorQuad.boundingBox());
        // If the enclosing composited layer itself is scrolled, we have to undo
        // the subtraction of its scroll offset since we want the offset
        // relative to the scrolling content, not the element itself.
        if (compositedLayer->layoutObject()->hasOverflowClip())
          rect.move(compositedLayer->layoutBox()->scrolledContentOffset());
      }
      PaintLayer::mapRectInPaintInvalidationContainerToBacking(
          *compositedLayer->layoutObject(), rect);
      rect.move(-graphicsLayer->offsetFromLayoutObject());

      glRects->push_back(rect);
    }
  }

  // Walk child layers of interest
  for (const PaintLayer* childLayer = curLayer->firstChild(); childLayer;
       childLayer = childLayer->nextSibling()) {
    if (layersWithRects.contains(childLayer)) {
      geometryMap.pushMappingsToAncestor(childLayer, curLayer);
      projectRectsToGraphicsLayerSpaceRecursive(
          childLayer, layerRects, graphicsRects, geometryMap, layersWithRects,
          layerChildFrameMap);
      geometryMap.popMappingsToAncestor(curLayer);
    }
  }

  // If this layer has any frames of interest as a child of it, walk those (with
  // an updated frame map).
  LayerFrameMap::iterator mapIter = layerChildFrameMap.find(curLayer);
  if (mapIter != layerChildFrameMap.end()) {
    for (size_t i = 0; i < mapIter->value.size(); i++) {
      const LocalFrame* childFrame = mapIter->value[i];
      const PaintLayer* childLayer =
          childFrame->view()->layoutViewItem().layer();
      if (layersWithRects.contains(childLayer)) {
        LayerFrameMap newLayerChildFrameMap;
        makeLayerChildFrameMap(childFrame, &newLayerChildFrameMap);
        geometryMap.pushMappingsToAncestor(childLayer, curLayer);
        projectRectsToGraphicsLayerSpaceRecursive(
            childLayer, layerRects, graphicsRects, geometryMap, layersWithRects,
            newLayerChildFrameMap);
        geometryMap.popMappingsToAncestor(curLayer);
      }
    }
  }
}

static void projectRectsToGraphicsLayerSpace(
    LocalFrame* mainFrame,
    const LayerHitTestRects& layerRects,
    GraphicsLayerHitTestRects& graphicsRects) {
  TRACE_EVENT0("input",
               "ScrollingCoordinator::projectRectsToGraphicsLayerSpace");
  bool touchHandlerInChildFrame = false;

  // We have a set of rects per Layer, we need to map them to their bounding
  // boxes in their enclosing composited layer. To do this most efficiently
  // we'll walk the Layer tree using LayoutGeometryMap. First record all the
  // branches we should traverse in the tree (including all documents on the
  // page).
  HashSet<const PaintLayer*> layersWithRects;
  for (const auto& layerRect : layerRects) {
    const PaintLayer* layer = layerRect.key;
    do {
      if (!layersWithRects.add(layer).isNewEntry)
        break;

      if (layer->parent()) {
        layer = layer->parent();
      } else {
        LayoutItem parentDocLayoutItem =
            layer->layoutObject()->frame()->ownerLayoutItem();
        if (!parentDocLayoutItem.isNull()) {
          layer = parentDocLayoutItem.enclosingLayer();
          touchHandlerInChildFrame = true;
        }
      }
    } while (layer);
  }

  // Now walk the layer projecting rects while maintaining a LayoutGeometryMap
  MapCoordinatesFlags flags = UseTransforms;
  if (touchHandlerInChildFrame)
    flags |= TraverseDocumentBoundaries;
  PaintLayer* rootLayer = mainFrame->contentLayoutItem().layer();
  LayoutGeometryMap geometryMap(flags);
  geometryMap.pushMappingsToAncestor(rootLayer, 0);
  LayerFrameMap layerChildFrameMap;
  makeLayerChildFrameMap(mainFrame, &layerChildFrameMap);
  projectRectsToGraphicsLayerSpaceRecursive(
      rootLayer, layerRects, graphicsRects, geometryMap, layersWithRects,
      layerChildFrameMap);
}

void ScrollingCoordinator::updateTouchEventTargetRectsIfNeeded() {
  TRACE_EVENT0("input",
               "ScrollingCoordinator::updateTouchEventTargetRectsIfNeeded");

  // TODO(chrishtr): implement touch event target rects for SPv2.
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    return;

  LayerHitTestRects touchEventTargetRects;
  computeTouchEventTargetRects(touchEventTargetRects);
  setTouchEventTargetRects(touchEventTargetRects);
}

void ScrollingCoordinator::reset() {
  for (const auto& scrollbar : m_horizontalScrollbars)
    GraphicsLayer::unregisterContentsLayer(scrollbar.value->layer());
  for (const auto& scrollbar : m_verticalScrollbars)
    GraphicsLayer::unregisterContentsLayer(scrollbar.value->layer());

  m_horizontalScrollbars.clear();
  m_verticalScrollbars.clear();
  m_layersWithTouchRects.clear();
  m_wasFrameScrollable = false;

  m_lastMainThreadScrollingReasons = 0;
  setShouldUpdateScrollLayerPositionOnMainThread(
      m_lastMainThreadScrollingReasons);
}

// Note that in principle this could be called more often than
// computeTouchEventTargetRects, for example during a non-composited scroll
// (although that's not yet implemented - crbug.com/261307).
void ScrollingCoordinator::setTouchEventTargetRects(
    LayerHitTestRects& layerRects) {
  TRACE_EVENT0("input", "ScrollingCoordinator::setTouchEventTargetRects");

  // Update the list of layers with touch hit rects.
  HashSet<const PaintLayer*> oldLayersWithTouchRects;
  m_layersWithTouchRects.swap(oldLayersWithTouchRects);
  for (const auto& layerRect : layerRects) {
    if (!layerRect.value.isEmpty()) {
      const PaintLayer* compositedLayer =
          layerRect.key
              ->enclosingLayerForPaintInvalidationCrossingFrameBoundaries();
      DCHECK(compositedLayer);
      m_layersWithTouchRects.add(compositedLayer);
    }
  }

  // Ensure we have an entry for each composited layer that previously had rects
  // (so that old ones will get cleared out). Note that ideally we'd track this
  // on GraphicsLayer instead of Layer, but we have no good hook into the
  // lifetime of a GraphicsLayer.
  for (const PaintLayer* layer : oldLayersWithTouchRects) {
    if (!layerRects.contains(layer))
      layerRects.add(layer, Vector<LayoutRect>());
  }

  GraphicsLayerHitTestRects graphicsLayerRects;
  projectRectsToGraphicsLayerSpace(m_page->deprecatedLocalMainFrame(),
                                   layerRects, graphicsLayerRects);

  for (const auto& layerRect : graphicsLayerRects) {
    const GraphicsLayer* graphicsLayer = layerRect.key;
    WebVector<WebRect> webRects(layerRect.value.size());
    for (size_t i = 0; i < layerRect.value.size(); ++i)
      webRects[i] = enclosingIntRect(layerRect.value[i]);
    graphicsLayer->platformLayer()->setTouchEventHandlerRegion(webRects);
  }
}

void ScrollingCoordinator::touchEventTargetRectsDidChange() {
  DCHECK(m_page);
  if (!m_page->mainFrame()->isLocalFrame() ||
      !m_page->deprecatedLocalMainFrame()->view())
    return;

  // Wait until after layout to update.
  if (m_page->deprecatedLocalMainFrame()->view()->needsLayout())
    return;

  // FIXME: scheduleAnimation() is just a method of forcing the compositor to
  // realize that it needs to commit here. We should expose a cleaner API for
  // this.
  LayoutViewItem layoutView =
      m_page->deprecatedLocalMainFrame()->contentLayoutItem();
  if (!layoutView.isNull() && layoutView.compositor() &&
      layoutView.compositor()->staleInCompositingMode())
    m_page->deprecatedLocalMainFrame()->view()->scheduleAnimation();

  m_touchEventTargetRectsAreDirty = true;
}

void ScrollingCoordinator::updateScrollParentForGraphicsLayer(
    GraphicsLayer* child,
    const PaintLayer* parent) {
  WebLayer* scrollParentWebLayer = nullptr;
  if (parent && parent->hasCompositedLayerMapping())
    scrollParentWebLayer =
        toWebLayer(parent->compositedLayerMapping()->scrollingContentsLayer());

  child->setScrollParent(scrollParentWebLayer);
}

void ScrollingCoordinator::updateClipParentForGraphicsLayer(
    GraphicsLayer* child,
    const PaintLayer* parent) {
  WebLayer* clipParentWebLayer = nullptr;
  if (parent && parent->hasCompositedLayerMapping())
    clipParentWebLayer =
        toWebLayer(parent->compositedLayerMapping()->parentForSublayers());

  child->setClipParent(clipParentWebLayer);
}

void ScrollingCoordinator::willDestroyLayer(PaintLayer* layer) {
  m_layersWithTouchRects.remove(layer);
}

void ScrollingCoordinator::setShouldUpdateScrollLayerPositionOnMainThread(
    MainThreadScrollingReasons mainThreadScrollingReasons) {
  if (!m_page->mainFrame()->isLocalFrame() ||
      !m_page->deprecatedLocalMainFrame()->view())
    return;

  GraphicsLayer* visualViewportLayer =
      m_page->frameHost().visualViewport().scrollLayer();
  WebLayer* visualViewportScrollLayer = toWebLayer(visualViewportLayer);
  GraphicsLayer* layer =
      m_page->deprecatedLocalMainFrame()->view()->layerForScrolling();
  if (WebLayer* scrollLayer = toWebLayer(layer)) {
    m_lastMainThreadScrollingReasons = mainThreadScrollingReasons;
    if (mainThreadScrollingReasons) {
      if (ScrollAnimatorBase* scrollAnimator =
              layer->getScrollableArea()->existingScrollAnimator()) {
        DCHECK(RuntimeEnabledFeatures::slimmingPaintV2Enabled() ||
               m_page->deprecatedLocalMainFrame()
                       ->document()
                       ->lifecycle()
                       .state() >= DocumentLifecycle::CompositingClean);
        scrollAnimator->takeOverCompositorAnimation();
      }
      scrollLayer->addMainThreadScrollingReasons(mainThreadScrollingReasons);
      if (visualViewportScrollLayer) {
        if (ScrollAnimatorBase* scrollAnimator =
                visualViewportLayer->getScrollableArea()
                    ->existingScrollAnimator()) {
          DCHECK(RuntimeEnabledFeatures::slimmingPaintV2Enabled() ||
                 m_page->deprecatedLocalMainFrame()
                         ->document()
                         ->lifecycle()
                         .state() >= DocumentLifecycle::CompositingClean);
          scrollAnimator->takeOverCompositorAnimation();
        }
        visualViewportScrollLayer->addMainThreadScrollingReasons(
            mainThreadScrollingReasons);
      }
    } else {
      // Clear all main thread scrolling reasons except the one that's set
      // if there is a running scroll animation.
      uint32_t mainThreadScrollingReasonsToClear = ~0u;
      mainThreadScrollingReasonsToClear &=
          ~MainThreadScrollingReason::kHandlingScrollFromMainThread;
      scrollLayer->clearMainThreadScrollingReasons(
          mainThreadScrollingReasonsToClear);
      if (visualViewportScrollLayer)
        visualViewportScrollLayer->clearMainThreadScrollingReasons(
            mainThreadScrollingReasonsToClear);
    }
  }
}

void ScrollingCoordinator::layerTreeViewInitialized(
    WebLayerTreeView& layerTreeView,
    FrameView* view) {
  if (Platform::current()->isThreadedAnimationEnabled() &&
      layerTreeView.compositorAnimationHost()) {
    std::unique_ptr<CompositorAnimationTimeline> timeline =
        CompositorAnimationTimeline::create();
    std::unique_ptr<CompositorAnimationHost> host =
        WTF::makeUnique<CompositorAnimationHost>(
            layerTreeView.compositorAnimationHost());
    if (view && view->frame().localFrameRoot() != m_page->mainFrame()) {
      view->setAnimationHost(std::move(host));
      view->setAnimationTimeline(std::move(timeline));
      view->compositorAnimationHost()->addTimeline(
          *view->compositorAnimationTimeline());
    } else {
      m_animationHost = std::move(host);
      m_programmaticScrollAnimatorTimeline = std::move(timeline);
      m_animationHost->addTimeline(*m_programmaticScrollAnimatorTimeline.get());
    }
  }
}

void ScrollingCoordinator::willCloseLayerTreeView(
    WebLayerTreeView& layerTreeView,
    FrameView* view) {
  if (view && view->frame().localFrameRoot() != m_page->mainFrame()) {
    view->compositorAnimationHost()->removeTimeline(
        *view->compositorAnimationTimeline());
    view->setAnimationTimeline(nullptr);
    view->setAnimationHost(nullptr);
  } else if (m_programmaticScrollAnimatorTimeline) {
    m_animationHost->removeTimeline(
        *m_programmaticScrollAnimatorTimeline.get());
    m_programmaticScrollAnimatorTimeline = nullptr;
    m_animationHost = nullptr;
  }
}

void ScrollingCoordinator::willBeDestroyed() {
  DCHECK(m_page);

  m_page = nullptr;
  for (const auto& scrollbar : m_horizontalScrollbars)
    GraphicsLayer::unregisterContentsLayer(scrollbar.value->layer());
  for (const auto& scrollbar : m_verticalScrollbars)
    GraphicsLayer::unregisterContentsLayer(scrollbar.value->layer());
}

bool ScrollingCoordinator::coordinatesScrollingForFrameView(
    FrameView* frameView) const {
  DCHECK(isMainThread());

  // We currently only support composited mode.
  LayoutViewItem layoutView = frameView->frame().contentLayoutItem();
  if (layoutView.isNull())
    return false;
  return layoutView.usesCompositing();
}

Region ScrollingCoordinator::computeShouldHandleScrollGestureOnMainThreadRegion(
    const LocalFrame* frame,
    const IntPoint& frameLocation) const {
  Region shouldHandleScrollGestureOnMainThreadRegion;
  FrameView* frameView = frame->view();
  if (!frameView || frameView->shouldThrottleRendering() ||
      !frameView->isVisible())
    return shouldHandleScrollGestureOnMainThreadRegion;

  IntPoint offset = frameLocation;
  offset.moveBy(frameView->frameRect().location());

  if (const FrameView::ScrollableAreaSet* scrollableAreas =
          frameView->scrollableAreas()) {
    for (const ScrollableArea* scrollableArea : *scrollableAreas) {
      if (scrollableArea->isFrameView() &&
          toFrameView(scrollableArea)->shouldThrottleRendering())
        continue;
      // Composited scrollable areas can be scrolled off the main thread.
      if (scrollableArea->usesCompositedScrolling())
        continue;
      IntRect box = scrollableArea->scrollableAreaBoundingBox();
      box.moveBy(offset);
      shouldHandleScrollGestureOnMainThreadRegion.unite(box);
    }
  }

  // We use GestureScrollBegin/Update/End for moving the resizer handle. So we
  // mark these small resizer areas as non-fast-scrollable to allow the scroll
  // gestures to be passed to main thread if they are targeting the resizer
  // area. (Resizing is done in EventHandler.cpp on main thread).
  if (const FrameView::ResizerAreaSet* resizerAreas =
          frameView->resizerAreas()) {
    for (const LayoutBox* box : *resizerAreas) {
      IntRect bounds = box->absoluteBoundingBoxRect();
      IntRect corner =
          box->layer()->getScrollableArea()->touchResizerCornerRect(bounds);
      corner.moveBy(offset);
      shouldHandleScrollGestureOnMainThreadRegion.unite(corner);
    }
  }

  if (const FrameView::ChildrenWidgetSet* children = frameView->children()) {
    for (const Member<Widget>& child : *children) {
      if (!(*child).isPluginView())
        continue;

      PluginView* pluginView = toPluginView(child.get());
      if (pluginView->wantsWheelEvents()) {
        IntRect box = pluginView->frameRect();
        box.moveBy(offset);
        shouldHandleScrollGestureOnMainThreadRegion.unite(box);
      }
    }
  }

  const FrameTree& tree = frame->tree();
  for (Frame* subFrame = tree.firstChild(); subFrame;
       subFrame = subFrame->tree().nextSibling()) {
    if (subFrame->isLocalFrame())
      shouldHandleScrollGestureOnMainThreadRegion.unite(
          computeShouldHandleScrollGestureOnMainThreadRegion(
              toLocalFrame(subFrame), offset));
  }

  return shouldHandleScrollGestureOnMainThreadRegion;
}

static void accumulateDocumentTouchEventTargetRects(LayerHitTestRects& rects,
                                                    const Document* document) {
  DCHECK(document);
  const EventTargetSet* targets =
      document->frameHost()->eventHandlerRegistry().eventHandlerTargets(
          EventHandlerRegistry::TouchStartOrMoveEventBlocking);
  if (!targets)
    return;

  // If there's a handler on the window, document, html or body element (fairly
  // common in practice), then we can quickly mark the entire document and skip
  // looking at any other handlers.  Note that technically a handler on the body
  // doesn't cover the whole document, but it's reasonable to be conservative
  // and report the whole document anyway.
  //
  // Fullscreen HTML5 video when OverlayFullscreenVideo is enabled is
  // implemented by replacing the root cc::layer with the video layer so doing
  // this optimization causes the compositor to think that there are no
  // handlers, therefore skip it.
  if (!document->layoutViewItem().compositor()->inOverlayFullscreenVideo()) {
    for (const auto& eventTarget : *targets) {
      EventTarget* target = eventTarget.key;
      Node* node = target->toNode();
      LocalDOMWindow* window = target->toLocalDOMWindow();
      // If the target is inside a throttled frame, skip it.
      if (window && window->frame()->view() &&
          window->frame()->view()->shouldThrottleRendering())
        continue;
      if (node && node->document().view() &&
          node->document().view()->shouldThrottleRendering())
        continue;
      if (window || node == document || node == document->documentElement() ||
          node == document->body()) {
        if (LayoutViewItem layoutView = document->layoutViewItem()) {
          layoutView.computeLayerHitTestRects(rects);
        }
        return;
      }
    }
  }

  for (const auto& eventTarget : *targets) {
    EventTarget* target = eventTarget.key;
    Node* node = target->toNode();
    if (!node || !node->isConnected())
      continue;

    // If the document belongs to an invisible subframe it does not have a
    // composited layer and should be skipped.
    if (node->document().isInInvisibleSubframe())
      continue;

    // If the node belongs to a throttled frame, skip it.
    if (node->document().view() &&
        node->document().view()->shouldThrottleRendering())
      continue;

    if (node->isDocumentNode() && node != document) {
      accumulateDocumentTouchEventTargetRects(rects, toDocument(node));
    } else if (LayoutObject* layoutObject = node->layoutObject()) {
      // If the set also contains one of our ancestor nodes then processing
      // this node would be redundant.
      bool hasTouchEventTargetAncestor = false;
      for (Node& ancestor : NodeTraversal::ancestorsOf(*node)) {
        if (hasTouchEventTargetAncestor)
          break;
        if (targets->contains(&ancestor))
          hasTouchEventTargetAncestor = true;
      }
      if (!hasTouchEventTargetAncestor) {
        // Walk up the tree to the outermost non-composited scrollable layer.
        PaintLayer* enclosingNonCompositedScrollLayer = nullptr;
        for (PaintLayer* parent = layoutObject->enclosingLayer();
             parent && parent->compositingState() == NotComposited;
             parent = parent->parent()) {
          if (parent->scrollsOverflow())
            enclosingNonCompositedScrollLayer = parent;
        }

        // Report the whole non-composited scroll layer as a touch hit rect
        // because any rects inside of it may move around relative to their
        // enclosing composited layer without causing the rects to be
        // recomputed. Non-composited scrolling occurs on the main thread, so
        // we're not getting much benefit from compositor touch hit testing in
        // this case anyway.
        if (enclosingNonCompositedScrollLayer)
          enclosingNonCompositedScrollLayer->computeSelfHitTestRects(rects);

        layoutObject->computeLayerHitTestRects(rects);
      }
    }
  }
}

void ScrollingCoordinator::computeTouchEventTargetRects(
    LayerHitTestRects& rects) {
  TRACE_EVENT0("input", "ScrollingCoordinator::computeTouchEventTargetRects");

  Document* document = m_page->deprecatedLocalMainFrame()->document();
  if (!document || !document->view())
    return;

  accumulateDocumentTouchEventTargetRects(rects, document);
}

void ScrollingCoordinator::
    frameViewHasBackgroundAttachmentFixedObjectsDidChange(
        FrameView* frameView) {
  DCHECK(isMainThread());
  DCHECK(m_page);

  if (!coordinatesScrollingForFrameView(frameView))
    return;

  m_shouldScrollOnMainThreadDirty = true;
}

void ScrollingCoordinator::frameViewFixedObjectsDidChange(
    FrameView* frameView) {
  DCHECK(isMainThread());
  DCHECK(m_page);

  if (!coordinatesScrollingForFrameView(frameView))
    return;

  m_shouldScrollOnMainThreadDirty = true;
}

bool ScrollingCoordinator::isForRootLayer(
    ScrollableArea* scrollableArea) const {
  if (!m_page->mainFrame()->isLocalFrame())
    return false;

  // FIXME(305811): Refactor for OOPI.
  LayoutViewItem layoutViewItem =
      m_page->deprecatedLocalMainFrame()->view()->layoutViewItem();
  return layoutViewItem.isNull()
             ? false
             : scrollableArea == layoutViewItem.layer()->getScrollableArea();
}

bool ScrollingCoordinator::isForMainFrame(
    ScrollableArea* scrollableArea) const {
  if (!m_page->mainFrame()->isLocalFrame())
    return false;

  // FIXME(305811): Refactor for OOPI.
  return scrollableArea == m_page->deprecatedLocalMainFrame()->view();
}

void ScrollingCoordinator::frameViewRootLayerDidChange(FrameView* frameView) {
  DCHECK(isMainThread());
  DCHECK(m_page);

  if (!coordinatesScrollingForFrameView(frameView))
    return;

  notifyGeometryChanged();
}

bool ScrollingCoordinator::frameViewIsDirty() const {
  FrameView* frameView = m_page->mainFrame()->isLocalFrame()
                             ? m_page->deprecatedLocalMainFrame()->view()
                             : nullptr;
  bool frameIsScrollable = frameView && frameView->isScrollable();
  if (frameIsScrollable != m_wasFrameScrollable)
    return true;

  if (WebLayer* scrollLayer =
          frameView ? toWebLayer(frameView->layerForScrolling()) : nullptr)
    return WebSize(frameView->contentsSize()) != scrollLayer->bounds();
  return false;
}

}  // namespace blink
