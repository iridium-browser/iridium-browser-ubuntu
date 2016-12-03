// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_MODEL_SAFE_WORKER_H_
#define COMPONENTS_SYNC_ENGINE_MODEL_SAFE_WORKER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/syncer_error.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace syncer {

// TODO(akalin): Move the non-exported functions in this file to a
// private header.

typedef base::Callback<enum SyncerError(void)> WorkCallback;

enum ModelSafeGroup {
  GROUP_PASSIVE = 0,   // Models that are just "passively" being synced; e.g.
                       // changes to these models don't need to be pushed to a
                       // native model.
  GROUP_UI,            // Models that live on UI thread and are being synced.
  GROUP_DB,            // Models that live on DB thread and are being synced.
  GROUP_FILE,          // Models that live on FILE thread and are being synced.
  GROUP_HISTORY,       // Models that live on history thread and are being
                       // synced.
  GROUP_PASSWORD,      // Models that live on the password thread and are
                       // being synced.  On windows and linux, this runs on the
                       // DB thread.
  GROUP_NON_BLOCKING,  // Models that correspond to non-blocking types. These
                       // models always stay in GROUP_NON_BLOCKING; changes are
                       // forwarded to these models without ModelSafeWorker/
                       // SyncBackendRegistrar involvement.
};

std::string ModelSafeGroupToString(ModelSafeGroup group);

// WorkerLoopDestructionObserver is notified when the thread where it works
// is going to be destroyed.
class WorkerLoopDestructionObserver {
 public:
  virtual void OnWorkerLoopDestroyed(ModelSafeGroup group) = 0;
};

// The Syncer uses a ModelSafeWorker for all tasks that could potentially
// modify syncable entries (e.g under a WriteTransaction). The ModelSafeWorker
// only knows how to do one thing, and that is take some work (in a fully
// pre-bound callback) and have it performed (as in Run()) from a thread which
// is guaranteed to be "model-safe", where "safe" refers to not allowing us to
// cause an embedding application model to fall out of sync with the
// syncable::Directory due to a race. Each ModelSafeWorker is affiliated with
// a thread and does actual work on that thread. On the destruction of that
// thread, the affiliated worker is effectively disabled to do more
// work and will notify its observer.
class ModelSafeWorker : public base::RefCountedThreadSafe<ModelSafeWorker>,
                        public base::MessageLoop::DestructionObserver {
 public:
  // Subclass should implement to observe destruction of the loop where
  // it actually does work. Called on UI thread immediately after worker is
  // created.
  virtual void RegisterForLoopDestruction() = 0;

  // Called on sync loop from SyncBackendRegistrar::ShutDown(). Post task to
  // working loop to stop observing loop destruction and invoke
  // |unregister_done_callback|.
  virtual void UnregisterForLoopDestruction(
      base::Callback<void(ModelSafeGroup)> unregister_done_callback);

  // If not stopped, call DoWorkAndWaitUntilDoneImpl() to do work. Otherwise
  // return CANNOT_DO_WORK.
  SyncerError DoWorkAndWaitUntilDone(const WorkCallback& work);

  // Soft stop worker by setting stopped_ flag. Called when sync is disabled
  // or browser is shutting down. Called on UI loop.
  virtual void RequestStop();

  virtual ModelSafeGroup GetModelSafeGroup() = 0;

  // MessageLoop::DestructionObserver implementation.
  void WillDestroyCurrentMessageLoop() override;

 protected:
  explicit ModelSafeWorker(WorkerLoopDestructionObserver* observer);
  ~ModelSafeWorker() override;

  // Any time the Syncer performs model modifications (e.g employing a
  // WriteTransaction), it should be done by this method to ensure it is done
  // from a model-safe thread.
  virtual SyncerError DoWorkAndWaitUntilDoneImpl(const WorkCallback& work) = 0;

  base::WaitableEvent* work_done_or_stopped() { return &work_done_or_stopped_; }

  // Return true if the worker was stopped. Thread safe.
  bool IsStopped();

  // Subclass should call this in RegisterForLoopDestruction() from the loop
  // where work is done.
  void SetWorkingLoopToCurrent();

 private:
  friend class base::RefCountedThreadSafe<ModelSafeWorker>;

  void UnregisterForLoopDestructionAsync(
      base::Callback<void(ModelSafeGroup)> unregister_done_callback);

  // Whether the worker should/can do more work. Set when sync is disabled or
  // when the worker's working thread is to be destroyed.
  base::Lock stopped_lock_;
  bool stopped_;

  // Signal set when work on native thread is finished or when native thread
  // is to be destroyed so no more work can be done.
  base::WaitableEvent work_done_or_stopped_;

  // Notified when working thread of the worker is to be destroyed.
  WorkerLoopDestructionObserver* observer_;

  // Remember working loop for posting task to unregister destruction
  // observation from sync thread when shutting down sync.
  base::Lock working_task_runner_lock_;
  scoped_refptr<base::SingleThreadTaskRunner> working_task_runner_;

  // Callback passed with UnregisterForLoopDestruction. Normally this
  // remains unset/unused and is stored only if |working_task_runner_| isn't
  // initialized by the time UnregisterForLoopDestruction is called.
  // It is safe to copy and thread safe.
  // See comments in model_safe_worker.cc for more details.
  base::Callback<void(ModelSafeGroup)> unregister_done_callback_;
};

// A map that details which ModelSafeGroup each ModelType
// belongs to.  Routing info can change in response to the user enabling /
// disabling sync for certain types, as well as model association completions.
typedef std::map<ModelType, ModelSafeGroup> ModelSafeRoutingInfo;

// Caller takes ownership of return value.
std::unique_ptr<base::DictionaryValue> ModelSafeRoutingInfoToValue(
    const ModelSafeRoutingInfo& routing_info);

std::string ModelSafeRoutingInfoToString(
    const ModelSafeRoutingInfo& routing_info);

ModelTypeSet GetRoutingInfoTypes(const ModelSafeRoutingInfo& routing_info);

ModelSafeGroup GetGroupForModelType(const ModelType type,
                                    const ModelSafeRoutingInfo& routes);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_MODEL_SAFE_WORKER_H_
