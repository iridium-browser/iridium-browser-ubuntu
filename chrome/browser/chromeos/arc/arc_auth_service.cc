// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_auth_service.h"

#include <utility>

#include "ash/common/shelf/shelf_delegate.h"
#include "ash/common/wm_shell.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/arc/arc_android_management_checker.h"
#include "chrome/browser/chromeos/arc/arc_auth_context.h"
#include "chrome/browser/chromeos/arc/arc_auth_notification.h"
#include "chrome/browser/chromeos/arc/arc_optin_uma.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_launcher.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/arc/arc_bridge_service.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "components/user_manager/user.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "ui/base/l10n/l10n_util.h"

namespace arc {

namespace {

// Weak pointer.  This class is owned by ArcServiceManager.
ArcAuthService* g_arc_auth_service = nullptr;

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

const char kStateNotInitialized[] = "NOT_INITIALIZED";
const char kStateStopped[] = "STOPPED";
const char kStateFetchingCode[] = "FETCHING_CODE";
const char kStateActive[] = "ACTIVE";

bool IsAccountManaged(Profile* profile) {
  return policy::ProfilePolicyConnectorFactory::GetForBrowserContext(profile)
      ->IsManaged();
}

bool IsArcDisabledForEnterprise() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kEnterpriseDisableArc);
}

ash::ShelfDelegate* GetShelfDelegate() {
  if (g_shelf_delegate_for_testing)
    return g_shelf_delegate_for_testing;
  if (ash::WmShell::HasInstance()) {
    DCHECK(ash::WmShell::Get()->shelf_delegate());
    return ash::WmShell::Get()->shelf_delegate();
  }
  return nullptr;
}

ProvisioningResult ConvertArcSignInFailureReasonToProvisioningResult(
    arc::mojom::ArcSignInFailureReason reason) {
  using ArcSignInFailureReason = arc::mojom::ArcSignInFailureReason;

#define MAP_PROVISIONING_RESULT(name) \
  case ArcSignInFailureReason::name:  \
    return ProvisioningResult::name

  switch (reason) {
    MAP_PROVISIONING_RESULT(UNKNOWN_ERROR);
    MAP_PROVISIONING_RESULT(MOJO_VERSION_MISMATCH);
    MAP_PROVISIONING_RESULT(MOJO_CALL_TIMEOUT);
    MAP_PROVISIONING_RESULT(DEVICE_CHECK_IN_FAILED);
    MAP_PROVISIONING_RESULT(DEVICE_CHECK_IN_TIMEOUT);
    MAP_PROVISIONING_RESULT(DEVICE_CHECK_IN_INTERNAL_ERROR);
    MAP_PROVISIONING_RESULT(GMS_NETWORK_ERROR);
    MAP_PROVISIONING_RESULT(GMS_SERVICE_UNAVAILABLE);
    MAP_PROVISIONING_RESULT(GMS_BAD_AUTHENTICATION);
    MAP_PROVISIONING_RESULT(GMS_SIGN_IN_FAILED);
    MAP_PROVISIONING_RESULT(GMS_SIGN_IN_TIMEOUT);
    MAP_PROVISIONING_RESULT(GMS_SIGN_IN_INTERNAL_ERROR);
    MAP_PROVISIONING_RESULT(CLOUD_PROVISION_FLOW_FAILED);
    MAP_PROVISIONING_RESULT(CLOUD_PROVISION_FLOW_TIMEOUT);
    MAP_PROVISIONING_RESULT(CLOUD_PROVISION_FLOW_INTERNAL_ERROR);
  }
#undef MAP_PROVISIONING_RESULT

  NOTREACHED() << "unknown reason: " << static_cast<int>(reason);
  return ProvisioningResult::UNKNOWN_ERROR;
}

}  // namespace

ArcAuthService::ArcAuthService(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this), weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!g_arc_auth_service);

  g_arc_auth_service = this;

  arc_bridge_service()->AddObserver(this);
  arc_bridge_service()->auth()->AddObserver(this);
}

ArcAuthService::~ArcAuthService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(this, g_arc_auth_service);

  Shutdown();
  arc_bridge_service()->auth()->RemoveObserver(this);
  arc_bridge_service()->RemoveObserver(this);

  g_arc_auth_service = nullptr;
}

// static
ArcAuthService* ArcAuthService::Get() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return g_arc_auth_service;
}

// static
void ArcAuthService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // TODO(dspaid): Implement a mechanism to allow this to sync on first boot
  // only.
  registry->RegisterBooleanPref(prefs::kArcEnabled, false);
  registry->RegisterBooleanPref(prefs::kArcSignedIn, false);
  registry->RegisterBooleanPref(prefs::kArcBackupRestoreEnabled, true);
  registry->RegisterBooleanPref(prefs::kArcLocationServiceEnabled, true);
}

// static
void ArcAuthService::DisableUIForTesting() {
  g_disable_ui_for_testing = true;
}

// static
void ArcAuthService::SetShelfDelegateForTesting(
    ash::ShelfDelegate* shelf_delegate) {
  g_shelf_delegate_for_testing = shelf_delegate;
}

// static
bool ArcAuthService::IsOptInVerificationDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kDisableArcOptInVerification);
}

// static
void ArcAuthService::EnableCheckAndroidManagementForTesting() {
  g_enable_check_android_management_for_testing = true;
}

// static
bool ArcAuthService::IsAllowedForProfile(const Profile* profile) {
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

  if (profile->IsLegacySupervised()) {
    VLOG(1) << "Supervised users are not supported in ARC.";
    return false;
  }

  user_manager::User const* const user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user || !user->HasGaiaAccount()) {
    VLOG(1) << "Users without GAIA accounts are not supported in ARC.";
    return false;
  }

  if (user_manager::UserManager::Get()
          ->IsCurrentUserCryptohomeDataEphemeral()) {
    VLOG(2) << "Users with ephemeral data are not supported in Arc.";
    return false;
  }

  return true;
}

void ArcAuthService::OnInstanceReady() {
  arc_bridge_service()->auth()->instance()->Init(
      binding_.CreateInterfacePtrAndBind());
}

void ArcAuthService::OnBridgeStopped(ArcBridgeService::StopReason reason) {
  // TODO(crbug.com/625923): Use |reason| to report more detailed errors.
  if (arc_sign_in_timer_.IsRunning()) {
    OnSignInFailedInternal(ProvisioningResult::ARC_STOPPED);
  }

  if (clear_required_) {
    // This should be always true, but just in case as this is looked at
    // inside RemoveArcData() at first.
    DCHECK(arc_bridge_service()->stopped());
    RemoveArcData();
  } else {
    // To support special "Stop and enable ARC" procedure for enterprise,
    // here call OnArcDataRemoved(true) as if the data removal is successfully
    // done.
    // TODO(hidehiko): Restructure the code.
    OnArcDataRemoved(true);
  }
}

void ArcAuthService::RemoveArcData() {
  if (!arc_bridge_service()->stopped()) {
    // Just set a flag. On bridge stopped, this will be re-called,
    // then session manager should remove the data.
    clear_required_ = true;
    return;
  }
  clear_required_ = false;
  chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->RemoveArcData(
      cryptohome::Identification(
          multi_user_util::GetAccountIdFromProfile(profile_)),
      base::Bind(&ArcAuthService::OnArcDataRemoved,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ArcAuthService::OnArcDataRemoved(bool success) {
  LOG_IF(ERROR, !success) << "Required ARC user data wipe failed.";

  // Here check if |reenable_arc_| is marked or not.
  // The only case this happens should be in the special case for enterprise
  // "on managed lost" case. In that case, OnBridgeStopped() should trigger
  // the RemoveArcData(), then this.
  // TODO(hidehiko): Restructure the code.
  if (!reenable_arc_)
    return;

  // Restart ARC anyway. Let the enterprise reporting instance decide whether
  // the ARC user data wipe is still required or not.
  reenable_arc_ = false;
  VLOG(1) << "Reenable ARC";
  EnableArc();
}

std::string ArcAuthService::GetAndResetAuthCode() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::string auth_code;
  auth_code_.swap(auth_code);
  return auth_code;
}

void ArcAuthService::GetAuthCodeDeprecated(
    const GetAuthCodeDeprecatedCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!IsOptInVerificationDisabled());
  callback.Run(mojo::String(GetAndResetAuthCode()));
}

void ArcAuthService::GetAuthCode(const GetAuthCodeCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const std::string auth_code = GetAndResetAuthCode();
  const bool verification_disabled = IsOptInVerificationDisabled();
  if (!auth_code.empty() || verification_disabled) {
    callback.Run(mojo::String(auth_code), !verification_disabled);
    return;
  }

  initial_opt_in_ = false;
  auth_callback_ = callback;
  StartUI();
}

void ArcAuthService::OnSignInComplete() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::ACTIVE);
  DCHECK(!sign_in_time_.is_null());

  arc_sign_in_timer_.Stop();

  if (!IsOptInVerificationDisabled() &&
      !profile_->GetPrefs()->GetBoolean(prefs::kArcSignedIn)) {
    playstore_launcher_.reset(
        new ArcAppLauncher(profile_, kPlayStoreAppId, true));
  }

  profile_->GetPrefs()->SetBoolean(prefs::kArcSignedIn, true);
  CloseUI();
  UpdateProvisioningTiming(base::Time::Now() - sign_in_time_, true,
                           IsAccountManaged(profile_));
  UpdateProvisioningResultUMA(ProvisioningResult::SUCCESS,
                              IsAccountManaged(profile_));

  FOR_EACH_OBSERVER(Observer, observer_list_, OnInitialStart());
}

void ArcAuthService::OnSignInFailed(arc::mojom::ArcSignInFailureReason reason) {
  OnSignInFailedInternal(
      ConvertArcSignInFailureReasonToProvisioningResult(reason));
}

void ArcAuthService::OnSignInFailedInternal(ProvisioningResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::ACTIVE);
  DCHECK(!sign_in_time_.is_null());

  arc_sign_in_timer_.Stop();

  UpdateProvisioningTiming(base::Time::Now() - sign_in_time_, false,
                           IsAccountManaged(profile_));
  UpdateOptInCancelUMA(OptInCancelReason::CLOUD_PROVISION_FLOW_FAIL);
  UpdateProvisioningResultUMA(result, IsAccountManaged(profile_));

  int error_message_id;
  switch (result) {
    case ProvisioningResult::GMS_NETWORK_ERROR:
      error_message_id = IDS_ARC_SIGN_IN_NETWORK_ERROR;
      break;
    case ProvisioningResult::GMS_SERVICE_UNAVAILABLE:
    case ProvisioningResult::GMS_SIGN_IN_FAILED:
    case ProvisioningResult::GMS_SIGN_IN_TIMEOUT:
    case ProvisioningResult::GMS_SIGN_IN_INTERNAL_ERROR:
      error_message_id = IDS_ARC_SIGN_IN_SERVICE_UNAVAILABLE_ERROR;
      break;
    case ProvisioningResult::GMS_BAD_AUTHENTICATION:
      error_message_id = IDS_ARC_SIGN_IN_BAD_AUTHENTICATION_ERROR;
      break;
    case ProvisioningResult::DEVICE_CHECK_IN_FAILED:
    case ProvisioningResult::DEVICE_CHECK_IN_TIMEOUT:
    case ProvisioningResult::DEVICE_CHECK_IN_INTERNAL_ERROR:
      error_message_id = IDS_ARC_SIGN_IN_GMS_NOT_AVAILABLE_ERROR;
      break;
    case ProvisioningResult::CLOUD_PROVISION_FLOW_FAILED:
    case ProvisioningResult::CLOUD_PROVISION_FLOW_TIMEOUT:
    case ProvisioningResult::CLOUD_PROVISION_FLOW_INTERNAL_ERROR:
      error_message_id = IDS_ARC_SIGN_IN_CLOUD_PROVISION_FLOW_FAIL_ERROR;
      break;
    default:
      error_message_id = IDS_ARC_SIGN_IN_UNKNOWN_ERROR;
      break;
  }

  if (result == ProvisioningResult::ARC_STOPPED) {
    if (profile_->GetPrefs()->HasPrefPath(prefs::kArcSignedIn))
      profile_->GetPrefs()->SetBoolean(prefs::kArcSignedIn, false);
    ShutdownBridgeAndShowUI(UIPage::ERROR,
                            l10n_util::GetStringUTF16(error_message_id));
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

  // We'll delay shutting down the bridge in this case to allow people to send
  // feedback.
  ShowUI(UIPage::ERROR_WITH_FEEDBACK,
         l10n_util::GetStringUTF16(error_message_id));
}

void ArcAuthService::GetIsAccountManaged(
    const GetIsAccountManagedCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  callback.Run(IsAccountManaged(profile_));
}

void ArcAuthService::SetState(State state) {
  if (state_ == state)
    return;

  state_ = state;
  FOR_EACH_OBSERVER(Observer, observer_list_, OnOptInChanged(state_));
}

bool ArcAuthService::IsAllowed() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return profile_ != nullptr;
}

void ArcAuthService::OnPrimaryUserProfilePrepared(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile && profile != profile_);

  Shutdown();

  if (!IsAllowedForProfile(profile))
    return;

  // TODO(khmel): Move this to IsAllowedForProfile.
  if (IsArcDisabledForEnterprise() && IsAccountManaged(profile)) {
    VLOG(2) << "Enterprise users are not supported in ARC.";
    return;
  }

  profile_ = profile;
  SetState(State::STOPPED);

  PrefServiceSyncableFromProfile(profile_)->AddSyncedPrefObserver(
      prefs::kArcEnabled, this);

  context_.reset(new ArcAuthContext(this, profile_));

  // In case UI is disabled we assume that ARC is opted-in.
  if (IsOptInVerificationDisabled()) {
    auth_code_.clear();
    StartArc();
    return;
  }

  if (!g_disable_ui_for_testing ||
      g_enable_check_android_management_for_testing) {
    ArcAndroidManagementChecker::StartClient();
  }
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kArcEnabled, base::Bind(&ArcAuthService::OnOptInPreferenceChanged,
                                     weak_ptr_factory_.GetWeakPtr()));
  if (profile_->GetPrefs()->GetBoolean(prefs::kArcEnabled)) {
    OnOptInPreferenceChanged();
  } else {
    RemoveArcData();
    UpdateEnabledStateUMA(false);
    PrefServiceSyncableFromProfile(profile_)->AddObserver(this);
    OnIsSyncingChanged();
  }
}

void ArcAuthService::OnIsSyncingChanged() {
  syncable_prefs::PrefServiceSyncable* const pref_service_syncable =
      PrefServiceSyncableFromProfile(profile_);
  if (!pref_service_syncable->IsSyncing())
    return;

  pref_service_syncable->RemoveObserver(this);

  if (IsArcEnabled())
    OnOptInPreferenceChanged();

  if (!g_disable_ui_for_testing && profile_->IsNewProfile() &&
      !profile_->GetPrefs()->HasPrefPath(prefs::kArcEnabled)) {
    ArcAuthNotification::Show(profile_);
  }
}

void ArcAuthService::Shutdown() {
  ShutdownBridgeAndCloseUI();
  if (profile_) {
    syncable_prefs::PrefServiceSyncable* pref_service_syncable =
        PrefServiceSyncableFromProfile(profile_);
    pref_service_syncable->RemoveObserver(this);
    pref_service_syncable->RemoveSyncedPrefObserver(prefs::kArcEnabled, this);
  }
  pref_change_registrar_.RemoveAll();
  context_.reset();
  profile_ = nullptr;
  SetState(State::NOT_INITIALIZED);
}

void ArcAuthService::ShowUI(UIPage page, const base::string16& status) {
  if (g_disable_ui_for_testing || IsOptInVerificationDisabled())
    return;

  SetUIPage(page, status);
  const extensions::AppWindowRegistry* const app_window_registry =
      extensions::AppWindowRegistry::Get(profile_);
  DCHECK(app_window_registry);
  if (app_window_registry->GetCurrentAppWindowForApp(
          ArcSupportHost::kHostAppId)) {
    return;
  }

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetInstalledExtension(
          ArcSupportHost::kHostAppId);
  CHECK(extension && extensions::util::IsAppLaunchable(
                         ArcSupportHost::kHostAppId, profile_));

  OpenApplication(CreateAppLaunchParamsUserContainer(
      profile_, extension, NEW_WINDOW, extensions::SOURCE_CHROME_INTERNAL));
}

void ArcAuthService::OnContextReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK(!initial_opt_in_);
  CheckAndroidManagement(false);
}

void ArcAuthService::OnSyncedPrefChanged(const std::string& path,
                                         bool from_sync) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Update UMA only for local changes
  if (!from_sync) {
    const bool arc_enabled =
        profile_->GetPrefs()->GetBoolean(prefs::kArcEnabled);
    UpdateOptInActionUMA(arc_enabled ? OptInActionType::OPTED_IN
                                     : OptInActionType::OPTED_OUT);

    if (!disable_arc_from_ui_ && !arc_enabled && !IsArcManaged()) {
      ash::ShelfDelegate* shelf_delegate = GetShelfDelegate();
      if (shelf_delegate)
        shelf_delegate->UnpinAppWithID(ArcSupportHost::kHostAppId);
    }
  }
}

void ArcAuthService::StopArc() {
  if (state_ != State::STOPPED) {
    UpdateEnabledStateUMA(false);
    profile_->GetPrefs()->SetBoolean(prefs::kArcSignedIn, false);
  }
  ShutdownBridgeAndCloseUI();
}

void ArcAuthService::OnOptInPreferenceChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);

  // TODO(dspaid): Move code from OnSyncedPrefChanged into this method.
  OnSyncedPrefChanged(prefs::kArcEnabled, IsArcManaged());

  const bool arc_enabled = IsArcEnabled();
  FOR_EACH_OBSERVER(Observer, observer_list_, OnOptInEnabled(arc_enabled));

  if (!arc_enabled) {
    StopArc();
    RemoveArcData();
    return;
  }

  if (state_ == State::ACTIVE)
    return;
  CloseUI();
  auth_code_.clear();

  if (!profile_->GetPrefs()->GetBoolean(prefs::kArcSignedIn)) {
    // Need pre-fetch auth code and show OptIn UI if needed.
    initial_opt_in_ = true;
    StartUI();
  } else {
    // Ready to start Arc, but check Android management first.
    if (!g_disable_ui_for_testing ||
        g_enable_check_android_management_for_testing) {
      CheckAndroidManagement(true);
    } else {
      StartArc();
    }
  }

  UpdateEnabledStateUMA(true);
}

void ArcAuthService::ShutdownBridge() {
  arc_sign_in_timer_.Stop();
  playstore_launcher_.reset();
  auth_callback_.Reset();
  android_management_checker_.reset();
  arc_bridge_service()->Shutdown();
  if (state_ != State::NOT_INITIALIZED)
    SetState(State::STOPPED);
  FOR_EACH_OBSERVER(Observer, observer_list_, OnShutdownBridge());
}

void ArcAuthService::ShutdownBridgeAndCloseUI() {
  ShutdownBridge();
  CloseUI();
}

void ArcAuthService::ShutdownBridgeAndShowUI(UIPage page,
                                             const base::string16& status) {
  ShutdownBridge();
  ShowUI(page, status);
}

void ArcAuthService::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observer_list_.AddObserver(observer);
}

void ArcAuthService::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observer_list_.RemoveObserver(observer);
}

void ArcAuthService::CloseUI() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnOptInUIClose());
  SetUIPage(UIPage::NO_PAGE, base::string16());
  if (!g_disable_ui_for_testing)
    ArcAuthNotification::Hide();
}

void ArcAuthService::SetUIPage(UIPage page, const base::string16& status) {
  ui_page_ = page;
  ui_page_status_ = status;
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnOptInUIShowPage(ui_page_, ui_page_status_));
}

// This is the special method to support enterprise mojo API.
// TODO(hidehiko): Remove this.
void ArcAuthService::StopAndEnableArc() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!arc_bridge_service()->stopped());
  reenable_arc_ = true;
  StopArc();
}

void ArcAuthService::StartArc() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_bridge_service()->HandleStartup();
  SetState(State::ACTIVE);
}

void ArcAuthService::SetAuthCodeAndStartArc(const std::string& auth_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!auth_code.empty());

  if (!auth_callback_.is_null()) {
    DCHECK_EQ(state_, State::FETCHING_CODE);
    SetState(State::ACTIVE);
    auth_callback_.Run(mojo::String(auth_code), !IsOptInVerificationDisabled());
    auth_callback_.Reset();
    return;
  }

  State state = state_;
  if (state != State::FETCHING_CODE) {
    ShutdownBridgeAndCloseUI();
    return;
  }

  sign_in_time_ = base::Time::Now();
  VLOG(1) << "Starting ARC for first sign in.";

  SetUIPage(UIPage::START_PROGRESS, base::string16());
  ShutdownBridge();
  auth_code_ = auth_code;
  arc_sign_in_timer_.Start(FROM_HERE, kArcSignInTimeout,
                           base::Bind(&ArcAuthService::OnArcSignInTimeout,
                                      weak_ptr_factory_.GetWeakPtr()));
  StartArc();
}

void ArcAuthService::OnArcSignInTimeout() {
  LOG(ERROR) << "Timed out waiting for first sign in.";
  OnSignInFailedInternal(ProvisioningResult::OVERALL_SIGN_IN_TIMEOUT);
}

void ArcAuthService::StartLso() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Update UMA only if error (with or without feedback) is currently shown.
  if (ui_page_ == UIPage::ERROR) {
    UpdateOptInActionUMA(OptInActionType::RETRY);
  } else if (ui_page_ == UIPage::ERROR_WITH_FEEDBACK) {
    UpdateOptInActionUMA(OptInActionType::RETRY);
    ShutdownBridge();
  }

  initial_opt_in_ = false;
  StartUI();
}

void ArcAuthService::CancelAuthCode() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (state_ == State::NOT_INITIALIZED) {
    NOTREACHED();
    return;
  }

  // In case |state_| is ACTIVE, |ui_page_| can be START_PROGRESS (which means
  // normal Arc booting) or  ERROR or ERROR_WITH_FEEDBACK (in case Arc can not
  // be started). If Arc is booting normally dont't stop it on progress close.
  if (state_ != State::FETCHING_CODE && ui_page_ != UIPage::ERROR &&
      ui_page_ != UIPage::ERROR_WITH_FEEDBACK) {
    return;
  }

  // Update UMA with user cancel only if error is not currently shown.
  if (ui_page_ != UIPage::ERROR && ui_page_ == UIPage::ERROR_WITH_FEEDBACK &&
      ui_page_ != UIPage::NO_PAGE) {
    UpdateOptInCancelUMA(OptInCancelReason::USER_CANCEL);
  }

  StopArc();

  if (IsArcManaged())
    return;

  base::AutoReset<bool> auto_reset(&disable_arc_from_ui_, true);
  DisableArc();
}

bool ArcAuthService::IsArcManaged() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);
  return profile_->GetPrefs()->IsManagedPreference(prefs::kArcEnabled);
}

bool ArcAuthService::IsArcEnabled() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!IsAllowed())
    return false;

  DCHECK(profile_);
  return profile_->GetPrefs()->GetBoolean(prefs::kArcEnabled);
}

void ArcAuthService::EnableArc() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);

  if (IsArcEnabled()) {
    OnOptInPreferenceChanged();
    return;
  }

  if (!IsArcManaged())
    profile_->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);
}

void ArcAuthService::DisableArc() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);
  profile_->GetPrefs()->SetBoolean(prefs::kArcEnabled, false);
}

void ArcAuthService::StartUI() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!arc_bridge_service()->stopped()) {
    // If the user attempts to re-enable ARC while the bridge is still running
    // the user should not be able to continue until the bridge has stopped.
    ShowUI(UIPage::ERROR, l10n_util::GetStringUTF16(
                              IDS_ARC_SIGN_IN_SERVICE_UNAVAILABLE_ERROR));
    return;
  }

  SetState(State::FETCHING_CODE);

  if (initial_opt_in_) {
    initial_opt_in_ = false;
    ShowUI(UIPage::TERMS_PROGRESS, base::string16());
  } else {
    context_->PrepareContext();
  }
}

void ArcAuthService::OnPrepareContextFailed() {
  DCHECK_EQ(state_, State::FETCHING_CODE);

  ShutdownBridgeAndShowUI(
      UIPage::ERROR,
      l10n_util::GetStringUTF16(IDS_ARC_SERVER_COMMUNICATION_ERROR));
  UpdateOptInCancelUMA(OptInCancelReason::NETWORK_ERROR);
}

void ArcAuthService::CheckAndroidManagement(bool background_mode) {
  // Do not send requests for Chrome OS managed users.
  if (IsAccountManaged(profile_)) {
    StartArcIfSignedIn();
    return;
  }

  // Do not send requests for well-known consumer domains.
  if (policy::BrowserPolicyConnector::IsNonEnterpriseUser(
          profile_->GetProfileUserName())) {
    StartArcIfSignedIn();
    return;
  }

  android_management_checker_.reset(
      new ArcAndroidManagementChecker(this, context_->token_service(),
                                      context_->account_id(), background_mode));
  if (background_mode)
    StartArcIfSignedIn();
}

void ArcAuthService::OnAndroidManagementChecked(
    policy::AndroidManagementClient::Result result) {
  switch (result) {
    case policy::AndroidManagementClient::Result::RESULT_UNMANAGED:
      StartArcIfSignedIn();
      break;
    case policy::AndroidManagementClient::Result::RESULT_MANAGED:
      if (android_management_checker_->background_mode()) {
        DisableArc();
        return;
      }
      ShutdownBridgeAndShowUI(
          UIPage::ERROR,
          l10n_util::GetStringUTF16(IDS_ARC_ANDROID_MANAGEMENT_REQUIRED_ERROR));
      UpdateOptInCancelUMA(OptInCancelReason::ANDROID_MANAGEMENT_REQUIRED);
      break;
    case policy::AndroidManagementClient::Result::RESULT_ERROR:
      ShutdownBridgeAndShowUI(
          UIPage::ERROR,
          l10n_util::GetStringUTF16(IDS_ARC_SERVER_COMMUNICATION_ERROR));
      UpdateOptInCancelUMA(OptInCancelReason::NETWORK_ERROR);
      break;
    default:
      NOTREACHED();
  }
}

void ArcAuthService::StartArcIfSignedIn() {
  if (state_ == State::ACTIVE)
    return;
  if (profile_->GetPrefs()->GetBoolean(prefs::kArcSignedIn) ||
      IsOptInVerificationDisabled()) {
    StartArc();
  } else {
    ShowUI(UIPage::LSO_PROGRESS, base::string16());
  }
}

std::ostream& operator<<(std::ostream& os, const ArcAuthService::State& state) {
  switch (state) {
    case ArcAuthService::State::NOT_INITIALIZED:
      return os << kStateNotInitialized;
    case ArcAuthService::State::STOPPED:
      return os << kStateStopped;
    case ArcAuthService::State::FETCHING_CODE:
      return os << kStateFetchingCode;
    case ArcAuthService::State::ACTIVE:
      return os << kStateActive;
    default:
      NOTREACHED();
      return os;
  }
}

}  // namespace arc
