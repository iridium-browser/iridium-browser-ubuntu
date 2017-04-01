// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_service_base.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/syslog_logging.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/device_info/local_device_info_provider.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine/engine_components_factory_impl.h"

namespace syncer {

namespace {

const base::FilePath::CharType kSyncDataFolderName[] =
    FILE_PATH_LITERAL("Sync Data");

#if defined(OS_WIN)
const base::FilePath::CharType kLoopbackServerBackendFilename[] =
    FILE_PATH_LITERAL("profile.pb");
#endif

EngineComponentsFactory::Switches EngineSwitchesFromCommandLine() {
  EngineComponentsFactory::Switches factory_switches = {
      EngineComponentsFactory::ENCRYPTION_KEYSTORE,
      EngineComponentsFactory::BACKOFF_NORMAL};

  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kSyncShortInitialRetryOverride)) {
    factory_switches.backoff_override =
        EngineComponentsFactory::BACKOFF_SHORT_INITIAL_RETRY_OVERRIDE;
  }
  if (cl->HasSwitch(switches::kSyncEnableGetUpdateAvoidance)) {
    factory_switches.pre_commit_updates_policy =
        EngineComponentsFactory::FORCE_ENABLE_PRE_COMMIT_UPDATE_AVOIDANCE;
  }
  if (cl->HasSwitch(switches::kSyncShortNudgeDelayForTest)) {
    factory_switches.nudge_delay =
        EngineComponentsFactory::NudgeDelay::SHORT_NUDGE_DELAY;
  }
  return factory_switches;
}

}  // namespace

SyncServiceBase::SyncServiceBase(std::unique_ptr<SyncClient> sync_client,
                                 std::unique_ptr<SigninManagerWrapper> signin,
                                 const version_info::Channel& channel,
                                 const base::FilePath& base_directory,
                                 const std::string& debug_identifier)
    : sync_client_(std::move(sync_client)),
      signin_(std::move(signin)),
      channel_(channel),
      base_directory_(base_directory),
      sync_data_folder_(
          base_directory_.Append(base::FilePath(kSyncDataFolderName))),
      debug_identifier_(debug_identifier),
      sync_prefs_(sync_client_->GetPrefService()) {}

SyncServiceBase::~SyncServiceBase() = default;

void SyncServiceBase::InitializeEngine() {
  DCHECK(engine_);

  if (!sync_thread_) {
    sync_thread_ = base::MakeUnique<base::Thread>("Chrome_SyncThread");
    base::Thread::Options options;
    options.timer_slack = base::TIMER_SLACK_MAXIMUM;
    CHECK(sync_thread_->StartWithOptions(options));
  }

  SyncEngine::InitParams params;
  params.sync_task_runner = sync_thread_->task_runner();
  params.host = this;
  params.registrar = base::MakeUnique<SyncBackendRegistrar>(
      debug_identifier_, base::Bind(&SyncClient::CreateModelWorkerForGroup,
                                    base::Unretained(sync_client_.get())));
  params.extensions_activity = sync_client_->GetExtensionsActivity();
  params.event_handler = GetJsEventHandler();
  params.service_url = sync_service_url();
  params.sync_user_agent = GetLocalDeviceInfoProvider()->GetSyncUserAgent();
  params.http_factory_getter = MakeHttpPostProviderFactoryGetter();
  params.credentials = GetCredentials();
  invalidation::InvalidationService* invalidator =
      sync_client_->GetInvalidationService();
  params.invalidator_client_id =
      invalidator ? invalidator->GetInvalidatorClientId() : "",
  params.sync_manager_factory = base::MakeUnique<SyncManagerFactory>();
  // The first time we start up the engine we want to ensure we have a clean
  // directory, so delete any old one that might be there.
  params.delete_sync_data_folder = !IsFirstSetupComplete();
  params.enable_local_sync_backend =
      GetLocalSyncConfig(&params.local_sync_backend_folder);
  params.restored_key_for_bootstrapping =
      sync_prefs_.GetEncryptionBootstrapToken();
  params.restored_keystore_key_for_bootstrapping =
      sync_prefs_.GetKeystoreEncryptionBootstrapToken();
  params.engine_components_factory =
      base::MakeUnique<EngineComponentsFactoryImpl>(
          EngineSwitchesFromCommandLine());
  params.unrecoverable_error_handler = GetUnrecoverableErrorHandler();
  params.report_unrecoverable_error_function =
      base::Bind(ReportUnrecoverableError, channel_);
  params.saved_nigori_state = MoveSavedNigoriState();
  sync_prefs_.GetInvalidationVersions(&params.invalidation_versions);

  engine_->Initialize(std::move(params));
}

bool SyncServiceBase::GetLocalSyncConfig(
    base::FilePath* local_sync_backend_folder) const {
  bool enable_local_sync_backend = false;
  *local_sync_backend_folder = sync_prefs_.GetLocalSyncBackendDir();
#if defined(OS_WIN)
  enable_local_sync_backend = sync_prefs_.IsLocalSyncEnabled();
  if (local_sync_backend_folder->empty()) {
    // TODO(pastarmovj): Add DIR_ROAMING_USER_DATA to PathService to simplify
    // this code and move the logic in its right place. See crbug/657810.
    if (!base::PathService::Get(base::DIR_APP_DATA,
                                local_sync_backend_folder)) {
      SYSLOG(WARNING) << "Local sync can not get the roaming profile folder.";
      return false;
    }
    *local_sync_backend_folder = local_sync_backend_folder->Append(
        FILE_PATH_LITERAL("Chrome/User Data"));
  }
  // This code as it is now will assume the same profile order is present on all
  // machines, which is not a given. It is to be defined if only the Default
  // profile should get this treatment or all profile as is the case now. The
  // solution for now will be to assume profiles are created in the same order
  // on all machines and in the future decide if only the Default one should be
  // considered roamed.
  // See http://crbug.com/674928.
  *local_sync_backend_folder =
      local_sync_backend_folder->Append(base_directory_.BaseName());
  *local_sync_backend_folder =
      local_sync_backend_folder->Append(kLoopbackServerBackendFilename);
#endif  // defined(OS_WIN)
  return enable_local_sync_backend;
}

}  // namespace syncer
