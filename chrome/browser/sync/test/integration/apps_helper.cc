// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/apps_helper.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_app_helper.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_extension_helper.h"
#include "chrome/browser/sync/test/integration/sync_extension_installer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/manifest.h"

using sync_datatype_helper::test;

namespace {

std::string CreateFakeAppName(int index) {
  return "fakeapp" + base::IntToString(index);
}

}  // namespace

namespace apps_helper {

bool HasSameApps(Profile* profile1, Profile* profile2) {
  return SyncAppHelper::GetInstance()->AppStatesMatch(profile1, profile2);
}

bool AllProfilesHaveSameApps() {
  const auto& profiles = test()->GetAllProfiles();
  for (auto* profile : profiles) {
    if (profile != profiles.front() &&
        !HasSameApps(profiles.front(), profile)) {
      DVLOG(1) << "Profiles apps do not match.";
      return false;
    }
  }
  return true;
}

std::string InstallApp(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->InstallExtension(
      profile,
      CreateFakeAppName(index),
      extensions::Manifest::TYPE_HOSTED_APP);
}

std::string InstallPlatformApp(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->InstallExtension(
      profile,
      CreateFakeAppName(index),
      extensions::Manifest::TYPE_PLATFORM_APP);
}

std::string InstallAppForAllProfiles(int index) {
  std::string extension_id;
  for (auto* profile : test()->GetAllProfiles()) {
    extension_id = InstallApp(profile, index);
  }
  return extension_id;
}

void UninstallApp(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->UninstallExtension(
      profile, CreateFakeAppName(index));
}

void EnableApp(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->EnableExtension(
      profile, CreateFakeAppName(index));
}

void DisableApp(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->DisableExtension(
      profile, CreateFakeAppName(index));
}

bool IsAppEnabled(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->IsExtensionEnabled(
      profile, CreateFakeAppName(index));
}

void IncognitoEnableApp(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->IncognitoEnableExtension(
      profile, CreateFakeAppName(index));
}

void IncognitoDisableApp(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->IncognitoDisableExtension(
      profile, CreateFakeAppName(index));
}

bool IsIncognitoEnabled(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->IsIncognitoEnabled(
      profile, CreateFakeAppName(index));
}

void InstallAppsPendingForSync(Profile* profile) {
  SyncExtensionHelper::GetInstance()->InstallExtensionsPendingForSync(profile);
}

syncer::StringOrdinal GetPageOrdinalForApp(Profile* profile,
                                           int app_index) {
  return SyncAppHelper::GetInstance()->GetPageOrdinalForApp(
      profile, CreateFakeAppName(app_index));
}

void SetPageOrdinalForApp(Profile* profile,
                          int app_index,
                          const syncer::StringOrdinal& page_ordinal) {
  SyncAppHelper::GetInstance()->SetPageOrdinalForApp(
      profile, CreateFakeAppName(app_index), page_ordinal);
}

syncer::StringOrdinal GetAppLaunchOrdinalForApp(Profile* profile,
                                                int app_index) {
  return SyncAppHelper::GetInstance()->GetAppLaunchOrdinalForApp(
      profile, CreateFakeAppName(app_index));
}

void SetAppLaunchOrdinalForApp(
    Profile* profile,
    int app_index,
    const syncer::StringOrdinal& app_launch_ordinal) {
  SyncAppHelper::GetInstance()->SetAppLaunchOrdinalForApp(
      profile, CreateFakeAppName(app_index), app_launch_ordinal);
}

void CopyNTPOrdinals(Profile* source, Profile* destination, int index) {
  SetPageOrdinalForApp(destination, index, GetPageOrdinalForApp(source, index));
  SetAppLaunchOrdinalForApp(
      destination, index, GetAppLaunchOrdinalForApp(source, index));
}

void FixNTPOrdinalCollisions(Profile* profile) {
  SyncAppHelper::GetInstance()->FixNTPOrdinalCollisions(profile);
}

namespace {

// A helper class to implement waiting for a set of profiles to have matching
// extensions lists.
class AppsMatchChecker : public StatusChangeChecker,
                         public extensions::ExtensionRegistryObserver,
                         public extensions::ExtensionPrefsObserver,
                         public content::NotificationObserver {
 public:
  explicit AppsMatchChecker(const std::vector<Profile*>& profiles);
  ~AppsMatchChecker() override;

  // StatusChangeChecker implementation.
  std::string GetDebugMessage() const override;
  bool IsExitConditionSatisfied() override;

  // extensions::ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(
      content::BrowserContext* context,
      const extensions::Extension* extenion,
      extensions::UnloadedExtensionInfo::Reason reason) override;
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const extensions::Extension* extension,
                            bool is_update) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  // extensions::ExtensionPrefsObserver implementation.
  void OnExtensionDisableReasonsChanged(const std::string& extension_id,
                                        int disabled_reasons) override;
  void OnExtensionRegistered(const std::string& extension_id,
                             const base::Time& install_time,
                             bool is_enabled) override;
  void OnExtensionPrefsLoaded(const std::string& extension_id,
                              const extensions::ExtensionPrefs* prefs) override;
  void OnExtensionPrefsDeleted(const std::string& extension_id) override;
  void OnExtensionStateChanged(const std::string& extension_id,
                               bool state) override;

  // Implementation of content::NotificationObserver.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  void Wait();

 private:
  std::vector<Profile*> profiles_;
  bool observing_;

  content::NotificationRegistrar registrar_;

  // This installs apps, too.
  ScopedVector<SyncedExtensionInstaller> synced_extension_installers_;

  DISALLOW_COPY_AND_ASSIGN(AppsMatchChecker);
};

AppsMatchChecker::AppsMatchChecker(const std::vector<Profile*>& profiles)
    : profiles_(profiles), observing_(false) {
  DCHECK_GE(profiles_.size(), 2U);
}

AppsMatchChecker::~AppsMatchChecker() {
  if (observing_) {
    for (std::vector<Profile*>::iterator it = profiles_.begin();
         it != profiles_.end();
         ++it) {
      extensions::ExtensionRegistry* registry =
          extensions::ExtensionRegistry::Get(*it);
      registry->RemoveObserver(this);
      extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(*it);
      prefs->RemoveObserver(this);
    }
  }
}

std::string AppsMatchChecker::GetDebugMessage() const {
  return "Waiting for apps to match";
}

bool AppsMatchChecker::IsExitConditionSatisfied() {
  std::vector<Profile*>::iterator it = profiles_.begin();
  Profile* profile0 = *it;
  ++it;
  for (; it != profiles_.end(); ++it) {
    if (!SyncAppHelper::GetInstance()->AppStatesMatch(profile0, *it)) {
      return false;
    }
  }
  return true;
}

void AppsMatchChecker::OnExtensionLoaded(
    content::BrowserContext* context,
    const extensions::Extension* extension) {
  CheckExitCondition();
}

void AppsMatchChecker::OnExtensionUnloaded(
    content::BrowserContext* context,
    const extensions::Extension* extenion,
    extensions::UnloadedExtensionInfo::Reason reason) {
  CheckExitCondition();
}

void AppsMatchChecker::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    bool is_update) {
  CheckExitCondition();
}

void AppsMatchChecker::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  CheckExitCondition();
}

void AppsMatchChecker::OnExtensionDisableReasonsChanged(
    const std::string& extension_id,
    int disabled_reasons) {
  CheckExitCondition();
}

void AppsMatchChecker::OnExtensionRegistered(const std::string& extension_id,
                                             const base::Time& install_time,
                                             bool is_enabled) {
  CheckExitCondition();
}

void AppsMatchChecker::OnExtensionPrefsLoaded(
    const std::string& extension_id,
    const extensions::ExtensionPrefs* prefs) {
  CheckExitCondition();
}

void AppsMatchChecker::OnExtensionPrefsDeleted(
    const std::string& extension_id) {
  CheckExitCondition();
}

void AppsMatchChecker::OnExtensionStateChanged(const std::string& extension_id,
                                               bool state) {
  CheckExitCondition();
}

void AppsMatchChecker::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_APP_LAUNCHER_REORDERED, type);
  CheckExitCondition();
}

void AppsMatchChecker::Wait() {
  for (std::vector<Profile*>::iterator it = profiles_.begin();
       it != profiles_.end();
       ++it) {
    // Begin mocking the installation of synced extensions from the web store.
    synced_extension_installers_.push_back(new SyncedExtensionInstaller(*it));

    // Register as an observer of ExtensionsRegistry to receive notifications of
    // big events, like installs and uninstalls.
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(*it);
    registry->AddObserver(this);

    // Register for ExtensionPrefs events, too, so we can get notifications
    // about
    // smaller but still syncable events, like launch type changes.
    extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(*it);
    prefs->AddObserver(this);
  }

  registrar_.Add(this,
                 chrome::NOTIFICATION_APP_LAUNCHER_REORDERED,
                 content::NotificationService::AllSources());

  observing_ = true;

  if (IsExitConditionSatisfied()) {
    DVLOG(1) << "Apps matched without waiting";
    return;
  }

  DVLOG(1) << "Starting Wait: " << GetDebugMessage();
  StartBlockingWait();
}

}  // namespace

bool AwaitAllProfilesHaveSameApps() {
  std::vector<Profile*> profiles;
  if (test()->use_verifier()) {
    profiles.push_back(test()->verifier());
  }
  for (int i = 0; i < test()->num_clients(); ++i) {
    profiles.push_back(test()->GetProfile(i));
  }

  AppsMatchChecker checker(profiles);
  checker.Wait();
  return !checker.TimedOut();
}

}  // namespace apps_helper
