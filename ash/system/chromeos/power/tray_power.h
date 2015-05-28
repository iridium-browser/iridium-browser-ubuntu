// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_POWER_TRAY_POWER_H_
#define ASH_SYSTEM_CHROMEOS_POWER_TRAY_POWER_H_

#include "ash/system/chromeos/power/power_status.h"
#include "ash/system/tray/system_tray_item.h"

class SkBitmap;

namespace gfx {
class Image;
class ImageSkia;
}

namespace message_center {
class MessageCenter;
}

namespace ash {

class BatteryNotification;

namespace tray {
class PowerTrayView;
}

class ASH_EXPORT TrayPower : public SystemTrayItem,
                             public PowerStatus::Observer {
 public:
  enum NotificationState {
    NOTIFICATION_NONE,

    // Low battery charge.
    NOTIFICATION_LOW_POWER,

    // Critically low battery charge.
    NOTIFICATION_CRITICAL,
  };

  // Time-based notification thresholds when on battery power.
  static const int kCriticalMinutes;
  static const int kLowPowerMinutes;
  static const int kNoWarningMinutes;

  // Percentage-based notification thresholds when using a low-power charger.
  static const int kCriticalPercentage;
  static const int kLowPowerPercentage;
  static const int kNoWarningPercentage;

  TrayPower(SystemTray* system_tray,
            message_center::MessageCenter* message_center);
  ~TrayPower() override;

 private:
  friend class TrayPowerTest;

  // This enum is used for histogram. The existing values should not be removed,
  // and the new values should be added just before CHARGER_TYPE_COUNT.
  enum ChargerType{
    UNKNOWN_CHARGER,
    MAINS_CHARGER,
    USB_CHARGER,
    UNCONFIRMED_SPRING_CHARGER,
    SAFE_SPRING_CHARGER,
    CHARGER_TYPE_COUNT,
  };

  // Overridden from SystemTrayItem.
  views::View* CreateTrayView(user::LoginStatus status) override;
  views::View* CreateDefaultView(user::LoginStatus status) override;
  void DestroyTrayView() override;
  void DestroyDefaultView() override;
  void UpdateAfterLoginStatusChange(user::LoginStatus status) override;
  void UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) override;

  // Overridden from PowerStatus::Observer.
  void OnPowerStatusChanged() override;

  // Show a notification that a low-power USB charger has been connected.
  // Returns true if a notification was shown or explicitly hidden.
  bool MaybeShowUsbChargerNotification();

  // Sets |notification_state_|. Returns true if a notification should be shown.
  bool UpdateNotificationState();
  bool UpdateNotificationStateForRemainingTime();
  bool UpdateNotificationStateForRemainingPercentage();

  message_center::MessageCenter* message_center_;  // Not owned.
  tray::PowerTrayView* power_tray_;
  scoped_ptr<BatteryNotification> battery_notification_;
  NotificationState notification_state_;

  // Was a USB charger connected the last time OnPowerStatusChanged() was
  // called?
  bool usb_charger_was_connected_;

  // Was line power connected the last time onPowerStatusChanged() was called?
  bool line_power_was_connected_;

  DISALLOW_COPY_AND_ASSIGN(TrayPower);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_POWER_TRAY_POWER_H_
