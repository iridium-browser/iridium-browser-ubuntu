// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SERVICE_H_
#define COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SERVICE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/sync/api/model_type_service.h"
#include "components/sync/api/model_type_store.h"
#include "components/sync/core/simple_metadata_change_list.h"
#include "components/sync/device_info/device_info_tracker.h"
#include "components/sync/device_info/local_device_info_provider.h"

namespace syncer {
class SyncError;
}  // namespace syncer

namespace syncer_v2 {
class ModelTypeChangeProcessor;
}  // namespace syncer_v2

namespace sync_pb {
class DeviceInfoSpecifics;
}  // namespace sync_pb

namespace sync_driver_v2 {

// USS service implementation for DEVICE_INFO model type. Handles storage of
// device info and associated sync metadata, applying/merging foreign changes,
// and allows public read access.
class DeviceInfoService : public syncer_v2::ModelTypeService,
                          public sync_driver::DeviceInfoTracker {
 public:
  typedef base::Callback<void(
      const syncer_v2::ModelTypeStore::InitCallback& callback)>
      StoreFactoryFunction;

  DeviceInfoService(
      sync_driver::LocalDeviceInfoProvider* local_device_info_provider,
      const StoreFactoryFunction& callback,
      const ChangeProcessorFactory& change_processor_factory);
  ~DeviceInfoService() override;

  // ModelTypeService implementation.
  std::unique_ptr<syncer_v2::MetadataChangeList> CreateMetadataChangeList()
      override;
  syncer::SyncError MergeSyncData(
      std::unique_ptr<syncer_v2::MetadataChangeList> metadata_change_list,
      syncer_v2::EntityDataMap entity_data_map) override;
  syncer::SyncError ApplySyncChanges(
      std::unique_ptr<syncer_v2::MetadataChangeList> metadata_change_list,
      syncer_v2::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllData(DataCallback callback) override;
  std::string GetClientTag(const syncer_v2::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer_v2::EntityData& entity_data) override;
  void OnChangeProcessorSet() override;

  // DeviceInfoTracker implementation.
  bool IsSyncing() const override;
  std::unique_ptr<sync_driver::DeviceInfo> GetDeviceInfo(
      const std::string& client_id) const override;
  ScopedVector<sync_driver::DeviceInfo> GetAllDeviceInfo() const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  int CountActiveDevices() const override;

 private:
  friend class DeviceInfoServiceTest;

  // Cache of all syncable and local data, stored by device cache guid.
  using ClientIdToSpecifics =
      std::map<std::string, std::unique_ptr<sync_pb::DeviceInfoSpecifics>>;

  static std::unique_ptr<sync_pb::DeviceInfoSpecifics> CopyToSpecifics(
      const sync_driver::DeviceInfo& info);

  // Allocate new DeviceInfo from SyncData.
  static std::unique_ptr<sync_driver::DeviceInfo> CopyToModel(
      const sync_pb::DeviceInfoSpecifics& specifics);
  // Conversion as we prepare to hand data to the processor.
  static std::unique_ptr<syncer_v2::EntityData> CopyToEntityData(
      const sync_pb::DeviceInfoSpecifics& specifics);

  // Store SyncData in the cache and durable storage.
  void StoreSpecifics(std::unique_ptr<sync_pb::DeviceInfoSpecifics> specifics,
                      syncer_v2::ModelTypeStore::WriteBatch* batch);
  // Delete SyncData from the cache and durable storage, returns true if there
  // was actually anything at the given tag.
  bool DeleteSpecifics(const std::string& tag,
                       syncer_v2::ModelTypeStore::WriteBatch* batch);

  // Notify all registered observers.
  void NotifyObservers();

  // Used as callback given to LocalDeviceInfoProvider.
  void OnProviderInitialized();

  // Methods used as callbacks given to DataTypeStore.
  void OnStoreCreated(syncer_v2::ModelTypeStore::Result result,
                      std::unique_ptr<syncer_v2::ModelTypeStore> store);
  void OnReadAllData(
      syncer_v2::ModelTypeStore::Result result,
      std::unique_ptr<syncer_v2::ModelTypeStore::RecordList> record_list);
  void OnReadAllMetadata(
      syncer_v2::ModelTypeStore::Result result,
      std::unique_ptr<syncer_v2::ModelTypeStore::RecordList> metadata_records,
      const std::string& global_metadata);
  void OnCommit(syncer_v2::ModelTypeStore::Result result);

  // Load metadata if the data is loaded and the provider is initialized.
  void LoadMetadataIfReady();

  // Performs reconciliation between the locally provided device info and the
  // stored device info data. If the sets of data differ, then we consider this
  // a local change and we send it to the processor.
  void ReconcileLocalAndStored();

  // Stores the updated version of the local copy of device info in durable
  // storage, in memory, and informs sync of the change. Should not be called
  // before the provider and processor have initialized.
  void SendLocalData();

  // Persists the changes in the given aggregators and notifies observers if
  // indicated to do as such.
  void CommitAndNotify(
      std::unique_ptr<syncer_v2::ModelTypeStore::WriteBatch> batch,
      std::unique_ptr<syncer_v2::MetadataChangeList> metadata_change_list,
      bool should_notify);

  // Counts the number of active devices relative to |now|. The activeness of a
  // device depends on the amount of time since it was updated, which means
  // comparing it against the current time. |now| is passed into this method to
  // allow unit tests to control expected results.
  int CountActiveDevices(const base::Time now) const;

  // Report an error starting up to sync if it tries to connect to this
  // datatype, since these errors prevent us from knowing if sync is enabled.
  void ReportStartupErrorToSync(const std::string& msg);

  // Find the timestamp for the last time this |device_info| was edited.
  static base::Time GetLastUpdateTime(
      const sync_pb::DeviceInfoSpecifics& specifics);

  // |local_device_info_provider_| isn't owned.
  const sync_driver::LocalDeviceInfoProvider* const local_device_info_provider_;

  ClientIdToSpecifics all_data_;

  // Registered observers, not owned.
  base::ObserverList<Observer, true> observers_;

  // Used to listen for provider initialization. If the provider is already
  // initialized during our constructor then the subscription is never used.
  std::unique_ptr<sync_driver::LocalDeviceInfoProvider::Subscription>
      subscription_;

  // In charge of actually persiting changes to disk, or loading previous data.
  std::unique_ptr<syncer_v2::ModelTypeStore> store_;

  // If |local_device_info_provider_| has initialized.
  bool has_provider_initialized_ = false;
  // If data has been loaded from the store.
  bool has_data_loaded_ = false;
  // if |change_processor()| has been given metadata.
  bool has_metadata_loaded_ = false;

  // Used to update our local device info once every pulse interval.
  base::OneShotTimer pulse_timer_;

  DISALLOW_COPY_AND_ASSIGN(DeviceInfoService);
};

}  // namespace sync_driver_v2

#endif  // COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SERVICE_H_
