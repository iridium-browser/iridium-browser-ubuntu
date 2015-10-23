// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/chrome_user_manager_impl.h"

#include <cstddef>
#include <set>

#include "ash/multi_profile_uma.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_app_launcher.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/signin/auth_sync_observer.h"
#include "chrome/browser/chromeos/login/signin/auth_sync_observer_factory.h"
#include "chrome/browser/chromeos/login/users/affiliation.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_manager_impl.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager_impl.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/profiles/multiprofiles_session_aborted_dialog.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/session_length_limiter.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "chrome/browser/supervised_user/chromeos/manager_password_service_factory.h"
#include "chrome/browser/supervised_user/chromeos/supervised_user_password_service_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/theme_resources.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/user_names.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/timezone/timezone_resolver.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/remove_user_delegate.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "policy/policy_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/wm/core/wm_core_switches.h"

using content::BrowserThread;

namespace chromeos {
namespace {

// A vector pref of the the regular users known on this device, arranged in LRU
// order.
const char kRegularUsers[] = "LoggedInUsers";

// A vector pref of the public accounts defined on this device.
const char kPublicAccounts[] = "PublicAccounts";

// Key for list of users that should be reported.
const char kReportingUsers[] = "reporting_users";

// A string pref that gets set when a public account is removed but a user is
// currently logged into that account, requiring the account's data to be
// removed after logout.
const char kPublicAccountPendingDataRemoval[] =
    "PublicAccountPendingDataRemoval";

bool FakeOwnership() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kStubCrosSettings);
}

std::string FullyCanonicalize(const std::string& email) {
  return gaia::CanonicalizeEmail(gaia::SanitizeEmail(email));
}

}  // namespace

// static
void ChromeUserManagerImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  ChromeUserManager::RegisterPrefs(registry);

  registry->RegisterListPref(kPublicAccounts);
  registry->RegisterStringPref(kPublicAccountPendingDataRemoval, std::string());
  registry->RegisterListPref(kReportingUsers);

  SupervisedUserManager::RegisterPrefs(registry);
  SessionLengthLimiter::RegisterPrefs(registry);
  BootstrapManager::RegisterPrefs(registry);
}

// static
scoped_ptr<ChromeUserManager> ChromeUserManagerImpl::CreateChromeUserManager() {
  return scoped_ptr<ChromeUserManager>(new ChromeUserManagerImpl());
}

ChromeUserManagerImpl::ChromeUserManagerImpl()
    : ChromeUserManager(base::ThreadTaskRunnerHandle::Get(),
                        BrowserThread::GetBlockingPool()),
      cros_settings_(CrosSettings::Get()),
      device_local_account_policy_service_(NULL),
      supervised_user_manager_(new SupervisedUserManagerImpl(this)),
      bootstrap_manager_(new BootstrapManager(this)),
      weak_factory_(this) {
  UpdateNumberOfUsers();

  // UserManager instance should be used only on UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  registrar_.Add(this,
                 chrome::NOTIFICATION_OWNERSHIP_STATUS_CHANGED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_CREATED,
                 content::NotificationService::AllSources());

  // Since we're in ctor postpone any actions till this is fully created.
  if (base::MessageLoop::current()) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ChromeUserManagerImpl::RetrieveTrustedDevicePolicies,
                   weak_factory_.GetWeakPtr()));
  }

  local_accounts_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefDeviceLocalAccounts,
      base::Bind(&ChromeUserManagerImpl::RetrieveTrustedDevicePolicies,
                 weak_factory_.GetWeakPtr()));
  multi_profile_user_controller_.reset(
      new MultiProfileUserController(this, GetLocalState()));

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  avatar_policy_observer_.reset(new policy::CloudExternalDataPolicyObserver(
      cros_settings_,
      connector->GetDeviceLocalAccountPolicyService(),
      policy::key::kUserAvatarImage,
      this));
  avatar_policy_observer_->Init();

  wallpaper_policy_observer_.reset(new policy::CloudExternalDataPolicyObserver(
      cros_settings_,
      connector->GetDeviceLocalAccountPolicyService(),
      policy::key::kWallpaperImage,
      this));
  wallpaper_policy_observer_->Init();
}

ChromeUserManagerImpl::~ChromeUserManagerImpl() {
}

void ChromeUserManagerImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ChromeUserManager::Shutdown();

  local_accounts_subscription_.reset();

  // Stop the session length limiter.
  session_length_limiter_.reset();

  if (device_local_account_policy_service_)
    device_local_account_policy_service_->RemoveObserver(this);

  for (UserImageManagerMap::iterator it = user_image_managers_.begin(),
                                     ie = user_image_managers_.end();
       it != ie;
       ++it) {
    it->second->Shutdown();
  }
  multi_profile_user_controller_.reset();
  avatar_policy_observer_.reset();
  wallpaper_policy_observer_.reset();
  registrar_.RemoveAll();
}

BootstrapManager* ChromeUserManagerImpl::GetBootstrapManager() {
  return bootstrap_manager_.get();
}

MultiProfileUserController*
ChromeUserManagerImpl::GetMultiProfileUserController() {
  return multi_profile_user_controller_.get();
}

UserImageManager* ChromeUserManagerImpl::GetUserImageManager(
    const std::string& user_id) {
  UserImageManagerMap::iterator ui = user_image_managers_.find(user_id);
  if (ui != user_image_managers_.end())
    return ui->second.get();
  linked_ptr<UserImageManagerImpl> mgr(new UserImageManagerImpl(user_id, this));
  user_image_managers_[user_id] = mgr;
  return mgr.get();
}

SupervisedUserManager* ChromeUserManagerImpl::GetSupervisedUserManager() {
  return supervised_user_manager_.get();
}

user_manager::UserList ChromeUserManagerImpl::GetUsersAllowedForMultiProfile()
    const {
  // Supervised users are not allowed to use multi-profiles.
  if (GetLoggedInUsers().size() == 1 &&
      GetPrimaryUser()->GetType() != user_manager::USER_TYPE_REGULAR) {
    return user_manager::UserList();
  }

  user_manager::UserList result;
  const user_manager::UserList& users = GetUsers();
  for (user_manager::UserList::const_iterator it = users.begin();
       it != users.end();
       ++it) {
    if ((*it)->GetType() == user_manager::USER_TYPE_REGULAR &&
        !(*it)->is_logged_in()) {
      MultiProfileUserController::UserAllowedInSessionReason check;
      multi_profile_user_controller_->IsUserAllowedInSession((*it)->email(),
                                                             &check);
      if (check ==
          MultiProfileUserController::NOT_ALLOWED_PRIMARY_USER_POLICY_FORBIDS) {
        return user_manager::UserList();
      }

      // Users with a policy that prevents them being added to a session will be
      // shown in login UI but will be grayed out.
      // Same applies to owner account (see http://crbug.com/385034).
      result.push_back(*it);
    }
  }

  return result;
}

user_manager::UserList
ChromeUserManagerImpl::GetUsersAllowedForSupervisedUsersCreation() const {
  CrosSettings* cros_settings = CrosSettings::Get();
  bool allow_new_user = true;
  cros_settings->GetBoolean(kAccountsPrefAllowNewUser, &allow_new_user);
  bool supervised_users_allowed = AreSupervisedUsersAllowed();

  // Restricted either by policy or by owner.
  if (!allow_new_user || !supervised_users_allowed)
    return user_manager::UserList();

  return GetUsersAllowedAsSupervisedUserManagers(GetUsers());
}

user_manager::UserList ChromeUserManagerImpl::GetUnlockUsers() const {
  const user_manager::UserList& logged_in_users = GetLoggedInUsers();
  if (logged_in_users.empty())
    return user_manager::UserList();

  user_manager::UserList unlock_users;
  Profile* profile =
      ProfileHelper::Get()->GetProfileByUserUnsafe(GetPrimaryUser());
  std::string primary_behavior =
      profile->GetPrefs()->GetString(prefs::kMultiProfileUserBehavior);

  // Specific case: only one logged in user or
  // primary user has primary-only multi-profile policy.
  if (logged_in_users.size() == 1 ||
      primary_behavior == MultiProfileUserController::kBehaviorPrimaryOnly) {
    if (GetPrimaryUser()->can_lock())
      unlock_users.push_back(primary_user_);
  } else {
    // Fill list of potential unlock users based on multi-profile policy state.
    for (user_manager::UserList::const_iterator it = logged_in_users.begin();
         it != logged_in_users.end();
         ++it) {
      user_manager::User* user = (*it);
      Profile* profile = ProfileHelper::Get()->GetProfileByUserUnsafe(user);
      const std::string behavior =
          profile->GetPrefs()->GetString(prefs::kMultiProfileUserBehavior);
      if (behavior == MultiProfileUserController::kBehaviorUnrestricted &&
          user->can_lock()) {
        unlock_users.push_back(user);
      } else if (behavior == MultiProfileUserController::kBehaviorPrimaryOnly) {
        NOTREACHED()
            << "Spotted primary-only multi-profile policy for non-primary user";
      }
    }
  }

  return unlock_users;
}

void ChromeUserManagerImpl::SessionStarted() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ChromeUserManager::SessionStarted();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::Source<UserManager>(this),
      content::Details<const user_manager::User>(GetActiveUser()));
}

void ChromeUserManagerImpl::RemoveUserInternal(
    const std::string& user_email,
    user_manager::RemoveUserDelegate* delegate) {
  CrosSettings* cros_settings = CrosSettings::Get();

  const base::Closure& callback =
      base::Bind(&ChromeUserManagerImpl::RemoveUserInternal,
                 weak_factory_.GetWeakPtr(),
                 user_email,
                 delegate);

  // Ensure the value of owner email has been fetched.
  if (CrosSettingsProvider::TRUSTED !=
      cros_settings->PrepareTrustedValues(callback)) {
    // Value of owner email is not fetched yet.  RemoveUserInternal will be
    // called again after fetch completion.
    return;
  }
  std::string owner;
  cros_settings->GetString(kDeviceOwner, &owner);
  if (user_email == owner) {
    // Owner is not allowed to be removed from the device.
    return;
  }
  RemoveNonOwnerUserInternal(user_email, delegate);
}

void ChromeUserManagerImpl::SaveUserOAuthStatus(
    const std::string& user_id,
    user_manager::User::OAuthTokenStatus oauth_token_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ChromeUserManager::SaveUserOAuthStatus(user_id, oauth_token_status);

  GetUserFlow(user_id)->HandleOAuthTokenStatusChange(oauth_token_status);
}

void ChromeUserManagerImpl::SaveUserDisplayName(
    const std::string& user_id,
    const base::string16& display_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ChromeUserManager::SaveUserDisplayName(user_id, display_name);

  // Do not update local state if data stored or cached outside the user's
  // cryptohome is to be treated as ephemeral.
  if (!IsUserNonCryptohomeDataEphemeral(user_id))
    supervised_user_manager_->UpdateManagerName(user_id, display_name);
}

void ChromeUserManagerImpl::StopPolicyObserverForTesting() {
  avatar_policy_observer_.reset();
  wallpaper_policy_observer_.reset();
}

void ChromeUserManagerImpl::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_OWNERSHIP_STATUS_CHANGED:
      if (!device_local_account_policy_service_) {
        policy::BrowserPolicyConnectorChromeOS* connector =
            g_browser_process->platform_part()
                ->browser_policy_connector_chromeos();
        device_local_account_policy_service_ =
            connector->GetDeviceLocalAccountPolicyService();
        if (device_local_account_policy_service_)
          device_local_account_policy_service_->AddObserver(this);
      }
      RetrieveTrustedDevicePolicies();
      UpdateOwnership();
      break;
    case chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED: {
      Profile* profile = content::Details<Profile>(details).ptr();
      if (IsUserLoggedIn() && !IsLoggedInAsGuest() && !IsLoggedInAsKioskApp()) {
        if (IsLoggedInAsSupervisedUser())
          SupervisedUserPasswordServiceFactory::GetForProfile(profile);
        if (IsLoggedInAsUserWithGaiaAccount())
          ManagerPasswordServiceFactory::GetForProfile(profile);

        if (!profile->IsOffTheRecord()) {
          AuthSyncObserver* sync_observer =
              AuthSyncObserverFactory::GetInstance()->GetForProfile(profile);
          sync_observer->StartObserving();
          multi_profile_user_controller_->StartObserving(profile);
        }
      }
      UpdateUserTimeZoneRefresher(profile);
      break;
    }
    case chrome::NOTIFICATION_PROFILE_CREATED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      user_manager::User* user =
          ProfileHelper::Get()->GetUserByProfile(profile);
      if (user != NULL) {
        user->set_profile_is_created();

        if (user->HasGaiaAccount()) {
          UserImageManager* image_manager = GetUserImageManager(user->email());
          image_manager->UserProfileCreated();
        }
      }

      // If there is pending user switch, do it now.
      if (!GetPendingUserSwitchID().empty()) {
        // Call SwitchActiveUser async because otherwise it may cause
        // ProfileManager::GetProfile before the profile gets registered
        // in ProfileManager. It happens in case of sync profile load when
        // NOTIFICATION_PROFILE_CREATED is called synchronously.
        base::MessageLoop::current()->PostTask(
            FROM_HERE,
            base::Bind(&ChromeUserManagerImpl::SwitchActiveUser,
                       weak_factory_.GetWeakPtr(),
                       GetPendingUserSwitchID()));
        SetPendingUserSwitchID(std::string());
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void ChromeUserManagerImpl::OnExternalDataSet(const std::string& policy,
                                              const std::string& user_id) {
  if (policy == policy::key::kUserAvatarImage)
    GetUserImageManager(user_id)->OnExternalDataSet(policy);
  else if (policy == policy::key::kWallpaperImage)
    WallpaperManager::Get()->OnPolicySet(policy, user_id);
  else
    NOTREACHED();
}

void ChromeUserManagerImpl::OnExternalDataCleared(const std::string& policy,
                                                  const std::string& user_id) {
  if (policy == policy::key::kUserAvatarImage)
    GetUserImageManager(user_id)->OnExternalDataCleared(policy);
  else if (policy == policy::key::kWallpaperImage)
    WallpaperManager::Get()->OnPolicyCleared(policy, user_id);
  else
    NOTREACHED();
}

void ChromeUserManagerImpl::OnExternalDataFetched(
    const std::string& policy,
    const std::string& user_id,
    scoped_ptr<std::string> data) {
  if (policy == policy::key::kUserAvatarImage)
    GetUserImageManager(user_id)->OnExternalDataFetched(policy, data.Pass());
  else if (policy == policy::key::kWallpaperImage)
    WallpaperManager::Get()->OnPolicyFetched(policy, user_id, data.Pass());
  else
    NOTREACHED();
}

void ChromeUserManagerImpl::OnPolicyUpdated(const std::string& user_id) {
  const user_manager::User* user = FindUser(user_id);
  if (!user || user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT)
    return;
  UpdatePublicAccountDisplayName(user_id);
}

void ChromeUserManagerImpl::OnDeviceLocalAccountsChanged() {
  // No action needed here, changes to the list of device-local accounts get
  // handled via the kAccountsPrefDeviceLocalAccounts device setting observer.
}

bool ChromeUserManagerImpl::CanCurrentUserLock() const {
  return ChromeUserManager::CanCurrentUserLock() &&
         GetCurrentUserFlow()->CanLockScreen();
}

bool ChromeUserManagerImpl::IsUserNonCryptohomeDataEphemeral(
    const std::string& user_id) const {
  // Data belonging to the obsolete public accounts whose data has not been
  // removed yet is not ephemeral.
  bool is_obsolete_public_account = IsPublicAccountMarkedForRemoval(user_id);

  return !is_obsolete_public_account &&
         ChromeUserManager::IsUserNonCryptohomeDataEphemeral(user_id);
}

bool ChromeUserManagerImpl::AreEphemeralUsersEnabled() const {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return GetEphemeralUsersEnabled() &&
         (connector->IsEnterpriseManaged() || !GetOwnerEmail().empty());
}

void ChromeUserManagerImpl::OnUserRemoved(const std::string& user_id) {
  RemoveReportingUser(FullyCanonicalize(user_id));
}

const std::string& ChromeUserManagerImpl::GetApplicationLocale() const {
  return g_browser_process->GetApplicationLocale();
}

PrefService* ChromeUserManagerImpl::GetLocalState() const {
  return g_browser_process ? g_browser_process->local_state() : NULL;
}

void ChromeUserManagerImpl::HandleUserOAuthTokenStatusChange(
    const std::string& user_id,
    user_manager::User::OAuthTokenStatus status) const {
  GetUserFlow(user_id)->HandleOAuthTokenStatusChange(status);
}

bool ChromeUserManagerImpl::IsEnterpriseManaged() const {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->IsEnterpriseManaged();
}

void ChromeUserManagerImpl::LoadPublicAccounts(
    std::set<std::string>* public_sessions_set) {
  const base::ListValue* prefs_public_sessions =
      GetLocalState()->GetList(kPublicAccounts);
  std::vector<std::string> public_sessions;
  ParseUserList(*prefs_public_sessions,
                std::set<std::string>(),
                &public_sessions,
                public_sessions_set);
  for (std::vector<std::string>::const_iterator it = public_sessions.begin();
       it != public_sessions.end();
       ++it) {
    users_.push_back(user_manager::User::CreatePublicAccountUser(*it));
    UpdatePublicAccountDisplayName(*it);
  }
}

void ChromeUserManagerImpl::PerformPreUserListLoadingActions() {
  // Clean up user list first. All code down the path should be synchronous,
  // so that local state after transaction rollback is in consistent state.
  // This process also should not trigger EnsureUsersLoaded again.
  if (supervised_user_manager_->HasFailedUserCreationTransaction())
    supervised_user_manager_->RollbackUserCreationTransaction();

  // Abandon all unfinished bootstraps.
  bootstrap_manager_->RemoveAllPendingBootstrap();
}

void ChromeUserManagerImpl::PerformPostUserListLoadingActions() {
  for (user_manager::UserList::iterator ui = users_.begin(), ue = users_.end();
       ui != ue;
       ++ui) {
    GetUserImageManager((*ui)->email())->LoadUserImage();
  }
}

void ChromeUserManagerImpl::PerformPostUserLoggedInActions(
    bool browser_restart) {
  // Initialize the session length limiter and start it only if
  // session limit is defined by the policy.
  session_length_limiter_.reset(
      new SessionLengthLimiter(NULL, browser_restart));
}

bool ChromeUserManagerImpl::IsDemoApp(const std::string& user_id) const {
  return DemoAppLauncher::IsDemoAppSession(user_id);
}

bool ChromeUserManagerImpl::IsKioskApp(const std::string& user_id) const {
  policy::DeviceLocalAccount::Type device_local_account_type;
  return policy::IsDeviceLocalAccountUser(user_id,
                                          &device_local_account_type) &&
         device_local_account_type ==
             policy::DeviceLocalAccount::TYPE_KIOSK_APP;
}

bool ChromeUserManagerImpl::IsPublicAccountMarkedForRemoval(
    const std::string& user_id) const {
  return user_id ==
         GetLocalState()->GetString(kPublicAccountPendingDataRemoval);
}

void ChromeUserManagerImpl::RetrieveTrustedDevicePolicies() {
  // Local state may not be initialized in unit_tests.
  if (!GetLocalState())
    return;

  SetEphemeralUsersEnabled(false);
  SetOwnerEmail(std::string());

  // Schedule a callback if device policy has not yet been verified.
  if (CrosSettingsProvider::TRUSTED !=
      cros_settings_->PrepareTrustedValues(
          base::Bind(&ChromeUserManagerImpl::RetrieveTrustedDevicePolicies,
                     weak_factory_.GetWeakPtr()))) {
    return;
  }

  bool ephemeral_users_enabled = false;
  cros_settings_->GetBoolean(kAccountsPrefEphemeralUsersEnabled,
                             &ephemeral_users_enabled);
  SetEphemeralUsersEnabled(ephemeral_users_enabled);

  std::string owner_email;
  cros_settings_->GetString(kDeviceOwner, &owner_email);
  SetOwnerEmail(owner_email);

  EnsureUsersLoaded();

  bool changed = UpdateAndCleanUpPublicAccounts(
      policy::GetDeviceLocalAccounts(cros_settings_));

  // If ephemeral users are enabled and we are on the login screen, take this
  // opportunity to clean up by removing all regular users except the owner.
  if (GetEphemeralUsersEnabled() && !IsUserLoggedIn()) {
    ListPrefUpdate prefs_users_update(GetLocalState(), kRegularUsers);
    prefs_users_update->Clear();
    for (user_manager::UserList::iterator it = users_.begin();
         it != users_.end();) {
      const std::string user_email = (*it)->email();
      if ((*it)->HasGaiaAccount() && user_email != GetOwnerEmail()) {
        RemoveNonCryptohomeData(user_email);
        DeleteUser(*it);
        it = users_.erase(it);
        changed = true;
      } else {
        if ((*it)->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT)
          prefs_users_update->Append(new base::StringValue(user_email));
        ++it;
      }
    }
  }

  if (changed)
    NotifyUserListChanged();
}

void ChromeUserManagerImpl::GuestUserLoggedIn() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ChromeUserManager::GuestUserLoggedIn();

  // TODO(nkostylev): Add support for passing guest session cryptohome
  // mount point. Legacy (--login-profile) value will be used for now.
  // http://crosbug.com/230859
  active_user_->SetStubImage(
      user_manager::UserImage(
          *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_PROFILE_PICTURE_LOADING)),
      user_manager::User::USER_IMAGE_INVALID,
      false);

  // Initializes wallpaper after active_user_ is set.
  WallpaperManager::Get()->SetUserWallpaperNow(chromeos::login::kGuestUserName);
}

void ChromeUserManagerImpl::RegularUserLoggedIn(const std::string& user_id) {
  ChromeUserManager::RegularUserLoggedIn(user_id);

  if (FakeOwnership()) {
    std::string owner_email = GetActiveUser()->email();
    VLOG(1) << "Set device owner to: " << owner_email;
    CrosSettings::Get()->SetString(kDeviceOwner, owner_email);
    SetOwnerEmail(owner_email);
  }

  if (IsCurrentUserNew())
    WallpaperManager::Get()->SetUserWallpaperNow(user_id);

  GetUserImageManager(user_id)->UserLoggedIn(IsCurrentUserNew(), false);

  WallpaperManager::Get()->EnsureLoggedInUserWallpaperLoaded();

  // Make sure that new data is persisted to Local State.
  GetLocalState()->CommitPendingWrite();
}

void ChromeUserManagerImpl::RegularUserLoggedInAsEphemeral(
    const std::string& user_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ChromeUserManager::RegularUserLoggedInAsEphemeral(user_id);

  GetUserImageManager(user_id)->UserLoggedIn(IsCurrentUserNew(), false);
  WallpaperManager::Get()->SetUserWallpaperNow(user_id);
}

void ChromeUserManagerImpl::SupervisedUserLoggedIn(const std::string& user_id) {
  // TODO(nkostylev): Refactor, share code with RegularUserLoggedIn().

  // Remove the user from the user list.
  active_user_ = RemoveRegularOrSupervisedUserFromList(user_id);

  // If the user was not found on the user list, create a new user.
  if (!GetActiveUser()) {
    SetIsCurrentUserNew(true);
    active_user_ = user_manager::User::CreateSupervisedUser(user_id);
    // Leaving OAuth token status at the default state = unknown.
    WallpaperManager::Get()->SetUserWallpaperNow(user_id);
  } else {
    if (supervised_user_manager_->CheckForFirstRun(user_id)) {
      SetIsCurrentUserNew(true);
      WallpaperManager::Get()->SetUserWallpaperNow(user_id);
    } else {
      SetIsCurrentUserNew(false);
    }
  }

  // Add the user to the front of the user list.
  ListPrefUpdate prefs_users_update(GetLocalState(), kRegularUsers);
  prefs_users_update->Insert(0, new base::StringValue(user_id));
  users_.insert(users_.begin(), active_user_);

  // Now that user is in the list, save display name.
  if (IsCurrentUserNew()) {
    SaveUserDisplayName(GetActiveUser()->email(),
                        GetActiveUser()->GetDisplayName());
  }

  GetUserImageManager(user_id)->UserLoggedIn(IsCurrentUserNew(), true);
  WallpaperManager::Get()->EnsureLoggedInUserWallpaperLoaded();

  // Make sure that new data is persisted to Local State.
  GetLocalState()->CommitPendingWrite();
}

bool ChromeUserManagerImpl::HasPendingBootstrap(
    const std::string& user_id) const {
  return bootstrap_manager_->HasPendingBootstrap(user_id);
}

void ChromeUserManagerImpl::PublicAccountUserLoggedIn(
    user_manager::User* user) {
  SetIsCurrentUserNew(true);
  active_user_ = user;

  // The UserImageManager chooses a random avatar picture when a user logs in
  // for the first time. Tell the UserImageManager that this user is not new to
  // prevent the avatar from getting changed.
  GetUserImageManager(user->email())->UserLoggedIn(false, true);
  WallpaperManager::Get()->EnsureLoggedInUserWallpaperLoaded();
}

void ChromeUserManagerImpl::KioskAppLoggedIn(const std::string& app_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  policy::DeviceLocalAccount::Type device_local_account_type;
  DCHECK(policy::IsDeviceLocalAccountUser(app_id, &device_local_account_type));
  DCHECK_EQ(policy::DeviceLocalAccount::TYPE_KIOSK_APP,
            device_local_account_type);

  active_user_ = user_manager::User::CreateKioskAppUser(app_id);
  active_user_->SetStubImage(
      user_manager::UserImage(
          *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_PROFILE_PICTURE_LOADING)),
      user_manager::User::USER_IMAGE_INVALID,
      false);

  WallpaperManager::Get()->SetUserWallpaperNow(app_id);

  // TODO(bartfab): Add KioskAppUsers to the users_ list and keep metadata like
  // the kiosk_app_id in these objects, removing the need to re-parse the
  // device-local account list here to extract the kiosk_app_id.
  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(cros_settings_);
  const policy::DeviceLocalAccount* account = NULL;
  for (std::vector<policy::DeviceLocalAccount>::const_iterator it =
           device_local_accounts.begin();
       it != device_local_accounts.end();
       ++it) {
    if (it->user_id == app_id) {
      account = &*it;
      break;
    }
  }
  std::string kiosk_app_id;
  if (account) {
    kiosk_app_id = account->kiosk_app_id;
  } else {
    LOG(ERROR) << "Logged into nonexistent kiosk-app account: " << app_id;
    NOTREACHED();
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(::switches::kForceAppMode);
  command_line->AppendSwitchASCII(::switches::kAppId, kiosk_app_id);

  // Disable window animation since kiosk app runs in a single full screen
  // window and window animation causes start-up janks.
  command_line->AppendSwitch(wm::switches::kWindowAnimationsDisabled);
}

void ChromeUserManagerImpl::DemoAccountLoggedIn() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  active_user_ =
      user_manager::User::CreateKioskAppUser(DemoAppLauncher::kDemoUserName);
  active_user_->SetStubImage(
      user_manager::UserImage(
          *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_PROFILE_PICTURE_LOADING)),
      user_manager::User::USER_IMAGE_INVALID,
      false);
  WallpaperManager::Get()->SetUserWallpaperNow(DemoAppLauncher::kDemoUserName);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(::switches::kForceAppMode);
  command_line->AppendSwitchASCII(::switches::kAppId,
                                  DemoAppLauncher::kDemoAppId);

  // Disable window animation since the demo app runs in a single full screen
  // window and window animation causes start-up janks.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      wm::switches::kWindowAnimationsDisabled);
}

void ChromeUserManagerImpl::NotifyOnLogin() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  UserSessionManager::OverrideHomedir();
  UpdateNumberOfUsers();

  ChromeUserManager::NotifyOnLogin();

  // TODO(nkostylev): Deprecate this notification in favor of
  // ActiveUserChanged() observer call.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_USER_CHANGED,
      content::Source<UserManager>(this),
      content::Details<const user_manager::User>(GetActiveUser()));

  UserSessionManager::GetInstance()->PerformPostUserLoggedInActions();
}

void ChromeUserManagerImpl::UpdateOwnership() {
  bool is_owner =
      FakeOwnership() || DeviceSettingsService::Get()->HasPrivateOwnerKey();
  VLOG(1) << "Current user " << (is_owner ? "is owner" : "is not owner");

  SetCurrentUserIsOwner(is_owner);
}

void ChromeUserManagerImpl::RemoveNonCryptohomeData(
    const std::string& user_id) {
  ChromeUserManager::RemoveNonCryptohomeData(user_id);

  WallpaperManager::Get()->RemoveUserWallpaperInfo(user_id);
  GetUserImageManager(user_id)->DeleteUserImage();

  supervised_user_manager_->RemoveNonCryptohomeData(user_id);

  multi_profile_user_controller_->RemoveCachedValues(user_id);

  EasyUnlockService::ResetLocalStateForUser(user_id);
}

void
ChromeUserManagerImpl::CleanUpPublicAccountNonCryptohomeDataPendingRemoval() {
  PrefService* local_state = GetLocalState();
  const std::string public_account_pending_data_removal =
      local_state->GetString(kPublicAccountPendingDataRemoval);
  if (public_account_pending_data_removal.empty() ||
      (IsUserLoggedIn() &&
       public_account_pending_data_removal == GetActiveUser()->email())) {
    return;
  }

  RemoveNonCryptohomeData(public_account_pending_data_removal);
  local_state->ClearPref(kPublicAccountPendingDataRemoval);
}

void ChromeUserManagerImpl::CleanUpPublicAccountNonCryptohomeData(
    const std::vector<std::string>& old_public_accounts) {
  std::set<std::string> users;
  for (user_manager::UserList::const_iterator it = users_.begin();
       it != users_.end();
       ++it)
    users.insert((*it)->email());

  // If the user is logged into a public account that has been removed from the
  // user list, mark the account's data as pending removal after logout.
  if (IsLoggedInAsPublicAccount()) {
    const std::string active_user_id = GetActiveUser()->email();
    if (users.find(active_user_id) == users.end()) {
      GetLocalState()->SetString(kPublicAccountPendingDataRemoval,
                                 active_user_id);
      users.insert(active_user_id);
    }
  }

  // Remove the data belonging to any other public accounts that are no longer
  // found on the user list.
  for (std::vector<std::string>::const_iterator it =
           old_public_accounts.begin();
       it != old_public_accounts.end();
       ++it) {
    if (users.find(*it) == users.end())
      RemoveNonCryptohomeData(*it);
  }
}

bool ChromeUserManagerImpl::UpdateAndCleanUpPublicAccounts(
    const std::vector<policy::DeviceLocalAccount>& device_local_accounts) {
  // Try to remove any public account data marked as pending removal.
  CleanUpPublicAccountNonCryptohomeDataPendingRemoval();

  // Get the current list of public accounts.
  std::vector<std::string> old_public_accounts;
  for (user_manager::UserList::const_iterator it = users_.begin();
       it != users_.end();
       ++it) {
    if ((*it)->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT)
      old_public_accounts.push_back((*it)->email());
  }

  // Get the new list of public accounts from policy.
  std::vector<std::string> new_public_accounts;
  for (std::vector<policy::DeviceLocalAccount>::const_iterator it =
           device_local_accounts.begin();
       it != device_local_accounts.end();
       ++it) {
    // TODO(mnissler, nkostylev, bartfab): Process Kiosk Apps within the
    // standard login framework: http://crbug.com/234694
    if (it->type == policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION)
      new_public_accounts.push_back(it->user_id);
  }

  // If the list of public accounts has not changed, return.
  if (new_public_accounts.size() == old_public_accounts.size()) {
    bool changed = false;
    for (size_t i = 0; i < new_public_accounts.size(); ++i) {
      if (new_public_accounts[i] != old_public_accounts[i]) {
        changed = true;
        break;
      }
    }
    if (!changed)
      return false;
  }

  // Persist the new list of public accounts in a pref.
  ListPrefUpdate prefs_public_accounts_update(GetLocalState(), kPublicAccounts);
  prefs_public_accounts_update->Clear();
  for (std::vector<std::string>::const_iterator it =
           new_public_accounts.begin();
       it != new_public_accounts.end();
       ++it) {
    prefs_public_accounts_update->AppendString(*it);
  }

  // Remove the old public accounts from the user list.
  for (user_manager::UserList::iterator it = users_.begin();
       it != users_.end();) {
    if ((*it)->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
      if (*it != GetLoggedInUser())
        DeleteUser(*it);
      it = users_.erase(it);
    } else {
      ++it;
    }
  }

  // Add the new public accounts to the front of the user list.
  for (std::vector<std::string>::const_reverse_iterator it =
           new_public_accounts.rbegin();
       it != new_public_accounts.rend();
       ++it) {
    if (IsLoggedInAsPublicAccount() && *it == GetActiveUser()->email())
      users_.insert(users_.begin(), GetLoggedInUser());
    else
      users_.insert(users_.begin(),
                    user_manager::User::CreatePublicAccountUser(*it));
    UpdatePublicAccountDisplayName(*it);
  }

  for (user_manager::UserList::iterator
           ui = users_.begin(),
           ue = users_.begin() + new_public_accounts.size();
       ui != ue;
       ++ui) {
    GetUserImageManager((*ui)->email())->LoadUserImage();
  }

  // Remove data belonging to public accounts that are no longer found on the
  // user list.
  CleanUpPublicAccountNonCryptohomeData(old_public_accounts);

  return true;
}

void ChromeUserManagerImpl::UpdatePublicAccountDisplayName(
    const std::string& user_id) {
  std::string display_name;

  if (device_local_account_policy_service_) {
    policy::DeviceLocalAccountPolicyBroker* broker =
        device_local_account_policy_service_->GetBrokerForUser(user_id);
    if (broker)
      display_name = broker->GetDisplayName();
  }

  // Set or clear the display name.
  SaveUserDisplayName(user_id, base::UTF8ToUTF16(display_name));
}

UserFlow* ChromeUserManagerImpl::GetCurrentUserFlow() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!IsUserLoggedIn())
    return GetDefaultUserFlow();
  return GetUserFlow(GetLoggedInUser()->email());
}

UserFlow* ChromeUserManagerImpl::GetUserFlow(const std::string& user_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  FlowMap::const_iterator it = specific_flows_.find(user_id);
  if (it != specific_flows_.end())
    return it->second;
  return GetDefaultUserFlow();
}

void ChromeUserManagerImpl::SetUserFlow(const std::string& user_id,
                                        UserFlow* flow) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ResetUserFlow(user_id);
  specific_flows_[user_id] = flow;
}

void ChromeUserManagerImpl::ResetUserFlow(const std::string& user_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  FlowMap::iterator it = specific_flows_.find(user_id);
  if (it != specific_flows_.end()) {
    delete it->second;
    specific_flows_.erase(it);
  }
}

bool ChromeUserManagerImpl::AreSupervisedUsersAllowed() const {
  bool supervised_users_allowed = false;
  cros_settings_->GetBoolean(kAccountsPrefSupervisedUsersEnabled,
                             &supervised_users_allowed);
  return supervised_users_allowed;
}

UserFlow* ChromeUserManagerImpl::GetDefaultUserFlow() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!default_flow_.get())
    default_flow_.reset(new DefaultUserFlow());
  return default_flow_.get();
}

void ChromeUserManagerImpl::NotifyUserListChanged() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_USER_LIST_CHANGED,
      content::Source<UserManager>(this),
      content::NotificationService::NoDetails());
}

void ChromeUserManagerImpl::NotifyUserAddedToSession(
    const user_manager::User* added_user,
    bool user_switch_pending) {
  // Special case for user session restoration after browser crash.
  // We don't switch to each user session that has been restored as once all
  // session will be restored we'll switch to the session that has been used
  // before the crash.
  if (user_switch_pending &&
      !UserSessionManager::GetInstance()->UserSessionsRestoreInProgress()) {
    SetPendingUserSwitchID(added_user->email());
  }

  UpdateNumberOfUsers();
  ChromeUserManager::NotifyUserAddedToSession(added_user, user_switch_pending);
}

void ChromeUserManagerImpl::OnUserNotAllowed(const std::string& user_email) {
  LOG(ERROR) << "Shutdown session because a user is not allowed to be in the "
                "current session";
  chromeos::ShowMultiprofilesSessionAbortedDialog(user_email);
}

void ChromeUserManagerImpl::RemovePendingBootstrapUser(
    const std::string& user_id) {
  DCHECK(HasPendingBootstrap(user_id));
  RemoveNonOwnerUserInternal(user_id, NULL);
}

void ChromeUserManagerImpl::UpdateNumberOfUsers() {
  size_t users = GetLoggedInUsers().size();
  if (users) {
    // Write the user number as UMA stat when a multi user session is possible.
    if ((users + GetUsersAllowedForMultiProfile().size()) > 1)
      ash::MultiProfileUMA::RecordUserCount(users);
  }

  base::debug::SetCrashKeyValue(
      crash_keys::kNumberOfUsers,
      base::StringPrintf("%" PRIuS, GetLoggedInUsers().size()));
}

void ChromeUserManagerImpl::UpdateUserTimeZoneRefresher(Profile* profile) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableTimeZoneTrackingOption)) {
    return;
  }

  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  if (user == NULL)
    return;

  // In Multi-Profile mode only primary user settings are in effect.
  if (user != user_manager::UserManager::Get()->GetPrimaryUser())
    return;

  if (!IsUserLoggedIn())
    return;

  // Timezone auto refresh is disabled for Guest, Supervized and OffTheRecord
  // users, but enabled for Kiosk mode.
  if (IsLoggedInAsGuest() || IsLoggedInAsSupervisedUser() ||
      profile->IsOffTheRecord()) {
    g_browser_process->platform_part()->GetTimezoneResolver()->Stop();
    return;
  }

  if (profile->GetPrefs()->GetBoolean(prefs::kResolveTimezoneByGeolocation) &&
      !system::HasSystemTimezonePolicy()) {
    g_browser_process->platform_part()->GetTimezoneResolver()->Start();
  } else {
    g_browser_process->platform_part()->GetTimezoneResolver()->Stop();
  }
}

void ChromeUserManagerImpl::SetUserAffiliation(
    const std::string& user_email,
    const AffiliationIDSet& user_affiliation_ids) {
  std::string canonicalized_email = FullyCanonicalize(user_email);
  user_manager::User* user = FindUserAndModify(canonicalized_email);

  if (user) {
    policy::BrowserPolicyConnectorChromeOS const* const connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    const bool is_affiliated = chromeos::IsUserAffiliated(
        user_affiliation_ids, connector->GetDeviceAffiliationIDs(),
        canonicalized_email, connector->GetEnterpriseDomain());
    user->set_affiliation(is_affiliated);

    if (user->GetType() == user_manager::USER_TYPE_REGULAR) {
      if (is_affiliated) {
        AddReportingUser(canonicalized_email);
      } else {
        RemoveReportingUser(canonicalized_email);
      }
    }
  }
}

bool ChromeUserManagerImpl::ShouldReportUser(const std::string& user_id) const {
  const base::ListValue& reporting_users =
      *(GetLocalState()->GetList(kReportingUsers));
  base::StringValue user_id_value(FullyCanonicalize(user_id));
  return !(reporting_users.Find(user_id_value) == reporting_users.end());
}

void ChromeUserManagerImpl::AddReportingUser(const std::string& user_id) {
  ListPrefUpdate users_update(GetLocalState(), kReportingUsers);
  users_update->AppendIfNotPresent(
      new base::StringValue(FullyCanonicalize(user_id)));
}

void ChromeUserManagerImpl::RemoveReportingUser(const std::string& user_id) {
  ListPrefUpdate users_update(GetLocalState(), kReportingUsers);
  users_update->Remove(base::StringValue(FullyCanonicalize(user_id)), NULL);
}

}  // namespace chromeos
