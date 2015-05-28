// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_TEST_WITH_SCOPE_H_
#define TOOLS_GN_TEST_WITH_SCOPE_H_

#include <vector>

#include "base/basictypes.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/err.h"
#include "tools/gn/input_file.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scope.h"
#include "tools/gn/settings.h"
#include "tools/gn/token.h"
#include "tools/gn/toolchain.h"
#include "tools/gn/value.h"

// A helper class for setting up a Scope that a test can use. It makes a
// toolchain and sets up all the build state.
class TestWithScope {
 public:
  TestWithScope();
  ~TestWithScope();

  BuildSettings* build_settings() { return &build_settings_; }
  Settings* settings() { return &settings_; }
  Toolchain* toolchain() { return &toolchain_; }
  Scope* scope() { return &scope_; }

  // This buffer accumulates output from any print() commands executed in the
  // context of this test. Note that the implementation of this is not
  // threadsafe so don't write tests that call print from multiple threads.
  std::string& print_output() { return print_output_; }

  // Fills in the tools for the given toolchain with reasonable default values.
  // The toolchain in this object will be automatically set up with this
  // function, it is exposed to allow tests to get the same functionality for
  // other toolchains they make
  static void SetupToolchain(Toolchain* toolchain);

 private:
  void AppendPrintOutput(const std::string& str);

  BuildSettings build_settings_;
  Settings settings_;
  Toolchain toolchain_;
  Scope scope_;

  std::string print_output_;

  DISALLOW_COPY_AND_ASSIGN(TestWithScope);
};

// Helper class to treat some string input as a file.
//
// Instantiate it with the contents you want, be sure to check for error, and
// then you can execute the ParseNode or whatever.
class TestParseInput {
 public:
  explicit TestParseInput(const std::string& input);
  ~TestParseInput();

  // Indicates whether and what error occurred during tokenizing and parsing.
  bool has_error() const { return parse_err_.has_error(); }
  const Err& parse_err() const { return parse_err_; }

  const InputFile& input_file() const { return input_file_; }
  const std::vector<Token>& tokens() const { return tokens_; }
  const ParseNode* parsed() const { return parsed_.get(); }

 private:
  InputFile input_file_;

  std::vector<Token> tokens_;
  scoped_ptr<ParseNode> parsed_;

  Err parse_err_;

  DISALLOW_COPY_AND_ASSIGN(TestParseInput);
};

#endif  // TOOLS_GN_TEST_WITH_SCOPE_H_
