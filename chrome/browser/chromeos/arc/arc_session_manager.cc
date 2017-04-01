// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_session_manager.h"

#include <utility>

#include "ash/common/shelf/shelf_delegate.h"
#include "ash/common/wm_shell.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/arc/arc_auth_context.h"
#include "chrome/browser/chromeos/arc/arc_auth_notification.h"
#include "chrome/browser/chromeos/arc/arc_optin_uma.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/chromeos/arc/optin/arc_terms_of_service_negotiator.h"
#include "chrome/browser/chromeos/arc/policy/arc_android_management_checker.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_util.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_launcher.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_session_runner.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_manager/user.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_prefs.h"

namespace arc {

namespace {

// Weak pointer.  This class is owned by ArcServiceManager.
ArcSessionManager* g_arc_session_manager = nullptr;

// Skip creating UI in unit tests
bool g_disable_ui_for_testing = false;

// Use specified ash::ShelfDelegate for unit tests.
ash::ShelfDelegate* g_shelf_delegate_for_testing = nullptr;

// The Android management check is disabled by default, it's used only for
// testing.
bool g_enable_check_android_management_for_testing = false;

// Maximum amount of time we'll wait for ARC to finish booting up. Once this
// timeout expires, keep ARC running in case the user wants to file feedback,
// but present the UI to try again.
constexpr base::TimeDelta kArcSignInTimeout = base::TimeDelta::FromMinutes(5);

ash::ShelfDelegate* GetShelfDelegate() {
  if (g_shelf_delegate_for_testing)
    return g_shelf_delegate_for_testing;
  if (ash::WmShell::HasInstance()) {
    DCHECK(ash::WmShell::Get()->shelf_delegate());
    return ash::WmShell::Get()->shelf_delegate();
  }
  return nullptr;
}

}  // namespace

ArcSessionManager::ArcSessionManager(
    std::unique_ptr<ArcSessionRunner> arc_session_runner)
    : arc_session_runner_(std::move(arc_session_runner)),
      attempt_user_exit_callback_(base::Bind(chrome::AttemptUserExit)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!g_arc_session_manager);
  g_arc_session_manager = this;
  arc_session_runner_->AddObserver(this);
}

ArcSessionManager::~ArcSessionManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Shutdown();
  arc_session_runner_->RemoveObserver(this);

  DCHECK_EQ(this, g_arc_session_manager);
  g_arc_session_manager = nullptr;
}

// static
ArcSessionManager* ArcSessionManager::Get() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return g_arc_session_manager;
}

// static
void ArcSessionManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // TODO(dspaid): Implement a mechanism to allow this to sync on first boot
  // only.
  registry->RegisterBooleanPref(prefs::kArcDataRemoveRequested, false);
  registry->RegisterBooleanPref(prefs::kArcEnabled, false);
  registry->RegisterBooleanPref(prefs::kArcSignedIn, false);
  registry->RegisterBooleanPref(prefs::kArcTermsAccepted, false);
  registry->RegisterBooleanPref(prefs::kArcBackupRestoreEnabled, true);
  registry->RegisterBooleanPref(prefs::kArcLocationServiceEnabled, true);
}

// static
void ArcSessionManager::DisableUIForTesting() {
  g_disable_ui_for_testing = true;
}

// static
void ArcSessionManager::SetShelfDelegateForTesting(
    ash::ShelfDelegate* shelf_delegate) {
  g_shelf_delegate_for_testing = shelf_delegate;
}

// static
bool ArcSessionManager::IsOptInVerificationDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kDisableArcOptInVerification);
}

// static
void ArcSessionManager::EnableCheckAndroidManagementForTesting() {
  g_enable_check_android_management_for_testing = true;
}

// static
bool ArcSessionManager::IsAllowedForProfile(const Profile* profile) {
  if (!ArcBridgeService::GetEnabled(base::CommandLine::ForCurrentProcess())) {
    VLOG(1) << "Arc is not enabled.";
    return false;
  }

  if (!profile) {
    VLOG(1) << "ARC is not supported for systems without profile.";
    return false;
  }

  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile)) {
    VLOG(1) << "Non-primary users are not supported in ARC.";
    return false;
  }

  // IsPrimaryProfile can return true for an incognito profile corresponding
  // to the primary profile, but ARC does not support it.
  if (profile->IsOffTheRecord()) {
    VLOG(1) << "Incognito profile is not supported in ARC.";
    return false;
  }

  if (profile->IsLegacySupervised()) {
    VLOG(1) << "Supervised users are not supported in ARC.";
    return false;
  }

  user_manager::User const* const user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if ((!user || !user->HasGaiaAccount()) && !IsArcKioskMode()) {
    VLOG(1) << "Users without GAIA accounts are not supported in ARC.";
    return false;
  }

  chromeos::UserFlow* user_flow =
      chromeos::ChromeUserManager::Get()->GetUserFlow(user->GetAccountId());
  if (!user_flow || !user_flow->CanStartArc()) {
    VLOG(1) << "ARC is not allowed in the current user flow.";
    return false;
  }

  if (user_manager::UserManager::Get()
          ->IsCurrentUserCryptohomeDataEphemeral()) {
    VLOG(2) << "Users with ephemeral data are not supported in Arc.";
    return false;
  }

  return true;
}

// static
bool ArcSessionManager::IsArcKioskMode() {
  return user_manager::UserManager::Get()->IsLoggedInAsArcKioskApp();
}

void ArcSessionManager::OnSessionReady() {
  for (auto& observer : arc_session_observer_list_)
    observer.OnSessionReady();
}

void ArcSessionManager::OnSessionStopped(StopReason reason) {
  // TODO(crbug.com/625923): Use |reason| to report more detailed errors.
  if (arc_sign_in_timer_.IsRunning())
    OnProvisioningFinished(ProvisioningResult::ARC_STOPPED);

  if (profile_->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested)) {
    // This should be always true, but just in case as this is looked at
    // inside RemoveArcData() at first.
    DCHECK(arc_session_runner_->IsStopped());
    RemoveArcData();
  } else {
    // To support special "Stop and enable ARC" procedure for enterprise,
    // here call MaybeReenableArc() asyncronously.
    // TODO(hidehiko): Restructure the code. crbug.com/665316
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&ArcSessionManager::MaybeReenableArc,
                              weak_ptr_factory_.GetWeakPtr()));
  }

  for (auto& observer : arc_session_observer_list_)
    observer.OnSessionStopped(reason);
}

void ArcSessionManager::RemoveArcData() {
  // Ignore redundant data removal request.
  if (state() == State::REMOVING_DATA_DIR)
    return;

  // OnArcDataRemoved resets this flag.
  profile_->GetPrefs()->SetBoolean(prefs::kArcDataRemoveRequested, true);

  if (!arc_session_runner_->IsStopped()) {
    // Just set a flag. On session stopped, this will be re-called,
    // then session manager should remove the data.
    return;
  }

  SetState(State::REMOVING_DATA_DIR);
  chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->RemoveArcData(
      cryptohome::Identification(
          multi_user_util::GetAccountIdFromProfile(profile_)),
      base::Bind(&ArcSessionManager::OnArcDataRemoved,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ArcSessionManager::OnArcDataRemoved(bool success) {
  LOG_IF(ERROR, !success) << "Required ARC user data wipe failed.";

  // TODO(khmel): Browser tests may shutdown profile by itself. Update browser
  // tests and remove this check.
  if (state() == State::NOT_INITIALIZED)
    return;

  for (auto& observer : observer_list_)
    observer.OnArcDataRemoved();

  profile_->GetPrefs()->SetBoolean(prefs::kArcDataRemoveRequested, false);
  DCHECK_EQ(state(), State::REMOVING_DATA_DIR);
  SetState(State::STOPPED);

  MaybeReenableArc();
}

void ArcSessionManager::MaybeReenableArc() {
  // Here check if |reenable_arc_| is marked or not.
  // The only case this happens should be in the special case for enterprise
  // "on managed lost" case. In that case, OnSessionStopped() should trigger
  // the RemoveArcData(), then this.
  if (!reenable_arc_ || !IsArcEnabled())
    return;

  // Restart ARC anyway. Let the enterprise reporting instance decide whether
  // the ARC user data wipe is still required or not.
  reenable_arc_ = false;
  VLOG(1) << "Reenable ARC";
  EnableArc();
}

void ArcSessionManager::OnProvisioningFinished(ProvisioningResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If the Mojo message to notify finishing the provisioning is already sent
  // from the container, it will be processed even after requesting to stop the
  // container. Ignore all |result|s arriving while ARC is disabled, in order to
  // avoid popping up an error message triggered below. This code intentionally
  // does not support the case of reenabling.
  if (!IsArcEnabled()) {
    LOG(WARNING) << "Provisioning result received after Arc was disabled. "
                 << "Ignoring result " << static_cast<int>(result) << ".";
    return;
  }

  // Due asynchronous nature of stopping the ARC instance,
  // OnProvisioningFinished may arrive after setting the |State::STOPPED| state
  // and |State::Active| is not guaranteed to be set here.
  // prefs::kArcDataRemoveRequested also can be active for now.

  if (provisioning_reported_) {
    // We don't expect ProvisioningResult::SUCCESS is reported twice or reported
    // after an error.
    DCHECK_NE(result, ProvisioningResult::SUCCESS);
    // TODO (khmel): Consider changing LOG to NOTREACHED once we guaranty that
    // no double message can happen in production.
    LOG(WARNING) << "Provisioning result was already reported. Ignoring "
                 << "additional result " << static_cast<int>(result) << ".";
    return;
  }
  provisioning_reported_ = true;

  if (result == ProvisioningResult::CHROME_SERVER_COMMUNICATION_ERROR) {
    if (IsArcKioskMode()) {
      VLOG(1) << "Robot account auth code fetching error";
      // Log out the user. All the cleanup will be done in Shutdown() method.
      // The callback is not called because auth code is empty.
      attempt_user_exit_callback_.Run();
      return;
    }

    // For backwards compatibility, use NETWORK_ERROR for
    // CHROME_SERVER_COMMUNICATION_ERROR case.
    UpdateOptInCancelUMA(OptInCancelReason::NETWORK_ERROR);
  } else if (!sign_in_time_.is_null()) {
    arc_sign_in_timer_.Stop();

    UpdateProvisioningTiming(base::Time::Now() - sign_in_time_,
                             result == ProvisioningResult::SUCCESS,
                             policy_util::IsAccountManaged(profile_));
    UpdateProvisioningResultUMA(result,
                                policy_util::IsAccountManaged(profile_));
    if (result != ProvisioningResult::SUCCESS)
      UpdateOptInCancelUMA(OptInCancelReason::CLOUD_PROVISION_FLOW_FAIL);
  }

  if (result == ProvisioningResult::SUCCESS) {
    if (support_host_)
      support_host_->Close();

    if (profile_->GetPrefs()->GetBoolean(prefs::kArcSignedIn))
      return;

    profile_->GetPrefs()->SetBoolean(prefs::kArcSignedIn, true);
    // Don't show Play Store app for ARC Kiosk because the only one UI in kiosk
    // mode must be the kiosk app and device is not needed for opt-in.
    if (!IsOptInVerificationDisabled() && !IsArcKioskMode()) {
      playstore_launcher_.reset(
          new ArcAppLauncher(profile_, kPlayStoreAppId, true));
    }

    for (auto& observer : observer_list_)
      observer.OnArcInitialStart();
    return;
  }

  ArcSupportHost::Error error;
  switch (result) {
    case ProvisioningResult::GMS_NETWORK_ERROR:
      error = ArcSupportHost::Error::SIGN_IN_NETWORK_ERROR;
      break;
    case ProvisioningResult::GMS_SERVICE_UNAVAILABLE:
    case ProvisioningResult::GMS_SIGN_IN_FAILED:
    case ProvisioningResult::GMS_SIGN_IN_TIMEOUT:
    case ProvisioningResult::GMS_SIGN_IN_INTERNAL_ERROR:
      error = ArcSupportHost::Error::SIGN_IN_SERVICE_UNAVAILABLE_ERROR;
      break;
    case ProvisioningResult::GMS_BAD_AUTHENTICATION:
      error = ArcSupportHost::Error::SIGN_IN_BAD_AUTHENTICATION_ERROR;
      break;
    case ProvisioningResult::DEVICE_CHECK_IN_FAILED:
    case ProvisioningResult::DEVICE_CHECK_IN_TIMEOUT:
    case ProvisioningResult::DEVICE_CHECK_IN_INTERNAL_ERROR:
      error = ArcSupportHost::Error::SIGN_IN_GMS_NOT_AVAILABLE_ERROR;
      break;
    case ProvisioningResult::CLOUD_PROVISION_FLOW_FAILED:
    case ProvisioningResult::CLOUD_PROVISION_FLOW_TIMEOUT:
    case ProvisioningResult::CLOUD_PROVISION_FLOW_INTERNAL_ERROR:
      error = ArcSupportHost::Error::SIGN_IN_CLOUD_PROVISION_FLOW_FAIL_ERROR;
      break;
    case ProvisioningResult::CHROME_SERVER_COMMUNICATION_ERROR:
      error = ArcSupportHost::Error::SERVER_COMMUNICATION_ERROR;
      break;
    default:
      error = ArcSupportHost::Error::SIGN_IN_UNKNOWN_ERROR;
      break;
  }

  if (result == ProvisioningResult::ARC_STOPPED ||
      result == ProvisioningResult::CHROME_SERVER_COMMUNICATION_ERROR) {
    if (profile_->GetPrefs()->HasPrefPath(prefs::kArcSignedIn))
      profile_->GetPrefs()->SetBoolean(prefs::kArcSignedIn, false);
    ShutdownSession();
    if (support_host_)
      support_host_->ShowError(error, false);
    return;
  }

  if (result == ProvisioningResult::CLOUD_PROVISION_FLOW_FAILED ||
      result == ProvisioningResult::CLOUD_PROVISION_FLOW_TIMEOUT ||
      result == ProvisioningResult::CLOUD_PROVISION_FLOW_INTERNAL_ERROR ||
      // OVERALL_SIGN_IN_TIMEOUT might be an indication that ARC believes it is
      // fully setup, but Chrome does not.
      result == ProvisioningResult::OVERALL_SIGN_IN_TIMEOUT ||
      // Just to be safe, remove data if we don't know the cause.
      result == ProvisioningResult::UNKNOWN_ERROR) {
    RemoveArcData();
  }

  // We'll delay shutting down the ARC instance in this case to allow people
  // to send feedback.
  if (support_host_)
    support_host_->ShowError(error, true /* = show send feedback button */);
}

void ArcSessionManager::SetState(State state) {
  state_ = state;
}

bool ArcSessionManager::IsAllowed() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return profile_ != nullptr;
}

void ArcSessionManager::OnPrimaryUserProfilePrepared(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile && profile != profile_);

  Shutdown();

  if (!IsAllowedForProfile(profile))
    return;

  // TODO(khmel): Move this to IsAllowedForProfile.
  if (policy_util::IsArcDisabledForEnterprise() &&
      policy_util::IsAccountManaged(profile)) {
    VLOG(2) << "Enterprise users are not supported in ARC.";
    return;
  }

  profile_ = profile;

  // Create the support host at initialization. Note that, practically,
  // ARC support Chrome app is rarely used (only opt-in and re-auth flow).
  // So, it may be better to initialize it lazily.
  // TODO(hidehiko): Revisit to think about lazy initialization.
  //
  // Don't show UI for ARC Kiosk because the only one UI in kiosk mode must
  // be the kiosk app. In case of error the UI will be useless as well, because
  // in typical use case there will be no one nearby the kiosk device, who can
  // do some action to solve the problem be means of UI.
  if (!g_disable_ui_for_testing && !IsOptInVerificationDisabled() &&
      !IsArcKioskMode()) {
    DCHECK(!support_host_);
    support_host_ = base::MakeUnique<ArcSupportHost>(profile_);
    support_host_->AddObserver(this);
  }

  DCHECK_EQ(State::NOT_INITIALIZED, state_);
  SetState(State::STOPPED);

  PrefServiceSyncableFromProfile(profile_)->AddSyncedPrefObserver(
      prefs::kArcEnabled, this);

  context_.reset(new ArcAuthContext(profile_));

  if (!g_disable_ui_for_testing ||
      g_enable_check_android_management_for_testing) {
    ArcAndroidManagementChecker::StartClient();
  }
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kArcEnabled,
      base::Bind(&ArcSessionManager::OnOptInPreferenceChanged,
                 weak_ptr_factory_.GetWeakPtr()));
  if (profile_->GetPrefs()->GetBoolean(prefs::kArcEnabled)) {
    // Don't start ARC if there is a pending request to remove the data. Restart
    // ARC once data removal finishes.
    if (profile_->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested)) {
      reenable_arc_ = true;
      RemoveArcData();
    } else {
      OnOptInPreferenceChanged();
    }
  } else {
    RemoveArcData();
    PrefServiceSyncableFromProfile(profile_)->AddObserver(this);
    OnIsSyncingChanged();
  }
}

void ArcSessionManager::OnIsSyncingChanged() {
  sync_preferences::PrefServiceSyncable* const pref_service_syncable =
      PrefServiceSyncableFromProfile(profile_);
  if (!pref_service_syncable->IsSyncing())
    return;

  pref_service_syncable->RemoveObserver(this);

  if (IsArcEnabled())
    OnOptInPreferenceChanged();

  if (!g_disable_ui_for_testing &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableArcOOBEOptIn) &&
      profile_->IsNewProfile() &&
      !profile_->GetPrefs()->HasPrefPath(prefs::kArcEnabled)) {
    ArcAuthNotification::Show(profile_);
  }
}

void ArcSessionManager::Shutdown() {
  if (!g_disable_ui_for_testing)
    ArcAuthNotification::Hide();

  ShutdownSession();
  if (support_host_) {
    support_host_->Close();
    support_host_->RemoveObserver(this);
    support_host_.reset();
  }
  if (profile_) {
    sync_preferences::PrefServiceSyncable* pref_service_syncable =
        PrefServiceSyncableFromProfile(profile_);
    pref_service_syncable->RemoveObserver(this);
    pref_service_syncable->RemoveSyncedPrefObserver(prefs::kArcEnabled, this);
  }
  pref_change_registrar_.RemoveAll();
  context_.reset();
  profile_ = nullptr;
  SetState(State::NOT_INITIALIZED);
}

void ArcSessionManager::OnSyncedPrefChanged(const std::string& path,
                                            bool from_sync) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Update UMA only for local changes
  if (!from_sync) {
    const bool arc_enabled =
        profile_->GetPrefs()->GetBoolean(prefs::kArcEnabled);
    UpdateOptInActionUMA(arc_enabled ? OptInActionType::OPTED_IN
                                     : OptInActionType::OPTED_OUT);

    if (!arc_enabled && !IsArcManaged()) {
      ash::ShelfDelegate* shelf_delegate = GetShelfDelegate();
      if (shelf_delegate)
        shelf_delegate->UnpinAppWithID(ArcSupportHost::kHostAppId);
    }
  }
}

void ArcSessionManager::StopArc() {
  if (state_ != State::STOPPED) {
    profile_->GetPrefs()->SetBoolean(prefs::kArcSignedIn, false);
    profile_->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted, false);
  }
  ShutdownSession();
  if (support_host_)
    support_host_->Close();
}

void ArcSessionManager::OnOptInPreferenceChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);

  // TODO(dspaid): Move code from OnSyncedPrefChanged into this method.
  OnSyncedPrefChanged(prefs::kArcEnabled, IsArcManaged());

  const bool arc_enabled = IsArcEnabled();
  for (auto& observer : observer_list_)
    observer.OnArcOptInChanged(arc_enabled);

  // Hide auth notification if it was opened before and arc.enabled pref was
  // explicitly set to true or false.
  if (!g_disable_ui_for_testing &&
      profile_->GetPrefs()->HasPrefPath(prefs::kArcEnabled)) {
    ArcAuthNotification::Hide();
  }

  if (!arc_enabled) {
    // Reset any pending request to re-enable Arc.
    reenable_arc_ = false;
    StopArc();
    RemoveArcData();
    return;
  }

  if (state_ == State::ACTIVE)
    return;

  if (state_ == State::REMOVING_DATA_DIR) {
    // Data removal request is in progress. Set flag to re-enable Arc once it is
    // finished.
    reenable_arc_ = true;
    return;
  }

  if (support_host_)
    support_host_->SetArcManaged(IsArcManaged());

  // For ARC Kiosk we skip ToS because it is very likely that near the device
  // there will be no one who is eligible to accept them.
  // TODO(poromov): Move to more Kiosk dedicated set-up phase.
  if (IsArcKioskMode())
    profile_->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted, true);

  // If it is marked that sign in has been successfully done, then directly
  // start ARC.
  // For testing, and for Kisok mode, we also skip ToS negotiation procedure.
  // For backward compatibility, this check needs to be prior to the
  // kArcTermsAccepted check below.
  if (profile_->GetPrefs()->GetBoolean(prefs::kArcSignedIn) ||
      IsOptInVerificationDisabled() || IsArcKioskMode()) {
    StartArc();

    // Skip Android management check for testing.
    // We also skip if Android management check for Kiosk mode,
    // because there are no managed human users for Kiosk exist.
    if (IsOptInVerificationDisabled() || IsArcKioskMode() ||
        (g_disable_ui_for_testing &&
         !g_enable_check_android_management_for_testing)) {
      return;
    }

    // Check Android management in parallel.
    // Note: Because the callback may be called in synchronous way (i.e. called
    // on the same stack), StartCheck() needs to be called *after* StartArc().
    // Otherwise, DisableArc() which may be called in
    // OnBackgroundAndroidManagementChecked() could be ignored.
    android_management_checker_ = base::MakeUnique<ArcAndroidManagementChecker>(
        profile_, context_->token_service(), context_->account_id(),
        true /* retry_on_error */);
    android_management_checker_->StartCheck(
        base::Bind(&ArcSessionManager::OnBackgroundAndroidManagementChecked,
                   weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // If it is marked that the Terms of service is accepted already,
  // just skip the negotiation with user, and start Android management
  // check directly.
  // This happens, e.g., when;
  // 1) User accepted the Terms of service on OOBE flow.
  // 2) User accepted the Terms of service on Opt-in flow, but logged out
  //   before ARC sign in procedure was done. Then, logs in again.
  if (profile_->GetPrefs()->GetBoolean(prefs::kArcTermsAccepted)) {
    support_host_->ShowArcLoading();
    StartArcAndroidManagementCheck();
    return;
  }

  // Need user's explicit Terms Of Service agreement.
  StartTermsOfServiceNegotiation();
}

void ArcSessionManager::ShutdownSession() {
  arc_sign_in_timer_.Stop();
  playstore_launcher_.reset();
  terms_of_service_negotiator_.reset();
  android_management_checker_.reset();
  arc_session_runner_->RequestStop();
  // TODO(hidehiko): The ARC instance's stopping is asynchronous, so it might
  // still be running when we return from this function. Do not set the
  // STOPPED state immediately here.
  if (state_ != State::NOT_INITIALIZED && state_ != State::REMOVING_DATA_DIR)
    SetState(State::STOPPED);
  for (auto& observer : observer_list_)
    observer.OnArcBridgeShutdown();
}

void ArcSessionManager::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observer_list_.AddObserver(observer);
}

void ArcSessionManager::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observer_list_.RemoveObserver(observer);
}

void ArcSessionManager::AddSessionObserver(ArcSessionObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_session_observer_list_.AddObserver(observer);
}

void ArcSessionManager::RemoveSessionObserver(ArcSessionObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_session_observer_list_.RemoveObserver(observer);
}

bool ArcSessionManager::IsSessionRunning() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return arc_session_runner_->IsRunning();
}

bool ArcSessionManager::IsSessionStopped() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return arc_session_runner_->IsStopped();
}

// This is the special method to support enterprise mojo API.
// TODO(hidehiko): Remove this.
void ArcSessionManager::StopAndEnableArc() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!arc_session_runner_->IsStopped());
  reenable_arc_ = true;
  StopArc();
}

void ArcSessionManager::StartArc() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Arc must be started only if no pending data removal request exists.
  DCHECK(!profile_->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));

  provisioning_reported_ = false;

  arc_session_runner_->RequestStart();
  SetState(State::ACTIVE);
}

void ArcSessionManager::OnArcSignInTimeout() {
  LOG(ERROR) << "Timed out waiting for first sign in.";
  OnProvisioningFinished(ProvisioningResult::OVERALL_SIGN_IN_TIMEOUT);
}

void ArcSessionManager::CancelAuthCode() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (state_ == State::NOT_INITIALIZED) {
    NOTREACHED();
    return;
  }

  // If ARC failed to boot normally, stop ARC. Similarly, if the current page is
  // LSO, closing the window should stop ARC since the user activity chooses to
  // not sign in. In any other case, ARC is booting normally and the instance
  // should not be stopped.
  if ((state_ != State::SHOWING_TERMS_OF_SERVICE &&
       state_ != State::CHECKING_ANDROID_MANAGEMENT) &&
      (!support_host_ ||
       (support_host_->ui_page() != ArcSupportHost::UIPage::ERROR &&
        support_host_->ui_page() != ArcSupportHost::UIPage::LSO))) {
    return;
  }

  // Update UMA with user cancel only if error is not currently shown.
  if (support_host_ &&
      support_host_->ui_page() != ArcSupportHost::UIPage::NO_PAGE &&
      support_host_->ui_page() != ArcSupportHost::UIPage::ERROR) {
    UpdateOptInCancelUMA(OptInCancelReason::USER_CANCEL);
  }

  StopArc();

  if (IsArcManaged())
    return;

  DisableArc();
}

bool ArcSessionManager::IsArcManaged() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);
  return profile_->GetPrefs()->IsManagedPreference(prefs::kArcEnabled);
}

bool ArcSessionManager::IsArcEnabled() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!IsAllowed())
    return false;

  DCHECK(profile_);
  return profile_->GetPrefs()->GetBoolean(prefs::kArcEnabled);
}

void ArcSessionManager::EnableArc() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);

  if (IsArcEnabled()) {
    OnOptInPreferenceChanged();
    return;
  }

  if (!IsArcManaged())
    profile_->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);
}

void ArcSessionManager::DisableArc() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);
  profile_->GetPrefs()->SetBoolean(prefs::kArcEnabled, false);
}

void ArcSessionManager::RecordArcState() {
  // Only record Enabled state if ARC is allowed in the first place, so we do
  // not split the ARC population by devices that cannot run ARC.
  if (IsAllowed())
    UpdateEnabledStateUMA(IsArcEnabled());
}

void ArcSessionManager::StartTermsOfServiceNegotiation() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!terms_of_service_negotiator_);

  if (!arc_session_runner_->IsStopped()) {
    // If the user attempts to re-enable ARC while the ARC instance is still
    // running the user should not be able to continue until the ARC instance
    // has stopped.
    if (support_host_) {
      support_host_->ShowError(
          ArcSupportHost::Error::SIGN_IN_SERVICE_UNAVAILABLE_ERROR, false);
    }
    return;
  }

  SetState(State::SHOWING_TERMS_OF_SERVICE);
  if (support_host_) {
    terms_of_service_negotiator_ =
        base::MakeUnique<ArcTermsOfServiceNegotiator>(profile_->GetPrefs(),
                                                      support_host_.get());
    terms_of_service_negotiator_->StartNegotiation(
        base::Bind(&ArcSessionManager::OnTermsOfServiceNegotiated,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void ArcSessionManager::OnTermsOfServiceNegotiated(bool accepted) {
  DCHECK(terms_of_service_negotiator_);
  terms_of_service_negotiator_.reset();

  if (!accepted) {
    // To cancel, user needs to close the window. Note that clicking "Cancel"
    // button effectively just closes the window.
    CancelAuthCode();
    return;
  }

  // Terms were accepted.
  profile_->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted, true);

  support_host_->ShowArcLoading();
  StartArcAndroidManagementCheck();
}

void ArcSessionManager::StartArcAndroidManagementCheck() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(arc_session_runner_->IsStopped());
  DCHECK(state_ == State::SHOWING_TERMS_OF_SERVICE ||
         state_ == State::CHECKING_ANDROID_MANAGEMENT);
  SetState(State::CHECKING_ANDROID_MANAGEMENT);

  android_management_checker_.reset(new ArcAndroidManagementChecker(
      profile_, context_->token_service(), context_->account_id(),
      false /* retry_on_error */));
  android_management_checker_->StartCheck(
      base::Bind(&ArcSessionManager::OnAndroidManagementChecked,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ArcSessionManager::OnAndroidManagementChecked(
    policy::AndroidManagementClient::Result result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::CHECKING_ANDROID_MANAGEMENT);

  switch (result) {
    case policy::AndroidManagementClient::Result::UNMANAGED:
      VLOG(1) << "Starting ARC for first sign in.";
      sign_in_time_ = base::Time::Now();
      arc_sign_in_timer_.Start(
          FROM_HERE, kArcSignInTimeout,
          base::Bind(&ArcSessionManager::OnArcSignInTimeout,
                     weak_ptr_factory_.GetWeakPtr()));
      StartArc();
      break;
    case policy::AndroidManagementClient::Result::MANAGED:
      ShutdownSession();
      if (support_host_) {
        support_host_->ShowError(
            ArcSupportHost::Error::ANDROID_MANAGEMENT_REQUIRED_ERROR, false);
      }
      UpdateOptInCancelUMA(OptInCancelReason::ANDROID_MANAGEMENT_REQUIRED);
      break;
    case policy::AndroidManagementClient::Result::ERROR:
      ShutdownSession();
      if (support_host_) {
        support_host_->ShowError(
            ArcSupportHost::Error::SERVER_COMMUNICATION_ERROR, false);
      }
      UpdateOptInCancelUMA(OptInCancelReason::NETWORK_ERROR);
      break;
  }
}

void ArcSessionManager::OnBackgroundAndroidManagementChecked(
    policy::AndroidManagementClient::Result result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  switch (result) {
    case policy::AndroidManagementClient::Result::UNMANAGED:
      // Do nothing. ARC should be started already.
      break;
    case policy::AndroidManagementClient::Result::MANAGED:
      DisableArc();
      break;
    case policy::AndroidManagementClient::Result::ERROR:
      // This code should not be reached. For background check,
      // retry_on_error should be set.
      NOTREACHED();
  }
}

void ArcSessionManager::OnWindowClosed() {
  DCHECK(support_host_);
  if (terms_of_service_negotiator_) {
    // In this case, ArcTermsOfServiceNegotiator should handle the case.
    // Do nothing.
    return;
  }
  CancelAuthCode();
}

void ArcSessionManager::OnTermsAgreed(bool is_metrics_enabled,
                                      bool is_backup_and_restore_enabled,
                                      bool is_location_service_enabled) {
  DCHECK(support_host_);
  DCHECK(terms_of_service_negotiator_);
  // This should be handled in ArcTermsOfServiceNegotiator. Do nothing here.
}

void ArcSessionManager::OnRetryClicked() {
  DCHECK(support_host_);

  UpdateOptInActionUMA(OptInActionType::RETRY);

  // TODO(hidehiko): Simplify the retry logic.
  if (terms_of_service_negotiator_) {
    // Currently Terms of service is shown. ArcTermsOfServiceNegotiator should
    // handle this.
  } else if (!profile_->GetPrefs()->GetBoolean(prefs::kArcTermsAccepted)) {
    StartTermsOfServiceNegotiation();
  } else if (support_host_->ui_page() == ArcSupportHost::UIPage::ERROR &&
             !arc_session_runner_->IsStopped()) {
    // ERROR_WITH_FEEDBACK is set in OnSignInFailed(). In the case, stopping
    // ARC was postponed to contain its internal state into the report.
    // Here, on retry, stop it, then restart.
    DCHECK_EQ(State::ACTIVE, state_);
    support_host_->ShowArcLoading();
    ShutdownSession();
    reenable_arc_ = true;
  } else if (state_ == State::ACTIVE) {
    // This case is handled in ArcAuthService.
    // Do nothing.
  } else {
    // Otherwise, we restart ARC. Note: this is the first boot case.
    // For second or later boot, either ERROR_WITH_FEEDBACK case or ACTIVE
    // case must hit.
    support_host_->ShowArcLoading();
    StartArcAndroidManagementCheck();
  }
}

void ArcSessionManager::OnSendFeedbackClicked() {
  DCHECK(support_host_);
  chrome::OpenFeedbackDialog(nullptr);
}

void ArcSessionManager::SetArcSessionRunnerForTesting(
    std::unique_ptr<ArcSessionRunner> arc_session_runner) {
  DCHECK(arc_session_runner);
  DCHECK(arc_session_runner_);
  DCHECK(arc_session_runner_->IsStopped());
  arc_session_runner_->RemoveObserver(this);
  arc_session_runner_ = std::move(arc_session_runner);
  arc_session_runner_->AddObserver(this);
}

void ArcSessionManager::SetAttemptUserExitCallbackForTesting(
    const base::Closure& callback) {
  DCHECK(!callback.is_null());
  attempt_user_exit_callback_ = callback;
}

std::ostream& operator<<(std::ostream& os,
                         const ArcSessionManager::State& state) {
  switch (state) {
    case ArcSessionManager::State::NOT_INITIALIZED:
      return os << "NOT_INITIALIZED";
    case ArcSessionManager::State::STOPPED:
      return os << "STOPPED";
    case ArcSessionManager::State::SHOWING_TERMS_OF_SERVICE:
      return os << "SHOWING_TERMS_OF_SERVICE";
    case ArcSessionManager::State::CHECKING_ANDROID_MANAGEMENT:
      return os << "CHECKING_ANDROID_MANAGEMENT";
    case ArcSessionManager::State::REMOVING_DATA_DIR:
      return os << "REMOVING_DATA_DIR";
    case ArcSessionManager::State::ACTIVE:
      return os << "ACTIVE";
  }

  // Some compiler reports an error even if all values of an enum-class are
  // covered indivisually in a switch statement.
  NOTREACHED();
  return os;
}

}  // namespace arc
