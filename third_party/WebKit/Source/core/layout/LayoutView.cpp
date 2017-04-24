/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc.
 *               All rights reserved.
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

#include "core/layout/LayoutView.h"

#include <inttypes.h>
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutGeometryMap.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/ViewFragmentationContext.h"
#include "core/layout/api/LayoutAPIShim.h"
#include "core/layout/api/LayoutPartItem.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/page/Page.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/ViewPaintInvalidator.h"
#include "core/paint/ViewPainter.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "platform/Histogram.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/TransformState.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/instrumentation/tracing/TracedValue.h"
#include "public/platform/Platform.h"
#include "wtf/PtrUtil.h"

namespace blink {

namespace {

class HitTestLatencyRecorder {
 public:
  HitTestLatencyRecorder(bool allowsChildFrameContent)
      : m_start(WTF::monotonicallyIncreasingTime()),
        m_allowsChildFrameContent(allowsChildFrameContent) {}

  ~HitTestLatencyRecorder() {
    int duration = static_cast<int>(
        (WTF::monotonicallyIncreasingTime() - m_start) * 1000000);

    if (m_allowsChildFrameContent) {
      DEFINE_STATIC_LOCAL(CustomCountHistogram, recursiveLatencyHistogram,
                          ("Event.Latency.HitTestRecursive", 0, 10000000, 100));
      recursiveLatencyHistogram.count(duration);
    } else {
      DEFINE_STATIC_LOCAL(CustomCountHistogram, latencyHistogram,
                          ("Event.Latency.HitTest", 0, 10000000, 100));
      latencyHistogram.count(duration);
    }
  }

 private:
  double m_start;
  bool m_allowsChildFrameContent;
};

}  // namespace

LayoutView::LayoutView(Document* document)
    : LayoutBlockFlow(document),
      m_frameView(document->view()),
      m_selectionStart(nullptr),
      m_selectionEnd(nullptr),
      m_selectionStartPos(-1),
      m_selectionEndPos(-1),
      m_layoutState(nullptr),
      m_layoutQuoteHead(nullptr),
      m_layoutCounterCount(0),
      m_hitTestCount(0),
      m_hitTestCacheHits(0),
      m_hitTestCache(HitTestCache::create()) {
  // init LayoutObject attributes
  setInline(false);

  m_minPreferredLogicalWidth = LayoutUnit();
  m_maxPreferredLogicalWidth = LayoutUnit();

  setPreferredLogicalWidthsDirty(MarkOnlyThis);

  setPositionState(EPosition::kAbsolute);  // to 0,0 :)
}

LayoutView::~LayoutView() {}

bool LayoutView::hitTest(HitTestResult& result) {
  // We have to recursively update layout/style here because otherwise, when the
  // hit test recurses into a child document, it could trigger a layout on the
  // parent document, which can destroy PaintLayer that are higher up in the
  // call stack, leading to crashes.
  // Note that Document::updateLayout calls its parent's updateLayout.
  // Note that if an iframe has its render pipeline throttled, it will not
  // update layout here, and it will also not propagate the hit test into the
  // iframe's inner document.
  frameView()->updateLifecycleToCompositingCleanPlusScrolling();
  HitTestLatencyRecorder hitTestLatencyRecorder(
      result.hitTestRequest().allowsChildFrameContent());
  return hitTestNoLifecycleUpdate(result);
}

bool LayoutView::hitTestNoLifecycleUpdate(HitTestResult& result) {
  TRACE_EVENT_BEGIN0("blink,devtools.timeline", "HitTest");
  m_hitTestCount++;

  ASSERT(!result.hitTestLocation().isRectBasedTest() ||
         result.hitTestRequest().listBased());

  commitPendingSelection();

  uint64_t domTreeVersion = document().domTreeVersion();
  HitTestResult cacheResult = result;
  bool hitLayer = false;
  if (m_hitTestCache->lookupCachedResult(cacheResult, domTreeVersion)) {
    m_hitTestCacheHits++;
    hitLayer = true;
    result = cacheResult;
  } else {
    hitLayer = layer()->hitTest(result);

    // FrameView scrollbars are not the same as Layer scrollbars tested by
    // Layer::hitTestOverflowControls, so we need to test FrameView scrollbars
    // separately here. Note that it's important we do this after the hit test
    // above, because that may overwrite the entire HitTestResult when it finds
    // a hit.
    IntPoint framePoint =
        frameView()->contentsToFrame(result.hitTestLocation().roundedPoint());
    if (Scrollbar* frameScrollbar =
            frameView()->scrollbarAtFramePoint(framePoint))
      result.setScrollbar(frameScrollbar);

    if (hitLayer)
      m_hitTestCache->addCachedResult(result, domTreeVersion);
  }

  TRACE_EVENT_END1(
      "blink,devtools.timeline", "HitTest", "endData",
      InspectorHitTestEvent::endData(result.hitTestRequest(),
                                     result.hitTestLocation(), result));
  return hitLayer;
}

void LayoutView::clearHitTestCache() {
  m_hitTestCache->clear();
  LayoutPartItem frameLayoutItem = frame()->ownerLayoutItem();
  if (!frameLayoutItem.isNull())
    frameLayoutItem.view().clearHitTestCache();
}

void LayoutView::computeLogicalHeight(
    LayoutUnit logicalHeight,
    LayoutUnit,
    LogicalExtentComputedValues& computedValues) const {
  computedValues.m_extent = LayoutUnit(viewLogicalHeightForBoxSizing());
}

void LayoutView::updateLogicalWidth() {
  setLogicalWidth(LayoutUnit(viewLogicalWidthForBoxSizing()));
}

bool LayoutView::isChildAllowed(LayoutObject* child,
                                const ComputedStyle&) const {
  return child->isBox();
}

void LayoutView::layoutContent() {
  ASSERT(needsLayout());

  LayoutBlockFlow::layout();

#if DCHECK_IS_ON()
  checkLayoutState();
#endif
}

#if DCHECK_IS_ON()
void LayoutView::checkLayoutState() {
  ASSERT(!m_layoutState->next());
}
#endif

void LayoutView::setShouldDoFullPaintInvalidationOnResizeIfNeeded(
    bool widthChanged,
    bool heightChanged) {
  // When background-attachment is 'fixed', we treat the viewport (instead of
  // the 'root' i.e. html or body) as the background positioning area, and we
  // should fully invalidate on viewport resize if the background image is not
  // composited and needs full paint invalidation on background positioning area
  // resize.
  if (style()->hasFixedBackgroundImage() &&
      (!m_compositor ||
       !m_compositor->needsFixedRootBackgroundLayer(layer()))) {
    if ((widthChanged && mustInvalidateFillLayersPaintOnWidthChange(
                             style()->backgroundLayers())) ||
        (heightChanged && mustInvalidateFillLayersPaintOnHeightChange(
                              style()->backgroundLayers())))
      setShouldDoFullPaintInvalidation(PaintInvalidationBoundsChange);
  }
}

void LayoutView::layout() {
  if (!document().paginated())
    setPageLogicalHeight(LayoutUnit());

  // TODO(wangxianzhu): Move this into ViewPaintInvalidator when
  // rootLayerScrolling is permanently enabled.
  IncludeScrollbarsInRect includeScrollbars =
      RuntimeEnabledFeatures::rootLayerScrollingEnabled() ? IncludeScrollbars
                                                          : ExcludeScrollbars;
  setShouldDoFullPaintInvalidationOnResizeIfNeeded(
      offsetWidth() != layoutSize(includeScrollbars).width(),
      offsetHeight() != layoutSize(includeScrollbars).height());

  if (pageLogicalHeight() && shouldUsePrintingLayout()) {
    m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = logicalWidth();
    if (!m_fragmentationContext) {
      m_fragmentationContext =
          WTF::wrapUnique(new ViewFragmentationContext(*this));
      m_paginationStateChanged = true;
    }
  } else if (m_fragmentationContext) {
    m_fragmentationContext.reset();
    m_paginationStateChanged = true;
  }

  SubtreeLayoutScope layoutScope(*this);

  // Use calcWidth/Height to get the new width/height, since this will take the
  // full page zoom factor into account.
  bool relayoutChildren =
      !shouldUsePrintingLayout() &&
      (!m_frameView || logicalWidth() != viewLogicalWidthForBoxSizing() ||
       logicalHeight() != viewLogicalHeightForBoxSizing());
  if (relayoutChildren) {
    layoutScope.setChildNeedsLayout(this);
    for (LayoutObject* child = firstChild(); child;
         child = child->nextSibling()) {
      if (child->isSVGRoot())
        continue;

      if ((child->isBox() && toLayoutBox(child)->hasRelativeLogicalHeight()) ||
          child->style()->logicalHeight().isPercentOrCalc() ||
          child->style()->logicalMinHeight().isPercentOrCalc() ||
          child->style()->logicalMaxHeight().isPercentOrCalc())
        layoutScope.setChildNeedsLayout(child);
    }

    if (document().svgExtensions())
      document()
          .accessSVGExtensions()
          .invalidateSVGRootsWithRelativeLengthDescendents(&layoutScope);
  }

  ASSERT(!m_layoutState);
  if (!needsLayout())
    return;

  LayoutState rootLayoutState(*this);

  layoutContent();

#if DCHECK_IS_ON()
  checkLayoutState();
#endif
  clearNeedsLayout();
}

LayoutRect LayoutView::visualOverflowRect() const {
  // In root layer scrolling mode, the LayoutView performs overflow clipping
  // like a regular scrollable div.
  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled())
    return LayoutBlockFlow::visualOverflowRect();

  // Ditto when not in compositing mode.
  if (!usesCompositing())
    return LayoutBlockFlow::visualOverflowRect();

  // In normal compositing mode, LayoutView doesn't actually apply clipping
  // on its descendants. Instead their visual overflow is propagated to
  // compositor()->m_rootContentLayer for accelerated scrolling.
  return layoutOverflowRect();
}

LayoutRect LayoutView::localVisualRect() const {
  // TODO(wangxianzhu): This is only required without rootLayerScrolls (though
  // it is also correct but unnecessary with rootLayerScrolls) because of the
  // special LayoutView overflow model.
  LayoutRect rect = visualOverflowRect();
  rect.unite(LayoutRect(rect.location(), viewRect().size()));
  return rect;
}

void LayoutView::mapLocalToAncestor(const LayoutBoxModelObject* ancestor,
                                    TransformState& transformState,
                                    MapCoordinatesFlags mode) const {
  if (!ancestor && mode & UseTransforms && shouldUseTransformFromContainer(0)) {
    TransformationMatrix t;
    getTransformFromContainer(0, LayoutSize(), t);
    transformState.applyTransform(t);
  }

  if ((mode & IsFixed) && m_frameView) {
    transformState.move(offsetForFixedPosition());
    // IsFixed flag is only applicable within this LayoutView.
    mode &= ~IsFixed;
  }

  if (ancestor == this)
    return;

  if (mode & TraverseDocumentBoundaries) {
    LayoutPartItem parentDocLayoutItem = frame()->ownerLayoutItem();
    if (!parentDocLayoutItem.isNull()) {
      if (!(mode & InputIsInFrameCoordinates)) {
        transformState.move(LayoutSize(-frame()->view()->getScrollOffset()));
      } else {
        // The flag applies to immediate LayoutView only.
        mode &= ~InputIsInFrameCoordinates;
      }

      transformState.move(parentDocLayoutItem.contentBoxOffset());

      parentDocLayoutItem.mapLocalToAncestor(ancestor, transformState, mode);
    } else {
      frameView()->applyTransformForTopFrameSpace(transformState);
    }
  }
}

const LayoutObject* LayoutView::pushMappingToContainer(
    const LayoutBoxModelObject* ancestorToStopAt,
    LayoutGeometryMap& geometryMap) const {
  LayoutSize offset;
  LayoutObject* container = nullptr;

  if (geometryMap.getMapCoordinatesFlags() & TraverseDocumentBoundaries) {
    if (LayoutPart* parentDocLayoutObject = toLayoutPart(
            LayoutAPIShim::layoutObjectFrom(frame()->ownerLayoutItem()))) {
      offset = -LayoutSize(m_frameView->getScrollOffset());
      offset += parentDocLayoutObject->contentBoxOffset();
      container = parentDocLayoutObject;
    }
  }

  // If a container was specified, and was not 0 or the LayoutView, then we
  // should have found it by now unless we're traversing to a parent document.
  DCHECK(!ancestorToStopAt || ancestorToStopAt == this || container);

  if ((!ancestorToStopAt || container) &&
      shouldUseTransformFromContainer(container)) {
    TransformationMatrix t;
    getTransformFromContainer(container, LayoutSize(), t);
    geometryMap.push(this, t, ContainsFixedPosition, offsetForFixedPosition());
  } else {
    geometryMap.push(this, offset, 0, offsetForFixedPosition());
  }

  return container;
}

void LayoutView::mapAncestorToLocal(const LayoutBoxModelObject* ancestor,
                                    TransformState& transformState,
                                    MapCoordinatesFlags mode) const {
  if (this != ancestor && (mode & TraverseDocumentBoundaries)) {
    if (LayoutPart* parentDocLayoutObject = toLayoutPart(
            LayoutAPIShim::layoutObjectFrom(frame()->ownerLayoutItem()))) {
      // A LayoutView is a containing block for fixed-position elements, so
      // don't carry this state across frames.
      parentDocLayoutObject->mapAncestorToLocal(ancestor, transformState,
                                                mode & ~IsFixed);

      transformState.move(parentDocLayoutObject->contentBoxOffset());
      transformState.move(LayoutSize(-frame()->view()->getScrollOffset()));
    }
  } else {
    DCHECK(this == ancestor || !ancestor);
  }

  if (mode & IsFixed)
    transformState.move(offsetForFixedPosition());
}

void LayoutView::computeSelfHitTestRects(Vector<LayoutRect>& rects,
                                         const LayoutPoint&) const {
  // Record the entire size of the contents of the frame. Note that we don't
  // just use the viewport size (containing block) here because we want to
  // ensure this includes all children (so we can avoid walking them
  // explicitly).
  rects.push_back(
      LayoutRect(LayoutPoint::zero(), LayoutSize(frameView()->contentsSize())));
}

PaintInvalidationReason LayoutView::invalidatePaintIfNeeded(
    const PaintInvalidationState& paintInvalidationState) {
  return LayoutBlockFlow::invalidatePaintIfNeeded(paintInvalidationState);
}

PaintInvalidationReason LayoutView::invalidatePaintIfNeeded(
    const PaintInvalidatorContext& context) const {
  return ViewPaintInvalidator(*this, context).invalidatePaintIfNeeded();
}

void LayoutView::paint(const PaintInfo& paintInfo,
                       const LayoutPoint& paintOffset) const {
  ViewPainter(*this).paint(paintInfo, paintOffset);
}

void LayoutView::paintBoxDecorationBackground(const PaintInfo& paintInfo,
                                              const LayoutPoint&) const {
  ViewPainter(*this).paintBoxDecorationBackground(paintInfo);
}

static void setShouldDoFullPaintInvalidationForViewAndAllDescendantsInternal(
    LayoutObject* object) {
  object->setShouldDoFullPaintInvalidation();
  for (LayoutObject* child = object->slowFirstChild(); child;
       child = child->nextSibling()) {
    setShouldDoFullPaintInvalidationForViewAndAllDescendantsInternal(child);
  }
}

void LayoutView::setShouldDoFullPaintInvalidationForViewAndAllDescendants() {
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    setShouldDoFullPaintInvalidationIncludingNonCompositingDescendants();
  else
    setShouldDoFullPaintInvalidationForViewAndAllDescendantsInternal(this);
}

void LayoutView::invalidatePaintForViewAndCompositedLayers() {
  setShouldDoFullPaintInvalidationIncludingNonCompositingDescendants();

  // The only way we know how to hit these ASSERTS below this point is via the
  // Chromium OS login screen.
  DisableCompositingQueryAsserts disabler;

  if (compositor()->inCompositingMode())
    compositor()->fullyInvalidatePaint();
}

bool LayoutView::mapToVisualRectInAncestorSpace(
    const LayoutBoxModelObject* ancestor,
    LayoutRect& rect,
    MapCoordinatesFlags mode,
    VisualRectFlags visualRectFlags) const {
  TransformState transformState(TransformState::ApplyTransformDirection,
                                FloatQuad(FloatRect(rect)));
  bool retval = mapToVisualRectInAncestorSpaceInternal(ancestor, transformState,
                                                       mode, visualRectFlags);
  transformState.flatten();
  rect = LayoutRect(transformState.lastPlanarQuad().boundingBox());
  return retval;
}

bool LayoutView::mapToVisualRectInAncestorSpaceInternal(
    const LayoutBoxModelObject* ancestor,
    TransformState& transformState,
    VisualRectFlags visualRectFlags) const {
  return mapToVisualRectInAncestorSpaceInternal(ancestor, transformState, 0,
                                                visualRectFlags);
}

bool LayoutView::mapToVisualRectInAncestorSpaceInternal(
    const LayoutBoxModelObject* ancestor,
    TransformState& transformState,
    MapCoordinatesFlags mode,
    VisualRectFlags visualRectFlags) const {
  if (mode & IsFixed)
    transformState.move(offsetForFixedPosition(true));

  // Apply our transform if we have one (because of full page zooming).
  if (layer() && layer()->transform()) {
    transformState.applyTransform(layer()->currentTransform(),
                                  TransformState::FlattenTransform);
  }

  transformState.flatten();

  if (ancestor == this)
    return true;

  Element* owner = document().localOwner();
  if (!owner) {
    LayoutRect rect(transformState.lastPlanarQuad().boundingBox());
    bool retval = frameView()->mapToVisualRectInTopFrameSpace(rect);
    transformState.setQuad(FloatQuad(FloatRect(rect)));
    return retval;
  }

  if (LayoutBox* obj = owner->layoutBox()) {
    LayoutRect rect(transformState.lastPlanarQuad().boundingBox());
    if (!(mode & InputIsInFrameCoordinates)) {
      // Intersect the viewport with the visual rect.
      LayoutRect viewRectangle = viewRect();
      if (visualRectFlags & EdgeInclusive) {
        if (!rect.inclusiveIntersect(viewRectangle)) {
          transformState.setQuad(FloatQuad(FloatRect(rect)));
          return false;
        }
      } else {
        rect.intersect(viewRectangle);
      }

      // Adjust for scroll offset of the view.
      rect.moveBy(-viewRectangle.location());
    }
    // Frames are painted at rounded-int position. Since we cannot efficiently
    // compute the subpixel offset of painting at this point in a a bottom-up
    // walk, round to the enclosing int rect, which will enclose the actual
    // visible rect.
    rect = LayoutRect(enclosingIntRect(rect));

    // Adjust for frame border.
    rect.move(obj->contentBoxOffset());
    transformState.setQuad(FloatQuad(FloatRect(rect)));

    return obj->mapToVisualRectInAncestorSpaceInternal(ancestor, transformState,
                                                       visualRectFlags);
  }

  // This can happen, e.g., if the iframe element has display:none.
  transformState.setQuad(FloatQuad(FloatRect()));
  return false;
}

LayoutSize LayoutView::offsetForFixedPosition(bool includePendingScroll) const {
  FloatSize adjustment;
  if (m_frameView) {
    adjustment += m_frameView->getScrollOffset();

    // FIXME: Paint invalidation should happen after scroll updates, so there
    // should be no pending scroll delta.
    // However, we still have paint invalidation during layout, so we can't
    // ASSERT for now. crbug.com/434950.
    // ASSERT(m_frameView->pendingScrollDelta().isZero());
    // If we have a pending scroll, invalidate the previous scroll position.
    if (includePendingScroll && !m_frameView->pendingScrollDelta().isZero())
      adjustment -= m_frameView->pendingScrollDelta();
  }

  if (hasOverflowClip())
    adjustment += FloatSize(scrolledContentOffset());

  return roundedLayoutSize(adjustment);
}

void LayoutView::absoluteRects(Vector<IntRect>& rects,
                               const LayoutPoint& accumulatedOffset) const {
  rects.push_back(
      pixelSnappedIntRect(accumulatedOffset, LayoutSize(layer()->size())));
}

void LayoutView::absoluteQuads(Vector<FloatQuad>& quads,
                               MapCoordinatesFlags mode) const {
  quads.push_back(localToAbsoluteQuad(
      FloatRect(FloatPoint(), FloatSize(layer()->size())), mode));
}

static LayoutObject* layoutObjectAfterPosition(LayoutObject* object,
                                               unsigned offset) {
  if (!object)
    return nullptr;

  LayoutObject* child = object->childAt(offset);
  return child ? child : object->nextInPreOrderAfterChildren();
}

static LayoutRect selectionRectForLayoutObject(const LayoutObject* object) {
  if (!object->isRooted())
    return LayoutRect();

  if (!object->canUpdateSelectionOnRootLineBoxes())
    return LayoutRect();

  return object->selectionRectInViewCoordinates();
}

IntRect LayoutView::selectionBounds() {
  // Now create a single bounding box rect that encloses the whole selection.
  LayoutRect selRect;

  typedef HashSet<const LayoutBlock*> VisitedContainingBlockSet;
  VisitedContainingBlockSet visitedContainingBlocks;

  commitPendingSelection();
  LayoutObject* os = m_selectionStart;
  LayoutObject* stop =
      layoutObjectAfterPosition(m_selectionEnd, m_selectionEndPos);
  while (os && os != stop) {
    if ((os->canBeSelectionLeaf() || os == m_selectionStart ||
         os == m_selectionEnd) &&
        os->getSelectionState() != SelectionNone) {
      // Blocks are responsible for painting line gaps and margin gaps. They
      // must be examined as well.
      selRect.unite(selectionRectForLayoutObject(os));
      const LayoutBlock* cb = os->containingBlock();
      while (cb && !cb->isLayoutView()) {
        selRect.unite(selectionRectForLayoutObject(cb));
        VisitedContainingBlockSet::AddResult addResult =
            visitedContainingBlocks.insert(cb);
        if (!addResult.isNewEntry)
          break;
        cb = cb->containingBlock();
      }
    }

    os = os->nextInPreOrder();
  }

  return pixelSnappedIntRect(selRect);
}

void LayoutView::invalidatePaintForSelection() {
  LayoutObject* end =
      layoutObjectAfterPosition(m_selectionEnd, m_selectionEndPos);
  for (LayoutObject* o = m_selectionStart; o && o != end;
       o = o->nextInPreOrder()) {
    if (!o->canBeSelectionLeaf() && o != m_selectionStart &&
        o != m_selectionEnd)
      continue;
    if (o->getSelectionState() == SelectionNone)
      continue;

    o->setShouldInvalidateSelection();
  }
}

// When exploring the LayoutTree looking for the nodes involved in the
// Selection, sometimes it's required to change the traversing direction because
// the "start" position is below the "end" one.
static inline LayoutObject* getNextOrPrevLayoutObjectBasedOnDirection(
    const LayoutObject* o,
    const LayoutObject* stop,
    bool& continueExploring,
    bool& exploringBackwards) {
  LayoutObject* next;
  if (exploringBackwards) {
    next = o->previousInPreOrder();
    continueExploring = next && !(next)->isLayoutView();
  } else {
    next = o->nextInPreOrder();
    continueExploring = next && next != stop;
    exploringBackwards = !next && (next != stop);
    if (exploringBackwards) {
      next = stop->previousInPreOrder();
      continueExploring = next && !next->isLayoutView();
    }
  }

  return next;
}

void LayoutView::setSelection(
    LayoutObject* start,
    int startPos,
    LayoutObject* end,
    int endPos,
    SelectionPaintInvalidationMode blockPaintInvalidationMode) {
  // This code makes no assumptions as to if the layout tree is up to date or
  // not and will not try to update it. Currently clearSelection calls this
  // (intentionally) without updating the layout tree as it doesn't care.
  // Other callers may want to force recalc style before calling this.

  // Make sure both our start and end objects are defined.
  // Check www.msnbc.com and try clicking around to find the case where this
  // happened.
  if ((start && !end) || (end && !start))
    return;

  // Just return if the selection hasn't changed.
  if (m_selectionStart == start && m_selectionStartPos == startPos &&
      m_selectionEnd == end && m_selectionEndPos == endPos)
    return;

  // Record the old selected objects. These will be used later when we compare
  // against the new selected objects.
  int oldStartPos = m_selectionStartPos;
  int oldEndPos = m_selectionEndPos;

  // Objects each have a single selection rect to examine.
  typedef HashMap<LayoutObject*, SelectionState> SelectedObjectMap;
  SelectedObjectMap oldSelectedObjects;
  // FIXME: |newSelectedObjects| doesn't really need to store the
  // SelectionState, it's just more convenient to have it use the same data
  // structure as |oldSelectedObjects|.
  SelectedObjectMap newSelectedObjects;

  // Blocks contain selected objects and fill gaps between them, either on the
  // left, right, or in between lines and blocks.
  // In order to get the visual rect right, we have to examine left, middle, and
  // right rects individually, since otherwise the union of those rects might
  // remain the same even when changes have occurred.
  typedef HashMap<LayoutBlock*, SelectionState> SelectedBlockMap;
  SelectedBlockMap oldSelectedBlocks;
  // FIXME: |newSelectedBlocks| doesn't really need to store the SelectionState,
  // it's just more convenient to have it use the same data structure as
  // |oldSelectedBlocks|.
  SelectedBlockMap newSelectedBlocks;

  LayoutObject* os = m_selectionStart;
  LayoutObject* stop =
      layoutObjectAfterPosition(m_selectionEnd, m_selectionEndPos);
  bool exploringBackwards = false;
  bool continueExploring = os && (os != stop);
  while (continueExploring) {
    if ((os->canBeSelectionLeaf() || os == m_selectionStart ||
         os == m_selectionEnd) &&
        os->getSelectionState() != SelectionNone) {
      // Blocks are responsible for painting line gaps and margin gaps.  They
      // must be examined as well.
      oldSelectedObjects.set(os, os->getSelectionState());
      if (blockPaintInvalidationMode == PaintInvalidationNewXOROld) {
        LayoutBlock* cb = os->containingBlock();
        while (cb && !cb->isLayoutView()) {
          SelectedBlockMap::AddResult result =
              oldSelectedBlocks.insert(cb, cb->getSelectionState());
          if (!result.isNewEntry)
            break;
          cb = cb->containingBlock();
        }
      }
    }

    os = getNextOrPrevLayoutObjectBasedOnDirection(os, stop, continueExploring,
                                                   exploringBackwards);
  }

  // Now clear the selection.
  SelectedObjectMap::iterator oldObjectsEnd = oldSelectedObjects.end();
  for (SelectedObjectMap::iterator i = oldSelectedObjects.begin();
       i != oldObjectsEnd; ++i)
    i->key->setSelectionStateIfNeeded(SelectionNone);

  // set selection start and end
  m_selectionStart = start;
  m_selectionStartPos = startPos;
  m_selectionEnd = end;
  m_selectionEndPos = endPos;

  // Update the selection status of all objects between m_selectionStart and
  // m_selectionEnd
  if (start && start == end) {
    start->setSelectionStateIfNeeded(SelectionBoth);
  } else {
    if (start)
      start->setSelectionStateIfNeeded(SelectionStart);
    if (end)
      end->setSelectionStateIfNeeded(SelectionEnd);
  }

  LayoutObject* o = start;
  stop = layoutObjectAfterPosition(end, endPos);

  while (o && o != stop) {
    if (o != start && o != end && o->canBeSelectionLeaf())
      o->setSelectionStateIfNeeded(SelectionInside);
    o = o->nextInPreOrder();
  }

  // Now that the selection state has been updated for the new objects, walk
  // them again and put them in the new objects list.
  o = start;
  exploringBackwards = false;
  continueExploring = o && (o != stop);
  while (continueExploring) {
    if ((o->canBeSelectionLeaf() || o == start || o == end) &&
        o->getSelectionState() != SelectionNone) {
      newSelectedObjects.set(o, o->getSelectionState());
      LayoutBlock* cb = o->containingBlock();
      while (cb && !cb->isLayoutView()) {
        SelectedBlockMap::AddResult result =
            newSelectedBlocks.insert(cb, cb->getSelectionState());
        if (!result.isNewEntry)
          break;
        cb = cb->containingBlock();
      }
    }

    o = getNextOrPrevLayoutObjectBasedOnDirection(o, stop, continueExploring,
                                                  exploringBackwards);
  }

  if (!m_frameView)
    return;

  // Have any of the old selected objects changed compared to the new selection?
  for (SelectedObjectMap::iterator i = oldSelectedObjects.begin();
       i != oldObjectsEnd; ++i) {
    LayoutObject* obj = i->key;
    SelectionState newSelectionState = obj->getSelectionState();
    SelectionState oldSelectionState = i->value;
    if (newSelectionState != oldSelectionState ||
        (m_selectionStart == obj && oldStartPos != m_selectionStartPos) ||
        (m_selectionEnd == obj && oldEndPos != m_selectionEndPos)) {
      obj->setShouldInvalidateSelection();
      newSelectedObjects.erase(obj);
    }
  }

  // Any new objects that remain were not found in the old objects dict, and so
  // they need to be updated.
  SelectedObjectMap::iterator newObjectsEnd = newSelectedObjects.end();
  for (SelectedObjectMap::iterator i = newSelectedObjects.begin();
       i != newObjectsEnd; ++i)
    i->key->setShouldInvalidateSelection();

  // Have any of the old blocks changed?
  SelectedBlockMap::iterator oldBlocksEnd = oldSelectedBlocks.end();
  for (SelectedBlockMap::iterator i = oldSelectedBlocks.begin();
       i != oldBlocksEnd; ++i) {
    LayoutBlock* block = i->key;
    SelectionState newSelectionState = block->getSelectionState();
    SelectionState oldSelectionState = i->value;
    if (newSelectionState != oldSelectionState) {
      block->setShouldInvalidateSelection();
      newSelectedBlocks.erase(block);
    }
  }

  // Any new blocks that remain were not found in the old blocks dict, and so
  // they need to be updated.
  SelectedBlockMap::iterator newBlocksEnd = newSelectedBlocks.end();
  for (SelectedBlockMap::iterator i = newSelectedBlocks.begin();
       i != newBlocksEnd; ++i)
    i->key->setShouldInvalidateSelection();
}

void LayoutView::clearSelection() {
  // For querying Layer::compositingState()
  // This is correct, since destroying layout objects needs to cause eager paint
  // invalidations.
  DisableCompositingQueryAsserts disabler;

  setSelection(0, -1, 0, -1, PaintInvalidationNewMinusOld);
}

bool LayoutView::hasPendingSelection() const {
  return m_frameView->frame().selection().isAppearanceDirty();
}

void LayoutView::commitPendingSelection() {
  TRACE_EVENT0("blink", "LayoutView::commitPendingSelection");
  m_frameView->frame().selection().commitAppearanceIfNeeded(*this);
}

LayoutObject* LayoutView::selectionStart() {
  commitPendingSelection();
  return m_selectionStart;
}

LayoutObject* LayoutView::selectionEnd() {
  commitPendingSelection();
  return m_selectionEnd;
}

void LayoutView::selectionStartEnd(int& startPos, int& endPos) {
  commitPendingSelection();
  startPos = m_selectionStartPos;
  endPos = m_selectionEndPos;
}

bool LayoutView::shouldUsePrintingLayout() const {
  if (!document().printing() || !m_frameView)
    return false;
  return m_frameView->frame().shouldUsePrintingLayout();
}

LayoutRect LayoutView::viewRect() const {
  if (shouldUsePrintingLayout())
    return LayoutRect(LayoutPoint(), size());
  if (m_frameView)
    return LayoutRect(m_frameView->visibleContentRect());
  return LayoutRect();
}

LayoutRect LayoutView::overflowClipRect(
    const LayoutPoint& location,
    OverlayScrollbarClipBehavior overlayScrollbarClipBehavior) const {
  LayoutRect rect = viewRect();
  if (rect.isEmpty())
    return LayoutBox::overflowClipRect(location, overlayScrollbarClipBehavior);

  rect.setLocation(location);
  if (hasOverflowClip())
    excludeScrollbars(rect, overlayScrollbarClipBehavior);

  return rect;
}

IntRect LayoutView::documentRect() const {
  LayoutRect overflowRect(layoutOverflowRect());
  flipForWritingMode(overflowRect);
  // TODO(crbug.com/650768): The pixel snapping looks incorrect.
  return pixelSnappedIntRect(overflowRect);
}

bool LayoutView::rootBackgroundIsEntirelyFixed() const {
  return style()->hasEntirelyFixedBackground();
}

IntSize LayoutView::layoutSize(
    IncludeScrollbarsInRect scrollbarInclusion) const {
  if (shouldUsePrintingLayout())
    return IntSize(size().width().toInt(), pageLogicalHeight().toInt());

  if (!m_frameView)
    return IntSize();

  IntSize result = m_frameView->layoutSize(IncludeScrollbars);
  if (scrollbarInclusion == ExcludeScrollbars)
    result =
        m_frameView->layoutViewportScrollableArea()->excludeScrollbars(result);
  return result;
}

int LayoutView::viewLogicalWidth(
    IncludeScrollbarsInRect scrollbarInclusion) const {
  return style()->isHorizontalWritingMode() ? viewWidth(scrollbarInclusion)
                                            : viewHeight(scrollbarInclusion);
}

int LayoutView::viewLogicalHeight(
    IncludeScrollbarsInRect scrollbarInclusion) const {
  return style()->isHorizontalWritingMode() ? viewHeight(scrollbarInclusion)
                                            : viewWidth(scrollbarInclusion);
}

int LayoutView::viewLogicalWidthForBoxSizing() const {
  return viewLogicalWidth(RuntimeEnabledFeatures::rootLayerScrollingEnabled()
                              ? IncludeScrollbars
                              : ExcludeScrollbars);
}

int LayoutView::viewLogicalHeightForBoxSizing() const {
  return viewLogicalHeight(RuntimeEnabledFeatures::rootLayerScrollingEnabled()
                               ? IncludeScrollbars
                               : ExcludeScrollbars);
}

LayoutUnit LayoutView::viewLogicalHeightForPercentages() const {
  if (shouldUsePrintingLayout())
    return pageLogicalHeight();
  return LayoutUnit(viewLogicalHeight());
}

float LayoutView::zoomFactor() const {
  return m_frameView->frame().pageZoomFactor();
}

void LayoutView::updateHitTestResult(HitTestResult& result,
                                     const LayoutPoint& point) {
  if (result.innerNode())
    return;

  Node* node = document().documentElement();
  if (node) {
    LayoutPoint adjustedPoint = point;
    offsetForContents(adjustedPoint);
    result.setNodeAndPosition(node, adjustedPoint);
  }
}

bool LayoutView::usesCompositing() const {
  return m_compositor && m_compositor->staleInCompositingMode();
}

PaintLayerCompositor* LayoutView::compositor() {
  if (!m_compositor)
    m_compositor = WTF::wrapUnique(new PaintLayerCompositor(*this));

  return m_compositor.get();
}

void LayoutView::setIsInWindow(bool isInWindow) {
  if (m_compositor)
    m_compositor->setIsInWindow(isInWindow);
#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
  // We don't invalidate layers during Document::detachLayoutTree(), so must
  // clear the should-keep-alive DisplayItemClients which may be deleted before
  // the layers being subsequence owners.
  if (!isInWindow && layer())
    layer()->endShouldKeepAliveAllClientsRecursive();
#endif
}

IntervalArena* LayoutView::intervalArena() {
  if (!m_intervalArena)
    m_intervalArena = IntervalArena::create();
  return m_intervalArena.get();
}

bool LayoutView::backgroundIsKnownToBeOpaqueInRect(const LayoutRect&) const {
  // FIXME: Remove this main frame check. Same concept applies to subframes too.
  if (!frame()->isMainFrame())
    return false;

  return m_frameView->hasOpaqueBackground();
}

FloatSize LayoutView::viewportSizeForViewportUnits() const {
  return frameView() ? frameView()->viewportSizeForViewportUnits()
                     : FloatSize();
}

void LayoutView::willBeDestroyed() {
  // TODO(wangxianzhu): This is a workaround of crbug.com/570706.
  // Should find and fix the root cause.
  if (PaintLayer* layer = this->layer())
    layer->setNeedsRepaint();
  LayoutBlockFlow::willBeDestroyed();
  m_compositor.reset();
}

void LayoutView::updateFromStyle() {
  LayoutBlockFlow::updateFromStyle();

  // LayoutView of the main frame is responsible for painting base background.
  if (document().isInMainFrame())
    setHasBoxDecorationBackground(true);
}

bool LayoutView::allowsOverflowClip() const {
  return RuntimeEnabledFeatures::rootLayerScrollingEnabled();
}

ScrollResult LayoutView::scroll(ScrollGranularity granularity,
                                const FloatSize& delta) {
  // TODO(bokan): We shouldn't need this specialization but we currently do
  // because of the Windows pan scrolling path. That should go through a more
  // normalized ScrollManager-like scrolling path and we should get rid of
  // of this override. All frame scrolling should be handled by
  // ViewportScrollCallback.

  if (!frameView())
    return ScrollResult(false, false, delta.width(), delta.height());

  return frameView()->getScrollableArea()->userScroll(granularity, delta);
}

LayoutRect LayoutView::debugRect() const {
  LayoutRect rect;
  LayoutBlock* block = containingBlock();
  if (block)
    block->adjustChildDebugRect(rect);

  rect.setWidth(LayoutUnit(viewWidth(IncludeScrollbars)));
  rect.setHeight(LayoutUnit(viewHeight(IncludeScrollbars)));

  return rect;
}

bool LayoutView::paintedOutputOfObjectHasNoEffectRegardlessOfSize() const {
  // Frame scroll corner is painted using LayoutView as the display item client.
  if (!RuntimeEnabledFeatures::rootLayerScrollingEnabled() &&
      (frameView()->horizontalScrollbar() || frameView()->verticalScrollbar()))
    return false;

  return LayoutBlockFlow::paintedOutputOfObjectHasNoEffectRegardlessOfSize();
}

}  // namespace blink
