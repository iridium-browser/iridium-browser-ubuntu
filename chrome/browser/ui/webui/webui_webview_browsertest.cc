// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/path_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class WebUIWebViewBrowserTest : public WebUIBrowserTest {
 public:
  WebUIWebViewBrowserTest() {}

  void SetUpOnMainThread() override {
    WebUIBrowserTest::SetUpOnMainThread();
    AddLibrary(
        base::FilePath(FILE_PATH_LITERAL("webview_content_script_test.js")));

    base::FilePath test_data_dir;
    PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  }

  GURL GetTestUrl(const std::string& path) const {
    return embedded_test_server()->base_url().Resolve(path);
  }

  GURL GetWebViewEnabledWebUIURL() const {
    return GURL(chrome::kChromeUIChromeSigninURL);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebUIWebViewBrowserTest);
};

IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, ExecuteScriptCode) {
  ui_test_utils::NavigateToURL(browser(), GetWebViewEnabledWebUIURL());

  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testExecuteScriptCode",
      new base::StringValue(GetTestUrl("empty.html").spec())));
}

IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, ExecuteScriptCodeFromFile) {
  ui_test_utils::NavigateToURL(browser(), GetWebViewEnabledWebUIURL());

  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testExecuteScriptCodeFromFile",
      new base::StringValue(GetTestUrl("empty.html").spec())));
}

IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, AddContentScript) {
  ui_test_utils::NavigateToURL(browser(), GetWebViewEnabledWebUIURL());

  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testAddContentScript",
      new base::StringValue(GetTestUrl("empty.html").spec())));
}

IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, AddMultiContentScripts) {
  ui_test_utils::NavigateToURL(browser(), GetWebViewEnabledWebUIURL());

  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testAddMultiContentScripts",
      new base::StringValue(GetTestUrl("empty.html").spec())));
}

IN_PROC_BROWSER_TEST_F(
    WebUIWebViewBrowserTest,
    AddContentScriptWithSameNameShouldOverwriteTheExistingOne) {
  ui_test_utils::NavigateToURL(browser(), GetWebViewEnabledWebUIURL());

  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testAddContentScriptWithSameNameShouldOverwriteTheExistingOne",
      new base::StringValue(GetTestUrl("empty.html").spec())));
}

IN_PROC_BROWSER_TEST_F(
    WebUIWebViewBrowserTest,
    AddContentScriptToOneWebViewShouldNotInjectToTheOtherWebView) {
  ui_test_utils::NavigateToURL(browser(), GetWebViewEnabledWebUIURL());

  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testAddContentScriptToOneWebViewShouldNotInjectToTheOtherWebView",
      new base::StringValue(GetTestUrl("empty.html").spec())));
}

IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, AddAndRemoveContentScripts) {
  ui_test_utils::NavigateToURL(browser(), GetWebViewEnabledWebUIURL());

  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testAddAndRemoveContentScripts",
      new base::StringValue(GetTestUrl("empty.html").spec())));
}

IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest,
                       AddContentScriptsWithNewWindowAPI) {
  ui_test_utils::NavigateToURL(browser(), GetWebViewEnabledWebUIURL());

  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testAddContentScriptsWithNewWindowAPI",
      new base::StringValue(GetTestUrl("guest_from_opener.html").spec())));
}

IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest,
                       ContentScriptIsInjectedAfterTerminateAndReloadWebView) {
  ui_test_utils::NavigateToURL(browser(), GetWebViewEnabledWebUIURL());

  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testContentScriptIsInjectedAfterTerminateAndReloadWebView",
      new base::StringValue(GetTestUrl("empty.html").spec())));
}

IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest,
                       ContentScriptExistsAsLongAsWebViewTagExists) {
  ui_test_utils::NavigateToURL(browser(), GetWebViewEnabledWebUIURL());

  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testContentScriptExistsAsLongAsWebViewTagExists",
      new base::StringValue(GetTestUrl("empty.html").spec())));
}

IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, AddContentScriptWithCode) {
  ui_test_utils::NavigateToURL(browser(), GetWebViewEnabledWebUIURL());

  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testAddContentScriptWithCode",
      new base::StringValue(GetTestUrl("empty.html").spec())));
}

#if defined(OS_CHROMEOS)
// Right now we only have incognito WebUI on CrOS, but this should
// theoretically work for all platforms.
IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, AddContentScriptIncognito) {
  Browser* incognito_browser = ui_test_utils::OpenURLOffTheRecord(
      browser()->profile(), GetWebViewEnabledWebUIURL());

  SetWebUIInstance(
      incognito_browser->tab_strip_model()->GetActiveWebContents()->GetWebUI());

  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testAddContentScript",
      new base::StringValue(GetTestUrl("empty.html").spec())));
}
#endif
