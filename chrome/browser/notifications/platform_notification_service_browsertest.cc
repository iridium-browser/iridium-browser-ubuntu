// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/desktop_notification_profile_util.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/notifications/notification_permission_context_factory.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/filename_util.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

// -----------------------------------------------------------------------------

// Dimensions of the icon.png resource in the notification test data directory.
const int kIconWidth = 100;
const int kIconHeight = 100;

const int kNotificationVibrationPattern[] = { 100, 200, 300 };

class PlatformNotificationServiceBrowserTest : public InProcessBrowserTest {
 public:
  PlatformNotificationServiceBrowserTest();
  ~PlatformNotificationServiceBrowserTest() override {}

  // InProcessBrowserTest overrides.
  void SetUp() override;
  void SetUpOnMainThread() override;
  void TearDown() override;

 protected:
  // Returns the Platform Notification Service these unit tests are for.
  PlatformNotificationServiceImpl* service() const {
    return PlatformNotificationServiceImpl::GetInstance();
  }

  // Grants permission to display Web Notifications for origin of the test
  // page that's being used in this browser test.
  void GrantNotificationPermissionForTest() const;

  bool RequestAndAcceptPermission();
  bool RequestAndDenyPermission();

  // Returns the UI Manager on which notifications will be displayed.
  StubNotificationUIManager* ui_manager() const { return ui_manager_.get(); }

  const base::FilePath& server_root() const { return server_root_; }

  // Navigates the browser to the test page indicated by |path|.
  void NavigateToTestPage(const std::string& path) const;

  // Executes |script| and stores the result as a string in |result|. A boolean
  // will be returned, indicating whether the script was executed successfully.
  bool RunScript(const std::string& script, std::string* result) const;

  net::HostPortPair ServerHostPort() const;
  GURL TestPageUrl() const;

 private:
  std::string RequestAndRespondToPermission(
      PermissionBubbleManager::AutoResponseType bubble_response);

  content::WebContents* GetActiveWebContents(Browser* browser) {
    return browser->tab_strip_model()->GetActiveWebContents();
  }

  const base::FilePath server_root_;
  const std::string test_page_url_;
  scoped_ptr<StubNotificationUIManager> ui_manager_;
  scoped_ptr<net::SpawnedTestServer> https_server_;
};

// -----------------------------------------------------------------------------

namespace {
const char kTestFileName[] = "notifications/platform_notification_service.html";
}

PlatformNotificationServiceBrowserTest::PlatformNotificationServiceBrowserTest()
    : server_root_(FILE_PATH_LITERAL("chrome/test/data")),
      // The test server has a base directory that doesn't exist in the
      // filesystem.
      test_page_url_(std::string("files/") + kTestFileName) {
}

void PlatformNotificationServiceBrowserTest::SetUp() {
  ui_manager_.reset(new StubNotificationUIManager);
  https_server_.reset(new net::SpawnedTestServer(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::BaseTestServer::SSLOptions(net::BaseTestServer::SSLOptions::CERT_OK),
      server_root_));
  ASSERT_TRUE(https_server_->Start());

  service()->SetNotificationUIManagerForTesting(ui_manager_.get());

  InProcessBrowserTest::SetUp();
}

void PlatformNotificationServiceBrowserTest::SetUpOnMainThread() {
  NavigateToTestPage(test_page_url_);

  InProcessBrowserTest::SetUpOnMainThread();
}

void PlatformNotificationServiceBrowserTest::TearDown() {
  service()->SetNotificationUIManagerForTesting(nullptr);
}

void PlatformNotificationServiceBrowserTest::
    GrantNotificationPermissionForTest() const {
  GURL origin = TestPageUrl().GetOrigin();

  DesktopNotificationProfileUtil::GrantPermission(browser()->profile(), origin);
  ASSERT_EQ(CONTENT_SETTING_ALLOW,
            DesktopNotificationProfileUtil::GetContentSetting(
                browser()->profile(), origin));
}

void PlatformNotificationServiceBrowserTest::NavigateToTestPage(
    const std::string& path) const {
  ui_test_utils::NavigateToURL(browser(), https_server_->GetURL(path));
}

bool PlatformNotificationServiceBrowserTest::RunScript(
    const std::string& script, std::string* result) const {
  return content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      script,
      result);
}

net::HostPortPair PlatformNotificationServiceBrowserTest::ServerHostPort()
    const {
  return https_server_->host_port_pair();
}

GURL PlatformNotificationServiceBrowserTest::TestPageUrl() const {
  return https_server_->GetURL(test_page_url_);
}

std::string
PlatformNotificationServiceBrowserTest::RequestAndRespondToPermission(
    PermissionBubbleManager::AutoResponseType bubble_response) {
  std::string result;
  content::WebContents* web_contents = GetActiveWebContents(browser());
  PermissionBubbleManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(bubble_response);
  EXPECT_TRUE(RunScript("RequestPermission();", &result));
  return result;
}

bool PlatformNotificationServiceBrowserTest::RequestAndAcceptPermission() {
  std::string result =
      RequestAndRespondToPermission(PermissionBubbleManager::ACCEPT_ALL);
  return "granted" == result;
}

bool PlatformNotificationServiceBrowserTest::RequestAndDenyPermission() {
  std::string result =
      RequestAndRespondToPermission(PermissionBubbleManager::DENY_ALL);
  return "denied" == result;
}

// -----------------------------------------------------------------------------

// TODO(peter): Move PlatformNotificationService-related tests over from
// notification_browsertest.cc to this file.

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       DisplayPersistentNotificationWithoutPermission) {
  RequestAndDenyPermission();

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotification()", &script_result));
  EXPECT_EQ(
      "TypeError: No notification permission has been granted for this origin.",
      script_result);

  ASSERT_EQ(0u, ui_manager()->GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       DisplayPersistentNotificationWithPermission) {
  RequestAndAcceptPermission();

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotification('action_none')",
       &script_result));
  EXPECT_EQ("ok", script_result);

  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());

  const Notification& notification = ui_manager()->GetNotificationAt(0);
  notification.delegate()->Click();

  ASSERT_TRUE(RunScript("GetMessageFromWorker()", &script_result));
  EXPECT_EQ("action_none", script_result);

  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       WebNotificationOptionsReflection) {
  ASSERT_NO_FATAL_FAILURE(GrantNotificationPermissionForTest());

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentAllOptionsNotification()",
                        &script_result));
  EXPECT_EQ("ok", script_result);

  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());

  // We don't use or check the notification's direction and language.
  const Notification& notification = ui_manager()->GetNotificationAt(0);
  EXPECT_EQ("Title", base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ("Contents", base::UTF16ToUTF8(notification.message()));
  EXPECT_EQ("replace-id", notification.tag());
  EXPECT_FALSE(notification.icon().IsEmpty());
  EXPECT_TRUE(notification.silent());

  EXPECT_EQ(kIconWidth, notification.icon().Width());
  EXPECT_EQ(kIconHeight, notification.icon().Height());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       WebNotificationOptionsVibrationPattern) {
  ASSERT_NO_FATAL_FAILURE(GrantNotificationPermissionForTest());

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotificationVibrate()",
                        &script_result));
  EXPECT_EQ("ok", script_result);

  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());

  const Notification& notification = ui_manager()->GetNotificationAt(0);
  EXPECT_EQ("Title", base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ("Contents", base::UTF16ToUTF8(notification.message()));

  EXPECT_THAT(notification.vibration_pattern(),
      testing::ElementsAreArray(kNotificationVibrationPattern));
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       CloseDisplayedPersistentNotification) {
  ASSERT_NO_FATAL_FAILURE(GrantNotificationPermissionForTest());

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotification('action_close')",
      &script_result));
  EXPECT_EQ("ok", script_result);

  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());

  const Notification& notification = ui_manager()->GetNotificationAt(0);
  notification.delegate()->Click();

  ASSERT_TRUE(RunScript("GetMessageFromWorker()", &script_result));
  EXPECT_EQ("action_close", script_result);

  ASSERT_EQ(0u, ui_manager()->GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       TestDisplayOriginContextMessage) {
  RequestAndAcceptPermission();

  // Creates a simple notification.
  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotification()", &script_result));

  net::HostPortPair host_port = ServerHostPort();

  const Notification& notification = ui_manager()->GetNotificationAt(0);

  EXPECT_TRUE(notification.context_message().empty());
  EXPECT_EQ("https://" + host_port.ToString() + "/",
            notification.origin_url().spec());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       CheckFilePermissionNotGranted) {
  // TODO(felt): This DCHECKs when bubbles are enabled, when the file_url is
  // persisted. crbug.com/502057
  if (PermissionBubbleManager::Enabled())
    return;

  // TODO(dewittj): It currently isn't possible to get the notification
  // permission for a file:// URL. If that changes, this test will fail to
  // remind the author that the
  // |PlatformNotificationServiceImpl::WebOriginDisplayName| function needs
  // to be updated to properly display file:// URL origins.
  // See crbug.com/402191.

  // This case should succeed because a normal page URL is used.
  std::string script_result;

  NotificationPermissionContext* permission_context =
      NotificationPermissionContextFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(permission_context);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_context->GetPermissionStatus(TestPageUrl(),
                                                    TestPageUrl()));

  RequestAndAcceptPermission();
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            permission_context->GetPermissionStatus(TestPageUrl(),
                                                    TestPageUrl()));

  // This case should fail because a file URL is used.
  base::FilePath dir_source_root;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &dir_source_root));
  base::FilePath full_file_path =
      dir_source_root.Append(server_root()).AppendASCII(kTestFileName);
  GURL file_url(net::FilePathToFileURL(full_file_path));

  ui_test_utils::NavigateToURL(browser(), file_url);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_context->GetPermissionStatus(file_url, file_url));

  RequestAndAcceptPermission();
  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_context->GetPermissionStatus(file_url, file_url))
      << "If this test fails, you may have fixed a bug preventing file origins "
      << "from sending their origin from Blink; if so you need to update the "
      << "display function for notification origins to show the file path.";
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       DataUrlAsNotificationImage) {
  ASSERT_NO_FATAL_FAILURE(GrantNotificationPermissionForTest());

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotificationDataUrlImage()",
                        &script_result));
  EXPECT_EQ("ok", script_result);

  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());

  const Notification& notification = ui_manager()->GetNotificationAt(0);
  EXPECT_FALSE(notification.icon().IsEmpty());

  EXPECT_EQ("Data URL Title", base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ(kIconWidth, notification.icon().Width());
  EXPECT_EQ(kIconHeight, notification.icon().Height());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       BlobAsNotificationImage) {
  ASSERT_NO_FATAL_FAILURE(GrantNotificationPermissionForTest());

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotificationBlobImage()",
                        &script_result));
  EXPECT_EQ("ok", script_result);

  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());

  const Notification& notification = ui_manager()->GetNotificationAt(0);
  EXPECT_FALSE(notification.icon().IsEmpty());

  EXPECT_EQ("Blob Title", base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ(kIconWidth, notification.icon().Width());
  EXPECT_EQ(kIconHeight, notification.icon().Height());
}
