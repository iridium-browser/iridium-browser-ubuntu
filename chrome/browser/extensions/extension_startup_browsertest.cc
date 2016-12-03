// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/shared_user_script_master.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/feature_switch.h"
#include "net/base/filename_util.h"

using extensions::FeatureSwitch;

// This file contains high-level startup tests for the extensions system. We've
// had many silly bugs where command line flags did not get propagated correctly
// into the services, so we didn't start correctly.

class ExtensionStartupTestBase : public InProcessBrowserTest {
 public:
  ExtensionStartupTestBase() : unauthenticated_load_allowed_(true) {
    num_expected_extensions_ = 3;
  }

 protected:
  // InProcessBrowserTest
  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (load_extensions_.empty()) {
      // If no |load_extensions_| were specified, allow unauthenticated
      // extension settings to be loaded from Preferences as if they had been
      // authenticated correctly before they were handed to the ExtensionSystem.
      command_line->AppendSwitchASCII(
          switches::kForceFieldTrials,
          base::StringPrintf(
              "%s/%s/",
              chrome_prefs::internals::kSettingsEnforcementTrialName,
              chrome_prefs::internals::kSettingsEnforcementGroupNoEnforcement));
#if defined(OFFICIAL_BUILD) && defined(OS_WIN)
      // In Windows official builds, it is not possible to disable settings
      // authentication.
      unauthenticated_load_allowed_ = false;
#endif
    } else {
      base::FilePath::StringType paths =
          base::JoinString(load_extensions_,
                           base::FilePath::StringType(1, ','));
      command_line->AppendSwitchNative(switches::kLoadExtension,
                                       paths);
      command_line->AppendSwitch(switches::kDisableExtensionsFileAccessCheck);
    }
  }

  bool SetUpUserDataDirectory() override {
    base::FilePath profile_dir;
    PathService::Get(chrome::DIR_USER_DATA, &profile_dir);
    profile_dir = profile_dir.AppendASCII(TestingProfile::kTestUserProfileDir);
    base::CreateDirectory(profile_dir);

    preferences_file_ = profile_dir.Append(chrome::kPreferencesFilename);
    user_scripts_dir_ = profile_dir.AppendASCII("User Scripts");
    extensions_dir_ = profile_dir.AppendASCII("Extensions");

    if (load_extensions_.empty()) {
      base::FilePath src_dir;
      PathService::Get(chrome::DIR_TEST_DATA, &src_dir);
      src_dir = src_dir.AppendASCII("extensions").AppendASCII("good");

      base::CopyFile(src_dir.Append(chrome::kPreferencesFilename),
                     preferences_file_);
      base::CopyDirectory(src_dir.AppendASCII("Extensions"),
                          profile_dir, true);  // recursive
    }
    return true;
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    // Bots are on a domain, turn off the domain check for settings hardening in
    // order to be able to test all SettingsEnforcement groups.
    chrome_prefs::DisableDomainCheckForTesting();
  }

  void TearDown() override {
    EXPECT_TRUE(base::DeleteFile(preferences_file_, false));

    // TODO(phajdan.jr): Check return values of the functions below, carefully.
    base::DeleteFile(user_scripts_dir_, true);
    base::DeleteFile(extensions_dir_, true);

    InProcessBrowserTest::TearDown();
  }

  void WaitForServicesToStart(int num_expected_extensions,
                              bool expect_extensions_enabled) {
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(browser()->profile());

    // Count the number of non-component extensions.
    int found_extensions = 0;
    for (const scoped_refptr<const extensions::Extension>& extension :
         registry->enabled_extensions()) {
      if (extension->location() != extensions::Manifest::COMPONENT)
        found_extensions++;
    }

    if (!unauthenticated_load_allowed_)
      num_expected_extensions = 0;

    ASSERT_EQ(static_cast<uint32_t>(num_expected_extensions),
              static_cast<uint32_t>(found_extensions));

    ExtensionService* service = extensions::ExtensionSystem::Get(
                                    browser()->profile())->extension_service();
    ASSERT_EQ(expect_extensions_enabled, service->extensions_enabled());

    content::WindowedNotificationObserver user_scripts_observer(
        extensions::NOTIFICATION_USER_SCRIPTS_UPDATED,
        content::NotificationService::AllSources());
    extensions::SharedUserScriptMaster* master =
        extensions::ExtensionSystem::Get(browser()->profile())->
            shared_user_script_master();
    if (!master->scripts_ready())
      user_scripts_observer.Wait();
    ASSERT_TRUE(master->scripts_ready());
  }

  void TestInjection(bool expect_css, bool expect_script) {
    if (!unauthenticated_load_allowed_) {
      expect_css = false;
      expect_script = false;
    }

    // Load a page affected by the content script and test to see the effect.
    base::FilePath test_file;
    PathService::Get(chrome::DIR_TEST_DATA, &test_file);
    test_file = test_file.AppendASCII("extensions")
                         .AppendASCII("test_file.html");

    ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(test_file));

    bool result = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "window.domAutomationController.send("
        "    document.defaultView.getComputedStyle(document.body, null)."
        "    getPropertyValue('background-color') == 'rgb(245, 245, 220)')",
        &result));
    EXPECT_EQ(expect_css, result);

    result = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "window.domAutomationController.send(document.title == 'Modified')",
        &result));
    EXPECT_EQ(expect_script, result);
  }

  base::FilePath preferences_file_;
  base::FilePath extensions_dir_;
  base::FilePath user_scripts_dir_;
  // True unless unauthenticated extension settings are not allowed to be
  // loaded in this configuration.
  bool unauthenticated_load_allowed_;
  // Extensions to load from the command line.
  std::vector<base::FilePath::StringType> load_extensions_;

  int num_expected_extensions_;
};


// ExtensionsStartupTest
// Ensures that we can startup the browser with --enable-extensions and some
// extensions installed and see them run and do basic things.
typedef ExtensionStartupTestBase ExtensionsStartupTest;

// Broken in official builds, http://crbug.com/474659
IN_PROC_BROWSER_TEST_F(ExtensionsStartupTest, DISABLED_Test) {
  WaitForServicesToStart(num_expected_extensions_, true);
  TestInjection(true, true);
}

// Broken in official builds, http://crbug.com/474659
// Sometimes times out on Mac.  http://crbug.com/48151
// Tests that disallowing file access on an extension prevents it from injecting
// script into a page with a file URL.
IN_PROC_BROWSER_TEST_F(ExtensionsStartupTest, DISABLED_NoFileAccess) {
  WaitForServicesToStart(num_expected_extensions_, true);

  // Keep a separate list of extensions for which to disable file access, since
  // doing so reloads them.
  std::vector<const extensions::Extension*> extension_list;

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  for (extensions::ExtensionSet::const_iterator it =
           registry->enabled_extensions().begin();
       it != registry->enabled_extensions().end(); ++it) {
    if ((*it)->location() == extensions::Manifest::COMPONENT)
      continue;
    if (extensions::util::AllowFileAccess((*it)->id(), browser()->profile()))
      extension_list.push_back(it->get());
  }

  for (size_t i = 0; i < extension_list.size(); ++i) {
    content::WindowedNotificationObserver user_scripts_observer(
        extensions::NOTIFICATION_USER_SCRIPTS_UPDATED,
        content::NotificationService::AllSources());
    extensions::util::SetAllowFileAccess(
        extension_list[i]->id(), browser()->profile(), false);
    user_scripts_observer.Wait();
  }

  TestInjection(false, false);
}

// ExtensionsLoadTest
// Ensures that we can startup the browser with --load-extension and see them
// run.
class ExtensionsLoadTest : public ExtensionStartupTestBase {
 public:
  ExtensionsLoadTest() {
    base::FilePath one_extension_path;
    PathService::Get(chrome::DIR_TEST_DATA, &one_extension_path);
    one_extension_path = one_extension_path
        .AppendASCII("extensions")
        .AppendASCII("good")
        .AppendASCII("Extensions")
        .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
        .AppendASCII("1.0.0.0");
    load_extensions_.push_back(one_extension_path.value());
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionsLoadTest, Test) {
  WaitForServicesToStart(1, true);
  TestInjection(true, true);
}

// ExtensionsLoadMultipleTest
// Ensures that we can startup the browser with multiple extensions
// via --load-extension=X1,X2,X3.
class ExtensionsLoadMultipleTest : public ExtensionStartupTestBase {
 public:
  ExtensionsLoadMultipleTest() {
    base::FilePath one_extension_path;
    PathService::Get(chrome::DIR_TEST_DATA, &one_extension_path);
    one_extension_path = one_extension_path
        .AppendASCII("extensions")
        .AppendASCII("good")
        .AppendASCII("Extensions")
        .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
        .AppendASCII("1.0.0.0");
    load_extensions_.push_back(one_extension_path.value());

    base::FilePath second_extension_path;
    PathService::Get(chrome::DIR_TEST_DATA, &second_extension_path);
    second_extension_path = second_extension_path
        .AppendASCII("extensions")
        .AppendASCII("app");
    load_extensions_.push_back(second_extension_path.value());

    base::FilePath third_extension_path;
    PathService::Get(chrome::DIR_TEST_DATA, &third_extension_path);
    third_extension_path = third_extension_path
        .AppendASCII("extensions")
        .AppendASCII("app1");
    load_extensions_.push_back(third_extension_path.value());

    base::FilePath fourth_extension_path;
    PathService::Get(chrome::DIR_TEST_DATA, &fourth_extension_path);
    fourth_extension_path = fourth_extension_path
        .AppendASCII("extensions")
        .AppendASCII("app2");
    load_extensions_.push_back(fourth_extension_path.value());
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionsLoadMultipleTest, Test) {
  WaitForServicesToStart(4, true);
  TestInjection(true, true);
}
