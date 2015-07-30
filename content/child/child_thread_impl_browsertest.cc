// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/discardable_memory.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "content/child/child_discardable_shared_memory_manager.h"
#include "content/child/child_thread_impl.h"
#include "content/common/host_discardable_shared_memory_manager.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "url/gurl.h"

namespace content {

class ChildThreadImplBrowserTest : public ContentBrowserTest {
 public:
  ChildThreadImplBrowserTest()
      : child_discardable_shared_memory_manager_(nullptr) {}

  // Overridden from BrowserTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kSingleProcess);
  }
  void SetUpOnMainThread() override {
    NavigateToURL(shell(), GURL(url::kAboutBlankURL));
    PostTaskToInProcessRendererAndWait(
        base::Bind(&ChildThreadImplBrowserTest::SetUpOnChildThread, this));
  }

  ChildDiscardableSharedMemoryManager*
  child_discardable_shared_memory_manager() {
    return child_discardable_shared_memory_manager_;
  }

 private:
  void SetUpOnChildThread() {
    child_discardable_shared_memory_manager_ =
        ChildThreadImpl::current()->discardable_shared_memory_manager();
  }

  ChildDiscardableSharedMemoryManager* child_discardable_shared_memory_manager_;
};

IN_PROC_BROWSER_TEST_F(ChildThreadImplBrowserTest,
                       DISABLED_LockDiscardableMemory) {
  const size_t kSize = 1024 * 1024;  // 1MiB.

  scoped_ptr<base::DiscardableMemory> memory =
      child_discardable_shared_memory_manager()
          ->AllocateLockedDiscardableMemory(kSize);

  ASSERT_TRUE(memory);
  void* addr = memory->data();
  ASSERT_NE(nullptr, addr);

  memory->Unlock();

  // Purge all unlocked memory.
  HostDiscardableSharedMemoryManager::current()->SetMemoryLimit(0);

  // Should fail as memory should have been purged.
  EXPECT_FALSE(memory->Lock());
}

IN_PROC_BROWSER_TEST_F(ChildThreadImplBrowserTest,
                       DISABLED_DiscardableMemoryAddressSpace) {
  const size_t kLargeSize = 4 * 1024 * 1024;   // 4MiB.
  const size_t kNumberOfInstances = 1024 + 1;  // >4GiB total.

  ScopedVector<base::DiscardableMemory> instances;
  for (size_t i = 0; i < kNumberOfInstances; ++i) {
    scoped_ptr<base::DiscardableMemory> memory =
        child_discardable_shared_memory_manager()
            ->AllocateLockedDiscardableMemory(kLargeSize);
    ASSERT_TRUE(memory);
    void* addr = memory->data();
    ASSERT_NE(nullptr, addr);
    memory->Unlock();
    instances.push_back(memory.Pass());
  }
}

IN_PROC_BROWSER_TEST_F(ChildThreadImplBrowserTest,
                       DISABLED_ReleaseFreeDiscardableMemory) {
  const size_t kSize = 1024 * 1024;  // 1MiB.

  scoped_ptr<base::DiscardableMemory> memory =
      child_discardable_shared_memory_manager()
          ->AllocateLockedDiscardableMemory(kSize);

  EXPECT_TRUE(memory);
  memory.reset();

  EXPECT_GE(HostDiscardableSharedMemoryManager::current()->GetBytesAllocated(),
            kSize);

  child_discardable_shared_memory_manager()->ReleaseFreeMemory();

  // Busy wait for host memory usage to be reduced.
  base::TimeTicks end =
      base::TimeTicks::Now() + base::TimeDelta::FromSeconds(5);
  while (base::TimeTicks::Now() < end) {
    if (!HostDiscardableSharedMemoryManager::current()->GetBytesAllocated())
      break;
  }

  EXPECT_LT(base::TimeTicks::Now(), end);
}

}  // content
