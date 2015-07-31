// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/frontend_data_type_controller.h"

#include "base/logging.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "components/sync_driver/change_processor.h"
#include "components/sync_driver/model_associator.h"
#include "content/public/browser/browser_thread.h"
#include "sync/api/sync_error.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/util/data_type_histogram.h"

using content::BrowserThread;

namespace browser_sync {

// TODO(tim): Legacy controllers are being left behind in componentization
// effort for now, hence passing null DisableTypeCallback and still having
// a dependency on ProfileSyncService.  That dep can probably be removed
// without too much work.
FrontendDataTypeController::FrontendDataTypeController(
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread,
    const base::Closure& error_callback,
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : DataTypeController(ui_thread, error_callback),
      profile_sync_factory_(profile_sync_factory),
      profile_(profile),
      sync_service_(sync_service),
      state_(NOT_RUNNING) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(profile_sync_factory);
  DCHECK(profile);
  DCHECK(sync_service);
}

void FrontendDataTypeController::LoadModels(
    const ModelLoadCallback& model_load_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  model_load_callback_ = model_load_callback;

  if (state_ != NOT_RUNNING) {
    model_load_callback.Run(type(),
                            syncer::SyncError(FROM_HERE,
                                              syncer::SyncError::DATATYPE_ERROR,
                                              "Model already running",
                                              type()));
    return;
  }

  state_ = MODEL_STARTING;
  if (!StartModels()) {
    // If we are waiting for some external service to load before associating
    // or we failed to start the models, we exit early. state_ will control
    // what we perform next.
    DCHECK(state_ == NOT_RUNNING || state_ == MODEL_STARTING);
    return;
  }

  OnModelLoaded();
}

void FrontendDataTypeController::OnModelLoaded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(state_, MODEL_STARTING);

  state_ = MODEL_LOADED;
  model_load_callback_.Run(type(), syncer::SyncError());
}

void FrontendDataTypeController::StartAssociating(
    const StartCallback& start_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!start_callback.is_null());
  DCHECK_EQ(state_, MODEL_LOADED);

  start_callback_ = start_callback;
  state_ = ASSOCIATING;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
    FROM_HERE, base::Bind(&FrontendDataTypeController::Associate, this));
}

void FrontendDataTypeController::Stop() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (state_ == NOT_RUNNING)
    return;

  State prev_state = state_;
  state_ = STOPPING;

  // If Stop() is called while Start() is waiting for the datatype model to
  // load, abort the start.
  if (prev_state == MODEL_STARTING) {
    AbortModelLoad();
    // We can just return here since we haven't performed association if we're
    // still in MODEL_STARTING.
    return;
  }

  CleanUpState();

  sync_service_->DeactivateDataType(type());

  if (model_associator()) {
    syncer::SyncError error;  // Not used.
    error = model_associator()->DisassociateModels();
  }

  set_model_associator(NULL);
  change_processor_.reset();

  state_ = NOT_RUNNING;
}

syncer::ModelSafeGroup FrontendDataTypeController::model_safe_group()
    const {
  return syncer::GROUP_UI;
}

std::string FrontendDataTypeController::name() const {
  // For logging only.
  return syncer::ModelTypeToString(type());
}

sync_driver::DataTypeController::State FrontendDataTypeController::state()
    const {
  return state_;
}

void FrontendDataTypeController::OnSingleDataTypeUnrecoverableError(
    const syncer::SyncError& error) {
  DCHECK_EQ(type(), error.model_type());
  RecordUnrecoverableError(error.location(), error.message());
  if (!model_load_callback_.is_null()) {
    syncer::SyncMergeResult local_merge_result(type());
    local_merge_result.set_error(error);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(model_load_callback_, type(), error));
  }
}

FrontendDataTypeController::FrontendDataTypeController()
    : DataTypeController(base::ThreadTaskRunnerHandle::Get(), base::Closure()),
      profile_sync_factory_(NULL),
      profile_(NULL),
      sync_service_(NULL),
      state_(NOT_RUNNING) {
}

FrontendDataTypeController::~FrontendDataTypeController() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

bool FrontendDataTypeController::StartModels() {
  DCHECK_EQ(state_, MODEL_STARTING);
  // By default, no additional services need to be started before we can proceed
  // with model association.
  return true;
}

void FrontendDataTypeController::RecordUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  DVLOG(1) << "Datatype Controller failed for type "
           << ModelTypeToString(type()) << "  "
           << message << " at location "
           << from_here.ToString();
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeRunFailures",
                            ModelTypeToHistogramInt(type()),
                            syncer::MODEL_TYPE_COUNT);

  if (!error_callback_.is_null())
    error_callback_.Run();
}

void FrontendDataTypeController::Associate() {
  if (state_ != ASSOCIATING) {
    // Stop() must have been called while Associate() task have been waiting.
    DCHECK_EQ(state_, NOT_RUNNING);
    return;
  }

  syncer::SyncMergeResult local_merge_result(type());
  syncer::SyncMergeResult syncer_merge_result(type());
  CreateSyncComponents();
  if (!model_associator()->CryptoReadyIfNecessary()) {
    StartDone(NEEDS_CRYPTO, local_merge_result, syncer_merge_result);
    return;
  }

  bool sync_has_nodes = false;
  if (!model_associator()->SyncModelHasUserCreatedNodes(&sync_has_nodes)) {
    syncer::SyncError error(FROM_HERE,
                            syncer::SyncError::UNRECOVERABLE_ERROR,
                            "Failed to load sync nodes",
                            type());
    local_merge_result.set_error(error);
    StartDone(UNRECOVERABLE_ERROR, local_merge_result, syncer_merge_result);
    return;
  }

  // TODO(zea): Have AssociateModels fill the local and syncer merge results.
  base::TimeTicks start_time = base::TimeTicks::Now();
  syncer::SyncError error;
  error = model_associator()->AssociateModels(
      &local_merge_result,
      &syncer_merge_result);
  // TODO(lipalani): crbug.com/122690 - handle abort.
  RecordAssociationTime(base::TimeTicks::Now() - start_time);
  if (error.IsSet()) {
    local_merge_result.set_error(error);
    StartDone(ASSOCIATION_FAILED, local_merge_result, syncer_merge_result);
    return;
  }

  state_ = RUNNING;
  // FinishStart() invokes the DataTypeManager callback, which can lead to a
  // call to Stop() if one of the other data types being started generates an
  // error.
  StartDone(!sync_has_nodes ? OK_FIRST_RUN : OK,
            local_merge_result,
            syncer_merge_result);
}

void FrontendDataTypeController::CleanUpState() {
  // Do nothing by default.
}

void FrontendDataTypeController::CleanUp() {
  CleanUpState();
  set_model_associator(NULL);
  change_processor_.reset();
}

void FrontendDataTypeController::AbortModelLoad() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CleanUp();
  state_ = NOT_RUNNING;
}

void FrontendDataTypeController::StartDone(
    ConfigureResult start_result,
    const syncer::SyncMergeResult& local_merge_result,
    const syncer::SyncMergeResult& syncer_merge_result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!IsSuccessfulResult(start_result)) {
    if (IsUnrecoverableResult(start_result))
      RecordUnrecoverableError(FROM_HERE, "StartFailed");

    CleanUp();
    if (start_result == ASSOCIATION_FAILED) {
      state_ = DISABLED;
    } else {
      state_ = NOT_RUNNING;
    }
    RecordStartFailure(start_result);
  }

  start_callback_.Run(start_result, local_merge_result, syncer_merge_result);
}

void FrontendDataTypeController::RecordAssociationTime(base::TimeDelta time) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#define PER_DATA_TYPE_MACRO(type_str) \
    UMA_HISTOGRAM_TIMES("Sync." type_str "AssociationTime", time);
  SYNC_DATA_TYPE_HISTOGRAM(type());
#undef PER_DATA_TYPE_MACRO
}

void FrontendDataTypeController::RecordStartFailure(ConfigureResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeStartFailures",
                            ModelTypeToHistogramInt(type()),
                            syncer::MODEL_TYPE_COUNT);
#define PER_DATA_TYPE_MACRO(type_str)                                    \
  UMA_HISTOGRAM_ENUMERATION("Sync." type_str "ConfigureFailure", result, \
                            MAX_CONFIGURE_RESULT);
  SYNC_DATA_TYPE_HISTOGRAM(type());
#undef PER_DATA_TYPE_MACRO
}

sync_driver::AssociatorInterface* FrontendDataTypeController::model_associator()
    const {
  return model_associator_.get();
}

void FrontendDataTypeController::set_model_associator(
    sync_driver::AssociatorInterface* model_associator) {
  model_associator_.reset(model_associator);
}

sync_driver::ChangeProcessor* FrontendDataTypeController::GetChangeProcessor()
    const {
  return change_processor_.get();
}

void FrontendDataTypeController::set_change_processor(
    sync_driver::ChangeProcessor* change_processor) {
  change_processor_.reset(change_processor);
}

}  // namespace browser_sync
