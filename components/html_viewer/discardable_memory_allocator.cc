// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/discardable_memory_allocator.h"

#include "base/memory/discardable_memory.h"
#include "base/stl_util.h"

namespace html_viewer {

// Interface to the rest of the program. These objects are owned outside of the
// allocator.
class DiscardableMemoryAllocator::DiscardableMemoryChunkImpl
    : public base::DiscardableMemory {
 public:
  DiscardableMemoryChunkImpl(size_t size, DiscardableMemoryAllocator* allocator)
      : is_locked_(true),
        size_(size),
        data_(new uint8_t[size]),
        allocator_(allocator) {}

  ~DiscardableMemoryChunkImpl() override {
    // Either the memory is discarded or the memory chunk is unlocked.
    DCHECK(data_ || !is_locked_);
    if (!is_locked_ && data_)
      allocator_->NotifyDestructed(unlocked_position_);
  }

  // Overridden from DiscardableMemoryChunk:
  bool Lock() override {
    DCHECK(!is_locked_);
    if (!data_)
      return false;

    is_locked_ = true;
    allocator_->NotifyLocked(unlocked_position_);
    return true;
  }

  void Unlock() override {
    DCHECK(is_locked_);
    DCHECK(data_);
    is_locked_ = false;
    unlocked_position_ = allocator_->NotifyUnlocked(this);
  }

  void* data() const override {
    if (data_) {
      DCHECK(is_locked_);
      return data_.get();
    }
    return nullptr;
  }

  size_t size() const { return size_; }

  void Discard() {
    DCHECK(!is_locked_);
    data_.reset();
  }

 private:
  bool is_locked_;
  size_t size_;
  scoped_ptr<uint8_t[]> data_;
  DiscardableMemoryAllocator* allocator_;

  std::list<DiscardableMemoryChunkImpl*>::iterator unlocked_position_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DiscardableMemoryChunkImpl);
};

DiscardableMemoryAllocator::DiscardableMemoryAllocator(
    size_t desired_max_memory)
    : desired_max_memory_(desired_max_memory),
      total_live_memory_(0u),
      locked_chunks_(0) {
}

DiscardableMemoryAllocator::~DiscardableMemoryAllocator() {
  DCHECK_EQ(0, locked_chunks_);
  STLDeleteElements(&live_unlocked_chunks_);
}

scoped_ptr<base::DiscardableMemory>
DiscardableMemoryAllocator::AllocateLockedDiscardableMemory(size_t size) {
  base::AutoLock lock(lock_);
  scoped_ptr<DiscardableMemoryChunkImpl> chunk(
      new DiscardableMemoryChunkImpl(size, this));
  total_live_memory_ += size;
  locked_chunks_++;

  // Go through the list of unlocked live chunks starting from the least
  // recently used, freeing as many as we can until we get our size under the
  // desired maximum.
  auto it = live_unlocked_chunks_.begin();
  while (total_live_memory_ > desired_max_memory_ &&
         it != live_unlocked_chunks_.end()) {
    total_live_memory_ -= (*it)->size();
    (*it)->Discard();
    it = live_unlocked_chunks_.erase(it);
  }

  return chunk.Pass();
}

std::list<DiscardableMemoryAllocator::DiscardableMemoryChunkImpl*>::iterator
DiscardableMemoryAllocator::NotifyUnlocked(DiscardableMemoryChunkImpl* chunk) {
  base::AutoLock lock(lock_);
  locked_chunks_--;
  return live_unlocked_chunks_.insert(live_unlocked_chunks_.end(), chunk);
}

void DiscardableMemoryAllocator::NotifyLocked(
    std::list<DiscardableMemoryChunkImpl*>::iterator it) {
  base::AutoLock lock(lock_);
  locked_chunks_++;
  live_unlocked_chunks_.erase(it);
}

void DiscardableMemoryAllocator::NotifyDestructed(
    std::list<DiscardableMemoryChunkImpl*>::iterator it) {
  base::AutoLock lock(lock_);
  live_unlocked_chunks_.erase(it);
}

}  // namespace html_viewer
