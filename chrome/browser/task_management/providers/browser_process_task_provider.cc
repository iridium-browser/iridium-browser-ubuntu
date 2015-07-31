// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/providers/browser_process_task_provider.h"

namespace task_management {

BrowserProcessTaskProvider::BrowserProcessTaskProvider()
    : browser_process_task_() {
}

BrowserProcessTaskProvider::~BrowserProcessTaskProvider() {
}

Task* BrowserProcessTaskProvider::GetTaskOfUrlRequest(int origin_pid,
                                                      int child_id,
                                                      int route_id) {
  if (origin_pid == 0 && child_id == -1)
    return &browser_process_task_;

  return nullptr;
}

void BrowserProcessTaskProvider::StartUpdating() {
  NotifyObserverTaskAdded(&browser_process_task_);
}

void BrowserProcessTaskProvider::StopUpdating() {
  // There's nothing to do here. The browser process task live as long as the
  // browser lives and when StopUpdating() is called the |observer_| has already
  // been cleared.
}

}  // namespace task_management
