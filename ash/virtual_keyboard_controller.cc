// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/virtual_keyboard_controller.h"

#include <vector>

#include "ash/common/ash_switches.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"

namespace ash {
namespace {

// Checks whether smart deployment is enabled.
bool IsSmartVirtualKeyboardEnabled() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          keyboard::switches::kEnableVirtualKeyboard)) {
    return false;
  }
  return keyboard::IsSmartDeployEnabled();
}

}  // namespace

VirtualKeyboardController::VirtualKeyboardController()
    : has_external_keyboard_(false),
      has_internal_keyboard_(false),
      has_touchscreen_(false),
      ignore_external_keyboard_(false) {
  WmShell::Get()->AddShellObserver(this);
  ui::InputDeviceManager::GetInstance()->AddObserver(this);
  UpdateDevices();
}

VirtualKeyboardController::~VirtualKeyboardController() {
  WmShell::Get()->RemoveShellObserver(this);
  ui::InputDeviceManager::GetInstance()->RemoveObserver(this);
}

void VirtualKeyboardController::OnMaximizeModeStarted() {
  if (!IsSmartVirtualKeyboardEnabled()) {
    SetKeyboardEnabled(true);
  } else {
    UpdateKeyboardEnabled();
  }
}

void VirtualKeyboardController::OnMaximizeModeEnded() {
  if (!IsSmartVirtualKeyboardEnabled()) {
    SetKeyboardEnabled(false);
  } else {
    UpdateKeyboardEnabled();
  }
}

void VirtualKeyboardController::OnTouchscreenDeviceConfigurationChanged() {
  UpdateDevices();
}

void VirtualKeyboardController::OnKeyboardDeviceConfigurationChanged() {
  UpdateDevices();
}

void VirtualKeyboardController::ToggleIgnoreExternalKeyboard() {
  ignore_external_keyboard_ = !ignore_external_keyboard_;
  UpdateKeyboardEnabled();
}

void VirtualKeyboardController::UpdateDevices() {
  ui::InputDeviceManager* device_data_manager =
      ui::InputDeviceManager::GetInstance();

  // Checks for touchscreens.
  has_touchscreen_ = device_data_manager->GetTouchscreenDevices().size() > 0;

  // Checks for keyboards.
  has_external_keyboard_ = false;
  has_internal_keyboard_ = false;
  for (const ui::InputDevice& device :
       device_data_manager->GetKeyboardDevices()) {
    if (has_internal_keyboard_ && has_external_keyboard_)
      break;
    ui::InputDeviceType type = device.type;
    if (type == ui::InputDeviceType::INPUT_DEVICE_INTERNAL)
      has_internal_keyboard_ = true;
    if (type == ui::InputDeviceType::INPUT_DEVICE_EXTERNAL)
      has_external_keyboard_ = true;
  }
  // Update keyboard state.
  UpdateKeyboardEnabled();
}

void VirtualKeyboardController::UpdateKeyboardEnabled() {
  if (!IsSmartVirtualKeyboardEnabled()) {
    SetKeyboardEnabled(WmShell::Get()
                           ->maximize_mode_controller()
                           ->IsMaximizeModeWindowManagerEnabled());
    return;
  }
  bool ignore_internal_keyboard = WmShell::Get()
                                      ->maximize_mode_controller()
                                      ->IsMaximizeModeWindowManagerEnabled();
  bool is_internal_keyboard_active =
      has_internal_keyboard_ && !ignore_internal_keyboard;
  SetKeyboardEnabled(!is_internal_keyboard_active && has_touchscreen_ &&
                     (!has_external_keyboard_ || ignore_external_keyboard_));
  WmShell::Get()
      ->system_tray_notifier()
      ->NotifyVirtualKeyboardSuppressionChanged(!is_internal_keyboard_active &&
                                                has_touchscreen_ &&
                                                has_external_keyboard_);
}

void VirtualKeyboardController::SetKeyboardEnabled(bool enabled) {
  bool was_enabled = keyboard::IsKeyboardEnabled();
  keyboard::SetTouchKeyboardEnabled(enabled);
  bool is_enabled = keyboard::IsKeyboardEnabled();
  if (is_enabled == was_enabled)
    return;
  if (is_enabled) {
    Shell::GetInstance()->CreateKeyboard();
  } else {
    Shell::GetInstance()->DeactivateKeyboard();
  }
}

}  // namespace ash
