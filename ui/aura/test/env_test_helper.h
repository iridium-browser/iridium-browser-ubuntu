// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_ENV_TEST_HELPER_H_
#define UI_AURA_TEST_ENV_TEST_HELPER_H_

#include <utility>

#include "base/macros.h"
#include "ui/aura/env.h"
#include "ui/aura/input_state_lookup.h"

namespace aura {
namespace test {

class EnvTestHelper {
 public:
  explicit EnvTestHelper(Env* env) : env_(env) {}
  ~EnvTestHelper() {}

  void SetInputStateLookup(
      std::unique_ptr<InputStateLookup> input_state_lookup) {
    env_->input_state_lookup_ = std::move(input_state_lookup);
  }

  void ResetEventState() {
    env_->mouse_button_flags_ = 0;
    env_->is_touch_down_ = false;
  }

 private:
  Env* env_;

  DISALLOW_COPY_AND_ASSIGN(EnvTestHelper);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_ENV_TEST_HELPER_H_
