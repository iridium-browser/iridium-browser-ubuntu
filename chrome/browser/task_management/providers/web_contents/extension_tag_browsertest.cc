// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/task_management/task_management_browsertest_util.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/browser/test_image_loader.h"
#include "extensions/common/constants.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/skia_util.h"

namespace task_management {

class ExtensionTagsTest : public ExtensionBrowserTest {
 public:
  ExtensionTagsTest() {}
  ~ExtensionTagsTest() override {}

 protected:
  // ExtensionBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);

    // Do not launch device discovery process.
    command_line->AppendSwitch(switches::kDisableDeviceDiscoveryNotifications);
  }

  const std::vector<WebContentsTag*>& tracked_tags() const {
    return WebContentsTagsManager::GetInstance()->tracked_tags();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionTagsTest);
};

// Tests loading, disabling, enabling and unloading extensions and how that will
// affect the recording of tags.
IN_PROC_BROWSER_TEST_F(ExtensionTagsTest, Basic) {
  // Browser tests start with a single tab.
  EXPECT_EQ(1U, tracked_tags().size());

  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
          .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
          .AppendASCII("1.0.0.0"));
  ASSERT_TRUE(extension);

  EXPECT_EQ(2U, tracked_tags().size());

  DisableExtension(extension->id());
  EXPECT_EQ(1U, tracked_tags().size());

  EnableExtension(extension->id());
  EXPECT_EQ(2U, tracked_tags().size());

  UnloadExtension(extension->id());
  EXPECT_EQ(1U, tracked_tags().size());
}

#if defined(OS_WIN)
// Test disabled due to flakiness on Windows XP.
// See bug: http://crbug.com/519333
#define MAYBE_PreAndPostExistingTaskProviding \
    DISABLED_PreAndPostExistingTaskProviding
#else
#define MAYBE_PreAndPostExistingTaskProviding PreAndPostExistingTaskProviding
#endif

IN_PROC_BROWSER_TEST_F(ExtensionTagsTest,
                       MAYBE_PreAndPostExistingTaskProviding) {
  // Browser tests start with a single tab.
  EXPECT_EQ(1U, tracked_tags().size());
  MockWebContentsTaskManager task_manager;
  EXPECT_TRUE(task_manager.tasks().empty());

  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
          .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
          .AppendASCII("1.0.0.0"));
  ASSERT_TRUE(extension);

  EXPECT_EQ(2U, tracked_tags().size());
  EXPECT_TRUE(task_manager.tasks().empty());

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  // Start observing, pre-existing tasks will be provided.
  task_manager.StartObserving();
  ASSERT_EQ(2U, task_manager.tasks().size());
  const Task* extension_task = task_manager.tasks().back();
  EXPECT_EQ(Task::EXTENSION, extension_task->GetType());

  SkBitmap expected_bitmap =
      extensions::TestImageLoader::LoadAndGetExtensionBitmap(
          extension,
          "icon_128.png",
          extension_misc::EXTENSION_ICON_SMALL);
  ASSERT_FALSE(expected_bitmap.empty());

  EXPECT_TRUE(gfx::BitmapsAreEqual(*extension_task->icon().bitmap(),
                                   expected_bitmap));

  // Unload the extension and expect that the task manager now shows only the
  // about:blank tab.
  UnloadExtension(extension->id());
  EXPECT_EQ(1U, tracked_tags().size());
  ASSERT_EQ(1U, task_manager.tasks().size());
  const Task* about_blank_task = task_manager.tasks().back();
  EXPECT_EQ(Task::RENDERER, about_blank_task->GetType());
  EXPECT_EQ(base::UTF8ToUTF16("Tab: about:blank"), about_blank_task->title());

  // Reload the extension, the task manager should show it again.
  ReloadExtension(extension->id());
  EXPECT_EQ(2U, tracked_tags().size());
  EXPECT_EQ(2U, task_manager.tasks().size());
  EXPECT_EQ(Task::EXTENSION, task_manager.tasks().back()->GetType());
}

}  // namespace task_management

