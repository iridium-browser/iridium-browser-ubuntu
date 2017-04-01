// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/window_dimmer.h"

#include <memory>

#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/wm/core/window_animations.h"

namespace ash {
namespace {

const int kDefaultDimAnimationDurationMs = 200;

const float kDefaultDimOpacity = 0.5f;

}  // namespace

WindowDimmer::WindowDimmer(WmWindow* parent)
    : parent_(parent),
      window_(WmShell::Get()->NewWindow(ui::wm::WINDOW_TYPE_NORMAL,
                                        ui::LAYER_SOLID_COLOR)) {
  window_->SetVisibilityChangesAnimated();
  window_->SetVisibilityAnimationType(
      ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  window_->SetVisibilityAnimationDuration(
      base::TimeDelta::FromMilliseconds(kDefaultDimAnimationDurationMs));
  window_->AddObserver(this);

  SetDimOpacity(kDefaultDimOpacity);

  parent->AddChild(window_);
  parent->AddObserver(this);
  parent->StackChildAtTop(window_);

  window_->SetBounds(gfx::Rect(parent_->GetBounds().size()));
}

WindowDimmer::~WindowDimmer() {
  if (parent_)
    parent_->RemoveObserver(this);
  if (window_) {
    window_->RemoveObserver(this);
    window_->Destroy();
  }
}

void WindowDimmer::SetDimOpacity(float target_opacity) {
  DCHECK(window_);
  window_->GetLayer()->SetColor(
      SkColorSetA(SK_ColorBLACK, 255 * target_opacity));
}

void WindowDimmer::OnWindowBoundsChanged(WmWindow* window,
                                         const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds) {
  if (window == parent_)
    window_->SetBounds(gfx::Rect(new_bounds.size()));
}

void WindowDimmer::OnWindowDestroying(WmWindow* window) {
  if (window == parent_) {
    parent_->RemoveObserver(this);
    parent_ = nullptr;
  } else {
    DCHECK_EQ(window_, window);
    window_->RemoveObserver(this);
    window_ = nullptr;
  }
}

void WindowDimmer::OnWindowTreeChanging(WmWindow* window,
                                        const TreeChangeParams& params) {
  if (window == window_ && params.target == window) {
    // This may happen on a display change or some unexpected condition. Hide
    // the window to ensure it isn't obscuring the wrong thing.
    window_->Hide();
  }
}

}  // namespace ash
