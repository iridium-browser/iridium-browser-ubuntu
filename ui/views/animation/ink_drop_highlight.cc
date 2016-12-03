// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_highlight.h"

#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/animation/ink_drop_highlight_observer.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"

namespace views {

namespace {

// The opacity of the highlight when it is not visible.
const float kHiddenOpacity = 0.0f;

}  // namespace

std::string ToString(InkDropHighlight::AnimationType animation_type) {
  switch (animation_type) {
    case InkDropHighlight::FADE_IN:
      return std::string("FADE_IN");
    case InkDropHighlight::FADE_OUT:
      return std::string("FADE_OUT");
  }
  NOTREACHED()
      << "Should never be reached but is necessary for some compilers.";
  return std::string("UNKNOWN");
}

InkDropHighlight::InkDropHighlight(
    const gfx::PointF& center_point,
    std::unique_ptr<BasePaintedLayerDelegate> layer_delegate)
    : center_point_(center_point),
      visible_opacity_(1.f),
      last_animation_initiated_was_fade_in_(false),
      layer_delegate_(std::move(layer_delegate)),
      layer_(new ui::Layer()),
      observer_(nullptr) {
  const gfx::Rect layer_bounds = layer_delegate_->GetPaintedBounds();
  size_ = explode_size_ = layer_bounds.size();

  layer_->SetBounds(layer_bounds);
  layer_->SetFillsBoundsOpaquely(false);
  layer_->set_delegate(layer_delegate_.get());
  layer_->SetVisible(false);
  layer_->SetMasksToBounds(false);
  layer_->set_name("InkDropHighlight:layer");
}

InkDropHighlight::InkDropHighlight(const gfx::Size& size,
                                   int corner_radius,
                                   const gfx::PointF& center_point,
                                   SkColor color)
    : InkDropHighlight(
          center_point,
          std::unique_ptr<BasePaintedLayerDelegate>(
              new RoundedRectangleLayerDelegate(color, size, corner_radius))) {
  visible_opacity_ = 0.128f;
  layer_->SetOpacity(visible_opacity_);
}

InkDropHighlight::~InkDropHighlight() {}

bool InkDropHighlight::IsFadingInOrVisible() const {
  return last_animation_initiated_was_fade_in_;
}

void InkDropHighlight::FadeIn(const base::TimeDelta& duration) {
  layer_->SetOpacity(kHiddenOpacity);
  layer_->SetVisible(true);
  AnimateFade(FADE_IN, duration, size_, size_);
}

void InkDropHighlight::FadeOut(const base::TimeDelta& duration, bool explode) {
  AnimateFade(FADE_OUT, duration, size_, explode ? explode_size_ : size_);
}

test::InkDropHighlightTestApi* InkDropHighlight::GetTestApi() {
  return nullptr;
}

void InkDropHighlight::AnimateFade(AnimationType animation_type,
                                   const base::TimeDelta& duration,
                                   const gfx::Size& initial_size,
                                   const gfx::Size& target_size) {
  last_animation_initiated_was_fade_in_ = animation_type == FADE_IN;

  layer_->SetTransform(CalculateTransform(initial_size));

  // The |animation_observer| will be destroyed when the
  // AnimationStartedCallback() returns true.
  ui::CallbackLayerAnimationObserver* animation_observer =
      new ui::CallbackLayerAnimationObserver(
          base::Bind(&InkDropHighlight::AnimationStartedCallback,
                     base::Unretained(this), animation_type),
          base::Bind(&InkDropHighlight::AnimationEndedCallback,
                     base::Unretained(this), animation_type));

  ui::LayerAnimator* animator = layer_->GetAnimator();
  ui::ScopedLayerAnimationSettings animation(animator);
  animation.SetTweenType(gfx::Tween::EASE_IN_OUT);
  animation.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  ui::LayerAnimationElement* opacity_element =
      ui::LayerAnimationElement::CreateOpacityElement(
          animation_type == FADE_IN ? visible_opacity_ : kHiddenOpacity,
          duration);
  ui::LayerAnimationSequence* opacity_sequence =
      new ui::LayerAnimationSequence(opacity_element);
  opacity_sequence->AddObserver(animation_observer);
  animator->StartAnimation(opacity_sequence);

  if (initial_size != target_size) {
    ui::LayerAnimationElement* transform_element =
        ui::LayerAnimationElement::CreateTransformElement(
            CalculateTransform(target_size), duration);
    ui::LayerAnimationSequence* transform_sequence =
        new ui::LayerAnimationSequence(transform_element);
    transform_sequence->AddObserver(animation_observer);
    animator->StartAnimation(transform_sequence);
  }

  animation_observer->SetActive();
}

gfx::Transform InkDropHighlight::CalculateTransform(
    const gfx::Size& size) const {
  gfx::Transform transform;
  transform.Translate(center_point_.x(), center_point_.y());
  transform.Scale(size.width() / size_.width(), size.height() / size_.height());
  gfx::Vector2dF layer_offset = layer_delegate_->GetCenteringOffset();
  transform.Translate(-layer_offset.x(), -layer_offset.y());
  return transform;
}

void InkDropHighlight::AnimationStartedCallback(
    AnimationType animation_type,
    const ui::CallbackLayerAnimationObserver& observer) {
  if (observer_)
    observer_->AnimationStarted(animation_type);
}

bool InkDropHighlight::AnimationEndedCallback(
    AnimationType animation_type,
    const ui::CallbackLayerAnimationObserver& observer) {
  // AnimationEndedCallback() may be invoked when this is being destroyed and
  // |layer_| may be null.
  if (animation_type == FADE_OUT && layer_)
    layer_->SetVisible(false);

  if (observer_) {
    observer_->AnimationEnded(animation_type,
                              observer.aborted_count()
                                  ? InkDropAnimationEndedReason::PRE_EMPTED
                                  : InkDropAnimationEndedReason::SUCCESS);
  }
  return true;
}

}  // namespace views
