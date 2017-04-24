// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tray_popup_item_container.h"

#include "ash/common/system/tray/tray_constants.h"
#include "ui/gfx/canvas.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

TrayPopupItemContainer::TrayPopupItemContainer(views::View* view,
                                               bool change_background)
    : active_(false), change_background_(change_background) {
  set_notify_enter_exit_on_child(true);
  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0);
  layout->SetDefaultFlex(1);
  SetLayoutManager(layout);
  if (view->layer()) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(view->layer()->fills_bounds_opaquely());
  }
  AddChildView(view);
  SetVisible(view->visible());
}

TrayPopupItemContainer::~TrayPopupItemContainer() {}

void TrayPopupItemContainer::SetActive(bool active) {
  if (!change_background_ || active_ == active)
    return;
  active_ = active;
  SchedulePaint();
}

void TrayPopupItemContainer::ChildVisibilityChanged(View* child) {
  if (visible() == child->visible())
    return;
  SetVisible(child->visible());
  PreferredSizeChanged();
}

void TrayPopupItemContainer::ChildPreferredSizeChanged(View* child) {
  PreferredSizeChanged();
}

void TrayPopupItemContainer::OnMouseEntered(const ui::MouseEvent& event) {
  SetActive(true);
}

void TrayPopupItemContainer::OnMouseExited(const ui::MouseEvent& event) {
  SetActive(false);
}

void TrayPopupItemContainer::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    SetActive(true);
  } else if (event->type() == ui::ET_GESTURE_TAP_CANCEL ||
             event->type() == ui::ET_GESTURE_TAP) {
    SetActive(false);
  }
}

void TrayPopupItemContainer::OnPaintBackground(gfx::Canvas* canvas) {
  if (child_count() == 0)
    return;

  views::View* view = child_at(0);
  if (!view->background()) {
    canvas->FillRect(gfx::Rect(size()),
                     (active_) ? kHoverBackgroundColor : kBackgroundColor);
  }
}

}  // namespace ash
