// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/screen_util.h"

#include "ash/common/wm/wm_screen_util.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_window.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_md_test_base.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/wm/window_util.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace test {

using ScreenUtilTest = AshMDTestBase;

INSTANTIATE_TEST_CASE_P(
    /* prefix intentionally left blank due to only one parameterization */,
    ScreenUtilTest,
    testing::Values(MaterialDesignController::NON_MATERIAL,
                    MaterialDesignController::MATERIAL_NORMAL,
                    MaterialDesignController::MATERIAL_EXPERIMENTAL));

TEST_P(ScreenUtilTest, Bounds) {
  if (!SupportsMultipleDisplays())
    return;
  const int height_offset = GetMdMaximizedWindowHeightOffset();

  UpdateDisplay("600x600,500x500");
  views::Widget* primary = views::Widget::CreateWindowWithContextAndBounds(
      NULL, CurrentContext(), gfx::Rect(10, 10, 100, 100));
  primary->Show();
  views::Widget* secondary = views::Widget::CreateWindowWithContextAndBounds(
      NULL, CurrentContext(), gfx::Rect(610, 10, 100, 100));
  secondary->Show();

  // Maximized bounds. By default the shelf is 47px tall (ash::kShelfSize).
  EXPECT_EQ(
      gfx::Rect(0, 0, 600, 553 + height_offset).ToString(),
      ScreenUtil::GetMaximizedWindowBoundsInParent(primary->GetNativeView())
          .ToString());
  EXPECT_EQ(
      gfx::Rect(0, 0, 500, 453 + height_offset).ToString(),
      ScreenUtil::GetMaximizedWindowBoundsInParent(secondary->GetNativeView())
          .ToString());

  // Display bounds
  EXPECT_EQ("0,0 600x600",
            ScreenUtil::GetDisplayBoundsInParent(primary->GetNativeView())
                .ToString());
  EXPECT_EQ("0,0 500x500",
            ScreenUtil::GetDisplayBoundsInParent(secondary->GetNativeView())
                .ToString());

  // Work area bounds
  EXPECT_EQ(
      gfx::Rect(0, 0, 600, 553 + height_offset).ToString(),
      ScreenUtil::GetDisplayWorkAreaBoundsInParent(primary->GetNativeView())
          .ToString());
  EXPECT_EQ(
      gfx::Rect(0, 0, 500, 453 + height_offset).ToString(),
      ScreenUtil::GetDisplayWorkAreaBoundsInParent(secondary->GetNativeView())
          .ToString());
}

// Test verifies a stable handling of secondary screen widget changes
// (crbug.com/226132).
TEST_P(ScreenUtilTest, StabilityTest) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("600x600,500x500");
  views::Widget* secondary = views::Widget::CreateWindowWithContextAndBounds(
      NULL, CurrentContext(), gfx::Rect(610, 10, 100, 100));
  EXPECT_EQ(Shell::GetAllRootWindows()[1],
            secondary->GetNativeView()->GetRootWindow());
  secondary->Show();
  secondary->Maximize();
  secondary->Show();
  secondary->SetFullscreen(true);
  secondary->Hide();
  secondary->Close();
}

TEST_P(ScreenUtilTest, ConvertRect) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("600x600,500x500");

  views::Widget* primary = views::Widget::CreateWindowWithContextAndBounds(
      NULL, CurrentContext(), gfx::Rect(10, 10, 100, 100));
  primary->Show();
  views::Widget* secondary = views::Widget::CreateWindowWithContextAndBounds(
      NULL, CurrentContext(), gfx::Rect(610, 10, 100, 100));
  secondary->Show();

  EXPECT_EQ("0,0 100x100",
            ScreenUtil::ConvertRectFromScreen(primary->GetNativeView(),
                                              gfx::Rect(10, 10, 100, 100))
                .ToString());
  EXPECT_EQ("10,10 100x100",
            ScreenUtil::ConvertRectFromScreen(secondary->GetNativeView(),
                                              gfx::Rect(620, 20, 100, 100))
                .ToString());

  EXPECT_EQ("40,40 100x100",
            ScreenUtil::ConvertRectToScreen(primary->GetNativeView(),
                                            gfx::Rect(30, 30, 100, 100))
                .ToString());
  EXPECT_EQ("650,50 100x100",
            ScreenUtil::ConvertRectToScreen(secondary->GetNativeView(),
                                            gfx::Rect(40, 40, 100, 100))
                .ToString());
}

TEST_P(ScreenUtilTest, ShelfDisplayBoundsInUnifiedDesktop) {
  if (!SupportsMultipleDisplays())
    return;
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();

  display_manager->SetUnifiedDesktopEnabled(true);

  views::Widget* widget = views::Widget::CreateWindowWithContextAndBounds(
      NULL, CurrentContext(), gfx::Rect(10, 10, 100, 100));
  WmWindow* window = WmLookup::Get()->GetWindowForWidget(widget);

  UpdateDisplay("500x400");
  EXPECT_EQ("0,0 500x400", wm::GetDisplayBoundsWithShelf(window).ToString());

  UpdateDisplay("500x400,600x400");
  EXPECT_EQ("0,0 500x400", wm::GetDisplayBoundsWithShelf(window).ToString());

  // Move to the 2nd physical display. Shelf's display still should be
  // the first.
  widget->SetBounds(gfx::Rect(800, 0, 100, 100));
  ASSERT_EQ("800,0 100x100", widget->GetWindowBoundsInScreen().ToString());

  EXPECT_EQ("0,0 500x400", wm::GetDisplayBoundsWithShelf(window).ToString());

  UpdateDisplay("600x500");
  EXPECT_EQ("0,0 600x500", wm::GetDisplayBoundsWithShelf(window).ToString());
}

}  // namespace test
}  // namespace ash
