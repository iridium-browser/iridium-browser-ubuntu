// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rotator/screen_rotation_animation.h"

#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/transform.h"

namespace ash {

ScreenRotationAnimation::ScreenRotationAnimation(
    ui::Layer* layer,
    int start_degrees,
    int end_degrees,
    float initial_opacity,
    float target_opacity,
    const gfx::Point3F& initial_scale,
    const gfx::Point3F& target_scale,
    gfx::Point pivot,
    base::TimeDelta duration,
    gfx::Tween::Type tween_type)
    : ui::LayerAnimationElement(
          LayerAnimationElement::TRANSFORM | LayerAnimationElement::OPACITY,
          duration),
      tween_type_(tween_type),
      initial_opacity_(initial_opacity),
      target_opacity_(target_opacity) {
  scoped_ptr<ui::InterpolatedTransform> scale(
      new ui::InterpolatedTransformAboutPivot(
          pivot, new ui::InterpolatedScale(initial_scale, target_scale)));

  scoped_ptr<ui::InterpolatedTransform> rotation(
      new ui::InterpolatedTransformAboutPivot(
          pivot, new ui::InterpolatedRotation(start_degrees, end_degrees)));

  // Use the target transform/bounds in case the layer is already animating.
  gfx::Transform current_transform = layer->GetTargetTransform();
  interpolated_transform_.reset(
      new ui::InterpolatedConstantTransform(current_transform));
  scale->SetChild(rotation.release());
  interpolated_transform_->SetChild(scale.release());
}

ScreenRotationAnimation::~ScreenRotationAnimation() {
}

void ScreenRotationAnimation::OnStart(ui::LayerAnimationDelegate* delegate) {
}

bool ScreenRotationAnimation::OnProgress(double current,
                                         ui::LayerAnimationDelegate* delegate) {
  const double tweened = gfx::Tween::CalculateValue(tween_type_, current);
  delegate->SetTransformFromAnimation(
      interpolated_transform_->Interpolate(tweened));
  delegate->SetOpacityFromAnimation(gfx::Tween::FloatValueBetween(
      tweened, initial_opacity_, target_opacity_));
  return true;
}

void ScreenRotationAnimation::OnGetTarget(TargetValue* target) const {
  target->transform = interpolated_transform_->Interpolate(1.0);
}

void ScreenRotationAnimation::OnAbort(ui::LayerAnimationDelegate* delegate) {
}

}  // namespace ash
