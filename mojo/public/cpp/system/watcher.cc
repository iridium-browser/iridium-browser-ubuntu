// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/watcher.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/trace_event/heap_profiler.h"
#include "mojo/public/c/system/functions.h"

namespace mojo {

Watcher::Watcher(const tracked_objects::Location& from_here,
                 scoped_refptr<base::SingleThreadTaskRunner> runner)
    : task_runner_(std::move(runner)),
      is_default_task_runner_(task_runner_ ==
                              base::ThreadTaskRunnerHandle::Get()),
      heap_profiler_tag_(from_here.file_name()),
      weak_factory_(this) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  weak_self_ = weak_factory_.GetWeakPtr();
}

Watcher::~Watcher() {
  if(IsWatching())
    Cancel();
}

bool Watcher::IsWatching() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return handle_.is_valid();
}

MojoResult Watcher::Start(Handle handle,
                          MojoHandleSignals signals,
                          const ReadyCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!IsWatching());
  DCHECK(!callback.is_null());

  callback_ = callback;
  handle_ = handle;
  MojoResult result = MojoWatch(handle_.value(), signals,
                                &Watcher::CallOnHandleReady,
                                reinterpret_cast<uintptr_t>(this));
  if (result != MOJO_RESULT_OK) {
    handle_.set_value(kInvalidHandleValue);
    callback_.Reset();
    DCHECK(result == MOJO_RESULT_FAILED_PRECONDITION ||
           result == MOJO_RESULT_INVALID_ARGUMENT);
    return result;
  }

  return MOJO_RESULT_OK;
}

void Watcher::Cancel() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // The watch may have already been cancelled if the handle was closed.
  if (!handle_.is_valid())
    return;

  MojoResult result =
      MojoCancelWatch(handle_.value(), reinterpret_cast<uintptr_t>(this));
  // |result| may be MOJO_RESULT_INVALID_ARGUMENT if |handle_| has closed, but
  // OnHandleReady has not yet been called.
  DCHECK(result == MOJO_RESULT_INVALID_ARGUMENT || result == MOJO_RESULT_OK);
  handle_.set_value(kInvalidHandleValue);
  callback_.Reset();
}

void Watcher::OnHandleReady(MojoResult result) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ReadyCallback callback = callback_;
  if (result == MOJO_RESULT_CANCELLED) {
    handle_.set_value(kInvalidHandleValue);
    callback_.Reset();
  }

  // NOTE: It's legal for |callback| to delete |this|.
  if (!callback.is_null()) {
    TRACE_HEAP_PROFILER_API_SCOPED_TASK_EXECUTION event(heap_profiler_tag_);
    callback.Run(result);
  }
}

// static
void Watcher::CallOnHandleReady(uintptr_t context,
                                MojoResult result,
                                MojoHandleSignalsState signals_state,
                                MojoWatchNotificationFlags flags) {
  // NOTE: It is safe to assume the Watcher still exists because this callback
  // will never be run after the Watcher's destructor.
  //
  // TODO: Maybe we should also expose |signals_state| through the Watcher API.
  // Current HandleWatcher users have no need for it, so it's omitted here.
  Watcher* watcher = reinterpret_cast<Watcher*>(context);

  if ((flags & MOJO_WATCH_NOTIFICATION_FLAG_FROM_SYSTEM) &&
      watcher->task_runner_->RunsTasksOnCurrentThread() &&
      watcher->is_default_task_runner_) {
    // System notifications will trigger from the task runner passed to
    // mojo::edk::InitIPCSupport(). In Chrome this happens to always be the
    // default task runner for the IO thread.
    watcher->OnHandleReady(result);
  } else {
    watcher->task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&Watcher::OnHandleReady, watcher->weak_self_, result));
  }
}

}  // namespace mojo
