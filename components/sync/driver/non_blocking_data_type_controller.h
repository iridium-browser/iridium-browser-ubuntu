// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_NON_BLOCKING_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_DRIVER_NON_BLOCKING_DATA_TYPE_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/data_type_controller.h"
#include "components/sync/driver/sync_prefs.h"

namespace sync_driver {
class SyncClient;
}

namespace syncer_v2 {
struct ActivationContext;
}

namespace sync_driver_v2 {

// Base class for DataType controllers for Unified Sync and Storage datatypes.
// Derived types must implement the following methods:
// - RunOnModelThread
// - RunOnUIThread
class NonBlockingDataTypeController : public sync_driver::DataTypeController {
 public:
  NonBlockingDataTypeController(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
      const base::Closure& error_callback,
      syncer::ModelType model_type,
      sync_driver::SyncClient* sync_client);

  // DataTypeErrorHandler interface.
  void OnSingleDataTypeUnrecoverableError(
      const syncer::SyncError& error) override;

  // DataTypeController interface.
  bool ShouldLoadModelBeforeConfigure() const override;
  void LoadModels(const ModelLoadCallback& model_load_callback) override;

  // Registers non-blocking data type with sync backend. In the process the
  // activation context is passed to ModelTypeRegistry, where ModelTypeWorker
  // gets created and connected with ModelTypeProcessor.
  void RegisterWithBackend(
      sync_driver::BackendDataTypeConfigurer* configurer) override;
  void StartAssociating(const StartCallback& start_callback) override;
  void ActivateDataType(
      sync_driver::BackendDataTypeConfigurer* configurer) override;
  void DeactivateDataType(
      sync_driver::BackendDataTypeConfigurer* configurer) override;
  void Stop() override;
  std::string name() const override;
  State state() const override;
  syncer::ModelType type() const override;

 protected:
  // DataTypeController is RefCounted.
  ~NonBlockingDataTypeController() override;

  // Returns true if the call is made on UI thread.
  bool BelongsToUIThread() const;

  // Posts the given task to the model thread, i.e. the thread the
  // datatype lives on.  Return value: True if task posted successfully,
  // false otherwise.
  virtual bool RunOnModelThread(const tracked_objects::Location& from_here,
                                const base::Closure& task) = 0;

  // Post the given task on the UI thread. If the call is made on UI thread
  // already, make a direct call without posting.
  virtual void RunOnUIThread(const tracked_objects::Location& from_here,
                             const base::Closure& task) = 0;

 private:
  void RecordStartFailure(ConfigureResult result) const;
  void RecordUnrecoverableError();
  void ReportLoadModelError(ConfigureResult result,
                            const syncer::SyncError& error);

  // If the DataType controller is waiting for models to load, once the models
  // are loaded this function should be called to let the base class
  // implementation know that it is safe to continue with the activation.
  // The error indicates whether the loading completed successfully.
  void LoadModelsDone(ConfigureResult result, const syncer::SyncError& error);

  // The function will do the real work when OnProcessorStarted got called. This
  // is called on the UI thread.
  void OnProcessorStarted(
      syncer::SyncError error,
      std::unique_ptr<syncer_v2::ActivationContext> activation_context);

  // Model Type for this controller
  syncer::ModelType model_type_;

  // Sync client
  sync_driver::SyncClient* const sync_client_;

  // Sync prefs. Used for determinig if DisableSync should be called during call
  // to Stop().
  sync_driver::SyncPrefs sync_prefs_;

  // State of this datatype controller.
  State state_;

  // Callbacks for use when starting the datatype.
  ModelLoadCallback model_load_callback_;

  // Controller receives |activation_context_| from SharedModelTypeProcessor
  // callback and must temporarily own it until ActivateDataType is called.
  std::unique_ptr<syncer_v2::ActivationContext> activation_context_;

  DISALLOW_COPY_AND_ASSIGN(NonBlockingDataTypeController);
};

}  // namespace sync_driver_v2

#endif  // COMPONENTS_SYNC_DRIVER_NON_BLOCKING_DATA_TYPE_CONTROLLER_H_
