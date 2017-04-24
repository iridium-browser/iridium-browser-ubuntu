// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef TerminatedArray_h
#define TerminatedArray_h

#include "wtf/Allocator.h"
#include "wtf/PtrUtil.h"
#include "wtf/VectorTraits.h"
#include "wtf/allocator/Partitions.h"
#include <memory>

namespace WTF {

// TerminatedArray<T> represents a sequence of elements of type T in which each
// element knows whether it is the last element in the sequence or not. For this
// check type T must provide isLastInArray method.
// TerminatedArray<T> can only be constructed by TerminatedArrayBuilder<T>.
template <typename T>
class TerminatedArray {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(TerminatedArray);

 public:
  // When TerminatedArray::Allocator implementations grow the backing
  // store, old is copied into the new and larger block.
  static_assert(VectorTraits<T>::canCopyWithMemcpy,
                "Array elements must be memory copyable");

  T& at(size_t index) { return reinterpret_cast<T*>(this)[index]; }
  const T& at(size_t index) const {
    return reinterpret_cast<const T*>(this)[index];
  }

  template <typename U>
  class iterator_base final {
    STACK_ALLOCATED();

   public:
    iterator_base& operator++() {
      if (m_val->isLastInArray()) {
        m_val = 0;
      } else {
        ++m_val;
      }
      return *this;
    }

    U& operator*() const { return *m_val; }

    bool operator==(const iterator_base& other) const {
      return m_val == other.m_val;
    }
    bool operator!=(const iterator_base& other) const {
      return !(*this == other);
    }

   private:
    iterator_base(U* val) : m_val(val) {}

    U* m_val;

    friend class TerminatedArray;
  };

  typedef iterator_base<T> iterator;
  typedef iterator_base<const T> const_iterator;

  iterator begin() { return iterator(reinterpret_cast<T*>(this)); }
  const_iterator begin() const {
    return const_iterator(reinterpret_cast<const T*>(this));
  }

  iterator end() { return iterator(0); }
  const_iterator end() const { return const_iterator(0); }

  size_t size() const {
    size_t count = 0;
    for (const_iterator it = begin(); it != end(); ++it)
      count++;
    return count;
  }

  // Match Allocator semantics to be able to use
  // std::unique_ptr<TerminatedArray>.
  void operator delete(void* p) { ::WTF::Partitions::fastFree(p); }

 private:
  // Allocator describes how TerminatedArrayBuilder should create new instances
  // of TerminateArray and manage their lifetimes.
  struct Allocator {
    STATIC_ONLY(Allocator);
    using PassPtr = std::unique_ptr<TerminatedArray>;
    using Ptr = std::unique_ptr<TerminatedArray>;

    static PassPtr release(Ptr& ptr) { return ptr.release(); }

    static PassPtr create(size_t capacity) {
      return WTF::wrapUnique(
          static_cast<TerminatedArray*>(WTF::Partitions::fastMalloc(
              capacity * sizeof(T), WTF_HEAP_PROFILER_TYPE_NAME(T))));
    }

    static PassPtr resize(Ptr ptr, size_t capacity) {
      return WTF::wrapUnique(static_cast<TerminatedArray*>(
          WTF::Partitions::fastRealloc(ptr.release(), capacity * sizeof(T),
                                       WTF_HEAP_PROFILER_TYPE_NAME(T))));
    }
  };

  // Prohibit construction. Allocator makes TerminatedArray instances for
  // TerminatedArrayBuilder by pointer casting.
  TerminatedArray();

  template <typename, template <typename> class>
  friend class TerminatedArrayBuilder;
};

}  // namespace WTF

using WTF::TerminatedArray;

#endif  // TerminatedArray_h
