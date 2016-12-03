// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_MODEL_TYPE_REGISTRY_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_MODEL_TYPE_REGISTRY_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/base/model_type.h"
#include "components/sync/core/model_type_connector.h"
#include "components/sync/core/non_blocking_sync_common.h"
#include "components/sync/core/sync_encryption_handler.h"
#include "components/sync/engine/cycle/type_debug_info_observer.h"
#include "components/sync/engine/model_safe_worker.h"
#include "components/sync/engine_impl/nudge_handler.h"

namespace syncer_v2 {
struct DataTypeState;
class ModelTypeProcessor;
class ModelTypeWorker;
}

namespace syncer {

namespace syncable {
class Directory;
}  // namespace syncable

class CommitContributor;
class DirectoryCommitContributor;
class DirectoryUpdateHandler;
class DirectoryTypeDebugInfoEmitter;
class UpdateHandler;

typedef std::map<ModelType, UpdateHandler*> UpdateHandlerMap;
typedef std::map<ModelType, CommitContributor*> CommitContributorMap;
typedef std::map<ModelType, DirectoryTypeDebugInfoEmitter*>
    DirectoryTypeDebugInfoEmitterMap;

// Keeps track of the sets of active update handlers and commit contributors.
class ModelTypeRegistry : public syncer_v2::ModelTypeConnector,
                          public SyncEncryptionHandler::Observer {
 public:
  // Constructs a ModelTypeRegistry that supports directory types.
  ModelTypeRegistry(const std::vector<scoped_refptr<ModelSafeWorker>>& workers,
                    syncable::Directory* directory,
                    NudgeHandler* nudge_handler);
  ~ModelTypeRegistry() override;

  // Sets the set of enabled types.
  void SetEnabledDirectoryTypes(const ModelSafeRoutingInfo& routing_info);

  // Enables an off-thread type for syncing.  Connects the given proxy
  // and its task_runner to the newly created worker.
  //
  // Expects that the proxy's ModelType is not currently enabled.
  void ConnectType(syncer::ModelType type,
                   std::unique_ptr<syncer_v2::ActivationContext>
                       activation_context) override;

  // Disables the syncing of an off-thread type.
  //
  // Expects that the type is currently enabled.
  // Deletes the worker associated with the type.
  void DisconnectType(syncer::ModelType type) override;

  // Implementation of SyncEncryptionHandler::Observer.
  void OnPassphraseRequired(
      PassphraseRequiredReason reason,
      const sync_pb::EncryptedData& pending_keys) override;
  void OnPassphraseAccepted() override;
  void OnBootstrapTokenUpdated(const std::string& bootstrap_token,
                               BootstrapTokenType type) override;
  void OnEncryptedTypesChanged(ModelTypeSet encrypted_types,
                               bool encrypt_everything) override;
  void OnEncryptionComplete() override;
  void OnCryptographerStateChanged(Cryptographer* cryptographer) override;
  void OnPassphraseTypeChanged(PassphraseType type,
                               base::Time passphrase_time) override;
  void OnLocalSetPassphraseEncryption(
      const SyncEncryptionHandler::NigoriState& nigori_state) override;

  // Gets the set of enabled types.
  ModelTypeSet GetEnabledTypes() const;

  // Returns set of types for which initial set of updates was downloaded and
  // applied.
  ModelTypeSet GetInitialSyncEndedTypes() const;

  // Simple getters.
  UpdateHandlerMap* update_handler_map();
  CommitContributorMap* commit_contributor_map();
  DirectoryTypeDebugInfoEmitterMap* directory_type_debug_info_emitter_map();

  void RegisterDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer);
  void UnregisterDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer);
  bool HasDirectoryTypeDebugInfoObserver(
      const syncer::TypeDebugInfoObserver* observer) const;
  void RequestEmitDebugInfo();

  base::WeakPtr<ModelTypeConnector> AsWeakPtr();

 private:
  void OnEncryptionStateChanged();

  ModelTypeSet GetEnabledNonBlockingTypes() const;
  ModelTypeSet GetEnabledDirectoryTypes() const;

  // Sets of handlers and contributors.
  ScopedVector<DirectoryCommitContributor> directory_commit_contributors_;
  ScopedVector<DirectoryUpdateHandler> directory_update_handlers_;
  ScopedVector<DirectoryTypeDebugInfoEmitter>
      directory_type_debug_info_emitters_;

  ScopedVector<syncer_v2::ModelTypeWorker> model_type_workers_;

  // Maps of UpdateHandlers and CommitContributors.
  // They do not own any of the objects they point to.
  UpdateHandlerMap update_handler_map_;
  CommitContributorMap commit_contributor_map_;

  // Map of DebugInfoEmitters for directory types.
  // Non-blocking types handle debug info differently.
  // Does not own its contents.
  DirectoryTypeDebugInfoEmitterMap directory_type_debug_info_emitter_map_;

  // The known ModelSafeWorkers.
  std::map<ModelSafeGroup, scoped_refptr<ModelSafeWorker>> workers_map_;

  // The directory.  Not owned.
  syncable::Directory* directory_;

  // A copy of the directory's most recent cryptographer.
  std::unique_ptr<Cryptographer> cryptographer_;

  // The set of encrypted types.
  ModelTypeSet encrypted_types_;

  // The NudgeHandler.  Not owned.
  NudgeHandler* nudge_handler_;

  // The set of enabled directory types.
  ModelTypeSet enabled_directory_types_;

  // The set of observers of per-type debug info.
  //
  // Each of the DirectoryTypeDebugInfoEmitters needs such a list.  There's
  // a lot of them, and their lifetimes are unpredictable, so it makes the
  // book-keeping easier if we just store the list here.  That way it's
  // guaranteed to live as long as this sync backend.
  base::ObserverList<TypeDebugInfoObserver> type_debug_info_observers_;

  base::WeakPtrFactory<ModelTypeRegistry> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ModelTypeRegistry);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_MODEL_TYPE_REGISTRY_H_
