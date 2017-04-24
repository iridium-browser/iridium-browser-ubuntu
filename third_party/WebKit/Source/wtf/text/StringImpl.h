/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2013 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef StringImpl_h
#define StringImpl_h

#include "wtf/ASCIICType.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/StringHasher.h"
#include "wtf/Vector.h"
#include "wtf/WTFExport.h"
#include "wtf/text/ASCIIFastPath.h"
#include "wtf/text/Unicode.h"
#include <limits.h>
#include <string.h>

#if DCHECK_IS_ON()
#include "wtf/ThreadRestrictionVerifier.h"
#endif

#if OS(MACOSX)
typedef const struct __CFString* CFStringRef;
#endif

#ifdef __OBJC__
@class NSString;
#endif

namespace WTF {

struct AlreadyHashed;
template <typename>
class RetainPtr;

enum TextCaseSensitivity {
  TextCaseSensitive,
  TextCaseASCIIInsensitive,

  // Unicode aware case insensitive matching. Non-ASCII characters might match
  // to ASCII characters. This flag is rarely used to implement web platform
  // features.
  TextCaseUnicodeInsensitive
};

enum StripBehavior { StripExtraWhiteSpace, DoNotStripWhiteSpace };

typedef bool (*CharacterMatchFunctionPtr)(UChar);
typedef bool (*IsWhiteSpaceFunctionPtr)(UChar);
typedef HashMap<unsigned, StringImpl*, AlreadyHashed> StaticStringsTable;

// Define STRING_STATS to turn on run time statistics of string sizes and memory
// usage
#undef STRING_STATS

#ifdef STRING_STATS
struct StringStats {
  inline void add8BitString(unsigned length) {
    ++m_totalNumberStrings;
    ++m_number8BitStrings;
    m_total8BitData += length;
  }

  inline void add16BitString(unsigned length) {
    ++m_totalNumberStrings;
    ++m_number16BitStrings;
    m_total16BitData += length;
  }

  void removeString(StringImpl*);
  void printStats();

  static const unsigned s_printStringStatsFrequency = 5000;
  static unsigned s_stringRemovesTillPrintStats;

  unsigned m_totalNumberStrings;
  unsigned m_number8BitStrings;
  unsigned m_number16BitStrings;
  unsigned long long m_total8BitData;
  unsigned long long m_total16BitData;
};

void addStringForStats(StringImpl*);
void removeStringForStats(StringImpl*);

#define STRING_STATS_ADD_8BIT_STRING(length)       \
  StringImpl::stringStats().add8BitString(length); \
  addStringForStats(this)
#define STRING_STATS_ADD_16BIT_STRING(length)       \
  StringImpl::stringStats().add16BitString(length); \
  addStringForStats(this)
#define STRING_STATS_REMOVE_STRING(string)        \
  StringImpl::stringStats().removeString(string); \
  removeStringForStats(this)
#else
#define STRING_STATS_ADD_8BIT_STRING(length) ((void)0)
#define STRING_STATS_ADD_16BIT_STRING(length) ((void)0)
#define STRING_STATS_REMOVE_STRING(string) ((void)0)
#endif

// You can find documentation about this class in this doc:
// https://docs.google.com/document/d/1kOCUlJdh2WJMJGDf-WoEQhmnjKLaOYRbiHz5TiGJl14/edit?usp=sharing
class WTF_EXPORT StringImpl {
  WTF_MAKE_NONCOPYABLE(StringImpl);

 private:
  // StringImpls are allocated out of the WTF buffer partition.
  void* operator new(size_t);
  void* operator new(size_t, void* ptr) { return ptr; }
  void operator delete(void*);

  // Used to construct static strings, which have an special refCount that can
  // never hit zero.  This means that the static string will never be
  // destroyed, which is important because static strings will be shared
  // across threads & ref-counted in a non-threadsafe manner.
  enum ConstructEmptyStringTag { ConstructEmptyString };
  explicit StringImpl(ConstructEmptyStringTag)
      : m_refCount(1),
        m_length(0),
        m_hash(0),
        m_containsOnlyASCII(true),
        m_needsASCIICheck(false),
        m_isAtomic(false),
        m_is8Bit(true),
        m_isStatic(true) {
    // Ensure that the hash is computed so that AtomicStringHash can call
    // existingHash() with impunity. The empty string is special because it
    // is never entered into AtomicString's HashKey, but still needs to
    // compare correctly.
    STRING_STATS_ADD_8BIT_STRING(m_length);
    hash();
  }

  enum ConstructEmptyString16BitTag { ConstructEmptyString16Bit };
  explicit StringImpl(ConstructEmptyString16BitTag)
      : m_refCount(1),
        m_length(0),
        m_hash(0),
        m_containsOnlyASCII(true),
        m_needsASCIICheck(false),
        m_isAtomic(false),
        m_is8Bit(false),
        m_isStatic(true) {
    STRING_STATS_ADD_16BIT_STRING(m_length);
    hash();
  }

  // FIXME: there has to be a less hacky way to do this.
  enum Force8Bit { Force8BitConstructor };
  StringImpl(unsigned length, Force8Bit)
      : m_refCount(1),
        m_length(length),
        m_hash(0),
        m_containsOnlyASCII(!length),
        m_needsASCIICheck(static_cast<bool>(length)),
        m_isAtomic(false),
        m_is8Bit(true),
        m_isStatic(false) {
    DCHECK(m_length);
    STRING_STATS_ADD_8BIT_STRING(m_length);
  }

  StringImpl(unsigned length)
      : m_refCount(1),
        m_length(length),
        m_hash(0),
        m_containsOnlyASCII(!length),
        m_needsASCIICheck(static_cast<bool>(length)),
        m_isAtomic(false),
        m_is8Bit(false),
        m_isStatic(false) {
    DCHECK(m_length);
    STRING_STATS_ADD_16BIT_STRING(m_length);
  }

  enum StaticStringTag { StaticString };
  StringImpl(unsigned length, unsigned hash, StaticStringTag)
      : m_refCount(1),
        m_length(length),
        m_hash(hash),
        m_containsOnlyASCII(!length),
        m_needsASCIICheck(static_cast<bool>(length)),
        m_isAtomic(false),
        m_is8Bit(true),
        m_isStatic(true) {}

 public:
  static StringImpl* empty;
  static StringImpl* empty16Bit;

  ~StringImpl();

  static void initStatics();

  static StringImpl* createStatic(const char* string,
                                  unsigned length,
                                  unsigned hash);
  static void reserveStaticStringsCapacityForSize(unsigned size);
  static void freezeStaticStrings();
  static const StaticStringsTable& allStaticStrings();
  static unsigned highestStaticStringLength() {
    return m_highestStaticStringLength;
  }

  static PassRefPtr<StringImpl> create(const UChar*, unsigned length);
  static PassRefPtr<StringImpl> create(const LChar*, unsigned length);
  static PassRefPtr<StringImpl> create8BitIfPossible(const UChar*,
                                                     unsigned length);
  template <size_t inlineCapacity>
  static PassRefPtr<StringImpl> create8BitIfPossible(
      const Vector<UChar, inlineCapacity>& vector) {
    return create8BitIfPossible(vector.data(), vector.size());
  }

  ALWAYS_INLINE static PassRefPtr<StringImpl> create(const char* s,
                                                     unsigned length) {
    return create(reinterpret_cast<const LChar*>(s), length);
  }
  static PassRefPtr<StringImpl> create(const LChar*);
  ALWAYS_INLINE static PassRefPtr<StringImpl> create(const char* s) {
    return create(reinterpret_cast<const LChar*>(s));
  }

  static PassRefPtr<StringImpl> createUninitialized(unsigned length,
                                                    LChar*& data);
  static PassRefPtr<StringImpl> createUninitialized(unsigned length,
                                                    UChar*& data);

  unsigned length() const { return m_length; }
  bool is8Bit() const { return m_is8Bit; }

  ALWAYS_INLINE const LChar* characters8() const {
    DCHECK(is8Bit());
    return reinterpret_cast<const LChar*>(this + 1);
  }
  ALWAYS_INLINE const UChar* characters16() const {
    DCHECK(!is8Bit());
    return reinterpret_cast<const UChar*>(this + 1);
  }
  ALWAYS_INLINE const void* bytes() const {
    return reinterpret_cast<const void*>(this + 1);
  }

  template <typename CharType>
  ALWAYS_INLINE const CharType* getCharacters() const;

  size_t charactersSizeInBytes() const {
    return length() * (is8Bit() ? sizeof(LChar) : sizeof(UChar));
  }

  bool isAtomic() const { return m_isAtomic; }
  void setIsAtomic(bool isAtomic) { m_isAtomic = isAtomic; }

  bool isStatic() const { return m_isStatic; }

  bool containsOnlyASCII() const;

  bool isSafeToSendToAnotherThread() const;

  // The high bits of 'hash' are always empty, but we prefer to store our
  // flags in the low bits because it makes them slightly more efficient to
  // access.  So, we shift left and right when setting and getting our hash
  // code.
  void setHash(unsigned hash) const {
    DCHECK(!hasHash());
    // Multiple clients assume that StringHasher is the canonical string
    // hash function.
    DCHECK(hash == (is8Bit() ? StringHasher::computeHashAndMaskTop8Bits(
                                   characters8(), m_length)
                             : StringHasher::computeHashAndMaskTop8Bits(
                                   characters16(), m_length)));
    m_hash = hash;
    DCHECK(hash);  // Verify that 0 is a valid sentinel hash value.
  }

  bool hasHash() const { return m_hash != 0; }

  unsigned existingHash() const {
    DCHECK(hasHash());
    return m_hash;
  }

  unsigned hash() const {
    if (hasHash())
      return existingHash();
    return hashSlowCase();
  }

  ALWAYS_INLINE bool hasOneRef() const {
#if DCHECK_IS_ON()
    DCHECK(isStatic() || m_verifier.isSafeToUse()) << asciiForDebugging();
#endif
    return m_refCount == 1;
  }

  ALWAYS_INLINE void ref() const {
#if DCHECK_IS_ON()
    DCHECK(isStatic() || m_verifier.onRef(m_refCount)) << asciiForDebugging();
#endif
    ++m_refCount;
  }

  ALWAYS_INLINE void deref() const {
#if DCHECK_IS_ON()
    DCHECK(isStatic() || m_verifier.onDeref(m_refCount))
        << asciiForDebugging() << " " << currentThread();
#endif
    if (!--m_refCount)
      destroyIfNotStatic();
  }

  // FIXME: Does this really belong in StringImpl?
  template <typename T>
  static void copyChars(T* destination,
                        const T* source,
                        unsigned numCharacters) {
    memcpy(destination, source, numCharacters * sizeof(T));
  }

  ALWAYS_INLINE static void copyChars(UChar* destination,
                                      const LChar* source,
                                      unsigned numCharacters) {
    for (unsigned i = 0; i < numCharacters; ++i)
      destination[i] = source[i];
  }

  // Some string features, like refcounting and the atomicity flag, are not
  // thread-safe. We achieve thread safety by isolation, giving each thread
  // its own copy of the string.
  PassRefPtr<StringImpl> isolatedCopy() const;

  PassRefPtr<StringImpl> substring(unsigned pos, unsigned len = UINT_MAX) const;

  UChar operator[](unsigned i) const {
    SECURITY_DCHECK(i < m_length);
    if (is8Bit())
      return characters8()[i];
    return characters16()[i];
  }
  UChar32 characterStartingAt(unsigned);

  bool containsOnlyWhitespace();

  int toIntStrict(bool* ok = 0, int base = 10);
  unsigned toUIntStrict(bool* ok = 0, int base = 10);
  int64_t toInt64Strict(bool* ok = 0, int base = 10);
  uint64_t toUInt64Strict(bool* ok = 0, int base = 10);

  int toInt(bool* ok = 0);          // ignores trailing garbage
  unsigned toUInt(bool* ok = 0);    // ignores trailing garbage
  int64_t toInt64(bool* ok = 0);    // ignores trailing garbage
  uint64_t toUInt64(bool* ok = 0);  // ignores trailing garbage

  // FIXME: Like the strict functions above, these give false for "ok" when
  // there is trailing garbage.  Like the non-strict functions above, these
  // return the value when there is trailing garbage.  It would be better if
  // these were more consistent with the above functions instead.
  double toDouble(bool* ok = 0);
  float toFloat(bool* ok = 0);

  PassRefPtr<StringImpl> lower();
  PassRefPtr<StringImpl> lowerASCII();
  PassRefPtr<StringImpl> upper();
  PassRefPtr<StringImpl> upperASCII();
  PassRefPtr<StringImpl> lower(const AtomicString& localeIdentifier);
  PassRefPtr<StringImpl> upper(const AtomicString& localeIdentifier);

  PassRefPtr<StringImpl> fill(UChar);
  // FIXME: Do we need fill(char) or can we just do the right thing if UChar is
  // ASCII?
  PassRefPtr<StringImpl> foldCase();

  PassRefPtr<StringImpl> truncate(unsigned length);

  PassRefPtr<StringImpl> stripWhiteSpace();
  PassRefPtr<StringImpl> stripWhiteSpace(IsWhiteSpaceFunctionPtr);
  PassRefPtr<StringImpl> simplifyWhiteSpace(
      StripBehavior = StripExtraWhiteSpace);
  PassRefPtr<StringImpl> simplifyWhiteSpace(
      IsWhiteSpaceFunctionPtr,
      StripBehavior = StripExtraWhiteSpace);

  PassRefPtr<StringImpl> removeCharacters(CharacterMatchFunctionPtr);
  template <typename CharType>
  ALWAYS_INLINE PassRefPtr<StringImpl> removeCharacters(
      const CharType* characters,
      CharacterMatchFunctionPtr);

  // Remove characters between [start, start+lengthToRemove). The range is
  // clamped to the size of the string. Does nothing if start >= length().
  PassRefPtr<StringImpl> remove(unsigned start, unsigned lengthToRemove = 1);

  // Find characters.
  size_t find(LChar character, unsigned start = 0);
  size_t find(char character, unsigned start = 0);
  size_t find(UChar character, unsigned start = 0);
  size_t find(CharacterMatchFunctionPtr, unsigned index = 0);

  // Find substrings.
  size_t find(const StringView&, unsigned index = 0);
  // Unicode aware case insensitive string matching. Non-ASCII characters might
  // match to ASCII characters. This function is rarely used to implement web
  // platform features.
  size_t findIgnoringCase(const StringView&, unsigned index = 0);
  size_t findIgnoringASCIICase(const StringView&, unsigned index = 0);

  size_t reverseFind(UChar, unsigned index = UINT_MAX);
  size_t reverseFind(const StringView&, unsigned index = UINT_MAX);

  bool startsWith(UChar) const;
  bool startsWith(const StringView&) const;
  bool startsWithIgnoringCase(const StringView&) const;
  bool startsWithIgnoringASCIICase(const StringView&) const;

  bool endsWith(UChar) const;
  bool endsWith(const StringView&) const;
  bool endsWithIgnoringCase(const StringView&) const;
  bool endsWithIgnoringASCIICase(const StringView&) const;

  // Replace parts of the string.
  PassRefPtr<StringImpl> replace(UChar pattern, UChar replacement);
  PassRefPtr<StringImpl> replace(UChar pattern, const StringView& replacement);
  PassRefPtr<StringImpl> replace(const StringView& pattern,
                                 const StringView& replacement);
  PassRefPtr<StringImpl> replace(unsigned index,
                                 unsigned lengthToReplace,
                                 const StringView& replacement);

  PassRefPtr<StringImpl> upconvertedString();

  // Copy characters from string starting at |start| up until |maxLength| or
  // the end of the string is reached. Returns the actual number of characters
  // copied.
  unsigned copyTo(UChar* buffer, unsigned start, unsigned maxLength) const;

  // Append characters from this string into a buffer. Expects the buffer to
  // have the methods:
  //    append(const UChar*, unsigned length);
  //    append(const LChar*, unsigned length);
  // StringBuilder and Vector conform to this protocol.
  template <typename BufferType>
  void appendTo(BufferType&,
                unsigned start = 0,
                unsigned length = UINT_MAX) const;

  // Prepend characters from this string into a buffer. Expects the buffer to
  // have the methods:
  //    prepend(const UChar*, unsigned length);
  //    prepend(const LChar*, unsigned length);
  // Vector conforms to this protocol.
  template <typename BufferType>
  void prependTo(BufferType&,
                 unsigned start = 0,
                 unsigned length = UINT_MAX) const;

#if OS(MACOSX)
  RetainPtr<CFStringRef> createCFString();
#endif
#ifdef __OBJC__
  operator NSString*();
#endif

#ifdef STRING_STATS
  ALWAYS_INLINE static StringStats& stringStats() { return m_stringStats; }
#endif
  static const UChar latin1CaseFoldTable[256];

 private:
  template <typename CharType>
  static size_t allocationSize(unsigned length) {
    RELEASE_ASSERT(
        length <= ((std::numeric_limits<unsigned>::max() - sizeof(StringImpl)) /
                   sizeof(CharType)));
    return sizeof(StringImpl) + length * sizeof(CharType);
  }

  PassRefPtr<StringImpl> replace(UChar pattern,
                                 const LChar* replacement,
                                 unsigned replacementLength);
  PassRefPtr<StringImpl> replace(UChar pattern,
                                 const UChar* replacement,
                                 unsigned replacementLength);

  template <class UCharPredicate>
  PassRefPtr<StringImpl> stripMatchedCharacters(UCharPredicate);
  template <typename CharType, class UCharPredicate>
  PassRefPtr<StringImpl> simplifyMatchedCharactersToSpace(UCharPredicate,
                                                          StripBehavior);
  NEVER_INLINE unsigned hashSlowCase() const;

  void destroyIfNotStatic() const;
  void updateContainsOnlyASCII() const;

#if DCHECK_IS_ON()
  std::string asciiForDebugging() const;
#endif

#ifdef STRING_STATS
  static StringStats m_stringStats;
#endif

  static unsigned m_highestStaticStringLength;

#if DCHECK_IS_ON()
  void assertHashIsCorrect() {
    DCHECK(hasHash());
    DCHECK_EQ(existingHash(), StringHasher::computeHashAndMaskTop8Bits(
                                  characters8(), length()));
  }
#endif

 private:
#if DCHECK_IS_ON()
  mutable ThreadRestrictionVerifier m_verifier;
#endif
  mutable unsigned m_refCount;
  const unsigned m_length;
  mutable unsigned m_hash : 24;
  mutable unsigned m_containsOnlyASCII : 1;
  mutable unsigned m_needsASCIICheck : 1;
  unsigned m_isAtomic : 1;
  const unsigned m_is8Bit : 1;
  const unsigned m_isStatic : 1;
};

template <>
ALWAYS_INLINE const LChar* StringImpl::getCharacters<LChar>() const {
  return characters8();
}

template <>
ALWAYS_INLINE const UChar* StringImpl::getCharacters<UChar>() const {
  return characters16();
}

WTF_EXPORT bool equal(const StringImpl*, const StringImpl*);
WTF_EXPORT bool equal(const StringImpl*, const LChar*);
inline bool equal(const StringImpl* a, const char* b) {
  return equal(a, reinterpret_cast<const LChar*>(b));
}
WTF_EXPORT bool equal(const StringImpl*, const LChar*, unsigned);
WTF_EXPORT bool equal(const StringImpl*, const UChar*, unsigned);
inline bool equal(const StringImpl* a, const char* b, unsigned length) {
  return equal(a, reinterpret_cast<const LChar*>(b), length);
}
inline bool equal(const LChar* a, StringImpl* b) {
  return equal(b, a);
}
inline bool equal(const char* a, StringImpl* b) {
  return equal(b, reinterpret_cast<const LChar*>(a));
}
WTF_EXPORT bool equalNonNull(const StringImpl* a, const StringImpl* b);

ALWAYS_INLINE bool StringImpl::containsOnlyASCII() const {
  if (m_needsASCIICheck)
    updateContainsOnlyASCII();
  return m_containsOnlyASCII;
}

template <typename CharType>
ALWAYS_INLINE bool equal(const CharType* a,
                         const CharType* b,
                         unsigned length) {
  return !memcmp(a, b, length * sizeof(CharType));
}

ALWAYS_INLINE bool equal(const LChar* a, const UChar* b, unsigned length) {
  for (unsigned i = 0; i < length; ++i) {
    if (a[i] != b[i])
      return false;
  }
  return true;
}

ALWAYS_INLINE bool equal(const UChar* a, const LChar* b, unsigned length) {
  return equal(b, a, length);
}

// Unicode aware case insensitive string matching. Non-ASCII characters might
// match to ASCII characters. These functions are rarely used to implement web
// platform features.
WTF_EXPORT bool equalIgnoringCase(const LChar*, const LChar*, unsigned length);
WTF_EXPORT bool equalIgnoringCase(const UChar*, const LChar*, unsigned length);
inline bool equalIgnoringCase(const LChar* a, const UChar* b, unsigned length) {
  return equalIgnoringCase(b, a, length);
}
WTF_EXPORT bool equalIgnoringCase(const UChar*, const UChar*, unsigned length);

WTF_EXPORT bool equalIgnoringNullity(StringImpl*, StringImpl*);

template <typename CharacterTypeA, typename CharacterTypeB>
inline bool equalIgnoringASCIICase(const CharacterTypeA* a,
                                   const CharacterTypeB* b,
                                   unsigned length) {
  for (unsigned i = 0; i < length; ++i) {
    if (toASCIILower(a[i]) != toASCIILower(b[i]))
      return false;
  }
  return true;
}

WTF_EXPORT int codePointCompareIgnoringASCIICase(const StringImpl*,
                                                 const LChar*);

inline size_t find(const LChar* characters,
                   unsigned length,
                   LChar matchCharacter,
                   unsigned index = 0) {
  // Some clients rely on being able to pass index >= length.
  if (index >= length)
    return kNotFound;
  const LChar* found = static_cast<const LChar*>(
      memchr(characters + index, matchCharacter, length - index));
  return found ? found - characters : kNotFound;
}

inline size_t find(const UChar* characters,
                   unsigned length,
                   UChar matchCharacter,
                   unsigned index = 0) {
  while (index < length) {
    if (characters[index] == matchCharacter)
      return index;
    ++index;
  }
  return kNotFound;
}

ALWAYS_INLINE size_t find(const UChar* characters,
                          unsigned length,
                          LChar matchCharacter,
                          unsigned index = 0) {
  return find(characters, length, static_cast<UChar>(matchCharacter), index);
}

inline size_t find(const LChar* characters,
                   unsigned length,
                   UChar matchCharacter,
                   unsigned index = 0) {
  if (matchCharacter & ~0xFF)
    return kNotFound;
  return find(characters, length, static_cast<LChar>(matchCharacter), index);
}

template <typename CharacterType>
inline size_t find(const CharacterType* characters,
                   unsigned length,
                   char matchCharacter,
                   unsigned index = 0) {
  return find(characters, length, static_cast<LChar>(matchCharacter), index);
}

inline size_t find(const LChar* characters,
                   unsigned length,
                   CharacterMatchFunctionPtr matchFunction,
                   unsigned index = 0) {
  while (index < length) {
    if (matchFunction(characters[index]))
      return index;
    ++index;
  }
  return kNotFound;
}

inline size_t find(const UChar* characters,
                   unsigned length,
                   CharacterMatchFunctionPtr matchFunction,
                   unsigned index = 0) {
  while (index < length) {
    if (matchFunction(characters[index]))
      return index;
    ++index;
  }
  return kNotFound;
}

template <typename CharacterType>
inline size_t reverseFind(const CharacterType* characters,
                          unsigned length,
                          CharacterType matchCharacter,
                          unsigned index = UINT_MAX) {
  if (!length)
    return kNotFound;
  if (index >= length)
    index = length - 1;
  while (characters[index] != matchCharacter) {
    if (!index--)
      return kNotFound;
  }
  return index;
}

ALWAYS_INLINE size_t reverseFind(const UChar* characters,
                                 unsigned length,
                                 LChar matchCharacter,
                                 unsigned index = UINT_MAX) {
  return reverseFind(characters, length, static_cast<UChar>(matchCharacter),
                     index);
}

inline size_t reverseFind(const LChar* characters,
                          unsigned length,
                          UChar matchCharacter,
                          unsigned index = UINT_MAX) {
  if (matchCharacter & ~0xFF)
    return kNotFound;
  return reverseFind(characters, length, static_cast<LChar>(matchCharacter),
                     index);
}

inline size_t StringImpl::find(LChar character, unsigned start) {
  if (is8Bit())
    return WTF::find(characters8(), m_length, character, start);
  return WTF::find(characters16(), m_length, character, start);
}

ALWAYS_INLINE size_t StringImpl::find(char character, unsigned start) {
  return find(static_cast<LChar>(character), start);
}

inline size_t StringImpl::find(UChar character, unsigned start) {
  if (is8Bit())
    return WTF::find(characters8(), m_length, character, start);
  return WTF::find(characters16(), m_length, character, start);
}

inline unsigned lengthOfNullTerminatedString(const UChar* string) {
  size_t length = 0;
  while (string[length] != UChar(0))
    ++length;
  RELEASE_ASSERT(length <= std::numeric_limits<unsigned>::max());
  return static_cast<unsigned>(length);
}

template <size_t inlineCapacity>
bool equalIgnoringNullity(const Vector<UChar, inlineCapacity>& a,
                          StringImpl* b) {
  if (!b)
    return !a.size();
  if (a.size() != b->length())
    return false;
  if (b->is8Bit())
    return equal(a.data(), b->characters8(), b->length());
  return equal(a.data(), b->characters16(), b->length());
}

template <typename CharacterType1, typename CharacterType2>
static inline int codePointCompare(unsigned l1,
                                   unsigned l2,
                                   const CharacterType1* c1,
                                   const CharacterType2* c2) {
  const unsigned lmin = l1 < l2 ? l1 : l2;
  unsigned pos = 0;
  while (pos < lmin && *c1 == *c2) {
    ++c1;
    ++c2;
    ++pos;
  }

  if (pos < lmin)
    return (c1[0] > c2[0]) ? 1 : -1;

  if (l1 == l2)
    return 0;

  return (l1 > l2) ? 1 : -1;
}

static inline int codePointCompare8(const StringImpl* string1,
                                    const StringImpl* string2) {
  return codePointCompare(string1->length(), string2->length(),
                          string1->characters8(), string2->characters8());
}

static inline int codePointCompare16(const StringImpl* string1,
                                     const StringImpl* string2) {
  return codePointCompare(string1->length(), string2->length(),
                          string1->characters16(), string2->characters16());
}

static inline int codePointCompare8To16(const StringImpl* string1,
                                        const StringImpl* string2) {
  return codePointCompare(string1->length(), string2->length(),
                          string1->characters8(), string2->characters16());
}

static inline int codePointCompare(const StringImpl* string1,
                                   const StringImpl* string2) {
  if (!string1)
    return (string2 && string2->length()) ? -1 : 0;

  if (!string2)
    return string1->length() ? 1 : 0;

  bool string1Is8Bit = string1->is8Bit();
  bool string2Is8Bit = string2->is8Bit();
  if (string1Is8Bit) {
    if (string2Is8Bit)
      return codePointCompare8(string1, string2);
    return codePointCompare8To16(string1, string2);
  }
  if (string2Is8Bit)
    return -codePointCompare8To16(string2, string1);
  return codePointCompare16(string1, string2);
}

static inline bool isSpaceOrNewline(UChar c) {
  // Use isASCIISpace() for basic Latin-1.
  // This will include newlines, which aren't included in Unicode DirWS.
  return c <= 0x7F
             ? WTF::isASCIISpace(c)
             : WTF::Unicode::direction(c) == WTF::Unicode::WhiteSpaceNeutral;
}

inline PassRefPtr<StringImpl> StringImpl::isolatedCopy() const {
  if (is8Bit())
    return create(characters8(), m_length);
  return create(characters16(), m_length);
}

template <typename BufferType>
inline void StringImpl::appendTo(BufferType& result,
                                 unsigned start,
                                 unsigned length) const {
  unsigned numberOfCharactersToCopy = std::min(length, m_length - start);
  if (!numberOfCharactersToCopy)
    return;
  if (is8Bit())
    result.append(characters8() + start, numberOfCharactersToCopy);
  else
    result.append(characters16() + start, numberOfCharactersToCopy);
}

template <typename BufferType>
inline void StringImpl::prependTo(BufferType& result,
                                  unsigned start,
                                  unsigned length) const {
  unsigned numberOfCharactersToCopy = std::min(length, m_length - start);
  if (!numberOfCharactersToCopy)
    return;
  if (is8Bit())
    result.prepend(characters8() + start, numberOfCharactersToCopy);
  else
    result.prepend(characters16() + start, numberOfCharactersToCopy);
}

// TODO(rob.buis) possibly find a better place for this method.
// Turns a UChar32 to uppercase based on localeIdentifier.
WTF_EXPORT UChar32 toUpper(UChar32, const AtomicString& localeIdentifier);

struct StringHash;

// StringHash is the default hash for StringImpl* and RefPtr<StringImpl>
template <typename T>
struct DefaultHash;
template <>
struct DefaultHash<StringImpl*> {
  typedef StringHash Hash;
};
template <>
struct DefaultHash<RefPtr<StringImpl>> {
  typedef StringHash Hash;
};

}  // namespace WTF

using WTF::StringImpl;
using WTF::TextCaseASCIIInsensitive;
using WTF::TextCaseUnicodeInsensitive;
using WTF::TextCaseSensitive;
using WTF::TextCaseSensitivity;
using WTF::equal;
using WTF::equalNonNull;
using WTF::lengthOfNullTerminatedString;
using WTF::reverseFind;

#endif
