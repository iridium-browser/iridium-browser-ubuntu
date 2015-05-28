// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/power/tray_power.h"

#include "ash/accessibility_delegate.h"
#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/system/chromeos/power/battery_notification.h"
#include "ash/system/date/date_view.h"
#include "ash/system/system_notifier.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {
namespace tray {

// This view is used only for the tray.
class PowerTrayView : public views::ImageView {
 public:
  PowerTrayView() {
    UpdateImage();
  }

  ~PowerTrayView() override {}

  // Overriden from views::View.
  void GetAccessibleState(ui::AXViewState* state) override {
    state->name = accessible_name_;
    state->role = ui::AX_ROLE_BUTTON;
  }

  void UpdateStatus(bool battery_alert) {
    UpdateImage();
    SetVisible(PowerStatus::Get()->IsBatteryPresent());

    if (battery_alert) {
      accessible_name_ = PowerStatus::Get()->GetAccessibleNameString(true);
      NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, true);
    }
  }

 private:
  void UpdateImage() {
    SetImage(PowerStatus::Get()->GetBatteryImage(PowerStatus::ICON_LIGHT));
  }

  base::string16 accessible_name_;

  DISALLOW_COPY_AND_ASSIGN(PowerTrayView);
};

}  // namespace tray

const int TrayPower::kCriticalMinutes = 5;
const int TrayPower::kLowPowerMinutes = 15;
const int TrayPower::kNoWarningMinutes = 30;
const int TrayPower::kCriticalPercentage = 5;
const int TrayPower::kLowPowerPercentage = 10;
const int TrayPower::kNoWarningPercentage = 15;

TrayPower::TrayPower(SystemTray* system_tray, MessageCenter* message_center)
    : SystemTrayItem(system_tray),
      message_center_(message_center),
      power_tray_(NULL),
      notification_state_(NOTIFICATION_NONE),
      usb_charger_was_connected_(false),
      line_power_was_connected_(false) {
  PowerStatus::Get()->AddObserver(this);
}

TrayPower::~TrayPower() {
  PowerStatus::Get()->RemoveObserver(this);
}

views::View* TrayPower::CreateTrayView(user::LoginStatus status) {
  // There may not be enough information when this is created about whether
  // there is a battery or not. So always create this, and adjust visibility as
  // necessary.
  CHECK(power_tray_ == NULL);
  power_tray_ = new tray::PowerTrayView();
  power_tray_->UpdateStatus(false);
  return power_tray_;
}

views::View* TrayPower::CreateDefaultView(user::LoginStatus status) {
  // Make sure icon status is up-to-date. (Also triggers stub activation).
  PowerStatus::Get()->RequestStatusUpdate();
  return NULL;
}

void TrayPower::DestroyTrayView() {
  power_tray_ = NULL;
}

void TrayPower::DestroyDefaultView() {
}

void TrayPower::UpdateAfterLoginStatusChange(user::LoginStatus status) {
}

void TrayPower::UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) {
  SetTrayImageItemBorder(power_tray_, alignment);
}

void TrayPower::OnPowerStatusChanged() {
  bool battery_alert = UpdateNotificationState();
  if (power_tray_)
    power_tray_->UpdateStatus(battery_alert);

  // Factory testing may place the battery into unusual states.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kAshHideNotificationsForFactory))
    return;

  MaybeShowUsbChargerNotification();

  if (battery_alert) {
    // Remove any existing notification so it's dismissed before adding a new
    // one. Otherwise we might update a "low battery" notification to "critical"
    // without it being shown again.
    battery_notification_.reset();
    battery_notification_.reset(
        new BatteryNotification(message_center_, notification_state_));
  } else if (notification_state_ == NOTIFICATION_NONE) {
    battery_notification_.reset();
  } else if (battery_notification_.get()) {
    battery_notification_->Update(notification_state_);
  }

  usb_charger_was_connected_ = PowerStatus::Get()->IsUsbChargerConnected();
  line_power_was_connected_ = PowerStatus::Get()->IsLinePowerConnected();
}

bool TrayPower::MaybeShowUsbChargerNotification() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const char kNotificationId[] = "usb-charger";
  bool usb_charger_is_connected = PowerStatus::Get()->IsUsbChargerConnected();

  // Check for a USB charger being connected.
  if (usb_charger_is_connected && !usb_charger_was_connected_) {
    scoped_ptr<Notification> notification(new Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE,
        kNotificationId,
        rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_LOW_POWER_CHARGER_TITLE),
        rb.GetLocalizedString(
            IDS_ASH_STATUS_TRAY_LOW_POWER_CHARGER_MESSAGE_SHORT),
        rb.GetImageNamed(IDR_AURA_NOTIFICATION_LOW_POWER_CHARGER),
        base::string16(),
        message_center::NotifierId(
            message_center::NotifierId::SYSTEM_COMPONENT,
            system_notifier::kNotifierPower),
        message_center::RichNotificationData(),
        NULL));
    message_center_->AddNotification(notification.Pass());
    return true;
  }

  // Check for unplug of a USB charger while the USB charger notification is
  // showing.
  if (!usb_charger_is_connected && usb_charger_was_connected_) {
    message_center_->RemoveNotification(kNotificationId, false);
    return true;
  }
  return false;
}

bool TrayPower::UpdateNotificationState() {
  const PowerStatus& status = *PowerStatus::Get();
  if (!status.IsBatteryPresent() ||
      status.IsBatteryTimeBeingCalculated() ||
      status.IsMainsChargerConnected()) {
    notification_state_ = NOTIFICATION_NONE;
    return false;
  }

  return status.IsUsbChargerConnected() ?
      UpdateNotificationStateForRemainingPercentage() :
      UpdateNotificationStateForRemainingTime();
}

bool TrayPower::UpdateNotificationStateForRemainingTime() {
  // The notification includes a rounded minutes value, so round the estimate
  // received from the power manager to match.
  const int remaining_minutes = static_cast<int>(
      PowerStatus::Get()->GetBatteryTimeToEmpty().InSecondsF() / 60.0 + 0.5);

  if (remaining_minutes >= kNoWarningMinutes ||
      PowerStatus::Get()->IsBatteryFull()) {
    notification_state_ = NOTIFICATION_NONE;
    return false;
  }

  switch (notification_state_) {
    case NOTIFICATION_NONE:
      if (remaining_minutes <= kCriticalMinutes) {
        notification_state_ = NOTIFICATION_CRITICAL;
        return true;
      }
      if (remaining_minutes <= kLowPowerMinutes) {
        notification_state_ = NOTIFICATION_LOW_POWER;
        return true;
      }
      return false;
    case NOTIFICATION_LOW_POWER:
      if (remaining_minutes <= kCriticalMinutes) {
        notification_state_ = NOTIFICATION_CRITICAL;
        return true;
      }
      return false;
    case NOTIFICATION_CRITICAL:
      return false;
  }
  NOTREACHED();
  return false;
}

bool TrayPower::UpdateNotificationStateForRemainingPercentage() {
  // The notification includes a rounded percentage, so round the value received
  // from the power manager to match.
  const int remaining_percentage =
      PowerStatus::Get()->GetRoundedBatteryPercent();

  if (remaining_percentage >= kNoWarningPercentage ||
      PowerStatus::Get()->IsBatteryFull()) {
    notification_state_ = NOTIFICATION_NONE;
    return false;
  }

  switch (notification_state_) {
    case NOTIFICATION_NONE:
      if (remaining_percentage <= kCriticalPercentage) {
        notification_state_ = NOTIFICATION_CRITICAL;
        return true;
      }
      if (remaining_percentage <= kLowPowerPercentage) {
        notification_state_ = NOTIFICATION_LOW_POWER;
        return true;
      }
      return false;
    case NOTIFICATION_LOW_POWER:
      if (remaining_percentage <= kCriticalPercentage) {
        notification_state_ = NOTIFICATION_CRITICAL;
        return true;
      }
      return false;
    case NOTIFICATION_CRITICAL:
      return false;
  }
  NOTREACHED();
  return false;
}

}  // namespace ash
