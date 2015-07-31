// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_controller.h"

#include "ash/ash_switches.h"
#include "ash/display/display_info.h"
#include "ash/display/display_layout_store.h"
#include "ash/display/display_manager.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/cursor_manager_test_api.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/command_line.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_handler.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/mouse_watcher_view_host.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

const char kDesktopBackgroundView[] = "DesktopBackgroundView";

template<typename T>
class Resetter {
 public:
  explicit Resetter(T* value) : value_(*value) {
    *value = 0;
  }
  ~Resetter() { }
  T value() { return value_; }

 private:
  T value_;
  DISALLOW_COPY_AND_ASSIGN(Resetter);
};

class TestObserver : public DisplayController::Observer,
                     public gfx::DisplayObserver,
                     public aura::client::FocusChangeObserver,
                     public aura::client::ActivationChangeObserver {
 public:
  TestObserver()
      : changing_count_(0),
        changed_count_(0),
        bounds_changed_count_(0),
        rotation_changed_count_(0),
        workarea_changed_count_(0),
        primary_changed_count_(0),
        changed_display_id_(0),
        focus_changed_count_(0),
        activation_changed_count_(0) {
    Shell::GetInstance()->display_controller()->AddObserver(this);
    Shell::GetScreen()->AddObserver(this);
    aura::client::GetFocusClient(Shell::GetPrimaryRootWindow())->
        AddObserver(this);
    aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())->
        AddObserver(this);
  }

  ~TestObserver() override {
    Shell::GetInstance()->display_controller()->RemoveObserver(this);
    Shell::GetScreen()->RemoveObserver(this);
    aura::client::GetFocusClient(Shell::GetPrimaryRootWindow())->
        RemoveObserver(this);
    aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())->
        RemoveObserver(this);
  }

  // Overridden from DisplayController::Observer
  void OnDisplayConfigurationChanging() override { ++changing_count_; }
  void OnDisplayConfigurationChanged() override { ++changed_count_; }

  // Overrideen from gfx::DisplayObserver
  void OnDisplayMetricsChanged(const gfx::Display& display,
                               uint32_t metrics) override {
    changed_display_id_ = display.id();
    if (metrics & DISPLAY_METRIC_BOUNDS)
      ++bounds_changed_count_;
    if (metrics & DISPLAY_METRIC_ROTATION)
      ++rotation_changed_count_;
    if (metrics & DISPLAY_METRIC_WORK_AREA)
      ++workarea_changed_count_;
    if (metrics & DISPLAY_METRIC_PRIMARY)
      ++primary_changed_count_;
  }
  void OnDisplayAdded(const gfx::Display& new_display) override {}
  void OnDisplayRemoved(const gfx::Display& old_display) override {}

  // Overridden from aura::client::FocusChangeObserver
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override {
    focus_changed_count_++;
  }

  // Overridden from aura::client::ActivationChangeObserver
  void OnWindowActivated(aura::Window* gained_active,
                         aura::Window* lost_active) override {
    activation_changed_count_++;
  }
  void OnAttemptToReactivateWindow(aura::Window* request_active,
                                   aura::Window* actual_active) override {}

  int CountAndReset() {
    EXPECT_EQ(changing_count_, changed_count_);
    changed_count_ = 0;
    return Resetter<int>(&changing_count_).value();
  }

  int64 GetBoundsChangedCountAndReset() {
    return Resetter<int>(&bounds_changed_count_).value();
  }

  int64 GetRotationChangedCountAndReset() {
    return Resetter<int>(&rotation_changed_count_).value();
  }

  int64 GetWorkareaChangedCountAndReset() {
    return Resetter<int>(&workarea_changed_count_).value();
  }

  int64 GetPrimaryChangedCountAndReset() {
    return Resetter<int>(&primary_changed_count_).value();
  }

  int64 GetChangedDisplayIdAndReset() {
    return Resetter<int64>(&changed_display_id_).value();
  }

  int GetFocusChangedCountAndReset() {
    return Resetter<int>(&focus_changed_count_).value();
  }

  int GetActivationChangedCountAndReset() {
    return Resetter<int>(&activation_changed_count_).value();
  }

 private:
  int changing_count_;
  int changed_count_;

  int bounds_changed_count_;
  int rotation_changed_count_;
  int workarea_changed_count_;
  int primary_changed_count_;
  int64 changed_display_id_;

  int focus_changed_count_;
  int activation_changed_count_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

gfx::Display GetPrimaryDisplay() {
  return Shell::GetScreen()->GetDisplayNearestWindow(
      Shell::GetAllRootWindows()[0]);
}

gfx::Display GetSecondaryDisplay() {
  return Shell::GetScreen()->GetDisplayNearestWindow(
      Shell::GetAllRootWindows()[1]);
}

void SetSecondaryDisplayLayoutAndOffset(DisplayLayout::Position position,
                                        int offset) {
  DisplayLayout layout(position, offset);
  ASSERT_GT(Shell::GetScreen()->GetNumDisplays(), 1);
  Shell::GetInstance()->display_manager()->
      SetLayoutForCurrentDisplays(layout);
}

void SetSecondaryDisplayLayout(DisplayLayout::Position position) {
  SetSecondaryDisplayLayoutAndOffset(position, 0);
}

void SetDefaultDisplayLayout(DisplayLayout::Position position) {
  Shell::GetInstance()->display_manager()->layout_store()->
      SetDefaultDisplayLayout(DisplayLayout(position, 0));
}

class DisplayControllerShutdownTest : public test::AshTestBase {
 public:
  DisplayControllerShutdownTest() {}
  ~DisplayControllerShutdownTest() override {}

  void TearDown() override {
    test::AshTestBase::TearDown();
    if (!SupportsMultipleDisplays())
      return;

    // Make sure that primary display is accessible after shutdown.
    gfx::Display primary = Shell::GetScreen()->GetPrimaryDisplay();
    EXPECT_EQ("0,0 444x333", primary.bounds().ToString());
    EXPECT_EQ(2, Shell::GetScreen()->GetNumDisplays());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayControllerShutdownTest);
};

class StartupHelper : public test::TestShellDelegate,
                      public DisplayController::Observer {
 public:
  StartupHelper() : displays_initialized_(false) {}
  ~StartupHelper() override {}

  // ash::ShellSelegate:
  void PreInit() override {
    Shell::GetInstance()->display_controller()->AddObserver(this);
  }

  // ash::DisplayController::Observer:
  void OnDisplaysInitialized() override {
    DCHECK(!displays_initialized_);
    displays_initialized_ = true;
  }

  bool displays_initialized() const {
    return displays_initialized_;
  }

 private:
  bool displays_initialized_;

  DISALLOW_COPY_AND_ASSIGN(StartupHelper);
};

class DisplayControllerStartupTest : public test::AshTestBase {
 public:
  DisplayControllerStartupTest() : startup_helper_(new StartupHelper) {}
  ~DisplayControllerStartupTest() override {}

  // ash::test::AshTestBase:
  void SetUp() override {
    ash_test_helper()->set_test_shell_delegate(startup_helper_);
    test::AshTestBase::SetUp();
  }
  void TearDown() override {
    Shell::GetInstance()->display_controller()->RemoveObserver(startup_helper_);
    test::AshTestBase::TearDown();
  }

  const StartupHelper* startup_helper() const { return startup_helper_; }

 private:
  StartupHelper* startup_helper_;  // Owned by ash::Shell.

  DISALLOW_COPY_AND_ASSIGN(DisplayControllerStartupTest);
};

class TestEventHandler : public ui::EventHandler {
 public:
  TestEventHandler() : target_root_(NULL),
                       touch_radius_x_(0.0),
                       touch_radius_y_(0.0),
                       scroll_x_offset_(0.0),
                       scroll_y_offset_(0.0),
                       scroll_x_offset_ordinal_(0.0),
                       scroll_y_offset_ordinal_(0.0) {}
  ~TestEventHandler() override {}

  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->flags() & ui::EF_IS_SYNTHESIZED &&
        event->type() != ui::ET_MOUSE_EXITED &&
        event->type() != ui::ET_MOUSE_ENTERED) {
      return;
    }
    aura::Window* target = static_cast<aura::Window*>(event->target());
    mouse_location_ = event->root_location();
    target_root_ = target->GetRootWindow();
    event->StopPropagation();
  }

  void OnTouchEvent(ui::TouchEvent* event) override {
    aura::Window* target = static_cast<aura::Window*>(event->target());
    // Only record when the target is the background which covers
    // entire root window.
    if (target->name() != kDesktopBackgroundView)
      return;
    touch_radius_x_ = event->radius_x();
    touch_radius_y_ = event->radius_y();
    event->StopPropagation();
  }

  void OnScrollEvent(ui::ScrollEvent* event) override {
    aura::Window* target = static_cast<aura::Window*>(event->target());
    // Only record when the target is the background which covers
    // entire root window.
    if (target->name() != kDesktopBackgroundView)
      return;

    if (event->type() == ui::ET_SCROLL) {
      scroll_x_offset_ = event->x_offset();
      scroll_y_offset_ = event->y_offset();
      scroll_x_offset_ordinal_ = event->x_offset_ordinal();
      scroll_y_offset_ordinal_ = event->y_offset_ordinal();
    }
    event->StopPropagation();
  }

  std::string GetLocationAndReset() {
    std::string result = mouse_location_.ToString();
    mouse_location_.SetPoint(0, 0);
    target_root_ = NULL;
    return result;
  }

  float touch_radius_x() { return touch_radius_x_; }
  float touch_radius_y() { return touch_radius_y_; }
  float scroll_x_offset() { return scroll_x_offset_; }
  float scroll_y_offset() { return scroll_y_offset_; }
  float scroll_x_offset_ordinal() { return scroll_x_offset_ordinal_; }
  float scroll_y_offset_ordinal() { return scroll_y_offset_ordinal_; }

 private:
  gfx::Point mouse_location_;
  aura::Window* target_root_;

  float touch_radius_x_;
  float touch_radius_y_;
  float scroll_x_offset_;
  float scroll_y_offset_;
  float scroll_x_offset_ordinal_;
  float scroll_y_offset_ordinal_;

  DISALLOW_COPY_AND_ASSIGN(TestEventHandler);
};

float GetStoredUIScale(int64 id) {
  return Shell::GetInstance()->display_manager()->GetDisplayInfo(id).
      GetEffectiveUIScale();
}

class TestMouseWatcherListener : public views::MouseWatcherListener {
 public:
  TestMouseWatcherListener() {}

 private:
  // views::MouseWatcherListener:
  void MouseMovedOutOfHost() override {}

  DISALLOW_COPY_AND_ASSIGN(TestMouseWatcherListener);
};

}  // namespace

typedef test::AshTestBase DisplayControllerTest;

TEST_F(DisplayControllerShutdownTest, Shutdown) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("444x333, 200x200");
}

TEST_F(DisplayControllerStartupTest, Startup) {
  if (!SupportsMultipleDisplays())
    return;

  EXPECT_TRUE(startup_helper()->displays_initialized());
}

TEST_F(DisplayControllerTest, SecondaryDisplayLayout) {
  if (!SupportsMultipleDisplays())
    return;

  // Creates windows to catch activation change event.
  scoped_ptr<aura::Window> w1(CreateTestWindowInShellWithId(1));
  w1->Focus();

  TestObserver observer;
  UpdateDisplay("500x500,400x400");
  EXPECT_EQ(1, observer.CountAndReset());  // resize and add
  EXPECT_EQ(1, observer.GetBoundsChangedCountAndReset());
  EXPECT_EQ(1, observer.GetWorkareaChangedCountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  gfx::Insets insets(5, 5, 5, 5);
  int64 secondary_display_id = ScreenUtil::GetSecondaryDisplay().id();
  Shell::GetInstance()->display_manager()->UpdateWorkAreaOfDisplay(
      secondary_display_id, insets);

  // Default layout is RIGHT.
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("500,0 400x400", GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ("505,5 390x390", GetSecondaryDisplay().work_area().ToString());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());

  // Layout the secondary display to the bottom of the primary.
  SetSecondaryDisplayLayout(DisplayLayout::BOTTOM);
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ(1, observer.GetBoundsChangedCountAndReset());
  EXPECT_EQ(1, observer.GetWorkareaChangedCountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ(secondary_display_id, observer.GetChangedDisplayIdAndReset());
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("0,500 400x400", GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ("5,505 390x390", GetSecondaryDisplay().work_area().ToString());

  // Layout the secondary display to the left of the primary.
  SetSecondaryDisplayLayout(DisplayLayout::LEFT);
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ(1, observer.GetBoundsChangedCountAndReset());
  EXPECT_EQ(1, observer.GetWorkareaChangedCountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ(secondary_display_id, observer.GetChangedDisplayIdAndReset());
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("-400,0 400x400", GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ("-395,5 390x390", GetSecondaryDisplay().work_area().ToString());

  // Layout the secondary display to the top of the primary.
  SetSecondaryDisplayLayout(DisplayLayout::TOP);
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ(1, observer.GetBoundsChangedCountAndReset());
  EXPECT_EQ(1, observer.GetWorkareaChangedCountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ(secondary_display_id, observer.GetChangedDisplayIdAndReset());
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("0,-400 400x400", GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ("5,-395 390x390", GetSecondaryDisplay().work_area().ToString());

  // Layout to the right with an offset.
  SetSecondaryDisplayLayoutAndOffset(DisplayLayout::RIGHT, 300);
  EXPECT_EQ(1, observer.CountAndReset());  // resize and add
  EXPECT_EQ(1, observer.GetBoundsChangedCountAndReset());
  EXPECT_EQ(1, observer.GetWorkareaChangedCountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ(secondary_display_id, observer.GetChangedDisplayIdAndReset());
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("500,300 400x400", GetSecondaryDisplay().bounds().ToString());

  // Keep the minimum 100.
  SetSecondaryDisplayLayoutAndOffset(DisplayLayout::RIGHT, 490);
  EXPECT_EQ(1, observer.CountAndReset());  // resize and add
  EXPECT_EQ(1, observer.GetBoundsChangedCountAndReset());
  EXPECT_EQ(1, observer.GetWorkareaChangedCountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ(secondary_display_id, observer.GetChangedDisplayIdAndReset());
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("500,400 400x400", GetSecondaryDisplay().bounds().ToString());

  SetSecondaryDisplayLayoutAndOffset(DisplayLayout::RIGHT, -400);
  EXPECT_EQ(secondary_display_id, observer.GetChangedDisplayIdAndReset());
  EXPECT_EQ(1, observer.GetBoundsChangedCountAndReset());
  EXPECT_EQ(1, observer.GetWorkareaChangedCountAndReset());
  EXPECT_EQ(1, observer.CountAndReset());  // resize and add
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("500,-300 400x400", GetSecondaryDisplay().bounds().ToString());

  //  Layout to the bottom with an offset.
  SetSecondaryDisplayLayoutAndOffset(DisplayLayout::BOTTOM, -200);
  EXPECT_EQ(secondary_display_id, observer.GetChangedDisplayIdAndReset());
  EXPECT_EQ(1, observer.GetBoundsChangedCountAndReset());
  EXPECT_EQ(1, observer.GetWorkareaChangedCountAndReset());
  EXPECT_EQ(1, observer.CountAndReset());  // resize and add
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("-200,500 400x400", GetSecondaryDisplay().bounds().ToString());

  // Keep the minimum 100.
  SetSecondaryDisplayLayoutAndOffset(DisplayLayout::BOTTOM, 490);
  EXPECT_EQ(secondary_display_id, observer.GetChangedDisplayIdAndReset());
  EXPECT_EQ(1, observer.GetBoundsChangedCountAndReset());
  EXPECT_EQ(1, observer.GetWorkareaChangedCountAndReset());
  EXPECT_EQ(1, observer.CountAndReset());  // resize and add
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("400,500 400x400", GetSecondaryDisplay().bounds().ToString());

  SetSecondaryDisplayLayoutAndOffset(DisplayLayout::BOTTOM, -400);
  EXPECT_EQ(secondary_display_id, observer.GetChangedDisplayIdAndReset());
  EXPECT_EQ(1, observer.GetBoundsChangedCountAndReset());
  EXPECT_EQ(1, observer.GetWorkareaChangedCountAndReset());
  EXPECT_EQ(1, observer.CountAndReset());  // resize and add
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("-300,500 400x400", GetSecondaryDisplay().bounds().ToString());

  // Setting the same layout shouldn't invoke observers.
  SetSecondaryDisplayLayoutAndOffset(DisplayLayout::BOTTOM, -400);
  EXPECT_EQ(0, observer.GetChangedDisplayIdAndReset());
  EXPECT_EQ(0, observer.GetBoundsChangedCountAndReset());
  EXPECT_EQ(0, observer.GetWorkareaChangedCountAndReset());
  EXPECT_EQ(0, observer.CountAndReset());  // resize and add
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("-300,500 400x400", GetSecondaryDisplay().bounds().ToString());

  UpdateDisplay("500x500");
  EXPECT_LE(1, observer.GetFocusChangedCountAndReset());
  EXPECT_LE(1, observer.GetActivationChangedCountAndReset());
}

namespace {

DisplayInfo CreateDisplayInfo(int64 id,
                              int y,
                              gfx::Display::Rotation rotation) {
  DisplayInfo info(id, "", false);
  info.SetBounds(gfx::Rect(0, y, 500, 500));
  info.SetRotation(rotation, gfx::Display::ROTATION_SOURCE_ACTIVE);
  return info;
}

DisplayInfo CreateMirroredDisplayInfo(int64 id,
                                      float device_scale_factor) {
  DisplayInfo info = CreateDisplayInfo(id, 0, gfx::Display::ROTATE_0);
  info.set_device_scale_factor(device_scale_factor);
  return info;
}

}  // namespace

TEST_F(DisplayControllerTest, MirrorToDockedWithFullscreen) {
  if (!SupportsMultipleDisplays())
    return;

  // Creates windows to catch activation change event.
  scoped_ptr<aura::Window> w1(CreateTestWindowInShellWithId(1));
  w1->Focus();

  // Docked mode.
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();

  const DisplayInfo internal_display_info =
      CreateMirroredDisplayInfo(1, 2.0f);
  const DisplayInfo external_display_info =
      CreateMirroredDisplayInfo(2, 1.0f);

  std::vector<DisplayInfo> display_info_list;
  // Mirror.
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info);
  display_manager->OnNativeDisplaysChanged(display_info_list);
  const int64 internal_display_id =
      test::DisplayManagerTestApi(display_manager).
      SetFirstDisplayAsInternalDisplay();
  EXPECT_EQ(1, internal_display_id);
  EXPECT_EQ(2U, display_manager->num_connected_displays());
  EXPECT_EQ(1U, display_manager->GetNumDisplays());

  wm::WindowState* window_state = wm::GetWindowState(w1.get());
  const wm::WMEvent toggle_fullscreen_event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  window_state->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ("0,0 250x250", w1->bounds().ToString());
  // Dock mode.
  TestObserver observer;
  display_info_list.clear();
  display_info_list.push_back(external_display_info);
  display_manager->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager->GetNumDisplays());
  EXPECT_EQ(1U, display_manager->num_connected_displays());
  // Observers are called due to primary change.
  EXPECT_EQ(2, observer.GetChangedDisplayIdAndReset());
  EXPECT_EQ(1, observer.GetBoundsChangedCountAndReset());
  EXPECT_EQ(1, observer.GetWorkareaChangedCountAndReset());
  EXPECT_EQ(1, observer.GetPrimaryChangedCountAndReset());
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());

  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ("0,0 500x500", w1->bounds().ToString());
}

TEST_F(DisplayControllerTest, BoundsUpdated) {
  if (!SupportsMultipleDisplays())
    return;

  // Creates windows to catch activation change event.
  scoped_ptr<aura::Window> w1(CreateTestWindowInShellWithId(1));
  w1->Focus();

  TestObserver observer;
  SetDefaultDisplayLayout(DisplayLayout::BOTTOM);
  UpdateDisplay("200x200,300x300");  // layout, resize and add.
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());

  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  gfx::Insets insets(5, 5, 5, 5);
  display_manager->UpdateWorkAreaOfDisplay(
      ScreenUtil::GetSecondaryDisplay().id(), insets);

  EXPECT_EQ("0,0 200x200", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("0,200 300x300", GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ("5,205 290x290", GetSecondaryDisplay().work_area().ToString());

  UpdateDisplay("400x400,200x200");
  EXPECT_EQ(1, observer.CountAndReset());  // two resizes
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ("0,0 400x400", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("0,400 200x200", GetSecondaryDisplay().bounds().ToString());

  UpdateDisplay("400x400,300x300");
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ("0,0 400x400", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("0,400 300x300", GetSecondaryDisplay().bounds().ToString());

  UpdateDisplay("400x400");
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_LE(1, observer.GetFocusChangedCountAndReset());
  EXPECT_LE(1, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ("0,0 400x400", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ(1, Shell::GetScreen()->GetNumDisplays());

  UpdateDisplay("400x500*2,300x300");
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  ASSERT_EQ(2, Shell::GetScreen()->GetNumDisplays());
  EXPECT_EQ("0,0 200x250", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("0,250 300x300", GetSecondaryDisplay().bounds().ToString());

  // No change
  UpdateDisplay("400x500*2,300x300");
  // We still call into Pre/PostDisplayConfigurationChange().
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());

  // Rotation
  observer.GetRotationChangedCountAndReset();  // we only want to reset.
  int64 primary_id = GetPrimaryDisplay().id();
  display_manager->SetDisplayRotation(primary_id, gfx::Display::ROTATE_90,
                                      gfx::Display::ROTATION_SOURCE_ACTIVE);
  EXPECT_EQ(1, observer.GetRotationChangedCountAndReset());
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  display_manager->SetDisplayRotation(primary_id, gfx::Display::ROTATE_90,
                                      gfx::Display::ROTATION_SOURCE_ACTIVE);
  EXPECT_EQ(0, observer.GetRotationChangedCountAndReset());
  EXPECT_EQ(0, observer.CountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());

  // UI scale is eanbled only on internal display.
  int64 secondary_id = GetSecondaryDisplay().id();
  test::DisplayManagerTestApi(display_manager)
      .SetInternalDisplayId(secondary_id);

  display_manager->SetDisplayUIScale(secondary_id, 1.125f);
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  display_manager->SetDisplayUIScale(secondary_id, 1.125f);
  EXPECT_EQ(0, observer.CountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  display_manager->SetDisplayUIScale(primary_id, 1.125f);
  EXPECT_EQ(0, observer.CountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  display_manager->SetDisplayUIScale(primary_id, 1.125f);
  EXPECT_EQ(0, observer.CountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
}

TEST_F(DisplayControllerTest, SwapPrimary) {
  if (!SupportsMultipleDisplays())
    return;

  DisplayController* display_controller =
      Shell::GetInstance()->display_controller();
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();

  UpdateDisplay("200x200,300x300");
  gfx::Display primary_display = Shell::GetScreen()->GetPrimaryDisplay();
  gfx::Display secondary_display = ScreenUtil::GetSecondaryDisplay();

  DisplayLayout display_layout(DisplayLayout::RIGHT, 50);
  display_manager->SetLayoutForCurrentDisplays(display_layout);

  EXPECT_NE(primary_display.id(), secondary_display.id());
  aura::Window* primary_root =
      display_controller->GetRootWindowForDisplayId(primary_display.id());
  aura::Window* secondary_root =
      display_controller->GetRootWindowForDisplayId(secondary_display.id());
  EXPECT_NE(primary_root, secondary_root);
  aura::Window* shelf_window =
      Shelf::ForPrimaryDisplay()->shelf_widget()->GetNativeView();
  EXPECT_TRUE(primary_root->Contains(shelf_window));
  EXPECT_FALSE(secondary_root->Contains(shelf_window));
  EXPECT_EQ(primary_display.id(),
            Shell::GetScreen()->GetDisplayNearestPoint(
                gfx::Point(-100, -100)).id());
  EXPECT_EQ(primary_display.id(),
            Shell::GetScreen()->GetDisplayNearestWindow(NULL).id());

  EXPECT_EQ("0,0 200x200", primary_display.bounds().ToString());
  EXPECT_EQ("0,0 200x153", primary_display.work_area().ToString());
  EXPECT_EQ("200,0 300x300", secondary_display.bounds().ToString());
  EXPECT_EQ("200,0 300x253", secondary_display.work_area().ToString());
  EXPECT_EQ("right, 50",
            display_manager->GetCurrentDisplayLayout().ToString());

  // Switch primary and secondary
  display_controller->SetPrimaryDisplay(secondary_display);
  const DisplayLayout& inverted_layout =
      display_manager->GetCurrentDisplayLayout();
  EXPECT_EQ("left, -50", inverted_layout.ToString());

  EXPECT_EQ(secondary_display.id(),
            Shell::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_EQ(primary_display.id(), ScreenUtil::GetSecondaryDisplay().id());
  EXPECT_EQ(primary_display.id(),
            Shell::GetScreen()->GetDisplayNearestPoint(
                gfx::Point(-100, -100)).id());
  EXPECT_EQ(secondary_display.id(),
            Shell::GetScreen()->GetDisplayNearestWindow(NULL).id());

  EXPECT_EQ(
      primary_root,
      display_controller->GetRootWindowForDisplayId(secondary_display.id()));
  EXPECT_EQ(
      secondary_root,
      display_controller->GetRootWindowForDisplayId(primary_display.id()));
  EXPECT_TRUE(primary_root->Contains(shelf_window));
  EXPECT_FALSE(secondary_root->Contains(shelf_window));

  // Test if the bounds are correctly swapped.
  gfx::Display swapped_primary = Shell::GetScreen()->GetPrimaryDisplay();
  gfx::Display swapped_secondary = ScreenUtil::GetSecondaryDisplay();
  EXPECT_EQ("0,0 300x300", swapped_primary.bounds().ToString());
  EXPECT_EQ("0,0 300x253", swapped_primary.work_area().ToString());
  EXPECT_EQ("-200,-50 200x200", swapped_secondary.bounds().ToString());

  EXPECT_EQ("-200,-50 200x153", swapped_secondary.work_area().ToString());

  aura::WindowTracker tracker;
  tracker.Add(primary_root);
  tracker.Add(secondary_root);

  // Deleting 2nd display should move the primary to original primary display.
  UpdateDisplay("200x200");
  RunAllPendingInMessageLoop();  // RootWindow is deleted in a posted task.
  EXPECT_EQ(1, Shell::GetScreen()->GetNumDisplays());
  EXPECT_EQ(primary_display.id(), Shell::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_EQ(primary_display.id(),
            Shell::GetScreen()->GetDisplayNearestPoint(
                gfx::Point(-100, -100)).id());
  EXPECT_EQ(primary_display.id(),
            Shell::GetScreen()->GetDisplayNearestWindow(NULL).id());
  EXPECT_TRUE(tracker.Contains(primary_root));
  EXPECT_FALSE(tracker.Contains(secondary_root));
  EXPECT_TRUE(primary_root->Contains(shelf_window));
}

TEST_F(DisplayControllerTest, FindNearestDisplay) {
  if (!SupportsMultipleDisplays())
    return;

  DisplayController* display_controller =
      Shell::GetInstance()->display_controller();
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();

  UpdateDisplay("200x200,300x300");
  DisplayLayout display_layout(DisplayLayout::RIGHT, 50);
  display_manager->SetLayoutForCurrentDisplays(display_layout);

  gfx::Display primary_display = Shell::GetScreen()->GetPrimaryDisplay();
  gfx::Display secondary_display = ScreenUtil::GetSecondaryDisplay();
  EXPECT_NE(primary_display.id(), secondary_display.id());
  aura::Window* primary_root =
      display_controller->GetRootWindowForDisplayId(primary_display.id());
  aura::Window* secondary_root =
      display_controller->GetRootWindowForDisplayId(secondary_display.id());
  EXPECT_NE(primary_root, secondary_root);

  // Test that points outside of any display return the nearest display.
  EXPECT_EQ(primary_display.id(), Shell::GetScreen()->GetDisplayNearestPoint(
      gfx::Point(-100, 0)).id());
  EXPECT_EQ(primary_display.id(), Shell::GetScreen()->GetDisplayNearestPoint(
      gfx::Point(0, -100)).id());
  EXPECT_EQ(primary_display.id(), Shell::GetScreen()->GetDisplayNearestPoint(
      gfx::Point(100, 100)).id());
  EXPECT_EQ(primary_display.id(), Shell::GetScreen()->GetDisplayNearestPoint(
      gfx::Point(224, 25)).id());
  EXPECT_EQ(secondary_display.id(), Shell::GetScreen()->GetDisplayNearestPoint(
      gfx::Point(226, 25)).id());
  EXPECT_EQ(secondary_display.id(), Shell::GetScreen()->GetDisplayNearestPoint(
      gfx::Point(600, 100)).id());
  EXPECT_EQ(primary_display.id(), Shell::GetScreen()->GetDisplayNearestPoint(
      gfx::Point(174, 225)).id());
  EXPECT_EQ(secondary_display.id(), Shell::GetScreen()->GetDisplayNearestPoint(
      gfx::Point(176, 225)).id());
  EXPECT_EQ(secondary_display.id(), Shell::GetScreen()->GetDisplayNearestPoint(
      gfx::Point(300, 400)).id());
}

TEST_F(DisplayControllerTest, SwapPrimaryById) {
  if (!SupportsMultipleDisplays())
    return;

  DisplayController* display_controller =
      Shell::GetInstance()->display_controller();
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();

  UpdateDisplay("200x200,300x300");
  gfx::Display primary_display = Shell::GetScreen()->GetPrimaryDisplay();
  gfx::Display secondary_display = ScreenUtil::GetSecondaryDisplay();

  DisplayLayout display_layout(DisplayLayout::RIGHT, 50);
  display_manager->SetLayoutForCurrentDisplays(display_layout);

  EXPECT_NE(primary_display.id(), secondary_display.id());
  aura::Window* primary_root =
      display_controller->GetRootWindowForDisplayId(primary_display.id());
  aura::Window* secondary_root =
      display_controller->GetRootWindowForDisplayId(secondary_display.id());
  aura::Window* shelf_window =
      Shelf::ForPrimaryDisplay()->shelf_widget()->GetNativeView();
  EXPECT_TRUE(primary_root->Contains(shelf_window));
  EXPECT_FALSE(secondary_root->Contains(shelf_window));
  EXPECT_NE(primary_root, secondary_root);
  EXPECT_EQ(primary_display.id(),
            Shell::GetScreen()->GetDisplayNearestPoint(
                gfx::Point(-100, -100)).id());
  EXPECT_EQ(primary_display.id(),
            Shell::GetScreen()->GetDisplayNearestWindow(NULL).id());

  // Switch primary and secondary by display ID.
  TestObserver observer;
  display_controller->SetPrimaryDisplayId(secondary_display.id());
  EXPECT_EQ(secondary_display.id(),
            Shell::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_EQ(primary_display.id(), ScreenUtil::GetSecondaryDisplay().id());
  EXPECT_LT(0, observer.CountAndReset());

  EXPECT_EQ(
      primary_root,
      display_controller->GetRootWindowForDisplayId(secondary_display.id()));
  EXPECT_EQ(
      secondary_root,
      display_controller->GetRootWindowForDisplayId(primary_display.id()));
  EXPECT_TRUE(primary_root->Contains(shelf_window));
  EXPECT_FALSE(secondary_root->Contains(shelf_window));

  const DisplayLayout& inverted_layout =
      display_manager->GetCurrentDisplayLayout();

  EXPECT_EQ("left, -50", inverted_layout.ToString());

  // Calling the same ID don't do anything.
  display_controller->SetPrimaryDisplayId(secondary_display.id());
  EXPECT_EQ(0, observer.CountAndReset());

  aura::WindowTracker tracker;
  tracker.Add(primary_root);
  tracker.Add(secondary_root);

  // Deleting 2nd display should move the primary to original primary display.
  UpdateDisplay("200x200");
  RunAllPendingInMessageLoop();  // RootWindow is deleted in a posted task.
  EXPECT_EQ(1, Shell::GetScreen()->GetNumDisplays());
  EXPECT_EQ(primary_display.id(), Shell::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_EQ(primary_display.id(),
            Shell::GetScreen()->GetDisplayNearestPoint(
                gfx::Point(-100, -100)).id());
  EXPECT_EQ(primary_display.id(),
            Shell::GetScreen()->GetDisplayNearestWindow(NULL).id());
  EXPECT_TRUE(tracker.Contains(primary_root));
  EXPECT_FALSE(tracker.Contains(secondary_root));
  EXPECT_TRUE(primary_root->Contains(shelf_window));

  // Adding 2nd display with the same ID.  The 2nd display should become primary
  // since secondary id is still stored as desirable_primary_id.
  std::vector<DisplayInfo> display_info_list;
  display_info_list.push_back(
      display_manager->GetDisplayInfo(primary_display.id()));
  display_info_list.push_back(
      display_manager->GetDisplayInfo(secondary_display.id()));
  display_manager->OnNativeDisplaysChanged(display_info_list);

  EXPECT_EQ(2, Shell::GetScreen()->GetNumDisplays());
  EXPECT_EQ(secondary_display.id(),
            Shell::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_EQ(primary_display.id(), ScreenUtil::GetSecondaryDisplay().id());
  EXPECT_EQ(
      primary_root,
      display_controller->GetRootWindowForDisplayId(secondary_display.id()));
  EXPECT_NE(
      primary_root,
      display_controller->GetRootWindowForDisplayId(primary_display.id()));
  EXPECT_TRUE(primary_root->Contains(shelf_window));

  // Deleting 2nd display and adding 2nd display with a different ID.  The 2nd
  // display shouldn't become primary.
  UpdateDisplay("200x200");
  DisplayInfo third_display_info(
      secondary_display.id() + 1, std::string(), false);
  third_display_info.SetBounds(secondary_display.bounds());
  ASSERT_NE(primary_display.id(), third_display_info.id());

  const DisplayInfo& primary_display_info =
      display_manager->GetDisplayInfo(primary_display.id());
  std::vector<DisplayInfo> display_info_list2;
  display_info_list2.push_back(primary_display_info);
  display_info_list2.push_back(third_display_info);
  display_manager->OnNativeDisplaysChanged(display_info_list2);
  EXPECT_EQ(2, Shell::GetScreen()->GetNumDisplays());
  EXPECT_EQ(primary_display.id(),
            Shell::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_EQ(third_display_info.id(), ScreenUtil::GetSecondaryDisplay().id());
  EXPECT_EQ(
      primary_root,
      display_controller->GetRootWindowForDisplayId(primary_display.id()));
  EXPECT_NE(
      primary_root,
      display_controller->GetRootWindowForDisplayId(third_display_info.id()));
  EXPECT_TRUE(primary_root->Contains(shelf_window));
}

TEST_F(DisplayControllerTest, OverscanInsets) {
  if (!SupportsMultipleDisplays())
    return;

  DisplayController* display_controller =
      Shell::GetInstance()->display_controller();
  TestEventHandler event_handler;
  Shell::GetInstance()->AddPreTargetHandler(&event_handler);

  UpdateDisplay("120x200,300x400*2");
  gfx::Display display1 = Shell::GetScreen()->GetPrimaryDisplay();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  display_controller->SetOverscanInsets(display1.id(),
                                        gfx::Insets(10, 15, 20, 25));
  EXPECT_EQ("0,0 80x170", root_windows[0]->bounds().ToString());
  EXPECT_EQ("150x200", root_windows[1]->bounds().size().ToString());
  EXPECT_EQ("80,0 150x200",
            ScreenUtil::GetSecondaryDisplay().bounds().ToString());

  ui::test::EventGenerator generator(root_windows[0]);
  generator.MoveMouseToInHost(20, 25);
  EXPECT_EQ("5,15", event_handler.GetLocationAndReset());

  display_controller->SetOverscanInsets(display1.id(), gfx::Insets());
  EXPECT_EQ("0,0 120x200", root_windows[0]->bounds().ToString());
  EXPECT_EQ("120,0 150x200",
            ScreenUtil::GetSecondaryDisplay().bounds().ToString());

  generator.MoveMouseToInHost(30, 20);
  EXPECT_EQ("30,20", event_handler.GetLocationAndReset());

  // Make sure the root window transformer uses correct scale
  // factor when swapping display. Test crbug.com/253690.
  UpdateDisplay("400x300*2,600x400/o");
  root_windows = Shell::GetAllRootWindows();
  gfx::Point point;
  Shell::GetAllRootWindows()[1]->GetHost()->
      GetRootTransform().TransformPoint(&point);
  EXPECT_EQ("15,10", point.ToString());

  display_controller->SwapPrimaryDisplay();
  point.SetPoint(0, 0);
  Shell::GetAllRootWindows()[1]->GetHost()->
      GetRootTransform().TransformPoint(&point);
  EXPECT_EQ("15,10", point.ToString());

  Shell::GetInstance()->RemovePreTargetHandler(&event_handler);
}

TEST_F(DisplayControllerTest, Rotate) {
  if (!SupportsMultipleDisplays())
    return;

  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  TestEventHandler event_handler;
  Shell::GetInstance()->AddPreTargetHandler(&event_handler);

  UpdateDisplay("120x200,300x400*2");
  gfx::Display display1 = Shell::GetScreen()->GetPrimaryDisplay();
  int64 display2_id = ScreenUtil::GetSecondaryDisplay().id();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ui::test::EventGenerator generator1(root_windows[0]);

  TestObserver observer;
  EXPECT_EQ("120x200", root_windows[0]->bounds().size().ToString());
  EXPECT_EQ("150x200", root_windows[1]->bounds().size().ToString());
  EXPECT_EQ("120,0 150x200",
            ScreenUtil::GetSecondaryDisplay().bounds().ToString());
  generator1.MoveMouseToInHost(50, 40);
  EXPECT_EQ("50,40", event_handler.GetLocationAndReset());
  EXPECT_EQ(gfx::Display::ROTATE_0, GetActiveDisplayRotation(display1.id()));
  EXPECT_EQ(gfx::Display::ROTATE_0, GetActiveDisplayRotation(display2_id));
  EXPECT_EQ(0, observer.GetRotationChangedCountAndReset());

  display_manager->SetDisplayRotation(display1.id(), gfx::Display::ROTATE_90,
                                      gfx::Display::ROTATION_SOURCE_ACTIVE);
  EXPECT_EQ("200x120", root_windows[0]->bounds().size().ToString());
  EXPECT_EQ("150x200", root_windows[1]->bounds().size().ToString());
  EXPECT_EQ("200,0 150x200",
            ScreenUtil::GetSecondaryDisplay().bounds().ToString());
  generator1.MoveMouseToInHost(50, 40);
  EXPECT_EQ("40,69", event_handler.GetLocationAndReset());
  EXPECT_EQ(gfx::Display::ROTATE_90, GetActiveDisplayRotation(display1.id()));
  EXPECT_EQ(gfx::Display::ROTATE_0, GetActiveDisplayRotation(display2_id));
  EXPECT_EQ(1, observer.GetRotationChangedCountAndReset());

  DisplayLayout display_layout(DisplayLayout::BOTTOM, 50);
  display_manager->SetLayoutForCurrentDisplays(display_layout);
  EXPECT_EQ("50,120 150x200",
            ScreenUtil::GetSecondaryDisplay().bounds().ToString());

  display_manager->SetDisplayRotation(display2_id, gfx::Display::ROTATE_270,
                                      gfx::Display::ROTATION_SOURCE_ACTIVE);
  EXPECT_EQ("200x120", root_windows[0]->bounds().size().ToString());
  EXPECT_EQ("200x150", root_windows[1]->bounds().size().ToString());
  EXPECT_EQ("50,120 200x150",
            ScreenUtil::GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ(gfx::Display::ROTATE_90, GetActiveDisplayRotation(display1.id()));
  EXPECT_EQ(gfx::Display::ROTATE_270, GetActiveDisplayRotation(display2_id));
  EXPECT_EQ(1, observer.GetRotationChangedCountAndReset());

#if !defined(OS_WIN)
  ui::test::EventGenerator generator2(root_windows[1]);
  generator2.MoveMouseToInHost(50, 40);
  EXPECT_EQ("179,25", event_handler.GetLocationAndReset());
  display_manager->SetDisplayRotation(display1.id(), gfx::Display::ROTATE_180,
                                      gfx::Display::ROTATION_SOURCE_ACTIVE);

  EXPECT_EQ("120x200", root_windows[0]->bounds().size().ToString());
  EXPECT_EQ("200x150", root_windows[1]->bounds().size().ToString());
  // Dislay must share at least 100, so the x's offset becomes 20.
  EXPECT_EQ("20,200 200x150",
            ScreenUtil::GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ(gfx::Display::ROTATE_180, GetActiveDisplayRotation(display1.id()));
  EXPECT_EQ(gfx::Display::ROTATE_270, GetActiveDisplayRotation(display2_id));
  EXPECT_EQ(1, observer.GetRotationChangedCountAndReset());

  generator1.MoveMouseToInHost(50, 40);
  EXPECT_EQ("69,159", event_handler.GetLocationAndReset());
#endif

  Shell::GetInstance()->RemovePreTargetHandler(&event_handler);
}

TEST_F(DisplayControllerTest, ScaleRootWindow) {
  if (!SupportsMultipleDisplays())
    return;

  TestEventHandler event_handler;
  Shell::GetInstance()->AddPreTargetHandler(&event_handler);

  UpdateDisplay("600x400*2@1.5,500x300");

  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  gfx::Display display1 = Shell::GetScreen()->GetPrimaryDisplay();
  test::DisplayManagerTestApi(display_manager)
      .SetInternalDisplayId(display1.id());

  gfx::Display display2 = ScreenUtil::GetSecondaryDisplay();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ("0,0 450x300", display1.bounds().ToString());
  EXPECT_EQ("0,0 450x300", root_windows[0]->bounds().ToString());
  EXPECT_EQ("450,0 500x300", display2.bounds().ToString());
  EXPECT_EQ(1.5f, GetStoredUIScale(display1.id()));
  EXPECT_EQ(1.0f, GetStoredUIScale(display2.id()));

  ui::test::EventGenerator generator(root_windows[0]);
  generator.MoveMouseToInHost(599, 200);
  EXPECT_EQ("449,150", event_handler.GetLocationAndReset());

  display_manager->SetDisplayUIScale(display1.id(), 1.25f);
  display1 = Shell::GetScreen()->GetPrimaryDisplay();
  display2 = ScreenUtil::GetSecondaryDisplay();
  EXPECT_EQ("0,0 375x250", display1.bounds().ToString());
  EXPECT_EQ("0,0 375x250", root_windows[0]->bounds().ToString());
  EXPECT_EQ("375,0 500x300", display2.bounds().ToString());
  EXPECT_EQ(1.25f, GetStoredUIScale(display1.id()));
  EXPECT_EQ(1.0f, GetStoredUIScale(display2.id()));

  Shell::GetInstance()->RemovePreTargetHandler(&event_handler);
}

TEST_F(DisplayControllerTest, TouchScale) {
  if (!SupportsMultipleDisplays())
    return;

  TestEventHandler event_handler;
  Shell::GetInstance()->AddPreTargetHandler(&event_handler);

  UpdateDisplay("200x200*2");
  gfx::Display display = Shell::GetScreen()->GetPrimaryDisplay();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  aura::Window* root_window = root_windows[0];
  ui::test::EventGenerator generator(root_window);

  generator.PressMoveAndReleaseTouchTo(50, 50);
  // Default test touches have radius_x/y = 1.0, with device scale
  // factor = 2, the scaled radius_x/y should be 0.5.
  EXPECT_EQ(0.5, event_handler.touch_radius_x());
  EXPECT_EQ(0.5, event_handler.touch_radius_y());

  generator.ScrollSequence(gfx::Point(0,0),
                           base::TimeDelta::FromMilliseconds(100),
                           10.0, 1.0, 5, 1);

  // ordinal_offset is invariant to the device scale factor.
  EXPECT_EQ(event_handler.scroll_x_offset(),
            event_handler.scroll_x_offset_ordinal());
  EXPECT_EQ(event_handler.scroll_y_offset(),
            event_handler.scroll_y_offset_ordinal());

  Shell::GetInstance()->RemovePreTargetHandler(&event_handler);
}

TEST_F(DisplayControllerTest, ConvertHostToRootCoords) {
  if (!SupportsMultipleDisplays())
    return;

  TestEventHandler event_handler;
  Shell::GetInstance()->AddPreTargetHandler(&event_handler);

  UpdateDisplay("600x400*2/r@1.5");

  gfx::Display display1 = Shell::GetScreen()->GetPrimaryDisplay();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ("0,0 300x450", display1.bounds().ToString());
  EXPECT_EQ("0,0 300x450", root_windows[0]->bounds().ToString());
  EXPECT_EQ(1.5f, GetStoredUIScale(display1.id()));

  ui::test::EventGenerator generator(root_windows[0]);
  generator.MoveMouseToInHost(0, 0);
  EXPECT_EQ("0,449", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(599, 0);
  EXPECT_EQ("0,0", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(599, 399);
  EXPECT_EQ("299,0", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(0, 399);
  EXPECT_EQ("299,449", event_handler.GetLocationAndReset());

  UpdateDisplay("600x400*2/u@1.5");
  display1 = Shell::GetScreen()->GetPrimaryDisplay();
  root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ("0,0 450x300", display1.bounds().ToString());
  EXPECT_EQ("0,0 450x300", root_windows[0]->bounds().ToString());
  EXPECT_EQ(1.5f, GetStoredUIScale(display1.id()));

  generator.MoveMouseToInHost(0, 0);
  EXPECT_EQ("449,299", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(599, 0);
  EXPECT_EQ("0,299", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(599, 399);
  EXPECT_EQ("0,0", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(0, 399);
  EXPECT_EQ("449,0", event_handler.GetLocationAndReset());

  UpdateDisplay("600x400*2/l@1.5");
  display1 = Shell::GetScreen()->GetPrimaryDisplay();
  root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ("0,0 300x450", display1.bounds().ToString());
  EXPECT_EQ("0,0 300x450", root_windows[0]->bounds().ToString());
  EXPECT_EQ(1.5f, GetStoredUIScale(display1.id()));

  generator.MoveMouseToInHost(0, 0);
  EXPECT_EQ("299,0", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(599, 0);
  EXPECT_EQ("299,449", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(599, 399);
  EXPECT_EQ("0,449", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(0, 399);
  EXPECT_EQ("0,0", event_handler.GetLocationAndReset());

  Shell::GetInstance()->RemovePreTargetHandler(&event_handler);
}

// Make sure that the compositor based mirroring can switch
// from/to dock mode.
TEST_F(DisplayControllerTest, DockToSingle) {
  if (!SupportsMultipleDisplays())
    return;

  DisplayManager* display_manager = Shell::GetInstance()->display_manager();

  const int64 internal_id = 1;

  const DisplayInfo internal_display_info =
      CreateDisplayInfo(internal_id, 0, gfx::Display::ROTATE_0);
  const DisplayInfo external_display_info =
      CreateDisplayInfo(2, 1, gfx::Display::ROTATE_90);

  std::vector<DisplayInfo> display_info_list;
  // Extended
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info);
  display_manager->OnNativeDisplaysChanged(display_info_list);
  const int64 internal_display_id =
      test::DisplayManagerTestApi(display_manager).
      SetFirstDisplayAsInternalDisplay();
  EXPECT_EQ(internal_id, internal_display_id);
  EXPECT_EQ(2U, display_manager->GetNumDisplays());

  // Dock mode.
  display_info_list.clear();
  display_info_list.push_back(external_display_info);
  display_manager->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager->GetNumDisplays());
  EXPECT_FALSE(Shell::GetPrimaryRootWindow()->GetHost()->
               GetRootTransform().IsIdentityOrIntegerTranslation());

  // Switch to single mode and make sure the transform is the one
  // for the internal display.
  display_info_list.clear();
  display_info_list.push_back(internal_display_info);
  display_manager->OnNativeDisplaysChanged(display_info_list);
  EXPECT_TRUE(Shell::GetPrimaryRootWindow()->GetHost()->
              GetRootTransform().IsIdentityOrIntegerTranslation());
}

// Tests if switching two displays at the same time while the primary display
// is swapped should not cause a crash. (crbug.com/426292)
TEST_F(DisplayControllerTest, ReplaceSwappedPrimary) {
  if (!SupportsMultipleDisplays())
    return;
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();

  const DisplayInfo first_display_info =
      CreateDisplayInfo(10, 0, gfx::Display::ROTATE_0);
  const DisplayInfo second_display_info =
      CreateDisplayInfo(11, 1, gfx::Display::ROTATE_0);

  std::vector<DisplayInfo> display_info_list;
  // Extended
  display_info_list.push_back(first_display_info);
  display_info_list.push_back(second_display_info);
  display_manager->OnNativeDisplaysChanged(display_info_list);

  Shell::GetInstance()->display_controller()->SwapPrimaryDisplay();

  EXPECT_EQ(11, Shell::GetScreen()->GetPrimaryDisplay().id());

  display_info_list.clear();
  const DisplayInfo new_first_display_info =
      CreateDisplayInfo(20, 0, gfx::Display::ROTATE_0);
  const DisplayInfo new_second_display_info =
      CreateDisplayInfo(21, 1, gfx::Display::ROTATE_0);
  display_info_list.push_back(new_first_display_info);
  display_info_list.push_back(new_second_display_info);
  display_manager->OnNativeDisplaysChanged(display_info_list);

  EXPECT_EQ(20, Shell::GetScreen()->GetPrimaryDisplay().id());
}

TEST_F(DisplayControllerTest, UpdateMouseLocationAfterDisplayChange) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("200x200,300x300");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  aura::Env* env = aura::Env::GetInstance();

  ui::test::EventGenerator generator(root_windows[0]);

  // Set the initial position.
  generator.MoveMouseToInHost(350, 150);
  EXPECT_EQ("350,150", env->last_mouse_location().ToString());

  // A mouse pointer will stay in the 2nd display.
  UpdateDisplay("300x300,200x200");
  EXPECT_EQ("450,50", env->last_mouse_location().ToString());

  // A mouse pointer will be outside of displays and move to the
  // center of 2nd display.
  UpdateDisplay("300x300,100x100");
  EXPECT_EQ("350,50", env->last_mouse_location().ToString());

  // 2nd display was disconnected, and the cursor is
  // now in the 1st display.
  UpdateDisplay("400x400");
  EXPECT_EQ("50,350", env->last_mouse_location().ToString());

  // 1st display's resolution has changed, and the mouse pointer is
  // now outside. Move the mouse pointer to the center of 1st display.
  UpdateDisplay("300x300");
  EXPECT_EQ("150,150", env->last_mouse_location().ToString());

  // Move the mouse pointer to the bottom of 1st display.
  generator.MoveMouseToInHost(150, 290);
  EXPECT_EQ("150,290", env->last_mouse_location().ToString());

  // The mouse pointer is now on 2nd display.
  UpdateDisplay("300x280,200x200");
  EXPECT_EQ("450,10", env->last_mouse_location().ToString());
}

TEST_F(DisplayControllerTest, UpdateMouseLocationAfterDisplayChange_2ndOnLeft) {
  if (!SupportsMultipleDisplays())
    return;

  // Set the 2nd display on the left.
  DisplayLayoutStore* layout_store =
      Shell::GetInstance()->display_manager()->layout_store();
  DisplayLayout layout = layout_store->default_display_layout();
  layout.position = DisplayLayout::LEFT;
  layout_store->SetDefaultDisplayLayout(layout);

  UpdateDisplay("200x200,300x300");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  EXPECT_EQ("-300,0 300x300",
            ScreenUtil::GetSecondaryDisplay().bounds().ToString());

  aura::Env* env = aura::Env::GetInstance();

  // Set the initial position.
  root_windows[0]->MoveCursorTo(gfx::Point(-150, 250));
  EXPECT_EQ("-150,250", env->last_mouse_location().ToString());

  // A mouse pointer will stay in 2nd display.
  UpdateDisplay("300x300,200x300");
  EXPECT_EQ("-50,150", env->last_mouse_location().ToString());

  // A mouse pointer will be outside of displays and move to the
  // center of 2nd display.
  UpdateDisplay("300x300,200x100");
  EXPECT_EQ("-100,50", env->last_mouse_location().ToString());

  // 2nd display was disconnected. Mouse pointer should move to
  // 1st display.
  UpdateDisplay("300x300");
  EXPECT_EQ("150,150", env->last_mouse_location().ToString());
}

// Test that the cursor swaps displays and that its scale factor and rotation
// are updated when the primary display is swapped.
TEST_F(DisplayControllerTest,
       UpdateMouseLocationAfterDisplayChange_SwapPrimary) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("200x200,200x200*2/r");

  aura::Env* env = aura::Env::GetInstance();
  Shell* shell = Shell::GetInstance();
  DisplayController* display_controller = shell->display_controller();
  test::CursorManagerTestApi test_api(shell->cursor_manager());

  display_controller->GetPrimaryRootWindow()->MoveCursorTo(gfx::Point(20, 50));

  EXPECT_EQ("20,50", env->last_mouse_location().ToString());
  EXPECT_EQ(1.0f, test_api.GetCurrentCursor().device_scale_factor());
  EXPECT_EQ(gfx::Display::ROTATE_0, test_api.GetCurrentCursorRotation());

  display_controller->SwapPrimaryDisplay();

  EXPECT_EQ("20,50", env->last_mouse_location().ToString());
  EXPECT_EQ(2.0f, test_api.GetCurrentCursor().device_scale_factor());
  EXPECT_EQ(gfx::Display::ROTATE_90, test_api.GetCurrentCursorRotation());
}

// Test that the cursor moves to the other display and that its scale factor
// and rotation are updated when the primary display is disconnected.
TEST_F(DisplayControllerTest,
       UpdateMouseLocationAfterDisplayChange_PrimaryDisconnected) {
  if (!SupportsMultipleDisplays())
    return;

  aura::Env* env = aura::Env::GetInstance();
  Shell* shell = Shell::GetInstance();
  DisplayController* display_controller = shell->display_controller();
  test::CursorManagerTestApi test_api(shell->cursor_manager());

  UpdateDisplay("300x300*2/r,200x200");
  // Swap the primary display to make it possible to remove the primary display
  // via UpdateDisplay().
  display_controller->SwapPrimaryDisplay();
  int primary_display_id = display_controller->GetPrimaryDisplayId();

  display_controller->GetPrimaryRootWindow()->MoveCursorTo(gfx::Point(20, 50));

  EXPECT_EQ("20,50", env->last_mouse_location().ToString());
  EXPECT_EQ(1.0f, test_api.GetCurrentCursor().device_scale_factor());
  EXPECT_EQ(gfx::Display::ROTATE_0, test_api.GetCurrentCursorRotation());

  UpdateDisplay("300x300*2/r");
  ASSERT_NE(primary_display_id, display_controller->GetPrimaryDisplayId());

  // Cursor should be centered on the remaining display.
  EXPECT_EQ("75,75", env->last_mouse_location().ToString());
  EXPECT_EQ(2.0f, test_api.GetCurrentCursor().device_scale_factor());
  EXPECT_EQ(gfx::Display::ROTATE_90, test_api.GetCurrentCursorRotation());
}

// GetRootWindowForDisplayId() for removed gfx::Display during
// OnDisplayRemoved() should not cause crash. See http://crbug.com/415222
TEST_F(DisplayControllerTest,
       GetRootWindowForDisplayIdDuringDisplayDisconnection) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("300x300,200x200");
  aura::Window* root2 =
      Shell::GetInstance()->display_controller()->GetRootWindowForDisplayId(
          ScreenUtil::GetSecondaryDisplay().id());
  views::Widget* widget = views::Widget::CreateWindowWithContextAndBounds(
      nullptr, root2, gfx::Rect(350, 0, 100, 100));
  views::View* view = new views::View();
  widget->GetContentsView()->AddChildView(view);
  view->SetBounds(0, 0, 100, 100);
  widget->Show();

  TestMouseWatcherListener listener;
  views::MouseWatcher watcher(
      new views::MouseWatcherViewHost(view, gfx::Insets()), &listener);
  watcher.Start();

  ui::test::EventGenerator event_generator(
      widget->GetNativeWindow()->GetRootWindow());
  event_generator.MoveMouseToCenterOf(widget->GetNativeWindow());

  UpdateDisplay("300x300");
  watcher.Stop();

  widget->CloseNow();
}

}  // namespace ash
