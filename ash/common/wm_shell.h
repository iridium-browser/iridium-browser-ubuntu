// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_SHELL_H_
#define ASH_COMMON_WM_SHELL_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/common/media_delegate.h"
#include "ash/common/metrics/gesture_action_type.h"
#include "ash/common/metrics/user_metrics_action.h"
#include "ash/common/wm/lock_state_observer.h"
#include "base/observer_list.h"
#include "ui/base/ui_base_types.h"

namespace display {
class Display;
}

namespace gfx {
class Insets;
class Point;
}

namespace views {
class PointerWatcher;
}

namespace ash {

class AcceleratorController;
class AccessibilityDelegate;
class BrightnessControlDelegate;
class DisplayInfo;
class FocusCycler;
class ImmersiveContextAsh;
class ImmersiveFullscreenController;
class KeyEventWatcher;
class KeyboardBrightnessControlDelegate;
class KeyboardUI;
class MaximizeModeController;
class MruWindowTracker;
class NewWindowDelegate;
class PaletteDelegate;
class ScopedDisableInternalMouseAndKeyboard;
class SessionStateDelegate;
class ShelfDelegate;
class ShelfModel;
class ShelfWindowWatcher;
class ShellDelegate;
class ShellObserver;
class SystemTrayDelegate;
class SystemTrayNotifier;
class ToastManager;
class WallpaperDelegate;
class WindowCycleController;
class WindowCycleEventFilter;
class WindowResizer;
class WindowSelectorController;
class WmActivationObserver;
class WmDisplayObserver;
class WmWindow;

enum class TaskSwitchSource;

namespace wm {
class MaximizeModeEventHandler;
class WindowState;
}

#if defined(OS_CHROMEOS)
class LogoutConfirmationController;
#endif

// Similar to ash::Shell. Eventually the two will be merged.
class ASH_EXPORT WmShell {
 public:
  // This is necessary for a handful of places that is difficult to plumb
  // through context.
  static void Set(WmShell* instance);
  static WmShell* Get();
  static bool HasInstance() { return instance_ != nullptr; }

  void Initialize();
  virtual void Shutdown();

  ShellDelegate* delegate() { return delegate_.get(); }

  AcceleratorController* accelerator_controller() {
    return accelerator_controller_.get();
  }

  AccessibilityDelegate* accessibility_delegate() {
    return accessibility_delegate_.get();
  }

  BrightnessControlDelegate* brightness_control_delegate() {
    return brightness_control_delegate_.get();
  }

  FocusCycler* focus_cycler() { return focus_cycler_.get(); }

  KeyboardBrightnessControlDelegate* keyboard_brightness_control_delegate() {
    return keyboard_brightness_control_delegate_.get();
  }

  KeyboardUI* keyboard_ui() { return keyboard_ui_.get(); }

  MaximizeModeController* maximize_mode_controller() {
    return maximize_mode_controller_.get();
  }

  MruWindowTracker* mru_window_tracker() { return mru_window_tracker_.get(); }

  MediaDelegate* media_delegate() { return media_delegate_.get(); }

  NewWindowDelegate* new_window_delegate() {
    return new_window_delegate_.get();
  }

  // NOTE: Prefer ScopedRootWindowForNewWindows when setting temporarily.
  void set_root_window_for_new_windows(WmWindow* root) {
    root_window_for_new_windows_ = root;
  }

  PaletteDelegate* palette_delegate() { return palette_delegate_.get(); }

  ShelfDelegate* shelf_delegate() { return shelf_delegate_.get(); }

  ShelfModel* shelf_model() { return shelf_model_.get(); }

  SystemTrayNotifier* system_tray_notifier() {
    return system_tray_notifier_.get();
  }

  SystemTrayDelegate* system_tray_delegate() {
    return system_tray_delegate_.get();
  }

  ToastManager* toast_manager() { return toast_manager_.get(); }

  WallpaperDelegate* wallpaper_delegate() { return wallpaper_delegate_.get(); }

  WindowCycleController* window_cycle_controller() {
    return window_cycle_controller_.get();
  }

  WindowSelectorController* window_selector_controller() {
    return window_selector_controller_.get();
  }

  // Creates a new window used as a container of other windows. No painting is
  // done to the created window.
  virtual WmWindow* NewContainerWindow() = 0;

  virtual WmWindow* GetFocusedWindow() = 0;
  virtual WmWindow* GetActiveWindow() = 0;

  virtual WmWindow* GetCaptureWindow() = 0;

  virtual WmWindow* GetPrimaryRootWindow() = 0;

  // Returns the root window for the specified display.
  virtual WmWindow* GetRootWindowForDisplayId(int64_t display_id) = 0;

  // Returns the root window that newly created windows should be added to.
  // Value can be temporarily overridden using ScopedRootWindowForNewWindows.
  // NOTE: this returns the root, newly created window should be added to the
  // appropriate container in the returned window.
  WmWindow* GetRootWindowForNewWindows();

  // Retuns the display info associated with |display_id|.
  // TODO(mash): Remove when DisplayManager has been moved. crbug.com/622480
  virtual const DisplayInfo& GetDisplayInfo(int64_t display_id) const = 0;

  // Matches that of DisplayManager::IsActiveDisplayId().
  // TODO(mash): Remove when DisplayManager has been moved. crbug.com/622480
  virtual bool IsActiveDisplayId(int64_t display_id) const = 0;

  // Returns true if the desktop is in unified mode.
  // TODO(mash): Remove when DisplayManager has been moved. crbug.com/622480
  virtual bool IsInUnifiedMode() const = 0;

  // Returns the first display; this is the first display listed by hardware,
  // which corresponds to internal displays on devices with integrated displays.
  // TODO(mash): Remove when DisplayManager has been moved. crbug.com/622480
  virtual display::Display GetFirstDisplay() const = 0;

  // Returns true if the first window shown on first run should be
  // unconditionally maximized, overriding the heuristic that normally chooses
  // the window size.
  virtual bool IsForceMaximizeOnFirstRun() = 0;

  // Sets work area insets of the display containing |window|, pings observers.
  virtual void SetDisplayWorkAreaInsets(WmWindow* window,
                                        const gfx::Insets& insets) = 0;

  // Returns true if a system-modal dialog window is currently open.
  bool IsSystemModalWindowOpen();

  // For testing only: set simulation that a modal window is open
  void SimulateModalWindowOpenForTesting(bool modal_window_open) {
    simulate_modal_window_open_for_testing_ = modal_window_open;
  }

  // Shows the app list on the active root window.
  void ShowAppList();

  // Dismisses the app list.
  void DismissAppList();

  // Shows the app list if it's not visible. Dismisses it otherwise.
  void ToggleAppList();

  // Returns app list actual visibility. This might differ from
  // GetAppListTargetVisibility() when hiding animation is still in flight.
  bool IsApplistVisible() const;

  // Returns app list target visibility.
  bool GetAppListTargetVisibility() const;

  // Returns true if a window is currently pinned.
  virtual bool IsPinned() = 0;

  // Sets/Unsets the |window| to as a pinned window. If this is called with a
  // window with WINDOW_STATE_TYPE_PINNED state, then this sets the |window|
  // as a pinned window. Otherwise, this unsets it.
  // For setting, a caller needs to guarantee that no windows are set
  // as pinned window. For unsetting, a caller needs to guarantee that the
  // |window| is the one which is currently set as a pinned window via previous
  // this function invocation.
  virtual void SetPinnedWindow(WmWindow* window) = 0;

  // Returns true if |window| can be shown for the current user. This is
  // intended to check if the current user matches the user associated with
  // |window|.
  // TODO(jamescook): Remove this when ShellDelegate has been moved.
  virtual bool CanShowWindowForUser(WmWindow* window) = 0;

  // See aura::client::CursorClient for details on these.
  virtual void LockCursor() = 0;
  virtual void UnlockCursor() = 0;
  virtual bool IsMouseEventsEnabled() = 0;

  virtual std::vector<WmWindow*> GetAllRootWindows() = 0;

  virtual void RecordGestureAction(GestureActionType action) = 0;
  virtual void RecordUserMetricsAction(UserMetricsAction action) = 0;
  virtual void RecordTaskSwitchMetric(TaskSwitchSource source) = 0;

  // Shows the context menu for the background and the shelf at
  // |location_in_screen|.
  virtual void ShowContextMenu(const gfx::Point& location_in_screen,
                               ui::MenuSourceType source_type) = 0;

  // Returns a WindowResizer to handle dragging. |next_window_resizer| is
  // the next WindowResizer in the WindowResizer chain. This may return
  // |next_window_resizer|.
  virtual std::unique_ptr<WindowResizer> CreateDragWindowResizer(
      std::unique_ptr<WindowResizer> next_window_resizer,
      wm::WindowState* window_state) = 0;

  virtual std::unique_ptr<WindowCycleEventFilter>
  CreateWindowCycleEventFilter() = 0;

  virtual std::unique_ptr<wm::MaximizeModeEventHandler>
  CreateMaximizeModeEventHandler() = 0;

  virtual std::unique_ptr<ScopedDisableInternalMouseAndKeyboard>
  CreateScopedDisableInternalMouseAndKeyboard() = 0;

  virtual std::unique_ptr<ImmersiveFullscreenController>
  CreateImmersiveFullscreenController() = 0;

  virtual std::unique_ptr<KeyEventWatcher> CreateKeyEventWatcher() = 0;

  void CreateShelfDelegate();

  // Called after maximize mode has started, windows might still animate though.
  void OnMaximizeModeStarted();

  // Called after maximize mode has ended, windows might still be returning to
  // their original position.
  void OnMaximizeModeEnded();

  // Called when the overview mode is about to be started (before the windows
  // get re-arranged).
  virtual void OnOverviewModeStarting() = 0;

  // Called after overview mode has ended.
  virtual void OnOverviewModeEnded() = 0;

  // Notify observers that fullscreen mode has changed for |root_window|.
  void NotifyFullscreenStateChanged(bool is_fullscreen, WmWindow* root_window);

  // Notify observers that |pinned_window| changed its pinned window state.
  void NotifyPinnedStateChanged(WmWindow* pinned_window);

  // Notify observers that the virtual keyboard has been activated/deactivated.
  void NotifyVirtualKeyboardActivated(bool activated);

  // Notify observers that the shelf was created for |root_window|.
  // TODO(jamescook): Move to Shelf.
  void NotifyShelfCreatedForRootWindow(WmWindow* root_window);

  // Notify observers that |root_window|'s shelf changed auto-hide alignment.
  // TODO(jamescook): Move to Shelf.
  void NotifyShelfAlignmentChanged(WmWindow* root_window);

  // Notify observers that |root_window|'s shelf changed auto-hide behavior.
  // TODO(jamescook): Move to Shelf.
  void NotifyShelfAutoHideBehaviorChanged(WmWindow* root_window);

  virtual SessionStateDelegate* GetSessionStateDelegate() = 0;

  virtual void AddActivationObserver(WmActivationObserver* observer) = 0;
  virtual void RemoveActivationObserver(WmActivationObserver* observer) = 0;

  virtual void AddDisplayObserver(WmDisplayObserver* observer) = 0;
  virtual void RemoveDisplayObserver(WmDisplayObserver* observer) = 0;

  void AddShellObserver(ShellObserver* observer);
  void RemoveShellObserver(ShellObserver* observer);

  // If |wants_moves| is true PointerWatcher::OnPointerEventObserved() is
  // called for pointer move events. Enabling pointer moves may incur a
  // performance hit and should be avoided if possible.
  virtual void AddPointerWatcher(views::PointerWatcher* watcher,
                                 bool wants_moves) = 0;
  virtual void RemovePointerWatcher(views::PointerWatcher* watcher) = 0;

  // TODO: Move these back to LockStateController when that has been moved.
  void OnLockStateEvent(LockStateObserver::EventType event);
  void AddLockStateObserver(LockStateObserver* observer);
  void RemoveLockStateObserver(LockStateObserver* observer);

  void SetShelfDelegateForTesting(std::unique_ptr<ShelfDelegate> test_delegate);
  void SetPaletteDelegateForTesting(
      std::unique_ptr<PaletteDelegate> palette_delegate);

  // True if any touch points are down.
  virtual bool IsTouchDown() = 0;

#if defined(OS_CHROMEOS)
  LogoutConfirmationController* logout_confirmation_controller() {
    return logout_confirmation_controller_.get();
  }

  // TODO(jamescook): Remove this when VirtualKeyboardController has been moved.
  virtual void ToggleIgnoreExternalKeyboard() = 0;

  // Enable or disable the laser pointer.
  virtual void SetLaserPointerEnabled(bool enabled) = 0;
#endif

 protected:
  explicit WmShell(std::unique_ptr<ShellDelegate> shell_delegate);
  virtual ~WmShell();

  base::ObserverList<ShellObserver>* shell_observers() {
    return &shell_observers_;
  }

  void SetKeyboardUI(std::unique_ptr<KeyboardUI> keyboard_ui);

  // Helpers to set (and initialize) or destroy various delegates.
  // TODO(msw|jamescook): Remove these once ShellDelegate, etc. are ported.
  void SetSystemTrayDelegate(std::unique_ptr<SystemTrayDelegate> delegate);
  void DeleteSystemTrayDelegate();

  void DeleteWindowCycleController();

  void DeleteWindowSelectorController();

  void CreateMaximizeModeController();
  void DeleteMaximizeModeController();

  void CreateMruWindowTracker();
  void DeleteMruWindowTracker();

  void DeleteToastManager();

  void SetAcceleratorController(
      std::unique_ptr<AcceleratorController> accelerator_controller);

 private:
  friend class AcceleratorControllerTest;
  friend class ScopedRootWindowForNewWindows;
  friend class Shell;

  static WmShell* instance_;

  base::ObserverList<ShellObserver> shell_observers_;
  std::unique_ptr<ShellDelegate> delegate_;

  std::unique_ptr<AcceleratorController> accelerator_controller_;
  std::unique_ptr<AccessibilityDelegate> accessibility_delegate_;
  std::unique_ptr<BrightnessControlDelegate> brightness_control_delegate_;
  std::unique_ptr<FocusCycler> focus_cycler_;
  std::unique_ptr<ImmersiveContextAsh> immersive_context_;
  std::unique_ptr<KeyboardBrightnessControlDelegate>
      keyboard_brightness_control_delegate_;
  std::unique_ptr<KeyboardUI> keyboard_ui_;
  std::unique_ptr<MaximizeModeController> maximize_mode_controller_;
  std::unique_ptr<MediaDelegate> media_delegate_;
  std::unique_ptr<MruWindowTracker> mru_window_tracker_;
  std::unique_ptr<NewWindowDelegate> new_window_delegate_;
  std::unique_ptr<PaletteDelegate> palette_delegate_;
  std::unique_ptr<ShelfDelegate> shelf_delegate_;
  std::unique_ptr<ShelfModel> shelf_model_;
  std::unique_ptr<ShelfWindowWatcher> shelf_window_watcher_;
  std::unique_ptr<SystemTrayNotifier> system_tray_notifier_;
  std::unique_ptr<SystemTrayDelegate> system_tray_delegate_;
  std::unique_ptr<ToastManager> toast_manager_;
  std::unique_ptr<WallpaperDelegate> wallpaper_delegate_;
  std::unique_ptr<WindowCycleController> window_cycle_controller_;
  std::unique_ptr<WindowSelectorController> window_selector_controller_;

  base::ObserverList<LockStateObserver> lock_state_observers_;

  // See comment for GetRootWindowForNewWindows().
  WmWindow* root_window_for_new_windows_ = nullptr;
  WmWindow* scoped_root_window_for_new_windows_ = nullptr;

  bool simulate_modal_window_open_for_testing_ = false;

#if defined(OS_CHROMEOS)
  std::unique_ptr<LogoutConfirmationController> logout_confirmation_controller_;
#endif
};

}  // namespace ash

#endif  // ASH_COMMON_WM_SHELL_H_
