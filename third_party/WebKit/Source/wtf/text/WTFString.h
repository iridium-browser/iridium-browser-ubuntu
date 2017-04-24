/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012, 2013 Apple Inc.
 * All rights reserved.
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

#ifndef WTFString_h
#define WTFString_h

// This file would be called String.h, but that conflicts with <string.h>
// on systems without case-sensitive file systems.

#include "wtf/Allocator.h"
#include "wtf/Compiler.h"
#include "wtf/HashTableDeletedValueType.h"
#include "wtf/WTFExport.h"
#include "wtf/text/ASCIIFastPath.h"
#include "wtf/text/StringImpl.h"
#include "wtf/text/StringView.h"
#include <algorithm>
#include <iosfwd>

#ifdef __OBJC__
#include <objc/objc.h>
#endif

namespace WTF {

class CString;
struct StringHash;

enum UTF8ConversionMode {
  LenientUTF8Conversion,
  StrictUTF8Conversion,
  StrictUTF8ConversionReplacingUnpairedSurrogatesWithFFFD
};

#define DISPATCH_CASE_OP(caseSensitivity, op, args)    \
  ((caseSensitivity == TextCaseSensitive)              \
       ? op args                                       \
       : (caseSensitivity == TextCaseASCIIInsensitive) \
             ? op##IgnoringASCIICase args              \
             : op##IgnoringCase args)

// You can find documentation about this class in this doc:
// https://docs.google.com/document/d/1kOCUlJdh2WJMJGDf-WoEQhmnjKLaOYRbiHz5TiGJl14/edit?usp=sharing
class WTF_EXPORT String {
  USING_FAST_MALLOC(String);

 public:
  // Construct a null string, distinguishable from an empty string.
  String() {}

  // Construct a string with UTF-16 data.
  String(const UChar* characters, unsigned length);

  // Construct a string by copying the contents of a vector.
  // This method will never create a null string. Vectors with size() == 0
  // will return the empty string.
  // NOTE: This is different from String(vector.data(), vector.size())
  // which will sometimes return a null string when vector.data() is null
  // which can only occur for vectors without inline capacity.
  // See: https://bugs.webkit.org/show_bug.cgi?id=109792
  template <size_t inlineCapacity>
  explicit String(const Vector<UChar, inlineCapacity>&);

  // Construct a string with UTF-16 data, from a null-terminated source.
  String(const UChar*);
  String(const char16_t* chars)
      : String(reinterpret_cast<const UChar*>(chars)) {}

  // Construct a string with latin1 data.
  String(const LChar* characters, unsigned length);
  String(const char* characters, unsigned length);

  // Construct a string with latin1 data, from a null-terminated source.
  String(const LChar* characters)
      : String(reinterpret_cast<const char*>(characters)) {}
  String(const char* characters)
      : String(characters, characters ? strlen(characters) : 0) {}

  // Construct a string referencing an existing StringImpl.
  String(StringImpl* impl) : m_impl(impl) {}
  String(PassRefPtr<StringImpl> impl) : m_impl(impl) {}

  void swap(String& o) { m_impl.swap(o.m_impl); }

  template <typename CharType>
  static String adopt(StringBuffer<CharType>& buffer) {
    if (!buffer.length())
      return StringImpl::empty;
    return String(buffer.release());
  }

  explicit operator bool() const { return !isNull(); }
  bool isNull() const { return !m_impl; }
  bool isEmpty() const { return !m_impl || !m_impl->length(); }

  StringImpl* impl() const { return m_impl.get(); }
  PassRefPtr<StringImpl> releaseImpl() { return m_impl.release(); }

  unsigned length() const {
    if (!m_impl)
      return 0;
    return m_impl->length();
  }

  const LChar* characters8() const {
    if (!m_impl)
      return 0;
    DCHECK(m_impl->is8Bit());
    return m_impl->characters8();
  }

  const UChar* characters16() const {
    if (!m_impl)
      return 0;
    DCHECK(!m_impl->is8Bit());
    return m_impl->characters16();
  }

  // Return characters8() or characters16() depending on CharacterType.
  template <typename CharacterType>
  inline const CharacterType* getCharacters() const;

  bool is8Bit() const { return m_impl->is8Bit(); }

  CString ascii() const;
  CString latin1() const;
  CString utf8(UTF8ConversionMode = LenientUTF8Conversion) const;

  UChar operator[](unsigned index) const {
    if (!m_impl || index >= m_impl->length())
      return 0;
    return (*m_impl)[index];
  }

  static String number(int);
  static String number(unsigned);
  static String number(long);
  static String number(unsigned long);
  static String number(long long);
  static String number(unsigned long long);

  static String number(double, unsigned precision = 6);

  // Number to String conversion following the ECMAScript definition.
  static String numberToStringECMAScript(double);
  static String numberToStringFixedWidth(double, unsigned decimalPlaces);

  // Find characters.
  size_t find(UChar c, unsigned start = 0) const {
    return m_impl ? m_impl->find(c, start) : kNotFound;
  }
  size_t find(LChar c, unsigned start = 0) const {
    return m_impl ? m_impl->find(c, start) : kNotFound;
  }
  size_t find(char c, unsigned start = 0) const {
    return find(static_cast<LChar>(c), start);
  }
  size_t find(CharacterMatchFunctionPtr matchFunction,
              unsigned start = 0) const {
    return m_impl ? m_impl->find(matchFunction, start) : kNotFound;
  }

  // Find substrings.
  size_t find(const StringView& value,
              unsigned start = 0,
              TextCaseSensitivity caseSensitivity = TextCaseSensitive) const {
    return m_impl
               ? DISPATCH_CASE_OP(caseSensitivity, m_impl->find, (value, start))
               : kNotFound;
  }

  // Unicode aware case insensitive string matching. Non-ASCII characters might
  // match to ASCII characters. This function is rarely used to implement web
  // platform features.
  size_t findIgnoringCase(const StringView& value, unsigned start = 0) const {
    return m_impl ? m_impl->findIgnoringCase(value, start) : kNotFound;
  }

  // ASCII case insensitive string matching.
  size_t findIgnoringASCIICase(const StringView& value,
                               unsigned start = 0) const {
    return m_impl ? m_impl->findIgnoringASCIICase(value, start) : kNotFound;
  }

  bool contains(char c) const { return find(c) != kNotFound; }
  bool contains(const StringView& value,
                TextCaseSensitivity caseSensitivity = TextCaseSensitive) const {
    return find(value, 0, caseSensitivity) != kNotFound;
  }

  // Find the last instance of a single character or string.
  size_t reverseFind(UChar c, unsigned start = UINT_MAX) const {
    return m_impl ? m_impl->reverseFind(c, start) : kNotFound;
  }
  size_t reverseFind(const StringView& value, unsigned start = UINT_MAX) const {
    return m_impl ? m_impl->reverseFind(value, start) : kNotFound;
  }

  UChar32 characterStartingAt(unsigned) const;

  bool startsWith(
      const StringView& prefix,
      TextCaseSensitivity caseSensitivity = TextCaseSensitive) const {
    return m_impl
               ? DISPATCH_CASE_OP(caseSensitivity, m_impl->startsWith, (prefix))
               : prefix.isEmpty();
  }
  bool startsWith(UChar character) const {
    return m_impl ? m_impl->startsWith(character) : false;
  }

  bool endsWith(const StringView& suffix,
                TextCaseSensitivity caseSensitivity = TextCaseSensitive) const {
    return m_impl
               ? DISPATCH_CASE_OP(caseSensitivity, m_impl->endsWith, (suffix))
               : suffix.isEmpty();
  }
  bool endsWith(UChar character) const {
    return m_impl ? m_impl->endsWith(character) : false;
  }

  void append(const StringView&);
  void append(LChar);
  void append(char c) { append(static_cast<LChar>(c)); }
  void append(UChar);
  void insert(const StringView&, unsigned pos);

  // TODO(esprehn): replace strangely both modifies this String *and* return a
  // value. It should only do one of those.
  String& replace(UChar pattern, UChar replacement) {
    if (m_impl)
      m_impl = m_impl->replace(pattern, replacement);
    return *this;
  }
  String& replace(UChar pattern, const StringView& replacement) {
    if (m_impl)
      m_impl = m_impl->replace(pattern, replacement);
    return *this;
  }
  String& replace(const StringView& pattern, const StringView& replacement) {
    if (m_impl)
      m_impl = m_impl->replace(pattern, replacement);
    return *this;
  }
  String& replace(unsigned index,
                  unsigned lengthToReplace,
                  const StringView& replacement) {
    if (m_impl)
      m_impl = m_impl->replace(index, lengthToReplace, replacement);
    return *this;
  }

  void fill(UChar c) {
    if (m_impl)
      m_impl = m_impl->fill(c);
  }

  void ensure16Bit();

  void truncate(unsigned length);
  void remove(unsigned start, unsigned length = 1);

  String substring(unsigned pos, unsigned len = UINT_MAX) const;
  String left(unsigned len) const { return substring(0, len); }
  String right(unsigned len) const { return substring(length() - len, len); }

  // Returns a lowercase/uppercase version of the string. These functions might
  // convert non-ASCII characters to ASCII characters. For example, lower() for
  // U+212A is 'k', upper() for U+017F is 'S'.
  // These functions are rarely used to implement web platform features.
  String lower() const;
  String upper() const;

  String lower(const AtomicString& localeIdentifier) const;
  String upper(const AtomicString& localeIdentifier) const;

  // Returns a uppercase version of the string.
  // This function converts ASCII characters only.
  String upperASCII() const;

  String stripWhiteSpace() const;
  String stripWhiteSpace(IsWhiteSpaceFunctionPtr) const;
  String simplifyWhiteSpace(StripBehavior = StripExtraWhiteSpace) const;
  String simplifyWhiteSpace(IsWhiteSpaceFunctionPtr,
                            StripBehavior = StripExtraWhiteSpace) const;

  String removeCharacters(CharacterMatchFunctionPtr) const;
  template <bool isSpecialCharacter(UChar)>
  bool isAllSpecialCharacters() const;

  // Return the string with case folded for case insensitive comparison.
  String foldCase() const;

  // Takes a printf format and args and prints into a String.
  PRINTF_FORMAT(1, 2) static String format(const char* format, ...);

  // Returns an uninitialized string. The characters needs to be written
  // into the buffer returned in data before the returned string is used.
  // Failure to do this will have unpredictable results.
  static String createUninitialized(unsigned length, UChar*& data) {
    return StringImpl::createUninitialized(length, data);
  }
  static String createUninitialized(unsigned length, LChar*& data) {
    return StringImpl::createUninitialized(length, data);
  }

  void split(const StringView& separator,
             bool allowEmptyEntries,
             Vector<String>& result) const;
  void split(const StringView& separator, Vector<String>& result) const {
    split(separator, false, result);
  }
  void split(UChar separator,
             bool allowEmptyEntries,
             Vector<String>& result) const;
  void split(UChar separator, Vector<String>& result) const {
    split(separator, false, result);
  }

  // Copy characters out of the string. See StringImpl.h for detailed docs.
  unsigned copyTo(UChar* buffer, unsigned start, unsigned maxLength) const {
    return m_impl ? m_impl->copyTo(buffer, start, maxLength) : 0;
  }
  template <typename BufferType>
  void appendTo(BufferType&,
                unsigned start = 0,
                unsigned length = UINT_MAX) const;
  template <typename BufferType>
  void prependTo(BufferType&,
                 unsigned start = 0,
                 unsigned length = UINT_MAX) const;

  // Convert the string into a number.

  int toIntStrict(bool* ok = 0, int base = 10) const;
  unsigned toUIntStrict(bool* ok = 0, int base = 10) const;
  int64_t toInt64Strict(bool* ok = 0, int base = 10) const;
  uint64_t toUInt64Strict(bool* ok = 0, int base = 10) const;

  int toInt(bool* ok = 0) const;
  unsigned toUInt(bool* ok = 0) const;
  int64_t toInt64(bool* ok = 0) const;
  uint64_t toUInt64(bool* ok = 0) const;

  // FIXME: Like the strict functions above, these give false for "ok" when
  // there is trailing garbage.  Like the non-strict functions above, these
  // return the value when there is trailing garbage.  It would be better if
  // these were more consistent with the above functions instead.
  double toDouble(bool* ok = 0) const;
  float toFloat(bool* ok = 0) const;

  String isolatedCopy() const;
  bool isSafeToSendToAnotherThread() const;

#ifdef __OBJC__
  String(NSString*);

  // This conversion maps null string to "", which loses the meaning of null
  // string, but we need this mapping because AppKit crashes when passed nil
  // NSStrings.
  operator NSString*() const {
    if (!m_impl)
      return @"";
    return *m_impl;
  }
#endif

  static String make8BitFrom16BitSource(const UChar*, size_t);
  template <size_t inlineCapacity>
  static String make8BitFrom16BitSource(
      const Vector<UChar, inlineCapacity>& buffer) {
    return make8BitFrom16BitSource(buffer.data(), buffer.size());
  }

  static String make16BitFrom8BitSource(const LChar*, size_t);

  // String::fromUTF8 will return a null string if
  // the input data contains invalid UTF-8 sequences.
  static String fromUTF8(const LChar*, size_t);
  static String fromUTF8(const LChar*);
  static String fromUTF8(const char* s, size_t length) {
    return fromUTF8(reinterpret_cast<const LChar*>(s), length);
  }
  static String fromUTF8(const char* s) {
    return fromUTF8(reinterpret_cast<const LChar*>(s));
  }
  static String fromUTF8(const CString&);

  // Tries to convert the passed in string to UTF-8, but will fall back to
  // Latin-1 if the string is not valid UTF-8.
  static String fromUTF8WithLatin1Fallback(const LChar*, size_t);
  static String fromUTF8WithLatin1Fallback(const char* s, size_t length) {
    return fromUTF8WithLatin1Fallback(reinterpret_cast<const LChar*>(s),
                                      length);
  }

  bool containsOnlyASCII() const {
    return !m_impl || m_impl->containsOnlyASCII();
  }
  bool containsOnlyLatin1() const;
  bool containsOnlyWhitespace() const {
    return !m_impl || m_impl->containsOnlyWhitespace();
  }

  size_t charactersSizeInBytes() const {
    return m_impl ? m_impl->charactersSizeInBytes() : 0;
  }

  // Hash table deleted values, which are only constructed and never copied or
  // destroyed.
  String(WTF::HashTableDeletedValueType) : m_impl(WTF::HashTableDeletedValue) {}
  bool isHashTableDeletedValue() const {
    return m_impl.isHashTableDeletedValue();
  }

#ifndef NDEBUG
  // For use in the debugger.
  void show() const;
#endif

 private:
  template <typename CharacterType>
  void appendInternal(CharacterType);

  RefPtr<StringImpl> m_impl;
};

#undef DISPATCH_CASE_OP

inline bool operator==(const String& a, const String& b) {
  // We don't use equalStringView here since we want the isAtomic() fast path
  // inside WTF::equal.
  return equal(a.impl(), b.impl());
}
inline bool operator==(const String& a, const char* b) {
  return equalStringView(a, b);
}
inline bool operator==(const char* a, const String& b) {
  return b == a;
}

inline bool operator!=(const String& a, const String& b) {
  return !(a == b);
}
inline bool operator!=(const String& a, const char* b) {
  return !(a == b);
}
inline bool operator!=(const char* a, const String& b) {
  return !(a == b);
}

inline bool equalPossiblyIgnoringCase(const String& a,
                                      const String& b,
                                      bool ignoreCase) {
  return ignoreCase ? equalIgnoringCase(a, b) : (a == b);
}

inline bool equalIgnoringNullity(const String& a, const String& b) {
  return equalIgnoringNullity(a.impl(), b.impl());
}

template <size_t inlineCapacity>
inline bool equalIgnoringNullity(const Vector<UChar, inlineCapacity>& a,
                                 const String& b) {
  return equalIgnoringNullity(a, b.impl());
}

inline void swap(String& a, String& b) {
  a.swap(b);
}

// Definitions of string operations

template <size_t inlineCapacity>
String::String(const Vector<UChar, inlineCapacity>& vector)
    : m_impl(vector.size() ? StringImpl::create(vector.data(), vector.size())
                           : StringImpl::empty) {}

template <>
inline const LChar* String::getCharacters<LChar>() const {
  DCHECK(is8Bit());
  return characters8();
}

template <>
inline const UChar* String::getCharacters<UChar>() const {
  DCHECK(!is8Bit());
  return characters16();
}

inline bool String::containsOnlyLatin1() const {
  if (isEmpty())
    return true;

  if (is8Bit())
    return true;

  const UChar* characters = characters16();
  UChar ored = 0;
  for (size_t i = 0; i < m_impl->length(); ++i)
    ored |= characters[i];
  return !(ored & 0xFF00);
}

#ifdef __OBJC__
// This is for situations in WebKit where the long standing behavior has been
// "nil if empty", so we try to maintain longstanding behavior for the sake of
// entrenched clients
inline NSString* nsStringNilIfEmpty(const String& str) {
  return str.isEmpty() ? nil : (NSString*)str;
}
#endif

WTF_EXPORT int codePointCompare(const String&, const String&);

inline bool codePointCompareLessThan(const String& a, const String& b) {
  return codePointCompare(a.impl(), b.impl()) < 0;
}

WTF_EXPORT int codePointCompareIgnoringASCIICase(const String&, const char*);

template <bool isSpecialCharacter(UChar)>
inline bool String::isAllSpecialCharacters() const {
  return StringView(*this).isAllSpecialCharacters<isSpecialCharacter>();
}

template <typename BufferType>
void String::appendTo(BufferType& result,
                      unsigned position,
                      unsigned length) const {
  if (!m_impl)
    return;
  m_impl->appendTo(result, position, length);
}

template <typename BufferType>
void String::prependTo(BufferType& result,
                       unsigned position,
                       unsigned length) const {
  if (!m_impl)
    return;
  m_impl->prependTo(result, position, length);
}

// StringHash is the default hash for String
template <typename T>
struct DefaultHash;
template <>
struct DefaultHash<String> {
  typedef StringHash Hash;
};

// Shared global empty string.
WTF_EXPORT extern const String& emptyString;
WTF_EXPORT extern const String& emptyString16Bit;
WTF_EXPORT extern const String& xmlnsWithColon;

// Pretty printer for gtest and base/logging.*.  It prepends and appends
// double-quotes, and escapes chracters other than ASCII printables.
WTF_EXPORT std::ostream& operator<<(std::ostream&, const String&);

inline StringView::StringView(const String& string,
                              unsigned offset,
                              unsigned length)
    : StringView(string.impl(), offset, length) {}
inline StringView::StringView(const String& string, unsigned offset)
    : StringView(string.impl(), offset) {}
inline StringView::StringView(const String& string)
    : StringView(string.impl()) {}

}  // namespace WTF

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(String);

using WTF::CString;
using WTF::StrictUTF8Conversion;
using WTF::StrictUTF8ConversionReplacingUnpairedSurrogatesWithFFFD;
using WTF::String;
using WTF::emptyString;
using WTF::emptyString16Bit;
using WTF::charactersAreAllASCII;
using WTF::equal;
using WTF::find;
using WTF::isSpaceOrNewline;

#include "wtf/text/AtomicString.h"
#endif  // WTFString_h
