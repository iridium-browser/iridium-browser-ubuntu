// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "net/dns/mock_host_resolver.h"
#include "url/gurl.h"

using extensions::Extension;

class ChromeAppAPITest : public ExtensionBrowserTest {
 protected:
  bool IsAppInstalledInMainFrame() {
    return IsAppInstalledInFrame(
        browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame());
  }
  bool IsAppInstalledInIFrame() {
    return IsAppInstalledInFrame(GetIFrame());
  }
  bool IsAppInstalledInFrame(content::RenderFrameHost* frame) {
    const char kGetAppIsInstalled[] =
        "window.domAutomationController.send(window.chrome.app.isInstalled);";
    bool result;
    CHECK(content::ExecuteScriptAndExtractBool(frame,
                                               kGetAppIsInstalled,
                                               &result));
    return result;
  }

  std::string InstallStateInMainFrame() {
    return InstallStateInFrame(
        browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame());
  }
  std::string InstallStateInIFrame() {
    return InstallStateInFrame(GetIFrame());
  }
  std::string InstallStateInFrame(content::RenderFrameHost* frame) {
    const char kGetAppInstallState[] =
        "window.chrome.app.installState("
        "    function(s) { window.domAutomationController.send(s); });";
    std::string result;
    CHECK(content::ExecuteScriptAndExtractString(frame,
                                                 kGetAppInstallState,
                                                 &result));
    return result;
  }

  std::string RunningStateInMainFrame() {
    return RunningStateInFrame(
        browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame());
  }
  std::string RunningStateInIFrame() {
    return RunningStateInFrame(GetIFrame());
  }
  std::string RunningStateInFrame(content::RenderFrameHost* frame) {
    const char kGetAppRunningState[] =
        "window.domAutomationController.send("
        "    window.chrome.app.runningState());";
    std::string result;
    CHECK(content::ExecuteScriptAndExtractString(frame,
                                                 kGetAppRunningState,
                                                 &result));
    return result;
  }

 private:
  content::RenderFrameHost* GetIFrame() {
    return content::FrameMatchingPredicate(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::Bind(&content::FrameIsChildOfMainFrame));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kAppsCheckoutURL,
                                    "http://checkout.com:");
  }
};

// Flaky http://crbug.com/238674
#if defined(OS_WIN)
#define MAYBE_IsInstalled DISABLED_IsInstalled
#else
#define MAYBE_IsInstalled IsInstalled
#endif
IN_PROC_BROWSER_TEST_F(ChromeAppAPITest, MAYBE_IsInstalled) {
  static const char kAppHost[] = "app.com";
  static const char kNonAppHost[] = "nonapp.com";

  host_resolver()->AddRule(kAppHost, "127.0.0.1");
  host_resolver()->AddRule(kNonAppHost, "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  GURL test_file_url(test_server()->GetURL("extensions/test_file.html"));
  GURL::Replacements replace_host;

  replace_host.SetHostStr(kAppHost);
  GURL app_url(test_file_url.ReplaceComponents(replace_host));

  replace_host.SetHostStr(kNonAppHost);
  GURL non_app_url(test_file_url.ReplaceComponents(replace_host));

  // Before the app is installed, app.com does not think that it is installed
  ui_test_utils::NavigateToURL(browser(), app_url);
  EXPECT_FALSE(IsAppInstalledInMainFrame());

  // Load an app which includes app.com in its extent.
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("app_dot_com_app"));
  ASSERT_TRUE(extension);

  // Even after the app is installed, the existing app.com tab is not in an
  // app process, so chrome.app.isInstalled should return false.
  EXPECT_FALSE(IsAppInstalledInMainFrame());

  // Test that a non-app page has chrome.app.isInstalled = false.
  ui_test_utils::NavigateToURL(browser(), non_app_url);
  EXPECT_FALSE(IsAppInstalledInMainFrame());

  // Test that a non-app page returns null for chrome.app.getDetails().
  const char kGetAppDetails[] =
      "window.domAutomationController.send("
      "    JSON.stringify(window.chrome.app.getDetails()));";
  std::string result;
  ASSERT_TRUE(
      content::ExecuteScriptAndExtractString(
          browser()->tab_strip_model()->GetActiveWebContents(),
          kGetAppDetails,
          &result));
  EXPECT_EQ("null", result);

  // Check that an app page has chrome.app.isInstalled = true.
  ui_test_utils::NavigateToURL(browser(), app_url);
  EXPECT_TRUE(IsAppInstalledInMainFrame());

  // Check that an app page returns the correct result for
  // chrome.app.getDetails().
  ui_test_utils::NavigateToURL(browser(), app_url);
  ASSERT_TRUE(
      content::ExecuteScriptAndExtractString(
          browser()->tab_strip_model()->GetActiveWebContents(),
          kGetAppDetails,
          &result));
  scoped_ptr<base::DictionaryValue> app_details(
      static_cast<base::DictionaryValue*>(base::JSONReader::Read(result)));
  // extension->manifest() does not contain the id.
  app_details->Remove("id", NULL);
  EXPECT_TRUE(app_details.get());
  EXPECT_TRUE(app_details->Equals(extension->manifest()->value()));

  // Try to change app.isInstalled.  Should silently fail, so
  // that isInstalled should have the initial value.
  ASSERT_TRUE(
      content::ExecuteScriptAndExtractString(
          browser()->tab_strip_model()->GetActiveWebContents(),
          "window.domAutomationController.send("
          "    function() {"
          "        var value = window.chrome.app.isInstalled;"
          "        window.chrome.app.isInstalled = !value;"
          "        if (window.chrome.app.isInstalled == value) {"
          "            return 'true';"
          "        } else {"
          "            return 'false';"
          "        }"
          "    }()"
          ");",
          &result));

  // Should not be able to alter window.chrome.app.isInstalled from javascript";
  EXPECT_EQ("true", result);
}

IN_PROC_BROWSER_TEST_F(ChromeAppAPITest, GetDetailsForFrame) {
  const char kAppHost[] = "app.com";
  const char kNonAppHost[] = "nonapp.com";
  const char kCheckoutHost[] = "checkout.com";

  host_resolver()->AddRule(kAppHost, "127.0.0.1");
  host_resolver()->AddRule(kNonAppHost, "127.0.0.1");
  host_resolver()->AddRule(kCheckoutHost, "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  GURL test_file_url(test_server()->GetURL(
      "files/extensions/get_app_details_for_frame.html"));
  GURL::Replacements replace_host;

  replace_host.SetHostStr(kCheckoutHost);
  GURL checkout_url(test_file_url.ReplaceComponents(replace_host));

  replace_host.SetHostStr(kAppHost);
  GURL app_url(test_file_url.ReplaceComponents(replace_host));

  // Load an app which includes app.com in its extent.
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("app_dot_com_app"));
  ASSERT_TRUE(extension);

  // Test that normal pages (even apps) cannot use getDetailsForFrame().
  ui_test_utils::NavigateToURL(browser(), app_url);
  const char kTestUnsuccessfulAccess[] =
      "window.domAutomationController.send(window.testUnsuccessfulAccess())";
  bool result = false;
  ASSERT_TRUE(
      content::ExecuteScriptAndExtractBool(
          browser()->tab_strip_model()->GetActiveWebContents(),
          kTestUnsuccessfulAccess,
          &result));
  EXPECT_TRUE(result);

  // Test that checkout can use getDetailsForFrame() and that it works
  // correctly.
  ui_test_utils::NavigateToURL(browser(), checkout_url);
  const char kGetDetailsForFrame[] =
      "window.domAutomationController.send("
      "    JSON.stringify(chrome.app.getDetailsForFrame(frames[0])))";
  std::string json;
  ASSERT_TRUE(
      content::ExecuteScriptAndExtractString(
          browser()->tab_strip_model()->GetActiveWebContents(),
          kGetDetailsForFrame,
          &json));

  scoped_ptr<base::DictionaryValue> app_details(
      static_cast<base::DictionaryValue*>(base::JSONReader::Read(json)));
  // extension->manifest() does not contain the id.
  app_details->Remove("id", NULL);
  EXPECT_TRUE(app_details.get());
  EXPECT_TRUE(app_details->Equals(extension->manifest()->value()));
}


IN_PROC_BROWSER_TEST_F(ChromeAppAPITest, InstallAndRunningState) {
  static const char kAppHost[] = "app.com";
  static const char kNonAppHost[] = "nonapp.com";

  host_resolver()->AddRule(kAppHost, "127.0.0.1");
  host_resolver()->AddRule(kNonAppHost, "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  GURL test_file_url(test_server()->GetURL(
      "files/extensions/get_app_details_for_frame.html"));
  GURL::Replacements replace_host;

  replace_host.SetHostStr(kAppHost);
  GURL app_url(test_file_url.ReplaceComponents(replace_host));

  replace_host.SetHostStr(kNonAppHost);
  GURL non_app_url(test_file_url.ReplaceComponents(replace_host));

  // Before the app is installed, app.com does not think that it is installed
  ui_test_utils::NavigateToURL(browser(), app_url);

  EXPECT_EQ("not_installed", InstallStateInMainFrame());
  EXPECT_EQ("cannot_run", RunningStateInMainFrame());
  EXPECT_FALSE(IsAppInstalledInMainFrame());

  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("app_dot_com_app"));
  ASSERT_TRUE(extension);

  EXPECT_EQ("installed", InstallStateInMainFrame());
  EXPECT_EQ("ready_to_run", RunningStateInMainFrame());
  EXPECT_FALSE(IsAppInstalledInMainFrame());

  // Reloading the page should put the tab in an app process.
  ui_test_utils::NavigateToURL(browser(), app_url);
  EXPECT_EQ("installed", InstallStateInMainFrame());
  EXPECT_EQ("running", RunningStateInMainFrame());
  EXPECT_TRUE(IsAppInstalledInMainFrame());

  // Disable the extension and verify the state.
  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();
  service->DisableExtension(extension->id(),
                            Extension::DISABLE_PERMISSIONS_INCREASE);
  ui_test_utils::NavigateToURL(browser(), app_url);

  EXPECT_EQ("disabled", InstallStateInMainFrame());
  EXPECT_EQ("cannot_run", RunningStateInMainFrame());
  EXPECT_FALSE(IsAppInstalledInMainFrame());

  service->EnableExtension(extension->id());
  EXPECT_EQ("installed", InstallStateInMainFrame());
  EXPECT_EQ("ready_to_run", RunningStateInMainFrame());
  EXPECT_FALSE(IsAppInstalledInMainFrame());

  // The non-app URL should still not be installed or running.
  ui_test_utils::NavigateToURL(browser(), non_app_url);

  EXPECT_EQ("not_installed", InstallStateInMainFrame());
  EXPECT_EQ("cannot_run", RunningStateInMainFrame());
  EXPECT_FALSE(IsAppInstalledInMainFrame());

  EXPECT_EQ("installed", InstallStateInIFrame());
  EXPECT_EQ("cannot_run", RunningStateInIFrame());
  EXPECT_FALSE(IsAppInstalledInIFrame());
}

IN_PROC_BROWSER_TEST_F(ChromeAppAPITest, InstallAndRunningStateFrame) {
  static const char kAppHost[] = "app.com";
  static const char kNonAppHost[] = "nonapp.com";

  host_resolver()->AddRule(kAppHost, "127.0.0.1");
  host_resolver()->AddRule(kNonAppHost, "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  GURL test_file_url(test_server()->GetURL(
      "files/extensions/get_app_details_for_frame_reversed.html"));
  GURL::Replacements replace_host;

  replace_host.SetHostStr(kAppHost);
  GURL app_url(test_file_url.ReplaceComponents(replace_host));

  replace_host.SetHostStr(kNonAppHost);
  GURL non_app_url(test_file_url.ReplaceComponents(replace_host));

  // Check the install and running state of a non-app iframe running
  // within an app.
  ui_test_utils::NavigateToURL(browser(), app_url);

  EXPECT_EQ("not_installed", InstallStateInIFrame());
  EXPECT_EQ("cannot_run", RunningStateInIFrame());
  EXPECT_FALSE(IsAppInstalledInIFrame());
}
