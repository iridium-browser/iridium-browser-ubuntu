// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "components/update_client/component_patcher.h"
#include "components/update_client/component_patcher_operation.h"
#include "components/update_client/component_patcher_unittest.h"
#include "components/update_client/test_installer.h"
#include "courgette/courgette.h"
#include "courgette/third_party/bsdiff/bsdiff.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestCallback {
 public:
  TestCallback();
  virtual ~TestCallback() {}
  void Set(update_client::ComponentUnpacker::Error error, int extra_code);

  int error_;
  int extra_code_;
  bool called_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestCallback);
};

TestCallback::TestCallback() : error_(-1), extra_code_(-1), called_(false) {
}

void TestCallback::Set(update_client::ComponentUnpacker::Error error,
                       int extra_code) {
  error_ = error;
  extra_code_ = extra_code;
  called_ = true;
}

}  // namespace

namespace update_client {

namespace {

base::FilePath test_file(const char* file) {
  base::FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  return path.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("update_client")
      .AppendASCII(file);
}

}  // namespace

ComponentPatcherOperationTest::ComponentPatcherOperationTest() {
  EXPECT_TRUE(unpack_dir_.CreateUniqueTempDir());
  EXPECT_TRUE(input_dir_.CreateUniqueTempDir());
  EXPECT_TRUE(installed_dir_.CreateUniqueTempDir());
  installer_ = new ReadOnlyTestInstaller(installed_dir_.path());
  task_runner_ = base::MessageLoop::current()->task_runner();
}

ComponentPatcherOperationTest::~ComponentPatcherOperationTest() {
}

// Verify that a 'create' delta update operation works correctly.
TEST_F(ComponentPatcherOperationTest, CheckCreateOperation) {
  EXPECT_TRUE(base::CopyFile(
      test_file("binary_output.bin"),
      input_dir_.path().Append(FILE_PATH_LITERAL("binary_output.bin"))));

  std::unique_ptr<base::DictionaryValue> command_args(
      new base::DictionaryValue());
  command_args->SetString("output", "output.bin");
  command_args->SetString("sha256", binary_output_hash);
  command_args->SetString("op", "create");
  command_args->SetString("patch", "binary_output.bin");

  TestCallback callback;
  scoped_refptr<DeltaUpdateOp> op = new DeltaUpdateOpCreate();
  op->Run(command_args.get(), input_dir_.path(), unpack_dir_.path(), NULL,
          base::Bind(&TestCallback::Set, base::Unretained(&callback)),
          task_runner_);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(true, callback.called_);
  EXPECT_EQ(ComponentUnpacker::kNone, callback.error_);
  EXPECT_EQ(0, callback.extra_code_);
  EXPECT_TRUE(base::ContentsEqual(
      unpack_dir_.path().Append(FILE_PATH_LITERAL("output.bin")),
      test_file("binary_output.bin")));
}

// Verify that a 'copy' delta update operation works correctly.
TEST_F(ComponentPatcherOperationTest, CheckCopyOperation) {
  EXPECT_TRUE(base::CopyFile(
      test_file("binary_output.bin"),
      installed_dir_.path().Append(FILE_PATH_LITERAL("binary_output.bin"))));

  std::unique_ptr<base::DictionaryValue> command_args(
      new base::DictionaryValue());
  command_args->SetString("output", "output.bin");
  command_args->SetString("sha256", binary_output_hash);
  command_args->SetString("op", "copy");
  command_args->SetString("input", "binary_output.bin");

  TestCallback callback;
  scoped_refptr<DeltaUpdateOp> op = new DeltaUpdateOpCopy();
  op->Run(command_args.get(), input_dir_.path(), unpack_dir_.path(),
          installer_.get(),
          base::Bind(&TestCallback::Set, base::Unretained(&callback)),
          task_runner_);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(true, callback.called_);
  EXPECT_EQ(ComponentUnpacker::kNone, callback.error_);
  EXPECT_EQ(0, callback.extra_code_);
  EXPECT_TRUE(base::ContentsEqual(
      unpack_dir_.path().Append(FILE_PATH_LITERAL("output.bin")),
      test_file("binary_output.bin")));
}

// Verify that a 'courgette' delta update operation works correctly.
TEST_F(ComponentPatcherOperationTest, CheckCourgetteOperation) {
  EXPECT_TRUE(base::CopyFile(
      test_file("binary_input.bin"),
      installed_dir_.path().Append(FILE_PATH_LITERAL("binary_input.bin"))));
  EXPECT_TRUE(base::CopyFile(test_file("binary_courgette_patch.bin"),
                             input_dir_.path().Append(FILE_PATH_LITERAL(
                                 "binary_courgette_patch.bin"))));

  std::unique_ptr<base::DictionaryValue> command_args(
      new base::DictionaryValue());
  command_args->SetString("output", "output.bin");
  command_args->SetString("sha256", binary_output_hash);
  command_args->SetString("op", "courgette");
  command_args->SetString("input", "binary_input.bin");
  command_args->SetString("patch", "binary_courgette_patch.bin");

  TestCallback callback;
  scoped_refptr<DeltaUpdateOp> op =
      CreateDeltaUpdateOp("courgette", NULL /* out_of_process_patcher */);
  op->Run(command_args.get(), input_dir_.path(), unpack_dir_.path(),
          installer_.get(),
          base::Bind(&TestCallback::Set, base::Unretained(&callback)),
          task_runner_);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(true, callback.called_);
  EXPECT_EQ(ComponentUnpacker::kNone, callback.error_);
  EXPECT_EQ(0, callback.extra_code_);
  EXPECT_TRUE(base::ContentsEqual(
      unpack_dir_.path().Append(FILE_PATH_LITERAL("output.bin")),
      test_file("binary_output.bin")));
}

// Verify that a 'bsdiff' delta update operation works correctly.
TEST_F(ComponentPatcherOperationTest, CheckBsdiffOperation) {
  EXPECT_TRUE(base::CopyFile(
      test_file("binary_input.bin"),
      installed_dir_.path().Append(FILE_PATH_LITERAL("binary_input.bin"))));
  EXPECT_TRUE(base::CopyFile(
      test_file("binary_bsdiff_patch.bin"),
      input_dir_.path().Append(FILE_PATH_LITERAL("binary_bsdiff_patch.bin"))));

  std::unique_ptr<base::DictionaryValue> command_args(
      new base::DictionaryValue());
  command_args->SetString("output", "output.bin");
  command_args->SetString("sha256", binary_output_hash);
  command_args->SetString("op", "courgette");
  command_args->SetString("input", "binary_input.bin");
  command_args->SetString("patch", "binary_bsdiff_patch.bin");

  TestCallback callback;
  scoped_refptr<DeltaUpdateOp> op =
      CreateDeltaUpdateOp("bsdiff", NULL /* out_of_process_patcher */);
  op->Run(command_args.get(), input_dir_.path(), unpack_dir_.path(),
          installer_.get(),
          base::Bind(&TestCallback::Set, base::Unretained(&callback)),
          task_runner_);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(true, callback.called_);
  EXPECT_EQ(ComponentUnpacker::kNone, callback.error_);
  EXPECT_EQ(0, callback.extra_code_);
  EXPECT_TRUE(base::ContentsEqual(
      unpack_dir_.path().Append(FILE_PATH_LITERAL("output.bin")),
      test_file("binary_output.bin")));
}

}  // namespace update_client
