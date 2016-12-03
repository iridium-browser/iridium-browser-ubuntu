// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SHARED_CHANGE_PROCESSOR_REF_H_
#define COMPONENTS_SYNC_DRIVER_SHARED_CHANGE_PROCESSOR_REF_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "components/sync/api/sync_change_processor.h"
#include "components/sync/api/sync_error_factory.h"
#include "components/sync/driver/shared_change_processor.h"

namespace sync_driver {

// A syncer::SyncChangeProcessor stub for interacting with a refcounted
// SharedChangeProcessor.
class SharedChangeProcessorRef : public syncer::SyncChangeProcessor,
                                 public syncer::SyncErrorFactory {
 public:
  SharedChangeProcessorRef(
      const scoped_refptr<SharedChangeProcessor>& change_processor);
  ~SharedChangeProcessorRef() override;

  // syncer::SyncChangeProcessor implementation.
  syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) override;
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override;
  syncer::SyncError UpdateDataTypeContext(
      syncer::ModelType type,
      syncer::SyncChangeProcessor::ContextRefreshStatus refresh_status,
      const std::string& context) override;

  // syncer::SyncErrorFactory implementation.
  syncer::SyncError CreateAndUploadError(
      const tracked_objects::Location& from_here,
      const std::string& message) override;

  // Default copy and assign welcome (and safe due to refcounted-ness).

 private:
  scoped_refptr<SharedChangeProcessor> change_processor_;
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_SHARED_CHANGE_PROCESSOR_REF_H_
