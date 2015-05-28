// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/easy_unlock_screenlock_state_handler.h"

#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/chromeos_utils.h"
#include "chrome/browser/signin/easy_unlock_metrics.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

ScreenlockBridge::UserPodCustomIcon GetIconForState(
    EasyUnlockScreenlockStateHandler::State state) {
  switch (state) {
    case EasyUnlockScreenlockStateHandler::STATE_NO_BLUETOOTH:
    case EasyUnlockScreenlockStateHandler::STATE_NO_PHONE:
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_NOT_AUTHENTICATED:
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_LOCKED:
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_UNLOCKABLE:
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_UNSUPPORTED:
    case EasyUnlockScreenlockStateHandler::STATE_RSSI_TOO_LOW:
      return ScreenlockBridge::USER_POD_CUSTOM_ICON_LOCKED;
    case EasyUnlockScreenlockStateHandler::STATE_TX_POWER_TOO_HIGH:
    case EasyUnlockScreenlockStateHandler::
             STATE_PHONE_LOCKED_AND_TX_POWER_TOO_HIGH:
      // TODO(isherman): This icon is currently identical to the regular locked
      // icon.  Once the reduced proximity range flag is removed, consider
      // deleting the redundant icon.
      return ScreenlockBridge::USER_POD_CUSTOM_ICON_LOCKED_WITH_PROXIMITY_HINT;
    case EasyUnlockScreenlockStateHandler::STATE_BLUETOOTH_CONNECTING:
      return ScreenlockBridge::USER_POD_CUSTOM_ICON_SPINNER;
    case EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED:
      return ScreenlockBridge::USER_POD_CUSTOM_ICON_UNLOCKED;
    default:
      return ScreenlockBridge::USER_POD_CUSTOM_ICON_NONE;
  }
}

bool HardlockOnClick(EasyUnlockScreenlockStateHandler::State state) {
  return state != EasyUnlockScreenlockStateHandler::STATE_INACTIVE;
}

size_t GetTooltipResourceId(EasyUnlockScreenlockStateHandler::State state) {
  switch (state) {
    case EasyUnlockScreenlockStateHandler::STATE_NO_BLUETOOTH:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_NO_BLUETOOTH;
    case EasyUnlockScreenlockStateHandler::STATE_NO_PHONE:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_NO_PHONE;
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_NOT_AUTHENTICATED:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_PHONE_NOT_AUTHENTICATED;
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_LOCKED:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_PHONE_LOCKED;
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_UNLOCKABLE:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_PHONE_UNLOCKABLE;
    case EasyUnlockScreenlockStateHandler::STATE_RSSI_TOO_LOW:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_RSSI_TOO_LOW;
    case EasyUnlockScreenlockStateHandler::STATE_TX_POWER_TOO_HIGH:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_TX_POWER_TOO_HIGH;
    case EasyUnlockScreenlockStateHandler::
             STATE_PHONE_LOCKED_AND_TX_POWER_TOO_HIGH:
      return
          IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_PHONE_LOCKED_AND_TX_POWER_TOO_HIGH;
    case EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_HARDLOCK_INSTRUCTIONS;
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_UNSUPPORTED:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_UNSUPPORTED_ANDROID_VERSION;
    default:
      return 0;
  }
}

bool TooltipContainsDeviceType(EasyUnlockScreenlockStateHandler::State state) {
  return state == EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED ||
         state == EasyUnlockScreenlockStateHandler::STATE_PHONE_UNLOCKABLE ||
         state == EasyUnlockScreenlockStateHandler::STATE_NO_BLUETOOTH ||
         state == EasyUnlockScreenlockStateHandler::STATE_PHONE_UNSUPPORTED ||
         state == EasyUnlockScreenlockStateHandler::STATE_TX_POWER_TOO_HIGH ||
         state == EasyUnlockScreenlockStateHandler::
                      STATE_PHONE_LOCKED_AND_TX_POWER_TOO_HIGH;
}

// Returns true iff the |state| corresponds to a locked remote device.
bool IsLockedState(EasyUnlockScreenlockStateHandler::State state) {
  return (state == EasyUnlockScreenlockStateHandler::STATE_PHONE_LOCKED ||
          state == EasyUnlockScreenlockStateHandler::
                       STATE_PHONE_LOCKED_AND_TX_POWER_TOO_HIGH);
}

}  // namespace


EasyUnlockScreenlockStateHandler::EasyUnlockScreenlockStateHandler(
    const std::string& user_email,
    HardlockState initial_hardlock_state,
    ScreenlockBridge* screenlock_bridge)
    : state_(STATE_INACTIVE),
      user_email_(user_email),
      screenlock_bridge_(screenlock_bridge),
      hardlock_state_(initial_hardlock_state),
      hardlock_ui_shown_(false),
      is_trial_run_(false),
      did_see_locked_phone_(false) {
  DCHECK(screenlock_bridge_);
  screenlock_bridge_->AddObserver(this);
}

EasyUnlockScreenlockStateHandler::~EasyUnlockScreenlockStateHandler() {
  screenlock_bridge_->RemoveObserver(this);
  // Make sure the screenlock state set by this gets cleared.
  ChangeState(STATE_INACTIVE);
}

bool EasyUnlockScreenlockStateHandler::IsActive() const {
  return state_ != STATE_INACTIVE;
}

bool EasyUnlockScreenlockStateHandler::InStateValidOnRemoteAuthFailure() const {
  // Note that NO_PHONE is not valid in this case because the phone may close
  // the connection if the auth challenge sent to it is invalid. This case
  // should be handled as authentication failure.
  return state_ == EasyUnlockScreenlockStateHandler::STATE_NO_BLUETOOTH ||
         state_ == EasyUnlockScreenlockStateHandler::STATE_PHONE_LOCKED;
}

void EasyUnlockScreenlockStateHandler::ChangeState(State new_state) {
  if (state_ == new_state)
    return;

  state_ = new_state;

  // If lock screen is not active or it forces offline password, just cache the
  // current state. The screenlock state will get refreshed in |ScreenDidLock|.
  if (!screenlock_bridge_->IsLocked())
    return;

  // Do nothing when auth type is online.
  if (screenlock_bridge_->lock_handler()->GetAuthType(user_email_) ==
      ScreenlockBridge::LockHandler::ONLINE_SIGN_IN) {
    return;
  }

  if (IsLockedState(state_))
    did_see_locked_phone_ = true;

  // No hardlock UI for trial run.
  if (!is_trial_run_ && hardlock_state_ != NO_HARDLOCK) {
    ShowHardlockUI();
    return;
  }

  UpdateScreenlockAuthType();

  ScreenlockBridge::UserPodCustomIcon icon = GetIconForState(state_);

  if (icon == ScreenlockBridge::USER_POD_CUSTOM_ICON_NONE) {
    screenlock_bridge_->lock_handler()->HideUserPodCustomIcon(user_email_);
    return;
  }

  ScreenlockBridge::UserPodCustomIconOptions icon_options;
  icon_options.SetIcon(icon);

  // Don't hardlock on trial run.
  if (is_trial_run_)
    icon_options.SetTrialRun();
  else if (HardlockOnClick(state_))
    icon_options.SetHardlockOnClick();

  UpdateTooltipOptions(&icon_options);

  // For states without tooltips, we still need to set an accessibility label.
  if (state_ == EasyUnlockScreenlockStateHandler::STATE_BLUETOOTH_CONNECTING) {
    icon_options.SetAriaLabel(
        l10n_util::GetStringUTF16(IDS_SMART_LOCK_SPINNER_ACCESSIBILITY_LABEL));
  }

  screenlock_bridge_->lock_handler()->ShowUserPodCustomIcon(user_email_,
                                                            icon_options);
}

void EasyUnlockScreenlockStateHandler::SetHardlockState(
    HardlockState new_state) {
  if (hardlock_state_ == new_state)
    return;

  if (new_state == LOGIN_FAILED && hardlock_state_ != NO_HARDLOCK)
    return;

  hardlock_state_ = new_state;

  // If hardlock_state_ was set to NO_HARDLOCK, this means the screen is about
  // to get unlocked. No need to update it in this case.
  if (hardlock_state_ != NO_HARDLOCK) {
    hardlock_ui_shown_ = false;

    RefreshScreenlockState();
  }
}

void EasyUnlockScreenlockStateHandler::MaybeShowHardlockUI() {
  if (hardlock_state_ != NO_HARDLOCK)
    ShowHardlockUI();
}

void EasyUnlockScreenlockStateHandler::SetTrialRun() {
  if (is_trial_run_)
    return;
  is_trial_run_ = true;
  RefreshScreenlockState();
  RecordEasyUnlockTrialRunEvent(EASY_UNLOCK_TRIAL_RUN_EVENT_LAUNCHED);
}

void EasyUnlockScreenlockStateHandler::RecordClickOnLockIcon() {
  if (!is_trial_run_)
    return;
  RecordEasyUnlockTrialRunEvent(EASY_UNLOCK_TRIAL_RUN_EVENT_CLICKED_LOCK_ICON);
}

void EasyUnlockScreenlockStateHandler::OnScreenDidLock(
    ScreenlockBridge::LockHandler::ScreenType screen_type) {
  did_see_locked_phone_ = IsLockedState(state_);
  RefreshScreenlockState();
}

void EasyUnlockScreenlockStateHandler::OnScreenDidUnlock(
    ScreenlockBridge::LockHandler::ScreenType screen_type) {
  if (hardlock_state_ == LOGIN_FAILED)
    hardlock_state_ = NO_HARDLOCK;
  hardlock_ui_shown_ = false;
  is_trial_run_ = false;

  // Upon a successful unlock event, record whether the user's phone was locked
  // at any point while the lock screen was up.
  if (state_ == STATE_AUTHENTICATED)
    RecordEasyUnlockDidUserManuallyUnlockPhone(did_see_locked_phone_);
  did_see_locked_phone_ = false;
}

void EasyUnlockScreenlockStateHandler::OnFocusedUserChanged(
    const std::string& user_id) {
}

void EasyUnlockScreenlockStateHandler::RefreshScreenlockState() {
  State last_state = state_;
  // This should force updating screenlock state.
  state_ = STATE_INACTIVE;
  ChangeState(last_state);
}

void EasyUnlockScreenlockStateHandler::ShowHardlockUI() {
  DCHECK(hardlock_state_ != NO_HARDLOCK);

  if (!screenlock_bridge_->IsLocked())
    return;

  // Do not override online signin.
  const ScreenlockBridge::LockHandler::AuthType existing_auth_type =
      screenlock_bridge_->lock_handler()->GetAuthType(user_email_);
  if (existing_auth_type == ScreenlockBridge::LockHandler::ONLINE_SIGN_IN)
    return;

  if (existing_auth_type != ScreenlockBridge::LockHandler::OFFLINE_PASSWORD) {
    screenlock_bridge_->lock_handler()->SetAuthType(
        user_email_,
        ScreenlockBridge::LockHandler::OFFLINE_PASSWORD,
        base::string16());
  }

  if (hardlock_state_ == NO_PAIRING) {
    screenlock_bridge_->lock_handler()->HideUserPodCustomIcon(user_email_);
    hardlock_ui_shown_ = false;
    return;
  }

  if (hardlock_ui_shown_)
    return;

  ScreenlockBridge::UserPodCustomIconOptions icon_options;
  if (hardlock_state_ == LOGIN_FAILED) {
    icon_options.SetIcon(ScreenlockBridge::USER_POD_CUSTOM_ICON_LOCKED);
  } else if (hardlock_state_ == PAIRING_CHANGED ||
             hardlock_state_ == PAIRING_ADDED) {
    icon_options.SetIcon(
        ScreenlockBridge::USER_POD_CUSTOM_ICON_LOCKED_TO_BE_ACTIVATED);
  } else {
    icon_options.SetIcon(ScreenlockBridge::USER_POD_CUSTOM_ICON_HARDLOCKED);
  }

  base::string16 device_name = GetDeviceName();
  base::string16 tooltip;
  if (hardlock_state_ == USER_HARDLOCK) {
    tooltip = l10n_util::GetStringFUTF16(
        IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_HARDLOCK_USER, device_name);
  } else if (hardlock_state_ == PAIRING_CHANGED) {
    tooltip = l10n_util::GetStringUTF16(
        IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_HARDLOCK_PAIRING_CHANGED);
  } else if (hardlock_state_ == PAIRING_ADDED) {
    tooltip = l10n_util::GetStringFUTF16(
        IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_HARDLOCK_PAIRING_ADDED, device_name,
        device_name);
  } else if (hardlock_state_ == LOGIN_FAILED) {
    tooltip = l10n_util::GetStringUTF16(
        IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_LOGIN_FAILURE);
  } else {
    LOG(ERROR) << "Unknown hardlock state " << hardlock_state_;
  }
  icon_options.SetTooltip(tooltip, true /* autoshow */);

  screenlock_bridge_->lock_handler()->ShowUserPodCustomIcon(user_email_,
                                                            icon_options);
  hardlock_ui_shown_ = true;
}

void EasyUnlockScreenlockStateHandler::UpdateTooltipOptions(
    ScreenlockBridge::UserPodCustomIconOptions* icon_options) {
  size_t resource_id = 0;
  base::string16 device_name;
  if (is_trial_run_ && state_ == STATE_AUTHENTICATED) {
    resource_id = IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_INITIAL_AUTHENTICATED;
  } else {
    resource_id = GetTooltipResourceId(state_);
    if (TooltipContainsDeviceType(state_))
      device_name = GetDeviceName();
  }

  if (!resource_id)
    return;

  base::string16 tooltip;
  if (device_name.empty()) {
    tooltip = l10n_util::GetStringUTF16(resource_id);
  } else {
    tooltip = l10n_util::GetStringFUTF16(resource_id, device_name);
  }

  if (tooltip.empty())
    return;

  icon_options->SetTooltip(
      tooltip,
      is_trial_run_ || (state_ != STATE_AUTHENTICATED) /* autoshow tooltip */);
}

base::string16 EasyUnlockScreenlockStateHandler::GetDeviceName() {
#if defined(OS_CHROMEOS)
  return chromeos::GetChromeDeviceType();
#else
  // TODO(tbarzic): Figure out the name for non Chrome OS case.
  return base::ASCIIToUTF16("Chrome");
#endif
}

void EasyUnlockScreenlockStateHandler::UpdateScreenlockAuthType() {
  if (!is_trial_run_ && hardlock_state_ != NO_HARDLOCK)
    return;

  // Do not override online signin.
  const ScreenlockBridge::LockHandler::AuthType existing_auth_type =
      screenlock_bridge_->lock_handler()->GetAuthType(user_email_);
  DCHECK_NE(ScreenlockBridge::LockHandler::ONLINE_SIGN_IN, existing_auth_type);

  if (state_ == STATE_AUTHENTICATED) {
    if (existing_auth_type != ScreenlockBridge::LockHandler::USER_CLICK) {
      screenlock_bridge_->lock_handler()->SetAuthType(
          user_email_,
          ScreenlockBridge::LockHandler::USER_CLICK,
          l10n_util::GetStringUTF16(
              IDS_EASY_UNLOCK_SCREENLOCK_USER_POD_AUTH_VALUE));
    }
  } else if (existing_auth_type !=
             ScreenlockBridge::LockHandler::OFFLINE_PASSWORD) {
    screenlock_bridge_->lock_handler()->SetAuthType(
        user_email_,
        ScreenlockBridge::LockHandler::OFFLINE_PASSWORD,
        base::string16());
  }
}
