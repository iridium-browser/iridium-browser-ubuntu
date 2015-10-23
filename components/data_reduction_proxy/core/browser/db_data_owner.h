// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DB_SERVICE_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DB_SERVICE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"

namespace base {
class FilePath;
}  // namespace base

namespace data_reduction_proxy {
class DataStore;
class DataUsageBucket;
class DataUsageStore;

typedef base::Callback<void(scoped_ptr<DataUsageBucket>)>
    OnLoadDataUsageBucketCallback;

// Contains and initializes all Data Reduction Proxy objects that have a
// lifetime based on the DB task runner.
class DBDataOwner {
 public:
  explicit DBDataOwner(scoped_ptr<DataStore> store);
  virtual ~DBDataOwner();

  // Initializes all the DB objects. Must be called on the DB task runner.
  void InitializeOnDBThread();

  // Loads the last stored data usage bucket from |DataStore| into |bucket|.
  void LoadCurrentDataUsageBucket(DataUsageBucket* bucket);

  // Stores |current| to |DataStore|.
  void StoreCurrentDataUsageBucket(scoped_ptr<DataUsageBucket> current);

  // Returns a weak pointer to self for use on UI thread.
  base::WeakPtr<DBDataOwner> GetWeakPtr();

 private:
  scoped_ptr<DataStore> store_;
  scoped_ptr<DataUsageStore> data_usage_;
  base::SequenceChecker sequence_checker_;
  base::WeakPtrFactory<DBDataOwner> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DBDataOwner);
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DB_SERVICE_H_
