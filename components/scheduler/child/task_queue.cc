// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/task_queue.h"

namespace scheduler {

bool TaskQueue::IsQueueEmpty() const {
  return GetQueueState() == QueueState::EMPTY;
}

}  // namespace scheduler
