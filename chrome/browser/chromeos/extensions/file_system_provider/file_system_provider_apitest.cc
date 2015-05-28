// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

namespace extensions {

class FileSystemProviderApiTest : public ExtensionApiTest {
 public:
  FileSystemProviderApiTest() {}

  // Loads a helper testing extension.
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    const extensions::Extension* extension = LoadExtensionWithFlags(
        test_data_dir_.AppendASCII("file_system_provider/test_util"),
        kFlagEnableIncognito);
    ASSERT_TRUE(extension);
  }
};

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Mount) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/mount",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Unmount) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/unmount",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, GetAll) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/get_all",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, GetMetadata) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/get_metadata",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, ReadDirectory) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/read_directory",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, ReadFile) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/read_file",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, BigFile) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/big_file",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Evil) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/evil",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, MimeType) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/mime_type",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, CreateDirectory) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags(
      "file_system_provider/create_directory", kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, DeleteEntry) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/delete_entry",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, CreateFile) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/create_file",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, CopyEntry) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/copy_entry",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, MoveEntry) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/move_entry",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Truncate) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/truncate",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, WriteFile) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/write_file",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Extension) {
  ASSERT_TRUE(RunComponentExtensionTest("file_system_provider/extension"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Thumbnail) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/thumbnail",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, AddWatcher) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/add_watcher",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, RemoveWatcher) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/remove_watcher",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Notify) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/notify",
                                          kFlagLoadAsComponent))
      << message_;
}

// TODO(mtomasz): Add a test for Notify() once it's wired to
// chrome.fileManagerPrivate.

}  // namespace extensions
