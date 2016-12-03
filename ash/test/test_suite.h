// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_SUITE_H_
#define ASH_TEST_TEST_SUITE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "base/test/test_suite.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace aura {
class Env;
}

namespace ash {
namespace test {

class AuraShellTestSuite : public base::TestSuite {
 public:
  AuraShellTestSuite(int argc, char** argv);
  ~AuraShellTestSuite() override;

 protected:
  // base::TestSuite:
  void Initialize() override;
  void Shutdown() override;

 private:
#if defined(OS_WIN)
  std::unique_ptr<base::win::ScopedCOMInitializer> com_initializer_;
#endif
  std::unique_ptr<aura::Env> env_;

  base::TestDiscardableMemoryAllocator discardable_memory_allocator_;
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_SUITE_H_
