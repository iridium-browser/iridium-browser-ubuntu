// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/power_button_controller.h"

#include "ash/common/accelerators/accelerator_controller.h"
#include "ash/common/ash_switches.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/system/audio/tray_audio.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/shell.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/session_state_animator.h"
#include "base/command_line.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/events/event_handler.h"
#include "ui/wm/core/compound_event_filter.h"

#if defined(OS_CHROMEOS)
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#endif

namespace ash {

PowerButtonController::PowerButtonController(LockStateController* controller)
    : power_button_down_(false),
      lock_button_down_(false),
      volume_down_pressed_(false),
#if defined(OS_CHROMEOS)
      volume_percent_before_screenshot_(0),
#endif
      brightness_is_zero_(false),
      internal_display_off_and_external_display_on_(false),
      has_legacy_power_button_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kAuraLegacyPowerButton)),
#if defined(OS_CHROMEOS)
      enable_quick_lock_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshEnableTouchView)),
#else
      enable_quick_lock_(false),
#endif
      controller_(controller) {
#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
      this);
  Shell::GetInstance()->display_configurator()->AddObserver(this);
#endif
  Shell::GetInstance()->PrependPreTargetHandler(this);
}

PowerButtonController::~PowerButtonController() {
  Shell::GetInstance()->RemovePreTargetHandler(this);
#if defined(OS_CHROMEOS)
  Shell::GetInstance()->display_configurator()->RemoveObserver(this);
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(
      this);
#endif
}

void PowerButtonController::OnScreenBrightnessChanged(double percent) {
  brightness_is_zero_ = percent <= 0.001;
}

void PowerButtonController::OnPowerButtonEvent(
    bool down,
    const base::TimeTicks& timestamp) {
  power_button_down_ = down;

  if (controller_->ShutdownRequested())
    return;

  // Avoid starting the lock/shutdown sequence if the power button is pressed
  // while the screen is off (http://crbug.com/128451), unless an external
  // display is still on (http://crosbug.com/p/24912).
  if (brightness_is_zero_ && !internal_display_off_and_external_display_on_)
    return;

  if (volume_down_pressed_ && down &&
      WmShell::Get()
          ->maximize_mode_controller()
          ->IsMaximizeModeWindowManagerEnabled()) {
    SystemTray* system_tray = Shell::GetInstance()->GetPrimarySystemTray();
    if (system_tray && system_tray->GetTrayAudio())
      system_tray->GetTrayAudio()->HideDetailedView(false);

    WmShell::Get()->accelerator_controller()->PerformActionIfEnabled(
        TAKE_SCREENSHOT);

#if defined(OS_CHROMEOS)
    // Restore volume.
    chromeos::CrasAudioHandler* audio_handler =
        chromeos::CrasAudioHandler::Get();
    audio_handler->SetOutputVolumePercentWithoutNotifyingObservers(
        volume_percent_before_screenshot_,
        chromeos::CrasAudioHandler::VOLUME_CHANGE_MAXIMIZE_MODE_SCREENSHOT);
#endif
    return;
  }

  const SessionStateDelegate* session_state_delegate =
      WmShell::Get()->GetSessionStateDelegate();
  if (has_legacy_power_button_) {
    // If power button releases won't get reported correctly because we're not
    // running on official hardware, just lock the screen or shut down
    // immediately.
    if (down) {
      if (session_state_delegate->CanLockScreen() &&
          !session_state_delegate->IsUserSessionBlocked() &&
          !controller_->LockRequested()) {
        controller_->StartLockAnimationAndLockImmediately(false);
      } else {
        controller_->RequestShutdown();
      }
    }
  } else {  // !has_legacy_power_button_
    if (down) {
      // If we already have a pending request to lock the screen, wait.
      if (controller_->LockRequested())
        return;

      if (session_state_delegate->CanLockScreen() &&
          !session_state_delegate->IsUserSessionBlocked()) {
        if (WmShell::Get()
                ->maximize_mode_controller()
                ->IsMaximizeModeWindowManagerEnabled() &&
            enable_quick_lock_)
          controller_->StartLockAnimationAndLockImmediately(true);
        else
          controller_->StartLockAnimation(true);
      } else {
        controller_->StartShutdownAnimation();
      }
    } else {  // Button is up.
      if (controller_->CanCancelLockAnimation())
        controller_->CancelLockAnimation();
      else if (controller_->CanCancelShutdownAnimation())
        controller_->CancelShutdownAnimation();
    }
  }
}

void PowerButtonController::OnLockButtonEvent(
    bool down,
    const base::TimeTicks& timestamp) {
  lock_button_down_ = down;

  const SessionStateDelegate* session_state_delegate =
      WmShell::Get()->GetSessionStateDelegate();
  if (!session_state_delegate->CanLockScreen() ||
      session_state_delegate->IsScreenLocked() ||
      controller_->LockRequested() || controller_->ShutdownRequested()) {
    return;
  }

  // Give the power button precedence over the lock button.
  if (power_button_down_)
    return;

  if (down)
    controller_->StartLockAnimation(false);
  else
    controller_->CancelLockAnimation();
}

void PowerButtonController::OnKeyEvent(ui::KeyEvent* event) {
  if (event->key_code() == ui::VKEY_VOLUME_DOWN) {
    volume_down_pressed_ = event->type() == ui::ET_KEY_PRESSED;
#if defined(OS_CHROMEOS)
    if (!event->is_repeat()) {
      chromeos::CrasAudioHandler* audio_handler =
          chromeos::CrasAudioHandler::Get();
      volume_percent_before_screenshot_ =
          audio_handler->GetOutputVolumePercent();
    }
#endif
  }
}

#if defined(OS_CHROMEOS)
void PowerButtonController::OnDisplayModeChanged(
    const ui::DisplayConfigurator::DisplayStateList& display_states) {
  bool internal_display_off = false;
  bool external_display_on = false;
  for (const ui::DisplaySnapshot* display : display_states) {
    if (display->type() == ui::DISPLAY_CONNECTION_TYPE_INTERNAL) {
      if (!display->current_mode())
        internal_display_off = true;
    } else if (display->current_mode()) {
      external_display_on = true;
    }
  }
  internal_display_off_and_external_display_on_ =
      internal_display_off && external_display_on;
}

void PowerButtonController::PowerButtonEventReceived(
    bool down,
    const base::TimeTicks& timestamp) {
  OnPowerButtonEvent(down, timestamp);
}
#endif  // defined(OS_CHROMEOS)

}  // namespace ash
