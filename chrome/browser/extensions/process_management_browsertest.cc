// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::NavigationController;
using content::WebContents;

namespace {

class ProcessManagementTest : public ExtensionBrowserTest {
 private:
  // This is needed for testing isolated apps, which are still experimental.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        extensions::switches::kEnableExperimentalExtensionApis);
  }
};

}  // namespace


// TODO(nasko): crbug.com/173137
#if defined(OS_WIN)
#define MAYBE_ProcessOverflow DISABLED_ProcessOverflow
#else
#define MAYBE_ProcessOverflow ProcessOverflow
#endif

// Ensure that an isolated app never shares a process with WebUIs, non-isolated
// extensions, and normal webpages.  None of these should ever comingle
// RenderProcessHosts even if we hit the process limit.
IN_PROC_BROWSER_TEST_F(ProcessManagementTest, MAYBE_ProcessOverflow) {
  // Set max renderers to 1 to force running out of processes.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("isolated_apps/app1")));
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("isolated_apps/app2")));
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("hosted_app")));
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("api_test/app_process")));

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL base_url = embedded_test_server()->GetURL(
      "/extensions/");
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  base_url = base_url.ReplaceComponents(replace_host);

  // Load an extension before adding tabs.
  const extensions::Extension* extension1 = LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/basics"));
  ASSERT_TRUE(extension1);
  GURL extension1_url = extension1->url();

  // Create multiple tabs for each type of renderer that might exist.
  ui_test_utils::NavigateToURL(
      browser(), base_url.Resolve("isolated_apps/app1/main.html"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("hosted_app/main.html"),
      NEW_FOREGROUND_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("test_file.html"),
      NEW_FOREGROUND_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("isolated_apps/app2/main.html"),
      NEW_FOREGROUND_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("api_test/app_process/path1/empty.html"),
      NEW_FOREGROUND_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("test_file_with_body.html"),
      NEW_FOREGROUND_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Load another copy of isolated app 1.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("isolated_apps/app1/main.html"),
      NEW_FOREGROUND_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Load another extension.
  const extensions::Extension* extension2 = LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/close_background"));
  ASSERT_TRUE(extension2);
  GURL extension2_url = extension2->url();

  // Get tab processes.
  ASSERT_EQ(9, browser()->tab_strip_model()->count());
  content::RenderProcessHost* isolated1_host =
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetRenderProcessHost();
  content::RenderProcessHost* ntp1_host =
      browser()->tab_strip_model()->GetWebContentsAt(1)->GetRenderProcessHost();
  content::RenderProcessHost* hosted1_host =
      browser()->tab_strip_model()->GetWebContentsAt(2)->GetRenderProcessHost();
  content::RenderProcessHost* web1_host =
      browser()->tab_strip_model()->GetWebContentsAt(3)->GetRenderProcessHost();

  content::RenderProcessHost* isolated2_host =
      browser()->tab_strip_model()->GetWebContentsAt(4)->GetRenderProcessHost();
  content::RenderProcessHost* ntp2_host =
      browser()->tab_strip_model()->GetWebContentsAt(5)->GetRenderProcessHost();
  content::RenderProcessHost* hosted2_host =
      browser()->tab_strip_model()->GetWebContentsAt(6)->GetRenderProcessHost();
  content::RenderProcessHost* web2_host =
      browser()->tab_strip_model()->GetWebContentsAt(7)->GetRenderProcessHost();

  content::RenderProcessHost* second_isolated1_host =
      browser()->tab_strip_model()->GetWebContentsAt(8)->GetRenderProcessHost();

  // Get extension processes.
  extensions::ProcessManager* process_manager =
      extensions::ProcessManager::Get(browser()->profile());
  content::RenderProcessHost* extension1_host =
      process_manager->GetSiteInstanceForURL(extension1_url)->GetProcess();
  content::RenderProcessHost* extension2_host =
      process_manager->GetSiteInstanceForURL(extension2_url)->GetProcess();

  // An isolated app only shares with other instances of itself, not other
  // isolated apps or anything else.
  EXPECT_EQ(isolated1_host, second_isolated1_host);
  EXPECT_NE(isolated1_host, isolated2_host);
  EXPECT_NE(isolated1_host, ntp1_host);
  EXPECT_NE(isolated1_host, hosted1_host);
  EXPECT_NE(isolated1_host, web1_host);
  EXPECT_NE(isolated1_host, extension1_host);
  EXPECT_NE(isolated2_host, ntp1_host);
  EXPECT_NE(isolated2_host, hosted1_host);
  EXPECT_NE(isolated2_host, web1_host);
  EXPECT_NE(isolated2_host, extension1_host);

  // Everything else is clannish.  WebUI only shares with other WebUI.
  EXPECT_EQ(ntp1_host, ntp2_host);
  EXPECT_NE(ntp1_host, hosted1_host);
  EXPECT_NE(ntp1_host, web1_host);
  EXPECT_NE(ntp1_host, extension1_host);

  // Hosted apps only share with each other.
  // Note that hosted2_host's app has the background permission and will use
  // process-per-site mode, but it should still share with hosted1_host's app.
  EXPECT_EQ(hosted1_host, hosted2_host);
  EXPECT_NE(hosted1_host, web1_host);
  EXPECT_NE(hosted1_host, extension1_host);

  // Web pages only share with each other.
  EXPECT_EQ(web1_host, web2_host);
  EXPECT_NE(web1_host, extension1_host);

  // Extensions only share with each other.
  EXPECT_EQ(extension1_host, extension2_host);
}

// See
#if defined(OS_WIN)
#define MAYBE_ExtensionProcessBalancing DISABLED_ExtensionProcessBalancing
#else
#define MAYBE_ExtensionProcessBalancing ExtensionProcessBalancing
#endif
// Test to verify that the policy of maximum share of extension processes is
// properly enforced.
IN_PROC_BROWSER_TEST_F(ProcessManagementTest, MAYBE_ExtensionProcessBalancing) {
  // Set max renderers to 6 so we can expect 2 extension processes to be
  // allocated.
  content::RenderProcessHost::SetMaxRendererProcessCount(6);

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL base_url = embedded_test_server()->GetURL(
      "/extensions/");
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  base_url = base_url.ReplaceComponents(replace_host);

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/none")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/basics")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/remove_popup")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/add_popup")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/no_icon")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("isolated_apps/app1")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/management/test")));

  ui_test_utils::NavigateToURL(
      browser(), base_url.Resolve("isolated_apps/app1/main.html"));

  ui_test_utils::NavigateToURL(
      browser(), base_url.Resolve("api_test/management/test/basics.html"));

  std::set<int> process_ids;
  Profile* profile = browser()->profile();
  extensions::ProcessManager* epm = extensions::ProcessManager::Get(profile);
  for (extensions::ExtensionHost* host : epm->background_hosts())
    process_ids.insert(host->render_process_host()->GetID());

  // We've loaded 5 extensions with background pages, 1 extension without
  // background page, and one isolated app. We expect only 2 unique processes
  // hosting those extensions.
  extensions::ProcessMap* process_map = extensions::ProcessMap::Get(profile);

  EXPECT_GE((size_t) 6, process_map->size());
  EXPECT_EQ((size_t) 2, process_ids.size());
}
