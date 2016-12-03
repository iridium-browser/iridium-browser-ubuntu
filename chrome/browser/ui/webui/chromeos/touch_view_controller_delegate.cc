// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/touch_view_controller_delegate.h"

#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/common/wm_shell.h"

namespace chromeos {

TouchViewControllerDelegate::TouchViewControllerDelegate() {
  ash::WmShell::Get()->AddShellObserver(this);
}

TouchViewControllerDelegate::~TouchViewControllerDelegate() {
  ash::WmShell::Get()->RemoveShellObserver(this);
}

void TouchViewControllerDelegate::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TouchViewControllerDelegate::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool TouchViewControllerDelegate::IsMaximizeModeEnabled() const {
  return ash::WmShell::Get()
      ->maximize_mode_controller()
      ->IsMaximizeModeWindowManagerEnabled();
}

void TouchViewControllerDelegate::OnMaximizeModeStarted() {
  FOR_EACH_OBSERVER(Observer, observers_, OnMaximizeModeStarted());
}

void TouchViewControllerDelegate::OnMaximizeModeEnded() {
  FOR_EACH_OBSERVER(Observer, observers_, OnMaximizeModeEnded());
}

}  // namespace chromeos
