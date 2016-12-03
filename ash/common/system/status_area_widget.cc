// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/status_area_widget.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/system/overview/overview_button_tray.h"
#include "ash/common/system/status_area_widget_delegate.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/web_notification/web_notification_tray.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "base/i18n/time_formatting.h"
#include "ui/native_theme/native_theme_dark_aura.h"

#if defined(OS_CHROMEOS)
#include "ash/common/system/chromeos/ime_menu/ime_menu_tray.h"
#include "ash/common/system/chromeos/palette/palette_tray.h"
#include "ash/common/system/chromeos/palette/palette_utils.h"
#include "ash/common/system/chromeos/session/logout_button_tray.h"
#include "ash/common/system/chromeos/virtual_keyboard/virtual_keyboard_tray.h"
#include "ui/display/display.h"
#endif

namespace ash {

StatusAreaWidget::StatusAreaWidget(WmWindow* status_container,
                                   WmShelf* wm_shelf)
    : status_area_widget_delegate_(new StatusAreaWidgetDelegate),
      overview_button_tray_(nullptr),
      system_tray_(nullptr),
      web_notification_tray_(nullptr),
#if defined(OS_CHROMEOS)
      logout_button_tray_(nullptr),
      palette_tray_(nullptr),
      virtual_keyboard_tray_(nullptr),
      ime_menu_tray_(nullptr),
#endif
      login_status_(LoginStatus::NOT_LOGGED_IN),
      wm_shelf_(wm_shelf) {
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = status_area_widget_delegate_;
  params.name = "StatusAreaWidget";
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  status_container->GetRootWindowController()
      ->ConfigureWidgetInitParamsForContainer(
          this, status_container->GetShellWindowId(), &params);
  Init(params);
  set_focus_on_creation(false);
  SetContentsView(status_area_widget_delegate_);
}

StatusAreaWidget::~StatusAreaWidget() {}

void StatusAreaWidget::CreateTrayViews() {
  AddOverviewButtonTray();
  AddSystemTray();
  AddWebNotificationTray();
#if defined(OS_CHROMEOS)
  AddLogoutButtonTray();
  AddPaletteTray();
  AddVirtualKeyboardTray();
  AddImeMenuTray();
#endif

  SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
  DCHECK(delegate);
  // Initialize after all trays have been created.
  system_tray_->InitializeTrayItems(delegate, web_notification_tray_);
  web_notification_tray_->Initialize();
#if defined(OS_CHROMEOS)
  logout_button_tray_->Initialize();
  virtual_keyboard_tray_->Initialize();
  ime_menu_tray_->Initialize();
#endif
  overview_button_tray_->Initialize();
  SetShelfAlignment(system_tray_->shelf_alignment());
  UpdateAfterLoginStatusChange(delegate->GetUserLoginStatus());
}

void StatusAreaWidget::Shutdown() {
  system_tray_->Shutdown();
  // Destroy the trays early, causing them to be removed from the view
  // hierarchy. Do not used scoped pointers since we don't want to destroy them
  // in the destructor if Shutdown() is not called (e.g. in tests).
  delete web_notification_tray_;
  web_notification_tray_ = nullptr;
  // Must be destroyed after |web_notification_tray_|.
  delete system_tray_;
  system_tray_ = nullptr;
#if defined(OS_CHROMEOS)
  delete ime_menu_tray_;
  ime_menu_tray_ = nullptr;
  delete virtual_keyboard_tray_;
  virtual_keyboard_tray_ = nullptr;
  delete logout_button_tray_;
  logout_button_tray_ = nullptr;
#endif
  delete overview_button_tray_;
  overview_button_tray_ = nullptr;
}

void StatusAreaWidget::SetShelfAlignment(ShelfAlignment alignment) {
  status_area_widget_delegate_->set_alignment(alignment);
  if (system_tray_)
    system_tray_->SetShelfAlignment(alignment);
  if (web_notification_tray_)
    web_notification_tray_->SetShelfAlignment(alignment);
#if defined(OS_CHROMEOS)
  if (logout_button_tray_)
    logout_button_tray_->SetShelfAlignment(alignment);
  if (virtual_keyboard_tray_)
    virtual_keyboard_tray_->SetShelfAlignment(alignment);
  if (ime_menu_tray_)
    ime_menu_tray_->SetShelfAlignment(alignment);
  if (palette_tray_)
    palette_tray_->SetShelfAlignment(alignment);
#endif
  if (overview_button_tray_)
    overview_button_tray_->SetShelfAlignment(alignment);
  status_area_widget_delegate_->UpdateLayout();
}

void StatusAreaWidget::UpdateAfterLoginStatusChange(LoginStatus login_status) {
  if (login_status_ == login_status)
    return;
  login_status_ = login_status;
  if (system_tray_)
    system_tray_->UpdateAfterLoginStatusChange(login_status);
  if (web_notification_tray_)
    web_notification_tray_->UpdateAfterLoginStatusChange(login_status);
#if defined(OS_CHROMEOS)
  if (logout_button_tray_)
    logout_button_tray_->UpdateAfterLoginStatusChange(login_status);
#endif
  if (overview_button_tray_)
    overview_button_tray_->UpdateAfterLoginStatusChange(login_status);
}

void StatusAreaWidget::OnTrayVisibilityChanged(TrayBackgroundView* tray) {
  if (!ash::MaterialDesignController::IsShelfMaterial())
    return;

  // No separator is required between |system_tray_| and |overview_button_tray_|
  // and no separator is required for the right most tray item.
  if (tray == overview_button_tray_ || tray == system_tray_) {
    tray->SetSeparatorVisibility(false);
    return;
  }
#if defined(OS_CHROMEOS)
  // If |logout_button_tray_| is visible, check if |tray| is visible and to
  // the left of |logout_button_tray_|. If it is the case, then no separator
  // is required between |tray| and |logout_button_tray_|. If
  // |logout_button_tray_| is not visible, then separator should always be
  // visible.
  tray->SetSeparatorVisibility(!IsNextVisibleTrayToLogout(tray) &&
                               tray != logout_button_tray_);
#else
  tray->SetSeparatorVisibility(true);
#endif
}

bool StatusAreaWidget::ShouldShowShelf() const {
  if ((system_tray_ && system_tray_->ShouldShowShelf()) ||
      (web_notification_tray_ &&
       web_notification_tray_->ShouldBlockShelfAutoHide()))
    return true;

#if defined(OS_CHROMEOS)
  if (palette_tray_ && palette_tray_->ShouldBlockShelfAutoHide())
    return true;
#endif

  if (!wm_shelf_->IsVisible())
    return false;

  // If the shelf is currently visible, don't hide the shelf if the mouse
  // is in any of the notification bubbles.
  return (system_tray_ && system_tray_->IsMouseInNotificationBubble()) ||
         (web_notification_tray_ &&
          web_notification_tray_->IsMouseInNotificationBubble());
}

bool StatusAreaWidget::IsMessageBubbleShown() const {
  return ((system_tray_ && system_tray_->IsAnyBubbleVisible()) ||
          (web_notification_tray_ &&
           web_notification_tray_->IsMessageCenterBubbleVisible()));
}

void StatusAreaWidget::SchedulePaint() {
  status_area_widget_delegate_->SchedulePaint();
  web_notification_tray_->SchedulePaint();
  system_tray_->SchedulePaint();
#if defined(OS_CHROMEOS)
  virtual_keyboard_tray_->SchedulePaint();
  logout_button_tray_->SchedulePaint();
  ime_menu_tray_->SchedulePaint();
  if (palette_tray_)
    palette_tray_->SchedulePaint();
#endif
  overview_button_tray_->SchedulePaint();
}

const ui::NativeTheme* StatusAreaWidget::GetNativeTheme() const {
  return MaterialDesignController::IsShelfMaterial()
             ? ui::NativeThemeDarkAura::instance()
             : Widget::GetNativeTheme();
}

void StatusAreaWidget::OnNativeWidgetActivationChanged(bool active) {
  Widget::OnNativeWidgetActivationChanged(active);
  if (active)
    status_area_widget_delegate_->SetPaneFocusAndFocusDefault();
}

void StatusAreaWidget::UpdateShelfItemBackground(int alpha) {
  web_notification_tray_->UpdateShelfItemBackground(alpha);
  system_tray_->UpdateShelfItemBackground(alpha);
#if defined(OS_CHROMEOS)
  virtual_keyboard_tray_->UpdateShelfItemBackground(alpha);
  logout_button_tray_->UpdateShelfItemBackground(alpha);
  ime_menu_tray_->UpdateShelfItemBackground(alpha);
  if (palette_tray_)
    palette_tray_->UpdateShelfItemBackground(alpha);
#endif
  overview_button_tray_->UpdateShelfItemBackground(alpha);
}

void StatusAreaWidget::AddSystemTray() {
  system_tray_ = new SystemTray(wm_shelf_);
  status_area_widget_delegate_->AddTray(system_tray_);
}

void StatusAreaWidget::AddWebNotificationTray() {
  DCHECK(system_tray_);
  web_notification_tray_ = new WebNotificationTray(
      wm_shelf_, WmLookup::Get()->GetWindowForWidget(this), system_tray_);
  status_area_widget_delegate_->AddTray(web_notification_tray_);
}

#if defined(OS_CHROMEOS)
void StatusAreaWidget::AddLogoutButtonTray() {
  logout_button_tray_ = new LogoutButtonTray(wm_shelf_);
  status_area_widget_delegate_->AddTray(logout_button_tray_);
}

void StatusAreaWidget::AddPaletteTray() {
  if (!IsPaletteFeatureEnabled())
    return;

  const display::Display& display =
      WmLookup::Get()->GetWindowForWidget(this)->GetDisplayNearestWindow();

  // Create the palette only on the internal display, where the stylus is
  // available. We also create a palette on every display if requested from the
  // command line.
  if (display.IsInternal() || IsPaletteEnabledOnEveryDisplay()) {
    palette_tray_ = new PaletteTray(wm_shelf_);
    status_area_widget_delegate_->AddTray(palette_tray_);
  }
}

void StatusAreaWidget::AddVirtualKeyboardTray() {
  virtual_keyboard_tray_ = new VirtualKeyboardTray(wm_shelf_);
  status_area_widget_delegate_->AddTray(virtual_keyboard_tray_);
}

void StatusAreaWidget::AddImeMenuTray() {
  ime_menu_tray_ = new ImeMenuTray(wm_shelf_);
  status_area_widget_delegate_->AddTray(ime_menu_tray_);
}

bool StatusAreaWidget::IsNextVisibleTrayToLogout(
    TrayBackgroundView* tray) const {
  int logout_button_index =
      status_area_widget_delegate_->GetIndexOf(logout_button_tray_);
  // Logout button should always exist.
  DCHECK_NE(-1, logout_button_index);
  if (!logout_button_tray_->visible())
    return false;

  for (int c = logout_button_index + 1;
       c < status_area_widget_delegate_->child_count(); c++) {
    if (status_area_widget_delegate_->child_at(c)->visible())
      return tray == status_area_widget_delegate_->child_at(c);
  }
  return false;
}
#endif

void StatusAreaWidget::AddOverviewButtonTray() {
  overview_button_tray_ = new OverviewButtonTray(wm_shelf_);
  status_area_widget_delegate_->AddTray(overview_button_tray_);
}

}  // namespace ash
