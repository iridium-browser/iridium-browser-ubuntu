// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/settings/cros_settings_names.h"
#if defined(GOOGLE_CHROME_BUILD)
#include "components/spellcheck/browser/pref_names.h"
#endif
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"

namespace chromeos {

namespace {

// Because policy is not needed in this test it is better to use e-mails that
// are definitely not enterprise. This lets us to avoid faking of policy fetch
// procedure.
const char* kTestOwner = "test-owner@gmail.com";
const char* kTestNonOwner = "test-user1@gmail.com";

const char* kKnownSettings[] = {
  kDeviceOwner,
  kAccountsPrefAllowGuest,
  kAccountsPrefAllowNewUser,
  kAccountsPrefDeviceLocalAccounts,
  kAccountsPrefShowUserNamesOnSignIn,
  kAccountsPrefSupervisedUsersEnabled,
};

// Stub settings provider that only handles the settings we need to control.
// StubCrosSettingsProvider handles more settings but leaves many of them unset
// which the Settings page doesn't expect.
class StubAccountSettingsProvider : public StubCrosSettingsProvider {
 public:
  StubAccountSettingsProvider() {
  }

  ~StubAccountSettingsProvider() override {}

  // StubCrosSettingsProvider implementation.
  bool HandlesSetting(const std::string& path) const override {
    const char** end = kKnownSettings + arraysize(kKnownSettings);
    return std::find(kKnownSettings, end, path) != end;
  }
};

struct PrefTest {
  const char* pref_name;
  bool owner_only;
  bool indicator;
};

const PrefTest kPrefTests[] = {
  { kSystemTimezone, false, false },
  { prefs::kUse24HourClock, false, false },
  { kAttestationForContentProtectionEnabled, true, true },
  { kAccountsPrefAllowGuest, true, false },
  { kAccountsPrefAllowNewUser, true, false },
  { kAccountsPrefShowUserNamesOnSignIn, true, false },
  { kAccountsPrefSupervisedUsersEnabled, true, false },
#if defined(GOOGLE_CHROME_BUILD)
  { kStatsReportingPref, true, true },
  { spellcheck::prefs::kSpellCheckUseSpellingService, false, false },
#endif
};

}  // namespace

class SharedOptionsTest : public LoginManagerTest {
 public:
  SharedOptionsTest()
      : LoginManagerTest(false),
        stub_settings_provider_(base::MakeUnique<StubCrosSettingsProvider>()),
        stub_settings_provider_ptr_(static_cast<StubCrosSettingsProvider*>(
            stub_settings_provider_.get())),
        test_owner_account_id_(AccountId::FromUserEmail(kTestOwner)),
        test_non_owner_account_id_(AccountId::FromUserEmail(kTestNonOwner)) {
    stub_settings_provider_->Set(kDeviceOwner, base::StringValue(kTestOwner));
  }

  ~SharedOptionsTest() override {}

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();

    CrosSettings* settings = CrosSettings::Get();

    // Add the stub settings provider, moving the device settings provider
    // behind it so our stub takes precedence.
    std::unique_ptr<CrosSettingsProvider> device_settings_provider =
        settings->RemoveSettingsProvider(settings->GetProvider(kDeviceOwner));
    settings->AddSettingsProvider(std::move(stub_settings_provider_));
    settings->AddSettingsProvider(std::move(device_settings_provider));

    // Notify ChromeUserManager of ownership change.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_OWNERSHIP_STATUS_CHANGED,
        content::Source<SharedOptionsTest>(this),
        content::NotificationService::NoDetails());
  }

  void TearDownOnMainThread() override {
    CrosSettings* settings = CrosSettings::Get();
    settings->RemoveSettingsProvider(stub_settings_provider_ptr_);
    LoginManagerTest::TearDownOnMainThread();
  }

 protected:
  void CheckOptionsUI(const user_manager::User* user,
                      bool is_owner,
                      bool is_primary) {
    ASSERT_NE(nullptr, user);
    Browser* browser = CreateBrowserForUser(user);
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();

    for (size_t i = 0; i < sizeof(kPrefTests) / sizeof(kPrefTests[0]); i++) {
      bool disabled = !is_owner && kPrefTests[i].owner_only;
      if (strcmp(kPrefTests[i].pref_name, kSystemTimezone) == 0) {
        disabled = ProfileHelper::Get()
                       ->GetProfileByUser(user)
                       ->GetPrefs()
                       ->GetBoolean(prefs::kResolveTimezoneByGeolocation);
      }

      CheckPreference(
          contents, kPrefTests[i].pref_name, disabled,
          !is_owner && kPrefTests[i].indicator ? "owner" : std::string());
    }
    CheckBanner(contents, is_primary);
    CheckSharedSections(contents, is_primary);
    CheckAccountsOverlay(contents, is_owner);
  }

  // Creates a browser and navigates to the Settings page.
  Browser* CreateBrowserForUser(const user_manager::User* user) {
    Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
    SigninManagerFactory::GetForProfile(profile)->SetAuthenticatedAccountInfo(
        GetGaiaIDForUserID(user->GetAccountId().GetUserEmail()),
        user->GetAccountId().GetUserEmail());

    ui_test_utils::BrowserAddedObserver observer;
    Browser* browser = CreateBrowser(profile);
    observer.WaitForSingleNewBrowser();

    ui_test_utils::NavigateToURL(browser,
                                 GURL("chrome://settings-frame"));
    return browser;
  }

  // Verifies a preference's disabled state and controlled-by indicator.
  void CheckPreference(content::WebContents* contents,
                       std::string pref_name,
                       bool disabled,
                       std::string controlled_by) {
    bool success;
    std::string js_expression = base::StringPrintf(
        "var prefSelector = '[pref=\"%s\"]';"
        "var controlledBy = '%s';"
        "var input = document.querySelector("
        "    'input' + prefSelector + ', select' + prefSelector);"
        "var success = false;"
        "if (input) {"
        "  success = input.disabled == %d;"
        "  var indicator = input.parentNode.parentNode.querySelector("
        "      '.controlled-setting-indicator');"
        "  if (controlledBy) {"
        "    success = success && indicator &&"
        "              indicator.getAttribute('controlled-by') == controlledBy;"
        "  } else {"
        "    success = success && (!indicator ||"
        "              !indicator.hasAttribute('controlled-by') ||"
        "              indicator.getAttribute('controlled-by') == '')"
        "  }"
        "}"
        "window.domAutomationController.send(!!success);",
        pref_name.c_str(), controlled_by.c_str(), disabled);
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        contents, js_expression, &success));
    EXPECT_TRUE(success);
  }

  // Verifies a checkbox's disabled state, controlled-by indicator and value.
  void CheckBooleanPreference(content::WebContents* contents,
                              std::string pref_name,
                              bool disabled,
                              std::string controlled_by,
                              bool expected_value) {
    CheckPreference(contents, pref_name, disabled, controlled_by);
    bool actual_value;
    std::string js_expression = base::StringPrintf(
        "window.domAutomationController.send(document.querySelector('"
        "    input[type=\"checkbox\"][pref=\"%s\"]').checked);",
        pref_name.c_str());
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        contents, js_expression, &actual_value));
    EXPECT_EQ(expected_value, actual_value);
  }

  // Verifies that the shared settings banner is visible only for
  // secondary users.
  void CheckBanner(content::WebContents* contents,
                   bool is_primary) {
    bool banner_visible;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        contents,
        "var e = $('secondary-user-banner');"
        "window.domAutomationController.send(e && !e.hidden);",
        &banner_visible));
    EXPECT_EQ(!is_primary, banner_visible);
  }

  // Verifies that sections of shared settings have the appropriate indicator.
  void CheckSharedSections(content::WebContents* contents,
                           bool is_primary) {
    // This only applies to the Internet options section.
    std::string controlled_by;
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        contents,
        "var e = document.querySelector("
        "    '#network-section-header span.controlled-setting-indicator');"
        "if (!e || !e.getAttribute('controlled-by')) {"
        "  window.domAutomationController.send('');"
        "} else {"
        "  window.domAutomationController.send("
        "      e.getAttribute('controlled-by'));"
        "}",
        &controlled_by));
    EXPECT_EQ(!is_primary ? "shared" : std::string(), controlled_by);
  }

  // Checks the Accounts header and non-checkbox inputs.
  void CheckAccountsOverlay(content::WebContents* contents, bool is_owner) {
    // Set cros.accounts.allowGuest to false so we can test the accounts list.
    // This has to be done after the PRE_* test or we can't add the owner.
    stub_settings_provider_ptr_->Set(kAccountsPrefAllowNewUser,
                                     base::FundamentalValue(false));

    bool success;
    std::string js_expression = base::StringPrintf(
        "var controlled = %d;"
        "var warning = $('ownerOnlyWarning');"
        "var userList = $('userList');"
        "var input = $('userNameEdit');"
        "var success;"
        "if (controlled)"
        "  success = warning && !warning.hidden && userList.disabled &&"
        "            input.disabled;"
        "else"
        "  success = (!warning || warning.hidden) && !userList.disabled &&"
        "            !input.disabled;"
        "window.domAutomationController.send(!!success);",
        !is_owner);
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        contents, js_expression, &success));
    EXPECT_TRUE(success) << "Accounts overlay incorrect for " <<
        (is_owner ? "owner." : "non-owner.");
  }

  std::unique_ptr<CrosSettingsProvider> stub_settings_provider_;
  StubCrosSettingsProvider* stub_settings_provider_ptr_;

  const AccountId test_owner_account_id_;
  const AccountId test_non_owner_account_id_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedOptionsTest);
};

IN_PROC_BROWSER_TEST_F(SharedOptionsTest, PRE_SharedOptions) {
  RegisterUser(test_owner_account_id_.GetUserEmail());
  RegisterUser(test_non_owner_account_id_.GetUserEmail());
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(SharedOptionsTest, SharedOptions) {
  // Log in the owner first, then add a secondary user.
  LoginUser(test_owner_account_id_.GetUserEmail());
  UserAddingScreen::Get()->Start();
  content::RunAllPendingInMessageLoop();
  AddUser(test_non_owner_account_id_.GetUserEmail());

  user_manager::UserManager* manager = user_manager::UserManager::Get();
  ASSERT_EQ(2u, manager->GetLoggedInUsers().size());
  {
    SCOPED_TRACE("Checking settings for owner, primary user.");
    CheckOptionsUI(manager->FindUser(manager->GetOwnerAccountId()), true, true);
  }
  {
    SCOPED_TRACE("Checking settings for non-owner, secondary user.");
    CheckOptionsUI(manager->FindUser(test_non_owner_account_id_), false, false);
  }
  // TODO(michaelpg): Add tests for non-primary owner and primary non-owner
  // when the owner-only multiprofile restriction is removed, probably M38.
}

IN_PROC_BROWSER_TEST_F(SharedOptionsTest, PRE_ScreenLockPreferencePrimary) {
  RegisterUser(test_owner_account_id_.GetUserEmail());
  RegisterUser(test_non_owner_account_id_.GetUserEmail());
  StartupUtils::MarkOobeCompleted();
}

// Tests the shared setting indicator for the primary user's auto-lock setting
// when the secondary user has enabled or disabled their preference.
// (The checkbox is unset if the current user's preference is false, but if any
// other signed-in user has enabled this preference, the shared setting
// indicator explains this.)
IN_PROC_BROWSER_TEST_F(SharedOptionsTest, ScreenLockPreferencePrimary) {
  LoginUser(test_owner_account_id_.GetUserEmail());
  UserAddingScreen::Get()->Start();
  content::RunAllPendingInMessageLoop();
  AddUser(test_non_owner_account_id_.GetUserEmail());

  user_manager::UserManager* manager = user_manager::UserManager::Get();
  const user_manager::User* user1 = manager->FindUser(test_owner_account_id_);
  const user_manager::User* user2 =
      manager->FindUser(test_non_owner_account_id_);

  PrefService* prefs1 =
      ProfileHelper::Get()->GetProfileByUser(user1)->GetPrefs();
  PrefService* prefs2 =
      ProfileHelper::Get()->GetProfileByUser(user2)->GetPrefs();

  // Set both users' preference to false, then change the secondary user's to
  // true. We'll do the opposite in the next test. Doesn't provide 100% coverage
  // but reloading the settings page is super slow on debug builds.
  prefs1->SetBoolean(prefs::kEnableAutoScreenLock, false);
  prefs2->SetBoolean(prefs::kEnableAutoScreenLock, false);

  Browser* browser = CreateBrowserForUser(user1);
  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();

  bool disabled = false;
  bool expected_value;
  std::string empty_controlled;
  std::string shared_controlled("shared");

  {
    SCOPED_TRACE("Screen lock false for both users");
    expected_value = false;
    CheckBooleanPreference(contents, prefs::kEnableAutoScreenLock, disabled,
                           empty_controlled, expected_value);
  }

  // Set the secondary user's preference to true, and reload the primary user's
  // browser to see the updated controlled-by indicator.
  prefs2->SetBoolean(prefs::kEnableAutoScreenLock, true);
  chrome::Reload(browser, WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(contents);
  {
    SCOPED_TRACE("Screen lock false for primary user");
    expected_value = false;
    CheckBooleanPreference(contents, prefs::kEnableAutoScreenLock, disabled,
                           shared_controlled, expected_value);
  }

  // Set the preference to true for the primary user and check that the
  // indicator disappears.
  prefs1->SetBoolean(prefs::kEnableAutoScreenLock, true);
  {
    SCOPED_TRACE("Screen lock true for both users");
    expected_value = true;
    CheckBooleanPreference(contents, prefs::kEnableAutoScreenLock, disabled,
                           empty_controlled, expected_value);
  }
}

IN_PROC_BROWSER_TEST_F(SharedOptionsTest, PRE_ScreenLockPreferenceSecondary) {
  RegisterUser(test_owner_account_id_.GetUserEmail());
  RegisterUser(test_non_owner_account_id_.GetUserEmail());
  StartupUtils::MarkOobeCompleted();
}

// Tests the shared setting indicator for the secondary user's auto-lock setting
// when the primary user has enabled or disabled their preference.
// (The checkbox is unset if the current user's preference is false, but if any
// other signed-in user has enabled this preference, the shared setting
// indicator explains this.)
IN_PROC_BROWSER_TEST_F(SharedOptionsTest, ScreenLockPreferenceSecondary) {
  LoginUser(test_owner_account_id_.GetUserEmail());
  UserAddingScreen::Get()->Start();
  content::RunAllPendingInMessageLoop();
  AddUser(test_non_owner_account_id_.GetUserEmail());

  user_manager::UserManager* manager = user_manager::UserManager::Get();
  const user_manager::User* user1 = manager->FindUser(test_owner_account_id_);
  const user_manager::User* user2 =
      manager->FindUser(test_non_owner_account_id_);

  PrefService* prefs1 =
      ProfileHelper::Get()->GetProfileByUser(user1)->GetPrefs();
  PrefService* prefs2 =
      ProfileHelper::Get()->GetProfileByUser(user2)->GetPrefs();

  // Set both users' preference to true, then change the secondary user's to
  // false.
  prefs1->SetBoolean(prefs::kEnableAutoScreenLock, true);
  prefs2->SetBoolean(prefs::kEnableAutoScreenLock, true);

  Browser* browser = CreateBrowserForUser(user2);
  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();

  bool disabled = false;
  bool expected_value;
  std::string empty_controlled;
  std::string shared_controlled("shared");

  {
    SCOPED_TRACE("Screen lock true for both users");
    expected_value = true;
    CheckBooleanPreference(contents, prefs::kEnableAutoScreenLock, disabled,
                           empty_controlled, expected_value);
  }

  // Set the secondary user's preference to false and check that the
  // controlled-by indicator is shown.
  prefs2->SetBoolean(prefs::kEnableAutoScreenLock, false);
  {
    SCOPED_TRACE("Screen lock false for secondary user");
    expected_value = false;
    CheckBooleanPreference(contents, prefs::kEnableAutoScreenLock, disabled,
                           shared_controlled, expected_value);
  }

  // Set the preference to false for the primary user and check that the
  // indicator disappears.
  prefs1->SetBoolean(prefs::kEnableAutoScreenLock, false);
  chrome::Reload(browser, WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(contents);
  {
    SCOPED_TRACE("Screen lock false for both users");
    expected_value = false;
    CheckBooleanPreference(contents, prefs::kEnableAutoScreenLock, disabled,
                           empty_controlled, expected_value);
  }
}

}  // namespace chromeos
