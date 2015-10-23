// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/worker_thread_ticker.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"

WorkerThreadTicker::WorkerThreadTicker(int tick_interval)
    : timer_thread_("worker_thread_ticker"),
      is_running_(false),
      tick_interval_(base::TimeDelta::FromMilliseconds(tick_interval)) {
}

WorkerThreadTicker::~WorkerThreadTicker() {
  Stop();
}

bool WorkerThreadTicker::RegisterTickHandler(Callback *tick_handler) {
  DCHECK(tick_handler);
  base::AutoLock lock(lock_);
  // You cannot change the list of handlers when the timer is running.
  // You need to call Stop first.
  if (IsRunning())
    return false;
  tick_handler_list_.push_back(tick_handler);
  return true;
}

bool WorkerThreadTicker::UnregisterTickHandler(Callback *tick_handler) {
  DCHECK(tick_handler);
  base::AutoLock lock(lock_);
  // You cannot change the list of handlers when the timer is running.
  // You need to call Stop first.
  if (IsRunning()) {
    return false;
  }
  TickHandlerListType::iterator index = std::remove(tick_handler_list_.begin(),
                                                    tick_handler_list_.end(),
                                                    tick_handler);
  if (index == tick_handler_list_.end()) {
    return false;
  }
  tick_handler_list_.erase(index, tick_handler_list_.end());
  return true;
}

bool WorkerThreadTicker::Start() {
  // Do this in a lock because we don't want 2 threads to
  // call Start at the same time
  base::AutoLock lock(lock_);
  if (IsRunning())
    return false;
  if (!timer_thread_.Start())
    return false;
  is_running_ = true;
  ScheduleTimerTask();
  return true;
}

bool WorkerThreadTicker::Stop() {
  // Do this in a lock because we don't want 2 threads to
  // call Stop at the same time
  base::AutoLock lock(lock_);
  if (!IsRunning())
    return false;
  is_running_ = false;
  timer_thread_.Stop();
  return true;
}

void WorkerThreadTicker::ScheduleTimerTask() {
  timer_thread_.task_runner()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&WorkerThreadTicker::TimerTask, base::Unretained(this)),
      tick_interval_);
}

void WorkerThreadTicker::TimerTask() {
  // When the ticker is running, the handler list CANNOT be modified.
  // So we can do the enumeration safely without a lock
  const TickHandlerListType& handlers = tick_handler_list_;
  for (TickHandlerListType::const_iterator i = handlers.begin();
       i != handlers.end(); ++i) {
    (*i)->OnTick();
  }

  ScheduleTimerTask();
}

