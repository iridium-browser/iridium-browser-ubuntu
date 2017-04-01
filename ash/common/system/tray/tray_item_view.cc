// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tray_item_view.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_item.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/public/cpp/shelf_types.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace {
const int kTrayIconHeight = 29;
const int kTrayIconWidth = 29;
const int kTrayItemAnimationDurationMS = 200;

// Animations can be disabled for testing.
bool animations_enabled = true;
}

namespace ash {

TrayItemView::TrayItemView(SystemTrayItem* owner)
    : owner_(owner), label_(NULL), image_view_(NULL) {
  SetPaintToLayer(true);
  layer()->SetFillsBoundsOpaquely(false);
  SetLayoutManager(new views::FillLayout());
}

TrayItemView::~TrayItemView() {}

// static
void TrayItemView::DisableAnimationsForTest() {
  animations_enabled = false;
}

void TrayItemView::CreateLabel() {
  label_ = new views::Label;
  AddChildView(label_);
  PreferredSizeChanged();
}

void TrayItemView::CreateImageView() {
  image_view_ = new views::ImageView;
  AddChildView(image_view_);
  PreferredSizeChanged();
}

void TrayItemView::SetVisible(bool set_visible) {
  if (!GetWidget() || !animations_enabled) {
    views::View::SetVisible(set_visible);
    return;
  }

  if (!animation_) {
    animation_.reset(new gfx::SlideAnimation(this));
    animation_->SetSlideDuration(GetAnimationDurationMS());
    animation_->SetTweenType(gfx::Tween::LINEAR);
    animation_->Reset(visible() ? 1.0 : 0.0);
  }

  if (!set_visible) {
    animation_->Hide();
    AnimationProgressed(animation_.get());
  } else {
    animation_->Show();
    AnimationProgressed(animation_.get());
    views::View::SetVisible(true);
  }
}

// static
bool TrayItemView::UseMd() {
  return MaterialDesignController::UseMaterialDesignSystemIcons();
}

int TrayItemView::GetAnimationDurationMS() {
  return kTrayItemAnimationDurationMS;
}

gfx::Size TrayItemView::GetPreferredSize() const {
  DCHECK_EQ(1, child_count());
  gfx::Size size;
  if (UseMd()) {
    gfx::Size inner_size = views::View::GetPreferredSize();
    if (image_view_)
      inner_size = gfx::Size(kTrayIconSize, kTrayIconSize);
    gfx::Rect rect(inner_size);
    rect.Inset(gfx::Insets(-GetTrayConstant(TRAY_IMAGE_ITEM_PADDING)));
    size = rect.size();
  } else {
    size = views::View::GetPreferredSize();
    if (IsHorizontalAlignment(owner()->system_tray()->shelf_alignment()))
      size.set_height(kTrayIconHeight);
    else
      size.set_width(kTrayIconWidth);
  }
  if (!animation_.get() || !animation_->is_animating())
    return size;
  if (IsHorizontalAlignment(owner()->system_tray()->shelf_alignment())) {
    size.set_width(std::max(
        1, static_cast<int>(size.width() * animation_->GetCurrentValue())));
  } else {
    size.set_height(std::max(
        1, static_cast<int>(size.height() * animation_->GetCurrentValue())));
  }
  return size;
}

int TrayItemView::GetHeightForWidth(int width) const {
  return GetPreferredSize().height();
}

void TrayItemView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void TrayItemView::AnimationProgressed(const gfx::Animation* animation) {
  gfx::Transform transform;
  if (IsHorizontalAlignment(owner()->system_tray()->shelf_alignment())) {
    transform.Translate(0, animation->CurrentValueBetween(
                               static_cast<double>(height()) / 2, 0.));
  } else {
    transform.Translate(
        animation->CurrentValueBetween(static_cast<double>(width() / 2), 0.),
        0);
  }
  transform.Scale(animation->GetCurrentValue(), animation->GetCurrentValue());
  layer()->SetTransform(transform);
  PreferredSizeChanged();
}

void TrayItemView::AnimationEnded(const gfx::Animation* animation) {
  if (animation->GetCurrentValue() < 0.1)
    views::View::SetVisible(false);
}

void TrayItemView::AnimationCanceled(const gfx::Animation* animation) {
  AnimationEnded(animation);
}

}  // namespace ash
