// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/api/model_type_store.h"

#include "components/sync/core/model_type_store_impl.h"

namespace syncer_v2 {

// static
void ModelTypeStore::CreateInMemoryStoreForTest(const InitCallback& callback) {
  ModelTypeStoreImpl::CreateInMemoryStoreForTest(callback);
}

// static
void ModelTypeStore::CreateStore(
    const syncer::ModelType type,
    const std::string& path,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const InitCallback& callback) {
  ModelTypeStoreImpl::CreateStore(type, path, blocking_task_runner, callback);
}

ModelTypeStore::~ModelTypeStore() {}

ModelTypeStore::WriteBatch::WriteBatch() {}

ModelTypeStore::WriteBatch::~WriteBatch() {}

}  // namespace syncer_v2
