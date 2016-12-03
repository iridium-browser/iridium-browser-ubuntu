// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/web_notification/ash_popup_alignment_delegate.h"

#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/shelf_types.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "base/i18n/rtl.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/views/message_popup_collection.h"

namespace ash {

namespace {

const int kToastMarginX = 3;

// If there should be no margin for the first item, this value needs to be
// substracted to flush the message to the shelf (the width of the border +
// shadow).
const int kNoToastMarginBorderAndShadowOffset = 2;

}  // namespace

AshPopupAlignmentDelegate::AshPopupAlignmentDelegate(WmShelf* shelf)
    : screen_(NULL), shelf_(shelf), tray_bubble_height_(0) {
  shelf_->AddObserver(this);
}

AshPopupAlignmentDelegate::~AshPopupAlignmentDelegate() {
  if (screen_)
    screen_->RemoveObserver(this);
  WmShell::Get()->RemoveShellObserver(this);
  shelf_->RemoveObserver(this);
}

void AshPopupAlignmentDelegate::StartObserving(
    display::Screen* screen,
    const display::Display& display) {
  screen_ = screen;
  work_area_ = display.work_area();
  screen->AddObserver(this);
  WmShell::Get()->AddShellObserver(this);
  if (tray_bubble_height_ > 0)
    UpdateWorkArea();
}

void AshPopupAlignmentDelegate::SetTrayBubbleHeight(int height) {
  tray_bubble_height_ = height;

  // If the shelf is shown during auto-hide state, the distance from the edge
  // should be reduced by the height of shelf's shown height.
  if (shelf_->GetVisibilityState() == SHELF_AUTO_HIDE &&
      shelf_->GetAutoHideState() == SHELF_AUTO_HIDE_SHOWN) {
    tray_bubble_height_ -= GetShelfConstant(SHELF_SIZE) -
                           GetShelfConstant(SHELF_INSETS_FOR_AUTO_HIDE);
  }

  if (tray_bubble_height_ > 0)
    tray_bubble_height_ += message_center::kMarginBetweenItems;
  else
    tray_bubble_height_ = 0;

  DoUpdateIfPossible();
}

int AshPopupAlignmentDelegate::GetToastOriginX(
    const gfx::Rect& toast_bounds) const {
  // In Ash, RTL UI language mirrors the whole ash layout, so the toast
  // widgets should be at the bottom-left instead of bottom right.
  if (base::i18n::IsRTL())
    return work_area_.x() + kToastMarginX;

  if (IsFromLeft())
    return work_area_.x() + kToastMarginX;
  return work_area_.right() - kToastMarginX - toast_bounds.width();
}

int AshPopupAlignmentDelegate::GetBaseLine() const {
  return work_area_.bottom() - kNoToastMarginBorderAndShadowOffset -
         tray_bubble_height_;
}

int AshPopupAlignmentDelegate::GetWorkAreaBottom() const {
  return work_area_.bottom() - tray_bubble_height_;
}

bool AshPopupAlignmentDelegate::IsTopDown() const {
  return false;
}

bool AshPopupAlignmentDelegate::IsFromLeft() const {
  return GetAlignment() == SHELF_ALIGNMENT_LEFT;
}

void AshPopupAlignmentDelegate::RecomputeAlignment(
    const display::Display& display) {
  // Nothing needs to be done.
}

void AshPopupAlignmentDelegate::ConfigureWidgetInitParamsForContainer(
    views::Widget* widget,
    views::Widget::InitParams* init_params) {
  // On ash, popups go in the status container.
  shelf_->GetWindow()
      ->GetRootWindowController()
      ->ConfigureWidgetInitParamsForContainer(
          widget, kShellWindowId_StatusContainer, init_params);
}

ShelfAlignment AshPopupAlignmentDelegate::GetAlignment() const {
  return shelf_->GetAlignment();
}

display::Display AshPopupAlignmentDelegate::GetCurrentDisplay() const {
  return shelf_->GetWindow()->GetDisplayNearestWindow();
}

void AshPopupAlignmentDelegate::UpdateWorkArea() {
  work_area_ = shelf_->GetUserWorkAreaBounds();
  DoUpdateIfPossible();
}

///////////////////////////////////////////////////////////////////////////////
// WmShelfObserver:

void AshPopupAlignmentDelegate::WillChangeVisibilityState(
    ShelfVisibilityState new_state) {
  UpdateWorkArea();
}

void AshPopupAlignmentDelegate::OnAutoHideStateChanged(
    ShelfAutoHideState new_state) {
  UpdateWorkArea();
}

///////////////////////////////////////////////////////////////////////////////
// display::DisplayObserver:

void AshPopupAlignmentDelegate::OnDisplayAdded(
    const display::Display& new_display) {}

void AshPopupAlignmentDelegate::OnDisplayRemoved(
    const display::Display& old_display) {}

void AshPopupAlignmentDelegate::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  if (GetCurrentDisplay().id() == display.id())
    UpdateWorkArea();
}

}  // namespace ash
