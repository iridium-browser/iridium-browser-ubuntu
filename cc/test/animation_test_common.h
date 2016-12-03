// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_ANIMATION_TEST_COMMON_H_
#define CC_TEST_ANIMATION_TEST_COMMON_H_

#include "cc/animation/animation.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/element_id.h"
#include "cc/animation/transform_operations.h"
#include "cc/output/filter_operations.h"
#include "cc/test/geometry_test_utils.h"

namespace cc {
class AnimationPlayer;
class ElementAnimations;
class LayerImpl;
class Layer;
}

namespace gfx {
class ScrollOffset;
}

namespace cc {

class FakeFloatAnimationCurve : public FloatAnimationCurve {
 public:
  FakeFloatAnimationCurve();
  explicit FakeFloatAnimationCurve(double duration);
  ~FakeFloatAnimationCurve() override;

  base::TimeDelta Duration() const override;
  float GetValue(base::TimeDelta now) const override;
  std::unique_ptr<AnimationCurve> Clone() const override;

 private:
  base::TimeDelta duration_;
};

class FakeTransformTransition : public TransformAnimationCurve {
 public:
  explicit FakeTransformTransition(double duration);
  ~FakeTransformTransition() override;

  base::TimeDelta Duration() const override;
  gfx::Transform GetValue(base::TimeDelta time) const override;
  bool AnimatedBoundsForBox(const gfx::BoxF& box,
                            gfx::BoxF* bounds) const override;
  bool AffectsScale() const override;
  bool IsTranslation() const override;
  bool PreservesAxisAlignment() const override;
  bool AnimationStartScale(bool forward_direction,
                           float* start_scale) const override;
  bool MaximumTargetScale(bool forward_direction,
                          float* max_scale) const override;

  std::unique_ptr<AnimationCurve> Clone() const override;

 private:
  base::TimeDelta duration_;
};

class FakeFloatTransition : public FloatAnimationCurve {
 public:
  FakeFloatTransition(double duration, float from, float to);
  ~FakeFloatTransition() override;

  base::TimeDelta Duration() const override;
  float GetValue(base::TimeDelta time) const override;

  std::unique_ptr<AnimationCurve> Clone() const override;

 private:
  base::TimeDelta duration_;
  float from_;
  float to_;
};

int AddScrollOffsetAnimationToElementAnimations(ElementAnimations* target,
                                                gfx::ScrollOffset initial_value,
                                                gfx::ScrollOffset target_value,
                                                bool impl_only);

int AddOpacityTransitionToElementAnimations(ElementAnimations* target,
                                            double duration,
                                            float start_opacity,
                                            float end_opacity,
                                            bool use_timing_function);

int AddAnimatedTransformToElementAnimations(ElementAnimations* target,
                                            double duration,
                                            int delta_x,
                                            int delta_y);

int AddAnimatedFilterToElementAnimations(ElementAnimations* target,
                                         double duration,
                                         float start_brightness,
                                         float end_brightness);

int AddAnimatedTransformToPlayer(AnimationPlayer* player,
                                 double duration,
                                 int delta_x,
                                 int delta_y);

int AddAnimatedTransformToPlayer(AnimationPlayer* player,
                                 double duration,
                                 TransformOperations start_operations,
                                 TransformOperations operations);

int AddOpacityTransitionToPlayer(AnimationPlayer* player,
                                 double duration,
                                 float start_opacity,
                                 float end_opacity,
                                 bool use_timing_function);

int AddAnimatedFilterToPlayer(AnimationPlayer* player,
                              double duration,
                              float start_brightness,
                              float end_brightness);

int AddOpacityStepsToElementAnimations(ElementAnimations* target,
                                       double duration,
                                       float start_opacity,
                                       float end_opacity,
                                       int num_steps);

void AddAnimationToElementWithPlayer(ElementId element_id,
                                     scoped_refptr<AnimationTimeline> timeline,
                                     std::unique_ptr<Animation> animation);
void AddAnimationToElementWithExistingPlayer(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    std::unique_ptr<Animation> animation);

void RemoveAnimationFromElementWithExistingPlayer(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    int animation_id);

Animation* GetAnimationFromElementWithExistingPlayer(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    int animation_id);

int AddAnimatedFilterToElementWithPlayer(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    double duration,
    float start_brightness,
    float end_brightness);

int AddAnimatedTransformToElementWithPlayer(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    double duration,
    int delta_x,
    int delta_y);

int AddAnimatedTransformToElementWithPlayer(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    double duration,
    TransformOperations start_operations,
    TransformOperations operations);

int AddOpacityTransitionToElementWithPlayer(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    double duration,
    float start_opacity,
    float end_opacity,
    bool use_timing_function);

void AbortAnimationsOnElementWithPlayer(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    TargetProperty::Type target_property);

}  // namespace cc

#endif  // CC_TEST_ANIMATION_TEST_COMMON_H_
