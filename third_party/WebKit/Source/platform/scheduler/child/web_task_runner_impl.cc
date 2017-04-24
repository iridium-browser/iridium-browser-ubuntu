// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/web_task_runner_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "public/platform/scheduler/base/task_queue.h"
#include "platform/scheduler/base/time_domain.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {
namespace scheduler {

RefPtr<WebTaskRunnerImpl> WebTaskRunnerImpl::create(
    scoped_refptr<TaskQueue> task_queue) {
  return adoptRef(new WebTaskRunnerImpl(std::move(task_queue)));
}

void WebTaskRunnerImpl::postDelayedTask(const WebTraceLocation& location,
                                        const base::Closure& task,
                                        double delayMs) {
  DCHECK_GE(delayMs, 0.0) << location.function_name() << " "
                          << location.file_name();
  task_queue_->PostDelayedTask(location, task,
                               base::TimeDelta::FromMillisecondsD(delayMs));
}

bool WebTaskRunnerImpl::runsTasksOnCurrentThread() {
  return task_queue_->RunsTasksOnCurrentThread();
}

double WebTaskRunnerImpl::virtualTimeSeconds() const {
  return (Now() - base::TimeTicks::UnixEpoch()).InSecondsF();
}

double WebTaskRunnerImpl::monotonicallyIncreasingVirtualTimeSeconds() const {
  return Now().ToInternalValue() /
         static_cast<double>(base::Time::kMicrosecondsPerSecond);
}

WebTaskRunnerImpl::WebTaskRunnerImpl(scoped_refptr<TaskQueue> task_queue)
    : task_queue_(std::move(task_queue)) {}

WebTaskRunnerImpl::~WebTaskRunnerImpl() {}

base::TimeTicks WebTaskRunnerImpl::Now() const {
  TimeDomain* time_domain = task_queue_->GetTimeDomain();
  // It's possible task_queue_ has been Unregistered which can lead to a null
  // TimeDomain.  If that happens just return the current real time.
  if (!time_domain)
    return base::TimeTicks::Now();
  return time_domain->Now();
}

base::SingleThreadTaskRunner* WebTaskRunnerImpl::toSingleThreadTaskRunner() {
  return task_queue_.get();
}

}  // namespace scheduler
}  // namespace blink
