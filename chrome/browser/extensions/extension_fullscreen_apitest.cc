// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser_window.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       ExtensionFullscreenAccessFail) {
  // Test that fullscreen cannot be accessed from an extension without
  // permission.
  ASSERT_TRUE(RunPlatformAppTest("fullscreen/no_permission")) << message_;
}

#if defined(OS_MACOSX)
// Fails on MAC: http://crbug.com/480370
#define MAYBE_ExtensionFullscreenAccessPass \
    DISABLED_ExtensionFullscreenAccessPass
#else
#define MAYBE_ExtensionFullscreenAccessPass ExtensionFullscreenAccessPass
#endif  // defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_ExtensionFullscreenAccessPass) {
  // Test that fullscreen can be accessed from an extension with permission.
  ASSERT_TRUE(RunPlatformAppTest("fullscreen/has_permission")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       FocusWindowDoesNotExitFullscreen) {
  browser()->window()->EnterFullscreen(
      GURL(), EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION,
      false);
  bool is_fullscreen = browser()->window()->IsFullscreen();
  ASSERT_TRUE(RunExtensionTest("window_update/focus")) << message_;
  ASSERT_EQ(is_fullscreen, browser()->window()->IsFullscreen());
}

// Fails flakily: http://crbug.com/308041
IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       DISABLED_UpdateWindowSizeExitsFullscreen) {
  browser()->window()->EnterFullscreen(
      GURL(), EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION,
      false);
  ASSERT_TRUE(RunExtensionTest("window_update/sizing")) << message_;
  ASSERT_FALSE(browser()->window()->IsFullscreen());
}

#if defined(OS_MACOSX)
// Fails on MAC: http://crbug.com/480370
#define MAYBE_DisplayModeWindowIsInFullscreen DISABLED_DisplayModeWindowIsInFullscreen
#else
#define MAYBE_DisplayModeWindowIsInFullscreen DisplayModeWindowIsInFullscreen
#endif  // defined(OS_MACOSX)

IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       MAYBE_DisplayModeWindowIsInFullscreen) {
  ASSERT_TRUE(RunPlatformAppTest("fullscreen/mq_display_mode")) << message_;
}
