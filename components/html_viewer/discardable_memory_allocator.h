// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_DISCARDABLE_MEMORY_ALLOCATOR_H_
#define COMPONENTS_HTML_VIEWER_DISCARDABLE_MEMORY_ALLOCATOR_H_

#include <list>

#include "base/memory/discardable_memory_allocator.h"
#include "base/synchronization/lock.h"

namespace html_viewer {

// A discarable memory allocator which will unallocate chunks on new
// allocations.
class DiscardableMemoryAllocator : public base::DiscardableMemoryAllocator {
 public:
  explicit DiscardableMemoryAllocator(size_t desired_max_memory);
  ~DiscardableMemoryAllocator() override;

  // Overridden from DiscardableMemoryAllocator:
  scoped_ptr<base::DiscardableMemory> AllocateLockedDiscardableMemory(
      size_t size) override;

 private:
  class DiscardableMemoryChunkImpl;
  friend class DiscardableMemoryChunkImpl;

  // Called by DiscardableMemoryChunkImpl when they are unlocked. This puts them
  // at the end of the live_unlocked_chunks_ list and passes an iterator to
  // their position in the unlocked chunk list.
  std::list<DiscardableMemoryChunkImpl*>::iterator NotifyUnlocked(
      DiscardableMemoryChunkImpl* chunk);

  // Called by DiscardableMemoryChunkImpl when they are locked. This removes the
  // passed in unlocked chunk list.
  void NotifyLocked(std::list<DiscardableMemoryChunkImpl*>::iterator it);

  // Called by DiscardableMemoryChunkImpl when it's destructed. It must be
  // unlocked, by definition.
  void NotifyDestructed(std::list<DiscardableMemoryChunkImpl*>::iterator it);

  // The amount of memory we can allocate before we try to free unlocked
  // chunks. We can go over this amount if all callers keep their discardable
  // chunks locked.
  const size_t desired_max_memory_;

  // Protects all members below, since this class can be called on the main
  // thread and impl side painting raster threads.
  base::Lock lock_;

  // A count of the sum of memory. Used to trigger discarding the oldest
  // memory.
  size_t total_live_memory_;

  // The number of locked chunks.
  int locked_chunks_;

  // A linked list of unlocked allocated chunks so that the tail is most
  // recently accessed chunks.
  std::list<DiscardableMemoryChunkImpl*> live_unlocked_chunks_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemoryAllocator);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_DISCARDABLE_MEMORY_ALLOCATOR_H_
