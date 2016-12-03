// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SYNC_SERVICE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/sync/api/sync_change.h"
#include "components/sync/api/sync_change_processor.h"
#include "components/sync/api/sync_data.h"
#include "components/sync/api/sync_error_factory.h"
#include "components/sync/api/syncable_service.h"
#include "components/sync/device_info/device_info_tracker.h"

namespace sync_driver {

class LocalDeviceInfoProvider;

// SyncableService implementation for DEVICE_INFO model type.
class DeviceInfoSyncService : public syncer::SyncableService,
                              public DeviceInfoTracker {
 public:
  explicit DeviceInfoSyncService(
      LocalDeviceInfoProvider* local_device_info_provider);
  ~DeviceInfoSyncService() override;

  // syncer::SyncableService implementation.
  syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
      std::unique_ptr<syncer::SyncErrorFactory> error_handler) override;
  void StopSyncing(syncer::ModelType type) override;
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override;
  syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) override;

  // DeviceInfoTracker implementation.
  bool IsSyncing() const override;
  std::unique_ptr<DeviceInfo> GetDeviceInfo(
      const std::string& client_id) const override;
  ScopedVector<DeviceInfo> GetAllDeviceInfo() const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  int CountActiveDevices() const override;

 private:
  friend class DeviceInfoSyncServiceTest;

  // Create SyncData from local DeviceInfo.
  syncer::SyncData CreateLocalData(const DeviceInfo* info);
  // Create SyncData from EntitySpecifics.
  static syncer::SyncData CreateLocalData(
      const sync_pb::EntitySpecifics& entity);

  // Allocate new DeviceInfo from SyncData.
  static DeviceInfo* CreateDeviceInfo(const syncer::SyncData& sync_data);
  // Store SyncData in the cache.
  void StoreSyncData(const std::string& client_id,
                     const syncer::SyncData& sync_data);
  // Delete SyncData from the cache.
  void DeleteSyncData(const std::string& client_id);
  // Notify all registered observers.
  void NotifyObservers();

  // Sends a copy of the current device's state to the processor/sync.
  void SendLocalData(const syncer::SyncChange::SyncChangeType change_type);

  // Finds the number of active devices give the current time, which allows for
  // better unit tests.
  int CountActiveDevices(const base::Time now) const;

  // Find the timestamp for the last time this |device_info| was edited.
  static base::Time GetLastUpdateTime(const syncer::SyncData& device_info);

  // |local_device_info_provider_| isn't owned.
  const LocalDeviceInfoProvider* const local_device_info_provider_;

  // Receives ownership of |sync_processor_| and |error_handler_| in
  // MergeDataAndStartSyncing() and destroy them in StopSyncing().
  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;
  std::unique_ptr<syncer::SyncErrorFactory> error_handler_;

  // Cache of all syncable and local data.
  typedef std::map<std::string, syncer::SyncData> SyncDataMap;
  SyncDataMap all_data_;

  // Registered observers, not owned.
  base::ObserverList<Observer, true> observers_;

  // Used to update our local device info once every pulse interval.
  base::OneShotTimer pulse_timer_;

  DISALLOW_COPY_AND_ASSIGN(DeviceInfoSyncService);
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SYNC_SERVICE_H_
