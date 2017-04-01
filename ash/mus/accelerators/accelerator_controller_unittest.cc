// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/accelerators/accelerator_controller.h"

#include "ash/common/accelerators/accelerator_table.h"
#include "ash/common/accessibility_delegate.h"
#include "ash/common/accessibility_types.h"
#include "ash/common/ash_switches.h"
#include "ash/common/ime_control_delegate.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/system/brightness_control_delegate.h"
#include "ash/common/system/keyboard_brightness_control_delegate.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/test/ash_test.h"
#include "ash/common/wm/panels/panel_layout_manager.h"
#include "ash/common/wm/window_positioning_utils.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm/wm_event.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/mus/accelerators/accelerator_controller_registrar_test_api.h"
#include "ash/mus/bridge/wm_shell_mus_test_api.h"
#include "ash/mus/property_util.h"
#include "ash/mus/test/wm_test_base.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "base/command_line.h"
#include "base/test/user_action_tester.cc"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_processor.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/views/widget/widget.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#include "ui/events/test/events_test_utils_x11.h"
#endif

namespace ash {

namespace {

class TestTarget : public ui::AcceleratorTarget {
 public:
  TestTarget() : accelerator_pressed_count_(0), accelerator_repeat_count_(0) {}
  ~TestTarget() override {}

  int accelerator_pressed_count() const { return accelerator_pressed_count_; }

  int accelerator_repeat_count() const { return accelerator_repeat_count_; }

  void reset() {
    accelerator_pressed_count_ = 0;
    accelerator_repeat_count_ = 0;
  }

  // Overridden from ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

 private:
  int accelerator_pressed_count_;
  int accelerator_repeat_count_;

  DISALLOW_COPY_AND_ASSIGN(TestTarget);
};

class ReleaseAccelerator : public ui::Accelerator {
 public:
  ReleaseAccelerator(ui::KeyboardCode keycode, int modifiers)
      : ui::Accelerator(keycode, modifiers) {
    set_type(ui::ET_KEY_RELEASED);
  }
};

class DummyBrightnessControlDelegate : public BrightnessControlDelegate {
 public:
  DummyBrightnessControlDelegate()
      : handle_brightness_down_count_(0), handle_brightness_up_count_(0) {}
  ~DummyBrightnessControlDelegate() override {}

  void HandleBrightnessDown(const ui::Accelerator& accelerator) override {
    ++handle_brightness_down_count_;
    last_accelerator_ = accelerator;
  }
  void HandleBrightnessUp(const ui::Accelerator& accelerator) override {
    ++handle_brightness_up_count_;
    last_accelerator_ = accelerator;
  }
  void SetBrightnessPercent(double percent, bool gradual) override {}
  void GetBrightnessPercent(
      const base::Callback<void(double)>& callback) override {
    callback.Run(100.0);
  }

  int handle_brightness_down_count() const {
    return handle_brightness_down_count_;
  }
  int handle_brightness_up_count() const { return handle_brightness_up_count_; }
  const ui::Accelerator& last_accelerator() const { return last_accelerator_; }

 private:
  int handle_brightness_down_count_;
  int handle_brightness_up_count_;
  ui::Accelerator last_accelerator_;

  DISALLOW_COPY_AND_ASSIGN(DummyBrightnessControlDelegate);
};

class DummyImeControlDelegate : public ImeControlDelegate {
 public:
  DummyImeControlDelegate()
      : handle_next_ime_count_(0),
        handle_previous_ime_count_(0),
        handle_switch_ime_count_(0) {}
  ~DummyImeControlDelegate() override {}

  bool CanCycleIme() override { return true; }
  void HandleNextIme() override { ++handle_next_ime_count_; }
  void HandlePreviousIme() override { ++handle_previous_ime_count_; }
  bool CanSwitchIme(const ui::Accelerator& accelerator) override {
    return true;
  }
  void HandleSwitchIme(const ui::Accelerator& accelerator) override {
    ++handle_switch_ime_count_;
  }

  int handle_next_ime_count() const { return handle_next_ime_count_; }
  int handle_previous_ime_count() const { return handle_previous_ime_count_; }
  int handle_switch_ime_count() const { return handle_switch_ime_count_; }

 private:
  int handle_next_ime_count_;
  int handle_previous_ime_count_;
  int handle_switch_ime_count_;

  DISALLOW_COPY_AND_ASSIGN(DummyImeControlDelegate);
};

class DummyKeyboardBrightnessControlDelegate
    : public KeyboardBrightnessControlDelegate {
 public:
  DummyKeyboardBrightnessControlDelegate()
      : handle_keyboard_brightness_down_count_(0),
        handle_keyboard_brightness_up_count_(0) {}
  ~DummyKeyboardBrightnessControlDelegate() override {}

  void HandleKeyboardBrightnessDown(
      const ui::Accelerator& accelerator) override {
    ++handle_keyboard_brightness_down_count_;
    last_accelerator_ = accelerator;
  }

  void HandleKeyboardBrightnessUp(const ui::Accelerator& accelerator) override {
    ++handle_keyboard_brightness_up_count_;
    last_accelerator_ = accelerator;
  }

  int handle_keyboard_brightness_down_count() const {
    return handle_keyboard_brightness_down_count_;
  }

  int handle_keyboard_brightness_up_count() const {
    return handle_keyboard_brightness_up_count_;
  }

  const ui::Accelerator& last_accelerator() const { return last_accelerator_; }

 private:
  int handle_keyboard_brightness_down_count_;
  int handle_keyboard_brightness_up_count_;
  ui::Accelerator last_accelerator_;

  DISALLOW_COPY_AND_ASSIGN(DummyKeyboardBrightnessControlDelegate);
};

bool TestTarget::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (accelerator.IsRepeat())
    ++accelerator_repeat_count_;
  else
    ++accelerator_pressed_count_;
  return true;
}

bool TestTarget::CanHandleAccelerators() const {
  return true;
}

void ProcessAccelerator(ui::KeyboardCode key_code, int flags) {
  const ui::Accelerator accelerator(key_code, flags);
  if (!WmShell::Get()->accelerator_controller()->IsRegistered(accelerator))
    return;

  mus::AcceleratorControllerRegistrarTestApi().ProcessAccelerator(accelerator);
}

}  // namespace

class AcceleratorControllerTest : public mus::WmTestBase {
 public:
  AcceleratorControllerTest() {}
  ~AcceleratorControllerTest() override {}

 protected:
  static AcceleratorController* GetController();

  static bool ProcessInController(const ui::Accelerator& accelerator) {
    if (accelerator.type() == ui::ET_KEY_RELEASED) {
      // If the |accelerator| should trigger on release, then we store the
      // pressed version of it first in history then the released one to
      // simulate what happens in reality.
      ui::Accelerator pressed_accelerator = accelerator;
      pressed_accelerator.set_type(ui::ET_KEY_PRESSED);
      GetController()->accelerator_history()->StoreCurrentAccelerator(
          pressed_accelerator);
    }
    GetController()->accelerator_history()->StoreCurrentAccelerator(
        accelerator);
    return GetController()->Process(accelerator);
  }

  static const ui::Accelerator& GetPreviousAccelerator() {
    return GetController()->accelerator_history()->previous_accelerator();
  }

  static const ui::Accelerator& GetCurrentAccelerator() {
    return GetController()->accelerator_history()->current_accelerator();
  }

  // Several functions to access ExitWarningHandler (as friend).
  static void StubForTest(ExitWarningHandler* ewh) {
    ewh->stub_timer_for_test_ = true;
  }
  static void Reset(ExitWarningHandler* ewh) {
    ewh->state_ = ExitWarningHandler::IDLE;
  }
  static void SimulateTimerExpired(ExitWarningHandler* ewh) {
    ewh->TimerAction();
  }
  static bool is_ui_shown(ExitWarningHandler* ewh) { return !!ewh->widget_; }
  static bool is_idle(ExitWarningHandler* ewh) {
    return ewh->state_ == ExitWarningHandler::IDLE;
  }
  static bool is_exiting(ExitWarningHandler* ewh) {
    return ewh->state_ == ExitWarningHandler::EXITING;
  }
  // TODO: needs support for TestShelfDelegate: http://crbug.com/632208.
  /*
  ui::Window* CreatePanel() {
    ui::Window* window = CreateTestWindow(
        gfx::Rect(5, 5, 20, 20),
        ui::wm::WINDOW_TYPE_PANEL);
    test::TestShelfDelegate* shelf_delegate =
        test::TestShelfDelegate::instance();
    shelf_delegate->AddShelfItem(window);
    PanelLayoutManager* manager =
        PanelLayoutManager::Get(WmWindow::Get(window));
    manager->Relayout();
    return window;
  }
  */

  void SetBrightnessControlDelegate(
      std::unique_ptr<BrightnessControlDelegate> delegate) {
    WmShell::Get()->brightness_control_delegate_ = std::move(delegate);
  }

  void SetKeyboardBrightnessControlDelegate(
      std::unique_ptr<KeyboardBrightnessControlDelegate> delegate) {
    WmShell::Get()->keyboard_brightness_control_delegate_ = std::move(delegate);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AcceleratorControllerTest);
};

AcceleratorController* AcceleratorControllerTest::GetController() {
  return WmShell::Get()->accelerator_controller();
}

#if !defined(OS_WIN)
// Double press of exit shortcut => exiting
TEST_F(AcceleratorControllerTest, ExitWarningHandlerTestDoublePress) {
  ui::Accelerator press(ui::VKEY_Q, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
  ui::Accelerator release(press);
  release.set_type(ui::ET_KEY_RELEASED);
  ExitWarningHandler* ewh = GetController()->GetExitWarningHandlerForTest();
  ASSERT_TRUE(ewh);
  StubForTest(ewh);
  EXPECT_TRUE(is_idle(ewh));
  EXPECT_FALSE(is_ui_shown(ewh));
  EXPECT_TRUE(ProcessInController(press));
  EXPECT_FALSE(ProcessInController(release));
  EXPECT_FALSE(is_idle(ewh));
  EXPECT_TRUE(is_ui_shown(ewh));
  EXPECT_TRUE(ProcessInController(press));  // second press before timer.
  EXPECT_FALSE(ProcessInController(release));
  SimulateTimerExpired(ewh);
  EXPECT_TRUE(is_exiting(ewh));
  EXPECT_FALSE(is_ui_shown(ewh));
  Reset(ewh);
}

// Single press of exit shortcut before timer => idle
TEST_F(AcceleratorControllerTest, ExitWarningHandlerTestSinglePress) {
  ui::Accelerator press(ui::VKEY_Q, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
  ui::Accelerator release(press);
  release.set_type(ui::ET_KEY_RELEASED);
  ExitWarningHandler* ewh = GetController()->GetExitWarningHandlerForTest();
  ASSERT_TRUE(ewh);
  StubForTest(ewh);
  EXPECT_TRUE(is_idle(ewh));
  EXPECT_FALSE(is_ui_shown(ewh));
  EXPECT_TRUE(ProcessInController(press));
  EXPECT_FALSE(ProcessInController(release));
  EXPECT_FALSE(is_idle(ewh));
  EXPECT_TRUE(is_ui_shown(ewh));
  SimulateTimerExpired(ewh);
  EXPECT_TRUE(is_idle(ewh));
  EXPECT_FALSE(is_ui_shown(ewh));
  Reset(ewh);
}

// Shutdown ash with exit warning bubble open should not crash.
TEST_F(AcceleratorControllerTest, LingeringExitWarningBubble) {
  ExitWarningHandler* ewh = GetController()->GetExitWarningHandlerForTest();
  ASSERT_TRUE(ewh);
  StubForTest(ewh);

  // Trigger once to show the bubble.
  ewh->HandleAccelerator();
  EXPECT_FALSE(is_idle(ewh));
  EXPECT_TRUE(is_ui_shown(ewh));

  // Exit ash and there should be no crash
}
#endif  // !defined(OS_WIN)

TEST_F(AcceleratorControllerTest, Register) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_NONE);
  TestTarget target;
  GetController()->Register(accelerator_a, &target);

  // The registered accelerator is processed.
  EXPECT_TRUE(ProcessInController(accelerator_a));
  EXPECT_EQ(1, target.accelerator_pressed_count());
}

TEST_F(AcceleratorControllerTest, RegisterMultipleTarget) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_NONE);
  TestTarget target1;
  GetController()->Register(accelerator_a, &target1);
  TestTarget target2;
  GetController()->Register(accelerator_a, &target2);

  // If multiple targets are registered with the same accelerator, the target
  // registered later processes the accelerator.
  EXPECT_TRUE(ProcessInController(accelerator_a));
  EXPECT_EQ(0, target1.accelerator_pressed_count());
  EXPECT_EQ(1, target2.accelerator_pressed_count());
}

TEST_F(AcceleratorControllerTest, Unregister) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_NONE);
  TestTarget target;
  GetController()->Register(accelerator_a, &target);
  const ui::Accelerator accelerator_b(ui::VKEY_B, ui::EF_NONE);
  GetController()->Register(accelerator_b, &target);

  // Unregistering a different accelerator does not affect the other
  // accelerator.
  GetController()->Unregister(accelerator_b, &target);
  EXPECT_TRUE(ProcessInController(accelerator_a));
  EXPECT_EQ(1, target.accelerator_pressed_count());

  // The unregistered accelerator is no longer processed.
  target.reset();
  GetController()->Unregister(accelerator_a, &target);
  EXPECT_FALSE(ProcessInController(accelerator_a));
  EXPECT_EQ(0, target.accelerator_pressed_count());
}

TEST_F(AcceleratorControllerTest, UnregisterAll) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_NONE);
  TestTarget target1;
  GetController()->Register(accelerator_a, &target1);
  const ui::Accelerator accelerator_b(ui::VKEY_B, ui::EF_NONE);
  GetController()->Register(accelerator_b, &target1);
  const ui::Accelerator accelerator_c(ui::VKEY_C, ui::EF_NONE);
  TestTarget target2;
  GetController()->Register(accelerator_c, &target2);
  GetController()->UnregisterAll(&target1);

  // All the accelerators registered for |target1| are no longer processed.
  EXPECT_FALSE(ProcessInController(accelerator_a));
  EXPECT_FALSE(ProcessInController(accelerator_b));
  EXPECT_EQ(0, target1.accelerator_pressed_count());

  // UnregisterAll with a different target does not affect the other target.
  EXPECT_TRUE(ProcessInController(accelerator_c));
  EXPECT_EQ(1, target2.accelerator_pressed_count());
}

TEST_F(AcceleratorControllerTest, Process) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_NONE);
  TestTarget target1;
  GetController()->Register(accelerator_a, &target1);

  // The registered accelerator is processed.
  EXPECT_TRUE(ProcessInController(accelerator_a));
  EXPECT_EQ(1, target1.accelerator_pressed_count());

  // The non-registered accelerator is not processed.
  const ui::Accelerator accelerator_b(ui::VKEY_B, ui::EF_NONE);
  EXPECT_FALSE(ProcessInController(accelerator_b));
}

TEST_F(AcceleratorControllerTest, IsRegistered) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_NONE);
  const ui::Accelerator accelerator_shift_a(ui::VKEY_A, ui::EF_SHIFT_DOWN);
  TestTarget target;
  GetController()->Register(accelerator_a, &target);
  EXPECT_TRUE(GetController()->IsRegistered(accelerator_a));
  EXPECT_FALSE(GetController()->IsRegistered(accelerator_shift_a));
  GetController()->UnregisterAll(&target);
  EXPECT_FALSE(GetController()->IsRegistered(accelerator_a));
}

TEST_F(AcceleratorControllerTest, WindowSnap) {
  aura::Window* aura_window = CreateTestWindow(gfx::Rect(5, 5, 20, 20));
  WmWindow* window = WmWindow::Get(aura_window);
  wm::WindowState* window_state = window->GetWindowState();

  window_state->Activate();

  {
    GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_LEFT);
    gfx::Rect expected_bounds =
        wm::GetDefaultLeftSnappedWindowBoundsInParent(window);
    EXPECT_EQ(expected_bounds.ToString(), window->GetBounds().ToString());
  }
  {
    GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_RIGHT);
    gfx::Rect expected_bounds =
        wm::GetDefaultRightSnappedWindowBoundsInParent(window);
    EXPECT_EQ(expected_bounds.ToString(), window->GetBounds().ToString());
  }
  {
    gfx::Rect normal_bounds = window_state->GetRestoreBoundsInParent();

    GetController()->PerformActionIfEnabled(TOGGLE_MAXIMIZED);
    EXPECT_TRUE(window_state->IsMaximized());
    EXPECT_NE(normal_bounds.ToString(), window->GetBounds().ToString());

    GetController()->PerformActionIfEnabled(TOGGLE_MAXIMIZED);
    EXPECT_FALSE(window_state->IsMaximized());
    // Window gets restored to its restore bounds since side-maximized state
    // is treated as a "maximized" state.
    EXPECT_EQ(normal_bounds.ToString(), window->GetBounds().ToString());

    GetController()->PerformActionIfEnabled(TOGGLE_MAXIMIZED);
    GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_LEFT);
    EXPECT_FALSE(window_state->IsMaximized());

    GetController()->PerformActionIfEnabled(TOGGLE_MAXIMIZED);
    GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_RIGHT);
    EXPECT_FALSE(window_state->IsMaximized());

    GetController()->PerformActionIfEnabled(TOGGLE_MAXIMIZED);
    EXPECT_TRUE(window_state->IsMaximized());
    GetController()->PerformActionIfEnabled(WINDOW_MINIMIZE);
    EXPECT_FALSE(window_state->IsMaximized());
    EXPECT_TRUE(window_state->IsMinimized());
    window_state->Restore();
    window_state->Activate();
  }
  {
    GetController()->PerformActionIfEnabled(WINDOW_MINIMIZE);
    EXPECT_TRUE(window_state->IsMinimized());
  }
}

// Tests that when window docking is disabled, only snapping windows works.
TEST_F(AcceleratorControllerTest, WindowSnapWithoutDocking) {
  ASSERT_FALSE(ash::switches::DockedWindowsEnabled());
  WmWindow* window = WmWindow::Get(CreateTestWindow(gfx::Rect(5, 5, 20, 20)));
  wm::WindowState* window_state = window->GetWindowState();
  window_state->Activate();

  // Snap right.
  GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_RIGHT);
  gfx::Rect normal_bounds = window_state->GetRestoreBoundsInParent();
  gfx::Rect expected_bounds =
      wm::GetDefaultRightSnappedWindowBoundsInParent(window);
  EXPECT_EQ(expected_bounds.ToString(), window->GetBounds().ToString());
  EXPECT_TRUE(window_state->IsSnapped());
  // Snap right again ->> becomes normal.
  GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_RIGHT);
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_FALSE(window_state->IsDocked());
  EXPECT_EQ(normal_bounds.ToString(), window->GetBounds().ToString());
  // Snap right.
  GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_RIGHT);
  EXPECT_TRUE(window_state->IsSnapped());
  EXPECT_FALSE(window_state->IsDocked());
  // Snap left.
  GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_LEFT);
  EXPECT_TRUE(window_state->IsSnapped());
  EXPECT_FALSE(window_state->IsDocked());
  expected_bounds = wm::GetDefaultLeftSnappedWindowBoundsInParent(window);
  EXPECT_EQ(expected_bounds.ToString(), window->GetBounds().ToString());
  // Snap left again ->> becomes normal.
  GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_LEFT);
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_FALSE(window_state->IsDocked());
  EXPECT_EQ(normal_bounds.ToString(), window->GetBounds().ToString());
}

// Test class used for testing docked windows.
class EnabledDockedWindowsAcceleratorControllerTest
    : public AcceleratorControllerTest {
 public:
  EnabledDockedWindowsAcceleratorControllerTest() = default;
  ~EnabledDockedWindowsAcceleratorControllerTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kAshEnableDockedWindows);
    AcceleratorControllerTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EnabledDockedWindowsAcceleratorControllerTest);
};

TEST_F(EnabledDockedWindowsAcceleratorControllerTest,
       WindowSnapLeftDockLeftRestore) {
  CreateTestWindow(gfx::Rect(5, 5, 20, 20));
  WmWindow* window1 = WmWindow::Get(CreateTestWindow(gfx::Rect(5, 5, 20, 20)));
  wm::WindowState* window1_state = window1->GetWindowState();
  window1_state->Activate();

  GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_LEFT);
  gfx::Rect normal_bounds = window1_state->GetRestoreBoundsInParent();
  gfx::Rect expected_bounds =
      wm::GetDefaultLeftSnappedWindowBoundsInParent(window1);
  EXPECT_EQ(expected_bounds.ToString(), window1->GetBounds().ToString());
  EXPECT_TRUE(window1_state->IsSnapped());
  GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_LEFT);
  EXPECT_FALSE(window1_state->IsNormalOrSnapped());
  EXPECT_TRUE(window1_state->IsDocked());
  GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_LEFT);
  EXPECT_FALSE(window1_state->IsDocked());
  EXPECT_EQ(normal_bounds.ToString(), window1->GetBounds().ToString());
}

TEST_F(EnabledDockedWindowsAcceleratorControllerTest,
       WindowSnapRightDockRightRestore) {
  CreateTestWindow(gfx::Rect(5, 5, 20, 20));
  WmWindow* window1 = WmWindow::Get(CreateTestWindow(gfx::Rect(5, 5, 20, 20)));

  wm::WindowState* window1_state = window1->GetWindowState();
  window1_state->Activate();

  GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_RIGHT);
  gfx::Rect normal_bounds = window1_state->GetRestoreBoundsInParent();
  gfx::Rect expected_bounds =
      wm::GetDefaultRightSnappedWindowBoundsInParent(window1);
  EXPECT_EQ(expected_bounds.ToString(), window1->GetBounds().ToString());
  EXPECT_TRUE(window1_state->IsSnapped());
  GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_RIGHT);
  EXPECT_FALSE(window1_state->IsNormalOrSnapped());
  EXPECT_TRUE(window1_state->IsDocked());
  GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_RIGHT);
  EXPECT_FALSE(window1_state->IsDocked());
  EXPECT_EQ(normal_bounds.ToString(), window1->GetBounds().ToString());
}

TEST_F(EnabledDockedWindowsAcceleratorControllerTest,
       WindowSnapLeftDockLeftSnapRight) {
  CreateTestWindow(gfx::Rect(5, 5, 20, 20));
  WmWindow* window1 = WmWindow::Get(CreateTestWindow(gfx::Rect(5, 5, 20, 20)));

  wm::WindowState* window1_state = window1->GetWindowState();
  window1_state->Activate();

  GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_LEFT);
  gfx::Rect expected_bounds =
      wm::GetDefaultLeftSnappedWindowBoundsInParent(window1);
  gfx::Rect expected_bounds2 =
      wm::GetDefaultRightSnappedWindowBoundsInParent(window1);
  EXPECT_EQ(expected_bounds.ToString(), window1->GetBounds().ToString());
  EXPECT_TRUE(window1_state->IsSnapped());
  GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_LEFT);
  EXPECT_FALSE(window1_state->IsNormalOrSnapped());
  EXPECT_TRUE(window1_state->IsDocked());
  GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_RIGHT);
  EXPECT_FALSE(window1_state->IsDocked());
  EXPECT_TRUE(window1_state->IsSnapped());
  EXPECT_EQ(expected_bounds2.ToString(), window1->GetBounds().ToString());
}

TEST_F(EnabledDockedWindowsAcceleratorControllerTest,
       WindowDockLeftMinimizeWindowWithRestore) {
  WindowOwner window_owner(
      WmWindow::Get(CreateTestWindow(gfx::Rect(5, 5, 20, 20))));
  WindowOwner window1_owner(
      WmWindow::Get(CreateTestWindow(gfx::Rect(5, 5, 20, 20))));
  WmWindow* window1 = window1_owner.window();

  wm::WindowState* window1_state = window1->GetWindowState();
  window1_state->Activate();

  WindowOwner window2_owner(
      WmWindow::Get(CreateTestWindow(gfx::Rect(5, 5, 20, 20))));
  WmWindow* window2 = window2_owner.window();

  wm::WindowState* window2_state = window2->GetWindowState();

  WindowOwner window3_owner(
      WmWindow::Get(CreateTestWindow(gfx::Rect(5, 5, 20, 20))));
  WmWindow* window3 = window3_owner.window();

  wm::WindowState* window3_state = window3->GetWindowState();
  window3_state->Activate();

  GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_LEFT);
  GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_LEFT);
  gfx::Rect window3_docked_bounds = window3->GetBounds();

  window2_state->Activate();
  GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_LEFT);
  GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_LEFT);
  window1_state->Activate();
  GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_LEFT);
  GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_LEFT);

  EXPECT_TRUE(window3_state->IsDocked());
  EXPECT_TRUE(window2_state->IsDocked());
  EXPECT_TRUE(window1_state->IsDocked());
  EXPECT_TRUE(window3_state->IsMinimized());

  window1_state->Activate();
  GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_LEFT);
  window2_state->Activate();
  GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_LEFT);
  window3_state->Unminimize();
  EXPECT_FALSE(window1_state->IsDocked());
  EXPECT_FALSE(window2_state->IsDocked());
  EXPECT_TRUE(window3_state->IsDocked());
  EXPECT_EQ(window3_docked_bounds.ToString(), window3->GetBounds().ToString());
}

// TODO: Needs CreatePanel(): http://crbug.com/632209.
/*
TEST_F(EnabledDockedWindowsAcceleratorControllerTest,
       WindowPanelDockLeftDockRightRestore) {
  WmWndow* window0 =
      WmWindow::Get(CreateTestWindow(gfx::Rect(5, 5, 20, 20)));

  std::unique_ptr<aura::Window> window(CreatePanel());
  wm::WindowState* window_state = wm::GetWindowState(window.get());
  window_state->Activate();

  gfx::Rect window_restore_bounds2 = window->bounds();
  GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_LEFT);
  gfx::Rect expected_bounds = wm::GetDefaultLeftSnappedWindowBoundsInParent(
      WmWindow::Get(window.get()));
  gfx::Rect window_restore_bounds = window_state->GetRestoreBoundsInScreen();
  EXPECT_NE(expected_bounds.ToString(), window->bounds().ToString());
  EXPECT_FALSE(window_state->IsSnapped());
  EXPECT_FALSE(window_state->IsNormalOrSnapped());
  EXPECT_TRUE(window_state->IsDocked());
  window_state->Restore();
  GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_RIGHT);
  EXPECT_TRUE(window_state->IsDocked());
  GetController()->PerformActionIfEnabled(WINDOW_CYCLE_SNAP_DOCK_RIGHT);
  EXPECT_FALSE(window_state->IsDocked());
  EXPECT_EQ(window_restore_bounds.ToString(),
            window_restore_bounds2.ToString());
  EXPECT_EQ(window_restore_bounds.ToString(), window->bounds().ToString());
}
*/

TEST_F(EnabledDockedWindowsAcceleratorControllerTest, CenterWindowAccelerator) {
  WindowOwner window_owner(
      WmWindow::Get(CreateTestWindow(gfx::Rect(5, 5, 20, 20))));
  WmWindow* window = window_owner.window();
  wm::WindowState* window_state = window->GetWindowState();
  window_state->Activate();

  // Center the window using accelerator.
  GetController()->PerformActionIfEnabled(WINDOW_POSITION_CENTER);
  gfx::Rect work_area = window->GetDisplayNearestWindow().work_area();
  gfx::Rect bounds = window->GetBoundsInScreen();
  EXPECT_NEAR(bounds.x() - work_area.x(), work_area.right() - bounds.right(),
              1);
  EXPECT_NEAR(bounds.y() - work_area.y(), work_area.bottom() - bounds.bottom(),
              1);

  // Add the window to docked container and try to center it.
  window->SetBounds(gfx::Rect(0, 0, 20, 20));
  const wm::WMEvent event(wm::WM_EVENT_DOCK);
  window->GetWindowState()->OnWMEvent(&event);
  EXPECT_EQ(kShellWindowId_DockedContainer,
            window->GetParent()->GetShellWindowId());

  gfx::Rect docked_bounds = window->GetBoundsInScreen();
  GetController()->PerformActionIfEnabled(WINDOW_POSITION_CENTER);
  // It should not get centered and should remain docked.
  EXPECT_EQ(kShellWindowId_DockedContainer,
            window->GetParent()->GetShellWindowId());
  EXPECT_EQ(docked_bounds.ToString(), window->GetBoundsInScreen().ToString());
}

// This is commented out for two reasons:
// . http://crbug.com/630683: ash doesn't see all key events.
// . http://crbug.com/615033: EventGenerator doesn't work with ash. That can be
//   worked around for the test, but no point until we decide on the first one.
/*
TEST_F(AcceleratorControllerTest, AutoRepeat) {
  ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  accelerator_a.set_type(ui::ET_KEY_PRESSED);
  TestTarget target_a;
  GetController()->Register(accelerator_a, &target_a);
  ui::Accelerator accelerator_b(ui::VKEY_B, ui::EF_CONTROL_DOWN);
  accelerator_b.set_type(ui::ET_KEY_PRESSED);
  TestTarget target_b;
  GetController()->Register(accelerator_b, &target_b);

  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.PressKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  generator.ReleaseKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(1, target_a.accelerator_pressed_count());
  EXPECT_EQ(0, target_a.accelerator_repeat_count());

  // Long press should generate one
  generator.PressKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  generator.PressKey(ui::VKEY_A, ui::EF_CONTROL_DOWN | ui::EF_IS_REPEAT);
  EXPECT_EQ(2, target_a.accelerator_pressed_count());
  EXPECT_EQ(1, target_a.accelerator_repeat_count());
  generator.PressKey(ui::VKEY_A, ui::EF_CONTROL_DOWN | ui::EF_IS_REPEAT);
  EXPECT_EQ(2, target_a.accelerator_pressed_count());
  EXPECT_EQ(2, target_a.accelerator_repeat_count());
  generator.ReleaseKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(2, target_a.accelerator_pressed_count());
  EXPECT_EQ(2, target_a.accelerator_repeat_count());

  // Long press was intercepted by another key press.
  generator.PressKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  generator.PressKey(ui::VKEY_A, ui::EF_CONTROL_DOWN | ui::EF_IS_REPEAT);
  generator.PressKey(ui::VKEY_B, ui::EF_CONTROL_DOWN);
  generator.ReleaseKey(ui::VKEY_B, ui::EF_CONTROL_DOWN);
  generator.PressKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  generator.PressKey(ui::VKEY_A, ui::EF_CONTROL_DOWN | ui::EF_IS_REPEAT);
  generator.ReleaseKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(1, target_b.accelerator_pressed_count());
  EXPECT_EQ(0, target_b.accelerator_repeat_count());
  EXPECT_EQ(4, target_a.accelerator_pressed_count());
  EXPECT_EQ(4, target_a.accelerator_repeat_count());
}
*/

// TODO: update once http://crbug.com/630683 is resolved.
/*
TEST_F(AcceleratorControllerTest, Previous) {
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.PressKey(ui::VKEY_VOLUME_MUTE, ui::EF_NONE);
  generator.ReleaseKey(ui::VKEY_VOLUME_MUTE, ui::EF_NONE);

  EXPECT_EQ(ui::VKEY_VOLUME_MUTE, GetPreviousAccelerator().key_code());
  EXPECT_EQ(ui::EF_NONE, GetPreviousAccelerator().modifiers());

  generator.PressKey(ui::VKEY_TAB, ui::EF_CONTROL_DOWN);
  generator.ReleaseKey(ui::VKEY_TAB, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(ui::VKEY_TAB, GetPreviousAccelerator().key_code());
  EXPECT_EQ(ui::EF_CONTROL_DOWN, GetPreviousAccelerator().modifiers());
}
*/

TEST_F(AcceleratorControllerTest, DontRepeatToggleFullscreen) {
  const AcceleratorData accelerators[] = {
      {true, ui::VKEY_J, ui::EF_ALT_DOWN, TOGGLE_FULLSCREEN},
      {true, ui::VKEY_K, ui::EF_ALT_DOWN, TOGGLE_FULLSCREEN},
  };
  GetController()->RegisterAccelerators(accelerators, arraysize(accelerators));

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(5, 5, 20, 20);
  views::Widget* widget = new views::Widget;
  WmWindow::Get(GetPrimaryRootWindow())
      ->GetRootWindowController()
      ->ConfigureWidgetInitParamsForContainer(
          widget, kShellWindowId_DefaultContainer, &params);
  widget->Init(params);
  widget->GetNativeView()->SetProperty(aura::client::kResizeBehaviorKey,
                                       ui::mojom::kResizeBehaviorCanMaximize);
  widget->Show();
  widget->Activate();

  wm::WindowState* window_state =
      WmLookup::Get()->GetWindowForWidget(widget)->GetWindowState();

  // Toggling not suppressed.
  ProcessAccelerator(ui::VKEY_J, ui::EF_ALT_DOWN);
  EXPECT_TRUE(window_state->IsFullscreen());

  // The same accelerator - toggling suppressed.
  ProcessAccelerator(ui::VKEY_J, ui::EF_ALT_DOWN | ui::EF_IS_REPEAT);
  EXPECT_TRUE(window_state->IsFullscreen());

  // Different accelerator.
  ProcessAccelerator(ui::VKEY_K, ui::EF_ALT_DOWN);
  EXPECT_FALSE(window_state->IsFullscreen());
}

TEST_F(AcceleratorControllerTest, GlobalAccelerators) {
  // CycleBackward
  EXPECT_TRUE(ProcessInController(
      ui::Accelerator(ui::VKEY_TAB, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN)));
  // CycleForward
  EXPECT_TRUE(
      ProcessInController(ui::Accelerator(ui::VKEY_TAB, ui::EF_ALT_DOWN)));
  // CycleLinear
  EXPECT_TRUE(ProcessInController(
      ui::Accelerator(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_NONE)));

#if defined(OS_CHROMEOS)
  // The "Take Screenshot", "Take Partial Screenshot", volume, brightness, and
  // keyboard brightness accelerators are only defined on ChromeOS.
  // TODO: needs ScreenShotDelegate converted: http://crbug.com/612331.
  /*
  {
    test::TestScreenshotDelegate* delegate = GetScreenshotDelegate();
    delegate->set_can_take_screenshot(false);
    EXPECT_TRUE(ProcessInController(
        ui::Accelerator(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN)));
    EXPECT_TRUE(
        ProcessInController(ui::Accelerator(ui::VKEY_PRINT, ui::EF_NONE)));
    EXPECT_TRUE(ProcessInController(ui::Accelerator(
        ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)));

    delegate->set_can_take_screenshot(true);
    EXPECT_EQ(0, delegate->handle_take_screenshot_count());
    EXPECT_TRUE(ProcessInController(
        ui::Accelerator(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(1, delegate->handle_take_screenshot_count());
    EXPECT_TRUE(
        ProcessInController(ui::Accelerator(ui::VKEY_PRINT, ui::EF_NONE)));
    EXPECT_EQ(2, delegate->handle_take_screenshot_count());
    EXPECT_TRUE(ProcessInController(ui::Accelerator(
        ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(2, delegate->handle_take_screenshot_count());
  }
  */
  const ui::Accelerator volume_mute(ui::VKEY_VOLUME_MUTE, ui::EF_NONE);
  const ui::Accelerator volume_down(ui::VKEY_VOLUME_DOWN, ui::EF_NONE);
  const ui::Accelerator volume_up(ui::VKEY_VOLUME_UP, ui::EF_NONE);
  {
    base::UserActionTester user_action_tester;
    ui::AcceleratorHistory* history = GetController()->accelerator_history();

    EXPECT_EQ(0, user_action_tester.GetActionCount("Accel_VolumeMute_F8"));
    EXPECT_TRUE(ProcessInController(volume_mute));
    EXPECT_EQ(1, user_action_tester.GetActionCount("Accel_VolumeMute_F8"));
    EXPECT_EQ(volume_mute, history->current_accelerator());

    EXPECT_EQ(0, user_action_tester.GetActionCount("Accel_VolumeDown_F9"));
    EXPECT_TRUE(ProcessInController(volume_down));
    EXPECT_EQ(1, user_action_tester.GetActionCount("Accel_VolumeDown_F9"));
    EXPECT_EQ(volume_down, history->current_accelerator());

    EXPECT_EQ(0, user_action_tester.GetActionCount("Accel_VolumeUp_F10"));
    EXPECT_TRUE(ProcessInController(volume_up));
    EXPECT_EQ(volume_up, history->current_accelerator());
    EXPECT_EQ(1, user_action_tester.GetActionCount("Accel_VolumeUp_F10"));
  }
  // Brightness
  // ui::VKEY_BRIGHTNESS_DOWN/UP are not defined on Windows.
  const ui::Accelerator brightness_down(ui::VKEY_BRIGHTNESS_DOWN, ui::EF_NONE);
  const ui::Accelerator brightness_up(ui::VKEY_BRIGHTNESS_UP, ui::EF_NONE);
  {
    DummyBrightnessControlDelegate* delegate =
        new DummyBrightnessControlDelegate;
    SetBrightnessControlDelegate(
        std::unique_ptr<BrightnessControlDelegate>(delegate));
    EXPECT_EQ(0, delegate->handle_brightness_down_count());
    EXPECT_TRUE(ProcessInController(brightness_down));
    EXPECT_EQ(1, delegate->handle_brightness_down_count());
    EXPECT_EQ(brightness_down, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_brightness_up_count());
    EXPECT_TRUE(ProcessInController(brightness_up));
    EXPECT_EQ(1, delegate->handle_brightness_up_count());
    EXPECT_EQ(brightness_up, delegate->last_accelerator());
  }

  // Keyboard brightness
  const ui::Accelerator alt_brightness_down(ui::VKEY_BRIGHTNESS_DOWN,
                                            ui::EF_ALT_DOWN);
  const ui::Accelerator alt_brightness_up(ui::VKEY_BRIGHTNESS_UP,
                                          ui::EF_ALT_DOWN);
  {
    EXPECT_TRUE(ProcessInController(alt_brightness_down));
    EXPECT_TRUE(ProcessInController(alt_brightness_up));
    DummyKeyboardBrightnessControlDelegate* delegate =
        new DummyKeyboardBrightnessControlDelegate;
    SetKeyboardBrightnessControlDelegate(
        std::unique_ptr<KeyboardBrightnessControlDelegate>(delegate));
    EXPECT_EQ(0, delegate->handle_keyboard_brightness_down_count());
    EXPECT_TRUE(ProcessInController(alt_brightness_down));
    EXPECT_EQ(1, delegate->handle_keyboard_brightness_down_count());
    EXPECT_EQ(alt_brightness_down, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_keyboard_brightness_up_count());
    EXPECT_TRUE(ProcessInController(alt_brightness_up));
    EXPECT_EQ(1, delegate->handle_keyboard_brightness_up_count());
    EXPECT_EQ(alt_brightness_up, delegate->last_accelerator());
  }
#endif

#if !defined(OS_WIN)
  // Exit
  ExitWarningHandler* ewh = GetController()->GetExitWarningHandlerForTest();
  ASSERT_TRUE(ewh);
  StubForTest(ewh);
  EXPECT_TRUE(is_idle(ewh));
  EXPECT_FALSE(is_ui_shown(ewh));
  EXPECT_TRUE(ProcessInController(
      ui::Accelerator(ui::VKEY_Q, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)));
  EXPECT_FALSE(is_idle(ewh));
  EXPECT_TRUE(is_ui_shown(ewh));
  SimulateTimerExpired(ewh);
  EXPECT_TRUE(is_idle(ewh));
  EXPECT_FALSE(is_ui_shown(ewh));
  Reset(ewh);
#endif

  // New tab
  EXPECT_TRUE(
      ProcessInController(ui::Accelerator(ui::VKEY_T, ui::EF_CONTROL_DOWN)));

  // New incognito window
  // TODO(sky): this should be true but fails because WmTestBase is not using
  // TestShellDelegate: http://crbug.com/632211.
  EXPECT_FALSE(ProcessInController(
      ui::Accelerator(ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)));

  // New window
  EXPECT_TRUE(
      ProcessInController(ui::Accelerator(ui::VKEY_N, ui::EF_CONTROL_DOWN)));

  // Restore tab
  EXPECT_TRUE(ProcessInController(
      ui::Accelerator(ui::VKEY_T, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)));

  // Show task manager
  EXPECT_TRUE(
      ProcessInController(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN)));

#if defined(OS_CHROMEOS)
  // Open file manager
  EXPECT_TRUE(ProcessInController(
      ui::Accelerator(ui::VKEY_M, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN)));

  // Lock screen
  // NOTE: Accelerators that do not work on the lock screen need to be
  // tested before the sequence below is invoked because it causes a side
  // effect of locking the screen.
  EXPECT_TRUE(
      ProcessInController(ui::Accelerator(ui::VKEY_L, ui::EF_COMMAND_DOWN)));
#endif
}

// TODO: fails in mash, http://crbug.com/632180.
TEST_F(AcceleratorControllerTest, DISABLED_GlobalAcceleratorsToggleAppList) {
  AccessibilityDelegate* delegate = WmShell::Get()->accessibility_delegate();
  EXPECT_FALSE(WmShell::Get()->GetAppListTargetVisibility());

  // The press event should not open the AppList, the release should instead.
  EXPECT_FALSE(
      ProcessInController(ui::Accelerator(ui::VKEY_LWIN, ui::EF_NONE)));
  EXPECT_EQ(ui::VKEY_LWIN, GetCurrentAccelerator().key_code());

  EXPECT_FALSE(WmShell::Get()->GetAppListTargetVisibility());

  EXPECT_TRUE(
      ProcessInController(ReleaseAccelerator(ui::VKEY_LWIN, ui::EF_NONE)));
  EXPECT_TRUE(WmShell::Get()->GetAppListTargetVisibility());

  EXPECT_EQ(ui::VKEY_LWIN, GetPreviousAccelerator().key_code());

  // When spoken feedback is on, the AppList should not toggle.
  delegate->ToggleSpokenFeedback(A11Y_NOTIFICATION_NONE);
  EXPECT_FALSE(
      ProcessInController(ui::Accelerator(ui::VKEY_LWIN, ui::EF_NONE)));
  EXPECT_FALSE(
      ProcessInController(ReleaseAccelerator(ui::VKEY_LWIN, ui::EF_NONE)));
  delegate->ToggleSpokenFeedback(A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(WmShell::Get()->GetAppListTargetVisibility());

  EXPECT_FALSE(
      ProcessInController(ui::Accelerator(ui::VKEY_LWIN, ui::EF_NONE)));
  EXPECT_TRUE(
      ProcessInController(ReleaseAccelerator(ui::VKEY_LWIN, ui::EF_NONE)));
  EXPECT_FALSE(WmShell::Get()->GetAppListTargetVisibility());

  // When spoken feedback is on, the AppList should not toggle.
  delegate->ToggleSpokenFeedback(A11Y_NOTIFICATION_NONE);
  EXPECT_FALSE(
      ProcessInController(ui::Accelerator(ui::VKEY_LWIN, ui::EF_NONE)));
  EXPECT_FALSE(
      ProcessInController(ReleaseAccelerator(ui::VKEY_LWIN, ui::EF_NONE)));
  delegate->ToggleSpokenFeedback(A11Y_NOTIFICATION_NONE);
  EXPECT_FALSE(WmShell::Get()->GetAppListTargetVisibility());

#if defined(OS_CHROMEOS)
  // The press of VKEY_BROWSER_SEARCH should toggle the AppList
  EXPECT_TRUE(ProcessInController(
      ui::Accelerator(ui::VKEY_BROWSER_SEARCH, ui::EF_NONE)));
  EXPECT_TRUE(WmShell::Get()->GetAppListTargetVisibility());
  EXPECT_FALSE(ProcessInController(
      ReleaseAccelerator(ui::VKEY_BROWSER_SEARCH, ui::EF_NONE)));
  EXPECT_TRUE(WmShell::Get()->GetAppListTargetVisibility());
#endif
}

TEST_F(AcceleratorControllerTest, ImeGlobalAccelerators) {
  // Test IME shortcuts.
  ui::Accelerator control_space_down(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN);
  control_space_down.set_type(ui::ET_KEY_PRESSED);
  ui::Accelerator control_space_up(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN);
  control_space_up.set_type(ui::ET_KEY_RELEASED);
  const ui::Accelerator convert(ui::VKEY_CONVERT, ui::EF_NONE);
  const ui::Accelerator non_convert(ui::VKEY_NONCONVERT, ui::EF_NONE);
  const ui::Accelerator wide_half_1(ui::VKEY_DBE_SBCSCHAR, ui::EF_NONE);
  const ui::Accelerator wide_half_2(ui::VKEY_DBE_DBCSCHAR, ui::EF_NONE);
  const ui::Accelerator hangul(ui::VKEY_HANGUL, ui::EF_NONE);
  EXPECT_FALSE(ProcessInController(control_space_down));
  EXPECT_FALSE(ProcessInController(control_space_up));
  EXPECT_FALSE(ProcessInController(convert));
  EXPECT_FALSE(ProcessInController(non_convert));
  EXPECT_FALSE(ProcessInController(wide_half_1));
  EXPECT_FALSE(ProcessInController(wide_half_2));
  EXPECT_FALSE(ProcessInController(hangul));
  DummyImeControlDelegate* delegate = new DummyImeControlDelegate;
  GetController()->SetImeControlDelegate(
      std::unique_ptr<ImeControlDelegate>(delegate));
  EXPECT_EQ(0, delegate->handle_previous_ime_count());
  EXPECT_TRUE(ProcessInController(control_space_down));
  EXPECT_EQ(1, delegate->handle_previous_ime_count());
  EXPECT_TRUE(ProcessInController(control_space_up));
  EXPECT_EQ(1, delegate->handle_previous_ime_count());
  EXPECT_EQ(0, delegate->handle_switch_ime_count());
  EXPECT_TRUE(ProcessInController(convert));
  EXPECT_EQ(1, delegate->handle_switch_ime_count());
  EXPECT_TRUE(ProcessInController(non_convert));
  EXPECT_EQ(2, delegate->handle_switch_ime_count());
  EXPECT_TRUE(ProcessInController(wide_half_1));
  EXPECT_EQ(3, delegate->handle_switch_ime_count());
  EXPECT_TRUE(ProcessInController(wide_half_2));
  EXPECT_EQ(4, delegate->handle_switch_ime_count());
  EXPECT_TRUE(ProcessInController(hangul));
  EXPECT_EQ(5, delegate->handle_switch_ime_count());
}

// TODO(nona|mazda): Remove this when crbug.com/139556 in a better way.
TEST_F(AcceleratorControllerTest, ImeGlobalAcceleratorsWorkaround139556) {
  // The workaround for crbug.com/139556 depends on the fact that we don't
  // use Shift+Alt+Enter/Space with ET_KEY_PRESSED as an accelerator. Test it.
  const ui::Accelerator shift_alt_return_press(
      ui::VKEY_RETURN, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
  EXPECT_FALSE(ProcessInController(shift_alt_return_press));
  const ui::Accelerator shift_alt_space_press(
      ui::VKEY_SPACE, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
  EXPECT_FALSE(ProcessInController(shift_alt_space_press));
}

TEST_F(AcceleratorControllerTest, PreferredReservedAccelerators) {
#if defined(OS_CHROMEOS)
  // Power key is reserved on chromeos.
  EXPECT_TRUE(GetController()->IsReserved(
      ui::Accelerator(ui::VKEY_POWER, ui::EF_NONE)));
  EXPECT_FALSE(GetController()->IsPreferred(
      ui::Accelerator(ui::VKEY_POWER, ui::EF_NONE)));
#endif
  // ALT+Tab are not reserved but preferred.
  EXPECT_FALSE(GetController()->IsReserved(
      ui::Accelerator(ui::VKEY_TAB, ui::EF_ALT_DOWN)));
  EXPECT_FALSE(GetController()->IsReserved(
      ui::Accelerator(ui::VKEY_TAB, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN)));
  EXPECT_TRUE(GetController()->IsPreferred(
      ui::Accelerator(ui::VKEY_TAB, ui::EF_ALT_DOWN)));
  EXPECT_TRUE(GetController()->IsPreferred(
      ui::Accelerator(ui::VKEY_TAB, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN)));

  // Others are not reserved nor preferred
  EXPECT_FALSE(GetController()->IsReserved(
      ui::Accelerator(ui::VKEY_PRINT, ui::EF_NONE)));
  EXPECT_FALSE(GetController()->IsPreferred(
      ui::Accelerator(ui::VKEY_PRINT, ui::EF_NONE)));
  EXPECT_FALSE(
      GetController()->IsReserved(ui::Accelerator(ui::VKEY_TAB, ui::EF_NONE)));
  EXPECT_FALSE(
      GetController()->IsPreferred(ui::Accelerator(ui::VKEY_TAB, ui::EF_NONE)));
  EXPECT_FALSE(
      GetController()->IsReserved(ui::Accelerator(ui::VKEY_A, ui::EF_NONE)));
  EXPECT_FALSE(
      GetController()->IsPreferred(ui::Accelerator(ui::VKEY_A, ui::EF_NONE)));
}

/*
namespace {

class PreferredReservedAcceleratorsTest : public test::AshTestBase {
 public:
  PreferredReservedAcceleratorsTest() {}
  ~PreferredReservedAcceleratorsTest() override {}

  // test::AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    Shell::GetInstance()->lock_state_controller()->set_animator_for_test(
        new test::TestSessionStateAnimator);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PreferredReservedAcceleratorsTest);
};

}  // namespace

// TODO: needs LockStateController ported: http://crbug.com/632189.
TEST_F(PreferredReservedAcceleratorsTest, AcceleratorsWithFullscreen) {
  WmWindow* w1 = WmWindow::Get(CreateTestWindow(gfx::Rect()));
  WmWindow* w2 = WmWindow::Get(CreateTestWindow(gfx::Rect()));
  wm::ActivateWindow(w1);

  wm::WMEvent fullscreen(wm::WM_EVENT_FULLSCREEN);
  wm::WindowState* w1_state = w1->GetWindowState();
  w1_state->OnWMEvent(&fullscreen);
  ASSERT_TRUE(w1_state->IsFullscreen());

  ui::test::EventGenerator& generator = GetEventGenerator();
#if defined(OS_CHROMEOS)
  // Power key (reserved) should always be handled.
  LockStateController::TestApi test_api(
      Shell::GetInstance()->lock_state_controller());
  EXPECT_FALSE(test_api.is_animating_lock());
  generator.PressKey(ui::VKEY_POWER, ui::EF_NONE);
  EXPECT_TRUE(test_api.is_animating_lock());
#endif

  // A fullscreen window can consume ALT-TAB (preferred).
  ASSERT_EQ(w1, wm::GetActiveWindow());
  generator.PressKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  ASSERT_EQ(w1, wm::GetActiveWindow());
  ASSERT_NE(w2, wm::GetActiveWindow());

  // ALT-TAB is non repeatable. Press A to cancel the
  // repeat record.
  generator.PressKey(ui::VKEY_A, ui::EF_NONE);
  generator.ReleaseKey(ui::VKEY_A, ui::EF_NONE);

  // A normal window shouldn't consume preferred accelerator.
  wm::WMEvent normal(wm::WM_EVENT_NORMAL);
  w1_state->OnWMEvent(&normal);
  ASSERT_FALSE(w1_state->IsFullscreen());

  EXPECT_EQ(w1, wm::GetActiveWindow());
  generator.PressKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  ASSERT_NE(w1, wm::GetActiveWindow());
  ASSERT_EQ(w2, wm::GetActiveWindow());
}

// TODO: needs LockStateController ported: http://crbug.com/632189.
TEST_F(PreferredReservedAcceleratorsTest, AcceleratorsWithPinned) {
  aura::Window* w1 = CreateTestWindowInShellWithId(0);
  aura::Window* w2 = CreateTestWindowInShellWithId(1);
  wm::ActivateWindow(w1);

  {
    wm::WMEvent pin_event(wm::WM_EVENT_PIN);
    wm::WindowState* w1_state = wm::GetWindowState(w1);
    w1_state->OnWMEvent(&pin_event);
    ASSERT_TRUE(w1_state->IsPinned());
  }

  ui::test::EventGenerator& generator = GetEventGenerator();
#if defined(OS_CHROMEOS)
  // Power key (reserved) should always be handled.
  LockStateController::TestApi test_api(
      Shell::GetInstance()->lock_state_controller());
  EXPECT_FALSE(test_api.is_animating_lock());
  generator.PressKey(ui::VKEY_POWER, ui::EF_NONE);
  EXPECT_TRUE(test_api.is_animating_lock());
#endif

  // A pinned window can consume ALT-TAB (preferred), but no side effect.
  ASSERT_EQ(w1, wm::GetActiveWindow());
  generator.PressKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  generator.ReleaseKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  ASSERT_EQ(w1, wm::GetActiveWindow());
  ASSERT_NE(w2, wm::GetActiveWindow());
}
*/

#if defined(OS_CHROMEOS)
TEST_F(AcceleratorControllerTest, DisallowedAtModalWindow) {
  std::set<AcceleratorAction> all_actions;
  for (size_t i = 0; i < kAcceleratorDataLength; ++i)
    all_actions.insert(kAcceleratorData[i].action);
  std::set<AcceleratorAction> all_debug_actions;
  for (size_t i = 0; i < kDebugAcceleratorDataLength; ++i)
    all_debug_actions.insert(kDebugAcceleratorData[i].action);
  std::set<AcceleratorAction> all_dev_actions;
  for (size_t i = 0; i < kDeveloperAcceleratorDataLength; ++i)
    all_dev_actions.insert(kDeveloperAcceleratorData[i].action);

  std::set<AcceleratorAction> actionsAllowedAtModalWindow;
  for (size_t k = 0; k < kActionsAllowedAtModalWindowLength; ++k)
    actionsAllowedAtModalWindow.insert(kActionsAllowedAtModalWindow[k]);
  for (const auto& action : actionsAllowedAtModalWindow) {
    EXPECT_TRUE(all_actions.find(action) != all_actions.end() ||
                all_debug_actions.find(action) != all_debug_actions.end() ||
                all_dev_actions.find(action) != all_dev_actions.end())
        << " action from kActionsAllowedAtModalWindow"
        << " not found in kAcceleratorData, kDebugAcceleratorData or"
        << " kDeveloperAcceleratorData action: " << action;
  }
  WmWindow* window = WmWindow::Get(CreateTestWindow(gfx::Rect(5, 5, 20, 20)));
  window->Activate();
  WmShell::Get()->SimulateModalWindowOpenForTesting(true);
  for (const auto& action : all_actions) {
    if (actionsAllowedAtModalWindow.find(action) ==
        actionsAllowedAtModalWindow.end()) {
      EXPECT_TRUE(GetController()->PerformActionIfEnabled(action))
          << " for action (disallowed at modal window): " << action;
    }
  }
  //  Testing of top row (F5-F10) accelerators that should still work
  //  when a modal window is open
  //
  // Screenshot
  // TODO: needs ScreenShotDelegate converted: http://crbug.com/612331.
  /*
    {
      test::TestScreenshotDelegate* delegate = GetScreenshotDelegate();
      delegate->set_can_take_screenshot(false);
      EXPECT_TRUE(ProcessInController(
          ui::Accelerator(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN)));
      EXPECT_TRUE(
          ProcessInController(ui::Accelerator(ui::VKEY_PRINT, ui::EF_NONE)));
      EXPECT_TRUE(ProcessInController(ui::Accelerator(
          ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_SHIFT_DOWN |
    ui::EF_CONTROL_DOWN)));
      delegate->set_can_take_screenshot(true);
      EXPECT_EQ(0, delegate->handle_take_screenshot_count());
      EXPECT_TRUE(ProcessInController(
          ui::Accelerator(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN)));
      EXPECT_EQ(1, delegate->handle_take_screenshot_count());
      EXPECT_TRUE(
          ProcessInController(ui::Accelerator(ui::VKEY_PRINT, ui::EF_NONE)));
      EXPECT_EQ(2, delegate->handle_take_screenshot_count());
      EXPECT_TRUE(ProcessInController(ui::Accelerator(
          ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_SHIFT_DOWN |
    ui::EF_CONTROL_DOWN)));
      EXPECT_EQ(2, delegate->handle_take_screenshot_count());
    }
  */
  // Brightness
  const ui::Accelerator brightness_down(ui::VKEY_BRIGHTNESS_DOWN, ui::EF_NONE);
  const ui::Accelerator brightness_up(ui::VKEY_BRIGHTNESS_UP, ui::EF_NONE);
  {
    DummyBrightnessControlDelegate* delegate =
        new DummyBrightnessControlDelegate;
    SetBrightnessControlDelegate(
        std::unique_ptr<BrightnessControlDelegate>(delegate));
    EXPECT_EQ(0, delegate->handle_brightness_down_count());
    EXPECT_TRUE(ProcessInController(brightness_down));
    EXPECT_EQ(1, delegate->handle_brightness_down_count());
    EXPECT_EQ(brightness_down, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_brightness_up_count());
    EXPECT_TRUE(ProcessInController(brightness_up));
    EXPECT_EQ(1, delegate->handle_brightness_up_count());
    EXPECT_EQ(brightness_up, delegate->last_accelerator());
  }
  // Volume
  const ui::Accelerator volume_mute(ui::VKEY_VOLUME_MUTE, ui::EF_NONE);
  const ui::Accelerator volume_down(ui::VKEY_VOLUME_DOWN, ui::EF_NONE);
  const ui::Accelerator volume_up(ui::VKEY_VOLUME_UP, ui::EF_NONE);
  {
    base::UserActionTester user_action_tester;
    ui::AcceleratorHistory* history = GetController()->accelerator_history();

    EXPECT_EQ(0, user_action_tester.GetActionCount("Accel_VolumeMute_F8"));
    EXPECT_TRUE(ProcessInController(volume_mute));
    EXPECT_EQ(1, user_action_tester.GetActionCount("Accel_VolumeMute_F8"));
    EXPECT_EQ(volume_mute, history->current_accelerator());

    EXPECT_EQ(0, user_action_tester.GetActionCount("Accel_VolumeDown_F9"));
    EXPECT_TRUE(ProcessInController(volume_down));
    EXPECT_EQ(1, user_action_tester.GetActionCount("Accel_VolumeDown_F9"));
    EXPECT_EQ(volume_down, history->current_accelerator());

    EXPECT_EQ(0, user_action_tester.GetActionCount("Accel_VolumeUp_F10"));
    EXPECT_TRUE(ProcessInController(volume_up));
    EXPECT_EQ(volume_up, history->current_accelerator());
    EXPECT_EQ(1, user_action_tester.GetActionCount("Accel_VolumeUp_F10"));
  }
}
#endif

// TODO: reenable. Disabled because shelf asynchronously created:
// http://crbug.com/632192.
TEST_F(AcceleratorControllerTest, DISABLED_DisallowedWithNoWindow) {
  AccessibilityDelegate* delegate = WmShell::Get()->accessibility_delegate();

  for (size_t i = 0; i < kActionsNeedingWindowLength; ++i) {
    delegate->TriggerAccessibilityAlert(A11Y_ALERT_NONE);
    EXPECT_TRUE(
        GetController()->PerformActionIfEnabled(kActionsNeedingWindow[i]));
    EXPECT_EQ(delegate->GetLastAccessibilityAlert(), A11Y_ALERT_WINDOW_NEEDED);
  }

  // Make sure we don't alert if we do have a window.
  for (size_t i = 0; i < kActionsNeedingWindowLength; ++i) {
    aura::Window* aura_window = CreateTestWindow(gfx::Rect(5, 5, 20, 20));
    WmWindow::Get(aura_window)->Activate();
    delegate->TriggerAccessibilityAlert(A11Y_ALERT_NONE);
    GetController()->PerformActionIfEnabled(kActionsNeedingWindow[i]);
    EXPECT_NE(delegate->GetLastAccessibilityAlert(), A11Y_ALERT_WINDOW_NEEDED);
    delete aura_window;
  }

  // Don't alert if we have a minimized window either.
  for (size_t i = 0; i < kActionsNeedingWindowLength; ++i) {
    aura::Window* aura_window = CreateTestWindow(gfx::Rect(5, 5, 20, 20));
    WmWindow::Get(aura_window)->Activate();
    GetController()->PerformActionIfEnabled(WINDOW_MINIMIZE);
    delegate->TriggerAccessibilityAlert(A11Y_ALERT_NONE);
    GetController()->PerformActionIfEnabled(kActionsNeedingWindow[i]);
    EXPECT_NE(delegate->GetLastAccessibilityAlert(), A11Y_ALERT_WINDOW_NEEDED);
    delete aura_window;
  }
}

#if defined(OS_CHROMEOS)
namespace {

// defines a class to test the behavior of deprecated accelerators.
class DeprecatedAcceleratorTester : public AcceleratorControllerTest {
 public:
  DeprecatedAcceleratorTester() {}
  ~DeprecatedAcceleratorTester() override {}

  // TODO: this override should be removed and put in WmTestBase.
  // http://crbug.com/632200.
  void UnblockUserSession() {}

  void SetUp() override {
    AcceleratorControllerTest::SetUp();

    // For testing the deprecated and new IME shortcuts.
    DummyImeControlDelegate* delegate = new DummyImeControlDelegate;
    GetController()->SetImeControlDelegate(
        std::unique_ptr<ImeControlDelegate>(delegate));
  }

  ui::Accelerator CreateAccelerator(const AcceleratorData& data) const {
    ui::Accelerator result(data.keycode, data.modifiers);
    result.set_type(data.trigger_on_press ? ui::ET_KEY_PRESSED
                                          : ui::ET_KEY_RELEASED);
    return result;
  }

  void ResetStateIfNeeded() {
    if (WmShell::Get()->GetSessionStateDelegate()->IsScreenLocked() ||
        WmShell::Get()->GetSessionStateDelegate()->IsUserSessionBlocked()) {
      UnblockUserSession();
    }
  }

  bool ContainsDeprecatedAcceleratorNotification(const char* const id) const {
    return nullptr != message_center()->FindVisibleNotificationById(id);
  }

  bool IsMessageCenterEmpty() const {
    return message_center()->GetVisibleNotifications().empty();
  }

  void RemoveAllNotifications() const {
    message_center()->RemoveAllNotifications(
        false /* by_user */, message_center::MessageCenter::RemoveType::ALL);
  }

  message_center::MessageCenter* message_center() const {
    return message_center::MessageCenter::Get();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeprecatedAcceleratorTester);
};

}  // namespace

// TODO: disabled because of UnblockUserSession() not working:
// http://crbug.com/632201.
TEST_F(DeprecatedAcceleratorTester,
       DISABLED_TestDeprecatedAcceleratorsBehavior) {
  for (size_t i = 0; i < kDeprecatedAcceleratorsLength; ++i) {
    const AcceleratorData& entry = kDeprecatedAccelerators[i];

    auto itr = GetController()->actions_with_deprecations_.find(entry.action);
    ASSERT_TRUE(itr != GetController()->actions_with_deprecations_.end());
    const DeprecatedAcceleratorData* data = itr->second;

    EXPECT_TRUE(IsMessageCenterEmpty());
    ui::Accelerator deprecated_accelerator = CreateAccelerator(entry);
    if (data->deprecated_enabled)
      EXPECT_TRUE(ProcessInController(deprecated_accelerator));
    else
      EXPECT_FALSE(ProcessInController(deprecated_accelerator));

    // We expect to see a notification in the message center.
    EXPECT_TRUE(
        ContainsDeprecatedAcceleratorNotification(data->uma_histogram_name));
    RemoveAllNotifications();

    // If the action is LOCK_SCREEN, we must reset the state by unlocking the
    // screen before we proceed testing the rest of accelerators.
    ResetStateIfNeeded();
  }
}

// TODO: disabled because of UnblockUserSession() not working:
// http://crbug.com/632201.
TEST_F(DeprecatedAcceleratorTester, DISABLED_TestNewAccelerators) {
  // Add below the new accelerators that replaced the deprecated ones (if any).
  const AcceleratorData kNewAccelerators[] = {
      {true, ui::VKEY_L, ui::EF_COMMAND_DOWN, LOCK_SCREEN},
      {true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN, NEXT_IME},
      {true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN, SHOW_TASK_MANAGER},
  };

  EXPECT_TRUE(IsMessageCenterEmpty());

  for (auto data : kNewAccelerators) {
    EXPECT_TRUE(ProcessInController(CreateAccelerator(data)));

    // Expect no notifications from the new accelerators.
    EXPECT_TRUE(IsMessageCenterEmpty());

    // If the action is LOCK_SCREEN, we must reset the state by unlocking the
    // screen before we proceed testing the rest of accelerators.
    ResetStateIfNeeded();
  }
}
#endif  // defined(OS_CHROMEOS)

}  // namespace ash
