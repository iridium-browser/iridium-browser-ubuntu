// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service.h"

#include <cstddef>
#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/net/chrome_cookie_notification_details.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/backend_migrator.h"
#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"
#include "chrome/browser/sync/glue/favicon_cache.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/glue/sync_backend_host_impl.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "chrome/browser/sync/glue/typed_url_data_type_controller.h"
#include "chrome/browser/sync/profile_sync_components_factory_impl.h"
#include "chrome/browser/sync/sessions/notification_service_sessions_router.h"
#include "chrome/browser/sync/supervised_user_signin_manager_wrapper.h"
#include "chrome/browser/sync/sync_error_controller.h"
#include "chrome/browser/sync/sync_stopped_reporter.h"
#include "chrome/browser/sync/sync_type_preference_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/invalidation/invalidation_service.h"
#include "components/invalidation/profile_invalidation_provider.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/sync_driver/change_processor.h"
#include "components/sync_driver/data_type_controller.h"
#include "components/sync_driver/device_info.h"
#include "components/sync_driver/pref_names.h"
#include "components/sync_driver/system_encryptor.h"
#include "components/sync_driver/user_selectable_sync_type.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context_getter.h"
#include "sync/api/sync_error.h"
#include "sync/internal_api/public/configure_reason.h"
#include "sync/internal_api/public/http_bridge_network_resources.h"
#include "sync/internal_api/public/network_resources.h"
#include "sync/internal_api/public/sessions/type_debug_info_observer.h"
#include "sync/internal_api/public/shutdown_reason.h"
#include "sync/internal_api/public/sync_context_proxy.h"
#include "sync/internal_api/public/sync_encryption_handler.h"
#include "sync/internal_api/public/util/experiments.h"
#include "sync/internal_api/public/util/sync_db_util.h"
#include "sync/internal_api/public/util/sync_string_conversions.h"
#include "sync/js/js_event_details.h"
#include "sync/protocol/sync.pb.h"
#include "sync/syncable/directory.h"
#include "sync/util/cryptographer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

#if defined(OS_ANDROID)
#include "sync/internal_api/public/read_transaction.h"
#endif

using browser_sync::NotificationServiceSessionsRouter;
using browser_sync::ProfileSyncServiceStartBehavior;
using browser_sync::SessionsSyncManager;
using browser_sync::SyncBackendHost;
using sync_driver::ChangeProcessor;
using sync_driver::DataTypeController;
using sync_driver::DataTypeManager;
using sync_driver::DataTypeStatusTable;
using sync_driver::DeviceInfoSyncService;
using syncer::ModelType;
using syncer::ModelTypeSet;
using syncer::JsBackend;
using syncer::JsController;
using syncer::JsEventDetails;
using syncer::JsEventHandler;
using syncer::ModelSafeRoutingInfo;
using syncer::SyncCredentials;
using syncer::SyncProtocolError;
using syncer::WeakHandle;

typedef GoogleServiceAuthError AuthError;

const char* ProfileSyncService::kSyncServerUrl =
    "https://clients4.google.com/chrome-sync";

const char* ProfileSyncService::kDevServerUrl =
    "https://clients4.google.com/chrome-sync/dev";

const char kSyncUnrecoverableErrorHistogram[] =
    "Sync.UnrecoverableErrors";

const net::BackoffEntry::Policy kRequestAccessTokenBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  0,

  // Initial delay for exponential back-off in ms.
  2000,

  // Factor by which the waiting time will be multiplied.
  2,

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  0.2, // 20%

  // Maximum amount of time we are willing to delay our request in ms.
  // TODO(pavely): crbug.com/246686 ProfileSyncService should retry
  // RequestAccessToken on connection state change after backoff
  1000 * 3600 * 4, // 4 hours.

  // Time to keep an entry from being discarded even when it
  // has no significant state, -1 to never discard.
  -1,

  // Don't use initial delay unless the last request was an error.
  false,
};

static const base::FilePath::CharType kSyncDataFolderName[] =
    FILE_PATH_LITERAL("Sync Data");

static const base::FilePath::CharType kSyncBackupDataFolderName[] =
    FILE_PATH_LITERAL("Sync Data Backup");

namespace {

void ClearBrowsingData(BrowsingDataRemover::Observer* observer,
                       Profile* profile,
                       base::Time start,
                       base::Time end) {
  // BrowsingDataRemover deletes itself when it's done.
  BrowsingDataRemover* remover = BrowsingDataRemover::CreateForRange(
      profile, start, end);
  if (observer)
    remover->AddObserver(observer);
  remover->Remove(BrowsingDataRemover::REMOVE_ALL,
                  BrowsingDataHelper::ALL);

  scoped_refptr<password_manager::PasswordStore> password =
      PasswordStoreFactory::GetForProfile(profile,
                                          ServiceAccessType::EXPLICIT_ACCESS);
  password->RemoveLoginsSyncedBetween(start, end);
}

// Perform the actual sync data folder deletion.
// This should only be called on the sync thread.
void DeleteSyncDataFolder(const base::FilePath& directory_path) {
  if (base::DirectoryExists(directory_path)) {
    if (!base::DeleteFile(directory_path, true))
      LOG(DFATAL) << "Could not delete the Sync Data folder.";
  }
}

}  // anonymous namespace

bool ShouldShowActionOnUI(
    const syncer::SyncProtocolError& error) {
  return (error.action != syncer::UNKNOWN_ACTION &&
          error.action != syncer::DISABLE_SYNC_ON_CLIENT &&
          error.action != syncer::STOP_SYNC_FOR_DISABLED_ACCOUNT);
}

ProfileSyncService::ProfileSyncService(
    scoped_ptr<ProfileSyncComponentsFactory> factory,
    Profile* profile,
    scoped_ptr<SupervisedUserSigninManagerWrapper> signin_wrapper,
    ProfileOAuth2TokenService* oauth2_token_service,
    ProfileSyncServiceStartBehavior start_behavior)
    : OAuth2TokenService::Consumer("sync"),
      last_auth_error_(AuthError::AuthErrorNone()),
      passphrase_required_reason_(syncer::REASON_PASSPHRASE_NOT_REQUIRED),
      factory_(factory.Pass()),
      profile_(profile),
      sync_prefs_(profile_->GetPrefs()),
      sync_service_url_(
          GetSyncServiceURL(*base::CommandLine::ForCurrentProcess())),
      is_first_time_sync_configure_(false),
      backend_initialized_(false),
      sync_disabled_by_admin_(false),
      is_auth_in_progress_(false),
      signin_(signin_wrapper.Pass()),
      unrecoverable_error_reason_(ERROR_REASON_UNSET),
      expect_sync_configuration_aborted_(false),
      encrypted_types_(syncer::SyncEncryptionHandler::SensitiveTypes()),
      encrypt_everything_allowed_(true),
      encrypt_everything_(false),
      encryption_pending_(false),
      configure_status_(DataTypeManager::UNKNOWN),
      oauth2_token_service_(oauth2_token_service),
      request_access_token_backoff_(&kRequestAccessTokenBackoffPolicy),
      connection_status_(syncer::CONNECTION_NOT_ATTEMPTED),
      last_get_token_error_(GoogleServiceAuthError::AuthErrorNone()),
      network_resources_(new syncer::HttpBridgeNetworkResources),
      backend_mode_(IDLE),
      need_backup_(false),
      backup_finished_(false),
      clear_browsing_data_(base::Bind(&ClearBrowsingData)),
      browsing_data_remover_observer_(NULL),
      sync_stopped_reporter_(
          new browser_sync::SyncStoppedReporter(
              sync_service_url_,
              profile_->GetRequestContext(),
              browser_sync::SyncStoppedReporter::ResultCallback())),
      weak_factory_(this),
      startup_controller_weak_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);
  startup_controller_.reset(new browser_sync::StartupController(
      start_behavior,
      oauth2_token_service,
      &sync_prefs_,
      signin_.get(),
      base::Bind(&ProfileSyncService::StartUpSlowBackendComponents,
                 startup_controller_weak_factory_.GetWeakPtr(),
                 SYNC)));
  backup_rollback_controller_.reset(new browser_sync::BackupRollbackController(
      &sync_prefs_,
      signin_.get(),
      base::Bind(&ProfileSyncService::StartUpSlowBackendComponents,
                 startup_controller_weak_factory_.GetWeakPtr(),
                 BACKUP),
      base::Bind(&ProfileSyncService::StartUpSlowBackendComponents,
                 startup_controller_weak_factory_.GetWeakPtr(),
                 ROLLBACK)));
  syncer::SyncableService::StartSyncFlare flare(
      sync_start_util::GetFlareForSyncableService(profile->GetPath()));
  scoped_ptr<browser_sync::LocalSessionEventRouter> router(
      new NotificationServiceSessionsRouter(profile, flare));

  DCHECK(factory_.get());
  local_device_ = factory_->CreateLocalDeviceInfoProvider();
  sessions_sync_manager_.reset(
      new SessionsSyncManager(profile, local_device_.get(), router.Pass()));
  device_info_sync_service_.reset(
      new DeviceInfoSyncService(local_device_.get()));
}

ProfileSyncService::~ProfileSyncService() {
  sync_prefs_.RemoveSyncPrefObserver(this);
  // Shutdown() should have been called before destruction.
  CHECK(!backend_initialized_);
}

bool ProfileSyncService::IsSyncEnabledAndLoggedIn() {
  // Exit if sync is disabled.
  if (IsManaged() || sync_prefs_.IsStartSuppressed())
    return false;

  // Sync is logged in if there is a non-empty effective account id.
  return !signin_->GetAccountIdToUse().empty();
}

bool ProfileSyncService::IsOAuthRefreshTokenAvailable() {
  if (!oauth2_token_service_)
    return false;

  return oauth2_token_service_->RefreshTokenIsAvailable(
      signin_->GetAccountIdToUse());
}

void ProfileSyncService::Initialize() {
  // We clear this here (vs Shutdown) because we want to remember that an error
  // happened on shutdown so we can display details (message, location) about it
  // in about:sync.
  ClearStaleErrors();

  sync_prefs_.AddSyncPrefObserver(this);

  // For now, the only thing we can do through policy is to turn sync off.
  if (IsManaged()) {
    DisableForUser();
    return;
  }

  RegisterAuthNotifications();

  if (!HasSyncSetupCompleted() || signin_->GetAccountIdToUse().empty()) {
    // Clean up in case of previous crash / setup abort / signout.
    DisableForUser();
  }

  TrySyncDatatypePrefRecovery();

#if defined(OS_CHROMEOS)
  std::string bootstrap_token = sync_prefs_.GetEncryptionBootstrapToken();
  if (bootstrap_token.empty()) {
    sync_prefs_.SetEncryptionBootstrapToken(
        sync_prefs_.GetSpareBootstrapToken());
  }
#endif

#if !defined(OS_ANDROID)
  DCHECK(sync_error_controller_ == NULL)
      << "Initialize() called more than once.";
  sync_error_controller_.reset(new SyncErrorController(this));
  AddObserver(sync_error_controller_.get());
#endif

  bool running_rollback = false;
  if (browser_sync::BackupRollbackController::IsBackupEnabled()) {
    // Backup is needed if user's not signed in or signed in but previous
    // backup didn't finish, i.e. backend didn't switch from backup to sync.
    need_backup_ = signin_->GetAccountIdToUse().empty() ||
        sync_prefs_.GetFirstSyncTime().is_null();

    // Try to resume rollback if it didn't finish in last session.
    running_rollback = backup_rollback_controller_->StartRollback();
  } else {
    need_backup_ = false;
  }

#if defined(ENABLE_PRE_SYNC_BACKUP)
  if (!running_rollback && signin_->GetAccountIdToUse().empty()) {
    CleanUpBackup();
  }
#else
  DCHECK(!running_rollback);
#endif

  memory_pressure_listener_.reset(new base::MemoryPressureListener(base::Bind(
      &ProfileSyncService::OnMemoryPressure, weak_factory_.GetWeakPtr())));
  startup_controller_->Reset(GetRegisteredDataTypes());
  startup_controller_->TryStart();
}

void ProfileSyncService::TrySyncDatatypePrefRecovery() {
  DCHECK(!backend_initialized());
  if (!HasSyncSetupCompleted())
    return;

  // There was a bug where OnUserChoseDatatypes was not properly called on
  // configuration (see crbug.com/154940). We detect this by checking whether
  // kSyncKeepEverythingSynced has a default value. If so, and sync setup has
  // completed, it means sync was not properly configured, so we manually
  // set kSyncKeepEverythingSynced.
  PrefService* const pref_service = profile_->GetPrefs();
  if (!pref_service)
    return;
  if (GetPreferredDataTypes().Size() > 1)
    return;

  const PrefService::Preference* keep_everything_synced =
      pref_service->FindPreference(
          sync_driver::prefs::kSyncKeepEverythingSynced);
  // This will be false if the preference was properly set or if it's controlled
  // by policy.
  if (!keep_everything_synced->IsDefaultValue())
    return;

  // kSyncKeepEverythingSynced was not properly set. Set it and the preferred
  // types now, before we configure.
  UMA_HISTOGRAM_COUNTS("Sync.DatatypePrefRecovery", 1);
  sync_prefs_.SetKeepEverythingSynced(true);
  syncer::ModelTypeSet registered_types = GetRegisteredDataTypes();
}

void ProfileSyncService::StartSyncingWithServer() {
  if (backend_)
    backend_->StartSyncingWithServer();
}

void ProfileSyncService::RegisterAuthNotifications() {
  oauth2_token_service_->AddObserver(this);
  if (signin())
    signin()->AddObserver(this);
}

void ProfileSyncService::UnregisterAuthNotifications() {
  if (signin())
    signin()->RemoveObserver(this);
  oauth2_token_service_->RemoveObserver(this);
}

void ProfileSyncService::RegisterDataTypeController(
    DataTypeController* data_type_controller) {
  DCHECK_EQ(
      directory_data_type_controllers_.count(data_type_controller->type()),
      0U);
  DCHECK(!GetRegisteredNonBlockingDataTypes().Has(
      data_type_controller->type()));
  directory_data_type_controllers_[data_type_controller->type()] =
      data_type_controller;
}

void ProfileSyncService::RegisterNonBlockingType(syncer::ModelType type) {
  DCHECK_EQ(directory_data_type_controllers_.count(type), 0U)
      << "Duplicate registration of type " << ModelTypeToString(type);

  // TODO(rlarocque): Set the enable flag properly when crbug.com/368834 is
  // fixed and we have some way of telling whether or not this type should be
  // enabled.
  non_blocking_data_type_manager_.RegisterType(type, false);
}

void ProfileSyncService::InitializeNonBlockingType(
    syncer::ModelType type,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::WeakPtr<syncer::ModelTypeSyncProxyImpl>& type_sync_proxy) {
  non_blocking_data_type_manager_.InitializeType(
      type, task_runner, type_sync_proxy);
}

bool ProfileSyncService::IsDataTypeControllerRunning(
    syncer::ModelType type) const {
  DataTypeController::TypeMap::const_iterator iter =
      directory_data_type_controllers_.find(type);
  if (iter == directory_data_type_controllers_.end()) {
    return false;
  }
  return iter->second->state() == DataTypeController::RUNNING;
}

browser_sync::OpenTabsUIDelegate* ProfileSyncService::GetOpenTabsUIDelegate() {
  if (!IsDataTypeControllerRunning(syncer::SESSIONS))
    return NULL;
  return sessions_sync_manager_.get();
}

browser_sync::FaviconCache* ProfileSyncService::GetFaviconCache() {
  return sessions_sync_manager_->GetFaviconCache();
}

browser_sync::SyncedWindowDelegatesGetter*
ProfileSyncService::GetSyncedWindowDelegatesGetter() const {
  return sessions_sync_manager_->GetSyncedWindowDelegatesGetter();
}

sync_driver::DeviceInfoTracker* ProfileSyncService::GetDeviceInfoTracker()
    const {
  return device_info_sync_service_.get();
}

sync_driver::LocalDeviceInfoProvider*
ProfileSyncService::GetLocalDeviceInfoProvider() {
  return local_device_.get();
}

void ProfileSyncService::GetDataTypeControllerStates(
  DataTypeController::StateMap* state_map) const {
    for (DataTypeController::TypeMap::const_iterator iter =
         directory_data_type_controllers_.begin();
         iter != directory_data_type_controllers_.end();
         ++iter)
      (*state_map)[iter->first] = iter->second.get()->state();
}

SyncCredentials ProfileSyncService::GetCredentials() {
  SyncCredentials credentials;
  if (backend_mode_ == SYNC) {
    credentials.email = signin_->GetEffectiveUsername();
    DCHECK(!credentials.email.empty());
    credentials.sync_token = access_token_;

    if (credentials.sync_token.empty())
      credentials.sync_token = "credentials_lost";

    credentials.scope_set.insert(signin_->GetSyncScopeToUse());
  }

  return credentials;
}

bool ProfileSyncService::ShouldDeleteSyncFolder() {
  switch (backend_mode_) {
    case SYNC:
      return !HasSyncSetupCompleted();
    case BACKUP:
      return true;
    case ROLLBACK:
      return false;
    case IDLE:
      NOTREACHED();
      return true;
  }
  return true;
}

void ProfileSyncService::InitializeBackend(bool delete_stale_data) {
  if (!backend_) {
    NOTREACHED();
    return;
  }

  SyncCredentials credentials = GetCredentials();

  scoped_refptr<net::URLRequestContextGetter> request_context_getter(
      profile_->GetRequestContext());

  if (backend_mode_ == SYNC && delete_stale_data)
    ClearStaleErrors();

  scoped_ptr<syncer::UnrecoverableErrorHandler>
      backend_unrecoverable_error_handler(
          new browser_sync::BackendUnrecoverableErrorHandler(
              MakeWeakHandle(weak_factory_.GetWeakPtr())));

  backend_->Initialize(
      this,
      sync_thread_.Pass(),
      GetJsEventHandler(),
      sync_service_url_,
      credentials,
      delete_stale_data,
      scoped_ptr<syncer::SyncManagerFactory>(
          new syncer::SyncManagerFactory(GetManagerType())).Pass(),
      backend_unrecoverable_error_handler.Pass(),
      &browser_sync::ChromeReportUnrecoverableError,
      network_resources_.get());
}

bool ProfileSyncService::IsEncryptedDatatypeEnabled() const {
  if (encryption_pending())
    return true;
  const syncer::ModelTypeSet preferred_types = GetPreferredDataTypes();
  const syncer::ModelTypeSet encrypted_types = GetEncryptedDataTypes();
  DCHECK(encrypted_types.Has(syncer::PASSWORDS));
  return !Intersection(preferred_types, encrypted_types).Empty();
}

void ProfileSyncService::OnProtocolEvent(
    const syncer::ProtocolEvent& event) {
  FOR_EACH_OBSERVER(browser_sync::ProtocolEventObserver,
                    protocol_event_observers_,
                    OnProtocolEvent(event));
}

void ProfileSyncService::OnDirectoryTypeCommitCounterUpdated(
    syncer::ModelType type,
    const syncer::CommitCounters& counters) {
  FOR_EACH_OBSERVER(syncer::TypeDebugInfoObserver,
                    type_debug_info_observers_,
                    OnCommitCountersUpdated(type, counters));
}

void ProfileSyncService::OnDirectoryTypeUpdateCounterUpdated(
    syncer::ModelType type,
    const syncer::UpdateCounters& counters) {
  FOR_EACH_OBSERVER(syncer::TypeDebugInfoObserver,
                    type_debug_info_observers_,
                    OnUpdateCountersUpdated(type, counters));
}

void ProfileSyncService::OnDirectoryTypeStatusCounterUpdated(
    syncer::ModelType type,
    const syncer::StatusCounters& counters) {
  FOR_EACH_OBSERVER(syncer::TypeDebugInfoObserver,
                    type_debug_info_observers_,
                    OnStatusCountersUpdated(type, counters));
}

void ProfileSyncService::OnDataTypeRequestsSyncStartup(
    syncer::ModelType type) {
  DCHECK(syncer::UserTypes().Has(type));
  if (backend_.get()) {
    DVLOG(1) << "A data type requested sync startup, but it looks like "
                "something else beat it to the punch.";
    return;
  }

  if (!GetPreferredDataTypes().Has(type)) {
    // We can get here as datatype SyncableServices are typically wired up
    // to the native datatype even if sync isn't enabled.
    DVLOG(1) << "Dropping sync startup request because type "
             << syncer::ModelTypeToString(type) << "not enabled.";
    return;
  }

  startup_controller_->OnDataTypeRequestsSyncStartup(type);
}

void ProfileSyncService::StartUpSlowBackendComponents(
    ProfileSyncService::BackendMode mode) {
  DCHECK_NE(IDLE, mode);
  if (backend_mode_ == mode) {
    return;
  }

  // Backend mode transition rules:
  // * can transit from IDLE to any other non-IDLE mode.
  // * forbidden to transit from SYNC to any other mode, i.e. SYNC backend must
  //   be explicitly shut down before backup/rollback starts.
  // * can not transit out of ROLLBACK mode until rollback is finished
  //   (successfully or unsuccessfully).
  // * can not transit out of BACKUP mode until backup is finished
  //   (successfully or unsuccessfully).
  // * if backup is needed, can only transit to SYNC if backup is finished,

  if (backend_mode_ == SYNC) {
    LOG(DFATAL) << "Shouldn't switch from mode SYNC to mode " << mode;
    return;
  }

  if (backend_mode_ == ROLLBACK ||
      (backend_mode_ == BACKUP && !backup_finished_)) {
    // Wait for rollback/backup to finish before start new backend.
    return;
  }

  if (mode == SYNC && NeedBackup() && !backup_finished_) {
    if (backend_mode_ != BACKUP)
      backup_rollback_controller_->StartBackup();
    return;
  }

  DVLOG(1) << "Start backend mode: " << mode;

  if (backend_) {
    if (mode == SYNC)
      ShutdownImpl(syncer::SWITCH_MODE_SYNC);
    else
      ShutdownImpl(syncer::STOP_SYNC);
  }

  backend_mode_ = mode;

  if (backend_mode_ == BACKUP)
    backup_start_time_ = base::Time::Now();

  if (backend_mode_ == SYNC && !backup_start_time_.is_null()) {
    UMA_HISTOGRAM_MEDIUM_TIMES("Sync.FirstSyncDelayByBackup",
                               base::Time::Now() - backup_start_time_);
    backup_start_time_ = base::Time();
  }

  if (backend_mode_ == ROLLBACK)
    ClearBrowsingDataSinceFirstSync();
  else if (backend_mode_ == SYNC)
    CheckSyncBackupIfNeeded();

  base::FilePath sync_folder = backend_mode_ == SYNC ?
      base::FilePath(kSyncDataFolderName) :
      base::FilePath(kSyncBackupDataFolderName);

  invalidation::InvalidationService* invalidator = NULL;
  if (backend_mode_ == SYNC) {
    invalidation::ProfileInvalidationProvider* provider =
        invalidation::ProfileInvalidationProviderFactory::GetForProfile(
            profile_);
    if (provider)
      invalidator = provider->GetInvalidationService();
  }

  directory_path_ = profile_->GetPath().Append(sync_folder);

  backend_.reset(
      factory_->CreateSyncBackendHost(
          profile_->GetDebugName(),
          profile_,
          invalidator,
          sync_prefs_.AsWeakPtr(),
          sync_folder));

  // Initialize the backend.  Every time we start up a new SyncBackendHost,
  // we'll want to start from a fresh SyncDB, so delete any old one that might
  // be there.
  InitializeBackend(ShouldDeleteSyncFolder());

  UpdateFirstSyncTimePref();

  ReportPreviousSessionMemoryWarningCount();
}

void ProfileSyncService::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK_EQ(access_token_request_, request);
  access_token_request_.reset();
  access_token_ = access_token;
  token_receive_time_ = base::Time::Now();
  last_get_token_error_ = GoogleServiceAuthError::AuthErrorNone();

  if (sync_prefs_.SyncHasAuthError()) {
    sync_prefs_.SetSyncAuthError(false);
    UMA_HISTOGRAM_ENUMERATION("Sync.SyncAuthError",
                              AUTH_ERROR_FIXED,
                              AUTH_ERROR_LIMIT);
  }

  if (HasSyncingBackend())
    backend_->UpdateCredentials(GetCredentials());
  else
    startup_controller_->TryStart();
}

void ProfileSyncService::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK_EQ(access_token_request_, request);
  DCHECK_NE(error.state(), GoogleServiceAuthError::NONE);
  access_token_request_.reset();
  last_get_token_error_ = error;
  switch (error.state()) {
    case GoogleServiceAuthError::CONNECTION_FAILED:
    case GoogleServiceAuthError::REQUEST_CANCELED:
    case GoogleServiceAuthError::SERVICE_ERROR:
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE: {
      // Transient error. Retry after some time.
      request_access_token_backoff_.InformOfRequest(false);
      next_token_request_time_ = base::Time::Now() +
          request_access_token_backoff_.GetTimeUntilRelease();
      request_access_token_retry_timer_.Start(
            FROM_HERE,
            request_access_token_backoff_.GetTimeUntilRelease(),
            base::Bind(&ProfileSyncService::RequestAccessToken,
                        weak_factory_.GetWeakPtr()));
      NotifyObservers();
      break;
    }
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS: {
      if (!sync_prefs_.SyncHasAuthError()) {
        sync_prefs_.SetSyncAuthError(true);
        UMA_HISTOGRAM_ENUMERATION("Sync.SyncAuthError",
                                  AUTH_ERROR_ENCOUNTERED,
                                  AUTH_ERROR_LIMIT);
      }
      // Fallthrough.
    }
    default: {
      if (error.state() != GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS) {
        LOG(ERROR) << "Unexpected persistent error: " << error.ToString();
      }
      // Show error to user.
      UpdateAuthErrorState(error);
    }
  }
}

void ProfileSyncService::OnRefreshTokenAvailable(
    const std::string& account_id) {
  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/422460 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "422460 ProfileSyncService::OnRefreshTokenAvailable"));

  if (account_id == signin_->GetAccountIdToUse())
    OnRefreshTokensLoaded();
}

void ProfileSyncService::OnRefreshTokenRevoked(
    const std::string& account_id) {
  if (!IsOAuthRefreshTokenAvailable()) {
    access_token_.clear();
    // The additional check around IsOAuthRefreshTokenAvailable() above
    // prevents us sounding the alarm if we actually have a valid token but
    // a refresh attempt failed for any variety of reasons
    // (e.g. flaky network). It's possible the token we do have is also
    // invalid, but in that case we should already have (or can expect) an
    // auth error sent from the sync backend.
    UpdateAuthErrorState(
        GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
  }
}

void ProfileSyncService::OnRefreshTokensLoaded() {
  // This notification gets fired when OAuth2TokenService loads the tokens
  // from storage.
  // Initialize the backend if sync is enabled. If the sync token was
  // not loaded, GetCredentials() will generate invalid credentials to
  // cause the backend to generate an auth error (crbug.com/121755).
  if (HasSyncingBackend()) {
    RequestAccessToken();
  } else {
    startup_controller_->TryStart();
  }
}

void ProfileSyncService::Shutdown() {
  UnregisterAuthNotifications();

  ShutdownImpl(syncer::BROWSER_SHUTDOWN);
  if (sync_error_controller_) {
    // Destroy the SyncErrorController when the service shuts down for good.
    RemoveObserver(sync_error_controller_.get());
    sync_error_controller_.reset();
  }

  if (sync_thread_)
    sync_thread_->Stop();
}

void ProfileSyncService::ShutdownImpl(syncer::ShutdownReason reason) {
  if (!backend_) {
    if (reason == syncer::ShutdownReason::DISABLE_SYNC && sync_thread_) {
      // If the backend is already shut down when a DISABLE_SYNC happens,
      // the data directory needs to be cleaned up here.
      sync_thread_->message_loop()->PostTask(FROM_HERE,
          base::Bind(&DeleteSyncDataFolder, directory_path_));
    }
    return;
  }

  if (reason == syncer::ShutdownReason::STOP_SYNC
      || reason == syncer::ShutdownReason::DISABLE_SYNC) {
    RemoveClientFromServer();
  }

  non_blocking_data_type_manager_.DisconnectSyncBackend();

  // First, we spin down the backend to stop change processing as soon as
  // possible.
  base::Time shutdown_start_time = base::Time::Now();
  backend_->StopSyncingForShutdown();

  // Stop all data type controllers, if needed.  Note that until Stop
  // completes, it is possible in theory to have a ChangeProcessor apply a
  // change from a native model.  In that case, it will get applied to the sync
  // database (which doesn't get destroyed until we destroy the backend below)
  // as an unsynced change.  That will be persisted, and committed on restart.
  if (directory_data_type_manager_) {
    if (directory_data_type_manager_->state() != DataTypeManager::STOPPED) {
      // When aborting as part of shutdown, we should expect an aborted sync
      // configure result, else we'll dcheck when we try to read the sync error.
      expect_sync_configuration_aborted_ = true;
      directory_data_type_manager_->Stop();
    }
    directory_data_type_manager_.reset();
  }

  // Shutdown the migrator before the backend to ensure it doesn't pull a null
  // snapshot.
  migrator_.reset();
  sync_js_controller_.AttachJsBackend(WeakHandle<syncer::JsBackend>());

  // Move aside the backend so nobody else tries to use it while we are
  // shutting it down.
  scoped_ptr<SyncBackendHost> doomed_backend(backend_.release());
  if (doomed_backend) {
    sync_thread_ = doomed_backend->Shutdown(reason);
    doomed_backend.reset();
  }
  base::TimeDelta shutdown_time = base::Time::Now() - shutdown_start_time;
  UMA_HISTOGRAM_TIMES("Sync.Shutdown.BackendDestroyedTime", shutdown_time);

  weak_factory_.InvalidateWeakPtrs();

  if (backend_mode_ == SYNC)
    startup_controller_->Reset(GetRegisteredDataTypes());

  // Don't let backup block sync regardless backup succeeded or not.
  if (backend_mode_ == BACKUP)
    backup_finished_ = true;

  // Sync could be blocked by rollback/backup. Post task to check whether sync
  // should start after shutting down rollback/backup backend.
  if ((backend_mode_ == ROLLBACK || backend_mode_ == BACKUP) &&
      reason != syncer::SWITCH_MODE_SYNC &&
      reason != syncer::BROWSER_SHUTDOWN) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&ProfileSyncService::TryStartSyncAfterBackup,
                              startup_controller_weak_factory_.GetWeakPtr()));
  }

  // Clear various flags.
  backend_mode_ = IDLE;
  expect_sync_configuration_aborted_ = false;
  is_auth_in_progress_ = false;
  backend_initialized_ = false;
  cached_passphrase_.clear();
  encryption_pending_ = false;
  encrypt_everything_ = false;
  encrypted_types_ = syncer::SyncEncryptionHandler::SensitiveTypes();
  passphrase_required_reason_ = syncer::REASON_PASSPHRASE_NOT_REQUIRED;
  request_access_token_retry_timer_.Stop();
  // Revert to "no auth error".
  if (last_auth_error_.state() != GoogleServiceAuthError::NONE)
    UpdateAuthErrorState(GoogleServiceAuthError::AuthErrorNone());

  NotifyObservers();

  // Mark this as a clean shutdown(without crash).
  sync_prefs_.SetCleanShutdown(true);
}

void ProfileSyncService::DisableForUser() {
  // Clear prefs (including SyncSetupHasCompleted) before shutting down so
  // PSS clients don't think we're set up while we're shutting down.
  sync_prefs_.ClearPreferences();
  ClearUnrecoverableError();
  ShutdownImpl(syncer::DISABLE_SYNC);
}

bool ProfileSyncService::HasSyncSetupCompleted() const {
  return sync_prefs_.HasSyncSetupCompleted();
}

void ProfileSyncService::SetSyncSetupCompleted() {
  sync_prefs_.SetSyncSetupCompleted();
}

void ProfileSyncService::UpdateLastSyncedTime() {
  sync_prefs_.SetLastSyncedTime(base::Time::Now());
}

void ProfileSyncService::NotifyObservers() {
  FOR_EACH_OBSERVER(sync_driver::SyncServiceObserver, observers_,
                    OnStateChanged());
}

void ProfileSyncService::NotifySyncCycleCompleted() {
  FOR_EACH_OBSERVER(sync_driver::SyncServiceObserver, observers_,
                    OnSyncCycleCompleted());
}

void ProfileSyncService::ClearStaleErrors() {
  ClearUnrecoverableError();
  last_actionable_error_ = SyncProtocolError();
  // Clear the data type errors as well.
  if (directory_data_type_manager_.get())
    directory_data_type_manager_->ResetDataTypeErrors();

}

void ProfileSyncService::ClearUnrecoverableError() {
  unrecoverable_error_reason_ = ERROR_REASON_UNSET;
  unrecoverable_error_message_.clear();
  unrecoverable_error_location_ = tracked_objects::Location();
}

// An invariant has been violated.  Transition to an error state where we try
// to do as little work as possible, to avoid further corruption or crashes.
void ProfileSyncService::OnUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  // Unrecoverable errors that arrive via the syncer::UnrecoverableErrorHandler
  // interface are assumed to originate within the syncer.
  unrecoverable_error_reason_ = ERROR_REASON_SYNCER;
  OnUnrecoverableErrorImpl(from_here, message, true);
}

void ProfileSyncService::OnUnrecoverableErrorImpl(
    const tracked_objects::Location& from_here,
    const std::string& message,
    bool delete_sync_database) {
  DCHECK(HasUnrecoverableError());
  unrecoverable_error_message_ = message;
  unrecoverable_error_location_ = from_here;

  UMA_HISTOGRAM_ENUMERATION(kSyncUnrecoverableErrorHistogram,
                            unrecoverable_error_reason_,
                            ERROR_REASON_LIMIT);
  std::string location;
  from_here.Write(true, true, &location);
  LOG(ERROR)
      << "Unrecoverable error detected at " << location
      << " -- ProfileSyncService unusable: " << message;

  // Shut all data types down.
  base::MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&ProfileSyncService::ShutdownImpl,
                 weak_factory_.GetWeakPtr(),
                 delete_sync_database ?
                     syncer::DISABLE_SYNC : syncer::STOP_SYNC));
}

void ProfileSyncService::ReenableDatatype(syncer::ModelType type) {
  if (!backend_initialized_)
    return;
  directory_data_type_manager_->ReenableType(type);
}

void ProfileSyncService::UpdateBackendInitUMA(bool success) {
  if (backend_mode_ != SYNC)
    return;

  is_first_time_sync_configure_ = !HasSyncSetupCompleted();

  if (is_first_time_sync_configure_) {
    UMA_HISTOGRAM_BOOLEAN("Sync.BackendInitializeFirstTimeSuccess", success);
  } else {
    UMA_HISTOGRAM_BOOLEAN("Sync.BackendInitializeRestoreSuccess", success);
  }

  base::Time on_backend_initialized_time = base::Time::Now();
  base::TimeDelta delta = on_backend_initialized_time -
      startup_controller_->start_backend_time();
  if (is_first_time_sync_configure_) {
    UMA_HISTOGRAM_LONG_TIMES("Sync.BackendInitializeFirstTime", delta);
  } else {
    UMA_HISTOGRAM_LONG_TIMES("Sync.BackendInitializeRestoreTime", delta);
  }
}

void ProfileSyncService::PostBackendInitialization() {
  // Never get here for backup / restore.
  DCHECK_EQ(backend_mode_, SYNC);

  if (last_backup_time_) {
    DCHECK(device_info_sync_service_);
    device_info_sync_service_->UpdateLocalDeviceBackupTime(*last_backup_time_);
  }

  if (protocol_event_observers_.might_have_observers()) {
    backend_->RequestBufferedProtocolEventsAndEnableForwarding();
  }

  non_blocking_data_type_manager_.ConnectSyncBackend(
      backend_->GetSyncContextProxy());

  if (type_debug_info_observers_.might_have_observers()) {
    backend_->EnableDirectoryTypeDebugInfoForwarding();
  }

  // If we have a cached passphrase use it to decrypt/encrypt data now that the
  // backend is initialized. We want to call this before notifying observers in
  // case this operation affects the "passphrase required" status.
  ConsumeCachedPassphraseIfPossible();

  // The very first time the backend initializes is effectively the first time
  // we can say we successfully "synced".  LastSyncedTime will only be null in
  // this case, because the pref wasn't restored on StartUp.
  if (sync_prefs_.GetLastSyncedTime().is_null()) {
    UpdateLastSyncedTime();
  }

  if (startup_controller_->auto_start_enabled() && !FirstSetupInProgress()) {
    // Backend is initialized but we're not in sync setup, so this must be an
    // autostart - mark our sync setup as completed and we'll start syncing
    // below.
    SetSyncSetupCompleted();
  }

  // Check HasSyncSetupCompleted() before NotifyObservers() to avoid spurious
  // data type configuration because observer may flag setup as complete and
  // trigger data type configuration.
  if (HasSyncSetupCompleted()) {
    ConfigureDataTypeManager();
  } else {
    DCHECK(FirstSetupInProgress());
  }

  NotifyObservers();
}

void ProfileSyncService::OnBackendInitialized(
    const syncer::WeakHandle<syncer::JsBackend>& js_backend,
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
        debug_info_listener,
    const std::string& cache_guid,
    bool success) {
  UpdateBackendInitUMA(success);

  if (!success) {
    // Something went unexpectedly wrong.  Play it safe: stop syncing at once
    // and surface error UI to alert the user sync has stopped.
    // Keep the directory around for now so that on restart we will retry
    // again and potentially succeed in presence of transient file IO failures
    // or permissions issues, etc.
    //
    // TODO(rlarocque): Consider making this UnrecoverableError less special.
    // Unlike every other UnrecoverableError, it does not delete our sync data.
    // This exception made sense at the time it was implemented, but our new
    // directory corruption recovery mechanism makes it obsolete.  By the time
    // we get here, we will have already tried and failed to delete the
    // directory.  It would be no big deal if we tried to delete it again.
    OnInternalUnrecoverableError(FROM_HERE,
                                 "BackendInitialize failure",
                                 false,
                                 ERROR_REASON_BACKEND_INIT_FAILURE);
    return;
  }

  backend_initialized_ = true;

  sync_js_controller_.AttachJsBackend(js_backend);
  debug_info_listener_ = debug_info_listener;

  SigninClient* signin_client =
      ChromeSigninClientFactory::GetForProfile(profile_);
  DCHECK(signin_client);
  std::string signin_scoped_device_id =
      signin_client->GetSigninScopedDeviceId();

  // Initialize local device info.
  local_device_->Initialize(cache_guid, signin_scoped_device_id);

  DVLOG(1) << "Setting preferred types for non-blocking DTM";
  non_blocking_data_type_manager_.SetPreferredTypes(GetPreferredDataTypes());

  // Give the DataTypeControllers a handle to the now initialized backend
  // as a UserShare.
  for (DataTypeController::TypeMap::iterator it =
       directory_data_type_controllers_.begin();
       it != directory_data_type_controllers_.end(); ++it) {
    it->second->OnUserShareReady(GetUserShare());
  }

  if (backend_mode_ == BACKUP || backend_mode_ == ROLLBACK)
    ConfigureDataTypeManager();
  else
    PostBackendInitialization();
}

void ProfileSyncService::OnSyncCycleCompleted() {
  UpdateLastSyncedTime();
  if (IsDataTypeControllerRunning(syncer::SESSIONS)) {
    // Trigger garbage collection of old sessions now that we've downloaded
    // any new session data.
    base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
        &SessionsSyncManager::DoGarbageCollection,
            base::AsWeakPtr(sessions_sync_manager_.get())));
  }
  DVLOG(2) << "Notifying observers sync cycle completed";
  NotifySyncCycleCompleted();
}

void ProfileSyncService::OnExperimentsChanged(
    const syncer::Experiments& experiments) {
  if (current_experiments_.Matches(experiments))
    return;

  current_experiments_ = experiments;

  profile()->GetPrefs()->SetBoolean(prefs::kInvalidationServiceUseGCMChannel,
                                    experiments.gcm_invalidations_enabled);
  profile()->GetPrefs()->SetBoolean(
      autofill::prefs::kAutofillWalletSyncExperimentEnabled,
      experiments.wallet_sync_enabled);
}

void ProfileSyncService::UpdateAuthErrorState(const AuthError& error) {
  is_auth_in_progress_ = false;
  last_auth_error_ = error;

  NotifyObservers();
}

namespace {

AuthError ConnectionStatusToAuthError(
    syncer::ConnectionStatus status) {
  switch (status) {
    case syncer::CONNECTION_OK:
      return AuthError::AuthErrorNone();
      break;
    case syncer::CONNECTION_AUTH_ERROR:
      return AuthError(AuthError::INVALID_GAIA_CREDENTIALS);
      break;
    case syncer::CONNECTION_SERVER_ERROR:
      return AuthError(AuthError::CONNECTION_FAILED);
      break;
    default:
      NOTREACHED();
      return AuthError(AuthError::CONNECTION_FAILED);
  }
}

}  // namespace

void ProfileSyncService::OnConnectionStatusChange(
    syncer::ConnectionStatus status) {
  connection_status_update_time_ = base::Time::Now();
  connection_status_ = status;
  if (status == syncer::CONNECTION_AUTH_ERROR) {
    // Sync server returned error indicating that access token is invalid. It
    // could be either expired or access is revoked. Let's request another
    // access token and if access is revoked then request for token will fail
    // with corresponding error. If access token is repeatedly reported
    // invalid, there may be some issues with server, e.g. authentication
    // state is inconsistent on sync and token server. In that case, we
    // backoff token requests exponentially to avoid hammering token server
    // too much and to avoid getting same token due to token server's caching
    // policy. |request_access_token_retry_timer_| is used to backoff request
    // triggered by both auth error and failure talking to GAIA server.
    // Therefore, we're likely to reach the backoff ceiling more quickly than
    // you would expect from looking at the BackoffPolicy if both types of
    // errors happen. We shouldn't receive two errors back-to-back without
    // attempting a token/sync request in between, thus crank up request delay
    // unnecessary. This is because we won't make a sync request if we hit an
    // error until GAIA succeeds at sending a new token, and we won't request
    // a new token unless sync reports a token failure. But to be safe, don't
    // schedule request if this happens.
    if (request_access_token_retry_timer_.IsRunning()) {
      // The timer to perform a request later is already running; nothing
      // further needs to be done at this point.
    } else if (request_access_token_backoff_.failure_count() == 0) {
      // First time request without delay. Currently invalid token is used
      // to initialize sync backend and we'll always end up here. We don't
      // want to delay initialization.
      request_access_token_backoff_.InformOfRequest(false);
      RequestAccessToken();
    } else  {
      request_access_token_backoff_.InformOfRequest(false);
      request_access_token_retry_timer_.Start(
          FROM_HERE,
          request_access_token_backoff_.GetTimeUntilRelease(),
          base::Bind(&ProfileSyncService::RequestAccessToken,
                     weak_factory_.GetWeakPtr()));
    }
  } else {
    // Reset backoff time after successful connection.
    if (status == syncer::CONNECTION_OK) {
      // Request shouldn't be scheduled at this time. But if it is, it's
      // possible that sync flips between OK and auth error states rapidly,
      // thus hammers token server. To be safe, only reset backoff delay when
      // no scheduled request.
      if (request_access_token_retry_timer_.IsRunning()) {
        NOTREACHED();
      } else {
        request_access_token_backoff_.Reset();
      }
    }

    const GoogleServiceAuthError auth_error =
        ConnectionStatusToAuthError(status);
    DVLOG(1) << "Connection status change: " << auth_error.ToString();
    UpdateAuthErrorState(auth_error);
  }
}

void ProfileSyncService::StopSyncingPermanently() {
  sync_prefs_.SetStartSuppressed(true);
  DisableForUser();
}

void ProfileSyncService::OnPassphraseRequired(
    syncer::PassphraseRequiredReason reason,
    const sync_pb::EncryptedData& pending_keys) {
  DCHECK(backend_.get());
  DCHECK(backend_->IsNigoriEnabled());

  // TODO(lipalani) : add this check to other locations as well.
  if (HasUnrecoverableError()) {
    // When unrecoverable error is detected we post a task to shutdown the
    // backend. The task might not have executed yet.
    return;
  }

  DVLOG(1) << "Passphrase required with reason: "
           << syncer::PassphraseRequiredReasonToString(reason);
  passphrase_required_reason_ = reason;

  const syncer::ModelTypeSet types = GetPreferredDirectoryDataTypes();
  if (directory_data_type_manager_) {
    // Reconfigure without the encrypted types (excluded implicitly via the
    // failed datatypes handler).
    directory_data_type_manager_->Configure(types,
                                            syncer::CONFIGURE_REASON_CRYPTO);
  }

  // TODO(rlarocque): Support non-blocking types.  http://crbug.com/351005.

  // Notify observers that the passphrase status may have changed.
  NotifyObservers();
}

void ProfileSyncService::OnPassphraseAccepted() {
  DVLOG(1) << "Received OnPassphraseAccepted.";

  // If the pending keys were resolved via keystore, it's possible we never
  // consumed our cached passphrase. Clear it now.
  if (!cached_passphrase_.empty())
    cached_passphrase_.clear();

  // Reset passphrase_required_reason_ since we know we no longer require the
  // passphrase. We do this here rather than down in ResolvePassphraseRequired()
  // because that can be called by OnPassphraseRequired() if no encrypted data
  // types are enabled, and we don't want to clobber the true passphrase error.
  passphrase_required_reason_ = syncer::REASON_PASSPHRASE_NOT_REQUIRED;

  // Make sure the data types that depend on the passphrase are started at
  // this time.
  const syncer::ModelTypeSet types = GetPreferredDirectoryDataTypes();
  if (directory_data_type_manager_) {
    // Re-enable any encrypted types if necessary.
    directory_data_type_manager_->Configure(types,
                                            syncer::CONFIGURE_REASON_CRYPTO);
  }

  // TODO(rlarocque): Support non-blocking types.  http://crbug.com/351005.

  NotifyObservers();
}

void ProfileSyncService::OnEncryptedTypesChanged(
    syncer::ModelTypeSet encrypted_types,
    bool encrypt_everything) {
  encrypted_types_ = encrypted_types;
  encrypt_everything_ = encrypt_everything;
  DCHECK(encrypt_everything_allowed_ || !encrypt_everything_);
  DVLOG(1) << "Encrypted types changed to "
           << syncer::ModelTypeSetToString(encrypted_types_)
           << " (encrypt everything is set to "
           << (encrypt_everything_ ? "true" : "false") << ")";
  DCHECK(encrypted_types_.Has(syncer::PASSWORDS));

  NotifyObservers();
}

void ProfileSyncService::OnEncryptionComplete() {
  DVLOG(1) << "Encryption complete";
  if (encryption_pending_ && encrypt_everything_) {
    encryption_pending_ = false;
    // This is to nudge the integration tests when encryption is
    // finished.
    NotifyObservers();
  }
}

void ProfileSyncService::OnMigrationNeededForTypes(
    syncer::ModelTypeSet types) {
  DCHECK(backend_initialized_);
  DCHECK(directory_data_type_manager_.get());

  // Migrator must be valid, because we don't sync until it is created and this
  // callback originates from a sync cycle.
  migrator_->MigrateTypes(types);
}

void ProfileSyncService::OnActionableError(const SyncProtocolError& error) {
  last_actionable_error_ = error;
  DCHECK_NE(last_actionable_error_.action,
            syncer::UNKNOWN_ACTION);
  switch (error.action) {
    case syncer::UPGRADE_CLIENT:
    case syncer::CLEAR_USER_DATA_AND_RESYNC:
    case syncer::ENABLE_SYNC_ON_ACCOUNT:
    case syncer::STOP_AND_RESTART_SYNC:
      // TODO(lipalani) : if setup in progress we want to display these
      // actions in the popup. The current experience might not be optimal for
      // the user. We just dismiss the dialog.
      if (startup_controller_->setup_in_progress()) {
        StopSyncingPermanently();
        expect_sync_configuration_aborted_ = true;
      }
      // Trigger an unrecoverable error to stop syncing.
      OnInternalUnrecoverableError(FROM_HERE,
                                   last_actionable_error_.error_description,
                                   true,
                                   ERROR_REASON_ACTIONABLE_ERROR);
      break;
    case syncer::DISABLE_SYNC_AND_ROLLBACK:
      backup_rollback_controller_->OnRollbackReceived();
      // Fall through to shutdown backend and sign user out.
    case syncer::DISABLE_SYNC_ON_CLIENT:
      StopSyncingPermanently();
#if !defined(OS_CHROMEOS)
      // On desktop Chrome, sign out the user after a dashboard clear.
      // Skip sign out on ChromeOS/Android.
      if (!startup_controller_->auto_start_enabled()) {
        SigninManagerFactory::GetForProfile(profile_)->SignOut(
            signin_metrics::SERVER_FORCED_DISABLE);
      }
#endif
      break;
    case syncer::ROLLBACK_DONE:
      backup_rollback_controller_->OnRollbackDone();
      break;
    case syncer::STOP_SYNC_FOR_DISABLED_ACCOUNT:
      // Sync disabled by domain admin. we should stop syncing until next
      // restart.
      sync_disabled_by_admin_ = true;
      ShutdownImpl(syncer::DISABLE_SYNC);
      break;
    default:
      NOTREACHED();
  }
  NotifyObservers();

  if (error.action == syncer::DISABLE_SYNC_ON_CLIENT ||
      (error.action == syncer::DISABLE_SYNC_AND_ROLLBACK &&
          !backup_rollback_controller_->StartRollback())) {
    // Clean up backup data for sign-out only or when rollback is disabled.
    CleanUpBackup();
  } else if (error.action == syncer::ROLLBACK_DONE) {
    // Shut down ROLLBACK backend and delete backup DB.
    ShutdownImpl(syncer::DISABLE_SYNC);
    sync_prefs_.ClearFirstSyncTime();
  }
}

void ProfileSyncService::OnConfigureDone(
    const DataTypeManager::ConfigureResult& result) {
  configure_status_ = result.status;
  data_type_status_table_ = result.data_type_status_table;

  if (backend_mode_ != SYNC) {
    if (configure_status_ == DataTypeManager::OK) {
      StartSyncingWithServer();

      // Backup is done after models are associated.
      if (backend_mode_ == BACKUP)
        backup_finished_ = true;

      // Asynchronously check whether sync needs to start.
      base::MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(&ProfileSyncService::TryStartSyncAfterBackup,
                                startup_controller_weak_factory_.GetWeakPtr()));
    } else if (!expect_sync_configuration_aborted_) {
      DVLOG(1) << "Backup/rollback backend failed to configure.";
      ShutdownImpl(syncer::STOP_SYNC);
    }

    return;
  }

  // We should have cleared our cached passphrase before we get here (in
  // OnBackendInitialized()).
  DCHECK(cached_passphrase_.empty());

  if (!sync_configure_start_time_.is_null()) {
    if (result.status == DataTypeManager::OK) {
      base::Time sync_configure_stop_time = base::Time::Now();
      base::TimeDelta delta = sync_configure_stop_time -
          sync_configure_start_time_;
      if (is_first_time_sync_configure_) {
        UMA_HISTOGRAM_LONG_TIMES("Sync.ServiceInitialConfigureTime", delta);
      } else {
        UMA_HISTOGRAM_LONG_TIMES("Sync.ServiceSubsequentConfigureTime",
                                  delta);
      }
    }
    sync_configure_start_time_ = base::Time();
  }

  // Notify listeners that configuration is done.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SYNC_CONFIGURE_DONE,
      content::Source<ProfileSyncService>(this),
      content::NotificationService::NoDetails());

  DVLOG(1) << "PSS OnConfigureDone called with status: " << configure_status_;
  // The possible status values:
  //    ABORT - Configuration was aborted. This is not an error, if
  //            initiated by user.
  //    OK - Some or all types succeeded.
  //    Everything else is an UnrecoverableError. So treat it as such.

  // First handle the abort case.
  if (configure_status_ == DataTypeManager::ABORTED &&
      expect_sync_configuration_aborted_) {
    DVLOG(0) << "ProfileSyncService::Observe Sync Configure aborted";
    expect_sync_configuration_aborted_ = false;
    return;
  }

  // Handle unrecoverable error.
  if (configure_status_ != DataTypeManager::OK) {
    // Something catastrophic had happened. We should only have one
    // error representing it.
    syncer::SyncError error =
        data_type_status_table_.GetUnrecoverableError();
    DCHECK(error.IsSet());
    std::string message =
        "Sync configuration failed with status " +
        DataTypeManager::ConfigureStatusToString(configure_status_) +
        " caused by " +
        syncer::ModelTypeSetToString(
            data_type_status_table_.GetUnrecoverableErrorTypes()) +
        ": " + error.message();
    LOG(ERROR) << "ProfileSyncService error: " << message;
    OnInternalUnrecoverableError(error.location(),
                                 message,
                                 true,
                                 ERROR_REASON_CONFIGURATION_FAILURE);
    return;
  }

  // We should never get in a state where we have no encrypted datatypes
  // enabled, and yet we still think we require a passphrase for decryption.
  DCHECK(!(IsPassphraseRequiredForDecryption() &&
           !IsEncryptedDatatypeEnabled()));

  // This must be done before we start syncing with the server to avoid
  // sending unencrypted data up on a first time sync.
  if (encryption_pending_)
    backend_->EnableEncryptEverything();
  NotifyObservers();

  if (migrator_.get() &&
      migrator_->state() != browser_sync::BackendMigrator::IDLE) {
    // Migration in progress.  Let the migrator know we just finished
    // configuring something.  It will be up to the migrator to call
    // StartSyncingWithServer() if migration is now finished.
    migrator_->OnConfigureDone(result);
  } else {
    StartSyncingWithServer();
  }
}

void ProfileSyncService::OnConfigureStart() {
  sync_configure_start_time_ = base::Time::Now();
  NotifyObservers();
}

ProfileSyncService::SyncStatusSummary
      ProfileSyncService::QuerySyncStatusSummary() {
  if (HasUnrecoverableError()) {
    return UNRECOVERABLE_ERROR;
  } else if (!backend_) {
    return NOT_ENABLED;
  } else if (backend_mode_ == BACKUP) {
    return BACKUP_USER_DATA;
  } else if (backend_mode_ == ROLLBACK) {
    return ROLLBACK_USER_DATA;
  } else if (backend_.get() && !HasSyncSetupCompleted()) {
    return SETUP_INCOMPLETE;
  } else if (
      backend_.get() && HasSyncSetupCompleted() &&
      directory_data_type_manager_.get() &&
      directory_data_type_manager_->state() == DataTypeManager::STOPPED) {
    return DATATYPES_NOT_INITIALIZED;
  } else if (SyncActive()) {
    return INITIALIZED;
  }
  return UNKNOWN_ERROR;
}

std::string ProfileSyncService::QuerySyncStatusSummaryString() {
  SyncStatusSummary status = QuerySyncStatusSummary();

  std::string config_status_str =
      configure_status_ != DataTypeManager::UNKNOWN ?
          DataTypeManager::ConfigureStatusToString(configure_status_) : "";

  switch (status) {
    case UNRECOVERABLE_ERROR:
      return "Unrecoverable error detected";
    case NOT_ENABLED:
      return "Syncing not enabled";
    case SETUP_INCOMPLETE:
      return "First time sync setup incomplete";
    case DATATYPES_NOT_INITIALIZED:
      return "Datatypes not fully initialized";
    case INITIALIZED:
      return "Sync service initialized";
    case BACKUP_USER_DATA:
      return "Backing-up user data. Status: " + config_status_str;
    case ROLLBACK_USER_DATA:
      return "Restoring user data. Status: " + config_status_str;
    default:
      return "Status unknown: Internal error?";
  }
}

std::string ProfileSyncService::GetBackendInitializationStateString() const {
  return startup_controller_->GetBackendInitializationStateString();
}

bool ProfileSyncService::auto_start_enabled() const {
  return startup_controller_->auto_start_enabled();
}

bool ProfileSyncService::setup_in_progress() const {
  return startup_controller_->setup_in_progress();
}

bool ProfileSyncService::QueryDetailedSyncStatus(
    SyncBackendHost::Status* result) {
  if (backend_.get() && backend_initialized_) {
    *result = backend_->GetDetailedStatus();
    return true;
  } else {
    SyncBackendHost::Status status;
    status.sync_protocol_error = last_actionable_error_;
    *result = status;
    return false;
  }
}

const AuthError& ProfileSyncService::GetAuthError() const {
  return last_auth_error_;
}

bool ProfileSyncService::FirstSetupInProgress() const {
  return !HasSyncSetupCompleted() && startup_controller_->setup_in_progress();
}

void ProfileSyncService::SetSetupInProgress(bool setup_in_progress) {
  // This method is a no-op if |setup_in_progress_| remains unchanged.
  if (startup_controller_->setup_in_progress() == setup_in_progress)
    return;

  startup_controller_->set_setup_in_progress(setup_in_progress);
  if (!setup_in_progress && backend_initialized())
    ReconfigureDatatypeManager();
  NotifyObservers();
}

bool ProfileSyncService::SyncActive() const {
  return backend_initialized_ && backend_mode_ == SYNC &&
         directory_data_type_manager_ &&
         directory_data_type_manager_->state() != DataTypeManager::STOPPED;
}

bool ProfileSyncService::backend_initialized() const {
  return backend_initialized_;
}

ProfileSyncService::BackendMode ProfileSyncService::backend_mode() const {
  return backend_mode_;
}

bool ProfileSyncService::ConfigurationDone() const {
  return directory_data_type_manager_ &&
      directory_data_type_manager_->state() == DataTypeManager::CONFIGURED;
}

bool ProfileSyncService::waiting_for_auth() const {
  return is_auth_in_progress_;
}

const syncer::Experiments& ProfileSyncService::current_experiments() const {
  return current_experiments_;
}

bool ProfileSyncService::HasUnrecoverableError() const {
  return unrecoverable_error_reason_ != ERROR_REASON_UNSET;
}

bool ProfileSyncService::IsPassphraseRequired() const {
  return passphrase_required_reason_ !=
      syncer::REASON_PASSPHRASE_NOT_REQUIRED;
}

bool ProfileSyncService::IsPassphraseRequiredForDecryption() const {
  // If there is an encrypted datatype enabled and we don't have the proper
  // passphrase, we must prompt the user for a passphrase. The only way for the
  // user to avoid entering their passphrase is to disable the encrypted types.
  return IsEncryptedDatatypeEnabled() && IsPassphraseRequired();
}

base::string16 ProfileSyncService::GetLastSyncedTimeString() const {
  const base::Time last_synced_time = sync_prefs_.GetLastSyncedTime();
  if (last_synced_time.is_null())
    return l10n_util::GetStringUTF16(IDS_SYNC_TIME_NEVER);

  base::TimeDelta time_since_last_sync = base::Time::Now() - last_synced_time;

  if (time_since_last_sync < base::TimeDelta::FromMinutes(1))
    return l10n_util::GetStringUTF16(IDS_SYNC_TIME_JUST_NOW);

  return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                                ui::TimeFormat::LENGTH_SHORT,
                                time_since_last_sync);
}

void ProfileSyncService::UpdateSelectedTypesHistogram(
    bool sync_everything, const syncer::ModelTypeSet chosen_types) const {
  if (!HasSyncSetupCompleted() ||
      sync_everything != sync_prefs_.HasKeepEverythingSynced()) {
    UMA_HISTOGRAM_BOOLEAN("Sync.SyncEverything", sync_everything);
  }

  // Only log the data types that are shown in the sync settings ui.
  // Note: the order of these types must match the ordering of
  // the respective types in ModelType
  const sync_driver::user_selectable_type::UserSelectableSyncType
      user_selectable_types[] = {
    sync_driver::user_selectable_type::BOOKMARKS,
    sync_driver::user_selectable_type::PREFERENCES,
    sync_driver::user_selectable_type::PASSWORDS,
    sync_driver::user_selectable_type::AUTOFILL,
    sync_driver::user_selectable_type::THEMES,
    sync_driver::user_selectable_type::TYPED_URLS,
    sync_driver::user_selectable_type::EXTENSIONS,
    sync_driver::user_selectable_type::APPS,
    sync_driver::user_selectable_type::WIFI_CREDENTIAL,
    sync_driver::user_selectable_type::PROXY_TABS,
  };

  static_assert(35 == syncer::MODEL_TYPE_COUNT,
                "custom config histogram must be updated");

  if (!sync_everything) {
    const syncer::ModelTypeSet current_types = GetPreferredDataTypes();

    syncer::ModelTypeSet type_set = syncer::UserSelectableTypes();
    syncer::ModelTypeSet::Iterator it = type_set.First();

    DCHECK_EQ(arraysize(user_selectable_types), type_set.Size());

    for (size_t i = 0; i < arraysize(user_selectable_types) && it.Good();
         ++i, it.Inc()) {
      const syncer::ModelType type = it.Get();
      if (chosen_types.Has(type) &&
          (!HasSyncSetupCompleted() || !current_types.Has(type))) {
        // Selected type has changed - log it.
        UMA_HISTOGRAM_ENUMERATION(
            "Sync.CustomSync",
            user_selectable_types[i],
            sync_driver::user_selectable_type::SELECTABLE_DATATYPE_COUNT + 1);
      }
    }
  }
}

#if defined(OS_CHROMEOS)
void ProfileSyncService::RefreshSpareBootstrapToken(
    const std::string& passphrase) {
  sync_driver::SystemEncryptor encryptor;
  syncer::Cryptographer temp_cryptographer(&encryptor);
  // The first 2 params (hostname and username) doesn't have any effect here.
  syncer::KeyParams key_params = {"localhost", "dummy", passphrase};

  std::string bootstrap_token;
  if (!temp_cryptographer.AddKey(key_params)) {
    NOTREACHED() << "Failed to add key to cryptographer.";
  }
  temp_cryptographer.GetBootstrapToken(&bootstrap_token);
  sync_prefs_.SetSpareBootstrapToken(bootstrap_token);
}
#endif

void ProfileSyncService::OnUserChoseDatatypes(
    bool sync_everything,
    syncer::ModelTypeSet chosen_types) {
  if (!backend_.get() && !HasUnrecoverableError()) {
    NOTREACHED();
    return;
  }

  UpdateSelectedTypesHistogram(sync_everything, chosen_types);
  sync_prefs_.SetKeepEverythingSynced(sync_everything);

  if (directory_data_type_manager_.get())
    directory_data_type_manager_->ResetDataTypeErrors();
  ChangePreferredDataTypes(chosen_types);
}

void ProfileSyncService::ChangePreferredDataTypes(
    syncer::ModelTypeSet preferred_types) {

  DVLOG(1) << "ChangePreferredDataTypes invoked";
  const syncer::ModelTypeSet registered_types = GetRegisteredDataTypes();
  const syncer::ModelTypeSet registered_preferred_types =
      Intersection(registered_types, preferred_types);
  sync_prefs_.SetPreferredDataTypes(registered_types,
                                    registered_preferred_types);

  // Now reconfigure the DTM.
  ReconfigureDatatypeManager();

  // TODO(rlarocque): Reconfigure the NonBlockingDataTypeManager, too.  Blocked
  // on crbug.com/368834.  Until that bug is fixed, it's difficult to tell
  // which types should be enabled and when.
}

syncer::ModelTypeSet ProfileSyncService::GetActiveDataTypes() const {
  if (!SyncActive() || !ConfigurationDone())
    return syncer::ModelTypeSet();
  const syncer::ModelTypeSet preferred_types = GetPreferredDataTypes();
  const syncer::ModelTypeSet failed_types =
      data_type_status_table_.GetFailedTypes();
  return Difference(preferred_types, failed_types);
}

syncer::ModelTypeSet ProfileSyncService::GetPreferredDataTypes() const {
  const syncer::ModelTypeSet registered_types = GetRegisteredDataTypes();
  const syncer::ModelTypeSet preferred_types =
      sync_prefs_.GetPreferredDataTypes(registered_types);
  const syncer::ModelTypeSet enforced_types =
      Intersection(GetDataTypesFromPreferenceProviders(), registered_types);
  return Union(preferred_types, enforced_types);
}

syncer::ModelTypeSet
ProfileSyncService::GetPreferredDirectoryDataTypes() const {
  const syncer::ModelTypeSet registered_directory_types =
      GetRegisteredDirectoryDataTypes();
  const syncer::ModelTypeSet preferred_types =
      sync_prefs_.GetPreferredDataTypes(registered_directory_types);
  const syncer::ModelTypeSet enforced_types =
      Intersection(GetDataTypesFromPreferenceProviders(),
                   registered_directory_types);
  return Union(preferred_types, enforced_types);
}

syncer::ModelTypeSet
ProfileSyncService::GetPreferredNonBlockingDataTypes() const {
  return sync_prefs_.GetPreferredDataTypes(GetRegisteredNonBlockingDataTypes());
}

syncer::ModelTypeSet ProfileSyncService::GetForcedDataTypes() const {
  // TODO(treib,zea): When SyncPrefs also implements SyncTypePreferenceProvider,
  // we'll need another way to distinguish user-choosable types from
  // programmatically-enabled types.
  return GetDataTypesFromPreferenceProviders();
}

syncer::ModelTypeSet ProfileSyncService::GetRegisteredDataTypes() const {
  return Union(GetRegisteredDirectoryDataTypes(),
               GetRegisteredNonBlockingDataTypes());
}

syncer::ModelTypeSet
ProfileSyncService::GetRegisteredDirectoryDataTypes() const {
  syncer::ModelTypeSet registered_types;
  // The directory_data_type_controllers_ are determined by command-line flags;
  // that's effectively what controls the values returned here.
  for (DataTypeController::TypeMap::const_iterator it =
       directory_data_type_controllers_.begin();
       it != directory_data_type_controllers_.end(); ++it) {
    registered_types.Put(it->first);
  }
  return registered_types;
}

syncer::ModelTypeSet
ProfileSyncService::GetRegisteredNonBlockingDataTypes() const {
  return non_blocking_data_type_manager_.GetRegisteredTypes();
}

bool ProfileSyncService::IsUsingSecondaryPassphrase() const {
  syncer::PassphraseType passphrase_type = GetPassphraseType();
  return passphrase_type == syncer::FROZEN_IMPLICIT_PASSPHRASE ||
         passphrase_type == syncer::CUSTOM_PASSPHRASE;
}

syncer::PassphraseType ProfileSyncService::GetPassphraseType() const {
  return backend_->GetPassphraseType();
}

base::Time ProfileSyncService::GetExplicitPassphraseTime() const {
  return backend_->GetExplicitPassphraseTime();
}

bool ProfileSyncService::IsCryptographerReady(
    const syncer::BaseTransaction* trans) const {
  return backend_.get() && backend_->IsCryptographerReady(trans);
}

void ProfileSyncService::ConfigurePriorityDataTypes() {
  const syncer::ModelTypeSet priority_types =
      Intersection(GetPreferredDirectoryDataTypes(),
                   syncer::PriorityUserTypes());
  if (!priority_types.Empty()) {
    const syncer::ConfigureReason reason = HasSyncSetupCompleted() ?
        syncer::CONFIGURE_REASON_RECONFIGURATION :
        syncer::CONFIGURE_REASON_NEW_CLIENT;
    directory_data_type_manager_->Configure(priority_types, reason);
  }
}

void ProfileSyncService::ConfigureDataTypeManager() {
  // Don't configure datatypes if the setup UI is still on the screen - this
  // is to help multi-screen setting UIs (like iOS) where they don't want to
  // start syncing data until the user is done configuring encryption options,
  // etc. ReconfigureDatatypeManager() will get called again once the UI calls
  // SetSetupInProgress(false).
  if (backend_mode_ == SYNC && startup_controller_->setup_in_progress())
    return;

  bool restart = false;
  if (!directory_data_type_manager_) {
    restart = true;
    directory_data_type_manager_.reset(
        factory_->CreateDataTypeManager(debug_info_listener_,
                                        &directory_data_type_controllers_,
                                        this,
                                        backend_.get(),
                                        this));

    // We create the migrator at the same time.
    migrator_.reset(
        new browser_sync::BackendMigrator(
            profile_->GetDebugName(), GetUserShare(),
            this, directory_data_type_manager_.get(),
            base::Bind(&ProfileSyncService::StartSyncingWithServer,
                       base::Unretained(this))));
  }

  syncer::ModelTypeSet types;
  syncer::ConfigureReason reason = syncer::CONFIGURE_REASON_UNKNOWN;
  if (backend_mode_ == BACKUP || backend_mode_ == ROLLBACK) {
    types = syncer::BackupTypes();
    reason = syncer::CONFIGURE_REASON_BACKUP_ROLLBACK;
  } else {
    types = GetPreferredDirectoryDataTypes();
    if (!HasSyncSetupCompleted()) {
      reason = syncer::CONFIGURE_REASON_NEW_CLIENT;
    } else if (restart) {
      // Datatype downloads on restart are generally due to newly supported
      // datatypes (although it's also possible we're picking up where a failed
      // previous configuration left off).
      // TODO(sync): consider detecting configuration recovery and setting
      // the reason here appropriately.
      reason = syncer::CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE;
    } else {
      // The user initiated a reconfiguration (either to add or remove types).
      reason = syncer::CONFIGURE_REASON_RECONFIGURATION;
    }
  }

  directory_data_type_manager_->Configure(types, reason);
}

syncer::UserShare* ProfileSyncService::GetUserShare() const {
  if (backend_.get() && backend_initialized_) {
    return backend_->GetUserShare();
  }
  NOTREACHED();
  return NULL;
}

syncer::sessions::SyncSessionSnapshot
ProfileSyncService::GetLastSessionSnapshot() const {
  if (backend_)
    return backend_->GetLastSessionSnapshot();
  return syncer::sessions::SyncSessionSnapshot();
}

bool ProfileSyncService::HasUnsyncedItems() const {
  if (HasSyncingBackend() && backend_initialized_) {
    return backend_->HasUnsyncedItems();
  }
  NOTREACHED();
  return false;
}

browser_sync::BackendMigrator*
ProfileSyncService::GetBackendMigratorForTest() {
  return migrator_.get();
}

void ProfileSyncService::GetModelSafeRoutingInfo(
    syncer::ModelSafeRoutingInfo* out) const {
  if (backend_.get() && backend_initialized_) {
    backend_->GetModelSafeRoutingInfo(out);
  } else {
    NOTREACHED();
  }
}

base::Value* ProfileSyncService::GetTypeStatusMap() const {
  scoped_ptr<base::ListValue> result(new base::ListValue());

  if (!backend_.get() || !backend_initialized_) {
    return result.release();
  }

  DataTypeStatusTable::TypeErrorMap error_map =
      data_type_status_table_.GetAllErrors();
  ModelTypeSet active_types;
  ModelTypeSet passive_types;
  ModelSafeRoutingInfo routing_info;
  backend_->GetModelSafeRoutingInfo(&routing_info);
  for (ModelSafeRoutingInfo::const_iterator it = routing_info.begin();
       it != routing_info.end(); ++it) {
    if (it->second == syncer::GROUP_PASSIVE) {
      passive_types.Put(it->first);
    } else {
      active_types.Put(it->first);
    }
  }

  SyncBackendHost::Status detailed_status = backend_->GetDetailedStatus();
  ModelTypeSet &throttled_types(detailed_status.throttled_types);
  ModelTypeSet registered = GetRegisteredDataTypes();
  scoped_ptr<base::DictionaryValue> type_status_header(
      new base::DictionaryValue());

  type_status_header->SetString("name", "Model Type");
  type_status_header->SetString("status", "header");
  type_status_header->SetString("value", "Group Type");
  type_status_header->SetString("num_entries", "Total Entries");
  type_status_header->SetString("num_live", "Live Entries");
  result->Append(type_status_header.release());

  scoped_ptr<base::DictionaryValue> type_status;
  for (ModelTypeSet::Iterator it = registered.First(); it.Good(); it.Inc()) {
    ModelType type = it.Get();

    type_status.reset(new base::DictionaryValue());
    type_status->SetString("name", ModelTypeToString(type));

    if (error_map.find(type) != error_map.end()) {
      const syncer::SyncError &error = error_map.find(type)->second;
      DCHECK(error.IsSet());
      switch (error.GetSeverity()) {
        case syncer::SyncError::SYNC_ERROR_SEVERITY_ERROR: {
            std::string error_text = "Error: " + error.location().ToString() +
                ", " + error.GetMessagePrefix() + error.message();
            type_status->SetString("status", "error");
            type_status->SetString("value", error_text);
          }
          break;
        case syncer::SyncError::SYNC_ERROR_SEVERITY_INFO:
          type_status->SetString("status", "disabled");
          type_status->SetString("value", error.message());
          break;
        default:
          NOTREACHED() << "Unexpected error severity.";
          break;
      }
    } else if (syncer::IsProxyType(type) && passive_types.Has(type)) {
      // Show a proxy type in "ok" state unless it is disabled by user.
      DCHECK(!throttled_types.Has(type));
      type_status->SetString("status", "ok");
      type_status->SetString("value", "Passive");
    } else if (throttled_types.Has(type) && passive_types.Has(type)) {
      type_status->SetString("status", "warning");
      type_status->SetString("value", "Passive, Throttled");
    } else if (passive_types.Has(type)) {
      type_status->SetString("status", "warning");
      type_status->SetString("value", "Passive");
    } else if (throttled_types.Has(type)) {
      type_status->SetString("status", "warning");
      type_status->SetString("value", "Throttled");
    } else if (GetRegisteredNonBlockingDataTypes().Has(type)) {
      type_status->SetString("status", "ok");
      type_status->SetString("value", "Non-Blocking");
    } else if (active_types.Has(type)) {
      type_status->SetString("status", "ok");
      type_status->SetString("value", "Active: " +
                             ModelSafeGroupToString(routing_info[type]));
    } else {
      type_status->SetString("status", "warning");
      type_status->SetString("value", "Disabled by User");
    }

    int live_count = detailed_status.num_entries_by_type[type] -
        detailed_status.num_to_delete_entries_by_type[type];
    type_status->SetInteger("num_entries",
                            detailed_status.num_entries_by_type[type]);
    type_status->SetInteger("num_live", live_count);

    result->Append(type_status.release());
  }
  return result.release();
}

void ProfileSyncService::DeactivateDataType(syncer::ModelType type) {
  if (!backend_)
    return;
  backend_->DeactivateDataType(type);
}

void ProfileSyncService::ConsumeCachedPassphraseIfPossible() {
  // If no cached passphrase, or sync backend hasn't started up yet, just exit.
  // If the backend isn't running yet, OnBackendInitialized() will call this
  // method again after the backend starts up.
  if (cached_passphrase_.empty() || !backend_initialized())
    return;

  // Backend is up and running, so we can consume the cached passphrase.
  std::string passphrase = cached_passphrase_;
  cached_passphrase_.clear();

  // If we need a passphrase to decrypt data, try the cached passphrase.
  if (passphrase_required_reason() == syncer::REASON_DECRYPTION) {
    if (SetDecryptionPassphrase(passphrase)) {
      DVLOG(1) << "Cached passphrase successfully decrypted pending keys";
      return;
    }
  }

  // If we get here, we don't have pending keys (or at least, the passphrase
  // doesn't decrypt them) - just try to re-encrypt using the encryption
  // passphrase.
  if (!IsUsingSecondaryPassphrase())
    SetEncryptionPassphrase(passphrase, IMPLICIT);
}

void ProfileSyncService::RequestAccessToken() {
  // Only one active request at a time.
  if (access_token_request_ != NULL)
    return;
  request_access_token_retry_timer_.Stop();
  OAuth2TokenService::ScopeSet oauth2_scopes;
  oauth2_scopes.insert(signin_->GetSyncScopeToUse());

  // Invalidate previous token, otherwise token service will return the same
  // token again.
  const std::string& account_id = signin_->GetAccountIdToUse();
  if (!access_token_.empty()) {
    oauth2_token_service_->InvalidateToken(
        account_id, oauth2_scopes, access_token_);
  }

  access_token_.clear();

  token_request_time_ = base::Time::Now();
  token_receive_time_ = base::Time();
  next_token_request_time_ = base::Time();
  access_token_request_ =
      oauth2_token_service_->StartRequest(account_id, oauth2_scopes, this);
}

void ProfileSyncService::SetEncryptionPassphrase(const std::string& passphrase,
                                                 PassphraseType type) {
  // This should only be called when the backend has been initialized.
  DCHECK(backend_initialized());
  DCHECK(!(type == IMPLICIT && IsUsingSecondaryPassphrase())) <<
      "Data is already encrypted using an explicit passphrase";
  DCHECK(!(type == EXPLICIT &&
           passphrase_required_reason_ == syncer::REASON_DECRYPTION)) <<
         "Can not set explicit passphrase when decryption is needed.";

  DVLOG(1) << "Setting " << (type == EXPLICIT ? "explicit" : "implicit")
           << " passphrase for encryption.";
  if (passphrase_required_reason_ == syncer::REASON_ENCRYPTION) {
    // REASON_ENCRYPTION implies that the cryptographer does not have pending
    // keys. Hence, as long as we're not trying to do an invalid passphrase
    // change (e.g. explicit -> explicit or explicit -> implicit), we know this
    // will succeed. If for some reason a new encryption key arrives via
    // sync later, the SBH will trigger another OnPassphraseRequired().
    passphrase_required_reason_ = syncer::REASON_PASSPHRASE_NOT_REQUIRED;
    NotifyObservers();
  }
  backend_->SetEncryptionPassphrase(passphrase, type == EXPLICIT);
}

bool ProfileSyncService::SetDecryptionPassphrase(
    const std::string& passphrase) {
  if (IsPassphraseRequired()) {
    DVLOG(1) << "Setting passphrase for decryption.";
    bool result = backend_->SetDecryptionPassphrase(passphrase);
    UMA_HISTOGRAM_BOOLEAN("Sync.PassphraseDecryptionSucceeded", result);
    return result;
  } else {
    NOTREACHED() << "SetDecryptionPassphrase must not be called when "
                    "IsPassphraseRequired() is false.";
    return false;
  }
}

bool ProfileSyncService::EncryptEverythingAllowed() const {
  return encrypt_everything_allowed_;
}

void ProfileSyncService::SetEncryptEverythingAllowed(bool allowed) {
  DCHECK(allowed || !backend_initialized() || !EncryptEverythingEnabled());
  encrypt_everything_allowed_ = allowed;
}

void ProfileSyncService::EnableEncryptEverything() {
  DCHECK(EncryptEverythingAllowed());

  // Tests override backend_initialized() to always return true, so we
  // must check that instead of |backend_initialized_|.
  // TODO(akalin): Fix the above. :/
  DCHECK(backend_initialized());
  // TODO(atwilson): Persist the encryption_pending_ flag to address the various
  // problems around cancelling encryption in the background (crbug.com/119649).
  if (!encrypt_everything_)
    encryption_pending_ = true;
}

bool ProfileSyncService::encryption_pending() const {
  // We may be called during the setup process before we're
  // initialized (via IsEncryptedDatatypeEnabled and
  // IsPassphraseRequiredForDecryption).
  return encryption_pending_;
}

bool ProfileSyncService::EncryptEverythingEnabled() const {
  DCHECK(backend_initialized_);
  return encrypt_everything_ || encryption_pending_;
}

syncer::ModelTypeSet ProfileSyncService::GetEncryptedDataTypes() const {
  DCHECK(encrypted_types_.Has(syncer::PASSWORDS));
  // We may be called during the setup process before we're
  // initialized.  In this case, we default to the sensitive types.
  return encrypted_types_;
}

void ProfileSyncService::OnSyncManagedPrefChange(bool is_sync_managed) {
  if (is_sync_managed) {
    DisableForUser();
  } else {
    // Sync is no longer disabled by policy. Try starting it up if appropriate.
    startup_controller_->TryStart();
  }
}

void ProfileSyncService::GoogleSigninSucceeded(const std::string& account_id,
                                               const std::string& username,
                                               const std::string& password) {
  if (!sync_prefs_.IsStartSuppressed() && !password.empty()) {
    cached_passphrase_ = password;
    // Try to consume the passphrase we just cached. If the sync backend
    // is not running yet, the passphrase will remain cached until the
    // backend starts up.
    ConsumeCachedPassphraseIfPossible();
  }
#if defined(OS_CHROMEOS)
  RefreshSpareBootstrapToken(password);
#endif
  if (!backend_initialized() || GetAuthError().state() != AuthError::NONE) {
    // Track the fact that we're still waiting for auth to complete.
    is_auth_in_progress_ = true;
  }
}

void ProfileSyncService::GoogleSignedOut(const std::string& account_id,
                                         const std::string& username) {
  sync_disabled_by_admin_ = false;
  DisableForUser();

  if (browser_sync::BackupRollbackController::IsBackupEnabled()) {
    need_backup_ = true;
    backup_finished_ = false;
  }
}

void ProfileSyncService::AddObserver(
    sync_driver::SyncServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void ProfileSyncService::RemoveObserver(
    sync_driver::SyncServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ProfileSyncService::AddProtocolEventObserver(
    browser_sync::ProtocolEventObserver* observer) {
  protocol_event_observers_.AddObserver(observer);
  if (HasSyncingBackend()) {
    backend_->RequestBufferedProtocolEventsAndEnableForwarding();
  }
}

void ProfileSyncService::RemoveProtocolEventObserver(
    browser_sync::ProtocolEventObserver* observer) {
  protocol_event_observers_.RemoveObserver(observer);
  if (HasSyncingBackend() &&
      !protocol_event_observers_.might_have_observers()) {
    backend_->DisableProtocolEventForwarding();
  }
}

void ProfileSyncService::AddTypeDebugInfoObserver(
    syncer::TypeDebugInfoObserver* type_debug_info_observer) {
  type_debug_info_observers_.AddObserver(type_debug_info_observer);
  if (type_debug_info_observers_.might_have_observers() &&
      backend_initialized_) {
    backend_->EnableDirectoryTypeDebugInfoForwarding();
  }
}

void ProfileSyncService::RemoveTypeDebugInfoObserver(
    syncer::TypeDebugInfoObserver* type_debug_info_observer) {
  type_debug_info_observers_.RemoveObserver(type_debug_info_observer);
  if (!type_debug_info_observers_.might_have_observers() &&
      backend_initialized_) {
    backend_->DisableDirectoryTypeDebugInfoForwarding();
  }
}

void ProfileSyncService::AddPreferenceProvider(
    SyncTypePreferenceProvider* provider) {
  DCHECK(!HasPreferenceProvider(provider))
      << "Providers may only be added once!";
  preference_providers_.insert(provider);
}

void ProfileSyncService::RemovePreferenceProvider(
    SyncTypePreferenceProvider* provider) {
  DCHECK(HasPreferenceProvider(provider))
      << "Only providers that have been added before can be removed!";
  preference_providers_.erase(provider);
}

bool ProfileSyncService::HasPreferenceProvider(
    SyncTypePreferenceProvider* provider) const {
  return preference_providers_.count(provider) > 0;
}

namespace {

class GetAllNodesRequestHelper
    : public base::RefCountedThreadSafe<GetAllNodesRequestHelper> {
 public:
  GetAllNodesRequestHelper(
      syncer::ModelTypeSet requested_types,
      const base::Callback<void(scoped_ptr<base::ListValue>)>& callback);

  void OnReceivedNodesForTypes(
      const std::vector<syncer::ModelType>& types,
      ScopedVector<base::ListValue> scoped_node_lists);

 private:
  friend class base::RefCountedThreadSafe<GetAllNodesRequestHelper>;
  virtual ~GetAllNodesRequestHelper();

  scoped_ptr<base::ListValue> result_accumulator_;

  syncer::ModelTypeSet awaiting_types_;
  base::Callback<void(scoped_ptr<base::ListValue>)> callback_;
};

GetAllNodesRequestHelper::GetAllNodesRequestHelper(
    syncer::ModelTypeSet requested_types,
    const base::Callback<void(scoped_ptr<base::ListValue>)>& callback)
    : result_accumulator_(new base::ListValue()),
      awaiting_types_(requested_types),
      callback_(callback) {}

GetAllNodesRequestHelper::~GetAllNodesRequestHelper() {
  if (!awaiting_types_.Empty()) {
    DLOG(WARNING)
        << "GetAllNodesRequest deleted before request was fulfilled.  "
        << "Missing types are: " << ModelTypeSetToString(awaiting_types_);
  }
}

// Called when the set of nodes for a type or set of types has been returned.
//
// The nodes for several types can be returned at the same time by specifying
// their types in the |types| array, and putting their results at the
// correspnding indices in the |scoped_node_lists|.
void GetAllNodesRequestHelper::OnReceivedNodesForTypes(
    const std::vector<syncer::ModelType>& types,
    ScopedVector<base::ListValue> scoped_node_lists) {
  DCHECK_EQ(types.size(), scoped_node_lists.size());

  // Take unsafe ownership of the node list.
  std::vector<base::ListValue*> node_lists;
  scoped_node_lists.release(&node_lists);

  for (size_t i = 0; i < node_lists.size() && i < types.size(); ++i) {
    const ModelType type = types[i];
    base::ListValue* node_list = node_lists[i];

    // Add these results to our list.
    scoped_ptr<base::DictionaryValue> type_dict(new base::DictionaryValue());
    type_dict->SetString("type", ModelTypeToString(type));
    type_dict->Set("nodes", node_list);
    result_accumulator_->Append(type_dict.release());

    // Remember that this part of the request is satisfied.
    awaiting_types_.Remove(type);
  }

  if (awaiting_types_.Empty()) {
    callback_.Run(result_accumulator_.Pass());
    callback_.Reset();
  }
}

}  // namespace

void ProfileSyncService::GetAllNodes(
    const base::Callback<void(scoped_ptr<base::ListValue>)>& callback) {
  ModelTypeSet directory_types = GetRegisteredDirectoryDataTypes();
  directory_types.PutAll(syncer::ControlTypes());
  scoped_refptr<GetAllNodesRequestHelper> helper =
      new GetAllNodesRequestHelper(directory_types, callback);

  if (!backend_initialized_) {
    // If there's no backend available to fulfill the request, handle it here.
    ScopedVector<base::ListValue> empty_results;
    std::vector<ModelType> type_vector;
    for (ModelTypeSet::Iterator it = directory_types.First();
         it.Good(); it.Inc()) {
      type_vector.push_back(it.Get());
      empty_results.push_back(new base::ListValue());
    }
    helper->OnReceivedNodesForTypes(type_vector, empty_results.Pass());
  } else {
    backend_->GetAllNodesForTypes(
        directory_types,
        base::Bind(&GetAllNodesRequestHelper::OnReceivedNodesForTypes, helper));
  }
}

bool ProfileSyncService::HasObserver(
    const sync_driver::SyncServiceObserver* observer) const {
  return observers_.HasObserver(observer);
}

base::WeakPtr<syncer::JsController> ProfileSyncService::GetJsController() {
  return sync_js_controller_.AsWeakPtr();
}

void ProfileSyncService::SyncEvent(SyncEventCodes code) {
  UMA_HISTOGRAM_ENUMERATION("Sync.EventCodes", code, MAX_SYNC_EVENT_CODE);
}

// static
bool ProfileSyncService::IsSyncEnabled() {
  // We have switches::kEnableSync just in case we need to change back to
  // sync-disabled-by-default on a platform.
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableSync);
}

bool ProfileSyncService::IsManaged() const {
  return sync_prefs_.IsManaged() || sync_disabled_by_admin_;
}

void ProfileSyncService::StopAndSuppress() {
  sync_prefs_.SetStartSuppressed(true);
  if (HasSyncingBackend()) {
    backend_->UnregisterInvalidationIds();
  }
  ShutdownImpl(syncer::STOP_SYNC);
}

bool ProfileSyncService::IsStartSuppressed() const {
  return sync_prefs_.IsStartSuppressed();
}

SigninManagerBase* ProfileSyncService::signin() const {
  if (!signin_)
    return NULL;
  return signin_->GetOriginal();
}

void ProfileSyncService::UnsuppressAndStart() {
  DCHECK(profile_);
  sync_prefs_.SetStartSuppressed(false);
  DCHECK(!signin_.get() || signin_->GetOriginal()->IsAuthenticated());
  startup_controller_->TryStart();
}

void ProfileSyncService::ReconfigureDatatypeManager() {
  // If we haven't initialized yet, don't configure the DTM as it could cause
  // association to start before a Directory has even been created.
  if (backend_initialized_) {
    DCHECK(backend_.get());
    ConfigureDataTypeManager();
  } else if (HasUnrecoverableError()) {
    // There is nothing more to configure. So inform the listeners,
    NotifyObservers();

    DVLOG(1) << "ConfigureDataTypeManager not invoked because of an "
             << "Unrecoverable error.";
  } else {
    DVLOG(0) << "ConfigureDataTypeManager not invoked because backend is not "
             << "initialized";
  }
}

syncer::ModelTypeSet ProfileSyncService::GetDataTypesFromPreferenceProviders()
    const {
  syncer::ModelTypeSet types;
  for (std::set<SyncTypePreferenceProvider*>::const_iterator it =
           preference_providers_.begin();
       it != preference_providers_.end();
       ++it) {
    types.PutAll((*it)->GetPreferredDataTypes());
  }
  return types;
}

const DataTypeStatusTable& ProfileSyncService::data_type_status_table()
    const {
  return data_type_status_table_;
}

void ProfileSyncService::OnInternalUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message,
    bool delete_sync_database,
    UnrecoverableErrorReason reason) {
  DCHECK(!HasUnrecoverableError());
  unrecoverable_error_reason_ = reason;
  OnUnrecoverableErrorImpl(from_here, message, delete_sync_database);
}

syncer::SyncManagerFactory::MANAGER_TYPE
ProfileSyncService::GetManagerType() const {
  switch (backend_mode_) {
    case SYNC:
      return syncer::SyncManagerFactory::NORMAL;
    case BACKUP:
      return syncer::SyncManagerFactory::BACKUP;
    case ROLLBACK:
      return syncer::SyncManagerFactory::ROLLBACK;
    case IDLE:
      NOTREACHED();
  }
  return syncer::SyncManagerFactory::NORMAL;
}

bool ProfileSyncService::IsRetryingAccessTokenFetchForTest() const {
  return request_access_token_retry_timer_.IsRunning();
}

std::string ProfileSyncService::GetAccessTokenForTest() const {
  return access_token_;
}

WeakHandle<syncer::JsEventHandler> ProfileSyncService::GetJsEventHandler() {
  return MakeWeakHandle(sync_js_controller_.AsWeakPtr());
}

syncer::SyncableService* ProfileSyncService::GetSessionsSyncableService() {
  return sessions_sync_manager_.get();
}

syncer::SyncableService* ProfileSyncService::GetDeviceInfoSyncableService() {
  return device_info_sync_service_.get();
}

ProfileSyncService::SyncTokenStatus::SyncTokenStatus()
    : connection_status(syncer::CONNECTION_NOT_ATTEMPTED),
      last_get_token_error(GoogleServiceAuthError::AuthErrorNone()) {}
ProfileSyncService::SyncTokenStatus::~SyncTokenStatus() {}

ProfileSyncService::SyncTokenStatus
ProfileSyncService::GetSyncTokenStatus() const {
  SyncTokenStatus status;
  status.connection_status_update_time = connection_status_update_time_;
  status.connection_status = connection_status_;
  status.token_request_time = token_request_time_;
  status.token_receive_time = token_receive_time_;
  status.last_get_token_error = last_get_token_error_;
  if (request_access_token_retry_timer_.IsRunning())
    status.next_token_request_time = next_token_request_time_;
  return status;
}

void ProfileSyncService::OverrideNetworkResourcesForTest(
    scoped_ptr<syncer::NetworkResources> network_resources) {
  network_resources_ = network_resources.Pass();
}

bool ProfileSyncService::HasSyncingBackend() const {
  return backend_mode_ != SYNC ? false : backend_ != NULL;
}

void ProfileSyncService::UpdateFirstSyncTimePref() {
  if (signin_->GetAccountIdToUse().empty()) {
    // Clear if user's not signed in and rollback is done.
    if (backend_mode_ != ROLLBACK)
      sync_prefs_.ClearFirstSyncTime();
  } else if (sync_prefs_.GetFirstSyncTime().is_null() &&
      backend_mode_ == SYNC) {
    // Set if not set before and it's syncing now.
    sync_prefs_.SetFirstSyncTime(base::Time::Now());
  }
}

void ProfileSyncService::ClearBrowsingDataSinceFirstSync() {
  base::Time first_sync_time = sync_prefs_.GetFirstSyncTime();
  if (first_sync_time.is_null())
    return;

  clear_browsing_data_.Run(browsing_data_remover_observer_,
                           profile_,
                           first_sync_time,
                           base::Time::Now());
}

void ProfileSyncService::SetBrowsingDataRemoverObserverForTesting(
    BrowsingDataRemover::Observer* observer) {
  browsing_data_remover_observer_ = observer;
}

void ProfileSyncService::SetClearingBrowseringDataForTesting(
    base::Callback<void(BrowsingDataRemover::Observer* observer,
                        Profile*,
                        base::Time,
                        base::Time)> c) {
  clear_browsing_data_ = c;
}

GURL ProfileSyncService::GetSyncServiceURL(
    const base::CommandLine& command_line) {
  // By default, dev, canary, and unbranded Chromium users will go to the
  // development servers. Development servers have more features than standard
  // sync servers. Users with officially-branded Chrome stable and beta builds
  // will go to the standard sync servers.
  GURL result(kDevServerUrl);

  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_STABLE ||
      channel == chrome::VersionInfo::CHANNEL_BETA) {
    result = GURL(kSyncServerUrl);
  }

  // Override the sync server URL from the command-line, if sync server
  // command-line argument exists.
  if (command_line.HasSwitch(switches::kSyncServiceURL)) {
    std::string value(command_line.GetSwitchValueASCII(
        switches::kSyncServiceURL));
    if (!value.empty()) {
      GURL custom_sync_url(value);
      if (custom_sync_url.is_valid()) {
        result = custom_sync_url;
      } else {
        LOG(WARNING) << "The following sync URL specified at the command-line "
                     << "is invalid: " << value;
      }
    }
  }
  return result;
}

void ProfileSyncService::CheckSyncBackupIfNeeded() {
  DCHECK_EQ(backend_mode_, SYNC);

#if defined(ENABLE_PRE_SYNC_BACKUP)
  const base::Time last_synced_time = sync_prefs_.GetLastSyncedTime();
  // Check backup once a day.
  if (!last_backup_time_ &&
      (last_synced_time.is_null() ||
          base::Time::Now() - last_synced_time >=
              base::TimeDelta::FromDays(1))) {
    // If sync thread is set, need to serialize check on sync thread after
    // closing backup DB.
    if (sync_thread_) {
      sync_thread_->message_loop_proxy()->PostTask(
          FROM_HERE,
          base::Bind(syncer::CheckSyncDbLastModifiedTime,
                     profile_->GetPath().Append(kSyncBackupDataFolderName),
                     base::ThreadTaskRunnerHandle::Get(),
                     base::Bind(&ProfileSyncService::CheckSyncBackupCallback,
                                weak_factory_.GetWeakPtr())));
    } else {
      content::BrowserThread::PostTask(
          content::BrowserThread::FILE, FROM_HERE,
          base::Bind(syncer::CheckSyncDbLastModifiedTime,
                     profile_->GetPath().Append(kSyncBackupDataFolderName),
                     base::ThreadTaskRunnerHandle::Get(),
                     base::Bind(&ProfileSyncService::CheckSyncBackupCallback,
                                weak_factory_.GetWeakPtr())));
    }
  }
#endif
}

void ProfileSyncService::CheckSyncBackupCallback(base::Time backup_time) {
  last_backup_time_.reset(new base::Time(backup_time));

  DCHECK(device_info_sync_service_);
  device_info_sync_service_->UpdateLocalDeviceBackupTime(*last_backup_time_);
}

void ProfileSyncService::TryStartSyncAfterBackup() {
  startup_controller_->Reset(GetRegisteredDataTypes());
  startup_controller_->TryStart();
}

void ProfileSyncService::CleanUpBackup() {
  sync_prefs_.ClearFirstSyncTime();
  profile_->GetIOTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(base::DeleteFile),
                 profile_->GetPath().Append(kSyncBackupDataFolderName),
                 true));
}

bool ProfileSyncService::NeedBackup() const {
  return need_backup_;
}

base::Time ProfileSyncService::GetDeviceBackupTimeForTesting() const {
  return device_info_sync_service_->GetLocalDeviceBackupTime();
}

void ProfileSyncService::FlushDirectory() const {
  // backend_initialized_ implies backend_ isn't NULL and the manager exists.
  // If sync is not initialized yet, we fail silently.
  if (backend_initialized_)
    backend_->FlushDirectory();
}

base::FilePath ProfileSyncService::GetDirectoryPathForTest() const {
  return directory_path_;
}

base::MessageLoop* ProfileSyncService::GetSyncLoopForTest() const {
  if (sync_thread_) {
    return sync_thread_->message_loop();
  } else if (backend_) {
    return backend_->GetSyncLoopForTesting();
  } else {
    return NULL;
  }
}

void ProfileSyncService::RemoveClientFromServer() const {
  if (!backend_initialized_) return;
  const std::string cache_guid = local_device_->GetLocalSyncCacheGUID();
  std::string birthday;
  syncer::UserShare* user_share = GetUserShare();
  if (user_share && user_share->directory.get()) {
    birthday = user_share->directory->store_birthday();
  }
  if (!access_token_.empty() && !cache_guid.empty() && !birthday.empty()) {
    sync_stopped_reporter_->ReportSyncStopped(
        access_token_, cache_guid, birthday);
  }
}

void ProfileSyncService::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    sync_prefs_.SetMemoryPressureWarningCount(
        sync_prefs_.GetMemoryPressureWarningCount() + 1);
  }
}

void ProfileSyncService::ReportPreviousSessionMemoryWarningCount() {
  int warning_received = sync_prefs_.GetMemoryPressureWarningCount();

  if (-1 != warning_received) {
    // -1 means it is new client.
    if (!sync_prefs_.DidSyncShutdownCleanly()) {
      UMA_HISTOGRAM_COUNTS("Sync.MemoryPressureWarningBeforeUncleanShutdown",
                           warning_received);
    } else {
      UMA_HISTOGRAM_COUNTS("Sync.MemoryPressureWarningBeforeCleanShutdown",
                           warning_received);
    }
  }
  sync_prefs_.SetMemoryPressureWarningCount(0);
  // Will set to true during a clean shutdown, so crash or something else will
  // remain this as false.
  sync_prefs_.SetCleanShutdown(false);
}
