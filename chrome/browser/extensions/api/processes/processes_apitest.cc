// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"

// Test is flaky: http://crbug.com/346990
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_Processes) {
  ASSERT_TRUE(RunExtensionTest("processes/api")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ProcessesVsTaskManager) {
  // This test is for the old implementation of the task manager. We must
  // explicitly disable the new one.
  task_manager::browsertest_util::EnableOldTaskManager();

  // Ensure task manager is not yet updating
  TaskManagerModel* model = TaskManager::GetInstance()->model();
  EXPECT_EQ(0, model->update_requests_);
  EXPECT_EQ(TaskManagerModel::IDLE, model->update_state_);

  // Load extension that adds listener in background page
  ExtensionTestMessageListener listener("ready", false);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("processes").AppendASCII("onupdated")));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Ensure the task manager has started updating
  EXPECT_EQ(1, model->update_requests_);
  EXPECT_EQ(TaskManagerModel::TASK_PENDING, model->update_state_);

  // Now show the task manager and wait for it to be ready
  chrome::ShowTaskManager(browser());

  EXPECT_EQ(2, model->update_requests_);
  EXPECT_EQ(TaskManagerModel::TASK_PENDING, model->update_state_);

  // Unload the extension and check that listener count decreases
  UnloadExtension(last_loaded_extension_id());
  EXPECT_EQ(1, model->update_requests_);
}
