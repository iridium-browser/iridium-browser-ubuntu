// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_NON_BLOCKING_SYNC_COMMON_H_
#define SYNC_INTERNAL_API_PUBLIC_NON_BLOCKING_SYNC_COMMON_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "sync/base/sync_export.h"
#include "sync/protocol/sync.pb.h"

namespace syncer_v2 {

static const int64 kUncommittedVersion = -1;

// Data-type global state that must be accessed and updated on the sync thread,
// but persisted on or through the model thread.
struct SYNC_EXPORT_PRIVATE DataTypeState {
  DataTypeState();
  ~DataTypeState();

  // The latest progress markers received from the server.
  sync_pb::DataTypeProgressMarker progress_marker;

  // A data type context.  Sent to the server in every commit or update
  // request.  May be updated by either by responses from the server or
  // requests made on the model thread.  The interpretation of this value may
  // be data-type specific.  Many data types ignore it.
  sync_pb::DataTypeContext type_context;

  // This value is set if this type's data should be encrypted on the server.
  // If this key changes, the client will need to re-commit all of its local
  // data to the server using the new encryption key.
  std::string encryption_key_name;

  // This flag is set to true when the first download cycle is complete.  The
  // ModelTypeSyncProxy should not attempt to commit any items until this
  // flag is set.
  bool initial_sync_done;
};

struct SYNC_EXPORT_PRIVATE CommitRequestData {
  CommitRequestData();
  ~CommitRequestData();

  std::string id;
  std::string client_tag_hash;

  // Strictly incrementing number for in-progress commits.  More information
  // about its meaning can be found in comments in the files that make use of
  // this struct.
  int64 sequence_number;

  int64 base_version;
  base::Time ctime;
  base::Time mtime;
  std::string non_unique_name;
  bool deleted;
  sync_pb::EntitySpecifics specifics;
};

struct SYNC_EXPORT_PRIVATE CommitResponseData {
  CommitResponseData();
  ~CommitResponseData();

  std::string id;
  std::string client_tag_hash;
  int64 sequence_number;
  int64 response_version;
};

struct SYNC_EXPORT_PRIVATE UpdateResponseData {
  UpdateResponseData();
  ~UpdateResponseData();

  std::string id;
  std::string client_tag_hash;
  int64 response_version;
  base::Time ctime;
  base::Time mtime;
  std::string non_unique_name;
  bool deleted;
  sync_pb::EntitySpecifics specifics;
  std::string encryption_key_name;
};

typedef std::vector<CommitRequestData> CommitRequestDataList;
typedef std::vector<CommitResponseData> CommitResponseDataList;
typedef std::vector<UpdateResponseData> UpdateResponseDataList;

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_NON_BLOCKING_SYNC_COMMON_H_
