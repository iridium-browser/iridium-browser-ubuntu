// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_PUBLIC_CPP_TESTS_VIEW_MANAGER_TEST_SUITE_H_
#define COMPONENTS_VIEW_MANAGER_PUBLIC_CPP_TESTS_VIEW_MANAGER_TEST_SUITE_H_

#include "base/test/test_suite.h"
#include "third_party/mojo/src/mojo/public/cpp/system/macros.h"

namespace mojo {

class ViewManagerTestSuite : public base::TestSuite {
 public:
  ViewManagerTestSuite(int argc, char** argv);
  ~ViewManagerTestSuite() override;

 protected:
  void Initialize() override;

 private:
  MOJO_DISALLOW_COPY_AND_ASSIGN(ViewManagerTestSuite);
};

}  // namespace mojo

#endif  // COMPONENTS_VIEW_MANAGER_PUBLIC_CPP_TESTS_VIEW_MANAGER_TEST_SUITE_H_
