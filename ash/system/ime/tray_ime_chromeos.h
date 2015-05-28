// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_IME_TRAY_IME_CHROMEOS_H_
#define ASH_SYSTEM_IME_TRAY_IME_CHROMEOS_H_

#include "ash/system/chromeos/virtual_keyboard/virtual_keyboard_observer.h"
#include "ash/system/ime/ime_observer.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray_accessibility.h"

namespace views {
class Label;
}

namespace ash {
struct IMEInfo;

namespace tray {
class IMEDefaultView;
class IMEDetailedView;
class IMENotificationView;
}

class TrayItemView;

class ASH_EXPORT TrayIME : public SystemTrayItem,
                           public IMEObserver,
                           public AccessibilityObserver,
                           public VirtualKeyboardObserver {
 public:
  explicit TrayIME(SystemTray* system_tray);
  ~TrayIME() override;

  // Overridden from VirtualKeyboardObserver.
  void OnKeyboardSuppressionChanged(bool suppressed) override;

  // Overridden from AccessibilityObserver:
  void OnAccessibilityModeChanged(
      ui::AccessibilityNotificationVisibility notify) override;

 private:
  friend class TrayIMETest;

  // Repopulates the DefaultView and DetailedView.
  void Update();
  // Updates the System Tray label.
  void UpdateTrayLabel(const IMEInfo& info, size_t count);
  // Returns whether the virtual keyboard toggle should be shown in the
  // detailed view.
  bool ShouldShowKeyboardToggle();
  // Returns the appropriate label for the detailed view.
  base::string16 GetDefaultViewLabel(bool show_ime_label);

  // Overridden from SystemTrayItem.
  views::View* CreateTrayView(user::LoginStatus status) override;
  views::View* CreateDefaultView(user::LoginStatus status) override;
  views::View* CreateDetailedView(user::LoginStatus status) override;
  void DestroyTrayView() override;
  void DestroyDefaultView() override;
  void DestroyDetailedView() override;
  void UpdateAfterLoginStatusChange(user::LoginStatus status) override;
  void UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) override;

  // Overridden from IMEObserver.
  void OnIMERefresh() override;

  // Whether the default view should be shown.
  bool ShouldDefaultViewBeVisible();

  TrayItemView* tray_label_;
  tray::IMEDefaultView* default_;
  tray::IMEDetailedView* detailed_;
  // Whether the virtual keyboard is suppressed.
  bool keyboard_suppressed_;
  // Cached IME info.
  IMEInfoList ime_list_;
  IMEInfo current_ime_;
  IMEPropertyInfoList property_list_;

  DISALLOW_COPY_AND_ASSIGN(TrayIME);
};

}  // namespace ash

#endif  // ASH_SYSTEM_IME_TRAY_IME_CHROMEOS_H_
