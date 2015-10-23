// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SYNC_BUNDLE_H_
#define CHROME_BROWSER_EXTENSIONS_SYNC_BUNDLE_H_

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_data.h"

class ExtensionSyncService;

namespace extensions {

class ExtensionSyncData;

class SyncBundle {
 public:
  SyncBundle();
  ~SyncBundle();

  void StartSyncing(scoped_ptr<syncer::SyncChangeProcessor> sync_processor);

  // Resets this class back to its default values, which will disable all
  // syncing until StartSyncing is called again.
  void Reset();

  // Has this bundle started syncing yet?
  // Returns true if StartSyncing has been called, false otherwise.
  bool IsSyncing() const;

  // Handles the given list of local SyncDatas. This updates the set of synced
  // extensions as appropriate, and then pushes the corresponding SyncChanges
  // to the server.
  void PushSyncDataList(const syncer::SyncDataList& sync_data_list);

  // Handles the sync deletion of the given extension. This updates the set of
  // synced extensions as appropriate, and then pushes a SyncChange to the
  // server.
  void PushSyncDeletion(const std::string& extension_id,
                        const syncer::SyncData& sync_data);

  // Pushes any sync changes to |extension| to the server.
  void PushSyncAddOrUpdate(const std::string& extension_id,
                           const syncer::SyncData& sync_data);

  // Applies the given sync change coming in from the server. This just updates
  // the list of synced extensions.
  void ApplySyncData(const ExtensionSyncData& extension_sync_data);

  // Checks if there is pending sync data for the extension with the given |id|
  // that should be sent to the server instead of the local state.
  bool HasPendingExtensionId(const std::string& id) const;

  // Adds a pending extension to be synced.
  void AddPendingExtension(const std::string& id,
                           const ExtensionSyncData& sync_data);

  // Returns a vector of all the pending sync data.
  std::vector<ExtensionSyncData> GetPendingData() const;

 private:
  // Creates a SyncChange to add or update an extension.
  syncer::SyncChange CreateSyncChange(const std::string& extension_id,
                                      const syncer::SyncData& sync_data) const;

  // Pushes the given list of SyncChanges to the server.
  void PushSyncChanges(const syncer::SyncChangeList& sync_change_list);

  void AddSyncedExtension(const std::string& id);
  void RemoveSyncedExtension(const std::string& id);
  bool HasSyncedExtension(const std::string& id) const;

  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;

  // Stores the set of extensions we know about. Used to decide if a sync change
  // should be ACTION_ADD or ACTION_UPDATE.
  std::set<std::string> synced_extensions_;

  // This stores changes we got from sync that we couldn't apply immediately
  // (such as installing a new extension, or an update). We'll send this back
  // to the server instead of the local state, to prevent the sync state from
  // flipping back and forth until all clients are on the same state.
  std::map<std::string, ExtensionSyncData> pending_sync_data_;

  DISALLOW_COPY_AND_ASSIGN(SyncBundle);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SYNC_BUNDLE_H_
