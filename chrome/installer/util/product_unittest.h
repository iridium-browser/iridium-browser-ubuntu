// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_PRODUCT_UNITTEST_H_
#define CHROME_INSTALLER_UTIL_PRODUCT_UNITTEST_H_

#include <windows.h>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestWithTempDir : public testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;

  base::ScopedTempDir test_dir_;
};

class TestWithTempDirAndDeleteTempOverrideKeys : public TestWithTempDir {
 protected:
  void SetUp() override;
  void TearDown() override;
};

#endif  // CHROME_INSTALLER_UTIL_PRODUCT_UNITTEST_H_
