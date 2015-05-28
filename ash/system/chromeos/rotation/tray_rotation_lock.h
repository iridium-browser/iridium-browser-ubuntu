// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_ROTATION_TRAY_ROTATION_LOCK_H_
#define ASH_SYSTEM_CHROMEOS_ROTATION_TRAY_ROTATION_LOCK_H_

#include "ash/content/display/screen_orientation_controller_chromeos.h"
#include "ash/shell_observer.h"
#include "ash/system/tray/tray_image_item.h"

namespace ash {

namespace tray {
class RotationLockDefaultView;
}  // namespace tray

// TrayRotationLock is a provider of views for the SystemTray. Both a tray view
// and a default view are provided. Each view indicates the current state of
// the rotation lock for the display which it appears on. The default view can
// be interacted with, it toggles the state of the rotation lock.
// TrayRotationLock is only available on the primary display.
class ASH_EXPORT TrayRotationLock
    : public TrayImageItem,
      public ScreenOrientationController::Observer,
      public ShellObserver {
 public:
  explicit TrayRotationLock(SystemTray* system_tray);
  ~TrayRotationLock() override;

  // ScreenOrientationController::Observer:
  void OnRotationLockChanged(bool rotation_locked) override;

  // SystemTrayItem:
  views::View* CreateDefaultView(user::LoginStatus status) override;

  // ShellObserver:
  void OnMaximizeModeStarted() override;
  void OnMaximizeModeEnded() override;

 protected:
  // TrayImageItem:
  bool GetInitialVisibility() override;

 private:
  friend class TrayRotationLockTest;

  // True if |on_primary_display_|, maximize mode is enabled, and rotation is
  // locked.
  bool ShouldBeVisible();

  // True if this is owned by a SystemTray on the primary display.
  bool OnPrimaryDisplay() const;

  DISALLOW_COPY_AND_ASSIGN(TrayRotationLock);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_ROTATION_TRAY_ROTATION_LOCK_H_
