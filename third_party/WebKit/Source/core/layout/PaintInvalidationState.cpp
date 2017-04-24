// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/PaintInvalidationState.h"

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutAPIShim.h"
#include "core/layout/api/LayoutPartItem.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/paint/PaintInvalidator.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintPropertyTreeBuilder.h"

namespace blink {

static bool supportsCachedOffsets(const LayoutObject& object) {
  // Can't compute paint offsets across objects with transforms, but if they are
  // paint invalidation containers, we don't actually need to compute *across*
  // the container, just up to it. (Also, such objects are the containing block
  // for all children.)
  return !(object.hasTransformRelatedProperty() &&
           !object.isPaintInvalidationContainer()) &&
         !object.hasFilterInducingProperty() && !object.isLayoutFlowThread() &&
         !object.isLayoutMultiColumnSpannerPlaceholder() &&
         !object.styleRef().isFlippedBlocksWritingMode() &&
         !(object.isLayoutBlock() && object.isSVG());
}

PaintInvalidationState::PaintInvalidationState(
    const LayoutView& layoutView,
    Vector<const LayoutObject*>& pendingDelayedPaintInvalidations)
    : m_currentObject(layoutView),
      m_forcedSubtreeInvalidationFlags(0),
      m_clipped(false),
      m_clippedForAbsolutePosition(false),
      m_cachedOffsetsEnabled(true),
      m_cachedOffsetsForAbsolutePositionEnabled(true),
      m_paintInvalidationContainer(&layoutView.containerForPaintInvalidation()),
      m_paintInvalidationContainerForStackedContents(
          m_paintInvalidationContainer),
      m_containerForAbsolutePosition(layoutView),
      m_pendingDelayedPaintInvalidations(pendingDelayedPaintInvalidations),
      m_paintingLayer(*layoutView.layer())
#ifdef CHECK_FAST_PATH_SLOW_PATH_EQUALITY
      ,
      m_canCheckFastPathSlowPathEquality(layoutView ==
                                         m_paintInvalidationContainer)
#endif
{
  DCHECK(!RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled());

  if (!supportsCachedOffsets(layoutView)) {
    m_cachedOffsetsEnabled = false;
    return;
  }

  FloatPoint point = layoutView.localToAncestorPoint(
      FloatPoint(), m_paintInvalidationContainer,
      TraverseDocumentBoundaries | InputIsInFrameCoordinates);
  m_paintOffset = LayoutSize(point.x(), point.y());
  m_paintOffsetForAbsolutePosition = m_paintOffset;
}

PaintInvalidationState::PaintInvalidationState(
    const PaintInvalidationState& parentState,
    const LayoutObject& currentObject)
    : m_currentObject(currentObject),
      m_forcedSubtreeInvalidationFlags(
          parentState.m_forcedSubtreeInvalidationFlags),
      m_clipped(parentState.m_clipped),
      m_clippedForAbsolutePosition(parentState.m_clippedForAbsolutePosition),
      m_clipRect(parentState.m_clipRect),
      m_clipRectForAbsolutePosition(parentState.m_clipRectForAbsolutePosition),
      m_paintOffset(parentState.m_paintOffset),
      m_paintOffsetForAbsolutePosition(
          parentState.m_paintOffsetForAbsolutePosition),
      m_cachedOffsetsEnabled(parentState.m_cachedOffsetsEnabled),
      m_cachedOffsetsForAbsolutePositionEnabled(
          parentState.m_cachedOffsetsForAbsolutePositionEnabled),
      m_paintInvalidationContainer(parentState.m_paintInvalidationContainer),
      m_paintInvalidationContainerForStackedContents(
          parentState.m_paintInvalidationContainerForStackedContents),
      m_containerForAbsolutePosition(
          currentObject.canContainAbsolutePositionObjects()
              ? currentObject
              : parentState.m_containerForAbsolutePosition),
      m_svgTransform(parentState.m_svgTransform),
      m_pendingDelayedPaintInvalidations(
          parentState.m_pendingDelayedPaintInvalidations),
      m_paintingLayer(parentState.childPaintingLayer(currentObject))
#ifdef CHECK_FAST_PATH_SLOW_PATH_EQUALITY
      ,
      m_canCheckFastPathSlowPathEquality(
          parentState.m_canCheckFastPathSlowPathEquality)
#endif
{
  DCHECK(!RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled());
  DCHECK(&m_paintingLayer == currentObject.paintingLayer());

  if (currentObject == parentState.m_currentObject) {
// Sometimes we create a new PaintInvalidationState from parentState on the same
// object (e.g. LayoutView, and the HorriblySlowRectMapping cases in
// LayoutBlock::invalidatePaintOfSubtreesIfNeeded()).
// TODO(wangxianzhu): Avoid this for
// RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled().
#if DCHECK_IS_ON()
    m_didUpdateForChildren = parentState.m_didUpdateForChildren;
#endif
    return;
  }

#if DCHECK_IS_ON()
  DCHECK(parentState.m_didUpdateForChildren);
#endif

  if (currentObject.isPaintInvalidationContainer()) {
    m_paintInvalidationContainer = toLayoutBoxModelObject(&currentObject);
    if (currentObject.styleRef().isStackingContext())
      m_paintInvalidationContainerForStackedContents =
          toLayoutBoxModelObject(&currentObject);
  } else if (currentObject.isLayoutView()) {
    // m_paintInvalidationContainerForStackedContents is only for stacked
    // descendants in its own frame, because it doesn't establish stacking
    // context for stacked contents in sub-frames. Contents stacked in the root
    // stacking context in this frame should use this frame's
    // paintInvalidationContainer.
    m_paintInvalidationContainerForStackedContents =
        m_paintInvalidationContainer;
  } else if (currentObject.isFloatingWithNonContainingBlockParent() ||
             currentObject.isColumnSpanAll()) {
    // In these cases, the object may belong to an ancestor of the current
    // paint invalidation container, in paint order.
    m_paintInvalidationContainer =
        &currentObject.containerForPaintInvalidation();
    m_cachedOffsetsEnabled = false;
  } else if (currentObject.styleRef().isStacked() &&
             // This is to exclude some objects (e.g. LayoutText) inheriting
             // stacked style from parent but aren't actually stacked.
             currentObject.hasLayer() &&
             m_paintInvalidationContainer !=
                 m_paintInvalidationContainerForStackedContents) {
    // The current object is stacked, so we should use
    // m_paintInvalidationContainerForStackedContents as its paint invalidation
    // container on which the current object is painted.
    m_paintInvalidationContainer =
        m_paintInvalidationContainerForStackedContents;
    // We are changing paintInvalidationContainer to
    // m_paintInvalidationContainerForStackedContents. Must disable cached
    // offsets because we didn't track paint offset from
    // m_paintInvalidationContainerForStackedContents.
    // TODO(wangxianzhu): There are optimization opportunities:
    // - Like what we do for fixed-position, calculate the paint offset in slow
    //   path and enable fast path for descendants if possible; or
    // - Track offset between the two paintInvalidationContainers.
    m_cachedOffsetsEnabled = false;
    if (m_forcedSubtreeInvalidationFlags &
        PaintInvalidatorContext::
            ForcedSubtreeFullInvalidationForStackedContents)
      m_forcedSubtreeInvalidationFlags |=
          PaintInvalidatorContext::ForcedSubtreeFullInvalidation;
  }

  if (!currentObject.isBoxModelObject() && !currentObject.isSVG())
    return;

  if (m_cachedOffsetsEnabled || currentObject == m_paintInvalidationContainer)
    m_cachedOffsetsEnabled = supportsCachedOffsets(currentObject);

  if (currentObject.isSVG()) {
    if (currentObject.isSVGRoot()) {
      m_svgTransform =
          toLayoutSVGRoot(currentObject).localToBorderBoxTransform();
      // Don't early return here, because the SVGRoot object needs to execute
      // the later code as a normal LayoutBox.
    } else {
      DCHECK(currentObject != m_paintInvalidationContainer);
      m_svgTransform *= currentObject.localToSVGParentTransform();
      return;
    }
  }

  if (currentObject == m_paintInvalidationContainer) {
    // When we hit a new paint invalidation container, we don't need to
    // continue forcing a check for paint invalidation, since we're
    // descending into a different invalidation container. (For instance if
    // our parents were moved, the entire container will just move.)
    if (currentObject != m_paintInvalidationContainerForStackedContents) {
      // However, we need to keep the FullInvalidationForStackedContents flag
      // if the current object isn't the paint invalidation container of
      // stacked contents.
      m_forcedSubtreeInvalidationFlags &= PaintInvalidatorContext::
          ForcedSubtreeFullInvalidationForStackedContents;
    } else {
      m_forcedSubtreeInvalidationFlags = 0;
      if (currentObject != m_containerForAbsolutePosition &&
          m_cachedOffsetsForAbsolutePositionEnabled && m_cachedOffsetsEnabled) {
        // The current object is the new paintInvalidationContainer for
        // absolute-position descendants but is not their container.
        // Call updateForCurrentObject() before resetting m_paintOffset to get
        // paint offset of the current object from the original
        // paintInvalidationContainerForStackingContents, then use this paint
        // offset to adjust m_paintOffsetForAbsolutePosition.
        updateForCurrentObject(parentState);
        m_paintOffsetForAbsolutePosition -= m_paintOffset;
        if (m_clippedForAbsolutePosition)
          m_clipRectForAbsolutePosition.move(-m_paintOffset);
      }
    }

    m_clipped = false;  // Will be updated in updateForChildren().
    m_paintOffset = LayoutSize();
#ifdef CHECK_FAST_PATH_SLOW_PATH_EQUALITY
    m_canCheckFastPathSlowPathEquality = true;
#endif
    return;
  }

  updateForCurrentObject(parentState);
}

PaintLayer& PaintInvalidationState::childPaintingLayer(
    const LayoutObject& child) const {
  if (child.hasLayer() && toLayoutBoxModelObject(child).hasSelfPaintingLayer())
    return *toLayoutBoxModelObject(child).layer();
  // See LayoutObject::paintingLayer() for the special-cases of floating under
  // inline and multicolumn.
  if (child.isColumnSpanAll() || child.isFloatingWithNonContainingBlockParent())
    return *child.paintingLayer();
  return m_paintingLayer;
}

void PaintInvalidationState::updateForCurrentObject(
    const PaintInvalidationState& parentState) {
  if (!m_cachedOffsetsEnabled)
    return;

  if (m_currentObject.isLayoutView()) {
    DCHECK(&parentState.m_currentObject ==
           LayoutAPIShim::layoutObjectFrom(
               toLayoutView(m_currentObject).frame()->ownerLayoutItem()));
    m_paintOffset +=
        toLayoutBox(parentState.m_currentObject).contentBoxOffset();
    // a LayoutView paints with a defined size but a pixel-rounded offset.
    m_paintOffset = LayoutSize(roundedIntSize(m_paintOffset));
    return;
  }

  EPosition position = m_currentObject.styleRef().position();

  if (position == EPosition::kFixed) {
    // Use slow path to get the offset of the fixed-position, and enable fast
    // path for descendants.
    FloatPoint fixedOffset = m_currentObject.localToAncestorPoint(
        FloatPoint(), m_paintInvalidationContainer, TraverseDocumentBoundaries);
    if (m_paintInvalidationContainer->isBox()) {
      const LayoutBox* box = toLayoutBox(m_paintInvalidationContainer);
      if (box->hasOverflowClip())
        fixedOffset.move(box->scrolledContentOffset());
    }
    m_paintOffset = LayoutSize(fixedOffset.x(), fixedOffset.y());
    // In the above way to get paint offset, we can't get accurate clip rect, so
    // just assume no clip. Clip on fixed-position is rare, in case that
    // paintInvalidationContainer crosses frame boundary and the LayoutView is
    // clipped by something in owner document.
    if (m_clipped) {
      m_clipped = false;
#ifdef CHECK_FAST_PATH_SLOW_PATH_EQUALITY
      m_canCheckFastPathSlowPathEquality = false;
#endif
    }
    return;
  }

  if (position == EPosition::kAbsolute) {
    m_cachedOffsetsEnabled = m_cachedOffsetsForAbsolutePositionEnabled;
    if (!m_cachedOffsetsEnabled)
      return;

    m_paintOffset = m_paintOffsetForAbsolutePosition;
    m_clipped = m_clippedForAbsolutePosition;
    m_clipRect = m_clipRectForAbsolutePosition;

    // Handle absolute-position block under relative-position inline.
    const LayoutObject& container = parentState.m_containerForAbsolutePosition;
    if (container.isInFlowPositioned() && container.isLayoutInline())
      m_paintOffset +=
          toLayoutInline(container).offsetForInFlowPositionedInline(
              toLayoutBox(m_currentObject));
  }

  if (m_currentObject.isBox())
    m_paintOffset += toLayoutBox(m_currentObject).locationOffset();

  if (m_currentObject.isInFlowPositioned() && m_currentObject.hasLayer())
    m_paintOffset += toLayoutBoxModelObject(m_currentObject)
                         .layer()
                         ->offsetForInFlowPosition();
}

void PaintInvalidationState::updateForChildren(PaintInvalidationReason reason) {
#if DCHECK_IS_ON()
  DCHECK(!m_didUpdateForChildren);
  m_didUpdateForChildren = true;
#endif

  switch (reason) {
    case PaintInvalidationDelayedFull:
      m_pendingDelayedPaintInvalidations.push_back(&m_currentObject);
      break;
    case PaintInvalidationSubtree:
      m_forcedSubtreeInvalidationFlags |=
          (PaintInvalidatorContext::ForcedSubtreeFullInvalidation |
           PaintInvalidatorContext::
               ForcedSubtreeFullInvalidationForStackedContents);
      break;
    case PaintInvalidationSVGResourceChange:
      m_forcedSubtreeInvalidationFlags |=
          PaintInvalidatorContext::ForcedSubtreeSVGResourceChange;
      break;
    default:
      break;
  }

  updateForNormalChildren();

  if (m_currentObject == m_containerForAbsolutePosition) {
    if (m_paintInvalidationContainer ==
        m_paintInvalidationContainerForStackedContents) {
      m_cachedOffsetsForAbsolutePositionEnabled = m_cachedOffsetsEnabled;
      if (m_cachedOffsetsEnabled) {
        m_paintOffsetForAbsolutePosition = m_paintOffset;
        m_clippedForAbsolutePosition = m_clipped;
        m_clipRectForAbsolutePosition = m_clipRect;
      }
    } else {
      // Cached offsets for absolute-position are from
      // m_paintInvalidationContainer, which can't be used if the
      // absolute-position descendants will use a different
      // paintInvalidationContainer.
      // TODO(wangxianzhu): Same optimization opportunities as under isStacked()
      // condition in the PaintInvalidationState::PaintInvalidationState(...
      // LayoutObject&...).
      m_cachedOffsetsForAbsolutePositionEnabled = false;
    }
  }
}

void PaintInvalidationState::updateForNormalChildren() {
  if (!m_cachedOffsetsEnabled)
    return;

  if (!m_currentObject.isBox())
    return;
  const LayoutBox& box = toLayoutBox(m_currentObject);

  if (box.isLayoutView()) {
    if (!RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
      if (box != m_paintInvalidationContainer) {
        m_paintOffset -=
            LayoutSize(toLayoutView(box).frameView()->getScrollOffset());
        addClipRectRelativeToPaintOffset(toLayoutView(box).viewRect());
      }
      return;
    }
  } else if (box.isSVGRoot()) {
    const LayoutSVGRoot& svgRoot = toLayoutSVGRoot(box);
    if (svgRoot.shouldApplyViewportClip())
      addClipRectRelativeToPaintOffset(
          LayoutRect(LayoutPoint(), LayoutSize(svgRoot.pixelSnappedSize())));
  } else if (box.isTableRow()) {
    // Child table cell's locationOffset() includes its row's locationOffset().
    m_paintOffset -= box.locationOffset();
  }

  if (!box.hasClipRelatedProperty())
    return;

  // Do not clip or scroll for the paint invalidation container, because the
  // semantics of visual rects do not include clipping or scrolling on that
  // object.
  if (box != m_paintInvalidationContainer) {
    // This won't work fully correctly for fixed-position elements, who should
    // receive CSS clip but for whom the current object is not in the containing
    // block chain.
    addClipRectRelativeToPaintOffset(box.clippingRect());
    if (box.hasOverflowClip())
      m_paintOffset -= box.scrolledContentOffset();
  }

  // FIXME: <http://bugs.webkit.org/show_bug.cgi?id=13443> Apply control clip if
  // present.
}

static FloatPoint slowLocalToAncestorPoint(const LayoutObject& object,
                                           const LayoutBoxModelObject& ancestor,
                                           const FloatPoint& point) {
  if (object.isLayoutView())
    return toLayoutView(object).localToAncestorPoint(
        point, &ancestor,
        TraverseDocumentBoundaries | InputIsInFrameCoordinates);
  FloatPoint result =
      object.localToAncestorPoint(point, &ancestor, TraverseDocumentBoundaries);
  // Paint invalidation does not include scroll of the ancestor.
  if (ancestor.isBox()) {
    const LayoutBox* box = toLayoutBox(&ancestor);
    if (box->hasOverflowClip())
      result.move(box->scrolledContentOffset());
  }
  return result;
}

LayoutPoint PaintInvalidationState::computeLocationInBacking(
    const LayoutPoint& visualRectLocation) const {
#if DCHECK_IS_ON()
  DCHECK(!m_didUpdateForChildren);
#endif

  // Use visual rect location for LayoutTexts because it suffices to check
  // visual rect change for layout caused invalidation.
  if (m_currentObject.isText())
    return visualRectLocation;

  FloatPoint point;
  if (m_paintInvalidationContainer != &m_currentObject) {
    if (m_cachedOffsetsEnabled) {
      if (m_currentObject.isSVGChild())
        point = m_svgTransform.mapPoint(point);
      point += FloatPoint(m_paintOffset);
#ifdef CHECK_FAST_PATH_SLOW_PATH_EQUALITY
      DCHECK(point ==
             slowLocalOriginToAncestorPoint(
                 m_currentObject, m_paintInvalidationContainer, FloatPoint()));
#endif
    } else {
      point = slowLocalToAncestorPoint(
          m_currentObject, *m_paintInvalidationContainer, FloatPoint());
    }
  }

  PaintLayer::mapPointInPaintInvalidationContainerToBacking(
      *m_paintInvalidationContainer, point);

  return LayoutPoint(point);
}

LayoutRect PaintInvalidationState::computeVisualRectInBacking() const {
#if DCHECK_IS_ON()
  DCHECK(!m_didUpdateForChildren);
#endif

  if (m_currentObject.isSVGChild())
    return computeVisualRectInBackingForSVG();

  LayoutRect rect = m_currentObject.localVisualRect();
  mapLocalRectToPaintInvalidationBacking(rect);
  return rect;
}

LayoutRect PaintInvalidationState::computeVisualRectInBackingForSVG() const {
  LayoutRect rect;
  if (m_cachedOffsetsEnabled) {
    FloatRect svgRect = SVGLayoutSupport::localVisualRect(m_currentObject);
    rect = SVGLayoutSupport::transformVisualRect(m_currentObject,
                                                 m_svgTransform, svgRect);
    rect.move(m_paintOffset);
    if (m_clipped)
      rect.intersect(m_clipRect);
#ifdef CHECK_FAST_PATH_SLOW_PATH_EQUALITY
    LayoutRect slowPathRect = SVGLayoutSupport::visualRectInAncestorSpace(
        m_currentObject, *m_paintInvalidationContainer);
    assertFastPathAndSlowPathRectsEqual(rect, slowPathRect);
#endif
  } else {
    // TODO(wangxianzhu): Sometimes m_cachedOffsetsEnabled==false doesn't mean
    // we can't use cached m_svgTransform. We can use hybrid fast path (for SVG)
    // and slow path (for things above the SVGRoot).
    rect = SVGLayoutSupport::visualRectInAncestorSpace(
        m_currentObject, *m_paintInvalidationContainer);
  }

  PaintLayer::mapRectInPaintInvalidationContainerToBacking(
      *m_paintInvalidationContainer, rect);

  return rect;
}

static void slowMapToVisualRectInAncestorSpace(
    const LayoutObject& object,
    const LayoutBoxModelObject& ancestor,
    LayoutRect& rect) {
  // TODO(wkorman): The flip below is required because visual rects are
  // currently in "physical coordinates with flipped block-flow direction"
  // (see LayoutBoxModelObject.h) but we need them to be in physical
  // coordinates.
  if (object.isBox())
    toLayoutBox(&object)->flipForWritingMode(rect);

  if (object.isLayoutView()) {
    toLayoutView(object).mapToVisualRectInAncestorSpace(
        &ancestor, rect, InputIsInFrameCoordinates, DefaultVisualRectFlags);
  } else {
    object.mapToVisualRectInAncestorSpace(&ancestor, rect);
  }
}

void PaintInvalidationState::mapLocalRectToPaintInvalidationContainer(
    LayoutRect& rect) const {
#if DCHECK_IS_ON()
  DCHECK(!m_didUpdateForChildren);
#endif

  if (m_cachedOffsetsEnabled) {
#ifdef CHECK_FAST_PATH_SLOW_PATH_EQUALITY
    LayoutRect slowPathRect(rect);
    slowMapToVisualRectInAncestorSpace(
        m_currentObject, *m_paintInvalidationContainer, slowPathRect);
#endif
    rect.move(m_paintOffset);
    if (m_clipped)
      rect.intersect(m_clipRect);
#ifdef CHECK_FAST_PATH_SLOW_PATH_EQUALITY
    assertFastPathAndSlowPathRectsEqual(rect, slowPathRect);
#endif
  } else {
    slowMapToVisualRectInAncestorSpace(m_currentObject,
                                       *m_paintInvalidationContainer, rect);
  }
}

void PaintInvalidationState::mapLocalRectToPaintInvalidationBacking(
    LayoutRect& rect) const {
  mapLocalRectToPaintInvalidationContainer(rect);

  PaintLayer::mapRectInPaintInvalidationContainerToBacking(
      *m_paintInvalidationContainer, rect);
}

void PaintInvalidationState::addClipRectRelativeToPaintOffset(
    const LayoutRect& localClipRect) {
  LayoutRect clipRect = localClipRect;
  clipRect.move(m_paintOffset);
  if (m_clipped) {
    m_clipRect.intersect(clipRect);
  } else {
    m_clipRect = clipRect;
    m_clipped = true;
  }
}

PaintLayer& PaintInvalidationState::paintingLayer() const {
  DCHECK(&m_paintingLayer == m_currentObject.paintingLayer());
  return m_paintingLayer;
}

#ifdef CHECK_FAST_PATH_SLOW_PATH_EQUALITY

static bool mayHaveBeenSaturated(LayoutUnit value) {
  // This is not accurate, just to avoid too big values.
  return value.abs() >= LayoutUnit::max() / 2;
}

static bool mayHaveBeenSaturated(const LayoutRect& rect) {
  return mayHaveBeenSaturated(rect.x()) || mayHaveBeenSaturated(rect.y()) ||
         mayHaveBeenSaturated(rect.width()) ||
         mayHaveBeenSaturated(rect.height());
}

void PaintInvalidationState::assertFastPathAndSlowPathRectsEqual(
    const LayoutRect& fastPathRect,
    const LayoutRect& slowPathRect) const {
  if (!m_canCheckFastPathSlowPathEquality)
    return;

  // TODO(crbug.com/597903): Fast path and slow path should generate equal empty
  // rects.
  if (fastPathRect.isEmpty() && slowPathRect.isEmpty())
    return;

  if (fastPathRect == slowPathRect)
    return;

  // LayoutUnit uses saturated arithmetic operations. If any interim or final
  // result is saturated, the same operations in different order produce
  // different results. Don't compare results if any of them may have been
  // saturated.
  if (mayHaveBeenSaturated(fastPathRect) || mayHaveBeenSaturated(slowPathRect))
    return;

  // Tolerate the difference between the two paths when crossing frame
  // boundaries.
  if (m_currentObject.view() != m_paintInvalidationContainer->view()) {
    LayoutRect inflatedFastPathRect = fastPathRect;
    inflatedFastPathRect.inflate(1);
    if (inflatedFastPathRect.contains(slowPathRect))
      return;
    LayoutRect inflatedSlowPathRect = slowPathRect;
    inflatedSlowPathRect.inflate(1);
    if (inflatedSlowPathRect.contains(fastPathRect))
      return;
  }

  LOG(ERROR) << "Fast path visual rect differs from slow path: fast: "
             << fastPathRect.toString()
             << " vs slow: " << slowPathRect.toString();
  showLayoutTree(&m_currentObject);

  ASSERT_NOT_REACHED();
}

#endif  // CHECK_FAST_PATH_SLOW_PATH_EQUALITY

static const PaintPropertyTreeBuilderContext& dummyTreeBuilderContext() {
  DEFINE_STATIC_LOCAL(PaintPropertyTreeBuilderContext, dummyContext, ());
  return dummyContext;
}

PaintInvalidatorContextAdapter::PaintInvalidatorContextAdapter(
    const PaintInvalidationState& paintInvalidationState)
    : PaintInvalidatorContext(dummyTreeBuilderContext()),
      m_paintInvalidationState(paintInvalidationState) {
  forcedSubtreeInvalidationFlags =
      paintInvalidationState.m_forcedSubtreeInvalidationFlags;
  paintInvalidationContainer =
      &paintInvalidationState.paintInvalidationContainer();
  paintingLayer = &paintInvalidationState.paintingLayer();
}

void PaintInvalidatorContextAdapter::mapLocalRectToPaintInvalidationBacking(
    const LayoutObject& object,
    LayoutRect& rect) const {
  DCHECK(&object == &m_paintInvalidationState.currentObject());
  m_paintInvalidationState.mapLocalRectToPaintInvalidationBacking(rect);
}

}  // namespace blink
