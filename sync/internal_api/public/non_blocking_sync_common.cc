// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/non_blocking_sync_common.h"

namespace syncer_v2 {

DataTypeState::DataTypeState() : initial_sync_done(false) {
}

DataTypeState::~DataTypeState() {
}

CommitRequestData::CommitRequestData()
    : sequence_number(0),
      base_version(0),
      deleted(false) {
}

CommitRequestData::~CommitRequestData() {
}

CommitResponseData::CommitResponseData()
    : sequence_number(0),
      response_version(0) {
}

CommitResponseData::~CommitResponseData() {
}

UpdateResponseData::UpdateResponseData()
    : response_version(0),
      deleted(false) {
}

UpdateResponseData::~UpdateResponseData() {
}

}  // namespace syncer
