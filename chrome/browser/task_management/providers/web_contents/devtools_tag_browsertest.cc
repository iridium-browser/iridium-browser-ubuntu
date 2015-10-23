// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/task_management/providers/web_contents/web_contents_tags_manager.h"
#include "chrome/browser/task_management/task_management_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

namespace task_management {

namespace {

const char kTestPage1[] = "files/devtools/debugger_test_page.html";
const char kTestPage2[] = "files/devtools/navigate_back.html";

}  // namespace

// Defines a browser test for testing that DevTools WebContents are being tagged
// properly by a DevToolsTag and that the TagsManager records these tags. It
// will also test that the WebContentsTaskProvider will be able to provide the
// appropriate DevToolsTask.
class DevToolsTagTest : public InProcessBrowserTest {
 public:
  DevToolsTagTest()
      : devtools_window_(nullptr) {
    CHECK(test_server()->Start());
  }

  ~DevToolsTagTest() override {}

  void LoadTestPage(const std::string& test_page) {
    GURL url = test_server()->GetURL(test_page);
    ui_test_utils::NavigateToURL(browser(), url);
  }

  void OpenDevToolsWindow(bool is_docked) {
    devtools_window_ = DevToolsWindowTesting::OpenDevToolsWindowSync(
        browser()->tab_strip_model()->GetWebContentsAt(0), is_docked);
  }

  void CloseDevToolsWindow() {
    DevToolsWindowTesting::CloseDevToolsWindowSync(devtools_window_);
  }

  WebContentsTagsManager* tags_manager() const {
    return WebContentsTagsManager::GetInstance();
  }

 private:
  DevToolsWindow* devtools_window_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsTagTest);
};

// Tests that opening a DevToolsWindow will result in tagging its main
// WebContents and that tag will be recorded by the TagsManager.
IN_PROC_BROWSER_TEST_F(DevToolsTagTest, TagsManagerRecordsATag) {
  // Browser tests start with a single tab.
  EXPECT_EQ(1U, tags_manager()->tracked_tags().size());

  // Navigating the same tab to the test page won't change the number of tracked
  // tags. No devtools yet.
  LoadTestPage(kTestPage1);
  EXPECT_EQ(1U, tags_manager()->tracked_tags().size());

  // Test both docked and undocked devtools.
  OpenDevToolsWindow(true);
  EXPECT_EQ(2U, tags_manager()->tracked_tags().size());
  CloseDevToolsWindow();
  EXPECT_EQ(1U, tags_manager()->tracked_tags().size());

  // For the undocked devtools there will be two tags one for the main contents
  // and one for the toolbox contents
  OpenDevToolsWindow(false);
  EXPECT_EQ(3U, tags_manager()->tracked_tags().size());
  CloseDevToolsWindow();
  EXPECT_EQ(1U, tags_manager()->tracked_tags().size());
}

IN_PROC_BROWSER_TEST_F(DevToolsTagTest, DevToolsTaskIsProvided) {
  MockWebContentsTaskManager task_manager;
  EXPECT_TRUE(task_manager.tasks().empty());
  // Browser tests start with a single tab.
  EXPECT_EQ(1U, tags_manager()->tracked_tags().size());

  task_manager.StartObserving();

  // The pre-existing tab is provided.
  EXPECT_EQ(1U, task_manager.tasks().size());

  LoadTestPage(kTestPage1);
  EXPECT_EQ(1U, tags_manager()->tracked_tags().size());
  EXPECT_EQ(1U, task_manager.tasks().size());

  OpenDevToolsWindow(true);
  EXPECT_EQ(2U, tags_manager()->tracked_tags().size());
  EXPECT_EQ(2U, task_manager.tasks().size());

  const Task* task = task_manager.tasks().back();
  EXPECT_EQ(Task::RENDERER, task->GetType());

  // Navigating to a new page will not change the title of the devtools main
  // WebContents.
  const base::string16 title1 = task->title();
  LoadTestPage(kTestPage2);
  const base::string16 title2 = task->title();
  EXPECT_EQ(title1, title2);

  CloseDevToolsWindow();
  EXPECT_EQ(1U, tags_manager()->tracked_tags().size());
  EXPECT_EQ(1U, task_manager.tasks().size());
}

}  // namespace task_management
