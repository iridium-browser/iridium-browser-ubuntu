// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_FAKE_INPUT_DEVICE_SETTINGS_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_FAKE_INPUT_DEVICE_SETTINGS_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"

namespace chromeos {
namespace system {

// This fake just memorizes current values of input devices settings.
class FakeInputDeviceSettings : public InputDeviceSettings {
 public:
  FakeInputDeviceSettings();
  ~FakeInputDeviceSettings() override;

  // Overridden from InputDeviceSettings.
  void TouchpadExists(const DeviceExistsCallback& callback) override;
  void UpdateTouchpadSettings(const TouchpadSettings& settings) override;
  void SetTouchpadSensitivity(int value) override;
  void SetTapToClick(bool enabled) override;
  void SetThreeFingerClick(bool enabled) override;
  void SetTapDragging(bool enabled) override;
  void MouseExists(const DeviceExistsCallback& callback) override;
  void UpdateMouseSettings(const MouseSettings& settings) override;
  void SetMouseSensitivity(int value) override;
  void SetPrimaryButtonRight(bool right) override;
  void SetNaturalScroll(bool enabled) override;
  void ReapplyTouchpadSettings() override;
  void ReapplyMouseSettings() override;

  const TouchpadSettings& current_touchpad_settings() const {
    return current_touchpad_settings_;
  }

  const MouseSettings& current_mouse_settings() const {
    return current_mouse_settings_;
  }

 private:
  TouchpadSettings current_touchpad_settings_;
  MouseSettings current_mouse_settings_;

  DISALLOW_COPY_AND_ASSIGN(FakeInputDeviceSettings);
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_FAKE_INPUT_DEVICE_SETTINGS_H_
