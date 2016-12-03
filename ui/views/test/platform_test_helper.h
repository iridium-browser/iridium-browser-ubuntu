// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_PLATFORM_TEST_HELPER_H_
#define UI_VIEWS_TEST_PLATFORM_TEST_HELPER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"

namespace views {

class PlatformTestHelper {
 public:
  using Factory = base::Callback<std::unique_ptr<PlatformTestHelper>(void)>;
  PlatformTestHelper() {}
  virtual ~PlatformTestHelper() {}

  static void set_factory(const Factory& factory);
  static std::unique_ptr<PlatformTestHelper> Create();

  // Whether we are running under the mus environment. Methods are static so
  // that they can be called before Create().
  static void SetIsMus();
  static bool IsMus();

 private:
  DISALLOW_COPY_AND_ASSIGN(PlatformTestHelper);
};

}  // namespace views

#endif  // UI_VIEWS_TEST_PLATFORM_TEST_HELPER_H_
