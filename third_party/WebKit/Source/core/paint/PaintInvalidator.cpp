// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintInvalidator.h"

#include "core/editing/FrameSelection.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTable.h"
#include "core/layout/LayoutView.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "core/paint/PaintPropertyTreeBuilder.h"

namespace blink {

template <typename Rect>
static LayoutRect slowMapToVisualRectInAncestorSpace(
    const LayoutObject& object,
    const LayoutBoxModelObject& ancestor,
    const Rect& rect) {
  if (object.isSVGChild()) {
    LayoutRect result;
    SVGLayoutSupport::mapToVisualRectInAncestorSpace(object, &ancestor,
                                                     FloatRect(rect), result);
    return result;
  }

  LayoutRect result(rect);
  if (object.isLayoutView())
    toLayoutView(object).mapToVisualRectInAncestorSpace(
        &ancestor, result, InputIsInFrameCoordinates, DefaultVisualRectFlags);
  else
    object.mapToVisualRectInAncestorSpace(&ancestor, result);
  return result;
}

// TODO(wangxianzhu): Combine this into
// PaintInvalidator::mapLocalRectToBacking() when removing
// PaintInvalidationState.
// This function is templatized to avoid FloatRect<->LayoutRect conversions
// which affect performance.
template <typename Rect, typename Point>
static LayoutRect mapLocalRectToPaintInvalidationBacking(
    GeometryMapper& geometryMapper,
    const LayoutObject& object,
    const Rect& localRect,
    const PaintInvalidatorContext& context) {
  bool isSVGChild = object.isSVGChild();

  // TODO(wkorman): The flip below is required because visual rects are
  // currently in "physical coordinates with flipped block-flow direction"
  // (see LayoutBoxModelObject.h) but we need them to be in physical
  // coordinates.
  Rect rect = localRect;
  // Writing-mode flipping doesn't apply to non-root SVG.
  if (!isSVGChild) {
    if (object.isBox()) {
      toLayoutBox(object).flipForWritingMode(rect);
    } else if (!(context.forcedSubtreeInvalidationFlags &
                 PaintInvalidatorContext::ForcedSubtreeSlowPathRect)) {
      // For SPv2 and the GeometryMapper path, we also need to convert the rect
      // for non-boxes into physical coordinates before applying paint offset.
      // (Otherwise we'll call mapToVisualrectInAncestorSpace() which requires
      // physical coordinates for boxes, but "physical coordinates with flipped
      // block-flow direction" for non-boxes for which we don't need to flip.)
      // TODO(wangxianzhu): Avoid containingBlock().
      object.containingBlock()->flipForWritingMode(rect);
    }
  }

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    // In SPv2, visual rects are in the space of their local transform node.
    // For SVG, the input rect is in local SVG coordinates in which paint
    // offset doesn't apply.
    if (!isSVGChild)
      rect.moveBy(Point(object.paintOffset()));
    // Use enclosingIntRect to ensure the final visual rect will cover the
    // rect in source coordinates no matter if the painting will use pixel
    // snapping.
    return LayoutRect(enclosingIntRect(rect));
  }

  LayoutRect result;
  if (context.forcedSubtreeInvalidationFlags &
      PaintInvalidatorContext::ForcedSubtreeSlowPathRect) {
    result = slowMapToVisualRectInAncestorSpace(
        object, *context.paintInvalidationContainer, rect);
  } else if (object == context.paintInvalidationContainer) {
    result = LayoutRect(rect);
  } else {
    // For non-root SVG, the input rect is in local SVG coordinates in which
    // paint offset doesn't apply.
    if (!isSVGChild) {
      rect.moveBy(Point(object.paintOffset()));
      // Use enclosingIntRect to ensure the final visual rect will cover the
      // rect in source coordinates no matter if the painting will use pixel
      // snapping.
      rect = Rect(enclosingIntRect(rect));
    }

    const auto* containerContentsProperties =
        context.paintInvalidationContainer->paintProperties()
            ->contentsProperties();
    if (context.treeBuilderContext.current.transform ==
            containerContentsProperties->transform() &&
        context.treeBuilderContext.current.clip ==
            containerContentsProperties->clip()) {
      result = LayoutRect(rect);
    } else {
      PropertyTreeState currentTreeState(
          context.treeBuilderContext.current.transform,
          context.treeBuilderContext.current.clip, nullptr, nullptr);
      result = LayoutRect(geometryMapper.sourceToDestinationVisualRect(
          FloatRect(rect), currentTreeState, *containerContentsProperties));
    }

    // Convert the result to the container's contents space.
    result.moveBy(-context.paintInvalidationContainer->paintOffset());
  }

  object.adjustVisualRectForRasterEffects(result);

  PaintLayer::mapRectInPaintInvalidationContainerToBacking(
      *context.paintInvalidationContainer, result);

  return result;
}

void PaintInvalidatorContext::mapLocalRectToPaintInvalidationBacking(
    const LayoutObject& object,
    LayoutRect& rect) const {
  GeometryMapper geometryMapper;
  rect = blink::mapLocalRectToPaintInvalidationBacking<LayoutRect, LayoutPoint>(
      geometryMapper, object, rect, *this);
}

LayoutRect PaintInvalidator::computeVisualRectInBacking(
    const LayoutObject& object,
    const PaintInvalidatorContext& context) {
  if (object.isSVGChild()) {
    FloatRect localRect = SVGLayoutSupport::localVisualRect(object);
    return mapLocalRectToPaintInvalidationBacking<FloatRect, FloatPoint>(
        m_geometryMapper, object, localRect, context);
  }
  return mapLocalRectToPaintInvalidationBacking<LayoutRect, LayoutPoint>(
      m_geometryMapper, object, object.localVisualRect(), context);
}

LayoutPoint PaintInvalidator::computeLocationInBacking(
    const LayoutObject& object,
    const PaintInvalidatorContext& context) {
  // Use visual rect location for LayoutTexts because it suffices to check
  // visual rect change for layout caused invalidation.
  if (object.isText())
    return context.newVisualRect.location();

  LayoutPoint point;
  if (object != context.paintInvalidationContainer) {
    point.moveBy(object.paintOffset());

    const auto* containerTransform =
        context.paintInvalidationContainer->paintProperties()
            ->contentsProperties()
            ->transform();
    if (context.treeBuilderContext.current.transform != containerTransform) {
      point = LayoutPoint(m_geometryMapper
                              .sourceToDestinationRect(
                                  FloatRect(FloatPoint(point), FloatSize()),
                                  context.treeBuilderContext.current.transform,
                                  containerTransform)
                              .location());
    }

    // Convert the result to the container's contents space.
    point.moveBy(-context.paintInvalidationContainer->paintOffset());
  }

  if (context.paintInvalidationContainer->layer()->groupedMapping()) {
    FloatPoint floatPoint(point);
    PaintLayer::mapPointInPaintInvalidationContainerToBacking(
        *context.paintInvalidationContainer, floatPoint);
    point = LayoutPoint(floatPoint);
  }

  return point;
}

void PaintInvalidator::updatePaintingLayer(const LayoutObject& object,
                                           PaintInvalidatorContext& context) {
  if (object.hasLayer() &&
      toLayoutBoxModelObject(object).hasSelfPaintingLayer()) {
    context.paintingLayer = toLayoutBoxModelObject(object).layer();
  } else if (object.isColumnSpanAll() ||
             (object.isFloating() && !object.parent()->isLayoutBlock())) {
    // See LayoutObject::paintingLayer() for the special-cases of floating under
    // inline and multicolumn.
    context.paintingLayer = object.paintingLayer();
  }

  if (object.isLayoutBlockFlow() && toLayoutBlockFlow(object).containsFloats())
    context.paintingLayer->setNeedsPaintPhaseFloat();

  if (object == context.paintingLayer->layoutObject())
    return;

  if (object.styleRef().hasOutline())
    context.paintingLayer->setNeedsPaintPhaseDescendantOutlines();

  if (object.hasBoxDecorationBackground()
      // We also paint overflow controls in background phase.
      || (object.hasOverflowClip() &&
          toLayoutBox(object).getScrollableArea()->hasOverflowControls())) {
    context.paintingLayer->setNeedsPaintPhaseDescendantBlockBackgrounds();
  }

  if (object.isTable()) {
    const LayoutTable& table = toLayoutTable(object);
    if (table.collapseBorders() && !table.collapsedBorders().isEmpty())
      context.paintingLayer->setNeedsPaintPhaseDescendantBlockBackgrounds();
  }
}

namespace {

// This is temporary to workaround paint invalidation issues in
// non-rootLayerScrolls mode.
// It undoes FrameView's content clip and scroll for paint invalidation of frame
// scroll controls and the LayoutView to which the content clip and scroll don't
// apply.
class ScopedUndoFrameViewContentClipAndScroll {
 public:
  ScopedUndoFrameViewContentClipAndScroll(const FrameView& frameView,
                                          PaintInvalidatorContext& context)
      : m_treeBuilderContext(const_cast<PaintPropertyTreeBuilderContext&>(
            context.treeBuilderContext)),
        m_savedContext(m_treeBuilderContext.current) {
    DCHECK(!RuntimeEnabledFeatures::rootLayerScrollingEnabled());

    if (frameView.contentClip() == m_savedContext.clip)
      m_treeBuilderContext.current.clip = m_savedContext.clip->parent();
    if (frameView.scroll() == m_savedContext.scroll)
      m_treeBuilderContext.current.scroll = m_savedContext.scroll->parent();
    if (frameView.scrollTranslation() == m_savedContext.transform)
      m_treeBuilderContext.current.transform =
          m_savedContext.transform->parent();
  }

  ~ScopedUndoFrameViewContentClipAndScroll() {
    m_treeBuilderContext.current = m_savedContext;
  }

 private:
  PaintPropertyTreeBuilderContext& m_treeBuilderContext;
  PaintPropertyTreeBuilderContext::ContainingBlockContext m_savedContext;
};

}  // namespace

void PaintInvalidator::updateContext(const LayoutObject& object,
                                     PaintInvalidatorContext& context) {
  Optional<ScopedUndoFrameViewContentClipAndScroll>
      undoFrameViewContentClipAndScroll;

  if (object.isPaintInvalidationContainer()) {
    context.paintInvalidationContainer = toLayoutBoxModelObject(&object);
    if (object.styleRef().isStackingContext())
      context.paintInvalidationContainerForStackedContents =
          toLayoutBoxModelObject(&object);
  } else if (object.isLayoutView()) {
    // paintInvalidationContainerForStackedContents is only for stacked
    // descendants in its own frame, because it doesn't establish stacking
    // context for stacked contents in sub-frames.
    // Contents stacked in the root stacking context in this frame should use
    // this frame's paintInvalidationContainer.
    context.paintInvalidationContainerForStackedContents =
        context.paintInvalidationContainer;
    if (!RuntimeEnabledFeatures::rootLayerScrollingEnabled())
      undoFrameViewContentClipAndScroll.emplace(
          *toLayoutView(object).frameView(), context);
  } else if (object.isFloating() && !object.parent()->isLayoutBlock()) {
    // See LayoutObject::paintingLayer() for specialty of floating objects.
    context.paintInvalidationContainer =
        &object.containerForPaintInvalidation();
  } else if (object.styleRef().isStacked() &&
             // This is to exclude some objects (e.g. LayoutText) inheriting
             // stacked style from parent but aren't actually stacked.
             object.hasLayer() &&
             context.paintInvalidationContainer !=
                 context.paintInvalidationContainerForStackedContents) {
    // The current object is stacked, so we should use
    // m_paintInvalidationContainerForStackedContents as its paint invalidation
    // container on which the current object is painted.
    context.paintInvalidationContainer =
        context.paintInvalidationContainerForStackedContents;
    if (context.forcedSubtreeInvalidationFlags &
        PaintInvalidatorContext::
            ForcedSubtreeFullInvalidationForStackedContents)
      context.forcedSubtreeInvalidationFlags |=
          PaintInvalidatorContext::ForcedSubtreeFullInvalidation;
  }

  if (object == context.paintInvalidationContainer) {
    // When we hit a new paint invalidation container, we don't need to
    // continue forcing a check for paint invalidation, since we're
    // descending into a different invalidation container. (For instance if
    // our parents were moved, the entire container will just move.)
    if (object != context.paintInvalidationContainerForStackedContents) {
      // However, we need to keep the
      // ForcedSubtreeFullInvalidationForStackedContents flag if the current
      // object isn't the paint invalidation container of stacked contents.
      context.forcedSubtreeInvalidationFlags &= PaintInvalidatorContext::
          ForcedSubtreeFullInvalidationForStackedContents;
    } else {
      context.forcedSubtreeInvalidationFlags = 0;
    }
  }

  DCHECK(context.paintInvalidationContainer ==
         object.containerForPaintInvalidation());
  DCHECK(context.paintingLayer == object.paintingLayer());

  if (object.mayNeedPaintInvalidationSubtree())
    context.forcedSubtreeInvalidationFlags |=
        PaintInvalidatorContext::ForcedSubtreeInvalidationChecking;

  // TODO(crbug.com/637313): This is temporary before we support filters in
  // GeometryMapper.
  // TODO(crbug.com/648274): This is a workaround for multi-column contents.
  if (object.hasFilterInducingProperty() || object.isLayoutFlowThread()) {
    context.forcedSubtreeInvalidationFlags |=
        PaintInvalidatorContext::ForcedSubtreeSlowPathRect;
  }

  ObjectPaintInvalidator objectPaintInvalidator(object);
  context.oldVisualRect = object.previousVisualRect();
  context.oldLocation = objectPaintInvalidator.previousLocationInBacking();
  context.newVisualRect = computeVisualRectInBacking(object, context);
  context.newLocation = computeLocationInBacking(object, context);

  IntSize adjustment = object.scrollAdjustmentForPaintInvalidation(
      *context.paintInvalidationContainer);
  context.newLocation.move(adjustment);
  context.newVisualRect.move(adjustment);

  object.getMutableForPainting().setPreviousVisualRect(context.newVisualRect);
  objectPaintInvalidator.setPreviousLocationInBacking(context.newLocation);
}

void PaintInvalidator::invalidatePaintIfNeeded(
    FrameView& frameView,
    PaintInvalidatorContext& context) {
  LayoutView* layoutView = frameView.layoutView();
  CHECK(layoutView);

  context.paintInvalidationContainer =
      context.paintInvalidationContainerForStackedContents =
          &layoutView->containerForPaintInvalidation();
  context.paintingLayer = layoutView->layer();

  if (!RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
    ScopedUndoFrameViewContentClipAndScroll undo(frameView, context);
    frameView.invalidatePaintOfScrollControlsIfNeeded(context);
  }

  frameView.frame().selection().invalidateCaretRect();
}

void PaintInvalidator::invalidatePaintIfNeeded(
    const LayoutObject& object,
    PaintInvalidatorContext& context) {
  object.getMutableForPainting().ensureIsReadyForPaintInvalidation();

  // The paint offset should already be updated through
  // PaintPropertyTreeBuilder::updatePropertiesForSelf.
  DCHECK(context.treeBuilderContext.current.paintOffset ==
         object.paintOffset());

  if (!context.forcedSubtreeInvalidationFlags &&
      !object
           .shouldCheckForPaintInvalidationRegardlessOfPaintInvalidationState())
    return;

  updatePaintingLayer(object, context);

  if (object.document().printing())
    return;  // Don't invalidate paints if we're printing.

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("blink.invalidation"),
               "PaintInvalidator::invalidatePaintIfNeeded()", "object",
               object.debugName().ascii());

  updateContext(object, context);

  if (!object
           .shouldCheckForPaintInvalidationRegardlessOfPaintInvalidationState() &&
      context.forcedSubtreeInvalidationFlags ==
          PaintInvalidatorContext::ForcedSubtreeInvalidationRectUpdate) {
    // We are done updating the visual rect. No other paint invalidation work to
    // do for this object.
    return;
  }

  PaintInvalidationReason reason = object.invalidatePaintIfNeeded(context);
  switch (reason) {
    case PaintInvalidationDelayedFull:
      m_pendingDelayedPaintInvalidations.push_back(&object);
      break;
    case PaintInvalidationSubtree:
      context.forcedSubtreeInvalidationFlags |=
          (PaintInvalidatorContext::ForcedSubtreeFullInvalidation |
           PaintInvalidatorContext::
               ForcedSubtreeFullInvalidationForStackedContents);
      break;
    case PaintInvalidationSVGResourceChange:
      context.forcedSubtreeInvalidationFlags |=
          PaintInvalidatorContext::ForcedSubtreeSVGResourceChange;
      break;
    default:
      break;
  }

  if (context.oldLocation != context.newLocation) {
    context.forcedSubtreeInvalidationFlags |=
        PaintInvalidatorContext::ForcedSubtreeInvalidationChecking;
  }

  // TODO(crbug.com/490725): This is a workaround for the bug, to force
  // descendant to update visual rects on clipping change.
  if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled() &&
      context.oldVisualRect != context.newVisualRect
      // Note that isLayoutView() below becomes unnecessary after the launch of
      // root layer scrolling.
      && (object.hasOverflowClip() || object.isLayoutView()) &&
      !toLayoutBox(object).usesCompositedScrolling())
    context.forcedSubtreeInvalidationFlags |=
        PaintInvalidatorContext::ForcedSubtreeInvalidationRectUpdate;
}

void PaintInvalidator::processPendingDelayedPaintInvalidations() {
  for (auto target : m_pendingDelayedPaintInvalidations)
    target->getMutableForPainting().setShouldDoFullPaintInvalidation(
        PaintInvalidationDelayedFull);
}

}  // namespace blink
