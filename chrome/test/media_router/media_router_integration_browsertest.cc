// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/media_router_integration_browsertest.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/media_router/media_router_dialog_controller_impl.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/base/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace media_router {

namespace {
// The path relative to <chromium src>/out/<build config> for media router
// browser test resources.
const base::FilePath::StringPieceType kResourcePath = FILE_PATH_LITERAL(
    "media_router/browser_test_resources/");
// The javascript snippets.
const std::string kCheckSessionScript = "checkSession();";
const std::string kCheckSessionFailedScript = "checkSessionFailedToStart();";
const std::string kStartSessionScript = "startSession();";
const std::string kStopSessionScript = "stopSession()";
const std::string kWaitDeviceScript = "waitUntilDeviceAvailable();";

void GetStartedSessionId(content::WebContents* web_contents,
                         std::string* session_id) {
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents, "window.domAutomationController.send(startedSession.id)",
      session_id));
}

}  // namespace

MediaRouterIntegrationBrowserTest::MediaRouterIntegrationBrowserTest() {
}

MediaRouterIntegrationBrowserTest::~MediaRouterIntegrationBrowserTest() {
}

void MediaRouterIntegrationBrowserTest::TearDownOnMainThread() {
  MediaRouterBaseBrowserTest::TearDownOnMainThread();
  test_navigation_observer_.reset();
}

void MediaRouterIntegrationBrowserTest::ExecuteJavaScriptAPI(
    content::WebContents* web_contents,
    const std::string& script) {
  std::string result(ExecuteScriptAndExtractString(web_contents, script));

  // Read the test result, the test result set by javascript is a
  // JSON string with the following format:
  // {"passed": "<true/false>", "errorMessage": "<error_message>"}
  scoped_ptr<base::Value> value =
      base::JSONReader::Read(result, base::JSON_ALLOW_TRAILING_COMMAS);

  // Convert to dictionary.
  base::DictionaryValue* dict_value = nullptr;
  ASSERT_TRUE(value->GetAsDictionary(&dict_value));

  // Extract the fields.
  bool passed = false;
  ASSERT_TRUE(dict_value->GetBoolean("passed", &passed));
  std::string error_message;
  ASSERT_TRUE(dict_value->GetString("errorMessage", &error_message));

  ASSERT_TRUE(passed) << error_message;
}

void MediaRouterIntegrationBrowserTest::OpenTestPage(
    base::FilePath::StringPieceType file_name) {
  base::FilePath full_path = GetResourceFile(file_name);
  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(full_path));
}

void MediaRouterIntegrationBrowserTest::OpenTestPageInNewTab(
    base::FilePath::StringPieceType file_name) {
  base::FilePath full_path = GetResourceFile(file_name);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), net::FilePathToFileURL(full_path), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
}

void MediaRouterIntegrationBrowserTest::StartSession(
    content::WebContents* web_contents) {
  test_navigation_observer_.reset(
      new content::TestNavigationObserver(web_contents, 1));
  test_navigation_observer_->StartWatchingNewWebContents();
  ExecuteJavaScriptAPI(web_contents, kStartSessionScript);
  test_navigation_observer_->Wait();
  test_navigation_observer_->StopWatchingNewWebContents();
}

void MediaRouterIntegrationBrowserTest::ChooseSink(
    content::WebContents* web_contents,
    const std::string& sink_id,
    const std::string& current_route) {
  content::WebContents* dialog_contents = GetMRDialog(web_contents);
  std::string route;
  if (current_route.empty()) {
    route = "null";
  } else {
    route = current_route;
    std::string script = base::StringPrintf(
        "window.document.getElementById('media-router-container')."
        "currentRoute_ = %s", route.c_str());
    ASSERT_TRUE(content::ExecuteScript(dialog_contents, script));
  }
  std::string script = base::StringPrintf(
      "window.document.getElementById('media-router-container')."
      "showOrCreateRoute_({'id': '%s', 'name': ''}, %s)",
      sink_id.c_str(), route.c_str());
  ASSERT_TRUE(content::ExecuteScript(dialog_contents, script));
}

content::WebContents* MediaRouterIntegrationBrowserTest::GetMRDialog(
    content::WebContents* web_contents) {
  MediaRouterDialogControllerImpl* controller =
      MediaRouterDialogControllerImpl::GetOrCreateForWebContents(web_contents);
  content::WebContents* dialog_contents = controller->GetMediaRouterDialog();
  CHECK(dialog_contents);
  return dialog_contents;
}

void MediaRouterIntegrationBrowserTest::SetTestData(
    base::FilePath::StringPieceType test_data_file) {
  base::FilePath full_path = GetResourceFile(test_data_file);
  JSONFileValueDeserializer deserializer(full_path);
  int error_code = 0;
  std::string error_message;
  scoped_ptr<base::Value> value(
      deserializer.Deserialize(&error_code, &error_message));
  CHECK(value.get()) << "Deserialize failed: " << error_message;
  std::string test_data_str;
  ASSERT_TRUE(base::JSONWriter::Write(*value, &test_data_str));
  ExecuteScriptInBackgroundPageNoWait(
      extension_id_,
      base::StringPrintf("localStorage['testdata'] = '%s'",
                         test_data_str.c_str()));
}

content::WebContents* MediaRouterIntegrationBrowserTest::OpenMRDialog(
    content::WebContents* web_contents) {
  MediaRouterDialogControllerImpl* controller =
      MediaRouterDialogControllerImpl::GetOrCreateForWebContents(web_contents);
  test_navigation_observer_.reset(
        new content::TestNavigationObserver(web_contents, 1));
  test_navigation_observer_->StartWatchingNewWebContents();
  CHECK(controller->ShowMediaRouterDialog());
  test_navigation_observer_->Wait();
  test_navigation_observer_->StopWatchingNewWebContents();
  content::WebContents* dialog_contents = controller->GetMediaRouterDialog();
  CHECK(dialog_contents);
  return dialog_contents;
}

base::FilePath MediaRouterIntegrationBrowserTest::GetResourceFile(
    base::FilePath::StringPieceType relative_path) const {
  base::FilePath base_dir;
  // ASSERT_TRUE can only be used in void returning functions.
  // Use CHECK instead in non-void returning functions.
  CHECK(PathService::Get(base::DIR_MODULE, &base_dir));
  base::FilePath full_path =
      base_dir.Append(kResourcePath).Append(relative_path);
  CHECK(PathExists(full_path));
  return full_path;
}

int MediaRouterIntegrationBrowserTest::ExecuteScriptAndExtractInt(
    const content::ToRenderFrameHost& adapter, const std::string& script) {
  int result;
  CHECK(content::ExecuteScriptAndExtractInt(adapter, script, &result));
  return result;
}

std::string MediaRouterIntegrationBrowserTest::ExecuteScriptAndExtractString(
    const content::ToRenderFrameHost& adapter, const std::string& script) {
  std::string result;
  CHECK(content::ExecuteScriptAndExtractString(adapter, script, &result));
  return result;
}

bool MediaRouterIntegrationBrowserTest::IsRouteCreatedOnUI() {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WebContents* dialog_contents = GetMRDialog(web_contents);
  std::string script;
  script = base::StringPrintf(
      "domAutomationController.send(window.document.getElementById("
      "'media-router-container').routeList.length)");
  return ExecuteScriptAndExtractInt(dialog_contents, script) == 1;
}

void MediaRouterIntegrationBrowserTest::WaitUntilRouteCreated() {
  ConditionalWait(
      base::TimeDelta::FromSeconds(10), base::TimeDelta::FromSeconds(1),
      base::Bind(&MediaRouterIntegrationBrowserTest::IsRouteCreatedOnUI,
                 base::Unretained(this)));
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest, MANUAL_Basic) {
  OpenTestPage(FILE_PATH_LITERAL("basic_test.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ExecuteJavaScriptAPI(web_contents, kWaitDeviceScript);
  StartSession(web_contents);
  ChooseSink(web_contents, "id1", "");
  ExecuteJavaScriptAPI(web_contents, kCheckSessionScript);
  Wait(base::TimeDelta::FromSeconds(5));
  ExecuteJavaScriptAPI(web_contents, kStopSessionScript);
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       MANUAL_Fail_No_Provider) {
  SetTestData(FILE_PATH_LITERAL("no_provider.json"));
  OpenTestPage(FILE_PATH_LITERAL("no_provider.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ExecuteJavaScriptAPI(web_contents, kWaitDeviceScript);
  StartSession(web_contents);
  ChooseSink(web_contents, "id1", "");
  ExecuteJavaScriptAPI(web_contents, kCheckSessionFailedScript);
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       MANUAL_Fail_Create_Route) {
  SetTestData(FILE_PATH_LITERAL("fail_create_route.json"));
  OpenTestPage(FILE_PATH_LITERAL("fail_create_route.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ExecuteJavaScriptAPI(web_contents, kWaitDeviceScript);
  StartSession(web_contents);
  ChooseSink(web_contents, "id1", "");
  ExecuteJavaScriptAPI(web_contents, kCheckSessionFailedScript);
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest, MANUAL_JoinSession) {
  OpenTestPage(FILE_PATH_LITERAL("basic_test.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ExecuteJavaScriptAPI(web_contents, kWaitDeviceScript);
  StartSession(web_contents);
  ChooseSink(web_contents, "id1", "");
  ExecuteJavaScriptAPI(web_contents, kCheckSessionScript);
  std::string session_id;
  GetStartedSessionId(web_contents, &session_id);

  OpenTestPageInNewTab(FILE_PATH_LITERAL("basic_test.html"));
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(new_web_contents);
  ASSERT_NE(web_contents, new_web_contents);
  ExecuteJavaScriptAPI(
      new_web_contents,
      base::StringPrintf("joinSession('%s');", session_id.c_str()));
  std::string joined_session_id;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      new_web_contents, "window.domAutomationController.send(joinedSession.id)",
      &joined_session_id));
  ASSERT_EQ(session_id, joined_session_id);
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       MANUAL_Fail_JoinSession) {
  OpenTestPage(FILE_PATH_LITERAL("basic_test.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ExecuteJavaScriptAPI(web_contents, kWaitDeviceScript);
  content::TestNavigationObserver test_navigation_observer(web_contents, 1);
  StartSession(web_contents);
  ChooseSink(web_contents, "id1", "");
  ExecuteJavaScriptAPI(web_contents, kCheckSessionScript);
  std::string session_id;
  GetStartedSessionId(web_contents, &session_id);

  SetTestData(FILE_PATH_LITERAL("fail_join_session.json"));
  OpenTestPage(FILE_PATH_LITERAL("fail_join_session.html"));
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(new_web_contents);
  ExecuteJavaScriptAPI(
      new_web_contents,
      base::StringPrintf("checkJoinSessionFails('%s');", session_id.c_str()));
}

}  // namespace media_router
