// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONTENT_DISPLAY_SCREEN_ORIENTATION_CONTROLLER_CHROMEOS_H_
#define ASH_CONTENT_DISPLAY_SCREEN_ORIENTATION_CONTROLLER_CHROMEOS_H_

#include <map>

#include "ash/ash_export.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/shell_observer.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/accelerometer/accelerometer_reader.h"
#include "chromeos/accelerometer/accelerometer_types.h"
#include "content/public/browser/screen_orientation_delegate.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/display.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Window;
}

namespace content {
class WebContents;
}

namespace ash {

// Implements ChromeOS specific functionality for ScreenOrientationProvider.
class ASH_EXPORT ScreenOrientationController
    : public aura::client::ActivationChangeObserver,
      public aura::WindowObserver,
      public chromeos::AccelerometerReader::Observer,
      public content::ScreenOrientationDelegate,
      public WindowTreeHostManager::Observer,
      public ShellObserver {
 public:
  // Observer that reports changes to the state of ScreenOrientationProvider's
  // rotation lock.
  class Observer {
   public:
    // Invoked when rotation is locked or unlocked.
    virtual void OnRotationLockChanged(bool rotation_locked) {}

   protected:
    virtual ~Observer() {}
  };

  ScreenOrientationController();
  ~ScreenOrientationController() override;

  // Add/Remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool ignore_display_configuration_updates() const {
    return ignore_display_configuration_updates_;
  }

  // True if |rotation_lock_| has been set and accelerometer updates should not
  // rotate the display.
  bool rotation_locked() const { return rotation_locked_; }

  // If |rotation_locked| future accelerometer updates should not change the
  // display rotation.
  void SetRotationLocked(bool rotation_locked);

  // Sets the display rotation for the given |source|. The new |rotation| will
  // also become active. Display changed notifications are surpressed for this
  // change.
  void SetDisplayRotation(gfx::Display::Rotation rotation,
                          gfx::Display::RotationSource source);

  // aura::client::ActivationChangeObserver:
  void OnWindowActivated(
      aura::client::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active) override;

  // aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowDestroying(aura::Window* window) override;

  // chromeos::AccelerometerReader::Observer:
  void OnAccelerometerUpdated(
      scoped_refptr<const chromeos::AccelerometerUpdate> update) override;

  // content::ScreenOrientationDelegate:
  bool FullScreenRequired(content::WebContents* web_contents) override;
  void Lock(content::WebContents* web_contents,
            blink::WebScreenOrientationLockType lock_orientation) override;
  bool ScreenOrientationProviderSupported() override;
  void Unlock(content::WebContents* web_contents) override;

  // WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanged() override;

  // ShellObserver:
  void OnMaximizeModeStarted() override;
  void OnMaximizeModeEnded() override;

 private:
  // Sets the display rotation to |rotation|. Future accelerometer updates
  // should not be used to change the rotation. SetRotationLocked(false) removes
  // the rotation lock.
  void LockRotation(gfx::Display::Rotation rotation,
                    gfx::Display::RotationSource source);

  // Sets the display rotation based on |lock_orientation|. Future accelerometer
  // updates should not be used to change the rotation. SetRotationLocked(false)
  // removes the rotation lock.
  void LockRotationToOrientation(
      blink::WebScreenOrientationLockType lock_orientation);

  // Locks rotation to the angle matching the primary orientation for
  // |lock_orientation|.
  void LockRotationToPrimaryOrientation(
      blink::WebScreenOrientationLockType lock_orientation);

  // Locks rotation to the angle matching the secondary orientation for
  // |lock_orientation|.
  void LockRotationToSecondaryOrientation(
      blink::WebScreenOrientationLockType lock_orientation);

  // For orientations that do not specify primary or secondary, locks to the
  // current rotation if it matches |lock_orientation|. Otherwise locks to a
  // matching rotation.
  void LockToRotationMatchingOrientation(
      blink::WebScreenOrientationLockType lock_orientation);

  // Detect screen rotation from |lid| accelerometer and automatically rotate
  // screen.
  void HandleScreenRotation(const chromeos::AccelerometerReading& lid);

  // Checks DisplayManager for registered rotation lock, and rotation,
  // preferences. These are then applied.
  void LoadDisplayRotationProperties();

  // Determines the rotation lock, and orientation, for the currently active
  // window, and applies it. If there is none, rotation lock will be removed.
  void ApplyLockForActiveWindow();

  // Removes a window and its locking preference.
  void RemoveLockingWindow(aura::Window* window);

  // Both |blink::WebScreenOrientationLockLandscape| and
  // |blink::WebScreenOrientationLockPortrait| allow for rotation between the
  // two angles of the same screen orientation
  // (http://www.w3.org/TR/screen-orientation/). Returns true if |rotation| is
  // supported for the current |rotation_locked_orientation_|.
  bool IsRotationAllowedInLockedState(gfx::Display::Rotation rotation);

  // Certain orientation locks allow for rotation between the two angles of the
  // same screen orientation. Returns true if |rotation_locked_orientation_|
  // allows rotation.
  bool CanRotateInLockedState();

  // The orientation of the display when at a rotation of 0.
  blink::WebScreenOrientationLockType natural_orientation_;

  // True when changes being applied cause OnDisplayConfigurationChanged() to be
  // called, and for which these changes should be ignored.
  bool ignore_display_configuration_updates_;

  // When true then accelerometer updates should not rotate the display.
  bool rotation_locked_;

  // The orientation to which the current |rotation_locked_| was applied.
  blink::WebScreenOrientationLockType rotation_locked_orientation_;

  // The rotation of the display set by the user. This rotation will be
  // restored upon exiting maximize mode.
  gfx::Display::Rotation user_rotation_;

  // The current rotation set by ScreenOrientationController for the internal
  // display.
  gfx::Display::Rotation current_rotation_;

  // Rotation Lock observers.
  base::ObserverList<Observer> observers_;

  // Tracks all windows that have requested a lock, as well as the requested
  // orientation.
  std::map<aura::Window*, blink::WebScreenOrientationLockType> locking_windows_;

  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationController);
};

}  // namespace ash

#endif  // ASH_CONTENT_DISPLAY_SCREEN_ORIENTATION_CONTROLLER_CHROMEOS_H_
