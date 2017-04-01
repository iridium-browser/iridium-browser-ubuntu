/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ShapeCache_h
#define ShapeCache_h

#include "platform/fonts/shaping/ShapeResult.h"
#include "platform/text/TextRun.h"
#include "wtf/Forward.h"
#include "wtf/HashFunctions.h"
#include "wtf/HashSet.h"
#include "wtf/HashTableDeletedValueType.h"
#include "wtf/StringHasher.h"
#include "wtf/WeakPtr.h"

namespace blink {

struct ShapeCacheEntry {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  ShapeCacheEntry() { m_shapeResult = nullptr; }
  RefPtr<const ShapeResult> m_shapeResult;
};

class ShapeCache {
  USING_FAST_MALLOC(ShapeCache);
  WTF_MAKE_NONCOPYABLE(ShapeCache);

 private:
  // Used to optimize small strings as hash table keys. Avoids malloc'ing an
  // out-of-line StringImpl.
  class SmallStringKey {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

   public:
    static unsigned capacity() { return s_capacity; }

    SmallStringKey()
        : m_length(s_emptyValueLength),
          m_direction(static_cast<unsigned>(TextDirection::kLtr)) {}

    SmallStringKey(WTF::HashTableDeletedValueType)
        : m_length(s_deletedValueLength),
          m_direction(static_cast<unsigned>(TextDirection::kLtr)) {}

    template <typename CharacterType>
    SmallStringKey(CharacterType* characters,
                   unsigned short length,
                   TextDirection direction)
        : m_length(length), m_direction(static_cast<unsigned>(direction)) {
      ASSERT(length <= s_capacity);

      StringHasher hasher;

      bool remainder = length & 1;
      length >>= 1;

      unsigned i = 0;
      while (length--) {
        m_characters[i] = characters[i];
        m_characters[i + 1] = characters[i + 1];
        hasher.addCharactersAssumingAligned(characters[i], characters[i + 1]);
        i += 2;
      }

      if (remainder) {
        m_characters[i] = characters[i];
        hasher.addCharacter(characters[i]);
      }

      m_hash = hasher.hash();
    }

    const UChar* characters() const { return m_characters; }
    unsigned short length() const { return m_length; }
    TextDirection direction() const {
      return static_cast<TextDirection>(m_direction);
    }
    unsigned hash() const { return m_hash; }

    bool isHashTableDeletedValue() const {
      return m_length == s_deletedValueLength;
    }
    bool isHashTableEmptyValue() const {
      return m_length == s_emptyValueLength;
    }

   private:
    static const unsigned s_capacity = 15;
    static const unsigned s_emptyValueLength = s_capacity + 1;
    static const unsigned s_deletedValueLength = s_capacity + 2;

    unsigned m_hash;
    unsigned m_length : 15;
    unsigned m_direction : 1;
    UChar m_characters[s_capacity];
  };

  struct SmallStringKeyHash {
    STATIC_ONLY(SmallStringKeyHash);
    static unsigned hash(const SmallStringKey& key) { return key.hash(); }
    static bool equal(const SmallStringKey& a, const SmallStringKey& b) {
      return a == b;
    }
    // Empty and deleted values have lengths that are not equal to any valid
    // length.
    static const bool safeToCompareToEmptyOrDeleted = true;
  };

  struct SmallStringKeyHashTraits : WTF::SimpleClassHashTraits<SmallStringKey> {
    STATIC_ONLY(SmallStringKeyHashTraits);
    static const bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const SmallStringKey& key) {
      return key.isHashTableEmptyValue();
    }
    static const unsigned minimumTableSize = 16;
  };

  friend bool operator==(const SmallStringKey&, const SmallStringKey&);

 public:
  ShapeCache() : m_weakFactory(this), m_version(0) {}

  ShapeCacheEntry* add(const TextRun& run, ShapeCacheEntry entry) {
    if (run.length() > SmallStringKey::capacity())
      return 0;

    return addSlowCase(run, entry);
  }

  void clearIfVersionChanged(unsigned version) {
    if (version != m_version) {
      clear();
      m_version = version;
    }
  }

  void clear() {
    m_singleCharMap.clear();
    m_shortStringMap.clear();
  }

  unsigned size() const {
    return m_singleCharMap.size() + m_shortStringMap.size();
  }

  size_t byteSize() const {
    size_t selfByteSize = 0;
    for (auto cacheEntry : m_singleCharMap) {
      selfByteSize += cacheEntry.value.m_shapeResult->byteSize();
    }
    for (auto cacheEntry : m_shortStringMap) {
      selfByteSize += cacheEntry.value.m_shapeResult->byteSize();
    }
    return selfByteSize;
  }

  WeakPtr<ShapeCache> weakPtr() { return m_weakFactory.createWeakPtr(); }

 private:
  ShapeCacheEntry* addSlowCase(const TextRun& run, ShapeCacheEntry entry) {
    unsigned length = run.length();
    bool isNewEntry;
    ShapeCacheEntry* value;
    if (length == 1) {
      uint32_t key = run[0];
      // All current codepointsin UTF-32 are bewteen 0x0 and 0x10FFFF,
      // as such use bit 32 to indicate direction.
      if (run.direction() == TextDirection::kRtl)
        key |= (1u << 31);
      SingleCharMap::AddResult addResult = m_singleCharMap.add(key, entry);
      isNewEntry = addResult.isNewEntry;
      value = &addResult.storedValue->value;
    } else {
      SmallStringKey smallStringKey;
      if (run.is8Bit())
        smallStringKey =
            SmallStringKey(run.characters8(), length, run.direction());
      else
        smallStringKey =
            SmallStringKey(run.characters16(), length, run.direction());

      SmallStringMap::AddResult addResult =
          m_shortStringMap.add(smallStringKey, entry);
      isNewEntry = addResult.isNewEntry;
      value = &addResult.storedValue->value;
    }

    if (!isNewEntry)
      return value;

    if (size() < s_maxSize)
      return value;

    // No need to be fancy: we're just trying to avoid pathological growth.
    m_singleCharMap.clear();
    m_shortStringMap.clear();

    return 0;
  }

  typedef HashMap<SmallStringKey,
                  ShapeCacheEntry,
                  SmallStringKeyHash,
                  SmallStringKeyHashTraits>
      SmallStringMap;
  typedef HashMap<uint32_t,
                  ShapeCacheEntry,
                  DefaultHash<uint32_t>::Hash,
                  WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>
      SingleCharMap;

  // Hard limit to guard against pathological growth. The expected number of
  // cache entries is a lot lower given the average word count for a web page
  // is well below 1,000 and even full length books rarely have over 10,000
  // unique words [1]. 1: http://www.mine-control.com/zack/guttenberg/
  // Our definition of a word is somewhat different from the norm in that we
  // only segment on space. Thus "foo", "foo-", and "foo)" would count as
  // three separate words. Given that 10,000 seems like a reasonable maximum.
  static const unsigned s_maxSize = 10000;

  SingleCharMap m_singleCharMap;
  SmallStringMap m_shortStringMap;
  WeakPtrFactory<ShapeCache> m_weakFactory;
  unsigned m_version;
};

inline bool operator==(const ShapeCache::SmallStringKey& a,
                       const ShapeCache::SmallStringKey& b) {
  if (a.length() != b.length() || a.direction() != b.direction())
    return false;
  return WTF::equal(a.characters(), b.characters(), a.length());
}

}  // namespace blink

#endif  // ShapeCache_h
