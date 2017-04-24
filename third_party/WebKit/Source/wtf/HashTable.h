/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2011, 2012 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008 David Levin <levin@chromium.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef WTF_HashTable_h
#define WTF_HashTable_h

#include "wtf/Alignment.h"
#include "wtf/Allocator.h"
#include "wtf/Assertions.h"
#include "wtf/ConditionalDestructor.h"
#include "wtf/HashTraits.h"
#include "wtf/PtrUtil.h"
#include "wtf/allocator/PartitionAllocator.h"
#include <memory>

#define DUMP_HASHTABLE_STATS 0
#define DUMP_HASHTABLE_STATS_PER_TABLE 0

#if DUMP_HASHTABLE_STATS
#include "wtf/Atomics.h"
#include "wtf/Threading.h"
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
#include "wtf/DataLog.h"
#include <type_traits>
#endif

#if DUMP_HASHTABLE_STATS
#if DUMP_HASHTABLE_STATS_PER_TABLE

#define UPDATE_PROBE_COUNTS()                                    \
  ++probeCount;                                                  \
  HashTableStats::instance().recordCollisionAtCount(probeCount); \
  ++perTableProbeCount;                                          \
  m_stats->recordCollisionAtCount(perTableProbeCount)
#define UPDATE_ACCESS_COUNTS()                              \
  atomicIncrement(&HashTableStats::instance().numAccesses); \
  int probeCount = 0;                                       \
  ++m_stats->numAccesses;                                   \
  int perTableProbeCount = 0
#else
#define UPDATE_PROBE_COUNTS() \
  ++probeCount;               \
  HashTableStats::instance().recordCollisionAtCount(probeCount)
#define UPDATE_ACCESS_COUNTS()                              \
  atomicIncrement(&HashTableStats::instance().numAccesses); \
  int probeCount = 0
#endif
#else
#if DUMP_HASHTABLE_STATS_PER_TABLE
#define UPDATE_PROBE_COUNTS() \
  ++perTableProbeCount;       \
  m_stats->recordCollisionAtCount(perTableProbeCount)
#define UPDATE_ACCESS_COUNTS() \
  ++m_stats->numAccesses;      \
  int perTableProbeCount = 0
#else
#define UPDATE_PROBE_COUNTS() \
  do {                        \
  } while (0)
#define UPDATE_ACCESS_COUNTS() \
  do {                         \
  } while (0)
#endif
#endif

namespace WTF {

// This is for tracing inside collections that have special support for weak
// pointers. The trait has a trace method which returns true if there are weak
// pointers to things that have not (yet) been marked live. Returning true
// indicates that the entry in the collection may yet be removed by weak
// handling. Default implementation for non-weak types is to use the regular
// non-weak TraceTrait. Default implementation for types with weakness is to
// call traceInCollection on the type's trait.
template <WeakHandlingFlag weakHandlingFlag,
          ShouldWeakPointersBeMarkedStrongly strongify,
          typename T,
          typename Traits>
struct TraceInCollectionTrait;

#if DUMP_HASHTABLE_STATS
struct WTF_EXPORT HashTableStats {
  HashTableStats()
      : numAccesses(0),
        numRehashes(0),
        numRemoves(0),
        numReinserts(0),
        maxCollisions(0),
        numCollisions(0),
        collisionGraph() {}

  // The following variables are all atomically incremented when modified.
  int numAccesses;
  int numRehashes;
  int numRemoves;
  int numReinserts;

  // The following variables are only modified in the recordCollisionAtCount
  // method within a mutex.
  int maxCollisions;
  int numCollisions;
  int collisionGraph[4096];

  void copy(const HashTableStats* other);
  void recordCollisionAtCount(int count);
  void dumpStats();

  static HashTableStats& instance();

  template <typename VisitorDispatcher>
  void trace(VisitorDispatcher) {}
};

#if DUMP_HASHTABLE_STATS_PER_TABLE
template <typename Allocator, bool isGCType = Allocator::isGarbageCollected>
class HashTableStatsPtr;

template <typename Allocator>
class HashTableStatsPtr<Allocator, false> final {
  STATIC_ONLY(HashTableStatsPtr);

 public:
  static std::unique_ptr<HashTableStats> create() {
    return WTF::wrapUnique(new HashTableStats);
  }

  static std::unique_ptr<HashTableStats> copy(
      const std::unique_ptr<HashTableStats>& other) {
    if (!other)
      return nullptr;
    return WTF::wrapUnique(new HashTableStats(*other));
  }

  static void swap(std::unique_ptr<HashTableStats>& stats,
                   std::unique_ptr<HashTableStats>& other) {
    stats.swap(other);
  }
};

template <typename Allocator>
class HashTableStatsPtr<Allocator, true> final {
  STATIC_ONLY(HashTableStatsPtr);

 public:
  static HashTableStats* create() {
    // Resort to manually allocating this POD on the vector
    // backing heap, as blink::GarbageCollected<> isn't in scope
    // in WTF.
    void* storage = reinterpret_cast<void*>(
        Allocator::template allocateVectorBacking<unsigned char>(
            sizeof(HashTableStats)));
    return new (storage) HashTableStats;
  }

  static HashTableStats* copy(const HashTableStats* other) {
    if (!other)
      return nullptr;
    HashTableStats* obj = create();
    obj->copy(other);
    return obj;
  }

  static void swap(HashTableStats*& stats, HashTableStats*& other) {
    std::swap(stats, other);
  }
};
#endif
#endif

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
class HashTable;
template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
class HashTableIterator;
template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
class HashTableConstIterator;
template <typename Value,
          typename HashFunctions,
          typename HashTraits,
          typename Allocator>
class LinkedHashSet;
template <WeakHandlingFlag x,
          typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y,
          typename Z>
struct WeakProcessingHashTableHelper;

typedef enum { HashItemKnownGood } HashItemKnownGoodTag;

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
class HashTableConstIterator final {
  DISALLOW_NEW();

 private:
  typedef HashTable<Key,
                    Value,
                    Extractor,
                    HashFunctions,
                    Traits,
                    KeyTraits,
                    Allocator>
      HashTableType;
  typedef HashTableIterator<Key,
                            Value,
                            Extractor,
                            HashFunctions,
                            Traits,
                            KeyTraits,
                            Allocator>
      iterator;
  typedef HashTableConstIterator<Key,
                                 Value,
                                 Extractor,
                                 HashFunctions,
                                 Traits,
                                 KeyTraits,
                                 Allocator>
      const_iterator;
  typedef Value ValueType;
  using value_type = ValueType;
  typedef typename Traits::IteratorConstGetType GetType;
  typedef const ValueType* PointerType;

  friend class HashTable<Key,
                         Value,
                         Extractor,
                         HashFunctions,
                         Traits,
                         KeyTraits,
                         Allocator>;
  friend class HashTableIterator<Key,
                                 Value,
                                 Extractor,
                                 HashFunctions,
                                 Traits,
                                 KeyTraits,
                                 Allocator>;

  void skipEmptyBuckets() {
    while (m_position != m_endPosition &&
           HashTableType::isEmptyOrDeletedBucket(*m_position))
      ++m_position;
  }

  HashTableConstIterator(PointerType position,
                         PointerType endPosition,
                         const HashTableType* container)
      : m_position(position),
        m_endPosition(endPosition)
#if DCHECK_IS_ON()
        ,
        m_container(container),
        m_containerModifications(container->modifications())
#endif
  {
    skipEmptyBuckets();
  }

  HashTableConstIterator(PointerType position,
                         PointerType endPosition,
                         const HashTableType* container,
                         HashItemKnownGoodTag)
      : m_position(position),
        m_endPosition(endPosition)
#if DCHECK_IS_ON()
        ,
        m_container(container),
        m_containerModifications(container->modifications())
#endif
  {
#if DCHECK_IS_ON()
    DCHECK_EQ(m_containerModifications, m_container->modifications());
#endif
  }

  void checkModifications() const {
#if DCHECK_IS_ON()
    // HashTable and collections that build on it do not support
    // modifications while there is an iterator in use. The exception is
    // ListHashSet, which has its own iterators that tolerate modification
    // of the underlying set.
    DCHECK_EQ(m_containerModifications, m_container->modifications());
    DCHECK(!m_container->accessForbidden());
#endif
  }

 public:
  HashTableConstIterator() {}

  GetType get() const {
    checkModifications();
    return m_position;
  }
  typename Traits::IteratorConstReferenceType operator*() const {
    return Traits::getToReferenceConstConversion(get());
  }
  GetType operator->() const { return get(); }

  const_iterator& operator++() {
    DCHECK_NE(m_position, m_endPosition);
    checkModifications();
    ++m_position;
    skipEmptyBuckets();
    return *this;
  }

  // postfix ++ intentionally omitted

  // Comparison.
  bool operator==(const const_iterator& other) const {
    return m_position == other.m_position;
  }
  bool operator!=(const const_iterator& other) const {
    return m_position != other.m_position;
  }
  bool operator==(const iterator& other) const {
    return *this == static_cast<const_iterator>(other);
  }
  bool operator!=(const iterator& other) const {
    return *this != static_cast<const_iterator>(other);
  }

  std::ostream& printTo(std::ostream& stream) const {
    if (m_position == m_endPosition)
      return stream << "iterator representing <end>";
    // TODO(tkent): Change |m_position| to |*m_position| to show the
    // pointed object. It requires a lot of new stream printer functions.
    return stream << "iterator pointing to " << m_position;
  }

 private:
  PointerType m_position;
  PointerType m_endPosition;
#if DCHECK_IS_ON()
  const HashTableType* m_container;
  int64_t m_containerModifications;
#endif
};

template <typename Key,
          typename Value,
          typename Extractor,
          typename Hash,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
std::ostream& operator<<(std::ostream& stream,
                         const HashTableConstIterator<Key,
                                                      Value,
                                                      Extractor,
                                                      Hash,
                                                      Traits,
                                                      KeyTraits,
                                                      Allocator>& iterator) {
  return iterator.printTo(stream);
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
class HashTableIterator final {
  DISALLOW_NEW();

 private:
  typedef HashTable<Key,
                    Value,
                    Extractor,
                    HashFunctions,
                    Traits,
                    KeyTraits,
                    Allocator>
      HashTableType;
  typedef HashTableIterator<Key,
                            Value,
                            Extractor,
                            HashFunctions,
                            Traits,
                            KeyTraits,
                            Allocator>
      iterator;
  typedef HashTableConstIterator<Key,
                                 Value,
                                 Extractor,
                                 HashFunctions,
                                 Traits,
                                 KeyTraits,
                                 Allocator>
      const_iterator;
  typedef Value ValueType;
  typedef typename Traits::IteratorGetType GetType;
  typedef ValueType* PointerType;

  friend class HashTable<Key,
                         Value,
                         Extractor,
                         HashFunctions,
                         Traits,
                         KeyTraits,
                         Allocator>;

  HashTableIterator(PointerType pos,
                    PointerType end,
                    const HashTableType* container)
      : m_iterator(pos, end, container) {}
  HashTableIterator(PointerType pos,
                    PointerType end,
                    const HashTableType* container,
                    HashItemKnownGoodTag tag)
      : m_iterator(pos, end, container, tag) {}

 public:
  HashTableIterator() {}

  // default copy, assignment and destructor are OK

  GetType get() const { return const_cast<GetType>(m_iterator.get()); }
  typename Traits::IteratorReferenceType operator*() const {
    return Traits::getToReferenceConversion(get());
  }
  GetType operator->() const { return get(); }

  iterator& operator++() {
    ++m_iterator;
    return *this;
  }

  // postfix ++ intentionally omitted

  // Comparison.
  bool operator==(const iterator& other) const {
    return m_iterator == other.m_iterator;
  }
  bool operator!=(const iterator& other) const {
    return m_iterator != other.m_iterator;
  }
  bool operator==(const const_iterator& other) const {
    return m_iterator == other;
  }
  bool operator!=(const const_iterator& other) const {
    return m_iterator != other;
  }

  operator const_iterator() const { return m_iterator; }
  std::ostream& printTo(std::ostream& stream) const {
    return m_iterator.printTo(stream);
  }

 private:
  const_iterator m_iterator;
};

template <typename Key,
          typename Value,
          typename Extractor,
          typename Hash,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
std::ostream& operator<<(std::ostream& stream,
                         const HashTableIterator<Key,
                                                 Value,
                                                 Extractor,
                                                 Hash,
                                                 Traits,
                                                 KeyTraits,
                                                 Allocator>& iterator) {
  return iterator.printTo(stream);
}

using std::swap;

template <typename T, typename Allocator, bool enterGCForbiddenScope>
struct Mover {
  STATIC_ONLY(Mover);
  static void move(T&& from, T& to) {
    to.~T();
    new (NotNull, &to) T(std::move(from));
  }
};

template <typename T, typename Allocator>
struct Mover<T, Allocator, true> {
  STATIC_ONLY(Mover);
  static void move(T&& from, T& to) {
    to.~T();
    Allocator::enterGCForbiddenScope();
    new (NotNull, &to) T(std::move(from));
    Allocator::leaveGCForbiddenScope();
  }
};

template <typename HashFunctions>
class IdentityHashTranslator {
  STATIC_ONLY(IdentityHashTranslator);

 public:
  template <typename T>
  static unsigned hash(const T& key) {
    return HashFunctions::hash(key);
  }
  template <typename T, typename U>
  static bool equal(const T& a, const U& b) {
    return HashFunctions::equal(a, b);
  }
  template <typename T, typename U, typename V>
  static void translate(T& location, U&&, V&& value) {
    location = std::forward<V>(value);
  }
};

template <typename HashTableType, typename ValueType>
struct HashTableAddResult final {
  STACK_ALLOCATED();
  HashTableAddResult(const HashTableType* container,
                     ValueType* storedValue,
                     bool isNewEntry)
      : storedValue(storedValue),
        isNewEntry(isNewEntry)
#if ENABLE(SECURITY_ASSERT)
        ,
        m_container(container),
        m_containerModifications(container->modifications())
#endif
  {
    ALLOW_UNUSED_LOCAL(container);
    DCHECK(container);
  }

  ValueType* storedValue;
  bool isNewEntry;

#if ENABLE(SECURITY_ASSERT)
  ~HashTableAddResult() {
    // If rehash happened before accessing storedValue, it's
    // use-after-free. Any modification may cause a rehash, so we check for
    // modifications here.

    // Rehash after accessing storedValue is harmless but will assert if the
    // AddResult destructor takes place after a modification. You may need
    // to limit the scope of the AddResult.
    SECURITY_DCHECK(m_containerModifications == m_container->modifications());
  }

 private:
  const HashTableType* m_container;
  const int64_t m_containerModifications;
#endif
};

template <typename Value, typename Extractor, typename KeyTraits>
struct HashTableHelper {
  STATIC_ONLY(HashTableHelper);
  static bool isEmptyBucket(const Value& value) {
    return isHashTraitsEmptyValue<KeyTraits>(Extractor::extract(value));
  }
  static bool isDeletedBucket(const Value& value) {
    return KeyTraits::isDeletedValue(Extractor::extract(value));
  }
  static bool isEmptyOrDeletedBucket(const Value& value) {
    return isEmptyBucket(value) || isDeletedBucket(value);
  }
};

template <typename HashTranslator,
          typename KeyTraits,
          bool safeToCompareToEmptyOrDeleted>
struct HashTableKeyChecker {
  STATIC_ONLY(HashTableKeyChecker);
  // There's no simple generic way to make this check if
  // safeToCompareToEmptyOrDeleted is false, so the check always passes.
  template <typename T>
  static bool checkKey(const T&) {
    return true;
  }
};

template <typename HashTranslator, typename KeyTraits>
struct HashTableKeyChecker<HashTranslator, KeyTraits, true> {
  STATIC_ONLY(HashTableKeyChecker);
  template <typename T>
  static bool checkKey(const T& key) {
    // FIXME : Check also equality to the deleted value.
    return !HashTranslator::equal(KeyTraits::emptyValue(), key);
  }
};

// Note: empty or deleted key values are not allowed, using them may lead to
// undefined behavior.  For pointer keys this means that null pointers are not
// allowed unless you supply custom key traits.
template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
class HashTable final
    : public ConditionalDestructor<HashTable<Key,
                                             Value,
                                             Extractor,
                                             HashFunctions,
                                             Traits,
                                             KeyTraits,
                                             Allocator>,
                                   Allocator::isGarbageCollected> {
  DISALLOW_NEW();

 public:
  typedef HashTableIterator<Key,
                            Value,
                            Extractor,
                            HashFunctions,
                            Traits,
                            KeyTraits,
                            Allocator>
      iterator;
  typedef HashTableConstIterator<Key,
                                 Value,
                                 Extractor,
                                 HashFunctions,
                                 Traits,
                                 KeyTraits,
                                 Allocator>
      const_iterator;
  typedef Traits ValueTraits;
  typedef Key KeyType;
  typedef typename KeyTraits::PeekInType KeyPeekInType;
  typedef Value ValueType;
  typedef Extractor ExtractorType;
  typedef KeyTraits KeyTraitsType;
  typedef IdentityHashTranslator<HashFunctions> IdentityTranslatorType;
  typedef HashTableAddResult<HashTable, ValueType> AddResult;

  HashTable();
  void finalize() {
    DCHECK(!Allocator::isGarbageCollected);
    if (LIKELY(!m_table))
      return;
    enterAccessForbiddenScope();
    deleteAllBucketsAndDeallocate(m_table, m_tableSize);
    leaveAccessForbiddenScope();
    m_table = nullptr;
  }

  HashTable(const HashTable&);
  HashTable(HashTable&&);
  void swap(HashTable&);
  HashTable& operator=(const HashTable&);
  HashTable& operator=(HashTable&&);

  // When the hash table is empty, just return the same iterator for end as
  // for begin.  This is more efficient because we don't have to skip all the
  // empty and deleted buckets, and iterating an empty table is a common case
  // that's worth optimizing.
  iterator begin() { return isEmpty() ? end() : makeIterator(m_table); }
  iterator end() { return makeKnownGoodIterator(m_table + m_tableSize); }
  const_iterator begin() const {
    return isEmpty() ? end() : makeConstIterator(m_table);
  }
  const_iterator end() const {
    return makeKnownGoodConstIterator(m_table + m_tableSize);
  }

  unsigned size() const {
    DCHECK(!accessForbidden());
    return m_keyCount;
  }
  unsigned capacity() const {
    DCHECK(!accessForbidden());
    return m_tableSize;
  }
  bool isEmpty() const {
    DCHECK(!accessForbidden());
    return !m_keyCount;
  }

  void reserveCapacityForSize(unsigned size);

  template <typename IncomingValueType>
  AddResult add(IncomingValueType&& value) {
    return add<IdentityTranslatorType>(Extractor::extract(value),
                                       std::forward<IncomingValueType>(value));
  }

  // A special version of add() that finds the object by hashing and comparing
  // with some other type, to avoid the cost of type conversion if the object
  // is already in the table.
  template <typename HashTranslator, typename T, typename Extra>
  AddResult add(T&& key, Extra&&);
  template <typename HashTranslator, typename T, typename Extra>
  AddResult addPassingHashCode(T&& key, Extra&&);

  iterator find(KeyPeekInType key) { return find<IdentityTranslatorType>(key); }
  const_iterator find(KeyPeekInType key) const {
    return find<IdentityTranslatorType>(key);
  }
  bool contains(KeyPeekInType key) const {
    return contains<IdentityTranslatorType>(key);
  }

  template <typename HashTranslator, typename T>
  iterator find(const T&);
  template <typename HashTranslator, typename T>
  const_iterator find(const T&) const;
  template <typename HashTranslator, typename T>
  bool contains(const T&) const;

  void remove(KeyPeekInType);
  void remove(iterator);
  void remove(const_iterator);
  void clear();

  static bool isEmptyBucket(const ValueType& value) {
    return isHashTraitsEmptyValue<KeyTraits>(Extractor::extract(value));
  }
  static bool isDeletedBucket(const ValueType& value) {
    return KeyTraits::isDeletedValue(Extractor::extract(value));
  }
  static bool isEmptyOrDeletedBucket(const ValueType& value) {
    return HashTableHelper<ValueType, Extractor,
                           KeyTraits>::isEmptyOrDeletedBucket(value);
  }

  ValueType* lookup(KeyPeekInType key) {
    return lookup<IdentityTranslatorType, KeyPeekInType>(key);
  }
  template <typename HashTranslator, typename T>
  ValueType* lookup(const T&);
  template <typename HashTranslator, typename T>
  const ValueType* lookup(const T&) const;

  template <typename VisitorDispatcher>
  void trace(VisitorDispatcher);

#if DCHECK_IS_ON()
  void enterAccessForbiddenScope() {
    DCHECK(!m_accessForbidden);
    m_accessForbidden = true;
  }
  void leaveAccessForbiddenScope() { m_accessForbidden = false; }
  bool accessForbidden() const { return m_accessForbidden; }
  int64_t modifications() const { return m_modifications; }
  void registerModification() { m_modifications++; }
  // HashTable and collections that build on it do not support modifications
  // while there is an iterator in use. The exception is ListHashSet, which
  // has its own iterators that tolerate modification of the underlying set.
  void checkModifications(int64_t mods) const {
    DCHECK_EQ(mods, m_modifications);
  }
#else
  ALWAYS_INLINE void enterAccessForbiddenScope() {}
  ALWAYS_INLINE void leaveAccessForbiddenScope() {}
  ALWAYS_INLINE bool accessForbidden() const { return false; }
  ALWAYS_INLINE int64_t modifications() const { return 0; }
  ALWAYS_INLINE void registerModification() {}
  ALWAYS_INLINE void checkModifications(int64_t mods) const {}
#endif

 private:
  static ValueType* allocateTable(unsigned size);
  static void deleteAllBucketsAndDeallocate(ValueType* table, unsigned size);

  typedef std::pair<ValueType*, bool> LookupType;
  typedef std::pair<LookupType, unsigned> FullLookupType;

  LookupType lookupForWriting(const Key& key) {
    return lookupForWriting<IdentityTranslatorType>(key);
  }
  template <typename HashTranslator, typename T>
  FullLookupType fullLookupForWriting(const T&);
  template <typename HashTranslator, typename T>
  LookupType lookupForWriting(const T&);

  void remove(ValueType*);

  bool shouldExpand() const {
    return (m_keyCount + m_deletedCount) * m_maxLoad >= m_tableSize;
  }
  bool mustRehashInPlace() const {
    return m_keyCount * m_minLoad < m_tableSize * 2;
  }
  bool shouldShrink() const {
    // isAllocationAllowed check should be at the last because it's
    // expensive.
    return m_keyCount * m_minLoad < m_tableSize &&
           m_tableSize > KeyTraits::minimumTableSize &&
           Allocator::isAllocationAllowed();
  }
  ValueType* expand(ValueType* entry = 0);
  void shrink() { rehash(m_tableSize / 2, 0); }

  ValueType* expandBuffer(unsigned newTableSize, ValueType* entry, bool&);
  ValueType* rehashTo(ValueType* newTable,
                      unsigned newTableSize,
                      ValueType* entry);
  ValueType* rehash(unsigned newTableSize, ValueType* entry);
  ValueType* reinsert(ValueType&&);

  static void initializeBucket(ValueType& bucket);
  static void deleteBucket(ValueType& bucket) {
    bucket.~ValueType();
    Traits::constructDeletedValue(bucket, Allocator::isGarbageCollected);
  }

  FullLookupType makeLookupResult(ValueType* position,
                                  bool found,
                                  unsigned hash) {
    return FullLookupType(LookupType(position, found), hash);
  }

  iterator makeIterator(ValueType* pos) {
    return iterator(pos, m_table + m_tableSize, this);
  }
  const_iterator makeConstIterator(ValueType* pos) const {
    return const_iterator(pos, m_table + m_tableSize, this);
  }
  iterator makeKnownGoodIterator(ValueType* pos) {
    return iterator(pos, m_table + m_tableSize, this, HashItemKnownGood);
  }
  const_iterator makeKnownGoodConstIterator(ValueType* pos) const {
    return const_iterator(pos, m_table + m_tableSize, this, HashItemKnownGood);
  }

  static const unsigned m_maxLoad = 2;
  static const unsigned m_minLoad = 6;

  unsigned tableSizeMask() const {
    size_t mask = m_tableSize - 1;
    DCHECK_EQ((mask & m_tableSize), 0u);
    return mask;
  }

  void setEnqueued() { m_queueFlag = true; }
  void clearEnqueued() { m_queueFlag = false; }
  bool enqueued() { return m_queueFlag; }

  ValueType* m_table;
  unsigned m_tableSize;
  unsigned m_keyCount;
#if DCHECK_IS_ON()
  unsigned m_deletedCount : 30;
  unsigned m_queueFlag : 1;
  unsigned m_accessForbidden : 1;
  unsigned m_modifications;
#else
  unsigned m_deletedCount : 31;
  unsigned m_queueFlag : 1;
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
 public:
  mutable
      typename std::conditional<Allocator::isGarbageCollected,
                                HashTableStats*,
                                std::unique_ptr<HashTableStats>>::type m_stats;
#endif

  template <WeakHandlingFlag x,
            typename T,
            typename U,
            typename V,
            typename W,
            typename X,
            typename Y,
            typename Z>
  friend struct WeakProcessingHashTableHelper;
  template <typename T, typename U, typename V, typename W>
  friend class LinkedHashSet;
};

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
inline HashTable<Key,
                 Value,
                 Extractor,
                 HashFunctions,
                 Traits,
                 KeyTraits,
                 Allocator>::HashTable()
    : m_table(nullptr),
      m_tableSize(0),
      m_keyCount(0),
      m_deletedCount(0),
      m_queueFlag(false)
#if DCHECK_IS_ON()
      ,
      m_accessForbidden(false),
      m_modifications(0)
#endif
#if DUMP_HASHTABLE_STATS_PER_TABLE
      ,
      m_stats(nullptr)
#endif
{
  static_assert(Allocator::isGarbageCollected ||
                    (!IsPointerToGarbageCollectedType<Key>::value &&
                     !IsPointerToGarbageCollectedType<Value>::value),
                "Cannot put raw pointers to garbage-collected classes into an "
                "off-heap collection.");
}

inline unsigned doubleHash(unsigned key) {
  key = ~key + (key >> 23);
  key ^= (key << 12);
  key ^= (key >> 7);
  key ^= (key << 2);
  key ^= (key >> 20);
  return key;
}

inline unsigned calculateCapacity(unsigned size) {
  for (unsigned mask = size; mask; mask >>= 1)
    size |= mask;         // 00110101010 -> 00111111111
  return (size + 1) * 2;  // 00111111111 -> 10000000000
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
void HashTable<Key,
               Value,
               Extractor,
               HashFunctions,
               Traits,
               KeyTraits,
               Allocator>::reserveCapacityForSize(unsigned newSize) {
  unsigned newCapacity = calculateCapacity(newSize);
  if (newCapacity < KeyTraits::minimumTableSize)
    newCapacity = KeyTraits::minimumTableSize;

  if (newCapacity > capacity()) {
    RELEASE_ASSERT(!static_cast<int>(
        newCapacity >>
        31));  // HashTable capacity should not overflow 32bit int.
    rehash(newCapacity, 0);
  }
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
template <typename HashTranslator, typename T>
inline Value*
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    lookup(const T& key) {
  return const_cast<Value*>(
      const_cast<const HashTable*>(this)->lookup<HashTranslator>(key));
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
template <typename HashTranslator, typename T>
inline const Value*
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    lookup(const T& key) const {
  DCHECK(!accessForbidden());
  DCHECK((HashTableKeyChecker<
          HashTranslator, KeyTraits,
          HashFunctions::safeToCompareToEmptyOrDeleted>::checkKey(key)));
  const ValueType* table = m_table;
  if (!table)
    return nullptr;

  size_t k = 0;
  size_t sizeMask = tableSizeMask();
  unsigned h = HashTranslator::hash(key);
  size_t i = h & sizeMask;

  UPDATE_ACCESS_COUNTS();

  while (1) {
    const ValueType* entry = table + i;

    if (HashFunctions::safeToCompareToEmptyOrDeleted) {
      if (HashTranslator::equal(Extractor::extract(*entry), key))
        return entry;

      if (isEmptyBucket(*entry))
        return nullptr;
    } else {
      if (isEmptyBucket(*entry))
        return nullptr;

      if (!isDeletedBucket(*entry) &&
          HashTranslator::equal(Extractor::extract(*entry), key))
        return entry;
    }
    UPDATE_PROBE_COUNTS();
    if (!k)
      k = 1 | doubleHash(h);
    i = (i + k) & sizeMask;
  }
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
template <typename HashTranslator, typename T>
inline typename HashTable<Key,
                          Value,
                          Extractor,
                          HashFunctions,
                          Traits,
                          KeyTraits,
                          Allocator>::LookupType
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    lookupForWriting(const T& key) {
  DCHECK(!accessForbidden());
  DCHECK(m_table);
  registerModification();

  ValueType* table = m_table;
  size_t k = 0;
  size_t sizeMask = tableSizeMask();
  unsigned h = HashTranslator::hash(key);
  size_t i = h & sizeMask;

  UPDATE_ACCESS_COUNTS();

  ValueType* deletedEntry = nullptr;

  while (1) {
    ValueType* entry = table + i;

    if (isEmptyBucket(*entry))
      return LookupType(deletedEntry ? deletedEntry : entry, false);

    if (HashFunctions::safeToCompareToEmptyOrDeleted) {
      if (HashTranslator::equal(Extractor::extract(*entry), key))
        return LookupType(entry, true);

      if (isDeletedBucket(*entry))
        deletedEntry = entry;
    } else {
      if (isDeletedBucket(*entry))
        deletedEntry = entry;
      else if (HashTranslator::equal(Extractor::extract(*entry), key))
        return LookupType(entry, true);
    }
    UPDATE_PROBE_COUNTS();
    if (!k)
      k = 1 | doubleHash(h);
    i = (i + k) & sizeMask;
  }
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
template <typename HashTranslator, typename T>
inline typename HashTable<Key,
                          Value,
                          Extractor,
                          HashFunctions,
                          Traits,
                          KeyTraits,
                          Allocator>::FullLookupType
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    fullLookupForWriting(const T& key) {
  DCHECK(!accessForbidden());
  DCHECK(m_table);
  registerModification();

  ValueType* table = m_table;
  size_t k = 0;
  size_t sizeMask = tableSizeMask();
  unsigned h = HashTranslator::hash(key);
  size_t i = h & sizeMask;

  UPDATE_ACCESS_COUNTS();

  ValueType* deletedEntry = nullptr;

  while (1) {
    ValueType* entry = table + i;

    if (isEmptyBucket(*entry))
      return makeLookupResult(deletedEntry ? deletedEntry : entry, false, h);

    if (HashFunctions::safeToCompareToEmptyOrDeleted) {
      if (HashTranslator::equal(Extractor::extract(*entry), key))
        return makeLookupResult(entry, true, h);

      if (isDeletedBucket(*entry))
        deletedEntry = entry;
    } else {
      if (isDeletedBucket(*entry))
        deletedEntry = entry;
      else if (HashTranslator::equal(Extractor::extract(*entry), key))
        return makeLookupResult(entry, true, h);
    }
    UPDATE_PROBE_COUNTS();
    if (!k)
      k = 1 | doubleHash(h);
    i = (i + k) & sizeMask;
  }
}

template <bool emptyValueIsZero>
struct HashTableBucketInitializer;

template <>
struct HashTableBucketInitializer<false> {
  STATIC_ONLY(HashTableBucketInitializer);
  template <typename Traits, typename Value>
  static void initialize(Value& bucket) {
    new (NotNull, &bucket) Value(Traits::emptyValue());
  }
};

template <>
struct HashTableBucketInitializer<true> {
  STATIC_ONLY(HashTableBucketInitializer);
  template <typename Traits, typename Value>
  static void initialize(Value& bucket) {
    // This initializes the bucket without copying the empty value.  That
    // makes it possible to use this with types that don't support copying.
    // The memset to 0 looks like a slow operation but is optimized by the
    // compilers.
    memset(&bucket, 0, sizeof(bucket));
  }
};

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
inline void
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    initializeBucket(ValueType& bucket) {
  HashTableBucketInitializer<Traits::emptyValueIsZero>::template initialize<
      Traits>(bucket);
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
template <typename HashTranslator, typename T, typename Extra>
typename HashTable<Key,
                   Value,
                   Extractor,
                   HashFunctions,
                   Traits,
                   KeyTraits,
                   Allocator>::AddResult
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    add(T&& key, Extra&& extra) {
  DCHECK(!accessForbidden());
  DCHECK(Allocator::isAllocationAllowed());
  if (!m_table)
    expand();

  DCHECK(m_table);

  ValueType* table = m_table;
  size_t k = 0;
  size_t sizeMask = tableSizeMask();
  unsigned h = HashTranslator::hash(key);
  size_t i = h & sizeMask;

  UPDATE_ACCESS_COUNTS();

  ValueType* deletedEntry = nullptr;
  ValueType* entry;
  while (1) {
    entry = table + i;

    if (isEmptyBucket(*entry))
      break;

    if (HashFunctions::safeToCompareToEmptyOrDeleted) {
      if (HashTranslator::equal(Extractor::extract(*entry), key))
        return AddResult(this, entry, false);

      if (isDeletedBucket(*entry))
        deletedEntry = entry;
    } else {
      if (isDeletedBucket(*entry))
        deletedEntry = entry;
      else if (HashTranslator::equal(Extractor::extract(*entry), key))
        return AddResult(this, entry, false);
    }
    UPDATE_PROBE_COUNTS();
    if (!k)
      k = 1 | doubleHash(h);
    i = (i + k) & sizeMask;
  }

  registerModification();

  if (deletedEntry) {
    // Overwrite any data left over from last use, using placement new or
    // memset.
    initializeBucket(*deletedEntry);
    entry = deletedEntry;
    --m_deletedCount;
  }

  HashTranslator::translate(*entry, std::forward<T>(key),
                            std::forward<Extra>(extra));
  DCHECK(!isEmptyOrDeletedBucket(*entry));

  ++m_keyCount;

  if (shouldExpand()) {
    entry = expand(entry);
  } else if (Traits::weakHandlingFlag == WeakHandlingInCollections &&
             shouldShrink()) {
    // When weak hash tables are processed by the garbage collector,
    // elements with no other strong references to them will have their
    // table entries cleared. But no shrinking of the backing store is
    // allowed at that time, as allocations are prohibited during that
    // GC phase.
    //
    // With that weak processing taking care of removals, explicit
    // remove()s of elements is rarely done. Which implies that the
    // weak hash table will never be checked if it can be shrunk.
    //
    // To prevent weak hash tables with very low load factors from
    // developing, we perform it when adding elements instead.
    entry = rehash(m_tableSize / 2, entry);
  }

  return AddResult(this, entry, true);
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
template <typename HashTranslator, typename T, typename Extra>
typename HashTable<Key,
                   Value,
                   Extractor,
                   HashFunctions,
                   Traits,
                   KeyTraits,
                   Allocator>::AddResult
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    addPassingHashCode(T&& key, Extra&& extra) {
  DCHECK(!accessForbidden());
  DCHECK(Allocator::isAllocationAllowed());
  if (!m_table)
    expand();

  FullLookupType lookupResult = fullLookupForWriting<HashTranslator>(key);

  ValueType* entry = lookupResult.first.first;
  bool found = lookupResult.first.second;
  unsigned h = lookupResult.second;

  if (found)
    return AddResult(this, entry, false);

  registerModification();

  if (isDeletedBucket(*entry)) {
    initializeBucket(*entry);
    --m_deletedCount;
  }

  HashTranslator::translate(*entry, std::forward<T>(key),
                            std::forward<Extra>(extra), h);
  DCHECK(!isEmptyOrDeletedBucket(*entry));

  ++m_keyCount;
  if (shouldExpand())
    entry = expand(entry);

  return AddResult(this, entry, true);
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
Value*
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    reinsert(ValueType&& entry) {
  DCHECK(m_table);
  registerModification();
  DCHECK(!lookupForWriting(Extractor::extract(entry)).second);
  DCHECK(
      !isDeletedBucket(*(lookupForWriting(Extractor::extract(entry)).first)));
#if DUMP_HASHTABLE_STATS
  atomicIncrement(&HashTableStats::instance().numReinserts);
#endif
#if DUMP_HASHTABLE_STATS_PER_TABLE
  ++m_stats->numReinserts;
#endif
  Value* newEntry = lookupForWriting(Extractor::extract(entry)).first;
  Mover<ValueType, Allocator,
        Traits::template NeedsToForbidGCOnMove<>::value>::move(std::move(entry),
                                                               *newEntry);

  return newEntry;
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
template <typename HashTranslator, typename T>
inline typename HashTable<Key,
                          Value,
                          Extractor,
                          HashFunctions,
                          Traits,
                          KeyTraits,
                          Allocator>::iterator
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    find(const T& key) {
  ValueType* entry = lookup<HashTranslator>(key);
  if (!entry)
    return end();

  return makeKnownGoodIterator(entry);
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
template <typename HashTranslator, typename T>
inline typename HashTable<Key,
                          Value,
                          Extractor,
                          HashFunctions,
                          Traits,
                          KeyTraits,
                          Allocator>::const_iterator
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    find(const T& key) const {
  ValueType* entry = const_cast<HashTable*>(this)->lookup<HashTranslator>(key);
  if (!entry)
    return end();

  return makeKnownGoodConstIterator(entry);
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
template <typename HashTranslator, typename T>
bool HashTable<Key,
               Value,
               Extractor,
               HashFunctions,
               Traits,
               KeyTraits,
               Allocator>::contains(const T& key) const {
  return const_cast<HashTable*>(this)->lookup<HashTranslator>(key);
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
void HashTable<Key,
               Value,
               Extractor,
               HashFunctions,
               Traits,
               KeyTraits,
               Allocator>::remove(ValueType* pos) {
  registerModification();
#if DUMP_HASHTABLE_STATS
  atomicIncrement(&HashTableStats::instance().numRemoves);
#endif
#if DUMP_HASHTABLE_STATS_PER_TABLE
  ++m_stats->numRemoves;
#endif

  enterAccessForbiddenScope();
  deleteBucket(*pos);
  leaveAccessForbiddenScope();
  ++m_deletedCount;
  --m_keyCount;

  if (shouldShrink())
    shrink();
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
inline void
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    remove(iterator it) {
  if (it == end())
    return;
  remove(const_cast<ValueType*>(it.m_iterator.m_position));
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
inline void
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    remove(const_iterator it) {
  if (it == end())
    return;
  remove(const_cast<ValueType*>(it.m_position));
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
inline void
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    remove(KeyPeekInType key) {
  remove(find(key));
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
Value*
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    allocateTable(unsigned size) {
  size_t allocSize = size * sizeof(ValueType);
  ValueType* result;
  // Assert that we will not use memset on things with a vtable entry.  The
  // compiler will also check this on some platforms. We would like to check
  // this on the whole value (key-value pair), but std::is_polymorphic will
  // return false for a pair of two types, even if one of the components is
  // polymorphic.
  static_assert(
      !Traits::emptyValueIsZero || !std::is_polymorphic<KeyType>::value,
      "empty value cannot be zero for things with a vtable");
  static_assert(Allocator::isGarbageCollected ||
                    ((!AllowsOnlyPlacementNew<KeyType>::value ||
                      !IsTraceable<KeyType>::value) &&
                     (!AllowsOnlyPlacementNew<ValueType>::value ||
                      !IsTraceable<ValueType>::value)),
                "Cannot put DISALLOW_NEW_EXCEPT_PLACEMENT_NEW objects that "
                "have trace methods into an off-heap HashTable");

  if (Traits::emptyValueIsZero) {
    result = Allocator::template allocateZeroedHashTableBacking<ValueType,
                                                                HashTable>(
        allocSize);
  } else {
    result = Allocator::template allocateHashTableBacking<ValueType, HashTable>(
        allocSize);
    for (unsigned i = 0; i < size; i++)
      initializeBucket(result[i]);
  }
  return result;
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
void HashTable<Key,
               Value,
               Extractor,
               HashFunctions,
               Traits,
               KeyTraits,
               Allocator>::deleteAllBucketsAndDeallocate(ValueType* table,
                                                         unsigned size) {
  if (!IsTriviallyDestructible<ValueType>::value) {
    for (unsigned i = 0; i < size; ++i) {
      // This code is called when the hash table is cleared or resized. We
      // have allocated a new backing store and we need to run the
      // destructors on the old backing store, as it is being freed. If we
      // are GCing we need to both call the destructor and mark the bucket
      // as deleted, otherwise the destructor gets called again when the
      // GC finds the backing store. With the default allocator it's
      // enough to call the destructor, since we will free the memory
      // explicitly and we won't see the memory with the bucket again.
      if (Allocator::isGarbageCollected) {
        if (!isEmptyOrDeletedBucket(table[i]))
          deleteBucket(table[i]);
      } else {
        if (!isDeletedBucket(table[i]))
          table[i].~ValueType();
      }
    }
  }
  Allocator::freeHashTableBacking(table);
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
Value*
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    expand(Value* entry) {
  unsigned newSize;
  if (!m_tableSize) {
    newSize = KeyTraits::minimumTableSize;
  } else if (mustRehashInPlace()) {
    newSize = m_tableSize;
  } else {
    newSize = m_tableSize * 2;
    RELEASE_ASSERT(newSize > m_tableSize);
  }

  return rehash(newSize, entry);
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
Value*
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    expandBuffer(unsigned newTableSize, Value* entry, bool& success) {
  success = false;
  DCHECK_LT(m_tableSize, newTableSize);
  if (!Allocator::expandHashTableBacking(m_table,
                                         newTableSize * sizeof(ValueType)))
    return nullptr;

  success = true;

  Value* newEntry = nullptr;
  unsigned oldTableSize = m_tableSize;
  ValueType* originalTable = m_table;

  ValueType* temporaryTable = allocateTable(oldTableSize);
  for (unsigned i = 0; i < oldTableSize; i++) {
    if (&m_table[i] == entry)
      newEntry = &temporaryTable[i];
    if (isEmptyOrDeletedBucket(m_table[i])) {
      DCHECK_NE(&m_table[i], entry);
      if (Traits::emptyValueIsZero) {
        memset(&temporaryTable[i], 0, sizeof(ValueType));
      } else {
        initializeBucket(temporaryTable[i]);
      }
    } else {
      Mover<ValueType, Allocator,
            Traits::template NeedsToForbidGCOnMove<>::value>::
          move(std::move(m_table[i]), temporaryTable[i]);
    }
  }
  m_table = temporaryTable;

  if (Traits::emptyValueIsZero) {
    memset(originalTable, 0, newTableSize * sizeof(ValueType));
  } else {
    for (unsigned i = 0; i < newTableSize; i++)
      initializeBucket(originalTable[i]);
  }
  newEntry = rehashTo(originalTable, newTableSize, newEntry);

  enterAccessForbiddenScope();
  deleteAllBucketsAndDeallocate(temporaryTable, oldTableSize);
  leaveAccessForbiddenScope();

  return newEntry;
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
Value*
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    rehashTo(ValueType* newTable, unsigned newTableSize, Value* entry) {
  unsigned oldTableSize = m_tableSize;
  ValueType* oldTable = m_table;

#if DUMP_HASHTABLE_STATS
  if (oldTableSize != 0)
    atomicIncrement(&HashTableStats::instance().numRehashes);
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
  if (oldTableSize != 0)
    ++m_stats->numRehashes;
#endif

  m_table = newTable;
  m_tableSize = newTableSize;

  Value* newEntry = nullptr;
  for (unsigned i = 0; i != oldTableSize; ++i) {
    if (isEmptyOrDeletedBucket(oldTable[i])) {
      DCHECK_NE(&oldTable[i], entry);
      continue;
    }
    Value* reinsertedEntry = reinsert(std::move(oldTable[i]));
    if (&oldTable[i] == entry) {
      DCHECK(!newEntry);
      newEntry = reinsertedEntry;
    }
  }

  m_deletedCount = 0;

#if DUMP_HASHTABLE_STATS_PER_TABLE
  if (!m_stats)
    m_stats = HashTableStatsPtr<Allocator>::create();
#endif

  return newEntry;
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
Value*
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    rehash(unsigned newTableSize, Value* entry) {
  unsigned oldTableSize = m_tableSize;
  ValueType* oldTable = m_table;

#if DUMP_HASHTABLE_STATS
  if (oldTableSize != 0)
    atomicIncrement(&HashTableStats::instance().numRehashes);
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
  if (oldTableSize != 0)
    ++m_stats->numRehashes;
#endif

  // The Allocator::isGarbageCollected check is not needed.  The check is just
  // a static hint for a compiler to indicate that Base::expandBuffer returns
  // false if Allocator is a PartitionAllocator.
  if (Allocator::isGarbageCollected && newTableSize > oldTableSize) {
    bool success;
    Value* newEntry = expandBuffer(newTableSize, entry, success);
    if (success)
      return newEntry;
  }

  ValueType* newTable = allocateTable(newTableSize);
  Value* newEntry = rehashTo(newTable, newTableSize, entry);

  enterAccessForbiddenScope();
  deleteAllBucketsAndDeallocate(oldTable, oldTableSize);
  leaveAccessForbiddenScope();

  return newEntry;
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
void HashTable<Key,
               Value,
               Extractor,
               HashFunctions,
               Traits,
               KeyTraits,
               Allocator>::clear() {
  registerModification();
  if (!m_table)
    return;

  enterAccessForbiddenScope();
  deleteAllBucketsAndDeallocate(m_table, m_tableSize);
  leaveAccessForbiddenScope();
  m_table = nullptr;
  m_tableSize = 0;
  m_keyCount = 0;
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    HashTable(const HashTable& other)
    : m_table(nullptr),
      m_tableSize(0),
      m_keyCount(0),
      m_deletedCount(0),
      m_queueFlag(false)
#if DCHECK_IS_ON()
      ,
      m_accessForbidden(false),
      m_modifications(0)
#endif
#if DUMP_HASHTABLE_STATS_PER_TABLE
      ,
      m_stats(HashTableStatsPtr<Allocator>::copy(other.m_stats))
#endif
{
  if (other.size())
    reserveCapacityForSize(other.size());
  // Copy the hash table the dumb way, by adding each element to the new
  // table.  It might be more efficient to copy the table slots, but it's not
  // clear that efficiency is needed.
  for (const auto& element : other)
    add(element);
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
    HashTable(HashTable&& other)
    : m_table(nullptr),
      m_tableSize(0),
      m_keyCount(0),
      m_deletedCount(0),
      m_queueFlag(false)
#if DCHECK_IS_ON()
      ,
      m_accessForbidden(false),
      m_modifications(0)
#endif
#if DUMP_HASHTABLE_STATS_PER_TABLE
      ,
      m_stats(HashTableStatsPtr<Allocator>::copy(other.m_stats))
#endif
{
  swap(other);
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
void HashTable<Key,
               Value,
               Extractor,
               HashFunctions,
               Traits,
               KeyTraits,
               Allocator>::swap(HashTable& other) {
  DCHECK(!accessForbidden());
  std::swap(m_table, other.m_table);
  std::swap(m_tableSize, other.m_tableSize);
  std::swap(m_keyCount, other.m_keyCount);
  // std::swap does not work for bit fields.
  unsigned deleted = m_deletedCount;
  m_deletedCount = other.m_deletedCount;
  other.m_deletedCount = deleted;
  DCHECK(!m_queueFlag);
  DCHECK(!other.m_queueFlag);

#if DCHECK_IS_ON()
  std::swap(m_modifications, other.m_modifications);
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
  HashTableStatsPtr<Allocator>::swap(m_stats, other.m_stats);
#endif
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>&
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
operator=(const HashTable& other) {
  HashTable tmp(other);
  swap(tmp);
  return *this;
}

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>&
HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::
operator=(HashTable&& other) {
  swap(other);
  return *this;
}

template <WeakHandlingFlag weakHandlingFlag,
          typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
struct WeakProcessingHashTableHelper;

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
struct WeakProcessingHashTableHelper<NoWeakHandlingInCollections,
                                     Key,
                                     Value,
                                     Extractor,
                                     HashFunctions,
                                     Traits,
                                     KeyTraits,
                                     Allocator> {
  STATIC_ONLY(WeakProcessingHashTableHelper);
  static void process(typename Allocator::Visitor* visitor, void* closure) {}
  static void ephemeronIteration(typename Allocator::Visitor* visitor,
                                 void* closure) {}
  static void ephemeronIterationDone(typename Allocator::Visitor* visitor,
                                     void* closure) {}
};

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
struct WeakProcessingHashTableHelper<WeakHandlingInCollections,
                                     Key,
                                     Value,
                                     Extractor,
                                     HashFunctions,
                                     Traits,
                                     KeyTraits,
                                     Allocator> {
  STATIC_ONLY(WeakProcessingHashTableHelper);

  using HashTableType = HashTable<Key,
                                  Value,
                                  Extractor,
                                  HashFunctions,
                                  Traits,
                                  KeyTraits,
                                  Allocator>;
  using ValueType = typename HashTableType::ValueType;

  // Used for purely weak and for weak-and-strong tables (ephemerons).
  static void process(typename Allocator::Visitor* visitor, void* closure) {
    HashTableType* table = reinterpret_cast<HashTableType*>(closure);
    if (!table->m_table)
      return;
    // Now perform weak processing (this is a no-op if the backing was
    // accessible through an iterator and was already marked strongly).
    for (ValueType* element = table->m_table + table->m_tableSize - 1;
         element >= table->m_table; element--) {
      if (!HashTableType::isEmptyOrDeletedBucket(*element)) {
        // At this stage calling trace can make no difference
        // (everything is already traced), but we use the return value
        // to remove things from the collection.

        // FIXME: This should be rewritten so that this can check if the
        // element is dead without calling trace, which is semantically
        // not correct to be called in weak processing stage.
        if (TraceInCollectionTrait<WeakHandlingInCollections,
                                   WeakPointersActWeak, ValueType,
                                   Traits>::trace(visitor, *element)) {
          table->registerModification();
          HashTableType::deleteBucket(*element);  // Also calls the destructor.
          table->m_deletedCount++;
          table->m_keyCount--;
          // We don't rehash the backing until the next add or delete,
          // because that would cause allocation during GC.
        }
      }
    }
  }

  // Called repeatedly for tables that have both weak and strong pointers.
  static void ephemeronIteration(typename Allocator::Visitor* visitor,
                                 void* closure) {
    HashTableType* table = reinterpret_cast<HashTableType*>(closure);
    DCHECK(table->m_table);
    // Check the hash table for elements that we now know will not be
    // removed by weak processing. Those elements need to have their strong
    // pointers traced.
    for (ValueType* element = table->m_table + table->m_tableSize - 1;
         element >= table->m_table; element--) {
      if (!HashTableType::isEmptyOrDeletedBucket(*element))
        TraceInCollectionTrait<WeakHandlingInCollections, WeakPointersActWeak,
                               ValueType, Traits>::trace(visitor, *element);
    }
  }

  // Called when the ephemeron iteration is done and before running the per
  // thread weak processing. It is guaranteed to be called before any thread
  // is resumed.
  static void ephemeronIterationDone(typename Allocator::Visitor* visitor,
                                     void* closure) {
    HashTableType* table = reinterpret_cast<HashTableType*>(closure);
#if DCHECK_IS_ON()
    DCHECK(Allocator::weakTableRegistered(visitor, table));
#endif
    table->clearEnqueued();
  }
};

template <typename Key,
          typename Value,
          typename Extractor,
          typename HashFunctions,
          typename Traits,
          typename KeyTraits,
          typename Allocator>
template <typename VisitorDispatcher>
void HashTable<Key,
               Value,
               Extractor,
               HashFunctions,
               Traits,
               KeyTraits,
               Allocator>::trace(VisitorDispatcher visitor) {
#if DUMP_HASHTABLE_STATS_PER_TABLE
  Allocator::markNoTracing(visitor, m_stats);
#endif

  // If someone else already marked the backing and queued up the trace and/or
  // weak callback then we are done. This optimization does not happen for
  // ListHashSet since its iterator does not point at the backing.
  if (!m_table || Allocator::isHeapObjectAlive(m_table))
    return;

  // Normally, we mark the backing store without performing trace. This means
  // it is marked live, but the pointers inside it are not marked.  Instead we
  // will mark the pointers below. However, for backing stores that contain
  // weak pointers the handling is rather different.  We don't mark the
  // backing store here, so the marking GC will leave the backing unmarked. If
  // the backing is found in any other way than through its HashTable (ie from
  // an iterator) then the mark bit will be set and the pointers will be
  // marked strongly, avoiding problems with iterating over things that
  // disappear due to weak processing while we are iterating over them. We
  // register the backing store pointer for delayed marking which will take
  // place after we know if the backing is reachable from elsewhere. We also
  // register a weakProcessing callback which will perform weak processing if
  // needed.
  if (Traits::weakHandlingFlag == NoWeakHandlingInCollections) {
    Allocator::markNoTracing(visitor, m_table);
  } else {
    Allocator::registerDelayedMarkNoTracing(visitor, m_table);
    // Since we're delaying marking this HashTable, it is possible that the
    // registerWeakMembers is called multiple times (in rare
    // cases). However, it shouldn't cause any issue.
    Allocator::registerWeakMembers(
        visitor, this,
        WeakProcessingHashTableHelper<Traits::weakHandlingFlag, Key, Value,
                                      Extractor, HashFunctions, Traits,
                                      KeyTraits, Allocator>::process);
  }
  // If the backing store will be moved by sweep compaction, register the
  // table reference pointing to the backing store object, so that the
  // reference is updated upon object relocation. A no-op if not enabled
  // by the visitor.
  Allocator::registerBackingStoreReference(visitor, &m_table);
  if (!IsTraceableInCollectionTrait<Traits>::value)
    return;
  if (Traits::weakHandlingFlag == WeakHandlingInCollections) {
    // If we have both strong and weak pointers in the collection then
    // we queue up the collection for fixed point iteration a la
    // Ephemerons:
    // http://dl.acm.org/citation.cfm?doid=263698.263733 - see also
    // http://www.jucs.org/jucs_14_21/eliminating_cycles_in_weak
#if DCHECK_IS_ON()
    DCHECK(!enqueued() || Allocator::weakTableRegistered(visitor, this));
#endif
    if (!enqueued()) {
      Allocator::registerWeakTable(
          visitor, this,
          WeakProcessingHashTableHelper<
              Traits::weakHandlingFlag, Key, Value, Extractor, HashFunctions,
              Traits, KeyTraits, Allocator>::ephemeronIteration,
          WeakProcessingHashTableHelper<
              Traits::weakHandlingFlag, Key, Value, Extractor, HashFunctions,
              Traits, KeyTraits, Allocator>::ephemeronIterationDone);
      setEnqueued();
    }
    // We don't need to trace the elements here, since registering as a
    // weak table above will cause them to be traced (perhaps several
    // times). It's better to wait until everything else is traced
    // before tracing the elements for the first time; this may reduce
    // (by one) the number of iterations needed to get to a fixed point.
    return;
  }
  for (ValueType* element = m_table + m_tableSize - 1; element >= m_table;
       element--) {
    if (!isEmptyOrDeletedBucket(*element))
      Allocator::template trace<VisitorDispatcher, ValueType, Traits>(visitor,
                                                                      *element);
  }
}

// iterator adapters

template <typename HashTableType, typename Traits>
struct HashTableConstIteratorAdapter {
  STACK_ALLOCATED();
  HashTableConstIteratorAdapter() {}
  HashTableConstIteratorAdapter(
      const typename HashTableType::const_iterator& impl)
      : m_impl(impl) {}
  typedef typename Traits::IteratorConstGetType GetType;
  typedef
      typename HashTableType::ValueTraits::IteratorConstGetType SourceGetType;

  GetType get() const {
    return const_cast<GetType>(SourceGetType(m_impl.get()));
  }
  typename Traits::IteratorConstReferenceType operator*() const {
    return Traits::getToReferenceConstConversion(get());
  }
  GetType operator->() const { return get(); }

  HashTableConstIteratorAdapter& operator++() {
    ++m_impl;
    return *this;
  }
  // postfix ++ intentionally omitted

  typename HashTableType::const_iterator m_impl;
};

template <typename HashTable, typename Traits>
std::ostream& operator<<(
    std::ostream& stream,
    const HashTableConstIteratorAdapter<HashTable, Traits>& iterator) {
  return stream << iterator.m_impl;
}

template <typename HashTableType, typename Traits>
struct HashTableIteratorAdapter {
  STACK_ALLOCATED();
  typedef typename Traits::IteratorGetType GetType;
  typedef typename HashTableType::ValueTraits::IteratorGetType SourceGetType;

  HashTableIteratorAdapter() {}
  HashTableIteratorAdapter(const typename HashTableType::iterator& impl)
      : m_impl(impl) {}

  GetType get() const {
    return const_cast<GetType>(SourceGetType(m_impl.get()));
  }
  typename Traits::IteratorReferenceType operator*() const {
    return Traits::getToReferenceConversion(get());
  }
  GetType operator->() const { return get(); }

  HashTableIteratorAdapter& operator++() {
    ++m_impl;
    return *this;
  }
  // postfix ++ intentionally omitted

  operator HashTableConstIteratorAdapter<HashTableType, Traits>() {
    typename HashTableType::const_iterator i = m_impl;
    return i;
  }

  typename HashTableType::iterator m_impl;
};

template <typename HashTable, typename Traits>
std::ostream& operator<<(
    std::ostream& stream,
    const HashTableIteratorAdapter<HashTable, Traits>& iterator) {
  return stream << iterator.m_impl;
}

template <typename T, typename U>
inline bool operator==(const HashTableConstIteratorAdapter<T, U>& a,
                       const HashTableConstIteratorAdapter<T, U>& b) {
  return a.m_impl == b.m_impl;
}

template <typename T, typename U>
inline bool operator!=(const HashTableConstIteratorAdapter<T, U>& a,
                       const HashTableConstIteratorAdapter<T, U>& b) {
  return a.m_impl != b.m_impl;
}

template <typename T, typename U>
inline bool operator==(const HashTableIteratorAdapter<T, U>& a,
                       const HashTableIteratorAdapter<T, U>& b) {
  return a.m_impl == b.m_impl;
}

template <typename T, typename U>
inline bool operator!=(const HashTableIteratorAdapter<T, U>& a,
                       const HashTableIteratorAdapter<T, U>& b) {
  return a.m_impl != b.m_impl;
}

// All 4 combinations of ==, != and Const,non const.
template <typename T, typename U>
inline bool operator==(const HashTableConstIteratorAdapter<T, U>& a,
                       const HashTableIteratorAdapter<T, U>& b) {
  return a.m_impl == b.m_impl;
}

template <typename T, typename U>
inline bool operator!=(const HashTableConstIteratorAdapter<T, U>& a,
                       const HashTableIteratorAdapter<T, U>& b) {
  return a.m_impl != b.m_impl;
}

template <typename T, typename U>
inline bool operator==(const HashTableIteratorAdapter<T, U>& a,
                       const HashTableConstIteratorAdapter<T, U>& b) {
  return a.m_impl == b.m_impl;
}

template <typename T, typename U>
inline bool operator!=(const HashTableIteratorAdapter<T, U>& a,
                       const HashTableConstIteratorAdapter<T, U>& b) {
  return a.m_impl != b.m_impl;
}

template <typename Collection1, typename Collection2>
inline void removeAll(Collection1& collection, const Collection2& toBeRemoved) {
  if (collection.isEmpty() || toBeRemoved.isEmpty())
    return;
  typedef typename Collection2::const_iterator CollectionIterator;
  CollectionIterator end(toBeRemoved.end());
  for (CollectionIterator it(toBeRemoved.begin()); it != end; ++it)
    collection.erase(*it);
}

}  // namespace WTF

#include "wtf/HashIterators.h"

#endif  // WTF_HashTable_h
