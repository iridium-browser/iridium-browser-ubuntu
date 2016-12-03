// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_CORE_TEST_TEST_INTERNAL_COMPONENTS_FACTORY_H_
#define COMPONENTS_SYNC_CORE_TEST_TEST_INTERNAL_COMPONENTS_FACTORY_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/sync/core/internal_components_factory.h"

namespace syncer {

class TestInternalComponentsFactory : public InternalComponentsFactory {
 public:
  explicit TestInternalComponentsFactory(const Switches& switches,
                                         StorageOption option,
                                         StorageOption* storage_used);
  ~TestInternalComponentsFactory() override;

  std::unique_ptr<SyncScheduler> BuildScheduler(
      const std::string& name,
      SyncCycleContext* context,
      syncer::CancelationSignal* cancelation_signal) override;

  std::unique_ptr<SyncCycleContext> BuildContext(
      ServerConnectionManager* connection_manager,
      syncable::Directory* directory,
      ExtensionsActivity* monitor,
      const std::vector<SyncEngineEventListener*>& listeners,
      DebugInfoGetter* debug_info_getter,
      ModelTypeRegistry* model_type_registry,
      const std::string& invalidator_client_id) override;

  std::unique_ptr<syncable::DirectoryBackingStore> BuildDirectoryBackingStore(
      StorageOption storage,
      const std::string& dir_name,
      const base::FilePath& backing_filepath) override;

  Switches GetSwitches() const override;

 private:
  const Switches switches_;
  const StorageOption storage_override_;
  StorageOption* storage_used_;

  DISALLOW_COPY_AND_ASSIGN(TestInternalComponentsFactory);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_CORE_TEST_TEST_INTERNAL_COMPONENTS_FACTORY_H_
