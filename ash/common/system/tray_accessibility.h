// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_ACCESSIBILITY_H_
#define ASH_COMMON_SYSTEM_TRAY_ACCESSIBILITY_H_

#include <stdint.h>

#include "ash/common/accessibility_delegate.h"
#include "ash/common/shell_observer.h"
#include "ash/common/system/accessibility_observer.h"
#include "ash/common/system/tray/tray_details_view.h"
#include "ash/common/system/tray/tray_image_item.h"
#include "ash/common/system/tray/tray_notification_view.h"
#include "ash/common/system/tray/view_click_listener.h"
#include "base/macros.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/button/button.h"

namespace chromeos {
class TrayAccessibilityTest;
}

namespace views {
class Button;
class ImageView;
class Label;
class View;
}

namespace ash {
class HoverHighlightView;
class SystemTrayItem;

namespace tray {

class AccessibilityPopupView : public TrayNotificationView {
 public:
  AccessibilityPopupView(SystemTrayItem* owner, uint32_t enabled_state_bits);

  const views::Label* label_for_test() const { return label_; }

 private:
  views::Label* CreateLabel(uint32_t enabled_state_bits);

  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityPopupView);
};

class AccessibilityDetailedView : public TrayDetailsView,
                                  public ViewClickListener,
                                  public views::ButtonListener,
                                  public ShellObserver {
 public:
  AccessibilityDetailedView(SystemTrayItem* owner, LoginStatus login);
  ~AccessibilityDetailedView() override {}

 private:
  // Add the accessibility feature list.
  void AppendAccessibilityList();

  // Add help entries.
  void AppendHelpEntries();

  HoverHighlightView* AddScrollListItem(const base::string16& text,
                                        bool highlight,
                                        bool checked);
  // Overridden from ViewClickListener.
  void OnViewClicked(views::View* sender) override;
  // Overridden from ButtonListener.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  views::View* spoken_feedback_view_;
  views::View* high_contrast_view_;
  views::View* screen_magnifier_view_;
  views::View* large_cursor_view_;
  views::View* help_view_;
  views::View* settings_view_;
  views::View* autoclick_view_;
  views::View* virtual_keyboard_view_;

  bool spoken_feedback_enabled_;
  bool high_contrast_enabled_;
  bool screen_magnifier_enabled_;
  bool large_cursor_enabled_;
  bool autoclick_enabled_;
  bool virtual_keyboard_enabled_;
  LoginStatus login_;

  friend class chromeos::TrayAccessibilityTest;
  DISALLOW_COPY_AND_ASSIGN(AccessibilityDetailedView);
};

}  // namespace tray

class TrayAccessibility : public TrayImageItem, public AccessibilityObserver {
 public:
  explicit TrayAccessibility(SystemTray* system_tray);
  ~TrayAccessibility() override;

 private:
  void SetTrayIconVisible(bool visible);
  tray::AccessibilityDetailedView* CreateDetailedMenu();

  // Overridden from TrayImageItem.
  bool GetInitialVisibility() override;
  views::View* CreateDefaultView(LoginStatus status) override;
  views::View* CreateDetailedView(LoginStatus status) override;
  void DestroyDefaultView() override;
  void DestroyDetailedView() override;
  void UpdateAfterLoginStatusChange(LoginStatus status) override;

  // Overridden from AccessibilityObserver.
  void OnAccessibilityModeChanged(
      AccessibilityNotificationVisibility notify) override;

  views::View* default_;
  tray::AccessibilityPopupView* detailed_popup_;
  tray::AccessibilityDetailedView* detailed_menu_;

  // Bitmap of fvalues from AccessibilityState.  Can contain any or
  // both of A11Y_SPOKEN_FEEDBACK A11Y_BRAILLE_DISPLAY_CONNECTED.
  uint32_t request_popup_view_state_;

  bool tray_icon_visible_;
  LoginStatus login_;

  // Bitmap of values from AccessibilityState enum.
  uint32_t previous_accessibility_state_;

  // A11y feature status on just entering the lock screen.
  bool show_a11y_menu_on_lock_screen_;

  friend class chromeos::TrayAccessibilityTest;
  DISALLOW_COPY_AND_ASSIGN(TrayAccessibility);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_ACCESSIBILITY_H_
