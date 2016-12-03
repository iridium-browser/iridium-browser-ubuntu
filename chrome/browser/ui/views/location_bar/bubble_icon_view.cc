// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/bubble_icon_view.h"

#include "chrome/browser/command_updater.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"

BubbleIconView::BubbleIconView(CommandUpdater* command_updater, int command_id)
    : image_(new views::ImageView()),
      command_updater_(command_updater),
      command_id_(command_id),
      active_(false),
      suppress_mouse_released_action_(false) {
  AddChildView(image_);
  image_->set_interactive(false);
  image_->EnableCanvasFlippingForRTLUI(true);
  if (ui::MaterialDesignController::IsModeMaterial()) {
    SetInkDropMode(InkDropMode::ON);
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  } else {
    image_->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  }
}

BubbleIconView::~BubbleIconView() {}

bool BubbleIconView::IsBubbleShowing() const {
  // If the bubble is being destroyed, it's considered showing though it may be
  // already invisible currently.
  return GetBubble() != nullptr;
}

void BubbleIconView::SetImage(const gfx::ImageSkia* image_skia) {
  image_->SetImage(image_skia);
}

const gfx::ImageSkia& BubbleIconView::GetImage() const {
  return image_->GetImage();
}

void BubbleIconView::SetTooltipText(const base::string16& tooltip) {
  image_->SetTooltipText(tooltip);
}

void BubbleIconView::GetAccessibleState(ui::AXViewState* state) {
  image_->GetAccessibleState(state);
  state->role = ui::AX_ROLE_BUTTON;
}

bool BubbleIconView::GetTooltipText(const gfx::Point& p,
                                    base::string16* tooltip) const {
  return !IsBubbleShowing() && image_->GetTooltipText(p, tooltip);
}

gfx::Size BubbleIconView::GetPreferredSize() const {
  return image_->GetPreferredSize();
}

void BubbleIconView::Layout() {
  View::Layout();
  image_->SetBoundsRect(GetLocalBounds());
}

bool BubbleIconView::OnMousePressed(const ui::MouseEvent& event) {
  // If the bubble is showing then don't reshow it when the mouse is released.
  suppress_mouse_released_action_ = IsBubbleShowing();
  if (!suppress_mouse_released_action_ && event.IsOnlyLeftMouseButton())
    AnimateInkDrop(views::InkDropState::ACTION_PENDING, &event);

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
    OnPressed(false);
    return;
  }
  if (!event.IsLeftMouseButton())
    return;

  const bool activated = HitTestPoint(event.location());
  AnimateInkDrop(
      activated ? views::InkDropState::ACTIVATED : views::InkDropState::HIDDEN,
      &event);
  if (activated)
    ExecuteCommand(EXECUTE_SOURCE_MOUSE);
  OnPressed(activated);
}

bool BubbleIconView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() != ui::VKEY_RETURN && event.key_code() != ui::VKEY_SPACE)
    return false;

  AnimateInkDrop(views::InkDropState::ACTIVATED, nullptr /* &event */);
  // As with CustomButton, return activates on key down and space activates on
  // key up.
  if (event.key_code() == ui::VKEY_RETURN)
    ExecuteCommand(EXECUTE_SOURCE_KEYBOARD);
  return true;
}

bool BubbleIconView::OnKeyReleased(const ui::KeyEvent& event) {
  if (event.key_code() != ui::VKEY_SPACE)
    return false;

  ExecuteCommand(EXECUTE_SOURCE_KEYBOARD);
  return true;
}

void BubbleIconView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  View::ViewHierarchyChanged(details);
  if (details.is_add && GetNativeTheme())
    UpdateIcon();
}

void BubbleIconView::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  UpdateIcon();
}

void BubbleIconView::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  image_->SetPaintToLayer(true);
  image_->layer()->SetFillsBoundsOpaquely(false);
  views::InkDropHostView::AddInkDropLayer(ink_drop_layer);
}

void BubbleIconView::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  views::InkDropHostView::RemoveInkDropLayer(ink_drop_layer);
  image_->SetPaintToLayer(false);
}

SkColor BubbleIconView::GetInkDropBaseColor() const {
  return color_utils::DeriveDefaultIconColor(GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldDefaultColor));
}

bool BubbleIconView::ShouldShowInkDropForFocus() const {
  return true;
}

void BubbleIconView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    AnimateInkDrop(views::InkDropState::ACTIVATED, event);
    ExecuteCommand(EXECUTE_SOURCE_GESTURE);
    event->SetHandled();
  }
}

void BubbleIconView::OnWidgetDestroying(views::Widget* widget) {
  widget->RemoveObserver(this);
}

void BubbleIconView::OnWidgetVisibilityChanged(views::Widget* widget,
                                               bool visible) {
  // |widget| is a bubble that has just got shown / hidden.
  if (!visible)
    AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr /* event */);
}

void BubbleIconView::ExecuteCommand(ExecuteSource source) {
  OnExecuting(source);
  if (command_updater_)
    command_updater_->ExecuteCommand(command_id_);
}

gfx::VectorIconId BubbleIconView::GetVectorIcon() const {
  return gfx::VectorIconId::VECTOR_ICON_NONE;
}

bool BubbleIconView::SetRasterIcon() {
  return false;
}

void BubbleIconView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  views::BubbleDialogDelegateView* bubble = GetBubble();
  if (bubble)
    bubble->OnAnchorBoundsChanged();
}

void BubbleIconView::UpdateIcon() {
  if (SetRasterIcon())
    return;

  const int icon_size =
      ui::MaterialDesignController::IsModeMaterial() ? 16 : 18;
  const ui::NativeTheme* theme = GetNativeTheme();
  SkColor icon_color =
      active_
          ? theme->GetSystemColor(ui::NativeTheme::kColorId_CallToActionColor)
          : GetInkDropBaseColor();
  image_->SetImage(
      gfx::CreateVectorIcon(GetVectorIcon(), icon_size, icon_color));
}

void BubbleIconView::SetActiveInternal(bool active) {
  if (active_ == active)
    return;
  active_ = active;
  UpdateIcon();
}
