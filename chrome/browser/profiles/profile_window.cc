// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_window.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/pref_service_flags_storage.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system.h"
#endif  // defined(ENABLE_EXTENSIONS)

#if !defined(OS_IOS)
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#endif  // !defined (OS_IOS)

using base::UserMetricsAction;
using content::BrowserThread;

namespace {

const char kNewProfileManagementExperimentInternalName[] =
    "enable-new-profile-management";

#if defined(ENABLE_EXTENSIONS)
void BlockExtensions(Profile* profile) {
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  extension_service->BlockAllExtensions();
}

void UnblockExtensions(Profile* profile) {
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  extension_service->UnblockAllExtensions();
}
#endif  // defined(ENABLE_EXTENSIONS)

// Handles running a callback when a new Browser for the given profile
// has been completely created.
class BrowserAddedForProfileObserver : public chrome::BrowserListObserver {
 public:
  BrowserAddedForProfileObserver(
      Profile* profile,
      ProfileManager::CreateCallback callback)
      : profile_(profile),
        callback_(callback) {
    DCHECK(!callback_.is_null());
    BrowserList::AddObserver(this);
  }
  ~BrowserAddedForProfileObserver() override {}

 private:
  // Overridden from BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override {
    if (browser->profile() == profile_) {
      BrowserList::RemoveObserver(this);
      callback_.Run(profile_, Profile::CREATE_STATUS_INITIALIZED);
      base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
    }
  }

  // Profile for which the browser should be opened.
  Profile* profile_;
  ProfileManager::CreateCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAddedForProfileObserver);
};

void OpenBrowserWindowForProfile(
    ProfileManager::CreateCallback callback,
    bool always_create,
    bool is_new_profile,
    chrome::HostDesktopType desktop_type,
    Profile* profile,
    Profile::CreateStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (status != Profile::CREATE_STATUS_INITIALIZED)
    return;

  chrome::startup::IsProcessStartup is_process_startup =
      chrome::startup::IS_NOT_PROCESS_STARTUP;
  chrome::startup::IsFirstRun is_first_run = chrome::startup::IS_NOT_FIRST_RUN;

  // If this is a brand new profile, then start a first run window.
  if (is_new_profile) {
    is_process_startup = chrome::startup::IS_PROCESS_STARTUP;
    is_first_run = chrome::startup::IS_FIRST_RUN;
  }

#if defined(ENABLE_EXTENSIONS)
  // The signin bit will still be set if the profile is being unlocked and the
  // browser window for it is opening. As part of this unlock process, unblock
  // all the extensions.
  const ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  int index = cache.GetIndexOfProfileWithPath(profile->GetPath());
  if (!profile->IsGuestSession() &&
      cache.ProfileIsSigninRequiredAtIndex(index)) {
    UnblockExtensions(profile);
  }
#endif  // defined(ENABLE_EXTENSIONS)

  // If |always_create| is false, and we have a |callback| to run, check
  // whether a browser already exists so that we can run the callback. We don't
  // want to rely on the observer listening to OnBrowserSetLastActive in this
  // case, as you could manually activate an incorrect browser and trigger
  // a false positive.
  if (!always_create) {
    Browser* browser = chrome::FindTabbedBrowser(profile, false, desktop_type);
    if (browser) {
      browser->window()->Activate();
      if (!callback.is_null())
        callback.Run(profile, Profile::CREATE_STATUS_INITIALIZED);
      return;
    }
  }

  // If there is a callback, create an observer to make sure it is only
  // run when the browser has been completely created. This observer will
  // delete itself once that happens. This should not leak, because we are
  // passing |always_create| = true to FindOrCreateNewWindow below, which ends
  // up calling LaunchBrowser and opens a new window. If for whatever reason
  // that fails, either something has crashed, or the observer will be cleaned
  // up when a different browser for this profile is opened.
  if (!callback.is_null())
    new BrowserAddedForProfileObserver(profile, callback);

  // We already dealt with the case when |always_create| was false and a browser
  // existed, which means that here a browser definitely needs to be created.
  // Passing true for |always_create| means we won't duplicate the code that
  // tries to find a browser.
  profiles::FindOrCreateNewWindowForProfile(
      profile,
      is_process_startup,
      is_first_run,
      desktop_type,
      true);
}

// Called after a |system_profile| is available to be used by the user manager.
// Based on the value of |tutorial_mode| we determine a url to be displayed
// by the webui and run the |callback|, if it exists. After opening a profile,
// perform |profile_open_action|.
void OnUserManagerSystemProfileCreated(
    const base::FilePath& profile_path_to_focus,
    profiles::UserManagerTutorialMode tutorial_mode,
    profiles::UserManagerProfileSelected profile_open_action,
    const base::Callback<void(Profile*, const std::string&)>& callback,
    Profile* system_profile,
    Profile::CreateStatus status) {
  if (status != Profile::CREATE_STATUS_INITIALIZED || callback.is_null())
    return;

  // Tell the webui which user should be focused.
  std::string page = chrome::kChromeUIUserManagerURL;

  if (tutorial_mode == profiles::USER_MANAGER_TUTORIAL_OVERVIEW) {
    page += profiles::kUserManagerDisplayTutorial;
  } else if (!profile_path_to_focus.empty()) {
    const ProfileInfoCache& cache =
        g_browser_process->profile_manager()->GetProfileInfoCache();
    size_t index = cache.GetIndexOfProfileWithPath(profile_path_to_focus);
    if (index != std::string::npos) {
      page += "#";
      page += base::IntToString(index);
    }
  } else if (profile_open_action ==
             profiles::USER_MANAGER_SELECT_PROFILE_TASK_MANAGER) {
    page += profiles::kUserManagerSelectProfileTaskManager;
  } else if (profile_open_action ==
             profiles::USER_MANAGER_SELECT_PROFILE_ABOUT_CHROME) {
    page += profiles::kUserManagerSelectProfileAboutChrome;
  } else if (profile_open_action ==
             profiles::USER_MANAGER_SELECT_PROFILE_CHROME_SETTINGS) {
    page += profiles::kUserManagerSelectProfileChromeSettings;
  } else if (profile_open_action ==
             profiles::USER_MANAGER_SELECT_PROFILE_CHROME_MEMORY) {
    page += profiles::kUserManagerSelectProfileChromeMemory;
  } else if (profile_open_action ==
             profiles::USER_MANAGER_SELECT_PROFILE_APP_LAUNCHER) {
    page += profiles::kUserManagerSelectProfileAppLauncher;
  }
  callback.Run(system_profile, page);
}

// Updates Chrome services that require notification when
// the new_profile_management's status changes.
void UpdateServicesWithNewProfileManagementFlag(Profile* profile,
                                                bool new_flag_status) {
  AccountReconcilor* account_reconcilor =
      AccountReconcilorFactory::GetForProfile(profile);
  account_reconcilor->OnNewProfileManagementFlagChanged(new_flag_status);
}

}  // namespace

namespace profiles {

// User Manager parameters are prefixed with hash.
const char kUserManagerDisplayTutorial[] = "#tutorial";
const char kUserManagerSelectProfileTaskManager[] = "#task-manager";
const char kUserManagerSelectProfileAboutChrome[] = "#about-chrome";
const char kUserManagerSelectProfileChromeSettings[] = "#chrome-settings";
const char kUserManagerSelectProfileChromeMemory[] = "#chrome-memory";
const char kUserManagerSelectProfileAppLauncher[] = "#app-launcher";

void FindOrCreateNewWindowForProfile(
    Profile* profile,
    chrome::startup::IsProcessStartup process_startup,
    chrome::startup::IsFirstRun is_first_run,
    chrome::HostDesktopType desktop_type,
    bool always_create) {
#if defined(OS_IOS)
  NOTREACHED();
#else
  DCHECK(profile);

  if (!always_create) {
    Browser* browser = chrome::FindTabbedBrowser(profile, false, desktop_type);
    if (browser) {
      browser->window()->Activate();
      return;
    }
  }

  content::RecordAction(UserMetricsAction("NewWindow"));
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreator browser_creator;
  browser_creator.LaunchBrowser(
      command_line, profile, base::FilePath(), process_startup, is_first_run);
#endif  // defined(OS_IOS)
}

void SwitchToProfile(const base::FilePath& path,
                     chrome::HostDesktopType desktop_type,
                     bool always_create,
                     ProfileManager::CreateCallback callback,
                     ProfileMetrics::ProfileOpen metric) {
  ProfileMetrics::LogProfileSwitch(metric,
                                   g_browser_process->profile_manager(),
                                   path);
  g_browser_process->profile_manager()->CreateProfileAsync(
      path,
      base::Bind(&OpenBrowserWindowForProfile,
                 callback,
                 always_create,
                 false,
                 desktop_type),
      base::string16(),
      base::string16(),
      std::string());
}

void SwitchToGuestProfile(chrome::HostDesktopType desktop_type,
                          ProfileManager::CreateCallback callback) {
  const base::FilePath& path = ProfileManager::GetGuestProfilePath();
  ProfileMetrics::LogProfileSwitch(ProfileMetrics::SWITCH_PROFILE_GUEST,
                                   g_browser_process->profile_manager(),
                                   path);
  g_browser_process->profile_manager()->CreateProfileAsync(
      path,
      base::Bind(&OpenBrowserWindowForProfile,
                 callback,
                 false,
                 false,
                 desktop_type),
      base::string16(),
      base::string16(),
      std::string());
}

bool HasProfileSwitchTargets(Profile* profile) {
  size_t min_profiles = profile->IsGuestSession() ? 1 : 2;
  size_t number_of_profiles =
      g_browser_process->profile_manager()->GetNumberOfProfiles();
  return number_of_profiles >= min_profiles;
}

void CreateAndSwitchToNewProfile(chrome::HostDesktopType desktop_type,
                                 ProfileManager::CreateCallback callback,
                                 ProfileMetrics::ProfileAdd metric) {
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();

  int placeholder_avatar_index = profiles::GetPlaceholderAvatarIndex();
  ProfileManager::CreateMultiProfileAsync(
      cache.ChooseNameForNewProfile(placeholder_avatar_index),
      base::UTF8ToUTF16(profiles::GetDefaultAvatarIconUrl(
          placeholder_avatar_index)),
      base::Bind(&OpenBrowserWindowForProfile,
                 callback,
                 true,
                 true,
                 desktop_type),
      std::string());
  ProfileMetrics::LogProfileAddNewUser(metric);
}

void GuestBrowserCloseSuccess(const base::FilePath& profile_path) {
  UserManager::Show(base::FilePath(),
                    profiles::USER_MANAGER_NO_TUTORIAL,
                    profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
}

void CloseGuestProfileWindows() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetProfileByPath(
      ProfileManager::GetGuestProfilePath());

  if (profile) {
    BrowserList::CloseAllBrowsersWithProfile(
        profile, base::Bind(&GuestBrowserCloseSuccess));
  }
}

void LockBrowserCloseSuccess(const base::FilePath& profile_path) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileInfoCache* cache = &profile_manager->GetProfileInfoCache();

  cache->SetProfileSigninRequiredAtIndex(
      cache->GetIndexOfProfileWithPath(profile_path), true);

#if defined(ENABLE_EXTENSIONS)
  // Profile guaranteed to exist for it to have been locked.
  BlockExtensions(profile_manager->GetProfileByPath(profile_path));
#endif  // defined(ENABLE_EXTENSIONS)

  chrome::HideTaskManager();
  UserManager::Show(profile_path,
                    profiles::USER_MANAGER_NO_TUTORIAL,
                    profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
}

void LockProfile(Profile* profile) {
  DCHECK(profile);
  if (profile) {
    BrowserList::CloseAllBrowsersWithProfile(
        profile, base::Bind(&LockBrowserCloseSuccess));
  }
}

bool IsLockAvailable(Profile* profile) {
  DCHECK(profile);
  if (!switches::IsNewProfileManagement())
    return false;

  if (profile->IsGuestSession() || profile->IsSystemProfile())
    return false;

  std::string hosted_domain = profile->GetPrefs()->
      GetString(prefs::kGoogleServicesHostedDomain);
  // TODO(mlerman): After one release remove any hosted_domain reference to the
  // pref, since all users will have this in the AccountTrackerService.
  if (hosted_domain.empty()) {
    AccountTrackerService* account_tracker =
        AccountTrackerServiceFactory::GetForProfile(profile);
    std::string account_id =
      SigninManagerFactory::GetForProfile(profile)->GetAuthenticatedAccountId();
    hosted_domain = account_tracker->GetAccountInfo(account_id).hosted_domain;
  }
  // TODO(mlerman): Prohibit only users who authenticate using SAML. Until then,
  // prohibited users who use hosted domains (aside from google.com).
  if (hosted_domain != Profile::kNoHostedDomainFound &&
      hosted_domain != "google.com") {
    return false;
  }

  const ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  for (size_t i = 0; i < cache.GetNumberOfProfiles(); ++i) {
    if (cache.ProfileIsSupervisedAtIndex(i))
      return true;
  }
  return false;
}

void CreateSystemProfileForUserManager(
    const base::FilePath& profile_path_to_focus,
    profiles::UserManagerTutorialMode tutorial_mode,
    profiles::UserManagerProfileSelected profile_open_action,
    const base::Callback<void(Profile*, const std::string&)>& callback) {
  // Create the system profile, if necessary, and open the User Manager
  // from the system profile.
  g_browser_process->profile_manager()->CreateProfileAsync(
      ProfileManager::GetSystemProfilePath(),
      base::Bind(&OnUserManagerSystemProfileCreated,
                 profile_path_to_focus,
                 tutorial_mode,
                 profile_open_action,
                 callback),
      base::string16(),
      base::string16(),
      std::string());
}

void ShowUserManagerMaybeWithTutorial(Profile* profile) {
  // Guest users cannot appear in the User Manager, nor display a tutorial.
  if (!profile || profile->IsGuestSession()) {
    UserManager::Show(base::FilePath(),
                      profiles::USER_MANAGER_NO_TUTORIAL,
                      profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
    return;
  }
  UserManager::Show(base::FilePath(),
                    profiles::USER_MANAGER_TUTORIAL_OVERVIEW,
                    profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
}

void EnableNewProfileManagementPreview(Profile* profile) {
#if defined(OS_ANDROID)
  NOTREACHED();
#else
  // TODO(rogerta): instead of setting experiment flags and command line
  // args, we should set a profile preference.
  const about_flags::Experiment experiment = {
      kNewProfileManagementExperimentInternalName,
      0,  // string id for title of experiment
      0,  // string id for description of experiment
      0,  // supported platforms
      about_flags::Experiment::ENABLE_DISABLE_VALUE,
      switches::kEnableNewProfileManagement,
      "",  // not used with ENABLE_DISABLE_VALUE type
      switches::kDisableNewProfileManagement,
      "",  // not used with ENABLE_DISABLE_VALUE type
      NULL,  // not used with ENABLE_DISABLE_VALUE type
      3
  };
  about_flags::PrefServiceFlagsStorage flags_storage(
      g_browser_process->local_state());
  about_flags::SetExperimentEnabled(
      &flags_storage,
      experiment.NameForChoice(1),
      true);

  switches::EnableNewProfileManagementForTesting(
      base::CommandLine::ForCurrentProcess());
  UserManager::Show(base::FilePath(),
                    profiles::USER_MANAGER_TUTORIAL_OVERVIEW,
                    profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
  UpdateServicesWithNewProfileManagementFlag(profile, true);
#endif
}

void DisableNewProfileManagementPreview(Profile* profile) {
  about_flags::PrefServiceFlagsStorage flags_storage(
      g_browser_process->local_state());
  about_flags::SetExperimentEnabled(
      &flags_storage,
      kNewProfileManagementExperimentInternalName,
      false);
  chrome::AttemptRestart();
  UpdateServicesWithNewProfileManagementFlag(profile, false);
}

void BubbleViewModeFromAvatarBubbleMode(
    BrowserWindow::AvatarBubbleMode mode,
    BubbleViewMode* bubble_view_mode,
    TutorialMode* tutorial_mode) {
  *tutorial_mode = TUTORIAL_MODE_NONE;
  switch (mode) {
    case BrowserWindow::AVATAR_BUBBLE_MODE_ACCOUNT_MANAGEMENT:
      *bubble_view_mode = BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT;
      return;
    case BrowserWindow::AVATAR_BUBBLE_MODE_SIGNIN:
      *bubble_view_mode = BUBBLE_VIEW_MODE_GAIA_SIGNIN;
      return;
    case BrowserWindow::AVATAR_BUBBLE_MODE_ADD_ACCOUNT:
      *bubble_view_mode = BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT;
      return;
    case BrowserWindow::AVATAR_BUBBLE_MODE_REAUTH:
      *bubble_view_mode = BUBBLE_VIEW_MODE_GAIA_REAUTH;
      return;
    case BrowserWindow::AVATAR_BUBBLE_MODE_CONFIRM_SIGNIN:
      *bubble_view_mode = BUBBLE_VIEW_MODE_PROFILE_CHOOSER;
      *tutorial_mode = TUTORIAL_MODE_CONFIRM_SIGNIN;
      return;
    case BrowserWindow::AVATAR_BUBBLE_MODE_SHOW_ERROR:
      *bubble_view_mode = BUBBLE_VIEW_MODE_PROFILE_CHOOSER;
      *tutorial_mode = TUTORIAL_MODE_SHOW_ERROR;
      return;
    case BrowserWindow::AVATAR_BUBBLE_MODE_FAST_USER_SWITCH:
      *bubble_view_mode = profiles::BUBBLE_VIEW_MODE_FAST_PROFILE_CHOOSER;
      return;
    default:
      *bubble_view_mode = profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER;
  }
}

bool ShouldShowWelcomeUpgradeTutorial(
    Profile* profile, TutorialMode tutorial_mode) {
  const int show_count = profile->GetPrefs()->GetInteger(
      prefs::kProfileAvatarTutorialShown);
  // Do not show the tutorial if user has dismissed it.
  if (show_count > signin_ui_util::kUpgradeWelcomeTutorialShowMax)
    return false;

  return tutorial_mode == TUTORIAL_MODE_WELCOME_UPGRADE ||
         show_count != signin_ui_util::kUpgradeWelcomeTutorialShowMax;
}

bool ShouldShowRightClickTutorial(Profile* profile) {
  PrefService* local_state = g_browser_process->local_state();
  const bool dismissed = local_state->GetBoolean(
      prefs::kProfileAvatarRightClickTutorialDismissed);

  // Don't show the tutorial if it's already been dismissed or if right-clicking
  // wouldn't show any targets.
  return !dismissed && HasProfileSwitchTargets(profile);
}

}  // namespace profiles
