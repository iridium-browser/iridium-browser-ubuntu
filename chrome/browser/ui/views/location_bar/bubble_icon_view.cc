// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/bubble_icon_view.h"

#include "chrome/browser/command_updater.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/events/event.h"
#include "ui/views/bubble/bubble_delegate.h"

BubbleIconView::BubbleIconView(CommandUpdater* command_updater, int command_id)
    : command_updater_(command_updater),
      command_id_(command_id),
      suppress_mouse_released_action_(false) {
  SetAccessibilityFocusable(true);
}

BubbleIconView::~BubbleIconView() {
}

bool BubbleIconView::IsBubbleShowing() const {
  // If the bubble is being destroyed, it's considered showing though it may be
  // already invisible currently.
  return GetBubble() != NULL;
}

void BubbleIconView::GetAccessibleState(ui::AXViewState* state) {
  views::ImageView::GetAccessibleState(state);
  state->role = ui::AX_ROLE_BUTTON;
}

bool BubbleIconView::GetTooltipText(const gfx::Point& p,
                                    base::string16* tooltip) const {
  if (IsBubbleShowing())
    return false;

  return views::ImageView::GetTooltipText(p, tooltip);
}

bool BubbleIconView::OnMousePressed(const ui::MouseEvent& event) {
  // If the bubble is showing then don't reshow it when the mouse is released.
  suppress_mouse_released_action_ = IsBubbleShowing();

  // We want to show the bubble on mouse release; that is the standard behavior
  // for buttons.
  return true;
}

void BubbleIconView::OnMouseReleased(const ui::MouseEvent& event) {
  // If this is the second click on this view then the bubble was showing on the
  // mouse pressed event and is hidden now. Prevent the bubble from reshowing by
  // doing nothing here.
  if (suppress_mouse_released_action_) {
    suppress_mouse_released_action_ = false;
    return;
  }

  if (event.IsOnlyLeftMouseButton() && HitTestPoint(event.location()))
    ExecuteCommand(EXECUTE_SOURCE_MOUSE);
}

bool BubbleIconView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE ||
      event.key_code() == ui::VKEY_RETURN) {
    ExecuteCommand(EXECUTE_SOURCE_KEYBOARD);
    return true;
  }
  return false;
}

void BubbleIconView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    ExecuteCommand(EXECUTE_SOURCE_GESTURE);
    event->SetHandled();
  }
}

void BubbleIconView::ExecuteCommand(ExecuteSource source) {
  OnExecuting(source);
  if (command_updater_)
    command_updater_->ExecuteCommand(command_id_);
}

void BubbleIconView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  views::BubbleDelegateView* bubble = GetBubble();
  if (bubble)
    bubble->OnAnchorBoundsChanged();
}
