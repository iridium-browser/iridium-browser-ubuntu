// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_POWER_BATTERY_NOTIFICATION_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_POWER_BATTERY_NOTIFICATION_H_

#include "ash/ash_export.h"
#include "ash/common/system/chromeos/power/tray_power.h"
#include "base/macros.h"

namespace message_center {
class MessageCenter;
}

namespace ash {

// Class for showing and hiding a MessageCenter low battery notification.
class ASH_EXPORT BatteryNotification {
 public:
  BatteryNotification(message_center::MessageCenter* message_center,
                      TrayPower::NotificationState notification_state);
  ~BatteryNotification();

  // Updates the notification if it still exists.
  void Update(TrayPower::NotificationState notification_state);

 private:
  message_center::MessageCenter* message_center_;

  DISALLOW_COPY_AND_ASSIGN(BatteryNotification);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_POWER_BATTERY_NOTIFICATION_H_
