// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/compositing/CompositingReasonFinder.h"

#include "core/CSSPropertyNames.h"
#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutView.h"
#include "core/page/Page.h"
#include "core/paint/PaintLayer.h"

namespace blink {

CompositingReasonFinder::CompositingReasonFinder(LayoutView& layoutView)
    : m_layoutView(layoutView),
      m_compositingTriggers(
          static_cast<CompositingTriggerFlags>(AllCompositingTriggers)) {
  updateTriggers();
}

void CompositingReasonFinder::updateTriggers() {
  m_compositingTriggers = 0;

  Settings& settings = m_layoutView.document().page()->settings();
  if (settings.getPreferCompositingToLCDTextEnabled()) {
    m_compositingTriggers |= ScrollableInnerFrameTrigger;
    m_compositingTriggers |= OverflowScrollTrigger;
    m_compositingTriggers |= ViewportConstrainedPositionedTrigger;
  }
}

bool CompositingReasonFinder::isMainFrame() const {
  return m_layoutView.document().isInMainFrame();
}

CompositingReasons CompositingReasonFinder::directReasons(
    const PaintLayer* layer) const {
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    return CompositingReasonNone;

  DCHECK_EQ(potentialCompositingReasonsFromStyle(layer->layoutObject()),
            layer->potentialCompositingReasonsFromStyle());
  CompositingReasons styleDeterminedDirectCompositingReasons =
      layer->potentialCompositingReasonsFromStyle() &
      CompositingReasonComboAllDirectStyleDeterminedReasons;

  return styleDeterminedDirectCompositingReasons |
         nonStyleDeterminedDirectReasons(layer);
}

// This information doesn't appear to be incorporated into CompositingReasons.
bool CompositingReasonFinder::requiresCompositingForScrollableFrame() const {
  // Need this done first to determine overflow.
  DCHECK(!m_layoutView.needsLayout());
  if (isMainFrame())
    return false;

  if (!(m_compositingTriggers & ScrollableInnerFrameTrigger))
    return false;

  return m_layoutView.frameView()->isScrollable();
}

CompositingReasons
CompositingReasonFinder::potentialCompositingReasonsFromStyle(
    LayoutObject& layoutObject) const {
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    return CompositingReasonNone;

  CompositingReasons reasons = CompositingReasonNone;

  const ComputedStyle& style = layoutObject.styleRef();

  if (requiresCompositingForTransform(layoutObject))
    reasons |= CompositingReason3DTransform;

  if (style.backfaceVisibility() == BackfaceVisibilityHidden)
    reasons |= CompositingReasonBackfaceVisibilityHidden;

  if (requiresCompositingForAnimation(style))
    reasons |= CompositingReasonActiveAnimation;

  if (style.hasWillChangeCompositingHint() &&
      !style.subtreeWillChangeContents())
    reasons |= CompositingReasonWillChangeCompositingHint;

  if (style.hasInlineTransform())
    reasons |= CompositingReasonInlineTransform;

  if (style.usedTransformStyle3D() == TransformStyle3DPreserve3D)
    reasons |= CompositingReasonPreserve3DWith3DDescendants;

  if (style.hasPerspective())
    reasons |= CompositingReasonPerspectiveWith3DDescendants;

  if (style.hasCompositorProxy())
    reasons |= CompositingReasonCompositorProxy;

  // If the implementation of createsGroup changes, we need to be aware of that
  // in this part of code.
  DCHECK((layoutObject.isTransparent() || layoutObject.hasMask() ||
          layoutObject.hasFilterInducingProperty() || style.hasBlendMode()) ==
         layoutObject.createsGroup());

  if (style.hasMask())
    reasons |= CompositingReasonMaskWithCompositedDescendants;

  if (style.hasFilterInducingProperty())
    reasons |= CompositingReasonFilterWithCompositedDescendants;

  if (style.hasBackdropFilter())
    reasons |= CompositingReasonBackdropFilter;

  // See Layer::updateTransform for an explanation of why we check both.
  if (layoutObject.hasTransformRelatedProperty() && style.hasTransform())
    reasons |= CompositingReasonTransformWithCompositedDescendants;

  if (layoutObject.isTransparent())
    reasons |= CompositingReasonOpacityWithCompositedDescendants;

  if (style.hasBlendMode())
    reasons |= CompositingReasonBlendingWithCompositedDescendants;

  if (layoutObject.hasReflection())
    reasons |= CompositingReasonReflectionWithCompositedDescendants;

  DCHECK(!(reasons & ~CompositingReasonComboAllStyleDeterminedReasons));
  return reasons;
}

bool CompositingReasonFinder::requiresCompositingForTransform(
    const LayoutObject& layoutObject) {
  // Note that we ask the layoutObject if it has a transform, because the style
  // may have transforms, but the layoutObject may be an inline that doesn't
  // support them.
  return layoutObject.hasTransformRelatedProperty() &&
         layoutObject.styleRef().has3DTransform();
}

CompositingReasons CompositingReasonFinder::nonStyleDeterminedDirectReasons(
    const PaintLayer* layer) const {
  CompositingReasons directReasons = CompositingReasonNone;
  LayoutObject& layoutObject = layer->layoutObject();

  if (m_compositingTriggers & OverflowScrollTrigger && layer->clipParent())
    directReasons |= CompositingReasonOutOfFlowClipping;

  if (layer->needsCompositedScrolling())
    directReasons |= CompositingReasonOverflowScrollingTouch;

  // Composite |layer| if it is inside of an ancestor scrolling layer, but that
  // scrolling layer is not on the stacking context ancestor chain of |layer|.
  // See the definition of the scrollParent property in Layer for more detail.
  if (const PaintLayer* scrollingAncestor = layer->ancestorScrollingLayer()) {
    if (scrollingAncestor->needsCompositedScrolling() && layer->scrollParent())
      directReasons |= CompositingReasonOverflowScrollingParent;
  }

  // TODO(flackr): Rename functions and variables to include sticky position
  // (i.e. ScrollDependentPosition rather than PositionFixed).
  if (requiresCompositingForScrollDependentPosition(layer))
    directReasons |= CompositingReasonScrollDependentPosition;

  directReasons |= layoutObject.additionalCompositingReasons();

  DCHECK(!(directReasons & CompositingReasonComboAllStyleDeterminedReasons));
  return directReasons;
}

bool CompositingReasonFinder::requiresCompositingForAnimation(
    const ComputedStyle& style) {
  if (style.subtreeWillChangeContents())
    return style.isRunningAnimationOnCompositor();

  return style.shouldCompositeForCurrentAnimations();
}

bool CompositingReasonFinder::requiresCompositingForOpacityAnimation(
    const ComputedStyle& style) {
  return style.subtreeWillChangeContents()
             ? style.isRunningOpacityAnimationOnCompositor()
             : style.hasCurrentOpacityAnimation();
}

bool CompositingReasonFinder::requiresCompositingForFilterAnimation(
    const ComputedStyle& style) {
  return style.subtreeWillChangeContents()
             ? style.isRunningFilterAnimationOnCompositor()
             : style.hasCurrentFilterAnimation();
}

bool CompositingReasonFinder::requiresCompositingForBackdropFilterAnimation(
    const ComputedStyle& style) {
  return style.subtreeWillChangeContents()
             ? style.isRunningBackdropFilterAnimationOnCompositor()
             : style.hasCurrentBackdropFilterAnimation();
}

bool CompositingReasonFinder::requiresCompositingForEffectAnimation(
    const ComputedStyle& style) {
  return requiresCompositingForOpacityAnimation(style) ||
         requiresCompositingForFilterAnimation(style) ||
         requiresCompositingForBackdropFilterAnimation(style);
}

bool CompositingReasonFinder::requiresCompositingForTransformAnimation(
    const ComputedStyle& style) {
  return style.subtreeWillChangeContents()
             ? style.isRunningTransformAnimationOnCompositor()
             : style.hasCurrentTransformAnimation();
}

bool CompositingReasonFinder::requiresCompositingForScrollDependentPosition(
    const PaintLayer* layer) const {
  if (layer->layoutObject().style()->position() != EPosition::kFixed &&
      layer->layoutObject().style()->position() != EPosition::kSticky)
    return false;

  if (!(m_compositingTriggers & ViewportConstrainedPositionedTrigger) &&
      (!RuntimeEnabledFeatures::compositeOpaqueFixedPositionEnabled() ||
       !layer->backgroundIsKnownToBeOpaqueInRect(
           LayoutRect(layer->boundingBoxForCompositing())) ||
       layer->compositesWithTransform() || layer->compositesWithOpacity())) {
    return false;
  }
  // Don't promote fixed position elements that are descendants of a non-view
  // container, e.g. transformed elements.  They will stay fixed wrt the
  // container rather than the enclosing frame.
  if (layer->sticksToViewport())
    return m_layoutView.frameView()->isScrollable();

  if (layer->layoutObject().style()->position() != EPosition::kSticky)
    return false;

  // Don't promote nested sticky elements; the compositor can't handle them.
  // TODO(smcgruer): Add cc nested sticky support (http://crbug.com/672710)
  PaintLayerScrollableArea* scrollableArea =
      layer->ancestorOverflowLayer()->getScrollableArea();
  DCHECK(scrollableArea->stickyConstraintsMap().contains(
      const_cast<PaintLayer*>(layer)));

  return layer->ancestorOverflowLayer()->scrollsOverflow() &&
         !scrollableArea->stickyConstraintsMap()
              .at(const_cast<PaintLayer*>(layer))
              .hasAncestorStickyElement();
}

}  // namespace blink
