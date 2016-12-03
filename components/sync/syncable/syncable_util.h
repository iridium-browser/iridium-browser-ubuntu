// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SYNCABLE_SYNCABLE_UTIL_H_
#define COMPONENTS_SYNC_SYNCABLE_SYNCABLE_UTIL_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "components/sync/base/model_type.h"

namespace tracked_objects {
class Location;
}

namespace syncer {
namespace syncable {

class BaseTransaction;
class BaseWriteTransaction;
class ModelNeutralMutableEntry;
class Id;

void ChangeEntryIDAndUpdateChildren(BaseWriteTransaction* trans,
                                    ModelNeutralMutableEntry* entry,
                                    const Id& new_id);

bool IsLegalNewParent(BaseTransaction* trans, const Id& id, const Id& parentid);

bool SyncAssert(bool condition,
                const tracked_objects::Location& location,
                const char* msg,
                BaseTransaction* trans);

int GetUnsyncedEntries(BaseTransaction* trans, std::vector<int64_t>* handles);

// Generates a fixed-length tag for the given string under the given model_type.
std::string GenerateSyncableHash(ModelType model_type,
                                 const std::string& client_tag);

// A helper for generating the bookmark type's tag.  This is required in more
// than one place, so we define the algorithm here to make sure the
// implementation is consistent.
std::string GenerateSyncableBookmarkHash(
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id);

}  // namespace syncable
}  // namespace syncer

#endif  // COMPONENTS_SYNC_SYNCABLE_SYNCABLE_UTIL_H_
