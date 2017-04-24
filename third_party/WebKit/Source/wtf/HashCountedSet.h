/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef WTF_HashCountedSet_h
#define WTF_HashCountedSet_h

#include "wtf/Assertions.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"
#include "wtf/allocator/PartitionAllocator.h"

namespace WTF {

// An unordered hash set that keeps track of how many times you added an item to
// the set. The iterators have fields ->key and ->value that return the set
// members and their counts, respectively.
template <typename Value,
          typename HashFunctions = typename DefaultHash<Value>::Hash,
          typename Traits = HashTraits<Value>,
          typename Allocator = PartitionAllocator>
class HashCountedSet {
  USE_ALLOCATOR(HashCountedSet, Allocator);
  WTF_MAKE_NONCOPYABLE(HashCountedSet);

 private:
  typedef HashMap<Value,
                  unsigned,
                  HashFunctions,
                  Traits,
                  HashTraits<unsigned>,
                  Allocator>
      ImplType;

 public:
  typedef Value ValueType;
  using value_type = ValueType;
  typedef typename ImplType::iterator iterator;
  typedef typename ImplType::const_iterator const_iterator;
  typedef typename ImplType::AddResult AddResult;

  HashCountedSet() {
    static_assert(Allocator::isGarbageCollected ||
                      !IsPointerToGarbageCollectedType<Value>::value,
                  "Cannot put raw pointers to garbage-collected classes into "
                  "an off-heap HashCountedSet. Use "
                  "HeapHashCountedSet<Member<T>> instead.");
  }

  void swap(HashCountedSet& other) { m_impl.swap(other.m_impl); }

  unsigned size() const { return m_impl.size(); }
  unsigned capacity() const { return m_impl.capacity(); }
  bool isEmpty() const { return m_impl.isEmpty(); }

  // Iterators iterate over pairs of values (called key) and counts (called
  // value).
  iterator begin() { return m_impl.begin(); }
  iterator end() { return m_impl.end(); }
  const_iterator begin() const { return m_impl.begin(); }
  const_iterator end() const { return m_impl.end(); }

  iterator find(const ValueType& value) { return m_impl.find(value); }
  const_iterator find(const ValueType& value) const {
    return m_impl.find(value);
  }
  bool contains(const ValueType& value) const { return m_impl.contains(value); }
  unsigned count(const ValueType& value) const { return m_impl.at(value); }

  // Increases the count if an equal value is already present the return value
  // is a pair of an iterator to the new value's location, and a bool that is
  // true if an new entry was added.
  AddResult add(const ValueType&);

  // Generalized add(), adding the value N times.
  AddResult add(const ValueType&, unsigned);

  // Reduces the count of the value, and removes it if count goes down to
  // zero, returns true if the value is removed.
  bool remove(const ValueType& value) { return remove(find(value)); }
  bool remove(iterator);

  // Removes the value, regardless of its count.
  void removeAll(const ValueType& value) { removeAll(find(value)); }
  void removeAll(iterator);

  // Clears the whole set.
  void clear() { m_impl.clear(); }

  Vector<Value> asVector() const;

  template <typename VisitorDispatcher>
  void trace(VisitorDispatcher visitor) {
    m_impl.trace(visitor);
  }

 private:
  ImplType m_impl;
};

template <typename T, typename U, typename V, typename W>
inline typename HashCountedSet<T, U, V, W>::AddResult
HashCountedSet<T, U, V, W>::add(const ValueType& value, unsigned count) {
  DCHECK_GT(count, 0u);
  AddResult result = m_impl.insert(value, 0);
  result.storedValue->value += count;
  return result;
}

template <typename T, typename U, typename V, typename W>
inline typename HashCountedSet<T, U, V, W>::AddResult
HashCountedSet<T, U, V, W>::add(const ValueType& value) {
  return add(value, 1u);
}

template <typename T, typename U, typename V, typename W>
inline bool HashCountedSet<T, U, V, W>::remove(iterator it) {
  if (it == end())
    return false;

  unsigned oldVal = it->value;
  DCHECK(oldVal);
  unsigned newVal = oldVal - 1;
  if (newVal) {
    it->value = newVal;
    return false;
  }

  m_impl.remove(it);
  return true;
}

template <typename T, typename U, typename V, typename W>
inline void HashCountedSet<T, U, V, W>::removeAll(iterator it) {
  if (it == end())
    return;

  m_impl.remove(it);
}

template <typename Value,
          typename HashFunctions,
          typename Traits,
          typename Allocator,
          typename VectorType>
inline void copyToVector(
    const HashCountedSet<Value, HashFunctions, Traits, Allocator>& collection,
    VectorType& vector) {
  {
    // Disallow GC across resize allocation, see crbug.com/568173
    typename VectorType::GCForbiddenScope scope;
    vector.resize(collection.size());
  }

  auto it = collection.begin();
  auto end = collection.end();
  for (unsigned i = 0; it != end; ++it, ++i)
    vector[i] = (*it).key;
}

template <typename T, typename U, typename V, typename W>
inline Vector<T> HashCountedSet<T, U, V, W>::asVector() const {
  Vector<T> vector;
  copyToVector(*this, vector);
  return vector;
}

}  // namespace WTF

using WTF::HashCountedSet;

#endif  // WTF_HashCountedSet_h
