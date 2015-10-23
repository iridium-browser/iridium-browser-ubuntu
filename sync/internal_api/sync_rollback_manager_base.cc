// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/sync_rollback_manager_base.h"

#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/syncable/directory_backing_store.h"
#include "sync/syncable/mutable_entry.h"

namespace {

// Permanent bookmark folders as defined in bookmark_model_associator.cc.
// No mobile bookmarks because they only exists with sync enabled.
const char kBookmarkBarTag[] = "bookmark_bar";
const char kOtherBookmarksTag[] = "other_bookmarks";

class DummyEntryptionHandler : public syncer::SyncEncryptionHandler {
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
  void Init() override {}
  void SetEncryptionPassphrase(const std::string& passphrase,
                               bool is_explicit) override {}
  void SetDecryptionPassphrase(const std::string& passphrase) override {}
  void EnableEncryptEverything() override {}
  bool EncryptEverythingEnabled() const override { return false; }
  syncer::PassphraseType GetPassphraseType() const override {
    return syncer::KEYSTORE_PASSPHRASE;
  }
};

}  // anonymous namespace

namespace syncer {

SyncRollbackManagerBase::SyncRollbackManagerBase()
    : dummy_handler_(new DummyEntryptionHandler),
      initialized_(false),
      weak_ptr_factory_(this) {
}

SyncRollbackManagerBase::~SyncRollbackManagerBase() {
}

bool SyncRollbackManagerBase::InitInternal(
    const base::FilePath& database_location,
    InternalComponentsFactory* internal_components_factory,
    InternalComponentsFactory::StorageOption storage,
    const WeakHandle<UnrecoverableErrorHandler>& unrecoverable_error_handler,
    const base::Closure& report_unrecoverable_error_function) {
  unrecoverable_error_handler_ = unrecoverable_error_handler;
  report_unrecoverable_error_function_ = report_unrecoverable_error_function;

  if (!InitBackupDB(database_location, internal_components_factory, storage)) {
    NotifyInitializationFailure();
    return false;
  }

  initialized_ = true;
  NotifyInitializationSuccess();
  return true;
}

ModelTypeSet SyncRollbackManagerBase::InitialSyncEndedTypes() {
  return share_.directory->InitialSyncEndedTypes();
}

ModelTypeSet SyncRollbackManagerBase::GetTypesWithEmptyProgressMarkerToken(
      ModelTypeSet types) {
  ModelTypeSet inited_types = share_.directory->InitialSyncEndedTypes();
  types.RemoveAll(inited_types);
  return types;
}

bool SyncRollbackManagerBase::PurgePartiallySyncedTypes() {
  NOTREACHED();
  return true;
}

void SyncRollbackManagerBase::UpdateCredentials(
    const SyncCredentials& credentials) {
}

void SyncRollbackManagerBase::StartSyncingNormally(
    const ModelSafeRoutingInfo& routing_info, base::Time last_poll_time){
}

void SyncRollbackManagerBase::ConfigureSyncer(
      ConfigureReason reason,
      ModelTypeSet to_download,
      ModelTypeSet to_purge,
      ModelTypeSet to_journal,
      ModelTypeSet to_unapply,
      const ModelSafeRoutingInfo& new_routing_info,
      const base::Closure& ready_task,
      const base::Closure& retry_task) {
  for (ModelTypeSet::Iterator type = to_download.First();
      type.Good(); type.Inc()) {
    if (InitTypeRootNode(type.Get())) {
      if (type.Get() == BOOKMARKS) {
        InitBookmarkFolder(kBookmarkBarTag);
        InitBookmarkFolder(kOtherBookmarksTag);
      }
    }
  }

  ready_task.Run();
}

void SyncRollbackManagerBase::SetInvalidatorEnabled(bool invalidator_enabled) {
}

void SyncRollbackManagerBase::OnIncomingInvalidation(
    syncer::ModelType type,
    scoped_ptr<InvalidationInterface> invalidation) {
  NOTREACHED();
}

void SyncRollbackManagerBase::AddObserver(SyncManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void SyncRollbackManagerBase::RemoveObserver(SyncManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

SyncStatus SyncRollbackManagerBase::GetDetailedStatus() const {
  return SyncStatus();
}

void SyncRollbackManagerBase::SaveChanges() {
}

void SyncRollbackManagerBase::ShutdownOnSyncThread(ShutdownReason reason) {
  if (initialized_) {
    share_.directory.reset();
    initialized_ = false;
  }
}

UserShare* SyncRollbackManagerBase::GetUserShare() {
  return &share_;
}

const std::string SyncRollbackManagerBase::cache_guid() {
  return share_.directory->cache_guid();
}

bool SyncRollbackManagerBase::ReceivedExperiment(Experiments* experiments) {
  return false;
}

bool SyncRollbackManagerBase::HasUnsyncedItems() {
  ReadTransaction trans(FROM_HERE, &share_);
  syncable::Directory::Metahandles unsynced;
  share_.directory->GetUnsyncedMetaHandles(trans.GetWrappedTrans(), &unsynced);
  return !unsynced.empty();
}

SyncEncryptionHandler* SyncRollbackManagerBase::GetEncryptionHandler() {
  return dummy_handler_.get();
}

void SyncRollbackManagerBase::RefreshTypes(ModelTypeSet types) {

}

void SyncRollbackManagerBase::HandleTransactionCompleteChangeEvent(
    ModelTypeSet models_with_changes) {
}

ModelTypeSet SyncRollbackManagerBase::HandleTransactionEndingChangeEvent(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans) {
  return ModelTypeSet();
}

void SyncRollbackManagerBase::HandleCalculateChangesChangeEventFromSyncApi(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans,
      std::vector<int64>* entries_changed) {
}

void SyncRollbackManagerBase::HandleCalculateChangesChangeEventFromSyncer(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans,
      std::vector<int64>* entries_changed) {
}

void SyncRollbackManagerBase::OnTransactionWrite(
    const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
    ModelTypeSet models_with_changes) {
}

void SyncRollbackManagerBase::NotifyInitializationSuccess() {
  FOR_EACH_OBSERVER(
      SyncManager::Observer, observers_,
      OnInitializationComplete(
          MakeWeakHandle(base::WeakPtr<JsBackend>()),
          MakeWeakHandle(base::WeakPtr<DataTypeDebugInfoListener>()),
          true, InitialSyncEndedTypes()));
}

void SyncRollbackManagerBase::NotifyInitializationFailure() {
  FOR_EACH_OBSERVER(
      SyncManager::Observer, observers_,
      OnInitializationComplete(
          MakeWeakHandle(base::WeakPtr<JsBackend>()),
          MakeWeakHandle(base::WeakPtr<DataTypeDebugInfoListener>()),
          false, ModelTypeSet()));
}

syncer_v2::SyncContextProxy* SyncRollbackManagerBase::GetSyncContextProxy() {
  return NULL;
}

ScopedVector<syncer::ProtocolEvent>
SyncRollbackManagerBase::GetBufferedProtocolEvents() {
  return ScopedVector<syncer::ProtocolEvent>().Pass();
}

scoped_ptr<base::ListValue> SyncRollbackManagerBase::GetAllNodesForType(
    syncer::ModelType type) {
  ReadTransaction trans(FROM_HERE, GetUserShare());
  scoped_ptr<base::ListValue> nodes(
      trans.GetDirectory()->GetNodeDetailsForType(trans.GetWrappedTrans(),
                                                  type));
  return nodes.Pass();
}

bool SyncRollbackManagerBase::InitBackupDB(
    const base::FilePath& sync_folder,
    InternalComponentsFactory* internal_components_factory,
    InternalComponentsFactory::StorageOption storage) {
  base::FilePath backup_db_path = sync_folder.Append(
      syncable::Directory::kSyncDatabaseFilename);
  scoped_ptr<syncable::DirectoryBackingStore> backing_store =
      internal_components_factory->BuildDirectoryBackingStore(
          storage, "backup", backup_db_path).Pass();

  DCHECK(backing_store.get());
  share_.directory.reset(
      new syncable::Directory(
          backing_store.release(),
          unrecoverable_error_handler_,
          report_unrecoverable_error_function_,
          NULL,
          NULL));
  return syncable::OPENED ==
      share_.directory->Open(
          "backup", this,
          MakeWeakHandle(weak_ptr_factory_.GetWeakPtr()));
}

bool SyncRollbackManagerBase::InitTypeRootNode(ModelType type) {
  WriteTransaction trans(FROM_HERE, &share_);
  ReadNode root(&trans);
  if (BaseNode::INIT_OK == root.InitTypeRoot(type))
    return true;

  syncable::MutableEntry entry(trans.GetWrappedWriteTrans(),
                               syncable::CREATE_NEW_UPDATE_ITEM,
                               syncable::Id::CreateFromServerId(
                                   ModelTypeToString(type)));
  if (!entry.good())
    return false;

  entry.PutParentId(syncable::Id::GetRoot());
  entry.PutBaseVersion(1);
  entry.PutUniqueServerTag(ModelTypeToRootTag(type));
  entry.PutNonUniqueName(ModelTypeToString(type));
  entry.PutIsDel(false);
  entry.PutIsDir(true);

  sync_pb::EntitySpecifics specifics;
  AddDefaultFieldValue(type, &specifics);
  entry.PutSpecifics(specifics);

  return true;
}

void SyncRollbackManagerBase::InitBookmarkFolder(const std::string& folder) {
  WriteTransaction trans(FROM_HERE, &share_);
  syncable::Entry bookmark_root(trans.GetWrappedTrans(),
                                syncable::GET_TYPE_ROOT,
                                BOOKMARKS);
  if (!bookmark_root.good())
    return;

  syncable::MutableEntry entry(trans.GetWrappedWriteTrans(),
                               syncable::CREATE_NEW_UPDATE_ITEM,
                               syncable::Id::CreateFromServerId(folder));
  if (!entry.good())
    return;

  entry.PutParentId(bookmark_root.GetId());
  entry.PutBaseVersion(1);
  entry.PutUniqueServerTag(folder);
  entry.PutNonUniqueName(folder);
  entry.PutIsDel(false);
  entry.PutIsDir(true);

  sync_pb::EntitySpecifics specifics;
  AddDefaultFieldValue(BOOKMARKS, &specifics);
  entry.PutSpecifics(specifics);
}

base::ObserverList<SyncManager::Observer>*
SyncRollbackManagerBase::GetObservers() {
  return &observers_;
}

void SyncRollbackManagerBase::RegisterDirectoryTypeDebugInfoObserver(
    syncer::TypeDebugInfoObserver* observer) {}

void SyncRollbackManagerBase::UnregisterDirectoryTypeDebugInfoObserver(
    syncer::TypeDebugInfoObserver* observer) {}

bool SyncRollbackManagerBase::HasDirectoryTypeDebugInfoObserver(
    syncer::TypeDebugInfoObserver* observer) { return false; }

void SyncRollbackManagerBase::RequestEmitDebugInfo() {}

void SyncRollbackManagerBase::ClearServerData(
    const ClearServerDataCallback& callback) {}

}  // namespace syncer
