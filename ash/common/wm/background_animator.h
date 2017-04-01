// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_BACKGROUND_ANIMATOR_H_
#define ASH_COMMON_WM_BACKGROUND_ANIMATOR_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"

namespace ash {

// How the background can be changed.
enum BackgroundAnimatorChangeType {
  BACKGROUND_CHANGE_ANIMATE,
  BACKGROUND_CHANGE_IMMEDIATE
};

class BackgroundAnimator;

// Delegate is notified any time the background changes.
class ASH_EXPORT BackgroundAnimatorDelegate {
 public:
  virtual void UpdateBackground(BackgroundAnimator* animator, int alpha) = 0;

  virtual void BackgroundAnimationEnded(BackgroundAnimator* animator) = 0;

 protected:
  virtual ~BackgroundAnimatorDelegate() {}
};

// BackgroundAnimator is used by the shelf to animate the background (alpha).
class ASH_EXPORT BackgroundAnimator : public gfx::AnimationDelegate {
 public:
  BackgroundAnimator(BackgroundAnimatorDelegate* delegate,
                     int min_alpha,
                     int max_alpha);
  ~BackgroundAnimator() override;

  // Sets the transition time in ms.
  void SetDuration(int time_in_ms);

  // Stops the animation. Does nothing if the animation is not running.
  void Stop();

  // Sets whether a background is rendered. Initial value is false. If |type|
  // is |CHANGE_IMMEDIATE| and an animation is not in progress this notifies
  // the delegate immediately (synchronously from this method).
  void SetPaintsBackground(bool value, BackgroundAnimatorChangeType type);
  bool paints_background() const { return paints_background_; }

  // Current alpha.
  int alpha() const { return alpha_; }

  // gfx::AnimationDelegate overrides:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

 private:
  BackgroundAnimatorDelegate* delegate_;

  const int min_alpha_;
  const int max_alpha_;

  gfx::SlideAnimation animation_;

  // Whether the background is painted.
  bool paints_background_;

  // Current alpha value of the background.
  int alpha_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundAnimator);
};

}  // namespace ash

#endif  // ASH_COMMON_WM_BACKGROUND_ANIMATOR_H_
