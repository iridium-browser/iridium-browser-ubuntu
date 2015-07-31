// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profiles_state.h"

#include "base/files/file_path.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/profiles/gaia_info_update_service.h"
#include "chrome/browser/profiles/gaia_info_update_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"

namespace profiles {

bool IsMultipleProfilesEnabled() {
#if defined(OS_ANDROID)
  return false;
#endif
  return true;
}

base::FilePath GetDefaultProfileDir(const base::FilePath& user_data_dir) {
  base::FilePath default_profile_dir(user_data_dir);
  default_profile_dir =
      default_profile_dir.AppendASCII(chrome::kInitialProfile);
  return default_profile_dir;
}

void RegisterPrefs(PrefRegistrySimple* registry) {
  // Preferences about global profile information.
  registry->RegisterStringPref(prefs::kProfileLastUsed, std::string());
  registry->RegisterIntegerPref(prefs::kProfilesNumCreated, 1);
  registry->RegisterListPref(prefs::kProfilesLastActive);

  // Preferences about the user manager.
  registry->RegisterBooleanPref(prefs::kBrowserGuestModeEnabled, true);
  registry->RegisterBooleanPref(prefs::kBrowserAddPersonEnabled, true);

  registry->RegisterBooleanPref(
      prefs::kProfileAvatarRightClickTutorialDismissed, false);
}

base::string16 GetAvatarNameForProfile(const base::FilePath& profile_path) {
  base::string16 display_name;

  if (profile_path == ProfileManager::GetGuestProfilePath()) {
    display_name = l10n_util::GetStringUTF16(IDS_GUEST_PROFILE_NAME);
  } else {
    const ProfileInfoCache& cache =
        g_browser_process->profile_manager()->GetProfileInfoCache();
    size_t index = cache.GetIndexOfProfileWithPath(profile_path);

    if (index == std::string::npos)
      return l10n_util::GetStringUTF16(IDS_SINGLE_PROFILE_DISPLAY_NAME);

    // Using the --new-avatar-menu flag, there's a couple of rules about what
    // the avatar button displays. If there's a single profile, with a default
    // name (i.e. of the form Person %d) not manually set, it should display
    // IDS_SINGLE_PROFILE_DISPLAY_NAME. If the profile is signed in but is using
    // a default name, use the profiles's email address. Otherwise, it
    // will return the actual name of the profile.
    const base::string16 profile_name = cache.GetNameOfProfileAtIndex(index);
    const base::string16 email = cache.GetUserNameOfProfileAtIndex(index);
    bool is_default_name = cache.ProfileIsUsingDefaultNameAtIndex(index) &&
        cache.IsDefaultProfileName(profile_name);

    if (cache.GetNumberOfProfiles() == 1 && is_default_name)
      display_name = l10n_util::GetStringUTF16(IDS_SINGLE_PROFILE_DISPLAY_NAME);
    else
      display_name = (is_default_name && !email.empty()) ? email : profile_name;
  }
  return display_name;
}

base::string16 GetAvatarButtonTextForProfile(Profile* profile) {
  const int kMaxCharactersToDisplay = 15;
  base::string16 name = GetAvatarNameForProfile(profile->GetPath());
  name = gfx::TruncateString(name,
                             kMaxCharactersToDisplay,
                             gfx::CHARACTER_BREAK);
  if (profile->IsLegacySupervised()) {
    name = l10n_util::GetStringFUTF16(IDS_SUPERVISED_USER_NEW_AVATAR_LABEL,
                                      name);
  }
  return name;
}

base::string16 GetProfileSwitcherTextForItem(const AvatarMenu::Item& item) {
  if (item.legacy_supervised) {
    return l10n_util::GetStringFUTF16(IDS_SUPERVISED_USER_NEW_AVATAR_LABEL,
                                      item.name);
  }
  if (item.child_account)
    return l10n_util::GetStringFUTF16(IDS_CHILD_AVATAR_LABEL, item.name);
  return item.name;
}

void UpdateProfileName(Profile* profile,
                       const base::string16& new_profile_name) {
  const ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile->GetPath());
  if (profile_index == std::string::npos)
    return;

  if (new_profile_name == cache.GetNameOfProfileAtIndex(profile_index))
    return;

  // This is only called when updating the profile name through the UI,
  // so we can assume the user has done this on purpose.
  PrefService* pref_service = profile->GetPrefs();
  pref_service->SetBoolean(prefs::kProfileUsingDefaultName, false);

  // Updating the profile preference will cause the cache to be updated for
  // this preference.
  pref_service->SetString(prefs::kProfileName,
                          base::UTF16ToUTF8(new_profile_name));
}

std::vector<std::string> GetSecondaryAccountsForProfile(
    Profile* profile,
    const std::string& primary_account) {
  std::vector<std::string> accounts =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile)->GetAccounts();

  // The vector returned by ProfileOAuth2TokenService::GetAccounts() contains
  // the primary account too, so we need to remove it from the list.
  std::vector<std::string>::iterator primary_index =
      std::find_if(accounts.begin(), accounts.end(),
                   std::bind1st(std::equal_to<std::string>(), primary_account));
  DCHECK(primary_index != accounts.end());
  accounts.erase(primary_index);

  return accounts;
}

bool IsRegularOrGuestSession(Browser* browser) {
  Profile* profile = browser->profile();
  return profile->IsGuestSession() || !profile->IsOffTheRecord();
}

bool IsProfileLocked(Profile* profile) {
  const ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile->GetPath());

  if (profile_index == std::string::npos)
    return false;

  return cache.ProfileIsSigninRequiredAtIndex(profile_index);
}

void UpdateIsProfileLockEnabledIfNeeded(Profile* profile) {
  DCHECK(switches::IsNewProfileManagement());

  if (!profile->GetPrefs()->GetString(prefs::kGoogleServicesHostedDomain).
      empty())
    return;

  UpdateGaiaProfileInfoIfNeeded(profile);
}

void UpdateGaiaProfileInfoIfNeeded(Profile* profile) {
  // If the --google-profile-info flag isn't used, then the
  // GAIAInfoUpdateService isn't initialized, and we can't download the profile
  // info.
  if (!switches::IsGoogleProfileInfo())
    return;

  DCHECK(profile);

  GAIAInfoUpdateService* service =
      GAIAInfoUpdateServiceFactory::GetInstance()->GetForProfile(profile);
  // The service may be null, for example during unit tests.
  if (service)
    service->Update();
}

SigninErrorController* GetSigninErrorController(Profile* profile) {
  return SigninErrorControllerFactory::GetForProfile(profile);
}

bool SetActiveProfileToGuestIfLocked() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  const base::FilePath& active_profile_path =
      profile_manager->GetLastUsedProfileDir(profile_manager->user_data_dir());
  const base::FilePath& guest_path = ProfileManager::GetGuestProfilePath();
  if (active_profile_path == guest_path)
    return true;

  const ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  size_t index = cache.GetIndexOfProfileWithPath(active_profile_path);
  if (!cache.ProfileIsSigninRequiredAtIndex(index))
    return false;

  SetLastUsedProfile(guest_path.BaseName().MaybeAsASCII());

  return true;
}

void RemoveBrowsingDataForProfile(const base::FilePath& profile_path) {
  // The BrowsingDataRemover relies on the ResourceDispatcherHost, which is
  // null in unit tests.
  if (!content::ResourceDispatcherHost::Get())
    return;

  Profile* profile = g_browser_process->profile_manager()->GetProfileByPath(
      profile_path);
  if (!profile)
    return;

  // For guest the browsing data is in the OTR profile.
  if (profile->IsGuestSession())
    profile = profile->GetOffTheRecordProfile();

  BrowsingDataRemover::CreateForUnboundedRange(profile)->Remove(
      BrowsingDataRemover::REMOVE_ALL, BrowsingDataHelper::ALL);
  // BrowsingDataRemover deletes itself.
}

void SetLastUsedProfile(const std::string& profile_dir) {
  // We should never be saving the System Profile as the last one used since it
  // shouldn't have a browser.
  if (profile_dir == base::FilePath(chrome::kSystemProfileDir).AsUTF8Unsafe())
    return;

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  local_state->SetString(prefs::kProfileLastUsed, profile_dir);
}

}  // namespace profiles
