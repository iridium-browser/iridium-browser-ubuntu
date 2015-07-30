// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_QUADS_LIST_CONTAINER_H_
#define CC_QUADS_LIST_CONTAINER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"

namespace cc {
class DisplayItem;
class DrawQuad;
class SharedQuadState;

// This class is a container type that handles allocating contiguous memory for
// new elements and traversing through elements with either iterator or reverse
// iterator. Since this container hands out raw pointers of its elements, it is
// very important that this container never reallocate its memory so those raw
// pointer will continue to be valid.  This class is used to contain
// SharedQuadState or DrawQuad. Since the size of each DrawQuad varies, to hold
// DrawQuads, the allocations size of each element in this class is
// LargestDrawQuadSize while BaseElementType is DrawQuad.
template <class BaseElementType>
class CC_EXPORT ListContainer {
 public:
  // BaseElementType is the type of raw pointers this class hands out; however,
  // its derived classes might require different memory sizes.
  // max_size_for_derived_class the largest memory size required for all the
  // derived classes to use for allocation.
  explicit ListContainer(size_t max_size_for_derived_class);
  // This constructor omits input variable for max_size_for_derived_class. This
  // is used when there is no derived classes from BaseElementType we need to
  // worry about, and allocation size is just sizeof(BaseElementType).
  ListContainer();
  // This constructor reserves the requested memory up front so only single
  // allocation is needed. When num_of_elements_to_reserve_for is zero, use the
  // default size.
  ListContainer(size_t max_size_for_derived_class,
                size_t num_of_elements_to_reserve_for);

  ~ListContainer();

  // This class deals only with char* and void*. It does allocation and passing
  // out raw pointers, as well as memory deallocation when being destroyed.
  class CC_EXPORT ListContainerCharAllocator;

  // This class points to a certain position inside memory of
  // ListContainerCharAllocator. It is a base class for ListContainer iterators.
  struct CC_EXPORT PositionInListContainerCharAllocator {
    ListContainerCharAllocator* ptr_to_container;
    size_t vector_index;
    char* item_iterator;

    PositionInListContainerCharAllocator(
        const PositionInListContainerCharAllocator& other);

    PositionInListContainerCharAllocator(ListContainerCharAllocator* container,
                                         size_t vector_ind,
                                         char* item_iter);

    bool operator==(const PositionInListContainerCharAllocator& other) const;
    bool operator!=(const PositionInListContainerCharAllocator& other) const;

    PositionInListContainerCharAllocator Increment();
    PositionInListContainerCharAllocator ReverseIncrement();
  };

  // Iterator classes that can be used to access data.
  /////////////////////////////////////////////////////////////////
  class CC_EXPORT Iterator : public PositionInListContainerCharAllocator {
    // This class is only defined to forward iterate through
    // ListContainerCharAllocator.
   public:
    Iterator(ListContainerCharAllocator* container,
             size_t vector_ind,
             char* item_iter,
             size_t index);
    ~Iterator();
    BaseElementType* operator->() const;
    BaseElementType* operator*() const;
    Iterator operator++(int unused_post_increment);
    Iterator& operator++();

    size_t index() const;

   private:
    // This is used to track how many increment has happened since begin(). It
    // is used to avoid double increment at places an index reference is
    // needed. For iterator this means begin() corresponds to index 0 and end()
    // corresponds to index |size|.
    size_t index_;
  };

  class CC_EXPORT ConstIterator : public PositionInListContainerCharAllocator {
    // This class is only defined to forward iterate through
    // ListContainerCharAllocator.
   public:
    ConstIterator(ListContainerCharAllocator* container,
                  size_t vector_ind,
                  char* item_iter,
                  size_t index);
    ConstIterator(const Iterator& other);  // NOLINT
    ~ConstIterator();
    const BaseElementType* operator->() const;
    const BaseElementType* operator*() const;
    ConstIterator operator++(int unused_post_increment);
    ConstIterator& operator++();

    size_t index() const;

   private:
    // This is used to track how many increment has happened since begin(). It
    // is used to avoid double increment at places an index reference is
    // needed. For iterator this means begin() corresponds to index 0 and end()
    // corresponds to index |size|.
    size_t index_;
  };

  class CC_EXPORT ReverseIterator
      : public PositionInListContainerCharAllocator {
    // This class is only defined to reverse iterate through
    // ListContainerCharAllocator.
   public:
    ReverseIterator(ListContainerCharAllocator* container,
                    size_t vector_ind,
                    char* item_iter,
                    size_t index);
    ~ReverseIterator();
    BaseElementType* operator->() const;
    BaseElementType* operator*() const;
    ReverseIterator operator++(int unused_post_increment);
    ReverseIterator& operator++();

    size_t index() const;

   private:
    // This is used to track how many increment has happened since rbegin(). It
    // is used to avoid double increment at places an index reference is
    // needed. For reverse iterator this means rbegin() corresponds to index 0
    // and rend() corresponds to index |size|.
    size_t index_;
  };

  class CC_EXPORT ConstReverseIterator
      : public PositionInListContainerCharAllocator {
    // This class is only defined to reverse iterate through
    // ListContainerCharAllocator.
   public:
    ConstReverseIterator(ListContainerCharAllocator* container,
                         size_t vector_ind,
                         char* item_iter,
                         size_t index);
    ConstReverseIterator(const ReverseIterator& other);  // NOLINT
    ~ConstReverseIterator();
    const BaseElementType* operator->() const;
    const BaseElementType* operator*() const;
    ConstReverseIterator operator++(int unused_post_increment);
    ConstReverseIterator& operator++();

    size_t index() const;

   private:
    // This is used to track how many increment has happened since rbegin(). It
    // is used to avoid double increment at places an index reference is
    // needed. For reverse iterator this means rbegin() corresponds to index 0
    // and rend() corresponds to index |size|.
    size_t index_;
  };

  // When called, all raw pointers that have been handed out are no longer
  // valid. Use with caution.
  // This function does not deallocate memory.
  void EraseAndInvalidateAllPointers(Iterator position);

  ConstReverseIterator crbegin() const;
  ConstReverseIterator crend() const;
  ConstReverseIterator rbegin() const;
  ConstReverseIterator rend() const;
  ReverseIterator rbegin();
  ReverseIterator rend();
  ConstIterator cbegin() const;
  ConstIterator cend() const;
  ConstIterator begin() const;
  ConstIterator end() const;
  Iterator begin();
  Iterator end();

  // TODO(weiliangc): front(), back() and ElementAt() function should return
  // reference, consistent with container-of-object.
  BaseElementType* front();
  BaseElementType* back();
  const BaseElementType* front() const;
  const BaseElementType* back() const;

  BaseElementType* ElementAt(size_t index);
  const BaseElementType* ElementAt(size_t index) const;

  // Take in derived element type and construct it at location generated by
  // Allocate().
  template <typename DerivedElementType>
  DerivedElementType* AllocateAndConstruct() {
    return new (Allocate(sizeof(DerivedElementType))) DerivedElementType;
  }
  // Take in derived element type and copy construct it at location generated by
  // Allocate().
  template <typename DerivedElementType>
  DerivedElementType* AllocateAndCopyFrom(const DerivedElementType* source) {
    return new (Allocate(sizeof(DerivedElementType)))
        DerivedElementType(*source);
  }
  // Construct a new element on top of an existing one.
  template <typename DerivedElementType>
  DerivedElementType* ReplaceExistingElement(Iterator at) {
    at->~BaseElementType();
    return new (*at) DerivedElementType();
  }

  size_t size() const;
  bool empty() const;
  void clear();

  size_t AvailableSizeWithoutAnotherAllocationForTesting() const;

 private:
  // Hands out memory location for an element at the end of data structure.
  void* Allocate(size_t size_of_actual_element_in_bytes);

  scoped_ptr<ListContainerCharAllocator> data_;

  DISALLOW_COPY_AND_ASSIGN(ListContainer);
};

#if !defined(COMPILER_MSVC)
extern template class ListContainer<SharedQuadState>;
extern template class ListContainer<DrawQuad>;
extern template class ListContainer<DisplayItem>;
#endif
}  // namespace cc

#endif  // CC_QUADS_LIST_CONTAINER_H_
