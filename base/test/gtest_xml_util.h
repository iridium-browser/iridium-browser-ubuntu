// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_GTEST_XML_UTIL_H_
#define BASE_TEST_GTEST_XML_UTIL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class FilePath;
struct TestResult;

// Generates an XML output file. Format is very close to GTest, but has
// extensions needed by the test launcher.
class XmlUnitTestResultPrinter : public testing::EmptyTestEventListener {
 public:
  XmlUnitTestResultPrinter();
  ~XmlUnitTestResultPrinter() override;

  // Must be called before adding as a listener. Returns true on success.
  bool Initialize(const FilePath& output_file_path) WARN_UNUSED_RESULT;

 private:
  // testing::EmptyTestEventListener:
  void OnTestCaseStart(const testing::TestCase& test_case) override;
  void OnTestStart(const testing::TestInfo& test_info) override;
  void OnTestEnd(const testing::TestInfo& test_info) override;
  void OnTestCaseEnd(const testing::TestCase& test_case) override;

  FILE* output_file_;

  DISALLOW_COPY_AND_ASSIGN(XmlUnitTestResultPrinter);
};

// Produces a vector of test results based on GTest output file.
// Returns true iff the output file exists and has been successfully parsed.
// On successful return |crashed| is set to true if the test results
// are valid but incomplete.
bool ProcessGTestOutput(const base::FilePath& output_file,
                        std::vector<TestResult>* results,
                        bool* crashed) WARN_UNUSED_RESULT;

}  // namespace base

#endif  // BASE_TEST_GTEST_XML_UTIL_H_
