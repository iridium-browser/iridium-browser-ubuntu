// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/syncable_service.h"

namespace syncer {

SyncableService::~SyncableService() {}

scoped_ptr<AttachmentStoreForSync>
SyncableService::GetAttachmentStoreForSync() {
  return scoped_ptr<AttachmentStoreForSync>();
}

void SyncableService::SetAttachmentService(
    scoped_ptr<AttachmentService> attachment_service) {
}

}  // namespace syncer
