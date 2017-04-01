// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CallbackStack_h
#define CallbackStack_h

#include "platform/heap/BlinkGC.h"
#include "wtf/Allocator.h"
#include "wtf/Assertions.h"
#include "wtf/Threading.h"
#include "wtf/ThreadingPrimitives.h"

namespace blink {

// The CallbackStack contains all the visitor callbacks used to trace and mark
// objects. A specific CallbackStack instance contains at most bufferSize
// elements.
// If more space is needed a new CallbackStack instance is created and chained
// together with the former instance. I.e. a logical CallbackStack can be made
// of multiple chained CallbackStack object instances.
class CallbackStack final {
  USING_FAST_MALLOC(CallbackStack);

 public:
  class Item {
    DISALLOW_NEW();

   public:
    Item() {}
    Item(void* object, VisitorCallback callback)
        : m_object(object), m_callback(callback) {}
    void* object() { return m_object; }
    VisitorCallback callback() { return m_callback; }
    void call(Visitor* visitor) { m_callback(visitor, m_object); }

   private:
    void* m_object;
    VisitorCallback m_callback;
  };

  static std::unique_ptr<CallbackStack> create();
  ~CallbackStack();

  void commit();
  void decommit();

  Item* allocateEntry();
  Item* pop();

  bool isEmpty() const;

  void invokeEphemeronCallbacks(Visitor*);

#if DCHECK_IS_ON()
  bool hasCallbackForObject(const void*);
#endif
  bool hasJustOneBlock() const;

  static const size_t kMinimalBlockSize;
  static const size_t kDefaultBlockSize = (1 << 13);

 private:
  class Block {
    USING_FAST_MALLOC(Block);

   public:
    explicit Block(Block* next);
    ~Block();

#if DCHECK_IS_ON()
    void clear();
#endif
    Block* next() const { return m_next; }
    void setNext(Block* next) { m_next = next; }

    bool isEmptyBlock() const { return m_current == &(m_buffer[0]); }

    size_t blockSize() const { return m_blockSize; }

    Item* allocateEntry() {
      if (LIKELY(m_current < m_limit))
        return m_current++;
      return nullptr;
    }

    Item* pop() {
      if (UNLIKELY(isEmptyBlock()))
        return nullptr;
      return --m_current;
    }

    void invokeEphemeronCallbacks(Visitor*);

#if DCHECK_IS_ON()
    bool hasCallbackForObject(const void*);
#endif

   private:
    size_t m_blockSize;

    Item* m_buffer;
    Item* m_limit;
    Item* m_current;
    Block* m_next;
  };

  CallbackStack();
  Item* popSlow();
  Item* allocateEntrySlow();
  void invokeOldestCallbacks(Block*, Block*, Visitor*);

  Block* m_first;
  Block* m_last;
};

class CallbackStackMemoryPool final {
  USING_FAST_MALLOC(CallbackStackMemoryPool);

 public:
  // 2048 * 8 * sizeof(Item) = 256 KB (64bit) is pre-allocated for the
  // underlying buffer of CallbackStacks.
  static const size_t kBlockSize = 2048;
  static const size_t kPooledBlockCount = 8;
  static const size_t kBlockBytes = kBlockSize * sizeof(CallbackStack::Item);

  static CallbackStackMemoryPool& instance();

  void initialize();
  void shutdown();
  CallbackStack::Item* allocate();
  void free(CallbackStack::Item*);

 private:
  Mutex m_mutex;
  int m_freeListFirst;
  int m_freeListNext[kPooledBlockCount];
  CallbackStack::Item* m_pooledMemory;
};

ALWAYS_INLINE CallbackStack::Item* CallbackStack::allocateEntry() {
  DCHECK(m_first);
  Item* item = m_first->allocateEntry();
  if (LIKELY(!!item))
    return item;

  return allocateEntrySlow();
}

ALWAYS_INLINE CallbackStack::Item* CallbackStack::pop() {
  Item* item = m_first->pop();
  if (LIKELY(!!item))
    return item;

  return popSlow();
}

}  // namespace blink

#endif  // CallbackStack_h
