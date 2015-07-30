// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_CHROME_USER_MANAGER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_CHROME_USER_MANAGER_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/easy_unlock/bootstrap_manager.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_manager_impl.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller_delegate.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_policy_observer.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "components/user_manager/user.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class PrefRegistrySimple;
class PrefService;
class ProfileSyncService;

namespace policy {
struct DeviceLocalAccount;
}

namespace user_manager {
class RemoveUserDelegate;
}

namespace chromeos {

class MultiProfileUserController;
class SupervisedUserManagerImpl;
class SessionLengthLimiter;

// Chrome specific implementation of the UserManager.
class ChromeUserManagerImpl
    : public ChromeUserManager,
      public content::NotificationObserver,
      public policy::CloudExternalDataPolicyObserver::Delegate,
      public policy::DeviceLocalAccountPolicyService::Observer,
      public MultiProfileUserControllerDelegate,
      public BootstrapManager::Delegate {
 public:
  ~ChromeUserManagerImpl() override;

  // Creates ChromeUserManagerImpl instance.
  static scoped_ptr<ChromeUserManager> CreateChromeUserManager();

  // Registers user manager preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // UserManagerInterface implementation:
  BootstrapManager* GetBootstrapManager() override;
  MultiProfileUserController* GetMultiProfileUserController() override;
  UserImageManager* GetUserImageManager(const std::string& user_id) override;
  SupervisedUserManager* GetSupervisedUserManager() override;
  UserFlow* GetCurrentUserFlow() const override;
  UserFlow* GetUserFlow(const std::string& user_id) const override;
  void SetUserFlow(const std::string& user_id, UserFlow* flow) override;
  void ResetUserFlow(const std::string& user_id) override;

  // UserManager implementation:
  void Shutdown() override;
  user_manager::UserList GetUsersAllowedForMultiProfile() const override;
  user_manager::UserList GetUsersAllowedForSupervisedUsersCreation()
      const override;
  user_manager::UserList GetUnlockUsers() const override;
  void SessionStarted() override;
  void SaveUserOAuthStatus(
      const std::string& user_id,
      user_manager::User::OAuthTokenStatus oauth_token_status) override;
  void SaveUserDisplayName(const std::string& user_id,
                           const base::string16& display_name) override;
  bool CanCurrentUserLock() const override;
  bool IsUserNonCryptohomeDataEphemeral(
      const std::string& user_id) const override;
  bool AreSupervisedUsersAllowed() const override;

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // policy::CloudExternalDataPolicyObserver::Delegate:
  void OnExternalDataSet(const std::string& policy,
                         const std::string& user_id) override;
  void OnExternalDataCleared(const std::string& policy,
                             const std::string& user_id) override;
  void OnExternalDataFetched(const std::string& policy,
                             const std::string& user_id,
                             scoped_ptr<std::string> data) override;

  // policy::DeviceLocalAccountPolicyService::Observer implementation.
  void OnPolicyUpdated(const std::string& user_id) override;
  void OnDeviceLocalAccountsChanged() override;

  void StopPolicyObserverForTesting();

  // UserManagerBase implementation:
  bool AreEphemeralUsersEnabled() const override;

 protected:
  const std::string& GetApplicationLocale() const override;
  PrefService* GetLocalState() const override;
  void HandleUserOAuthTokenStatusChange(
      const std::string& user_id,
      user_manager::User::OAuthTokenStatus status) const override;
  bool IsEnterpriseManaged() const override;
  void LoadPublicAccounts(std::set<std::string>* users_set) override;
  void NotifyOnLogin() override;
  void NotifyUserAddedToSession(const user_manager::User* added_user,
                                bool user_switch_pending) override;
  void PerformPreUserListLoadingActions() override;
  void PerformPostUserListLoadingActions() override;
  void PerformPostUserLoggedInActions(bool browser_restart) override;
  void RemoveNonCryptohomeData(const std::string& user_id) override;
  void RemoveUserInternal(const std::string& user_email,
                          user_manager::RemoveUserDelegate* delegate) override;
  bool IsDemoApp(const std::string& user_id) const override;
  bool IsKioskApp(const std::string& user_id) const override;
  bool IsPublicAccountMarkedForRemoval(
      const std::string& user_id) const override;
  void DemoAccountLoggedIn() override;
  void GuestUserLoggedIn() override;
  void KioskAppLoggedIn(const std::string& app_id) override;
  void PublicAccountUserLoggedIn(user_manager::User* user) override;
  void RegularUserLoggedIn(const std::string& user_id) override;
  void RegularUserLoggedInAsEphemeral(const std::string& user_id) override;
  void SupervisedUserLoggedIn(const std::string& user_id) override;
  bool HasPendingBootstrap(const std::string& user_id) const override;

 private:
  friend class SupervisedUserManagerImpl;
  friend class UserManagerTest;
  friend class WallpaperManager;
  friend class WallpaperManagerTest;

  typedef base::hash_map<std::string, linked_ptr<UserImageManager> >
      UserImageManagerMap;

  ChromeUserManagerImpl();

  // Retrieves trusted device policies and removes users from the persistent
  // list if ephemeral users are enabled. Schedules a callback to itself if
  // trusted device policies are not yet available.
  void RetrieveTrustedDevicePolicies();

  // Updates current user ownership on UI thread.
  void UpdateOwnership();

  // If data for a public account is marked as pending removal and the user is
  // no longer logged into that account, removes the data.
  void CleanUpPublicAccountNonCryptohomeDataPendingRemoval();

  // Removes data belonging to public accounts that are no longer found on the
  // user list. If the user is currently logged into one of these accounts, the
  // data for that account is not removed immediately but marked as pending
  // removal after logout.
  void CleanUpPublicAccountNonCryptohomeData(
      const std::vector<std::string>& old_public_accounts);

  // Replaces the list of public accounts with those found in
  // |device_local_accounts|. Ensures that data belonging to accounts no longer
  // on the list is removed. Returns |true| if the list has changed.
  // Public accounts are defined by policy. This method is called whenever an
  // updated list of public accounts is received from policy.
  bool UpdateAndCleanUpPublicAccounts(
      const std::vector<policy::DeviceLocalAccount>& device_local_accounts);

  // Updates the display name for public account |username| from policy settings
  // associated with that username.
  void UpdatePublicAccountDisplayName(const std::string& user_id);

  // Notifies the UI about a change to the user list.
  void NotifyUserListChanged();

  // Lazily creates default user flow.
  UserFlow* GetDefaultUserFlow() const;

  // MultiProfileUserControllerDelegate implementation:
  void OnUserNotAllowed(const std::string& user_email) override;

  // BootstrapManager::Delegate implementation:
  void RemovePendingBootstrapUser(const std::string& user_id) override;

  // Update the number of users.
  void UpdateNumberOfUsers();

  // Starts (or stops) automatic timezone refresh on geolocation,
  // depending on user preferences.
  void UpdateUserTimeZoneRefresher(Profile* profile);

  // Interface to the signed settings store.
  CrosSettings* cros_settings_;

  // Interface to device-local account definitions and associated policy.
  policy::DeviceLocalAccountPolicyService* device_local_account_policy_service_;

  content::NotificationRegistrar registrar_;

  // User avatar managers.
  UserImageManagerMap user_image_managers_;

  // Supervised user manager.
  scoped_ptr<SupervisedUserManagerImpl> supervised_user_manager_;

  // Session length limiter.
  scoped_ptr<SessionLengthLimiter> session_length_limiter_;

  typedef std::map<std::string, UserFlow*> FlowMap;

  // Lazy-initialized default flow.
  mutable scoped_ptr<UserFlow> default_flow_;

  // Specific flows by user e-mail. Keys should be canonicalized before
  // access.
  FlowMap specific_flows_;

  scoped_ptr<CrosSettings::ObserverSubscription> local_accounts_subscription_;

  scoped_ptr<MultiProfileUserController> multi_profile_user_controller_;

  // Observer for the policy that can be used to manage user images.
  scoped_ptr<policy::CloudExternalDataPolicyObserver> avatar_policy_observer_;

  // Observer for the policy that can be used to manage wallpapers.
  scoped_ptr<policy::CloudExternalDataPolicyObserver>
      wallpaper_policy_observer_;

  scoped_ptr<BootstrapManager> bootstrap_manager_;

  base::WeakPtrFactory<ChromeUserManagerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeUserManagerImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_CHROME_USER_MANAGER_IMPL_H_
