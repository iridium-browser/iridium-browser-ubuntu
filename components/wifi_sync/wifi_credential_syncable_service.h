// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WIFI_SYNC_WIFI_CREDENTIAL_SYNCABLE_SERVICE_H_
#define COMPONENTS_WIFI_SYNC_WIFI_CREDENTIAL_SYNCABLE_SERVICE_H_

#include <map>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/wifi_sync/wifi_config_delegate.h"
#include "components/wifi_sync/wifi_credential.h"
#include "components/wifi_sync/wifi_security_class.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/syncable_service.h"

namespace wifi_sync {

// KeyedService that synchronizes WiFi credentials between local settings,
// and Chrome Sync.
//
// This service does not necessarily own the storage for WiFi
// credentials. In particular, on ChromeOS, WiFi credential storage is
// managed by the ChromeOS connection manager ("Shill").
//
// On ChromeOS, this class should only be instantiated
// for the primary user profile, as that is the only profile for
// which a Shill profile is loaded.
class WifiCredentialSyncableService
    : public syncer::SyncableService, public KeyedService {
 public:
  // Constructs a syncable service. Changes from Chrome Sync will be
  // applied locally by |network_config_delegate|. Local changes will
  // be propagated to Chrome Sync using the |sync_processor| provided
  // in the call to MergeDataAndStartSyncing.
  explicit WifiCredentialSyncableService(
      scoped_ptr<WifiConfigDelegate> network_config_delegate);
  ~WifiCredentialSyncableService() override;

  // syncer::SyncableService implementation.
  syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> error_handler) override;
  void StopSyncing(syncer::ModelType type) override;
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override;
  syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& caller_location,
      const syncer::SyncChangeList& change_list) override;

  // Adds a WiFiCredential to Chrome Sync. |item_id| is a persistent
  // identifier which can be used to later remove the credential. It
  // is an error to add a network that already exists. It is also an
  // error to call this method before MergeDataAndStartSyncing(), or
  // after StopSyncing().
  //
  // TODO(quiche): Allow changing a credential, by addding it again.
  // crbug.com/431436
  bool AddToSyncedNetworks(const std::string& item_id,
                           const WifiCredential& credential);

 private:
  using SsidAndSecurityClass =
      std::pair<WifiCredential::SsidBytes, WifiSecurityClass>;
  using SsidAndSecurityClassToPassphrase =
      std::map<SsidAndSecurityClass, std::string>;

  // The syncer::ModelType that this SyncableService processes and
  // generates updates for.
  static const syncer::ModelType kModelType;

  // The object we use to change local network configuration.
  const scoped_ptr<WifiConfigDelegate> network_config_delegate_;

  // Our SyncChangeProcessor instance. Used to push changes into
  // Chrome Sync.
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;

  // The networks and passphrases that are already known by Chrome
  // Sync. All synced networks must be included in this map, even if
  // they do not use passphrases.
  SsidAndSecurityClassToPassphrase synced_networks_and_passphrases_;

  DISALLOW_COPY_AND_ASSIGN(WifiCredentialSyncableService);
};

}  // namespace wifi_sync

#endif  // COMPONENTS_WIFI_SYNC_WIFI_CREDENTIAL_SYNCABLE_SERVICE_H_
