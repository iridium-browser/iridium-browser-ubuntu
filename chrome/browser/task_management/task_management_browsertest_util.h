// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_TASK_MANAGEMENT_BROWSERTEST_UTIL_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_TASK_MANAGEMENT_BROWSERTEST_UTIL_H_

#include "chrome/browser/task_management/providers/task_provider_observer.h"
#include "chrome/browser/task_management/providers/web_contents/web_contents_tags_manager.h"
#include "chrome/browser/task_management/providers/web_contents/web_contents_task_provider.h"

namespace task_management {

// Defines a test class that will act as a task manager that is designed to
// only observe the WebContents-based tasks.
class MockWebContentsTaskManager : public TaskProviderObserver {
 public:
  MockWebContentsTaskManager();
  ~MockWebContentsTaskManager() override;

  // task_management::TaskProviderObserver:
  void TaskAdded(Task* task) override;
  void TaskRemoved(Task* task) override;

  // Start / Stop observing the WebContentsTaskProvider.
  void StartObserving();
  void StopObserving();

  const std::vector<Task*>& tasks() const { return tasks_; }

 private:
  std::vector<Task*> tasks_;
  WebContentsTaskProvider provider_;

  DISALLOW_COPY_AND_ASSIGN(MockWebContentsTaskManager);
};

}  // namespace task_management

#endif  // CHROME_BROWSER_TASK_MANAGEMENT_TASK_MANAGEMENT_BROWSERTEST_UTIL_H_
