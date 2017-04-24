/*
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef AtomicString_h
#define AtomicString_h

#include "wtf/Allocator.h"
#include "wtf/HashTableDeletedValueType.h"
#include "wtf/WTFExport.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringView.h"
#include "wtf/text/WTFString.h"
#include <cstring>
#include <iosfwd>

namespace WTF {

struct AtomicStringHash;

// An AtomicString instance represents a string, and multiple AtomicString
// instances can share their string storage if the strings are
// identical. Comparing two AtomicString instances is much faster than comparing
// two String instances because we just check string storage identity.
//
// AtomicString instances are not thread-safe. An AtomicString instance created
// in a thread must be used only in the creator thread.  If multiple threads
// access a single AtomicString instance, we have race condition of a reference
// count in StringImpl, and would hit a runtime CHECK in
// AtomicStringTable::remove().
//
// Exception: nullAtom and emptyAtom, are shared in multiple threads, and are
// never stored in AtomicStringTable.
class WTF_EXPORT AtomicString {
  USING_FAST_MALLOC(AtomicString);

 public:
  // The function is defined in StringStatics.cpp.
  static void init();

  AtomicString() {}
  AtomicString(const LChar* chars)
      : AtomicString(chars,
                     chars ? strlen(reinterpret_cast<const char*>(chars)) : 0) {
  }
  AtomicString(const char* chars)
      : AtomicString(reinterpret_cast<const LChar*>(chars)) {}
  AtomicString(const LChar* chars, unsigned length);
  AtomicString(const UChar* chars, unsigned length);
  AtomicString(const UChar* chars);
  AtomicString(const char16_t* chars)
      : AtomicString(reinterpret_cast<const UChar*>(chars)) {}

  template <size_t inlineCapacity>
  explicit AtomicString(const Vector<UChar, inlineCapacity>& vector)
      : AtomicString(vector.data(), vector.size()) {}

  // Constructing an AtomicString from a String / StringImpl can be expensive if
  // the StringImpl is not already atomic.
  explicit AtomicString(StringImpl* impl) : m_string(add(impl)) {}
  explicit AtomicString(const String& s) : m_string(add(s.impl())) {}

  // Hash table deleted values, which are only constructed and never copied or
  // destroyed.
  AtomicString(WTF::HashTableDeletedValueType)
      : m_string(WTF::HashTableDeletedValue) {}
  bool isHashTableDeletedValue() const {
    return m_string.isHashTableDeletedValue();
  }

  explicit operator bool() const { return !isNull(); }
  operator const String&() const { return m_string; }
  const String& getString() const { return m_string; }

  StringImpl* impl() const { return m_string.impl(); }

  bool is8Bit() const { return m_string.is8Bit(); }
  const LChar* characters8() const { return m_string.characters8(); }
  const UChar* characters16() const { return m_string.characters16(); }
  unsigned length() const { return m_string.length(); }

  UChar operator[](unsigned i) const { return m_string[i]; }

  // Find characters.
  size_t find(UChar c, unsigned start = 0) const {
    return m_string.find(c, start);
  }
  size_t find(LChar c, unsigned start = 0) const {
    return m_string.find(c, start);
  }
  size_t find(char c, unsigned start = 0) const {
    return find(static_cast<LChar>(c), start);
  }
  size_t find(CharacterMatchFunctionPtr matchFunction,
              unsigned start = 0) const {
    return m_string.find(matchFunction, start);
  }

  // Find substrings.
  size_t find(const StringView& value,
              unsigned start = 0,
              TextCaseSensitivity caseSensitivity = TextCaseSensitive) const {
    return m_string.find(value, start, caseSensitivity);
  }

  // Unicode aware case insensitive string matching. Non-ASCII characters might
  // match to ASCII characters. This function is rarely used to implement web
  // platform features.
  size_t findIgnoringCase(const StringView& value, unsigned start = 0) const {
    return m_string.findIgnoringCase(value, start);
  }

  // ASCII case insensitive string matching.
  size_t findIgnoringASCIICase(const StringView& value,
                               unsigned start = 0) const {
    return m_string.findIgnoringASCIICase(value, start);
  }

  bool contains(char c) const { return find(c) != kNotFound; }
  bool contains(const StringView& value,
                TextCaseSensitivity caseSensitivity = TextCaseSensitive) const {
    return find(value, 0, caseSensitivity) != kNotFound;
  }

  // Find the last instance of a single character or string.
  size_t reverseFind(UChar c, unsigned start = UINT_MAX) const {
    return m_string.reverseFind(c, start);
  }
  size_t reverseFind(const StringView& value, unsigned start = UINT_MAX) const {
    return m_string.reverseFind(value, start);
  }

  bool startsWith(
      const StringView& prefix,
      TextCaseSensitivity caseSensitivity = TextCaseSensitive) const {
    return m_string.startsWith(prefix, caseSensitivity);
  }
  bool startsWith(UChar character) const {
    return m_string.startsWith(character);
  }

  bool endsWith(const StringView& suffix,
                TextCaseSensitivity caseSensitivity = TextCaseSensitive) const {
    return m_string.endsWith(suffix, caseSensitivity);
  }
  bool endsWith(UChar character) const { return m_string.endsWith(character); }

  // Returns a lowercase version of the string. This function might
  // convert non-ASCII characters to ASCII characters. For example,
  // lower() for U+212A is 'k'.
  // This function is rarely used to implement web platform features.
  AtomicString lower() const;

  // Returns a lowercase/uppercase version of the string.
  // These functions convert ASCII characters only.
  AtomicString lowerASCII() const;
  AtomicString upperASCII() const;

  int toInt(bool* ok = 0) const { return m_string.toInt(ok); }
  double toDouble(bool* ok = 0) const { return m_string.toDouble(ok); }
  float toFloat(bool* ok = 0) const { return m_string.toFloat(ok); }

  static AtomicString number(int);
  static AtomicString number(unsigned);
  static AtomicString number(long);
  static AtomicString number(unsigned long);
  static AtomicString number(long long);
  static AtomicString number(unsigned long long);

  static AtomicString number(double, unsigned precision = 6);

  bool isNull() const { return m_string.isNull(); }
  bool isEmpty() const { return m_string.isEmpty(); }

#ifdef __OBJC__
  AtomicString(NSString* s) : m_string(add((CFStringRef)s)) {}
  operator NSString*() const { return m_string; }
#endif
  // AtomicString::fromUTF8 will return a null string if
  // the input data contains invalid UTF-8 sequences.
  // NOTE: Passing a zero size means use the whole string.
  static AtomicString fromUTF8(const char*, size_t length);
  static AtomicString fromUTF8(const char*);

  CString ascii() const { return m_string.ascii(); }
  CString latin1() const { return m_string.latin1(); }
  CString utf8(UTF8ConversionMode mode = LenientUTF8Conversion) const {
    return m_string.utf8(mode);
  }

  size_t charactersSizeInBytes() const {
    return m_string.charactersSizeInBytes();
  }

  bool isSafeToSendToAnotherThread() const {
    return m_string.isSafeToSendToAnotherThread();
  }

#ifndef NDEBUG
  void show() const;
#endif

 private:
  String m_string;

  ALWAYS_INLINE static PassRefPtr<StringImpl> add(StringImpl* r) {
    if (!r || r->isAtomic())
      return r;
    return addSlowCase(r);
  }
  static PassRefPtr<StringImpl> addSlowCase(StringImpl*);
#if OS(MACOSX)
  static PassRefPtr<StringImpl> add(CFStringRef);
#endif
};

inline bool operator==(const AtomicString& a, const AtomicString& b) {
  return a.impl() == b.impl();
}
inline bool operator==(const AtomicString& a, const String& b) {
  // We don't use equalStringView so we get the isAtomic() optimization inside
  // WTF::equal.
  return equal(a.impl(), b.impl());
}
inline bool operator==(const String& a, const AtomicString& b) {
  return b == a;
}
inline bool operator==(const AtomicString& a, const char* b) {
  return equalStringView(a, b);
}
inline bool operator==(const char* a, const AtomicString& b) {
  return b == a;
}

inline bool operator!=(const AtomicString& a, const AtomicString& b) {
  return a.impl() != b.impl();
}
inline bool operator!=(const AtomicString& a, const String& b) {
  return !(a == b);
}
inline bool operator!=(const String& a, const AtomicString& b) {
  return !(a == b);
}
inline bool operator!=(const AtomicString& a, const char* b) {
  return !(a == b);
}
inline bool operator!=(const char* a, const AtomicString& b) {
  return !(a == b);
}

// Define external global variables for the commonly used atomic strings.
// These are only usable from the main thread.
WTF_EXPORT extern const AtomicString& nullAtom;
WTF_EXPORT extern const AtomicString& emptyAtom;
WTF_EXPORT extern const AtomicString& starAtom;
WTF_EXPORT extern const AtomicString& xmlAtom;
WTF_EXPORT extern const AtomicString& xmlnsAtom;
WTF_EXPORT extern const AtomicString& xlinkAtom;
WTF_EXPORT extern const AtomicString& httpAtom;
WTF_EXPORT extern const AtomicString& httpsAtom;

// AtomicStringHash is the default hash for AtomicString
template <typename T>
struct DefaultHash;
template <>
struct DefaultHash<AtomicString> {
  typedef AtomicStringHash Hash;
};

// Pretty printer for gtest and base/logging.*.  It prepends and appends
// double-quotes, and escapes chracters other than ASCII printables.
WTF_EXPORT std::ostream& operator<<(std::ostream&, const AtomicString&);

inline StringView::StringView(const AtomicString& string,
                              unsigned offset,
                              unsigned length)
    : StringView(string.impl(), offset, length) {}
inline StringView::StringView(const AtomicString& string, unsigned offset)
    : StringView(string.impl(), offset) {}
inline StringView::StringView(const AtomicString& string)
    : StringView(string.impl()) {}

}  // namespace WTF

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(AtomicString);

using WTF::AtomicString;
using WTF::nullAtom;
using WTF::emptyAtom;
using WTF::starAtom;
using WTF::xmlAtom;
using WTF::xmlnsAtom;
using WTF::xlinkAtom;

#include "wtf/text/StringConcatenate.h"
#endif  // AtomicString_h
