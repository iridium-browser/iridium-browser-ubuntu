// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_METADATA_STORE_IMPL_H_
#define CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_METADATA_STORE_IMPL_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/offline_pages/offline_page_metadata_store.h"

namespace base {
class FilePath;
}

namespace offline_pages {

class OfflinePageEntry;

// Implements OfflinePageMetadataStore using leveldb_proto::ProtoDatabase
// component. Stores metadata of offline pages as serialized protobufs in a
// LevelDB key/value pairs.
// Underlying implementation guarantees that all of the method calls will be
// executed sequentially, and started operations will finish even after the
// store is already destroyed (callbacks will be called).
class OfflinePageMetadataStoreImpl : public OfflinePageMetadataStore {
 public:
  OfflinePageMetadataStoreImpl(
      scoped_ptr<leveldb_proto::ProtoDatabase<OfflinePageEntry>> database,
      const base::FilePath& database_dir);
  ~OfflinePageMetadataStoreImpl() override;

  // OfflinePageMetadataStore implementation:
  void Load(const LoadCallback& callback) override;
  void AddOfflinePage(const OfflinePageItem& offline_page_record,
                      const UpdateCallback& callback) override;
  void RemoveOfflinePages(const std::vector<int64>& bookmark_ids,
                          const UpdateCallback& callback) override;

 private:
  // Callback for when initialization of the |database_| is done.
  void OnInitDone(bool success);

  // Implements the update.
  void UpdateEntries(
      scoped_ptr<leveldb_proto::ProtoDatabase<OfflinePageEntry>::KeyEntryVector>
          entries_to_save,
      scoped_ptr<std::vector<std::string>> keys_to_remove,
      const UpdateCallback& callback);

  // Resets the database. This is to be used when one of the operations fails
  // with no good explanation.
  void ResetDB();

  scoped_ptr<leveldb_proto::ProtoDatabase<OfflinePageEntry>> database_;

  base::WeakPtrFactory<OfflinePageMetadataStoreImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageMetadataStoreImpl);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_METADATA_STORE_IMPL_H_
