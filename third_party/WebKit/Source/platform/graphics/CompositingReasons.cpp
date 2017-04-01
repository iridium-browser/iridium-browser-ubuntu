// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CompositingReasons.h"

#include "wtf/StdLibExtras.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

const CompositingReasonStringMap kCompositingReasonStringMap[] = {
    {CompositingReasonNone, "Unknown", "No reason given"},
    {CompositingReason3DTransform, "transform3D", "Has a 3d transform"},
    {CompositingReasonVideo, "video", "Is an accelerated video"},
    {CompositingReasonCanvas, "canvas",
     "Is an accelerated canvas, or is a display list backed canvas that was "
     "promoted to a layer based on a performance heuristic."},
    {CompositingReasonPlugin, "plugin", "Is an accelerated plugin"},
    {CompositingReasonIFrame, "iFrame", "Is an accelerated iFrame"},
    {CompositingReasonBackfaceVisibilityHidden, "backfaceVisibilityHidden",
     "Has backface-visibility: hidden"},
    {CompositingReasonActiveAnimation, "activeAnimation",
     "Has an active accelerated animation or transition"},
    {CompositingReasonTransitionProperty, "transitionProperty",
     "Has an acceleratable transition property (active or inactive)"},
    {CompositingReasonScrollDependentPosition, "scrollDependentPosition",
     "Is fixed or sticky position"},
    {CompositingReasonOverflowScrollingTouch, "overflowScrollingTouch",
     "Is a scrollable overflow element"},
    {CompositingReasonOverflowScrollingParent, "overflowScrollingParent",
     "Scroll parent is not an ancestor"},
    {CompositingReasonOutOfFlowClipping, "outOfFlowClipping",
     "Has clipping ancestor"},
    {CompositingReasonVideoOverlay, "videoOverlay",
     "Is overlay controls for video"},
    {CompositingReasonWillChangeCompositingHint, "willChange",
     "Has a will-change compositing hint"},
    {CompositingReasonBackdropFilter, "backdropFilter",
     "Has a backdrop filter"},
    {CompositingReasonCompositorProxy, "compositorProxy",
     "Has a CompositorProxy object"},
    {CompositingReasonAssumedOverlap, "assumedOverlap",
     "Might overlap other composited content"},
    {CompositingReasonOverlap, "overlap", "Overlaps other composited content"},
    {CompositingReasonNegativeZIndexChildren, "negativeZIndexChildren",
     "Parent with composited negative z-index content"},
    {CompositingReasonSquashingDisallowed, "squashingDisallowed",
     "Layer was separately composited because it could not be squashed."},
    {CompositingReasonTransformWithCompositedDescendants,
     "transformWithCompositedDescendants",
     "Has a transform that needs to be known by compositor because of "
     "composited descendants"},
    {CompositingReasonOpacityWithCompositedDescendants,
     "opacityWithCompositedDescendants",
     "Has opacity that needs to be applied by compositor because of composited "
     "descendants"},
    {CompositingReasonMaskWithCompositedDescendants,
     "maskWithCompositedDescendants",
     "Has a mask that needs to be known by compositor because of composited "
     "descendants"},
    {CompositingReasonReflectionWithCompositedDescendants,
     "reflectionWithCompositedDescendants",
     "Has a reflection that needs to be known by compositor because of "
     "composited descendants"},
    {CompositingReasonFilterWithCompositedDescendants,
     "filterWithCompositedDescendants",
     "Has a filter effect that needs to be known by compositor because of "
     "composited descendants"},
    {CompositingReasonBlendingWithCompositedDescendants,
     "blendingWithCompositedDescendants",
     "Has a blending effect that needs to be known by compositor because of "
     "composited descendants"},
    {CompositingReasonClipsCompositingDescendants,
     "clipsCompositingDescendants",
     "Has a clip that needs to be known by compositor because of composited "
     "descendants"},
    {CompositingReasonPerspectiveWith3DDescendants,
     "perspectiveWith3DDescendants",
     "Has a perspective transform that needs to be known by compositor because "
     "of 3d descendants"},
    {CompositingReasonPreserve3DWith3DDescendants,
     "preserve3DWith3DDescendants",
     "Has a preserves-3d property that needs to be known by compositor because "
     "of 3d descendants"},
    {CompositingReasonReflectionOfCompositedParent,
     "reflectionOfCompositedParent", "Is a reflection of a composited layer"},
    {CompositingReasonIsolateCompositedDescendants,
     "isolateCompositedDescendants",
     "Should isolate descendants to apply a blend effect"},
    {CompositingReasonPositionFixedWithCompositedDescendants,
     "positionFixedWithCompositedDescendants"
     "Is a position:fixed element with composited descendants"},
    {CompositingReasonRoot, "root", "Is the root layer"},
    {CompositingReasonLayerForAncestorClip, "layerForAncestorClip",
     "Secondary layer, applies a clip due to a sibling in the compositing "
     "tree"},
    {CompositingReasonLayerForDescendantClip, "layerForDescendantClip",
     "Secondary layer, to clip descendants of the owning layer"},
    {CompositingReasonLayerForPerspective, "layerForPerspective",
     "Secondary layer, to house the perspective transform for all descendants"},
    {CompositingReasonLayerForHorizontalScrollbar,
     "layerForHorizontalScrollbar",
     "Secondary layer, the horizontal scrollbar layer"},
    {CompositingReasonLayerForVerticalScrollbar, "layerForVerticalScrollbar",
     "Secondary layer, the vertical scrollbar layer"},
    {CompositingReasonLayerForOverflowControlsHost,
     "layerForOverflowControlsHost",
     "Secondary layer, the overflow controls host layer"},
    {CompositingReasonLayerForScrollCorner, "layerForScrollCorner",
     "Secondary layer, the scroll corner layer"},
    {CompositingReasonLayerForScrollingContents, "layerForScrollingContents",
     "Secondary layer, to house contents that can be scrolled"},
    {CompositingReasonLayerForScrollingContainer, "layerForScrollingContainer",
     "Secondary layer, used to position the scrolling contents while "
     "scrolling"},
    {CompositingReasonLayerForSquashingContents, "layerForSquashingContents",
     "Secondary layer, home for a group of squashable content"},
    {CompositingReasonLayerForSquashingContainer, "layerForSquashingContainer",
     "Secondary layer, no-op layer to place the squashing layer correctly in "
     "the composited layer tree"},
    {CompositingReasonLayerForForeground, "layerForForeground",
     "Secondary layer, to contain any normal flow and positive z-index "
     "contents on top of a negative z-index layer"},
    {CompositingReasonLayerForBackground, "layerForBackground",
     "Secondary layer, to contain acceleratable background content"},
    {CompositingReasonLayerForMask, "layerForMask",
     "Secondary layer, to contain the mask contents"},
    {CompositingReasonLayerForClippingMask, "layerForClippingMask",
     "Secondary layer, for clipping mask"},
    {CompositingReasonLayerForAncestorClippingMask,
     "layerForAncestorClippingMask",
     "Secondary layer, applies a clipping mask due to a sibling in the "
     "composited layer tree"},
    {CompositingReasonLayerForScrollingBlockSelection,
     "layerForScrollingBlockSelection",
     "Secondary layer, to house block selection gaps for composited scrolling "
     "with no scrolling contents"},
    {CompositingReasonLayerForDecoration, "layerForDecoration",
     "Layer painted on top of other layers as decoration"},
    {CompositingReasonInlineTransform, "inlineTransform",
     "Has an inline transform, which causes subsequent layers to assume "
     "overlap"},
};

const size_t kNumberOfCompositingReasons =
    WTF_ARRAY_LENGTH(kCompositingReasonStringMap);

String compositingReasonsAsString(CompositingReasons reasons) {
  if (!reasons)
    return "none";

  StringBuilder builder;
  for (size_t i = 0; i < kNumberOfCompositingReasons; ++i) {
    if (reasons & kCompositingReasonStringMap[i].reason) {
      if (builder.length())
        builder.append(',');
      builder.append(kCompositingReasonStringMap[i].shortName);
    }
  }
  return builder.toString();
}

}  // namespace blink
