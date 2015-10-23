// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/db_data_owner.h"

#include "base/logging.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/data_reduction_proxy/core/browser/data_store.h"
#include "components/data_reduction_proxy/core/browser/data_usage_store.h"
#include "components/data_reduction_proxy/proto/data_store.pb.h"

namespace data_reduction_proxy {

DBDataOwner::DBDataOwner(scoped_ptr<DataStore> store)
    : store_(store.Pass()),
      data_usage_(new DataUsageStore(store_.get())),
      weak_factory_(this) {
  sequence_checker_.DetachFromSequence();
}

DBDataOwner::~DBDataOwner() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
}

void DBDataOwner::InitializeOnDBThread() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  store_->InitializeOnDBThread();
}

void DBDataOwner::LoadCurrentDataUsageBucket(DataUsageBucket* bucket) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  data_usage_->LoadCurrentDataUsageBucket(bucket);
}

void DBDataOwner::StoreCurrentDataUsageBucket(
    scoped_ptr<DataUsageBucket> current) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  data_usage_->StoreCurrentDataUsageBucket(*current.get());
}

base::WeakPtr<DBDataOwner> DBDataOwner::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace data_reduction_proxy
