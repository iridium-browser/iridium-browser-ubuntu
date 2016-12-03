// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELL_WINDOW_IDS_H_
#define ASH_COMMON_SHELL_WINDOW_IDS_H_

#include <stddef.h>

#include "ash/ash_export.h"

// Declarations of ids of special shell windows.

namespace ash {

// Used to indicate no shell window id.
const int kShellWindowId_Invalid = -1;

// A higher-level container that holds all of the containers stacked below
// kShellWindowId_LockScreenContainer.  Only used by PowerButtonController for
// animating lower-level containers.
const int kShellWindowId_NonLockScreenContainersContainer = 0;

// A higher-level container that holds containers that hold lock-screen
// windows.  Only used by PowerButtonController for animating lower-level
// containers.
const int kShellWindowId_LockScreenContainersContainer = 1;

// A higher-level container that holds containers that hold lock-screen-related
// windows (which we want to display while the screen is locked; effectively
// containers stacked above kShellWindowId_LockSystemModalContainer).  Only used
// by PowerButtonController for animating lower-level containers.
const int kShellWindowId_LockScreenRelatedContainersContainer = 2;

// A container used for windows of WINDOW_TYPE_CONTROL that have no parent.
// This container is not visible.
const int kShellWindowId_UnparentedControlContainer = 3;

// The desktop background window.
const int kShellWindowId_DesktopBackgroundContainer = 4;

// The virtual keyboard container.
// NOTE: this is lazily created.
const int kShellWindowId_VirtualKeyboardContainer = 5;

// The container for standard top-level windows.
const int kShellWindowId_DefaultContainer = 6;

// The container for top-level windows with the 'always-on-top' flag set.
const int kShellWindowId_AlwaysOnTopContainer = 7;

// The container for windows docked to either side of the desktop.
const int kShellWindowId_DockedContainer = 8;

// The container for the shelf.
const int kShellWindowId_ShelfContainer = 9;

// The container for bubbles which float over the shelf.
const int kShellWindowId_ShelfBubbleContainer = 10;

// The container for panel windows.
const int kShellWindowId_PanelContainer = 11;

// The container for the app list.
const int kShellWindowId_AppListContainer = 12;

// The container for user-specific modal windows.
const int kShellWindowId_SystemModalContainer = 13;

// The container for the lock screen background.
const int kShellWindowId_LockScreenBackgroundContainer = 14;

// The container for the lock screen.
const int kShellWindowId_LockScreenContainer = 15;

// The container for the lock screen modal windows.
const int kShellWindowId_LockSystemModalContainer = 16;

// The container for the status area.
const int kShellWindowId_StatusContainer = 17;

// A parent container that holds the virtual keyboard container and ime windows
// if any. This is to ensure that the virtual keyboard or ime window is stacked
// above most containers but below the mouse cursor and the power off animation.
const int kShellWindowId_ImeWindowParentContainer = 18;

// The container for menus.
const int kShellWindowId_MenuContainer = 19;

// The container for drag/drop images and tooltips.
const int kShellWindowId_DragImageAndTooltipContainer = 20;

// The container for bubbles briefly overlaid onscreen to show settings changes
// (volume, brightness, input method bubbles, etc.).
const int kShellWindowId_SettingBubbleContainer = 21;

// The container for special components overlaid onscreen, such as the
// region selector for partial screenshots.
const int kShellWindowId_OverlayContainer = 22;

// ID of the window created by PhantomWindowController or DragWindowController.
const int kShellWindowId_PhantomWindow = 23;

// The container for mouse cursor.
const int kShellWindowId_MouseCursorContainer = 24;

// The topmost container, used for power off animation.
const int kShellWindowId_PowerButtonAnimationContainer = 25;

const int kShellWindowId_Min = 0;
const int kShellWindowId_Max = kShellWindowId_PowerButtonAnimationContainer;

// These are the list of container ids of containers which may contain windows
// that need to be activated.
ASH_EXPORT extern const int kActivatableShellWindowIds[];
ASH_EXPORT extern const size_t kNumActivatableShellWindowIds;

// Returns true if |id| is in |kActivatableShellWindowIds|.
ASH_EXPORT bool IsActivatableShellWindowId(int id);

}  // namespace ash

#endif  // ASH_COMMON_SHELL_WINDOW_IDS_H_
