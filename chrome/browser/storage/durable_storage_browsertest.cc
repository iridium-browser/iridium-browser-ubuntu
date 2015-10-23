// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/dom_operation_notification_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class DurableStorageBrowserTest : public InProcessBrowserTest {
 public:
  DurableStorageBrowserTest() = default;
  ~DurableStorageBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine*) override;
  void SetUpOnMainThread() override;

 protected:
  content::RenderFrameHost* GetRenderFrameHost() {
    return browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  }
  GURL url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DurableStorageBrowserTest);
};

void DurableStorageBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitch(
              switches::kEnableExperimentalWebPlatformFeatures);
}

void DurableStorageBrowserTest::SetUpOnMainThread() {
  if (embedded_test_server()->Started())
    return;
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  url_ = embedded_test_server()->GetURL("/durable/durability-permissions.html");
}

IN_PROC_BROWSER_TEST_F(DurableStorageBrowserTest, DenyString) {
  ui_test_utils::NavigateToURL(browser(), url_);
  PermissionBubbleManager::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents())
      ->set_auto_response_for_test(PermissionBubbleManager::DENY_ALL);
  bool default_box_is_persistent;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetRenderFrameHost(), "requestPermission()", &default_box_is_persistent));
  EXPECT_FALSE(default_box_is_persistent);
  std::string permission_string;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      GetRenderFrameHost(), "checkPermission()", &permission_string));
  EXPECT_EQ("denied", permission_string);
}

IN_PROC_BROWSER_TEST_F(DurableStorageBrowserTest, FirstTabSeesResult) {
  ui_test_utils::NavigateToURL(browser(), url_);
  std::string permission_string;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      GetRenderFrameHost(), "checkPermission()", &permission_string));
  EXPECT_EQ("default", permission_string);

  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(), url_);
  PermissionBubbleManager::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents())
      ->set_auto_response_for_test(PermissionBubbleManager::ACCEPT_ALL);
  bool default_box_is_persistent = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetRenderFrameHost(), "requestPermission()", &default_box_is_persistent));
  EXPECT_TRUE(default_box_is_persistent);

  browser()->tab_strip_model()->ActivateTabAt(0, false);
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      GetRenderFrameHost(), "checkPermission()", &permission_string));
  EXPECT_EQ("granted", permission_string);
}
