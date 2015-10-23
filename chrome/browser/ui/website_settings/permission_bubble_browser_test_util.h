// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_BROWSER_TEST_UTIL_H_
#define CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_BROWSER_TEST_UTIL_H_

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/website_settings/permission_bubble_view.h"

namespace base {
class CommandLine;
}
class PermissionBubbleRequest;
class Browser;

class TestPermissionBubbleViewDelegate : public PermissionBubbleView::Delegate {
 public:
  TestPermissionBubbleViewDelegate();

  void ToggleAccept(int, bool) override {}
  void Accept() override {}
  void Deny() override {}
  void Closing() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestPermissionBubbleViewDelegate);
};

// Use this class to test on a default window or an app window. Inheriting from
// ExtensionBrowserTest allows us to easily load and launch apps, and doesn't
// really add any extra work.
class PermissionBubbleBrowserTest : public ExtensionBrowserTest {
 public:
  PermissionBubbleBrowserTest();
  ~PermissionBubbleBrowserTest() override;

  void SetUpOnMainThread() override;

  // Opens an app window, and returns the associated browser.
  Browser* OpenExtensionAppWindow();

  std::vector<PermissionBubbleRequest*> requests() { return requests_.get(); }
  std::vector<bool> accept_states() { return accept_states_; }
  PermissionBubbleView::Delegate* test_delegate() { return &test_delegate_; }

 private:
  TestPermissionBubbleViewDelegate test_delegate_;
  ScopedVector<PermissionBubbleRequest> requests_;
  std::vector<bool> accept_states_;

  DISALLOW_COPY_AND_ASSIGN(PermissionBubbleBrowserTest);
};

// Use this class to test on a kiosk window.
class PermissionBubbleKioskBrowserTest : public PermissionBubbleBrowserTest {
 public:
  PermissionBubbleKioskBrowserTest();
  ~PermissionBubbleKioskBrowserTest() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;

  DISALLOW_COPY_AND_ASSIGN(PermissionBubbleKioskBrowserTest);
};

#endif  // CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_BROWSER_TEST_UTIL_H_
