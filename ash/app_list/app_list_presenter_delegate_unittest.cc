// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/common/shell_window_ids.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/presenter/app_list_presenter_impl.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {
const int kMinimalCenteredAppListMargin = 10;
}

// The parameter is true to test the centered app list, false for normal.
// (The test name ends in "/0" for normal, "/1" for centered.)
class AppListPresenterDelegateTest
    : public test::AshTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  AppListPresenterDelegateTest();
  virtual ~AppListPresenterDelegateTest();

  // testing::Test:
  void SetUp() override;

  app_list::AppListPresenterImpl* GetAppListPresenter();
  bool IsCentered() const;
};

AppListPresenterDelegateTest::AppListPresenterDelegateTest() {}

AppListPresenterDelegateTest::~AppListPresenterDelegateTest() {}

void AppListPresenterDelegateTest::SetUp() {
  AshTestBase::SetUp();
  if (IsCentered()) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(app_list::switches::kEnableCenteredAppList);
  }

  // Make the display big enough to hold the experimental app list.
  UpdateDisplay("1024x768");
}

app_list::AppListPresenterImpl*
AppListPresenterDelegateTest::GetAppListPresenter() {
  return ash_test_helper()->test_shell_delegate()->app_list_presenter();
}

bool AppListPresenterDelegateTest::IsCentered() const {
  return GetParam();
}

// Tests that app launcher hides when focus moves to a normal window.
TEST_P(AppListPresenterDelegateTest, HideOnFocusOut) {
  WmShell::Get()->ShowAppList();
  EXPECT_TRUE(WmShell::Get()->GetAppListTargetVisibility());

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window.get());

  EXPECT_FALSE(WmShell::Get()->GetAppListTargetVisibility());
}

// Tests that app launcher remains visible when focus is moved to a different
// window in kShellWindowId_AppListContainer.
TEST_P(AppListPresenterDelegateTest,
       RemainVisibleWhenFocusingToApplistContainer) {
  WmShell::Get()->ShowAppList();
  EXPECT_TRUE(WmShell::Get()->GetAppListTargetVisibility());

  aura::Window* applist_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AppListContainer);
  std::unique_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(0, applist_container));
  wm::ActivateWindow(window.get());

  EXPECT_TRUE(WmShell::Get()->GetAppListTargetVisibility());
}

// Tests that clicking outside the app-list bubble closes it.
TEST_P(AppListPresenterDelegateTest, ClickOutsideBubbleClosesBubble) {
  WmShell::Get()->ShowAppList();
  aura::Window* app_window = GetAppListPresenter()->GetWindow();
  ASSERT_TRUE(app_window);
  ui::test::EventGenerator& generator = GetEventGenerator();
  // Click on the bubble itself. The bubble should remain visible.
  generator.MoveMouseToCenterOf(app_window);
  generator.ClickLeftButton();
  EXPECT_TRUE(WmShell::Get()->GetAppListTargetVisibility());

  // Click outside the bubble. This should close it.
  gfx::Rect app_window_bounds = app_window->GetBoundsInRootWindow();
  gfx::Point point_outside =
      gfx::Point(app_window_bounds.right(), app_window_bounds.y()) +
      gfx::Vector2d(10, 0);
  generator.MoveMouseToInHost(point_outside);
  generator.ClickLeftButton();
  EXPECT_FALSE(WmShell::Get()->GetAppListTargetVisibility());
}

// Tests that clicking outside the app-list bubble closes it.
TEST_P(AppListPresenterDelegateTest, TapOutsideBubbleClosesBubble) {
  WmShell::Get()->ShowAppList();

  aura::Window* app_window = GetAppListPresenter()->GetWindow();
  ASSERT_TRUE(app_window);
  gfx::Rect app_window_bounds = app_window->GetBoundsInRootWindow();

  ui::test::EventGenerator& generator = GetEventGenerator();
  // Click on the bubble itself. The bubble should remain visible.
  generator.GestureTapAt(app_window_bounds.CenterPoint());
  EXPECT_TRUE(WmShell::Get()->GetAppListTargetVisibility());

  // Click outside the bubble. This should close it.
  gfx::Point point_outside =
      gfx::Point(app_window_bounds.right(), app_window_bounds.y()) +
      gfx::Vector2d(10, 0);
  generator.GestureTapAt(point_outside);
  EXPECT_FALSE(WmShell::Get()->GetAppListTargetVisibility());
}

// Tests opening the app launcher on a non-primary display, then deleting the
// display.
TEST_P(AppListPresenterDelegateTest, NonPrimaryDisplay) {
  if (!SupportsMultipleDisplays())
    return;

  // Set up a screen with two displays (horizontally adjacent).
  UpdateDisplay("1024x768,1024x768");

  std::vector<WmWindow*> root_windows = WmShell::Get()->GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  WmWindow* secondary_root = root_windows[1];
  EXPECT_EQ("1024,0 1024x768", secondary_root->GetBoundsInScreen().ToString());

  WmShell::Get()->delegate()->GetAppListPresenter()->Show(
      secondary_root->GetDisplayNearestWindow().id());
  EXPECT_TRUE(WmShell::Get()->GetAppListTargetVisibility());

  // Remove the secondary display. Shouldn't crash (http://crbug.com/368990).
  UpdateDisplay("1024x768");

  // Updating the displays should close the app list.
  EXPECT_FALSE(WmShell::Get()->GetAppListTargetVisibility());
}

// Tests opening the app launcher on a tiny display that is too small to contain
// it.
TEST_P(AppListPresenterDelegateTest, TinyDisplay) {
  // Don't test this for the non-centered app list case; it isn't designed for
  // small displays. The most common case of a small display --- when the
  // virtual keyboard is open --- switches into the centered app list mode, so
  // we just want to run this test in that case.
  if (!IsCentered())
    return;

  // UpdateDisplay is not supported in this case, so just skip the test.
  if (!SupportsHostWindowResize())
    return;

  // Set up a screen with a tiny display (height smaller than the app list).
  UpdateDisplay("400x300");

  WmShell::Get()->ShowAppList();
  EXPECT_TRUE(WmShell::Get()->GetAppListTargetVisibility());

  // The top of the app list should be on-screen (even if the bottom is not).
  // We need to manually calculate the Y coordinate of the top of the app list
  // from the anchor (center) and height. There isn't a bounds rect that gives
  // the actual app list position (the widget bounds include the bubble border
  // which is much bigger than the actual app list size).
  app_list::AppListView* app_list = GetAppListPresenter()->GetView();
  int app_list_view_top =
      app_list->anchor_rect().y() - app_list->bounds().height() / 2;
  EXPECT_GE(app_list_view_top, kMinimalCenteredAppListMargin);
}

INSTANTIATE_TEST_CASE_P(AppListPresenterDelegateTestInstance,
                        AppListPresenterDelegateTest,
                        ::testing::Bool());

}  // namespace ash
