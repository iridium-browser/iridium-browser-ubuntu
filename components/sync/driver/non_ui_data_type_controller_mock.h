// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_NON_UI_DATA_TYPE_CONTROLLER_MOCK_H_
#define COMPONENTS_SYNC_DRIVER_NON_UI_DATA_TYPE_CONTROLLER_MOCK_H_

#include <string>

#include "components/sync/api/sync_error.h"
#include "components/sync/driver/non_ui_data_type_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace sync_driver {

class NonUIDataTypeControllerMock : public NonUIDataTypeController {
 public:
  NonUIDataTypeControllerMock();

  // DataTypeController mocks.
  MOCK_METHOD1(StartAssociating, void(const StartCallback& start_callback));
  MOCK_METHOD1(LoadModels, void(const ModelLoadCallback& model_load_callback));
  MOCK_METHOD0(Stop, void());
  MOCK_CONST_METHOD0(type, syncer::ModelType());
  MOCK_CONST_METHOD0(name, std::string());
  MOCK_CONST_METHOD0(model_safe_group, syncer::ModelSafeGroup());
  MOCK_CONST_METHOD0(state, State());
  MOCK_METHOD1(OnSingleDataTypeUnrecoverableError,
               void(const syncer::SyncError& error));

  // NonUIDataTypeController mocks.
  MOCK_METHOD0(StartModels, bool());
  MOCK_METHOD0(StopModels, void());
  MOCK_METHOD2(PostTaskOnBackendThread,
               bool(const tracked_objects::Location&, const base::Closure&));
  MOCK_METHOD3(StartDone,
               void(DataTypeController::ConfigureResult result,
                    const syncer::SyncMergeResult& local_merge_result,
                    const syncer::SyncMergeResult& syncer_merge_result));
  MOCK_METHOD1(RecordStartFailure, void(ConfigureResult result));

 protected:
  virtual ~NonUIDataTypeControllerMock();
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_NON_UI_DATA_TYPE_CONTROLLER_MOCK_H_
