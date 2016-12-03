// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_CORE_TEST_SYNC_MANAGER_FACTORY_FOR_PROFILE_SYNC_TEST_H_
#define COMPONENTS_SYNC_CORE_TEST_SYNC_MANAGER_FACTORY_FOR_PROFILE_SYNC_TEST_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "components/sync/core/sync_manager_factory.h"

namespace syncer {

class SyncManagerFactoryForProfileSyncTest : public syncer::SyncManagerFactory {
 public:
  explicit SyncManagerFactoryForProfileSyncTest(base::Closure init_callback);
  ~SyncManagerFactoryForProfileSyncTest() override;
  std::unique_ptr<syncer::SyncManager> CreateSyncManager(
      const std::string& name) override;

 private:
  base::Closure init_callback_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_CORE_TEST_SYNC_MANAGER_FACTORY_FOR_PROFILE_SYNC_TEST_H_
