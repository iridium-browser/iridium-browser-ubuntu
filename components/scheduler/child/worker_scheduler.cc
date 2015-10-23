// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/worker_scheduler.h"

#include "base/message_loop/message_loop.h"
#include "components/scheduler/child/scheduler_task_runner_delegate.h"
#include "components/scheduler/child/worker_scheduler_impl.h"

namespace scheduler {

WorkerScheduler::WorkerScheduler() {
}

WorkerScheduler::~WorkerScheduler() {
}

// static
scoped_ptr<WorkerScheduler> WorkerScheduler::Create(
    scoped_refptr<SchedulerTaskRunnerDelegate> main_task_runner) {
  return make_scoped_ptr(new WorkerSchedulerImpl(main_task_runner.Pass()));
}

}  // namespace scheduler
