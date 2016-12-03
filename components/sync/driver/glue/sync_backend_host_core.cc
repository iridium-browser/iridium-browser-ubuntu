// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/glue/sync_backend_host_core.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/sync/core/http_post_provider_factory.h"
#include "components/sync/core/internal_components_factory.h"
#include "components/sync/core/sync_manager.h"
#include "components/sync/core/sync_manager_factory.h"
#include "components/sync/device_info/local_device_info_provider_impl.h"
#include "components/sync/driver/glue/sync_backend_registrar.h"
#include "components/sync/driver/invalidation_adapter.h"
#include "components/sync/engine/cycle/commit_counters.h"
#include "components/sync/engine/cycle/status_counters.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/cycle/update_counters.h"
#include "components/sync/engine/events/protocol_event.h"
#include "url/gurl.h"

// Helper macros to log with the syncer thread name; useful when there
// are multiple syncers involved.

#define SLOG(severity) LOG(severity) << name_ << ": "

#define SDVLOG(verbose_level) DVLOG(verbose_level) << name_ << ": "

static const int kSaveChangesIntervalSeconds = 10;

namespace syncer {
class InternalComponentsFactory;
}  // namespace syncer

namespace net {
class URLFetcher;
}

namespace {

void BindFetcherToDataTracker(net::URLFetcher* fetcher) {
  data_use_measurement::DataUseUserData::AttachToFetcher(
      fetcher, data_use_measurement::DataUseUserData::SYNC);
}

}  // namespace

namespace browser_sync {

DoInitializeOptions::DoInitializeOptions(
    base::MessageLoop* sync_loop,
    SyncBackendRegistrar* registrar,
    const std::vector<scoped_refptr<syncer::ModelSafeWorker>>& workers,
    const scoped_refptr<syncer::ExtensionsActivity>& extensions_activity,
    const syncer::WeakHandle<syncer::JsEventHandler>& event_handler,
    const GURL& service_url,
    const std::string& sync_user_agent,
    std::unique_ptr<syncer::HttpPostProviderFactory> http_bridge_factory,
    const syncer::SyncCredentials& credentials,
    const std::string& invalidator_client_id,
    std::unique_ptr<syncer::SyncManagerFactory> sync_manager_factory,
    bool delete_sync_data_folder,
    const std::string& restored_key_for_bootstrapping,
    const std::string& restored_keystore_key_for_bootstrapping,
    std::unique_ptr<syncer::InternalComponentsFactory>
        internal_components_factory,
    const syncer::WeakHandle<syncer::UnrecoverableErrorHandler>&
        unrecoverable_error_handler,
    const base::Closure& report_unrecoverable_error_function,
    std::unique_ptr<syncer::SyncEncryptionHandler::NigoriState>
        saved_nigori_state,
    const std::map<syncer::ModelType, int64_t>& invalidation_versions)
    : sync_loop(sync_loop),
      registrar(registrar),
      workers(workers),
      extensions_activity(extensions_activity),
      event_handler(event_handler),
      service_url(service_url),
      sync_user_agent(sync_user_agent),
      http_bridge_factory(std::move(http_bridge_factory)),
      credentials(credentials),
      invalidator_client_id(invalidator_client_id),
      sync_manager_factory(std::move(sync_manager_factory)),
      delete_sync_data_folder(delete_sync_data_folder),
      restored_key_for_bootstrapping(restored_key_for_bootstrapping),
      restored_keystore_key_for_bootstrapping(
          restored_keystore_key_for_bootstrapping),
      internal_components_factory(std::move(internal_components_factory)),
      unrecoverable_error_handler(unrecoverable_error_handler),
      report_unrecoverable_error_function(report_unrecoverable_error_function),
      saved_nigori_state(std::move(saved_nigori_state)),
      invalidation_versions(invalidation_versions) {}

DoInitializeOptions::~DoInitializeOptions() {}

DoConfigureSyncerTypes::DoConfigureSyncerTypes() {}

DoConfigureSyncerTypes::DoConfigureSyncerTypes(
    const DoConfigureSyncerTypes& other) = default;

DoConfigureSyncerTypes::~DoConfigureSyncerTypes() {}

SyncBackendHostCore::SyncBackendHostCore(
    const std::string& name,
    const base::FilePath& sync_data_folder_path,
    bool has_sync_setup_completed,
    const base::WeakPtr<SyncBackendHostImpl>& backend)
    : name_(name),
      sync_data_folder_path_(sync_data_folder_path),
      host_(backend),
      sync_loop_(NULL),
      registrar_(NULL),
      has_sync_setup_completed_(has_sync_setup_completed),
      forward_protocol_events_(false),
      forward_type_info_(false),
      weak_ptr_factory_(this) {
  DCHECK(backend.get());
}

SyncBackendHostCore::~SyncBackendHostCore() {
  DCHECK(!sync_manager_.get());
}

void SyncBackendHostCore::OnSyncCycleCompleted(
    const syncer::SyncCycleSnapshot& snapshot) {
  if (!sync_loop_)
    return;
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());

  host_.Call(FROM_HERE,
             &SyncBackendHostImpl::HandleSyncCycleCompletedOnFrontendLoop,
             snapshot);
}

void SyncBackendHostCore::DoRefreshTypes(syncer::ModelTypeSet types) {
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());
  sync_manager_->RefreshTypes(types);
}

void SyncBackendHostCore::OnInitializationComplete(
    const syncer::WeakHandle<syncer::JsBackend>& js_backend,
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
        debug_info_listener,
    bool success,
    const syncer::ModelTypeSet restored_types) {
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());

  if (!success) {
    DoDestroySyncManager(syncer::STOP_SYNC);
    host_.Call(FROM_HERE,
               &SyncBackendHostImpl::HandleInitializationFailureOnFrontendLoop);
    return;
  }

  // Register for encryption related changes now. We have to do this before
  // the initializing downloading control types or initializing the encryption
  // handler in order to receive notifications triggered during encryption
  // startup.
  sync_manager_->GetEncryptionHandler()->AddObserver(this);

  // Sync manager initialization is complete, so we can schedule recurring
  // SaveChanges.
  sync_loop_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&SyncBackendHostCore::StartSavingChanges,
                            weak_ptr_factory_.GetWeakPtr()));

  // Hang on to these for a while longer.  We're not ready to hand them back to
  // the UI thread yet.
  js_backend_ = js_backend;
  debug_info_listener_ = debug_info_listener;

  // Before proceeding any further, we need to download the control types and
  // purge any partial data (ie. data downloaded for a type that was on its way
  // to being initially synced, but didn't quite make it.).  The following
  // configure cycle will take care of this.  It depends on the registrar state
  // which we initialize below to ensure that we don't perform any downloads if
  // all control types have already completed their initial sync.
  registrar_->SetInitialTypes(restored_types);

  syncer::ConfigureReason reason =
      restored_types.Empty() ? syncer::CONFIGURE_REASON_NEW_CLIENT
                             : syncer::CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE;

  syncer::ModelTypeSet new_control_types = registrar_->ConfigureDataTypes(
      syncer::ControlTypes(), syncer::ModelTypeSet());
  syncer::ModelSafeRoutingInfo routing_info;
  registrar_->GetModelSafeRoutingInfo(&routing_info);
  SDVLOG(1) << "Control Types "
            << syncer::ModelTypeSetToString(new_control_types)
            << " added; calling ConfigureSyncer";

  syncer::ModelTypeSet types_to_purge = syncer::Difference(
      syncer::ModelTypeSet::All(), GetRoutingInfoTypes(routing_info));

  sync_manager_->ConfigureSyncer(
      reason, new_control_types, types_to_purge, syncer::ModelTypeSet(),
      syncer::ModelTypeSet(), routing_info,
      base::Bind(&SyncBackendHostCore::DoInitialProcessControlTypes,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Closure());
}

void SyncBackendHostCore::OnConnectionStatusChange(
    syncer::ConnectionStatus status) {
  if (!sync_loop_)
    return;
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());
  host_.Call(FROM_HERE,
             &SyncBackendHostImpl::HandleConnectionStatusChangeOnFrontendLoop,
             status);
}

void SyncBackendHostCore::OnPassphraseRequired(
    syncer::PassphraseRequiredReason reason,
    const sync_pb::EncryptedData& pending_keys) {
  if (!sync_loop_)
    return;
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());
  host_.Call(FROM_HERE, &SyncBackendHostImpl::NotifyPassphraseRequired, reason,
             pending_keys);
}

void SyncBackendHostCore::OnPassphraseAccepted() {
  if (!sync_loop_)
    return;
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());
  host_.Call(FROM_HERE, &SyncBackendHostImpl::NotifyPassphraseAccepted);
}

void SyncBackendHostCore::OnBootstrapTokenUpdated(
    const std::string& bootstrap_token,
    syncer::BootstrapTokenType type) {
  if (!sync_loop_)
    return;
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());
  host_.Call(FROM_HERE, &SyncBackendHostImpl::PersistEncryptionBootstrapToken,
             bootstrap_token, type);
}

void SyncBackendHostCore::OnEncryptedTypesChanged(
    syncer::ModelTypeSet encrypted_types,
    bool encrypt_everything) {
  if (!sync_loop_)
    return;
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());
  // NOTE: We're in a transaction.
  host_.Call(FROM_HERE, &SyncBackendHostImpl::NotifyEncryptedTypesChanged,
             encrypted_types, encrypt_everything);
}

void SyncBackendHostCore::OnEncryptionComplete() {
  if (!sync_loop_)
    return;
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());
  // NOTE: We're in a transaction.
  host_.Call(FROM_HERE, &SyncBackendHostImpl::NotifyEncryptionComplete);
}

void SyncBackendHostCore::OnCryptographerStateChanged(
    syncer::Cryptographer* cryptographer) {
  // Do nothing.
}

void SyncBackendHostCore::OnPassphraseTypeChanged(syncer::PassphraseType type,
                                                  base::Time passphrase_time) {
  host_.Call(FROM_HERE,
             &SyncBackendHostImpl::HandlePassphraseTypeChangedOnFrontendLoop,
             type, passphrase_time);
}

void SyncBackendHostCore::OnLocalSetPassphraseEncryption(
    const syncer::SyncEncryptionHandler::NigoriState& nigori_state) {
  host_.Call(
      FROM_HERE,
      &SyncBackendHostImpl::HandleLocalSetPassphraseEncryptionOnFrontendLoop,
      nigori_state);
}

void SyncBackendHostCore::OnCommitCountersUpdated(
    syncer::ModelType type,
    const syncer::CommitCounters& counters) {
  host_.Call(
      FROM_HERE,
      &SyncBackendHostImpl::HandleDirectoryCommitCountersUpdatedOnFrontendLoop,
      type, counters);
}

void SyncBackendHostCore::OnUpdateCountersUpdated(
    syncer::ModelType type,
    const syncer::UpdateCounters& counters) {
  host_.Call(
      FROM_HERE,
      &SyncBackendHostImpl::HandleDirectoryUpdateCountersUpdatedOnFrontendLoop,
      type, counters);
}

void SyncBackendHostCore::OnStatusCountersUpdated(
    syncer::ModelType type,
    const syncer::StatusCounters& counters) {
  host_.Call(
      FROM_HERE,
      &SyncBackendHostImpl::HandleDirectoryStatusCountersUpdatedOnFrontendLoop,
      type, counters);
}

void SyncBackendHostCore::OnActionableError(
    const syncer::SyncProtocolError& sync_error) {
  if (!sync_loop_)
    return;
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());
  host_.Call(FROM_HERE,
             &SyncBackendHostImpl::HandleActionableErrorEventOnFrontendLoop,
             sync_error);
}

void SyncBackendHostCore::OnMigrationRequested(syncer::ModelTypeSet types) {
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());
  host_.Call(FROM_HERE,
             &SyncBackendHostImpl::HandleMigrationRequestedOnFrontendLoop,
             types);
}

void SyncBackendHostCore::OnProtocolEvent(const syncer::ProtocolEvent& event) {
  // TODO(rlarocque): Find a way to pass event_clone as a scoped_ptr.
  if (forward_protocol_events_) {
    std::unique_ptr<syncer::ProtocolEvent> event_clone(event.Clone());
    host_.Call(FROM_HERE,
               &SyncBackendHostImpl::HandleProtocolEventOnFrontendLoop,
               event_clone.release());
  }
}

void SyncBackendHostCore::DoOnInvalidatorStateChange(
    syncer::InvalidatorState state) {
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());
  sync_manager_->SetInvalidatorEnabled(state == syncer::INVALIDATIONS_ENABLED);
}

void SyncBackendHostCore::DoOnIncomingInvalidation(
    const syncer::ObjectIdInvalidationMap& invalidation_map) {
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());

  syncer::ObjectIdSet ids = invalidation_map.GetObjectIds();
  for (const invalidation::ObjectId& object_id : ids) {
    syncer::ModelType type;
    if (!NotificationTypeToRealModelType(object_id.name(), &type)) {
      DLOG(WARNING) << "Notification has invalid id: "
                    << syncer::ObjectIdToString(object_id);
    } else {
      syncer::SingleObjectInvalidationSet invalidation_set =
          invalidation_map.ForObject(object_id);
      for (syncer::Invalidation invalidation : invalidation_set) {
        auto last_invalidation = last_invalidation_versions_.find(type);
        if (!invalidation.is_unknown_version() &&
            last_invalidation != last_invalidation_versions_.end() &&
            invalidation.version() <= last_invalidation->second) {
          DVLOG(1) << "Ignoring redundant invalidation for "
                   << syncer::ModelTypeToString(type) << " with version "
                   << invalidation.version() << ", last seen version was "
                   << last_invalidation->second;
          continue;
        }
        std::unique_ptr<syncer::InvalidationInterface> inv_adapter(
            new InvalidationAdapter(invalidation));
        sync_manager_->OnIncomingInvalidation(type, std::move(inv_adapter));
        if (!invalidation.is_unknown_version())
          last_invalidation_versions_[type] = invalidation.version();
      }
    }
  }

  host_.Call(FROM_HERE, &SyncBackendHostImpl::UpdateInvalidationVersions,
             last_invalidation_versions_);
}

void SyncBackendHostCore::DoInitialize(
    std::unique_ptr<DoInitializeOptions> options) {
  DCHECK(!sync_loop_);
  sync_loop_ = options->sync_loop;
  DCHECK(sync_loop_);

  // Finish initializing the HttpBridgeFactory.  We do this here because
  // building the user agent may block on some platforms.
  options->http_bridge_factory->Init(options->sync_user_agent,
                                     base::Bind(&BindFetcherToDataTracker));

  // Blow away the partial or corrupt sync data folder before doing any more
  // initialization, if necessary.
  if (options->delete_sync_data_folder) {
    DeleteSyncDataFolder();
  }

  // Make sure that the directory exists before initializing the backend.
  // If it already exists, this will do no harm.
  if (!base::CreateDirectory(sync_data_folder_path_)) {
    DLOG(FATAL) << "Sync Data directory creation failed.";
  }

  // Load the previously persisted set of invalidation versions into memory.
  last_invalidation_versions_ = options->invalidation_versions;

  DCHECK(!registrar_);
  registrar_ = options->registrar;
  DCHECK(registrar_);

  sync_manager_ = options->sync_manager_factory->CreateSyncManager(name_);
  sync_manager_->AddObserver(this);

  syncer::SyncManager::InitArgs args;
  args.database_location = sync_data_folder_path_;
  args.event_handler = options->event_handler;
  args.service_url = options->service_url;
  args.post_factory = std::move(options->http_bridge_factory);
  args.workers = options->workers;
  args.extensions_activity = options->extensions_activity.get();
  args.change_delegate = options->registrar;  // as SyncManager::ChangeDelegate
  args.credentials = options->credentials;
  args.invalidator_client_id = options->invalidator_client_id;
  args.restored_key_for_bootstrapping = options->restored_key_for_bootstrapping;
  args.restored_keystore_key_for_bootstrapping =
      options->restored_keystore_key_for_bootstrapping;
  args.internal_components_factory =
      std::move(options->internal_components_factory);
  args.encryptor = &encryptor_;
  args.unrecoverable_error_handler = options->unrecoverable_error_handler;
  args.report_unrecoverable_error_function =
      options->report_unrecoverable_error_function;
  args.cancelation_signal = &stop_syncing_signal_;
  args.saved_nigori_state = std::move(options->saved_nigori_state);
  sync_manager_->Init(&args);
}

void SyncBackendHostCore::DoUpdateCredentials(
    const syncer::SyncCredentials& credentials) {
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());
  // UpdateCredentials can be called during backend initialization, possibly
  // when backend initialization has failed but hasn't notified the UI thread
  // yet. In that case, the sync manager may have been destroyed on the sync
  // thread before this task was executed, so we do nothing.
  if (sync_manager_) {
    sync_manager_->UpdateCredentials(credentials);
  }
}

void SyncBackendHostCore::DoStartSyncing(
    const syncer::ModelSafeRoutingInfo& routing_info,
    base::Time last_poll_time) {
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());
  sync_manager_->StartSyncingNormally(routing_info, last_poll_time);
}

void SyncBackendHostCore::DoSetEncryptionPassphrase(
    const std::string& passphrase,
    bool is_explicit) {
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());
  sync_manager_->GetEncryptionHandler()->SetEncryptionPassphrase(passphrase,
                                                                 is_explicit);
}

void SyncBackendHostCore::DoInitialProcessControlTypes() {
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());

  DVLOG(1) << "Initilalizing Control Types";

  // Initialize encryption.
  sync_manager_->GetEncryptionHandler()->Init();

  // Note: experiments are currently handled via SBH::AddExperimentalTypes,
  // which is called at the end of every sync cycle.
  // TODO(zea): eventually add an experiment handler and initialize it here.

  if (!sync_manager_->GetUserShare()) {  // NULL in some tests.
    DVLOG(1) << "Skipping initialization of DeviceInfo";
    host_.Call(FROM_HERE,
               &SyncBackendHostImpl::HandleInitializationFailureOnFrontendLoop);
    return;
  }

  if (!sync_manager_->InitialSyncEndedTypes().HasAll(syncer::ControlTypes())) {
    LOG(ERROR) << "Failed to download control types";
    host_.Call(FROM_HERE,
               &SyncBackendHostImpl::HandleInitializationFailureOnFrontendLoop);
    return;
  }

  host_.Call(FROM_HERE,
             &SyncBackendHostImpl::HandleInitializationSuccessOnFrontendLoop,
             js_backend_, debug_info_listener_,
             base::Passed(sync_manager_->GetModelTypeConnectorProxy()),
             sync_manager_->cache_guid());

  js_backend_.Reset();
  debug_info_listener_.Reset();
}

void SyncBackendHostCore::DoSetDecryptionPassphrase(
    const std::string& passphrase) {
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());
  sync_manager_->GetEncryptionHandler()->SetDecryptionPassphrase(passphrase);
}

void SyncBackendHostCore::DoEnableEncryptEverything() {
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());
  sync_manager_->GetEncryptionHandler()->EnableEncryptEverything();
}

void SyncBackendHostCore::ShutdownOnUIThread() {
  // This will cut short any blocking network tasks, cut short any in-progress
  // sync cycles, and prevent the creation of new blocking network tasks and new
  // sync cycles.  If there was an in-progress network request, it would have
  // had a reference to the RequestContextGetter.  This reference will be
  // dropped by the time this function returns.
  //
  // It is safe to call this even if Sync's backend classes have not been
  // initialized yet.  Those classes will receive the message when the sync
  // thread finally getes around to constructing them.
  stop_syncing_signal_.Signal();

  // This will drop the HttpBridgeFactory's reference to the
  // RequestContextGetter.  Once this has been called, the HttpBridgeFactory can
  // no longer be used to create new HttpBridge instances.  We can get away with
  // this because the stop_syncing_signal_ has already been signalled, which
  // guarantees that the ServerConnectionManager will no longer attempt to
  // create new connections.
  release_request_context_signal_.Signal();
}

void SyncBackendHostCore::DoShutdown(syncer::ShutdownReason reason) {
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());

  DoDestroySyncManager(reason);

  registrar_ = NULL;

  if (reason == syncer::DISABLE_SYNC)
    DeleteSyncDataFolder();

  host_.Reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void SyncBackendHostCore::DoDestroySyncManager(syncer::ShutdownReason reason) {
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());
  if (sync_manager_) {
    DisableDirectoryTypeDebugInfoForwarding();
    save_changes_timer_.reset();
    sync_manager_->RemoveObserver(this);
    sync_manager_->ShutdownOnSyncThread(reason);
    sync_manager_.reset();
  }
}

void SyncBackendHostCore::DoConfigureSyncer(
    syncer::ConfigureReason reason,
    const DoConfigureSyncerTypes& config_types,
    const syncer::ModelSafeRoutingInfo routing_info,
    const base::Callback<void(syncer::ModelTypeSet, syncer::ModelTypeSet)>&
        ready_task,
    const base::Closure& retry_callback) {
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());
  DCHECK(!ready_task.is_null());
  DCHECK(!retry_callback.is_null());
  base::Closure chained_ready_task(base::Bind(
      &SyncBackendHostCore::DoFinishConfigureDataTypes,
      weak_ptr_factory_.GetWeakPtr(), config_types.to_download, ready_task));
  base::Closure chained_retry_task(
      base::Bind(&SyncBackendHostCore::DoRetryConfiguration,
                 weak_ptr_factory_.GetWeakPtr(), retry_callback));
  sync_manager_->ConfigureSyncer(reason, config_types.to_download,
                                 config_types.to_purge, config_types.to_journal,
                                 config_types.to_unapply, routing_info,
                                 chained_ready_task, chained_retry_task);
}

void SyncBackendHostCore::DoFinishConfigureDataTypes(
    syncer::ModelTypeSet types_to_config,
    const base::Callback<void(syncer::ModelTypeSet, syncer::ModelTypeSet)>&
        ready_task) {
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());

  // Update the enabled types for the bridge and sync manager.
  syncer::ModelSafeRoutingInfo routing_info;
  registrar_->GetModelSafeRoutingInfo(&routing_info);
  syncer::ModelTypeSet enabled_types = GetRoutingInfoTypes(routing_info);
  enabled_types.RemoveAll(syncer::ProxyTypes());

  const syncer::ModelTypeSet failed_configuration_types =
      Difference(types_to_config, sync_manager_->InitialSyncEndedTypes());
  const syncer::ModelTypeSet succeeded_configuration_types =
      Difference(types_to_config, failed_configuration_types);
  host_.Call(FROM_HERE,
             &SyncBackendHostImpl::FinishConfigureDataTypesOnFrontendLoop,
             enabled_types, succeeded_configuration_types,
             failed_configuration_types, ready_task);
}

void SyncBackendHostCore::DoRetryConfiguration(
    const base::Closure& retry_callback) {
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());
  host_.Call(FROM_HERE, &SyncBackendHostImpl::RetryConfigurationOnFrontendLoop,
             retry_callback);
}

void SyncBackendHostCore::SendBufferedProtocolEventsAndEnableForwarding() {
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());
  forward_protocol_events_ = true;

  if (sync_manager_) {
    // Grab our own copy of the buffered events.
    // The buffer is not modified by this operation.
    std::vector<syncer::ProtocolEvent*> buffered_events;
    sync_manager_->GetBufferedProtocolEvents().release(&buffered_events);

    // Send them all over the fence to the host.
    for (std::vector<syncer::ProtocolEvent*>::iterator it =
             buffered_events.begin();
         it != buffered_events.end(); ++it) {
      // TODO(rlarocque): Make it explicit that host_ takes ownership.
      host_.Call(FROM_HERE,
                 &SyncBackendHostImpl::HandleProtocolEventOnFrontendLoop, *it);
    }
  }
}

void SyncBackendHostCore::DisableProtocolEventForwarding() {
  forward_protocol_events_ = false;
}

void SyncBackendHostCore::EnableDirectoryTypeDebugInfoForwarding() {
  DCHECK(sync_manager_);

  forward_type_info_ = true;

  if (!sync_manager_->HasDirectoryTypeDebugInfoObserver(this))
    sync_manager_->RegisterDirectoryTypeDebugInfoObserver(this);
  sync_manager_->RequestEmitDebugInfo();
}

void SyncBackendHostCore::DisableDirectoryTypeDebugInfoForwarding() {
  DCHECK(sync_manager_);

  if (!forward_type_info_)
    return;

  forward_type_info_ = false;

  if (sync_manager_->HasDirectoryTypeDebugInfoObserver(this))
    sync_manager_->UnregisterDirectoryTypeDebugInfoObserver(this);
}

void SyncBackendHostCore::DeleteSyncDataFolder() {
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());
  if (base::DirectoryExists(sync_data_folder_path_)) {
    if (!base::DeleteFile(sync_data_folder_path_, true))
      SLOG(DFATAL) << "Could not delete the Sync Data folder.";
  }
}

void SyncBackendHostCore::GetAllNodesForTypes(
    syncer::ModelTypeSet types,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::Callback<void(const std::vector<syncer::ModelType>& type,
                        ScopedVector<base::ListValue>)> callback) {
  std::vector<syncer::ModelType> types_vector;
  ScopedVector<base::ListValue> node_lists;

  syncer::ModelSafeRoutingInfo routes;
  registrar_->GetModelSafeRoutingInfo(&routes);
  syncer::ModelTypeSet enabled_types = GetRoutingInfoTypes(routes);

  for (syncer::ModelTypeSet::Iterator it = types.First(); it.Good(); it.Inc()) {
    types_vector.push_back(it.Get());
    if (!enabled_types.Has(it.Get())) {
      node_lists.push_back(new base::ListValue());
    } else {
      node_lists.push_back(
          sync_manager_->GetAllNodesForType(it.Get()).release());
    }
  }

  task_runner->PostTask(
      FROM_HERE, base::Bind(callback, types_vector, base::Passed(&node_lists)));
}

void SyncBackendHostCore::StartSavingChanges() {
  // We may already be shut down.
  if (!sync_loop_)
    return;
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());
  DCHECK(!save_changes_timer_.get());
  save_changes_timer_.reset(new base::RepeatingTimer());
  save_changes_timer_->Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kSaveChangesIntervalSeconds),
      this, &SyncBackendHostCore::SaveChanges);
}

void SyncBackendHostCore::SaveChanges() {
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());
  sync_manager_->SaveChanges();
}

void SyncBackendHostCore::DoClearServerData(
    const syncer::SyncManager::ClearServerDataCallback& frontend_callback) {
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());
  const syncer::SyncManager::ClearServerDataCallback callback =
      base::Bind(&SyncBackendHostCore::ClearServerDataDone,
                 weak_ptr_factory_.GetWeakPtr(), frontend_callback);
  sync_manager_->ClearServerData(callback);
}

void SyncBackendHostCore::DoOnCookieJarChanged(bool account_mismatch,
                                               bool empty_jar) {
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());
  sync_manager_->OnCookieJarChanged(account_mismatch, empty_jar);
}

void SyncBackendHostCore::ClearServerDataDone(
    const base::Closure& frontend_callback) {
  DCHECK(sync_loop_->task_runner()->BelongsToCurrentThread());
  host_.Call(FROM_HERE, &SyncBackendHostImpl::ClearServerDataDoneOnFrontendLoop,
             frontend_callback);
}

}  // namespace browser_sync
