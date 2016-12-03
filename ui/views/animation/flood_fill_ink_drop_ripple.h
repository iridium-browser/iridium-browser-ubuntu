// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_FLOOD_FILL_INK_DROP_RIPPLE_H_
#define UI_VIEWS_ANIMATION_FLOOD_FILL_INK_DROP_RIPPLE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/transform.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/views_export.h"

namespace ui {
class Layer;
}  // namespace ui

namespace views {
class CircleLayerDelegate;

namespace test {
class FloodFillInkDropRippleTestApi;
}  // namespace test

// An ink drop ripple that starts as a small circle and flood fills a
// rectangle of the given size. The circle is clipped to the rectangles bounds.
//
// The valid InkDropState transitions are defined below:
//
//   {All InkDropStates}      => HIDDEN
//   HIDDEN                   => ACTION_PENDING
//   HIDDEN, ACTION_PENDING   => ACTION_TRIGGERED
//   ACTION_PENDING           => ALTERNATE_ACTION_PENDING
//   ALTERNATE_ACTION_PENDING => ALTERNATE_ACTION_TRIGGERED
//   {All InkDropStates}      => ACTIVATED
//   {All InkDropStates}      => DEACTIVATED
//
class VIEWS_EXPORT FloodFillInkDropRipple : public InkDropRipple {
 public:
  FloodFillInkDropRipple(const gfx::Rect& clip_bounds,
                         const gfx::Point& center_point,
                         SkColor color,
                         float visible_opacity);
  ~FloodFillInkDropRipple() override;

  // InkDropRipple:
  void SnapToActivated() override;
  ui::Layer* GetRootLayer() override;
  bool IsVisible() const override;

 private:
  friend class test::FloodFillInkDropRippleTestApi;

  // InkDropRipple:
  void AnimateStateChange(InkDropState old_ink_drop_state,
                          InkDropState new_ink_drop_state,
                          ui::LayerAnimationObserver* observer) override;
  void SetStateToHidden() override;
  void AbortAllAnimations() override;

  // Animates the |painted_layer_| to the specified |transform|. The animation
  // will be configured with the given |duration|, |tween|, and
  // |preemption_strategy| values. The |observer| will be added to all
  // LayerAnimationSequences if not null.
  void AnimateToTransform(
      const gfx::Transform& transform,
      base::TimeDelta duration,
      ui::LayerAnimator::PreemptionStrategy preemption_strategy,
      gfx::Tween::Type tween,
      ui::LayerAnimationObserver* observer);

  // Sets the opacity of the ink drop. Note that this does not perform any
  // animation.
  void SetOpacity(float opacity);

  // Animates the |painted_layer_| to the specified |opacity|. The animation
  // will be configured with the given |duration|, |tween|, and
  // |preemption_strategy| values. The |observer| will be added to all
  // LayerAnimationSequences if not null.
  void AnimateToOpacity(
      float opacity,
      base::TimeDelta duration,
      ui::LayerAnimator::PreemptionStrategy preemption_strategy,
      gfx::Tween::Type tween,
      ui::LayerAnimationObserver* observer);

  // Returns the Transform to be applied to the |painted_layer_| for the given
  // |target_radius|.
  gfx::Transform CalculateTransform(float target_radius) const;

  // Returns the target Transform for when the ink drop is fully shown.
  gfx::Transform GetMaxSizeTargetTransform() const;

  // Returns the largest distance from |point| to the corners of the
  // |root_layer_| bounds.
  float MaxDistanceToCorners(const gfx::Point& point) const;

  // The point where the Center of the ink drop's circle should be drawn.
  gfx::Point center_point_;

  // Ink drop opacity when it is visible.
  float visible_opacity_;

  // The root layer that parents the animating layer. The root layer is used to
  // manipulate opacity and clipping bounds, and it child is used to manipulate
  // the different shape of the ink drop.
  ui::Layer root_layer_;

  // ui::LayerDelegate to paint the |painted_layer_|.
  CircleLayerDelegate circle_layer_delegate_;

  // Child ui::Layer of |root_layer_|. Used to  manipulate the different size
  // and shape of the ink drop.
  ui::Layer painted_layer_;

  // The current ink drop state.
  InkDropState ink_drop_state_;

  DISALLOW_COPY_AND_ASSIGN(FloodFillInkDropRipple);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_FLOOD_FILL_INK_DROP_RIPPLE_H_
