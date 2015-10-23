// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/discardable_memory_allocator.h"

#include "base/memory/discardable_memory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace html_viewer {
namespace {

const size_t kOneKilobyte = 1024;
const size_t kAlmostOneMegabyte = 1023 * kOneKilobyte;
const size_t kOneMegabyte = 1024 * kOneKilobyte;

TEST(DiscardableMemoryAllocator, Basic) {
  DiscardableMemoryAllocator allocator(kOneMegabyte);
  scoped_ptr<base::DiscardableMemory> chunk;
  // Make sure the chunk is locked when allocated. In debug mode, we will
  // dcheck.
  chunk = allocator.AllocateLockedDiscardableMemory(kOneKilobyte);
  chunk->Unlock();

  // Make sure we can lock a chunk.
  EXPECT_TRUE(chunk->Lock());
  chunk->Unlock();
}

TEST(DiscardableMemoryAllocator, DiscardChunks) {
  DiscardableMemoryAllocator allocator(kOneMegabyte);

  scoped_ptr<base::DiscardableMemory> chunk_to_remove =
      allocator.AllocateLockedDiscardableMemory(kAlmostOneMegabyte);
  chunk_to_remove->Unlock();

  // Allocating a second chunk should deallocate the first one due to memory
  // pressure, since we only have one megabyte available.
  scoped_ptr<base::DiscardableMemory> chunk_to_keep =
      allocator.AllocateLockedDiscardableMemory(kAlmostOneMegabyte);

  // Fail to get a lock because allocating the second chunk removed the first.
  EXPECT_FALSE(chunk_to_remove->Lock());

  chunk_to_keep->Unlock();
}

TEST(DiscardableMemoryAllocator, DontDiscardLiveChunks) {
  DiscardableMemoryAllocator allocator(kOneMegabyte);

  scoped_ptr<base::DiscardableMemory> chunk_one =
      allocator.AllocateLockedDiscardableMemory(kAlmostOneMegabyte);
  scoped_ptr<base::DiscardableMemory> chunk_two =
      allocator.AllocateLockedDiscardableMemory(kAlmostOneMegabyte);
  scoped_ptr<base::DiscardableMemory> chunk_three =
      allocator.AllocateLockedDiscardableMemory(kAlmostOneMegabyte);

  // These accesses will fail if the underlying weak ptr has been deallocated.
  EXPECT_NE(nullptr, chunk_one->data());
  EXPECT_NE(nullptr, chunk_two->data());
  EXPECT_NE(nullptr, chunk_three->data());

  chunk_one->Unlock();
  chunk_two->Unlock();
  chunk_three->Unlock();
}

}  // namespace
}  // namespace html_viewer
