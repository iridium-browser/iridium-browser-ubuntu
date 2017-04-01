/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010, 2012 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
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
 */

#include "wtf/text/WTFString.h"

#include "base/strings/string_util.h"
#include "wtf/ASCIICType.h"
#include "wtf/DataLog.h"
#include "wtf/HexNumber.h"
#include "wtf/MathExtras.h"
#include "wtf/StringExtras.h"
#include "wtf/Vector.h"
#include "wtf/dtoa.h"
#include "wtf/text/CString.h"
#include "wtf/text/CharacterNames.h"
#include "wtf/text/IntegerToStringConversion.h"
#include "wtf/text/UTF8.h"
#include "wtf/text/Unicode.h"
#include <algorithm>
#include <stdarg.h>

namespace WTF {

using namespace Unicode;

namespace {

Vector<char> asciiDebug(StringImpl* impl) {
  if (!impl)
    return asciiDebug(String("[null]").impl());

  Vector<char> buffer;
  for (unsigned i = 0; i < impl->length(); ++i) {
    UChar ch = (*impl)[i];
    if (isASCIIPrintable(ch)) {
      if (ch == '\\')
        buffer.push_back('\\');
      buffer.push_back(static_cast<char>(ch));
    } else {
      buffer.push_back('\\');
      buffer.push_back('u');
      appendUnsignedAsHexFixedSize(ch, buffer, 4);
    }
  }
  buffer.push_back('\0');
  return buffer;
}

}  // namespace

// Construct a string with UTF-16 data.
String::String(const UChar* characters, unsigned length)
    : m_impl(characters ? StringImpl::create(characters, length) : nullptr) {}

// Construct a string with UTF-16 data, from a null-terminated source.
String::String(const UChar* str) {
  if (!str)
    return;
  m_impl = StringImpl::create(str, lengthOfNullTerminatedString(str));
}

// Construct a string with latin1 data.
String::String(const LChar* characters, unsigned length)
    : m_impl(characters ? StringImpl::create(characters, length) : nullptr) {}

String::String(const char* characters, unsigned length)
    : m_impl(characters ? StringImpl::create(
                              reinterpret_cast<const LChar*>(characters),
                              length)
                        : nullptr) {}

// Construct a string with latin1 data, from a null-terminated source.
String::String(const LChar* characters)
    : m_impl(characters ? StringImpl::create(characters) : nullptr) {}

String::String(const char* characters)
    : m_impl(characters ? StringImpl::create(
                              reinterpret_cast<const LChar*>(characters))
                        : nullptr) {}

void String::append(const StringView& string) {
  if (string.isEmpty())
    return;
  if (!m_impl) {
    m_impl = string.toString().releaseImpl();
    return;
  }

  // FIXME: This is extremely inefficient. So much so that we might want to
  // take this out of String's API. We can make it better by optimizing the
  // case where exactly one String is pointing at this StringImpl, but even
  // then it's going to require a call into the allocator every single time.

  if (m_impl->is8Bit() && string.is8Bit()) {
    LChar* data;
    RELEASE_ASSERT(string.length() <=
                   std::numeric_limits<unsigned>::max() - m_impl->length());
    RefPtr<StringImpl> newImpl = StringImpl::createUninitialized(
        m_impl->length() + string.length(), data);
    memcpy(data, m_impl->characters8(), m_impl->length() * sizeof(LChar));
    memcpy(data + m_impl->length(), string.characters8(),
           string.length() * sizeof(LChar));
    m_impl = newImpl.release();
    return;
  }

  UChar* data;
  RELEASE_ASSERT(string.length() <=
                 std::numeric_limits<unsigned>::max() - m_impl->length());
  RefPtr<StringImpl> newImpl =
      StringImpl::createUninitialized(m_impl->length() + string.length(), data);

  if (m_impl->is8Bit())
    StringImpl::copyChars(data, m_impl->characters8(), m_impl->length());
  else
    StringImpl::copyChars(data, m_impl->characters16(), m_impl->length());

  if (string.is8Bit())
    StringImpl::copyChars(data + m_impl->length(), string.characters8(),
                          string.length());
  else
    StringImpl::copyChars(data + m_impl->length(), string.characters16(),
                          string.length());

  m_impl = newImpl.release();
}

template <typename CharacterType>
inline void String::appendInternal(CharacterType c) {
  // FIXME: This is extremely inefficient. So much so that we might want to
  // take this out of String's API. We can make it better by optimizing the
  // case where exactly one String is pointing at this StringImpl, but even
  // then it's going to require a call into the allocator every single time.
  if (!m_impl) {
    m_impl = StringImpl::create(&c, 1);
    return;
  }

  // FIXME: We should be able to create an 8 bit string via this code path.
  UChar* data;
  RELEASE_ASSERT(m_impl->length() < std::numeric_limits<unsigned>::max());
  RefPtr<StringImpl> newImpl =
      StringImpl::createUninitialized(m_impl->length() + 1, data);
  if (m_impl->is8Bit())
    StringImpl::copyChars(data, m_impl->characters8(), m_impl->length());
  else
    StringImpl::copyChars(data, m_impl->characters16(), m_impl->length());
  data[m_impl->length()] = c;
  m_impl = newImpl.release();
}

void String::append(LChar c) {
  appendInternal(c);
}

void String::append(UChar c) {
  appendInternal(c);
}

int codePointCompare(const String& a, const String& b) {
  return codePointCompare(a.impl(), b.impl());
}

int codePointCompareIgnoringASCIICase(const String& a, const char* b) {
  return codePointCompareIgnoringASCIICase(a.impl(),
                                           reinterpret_cast<const LChar*>(b));
}

template <typename CharType>
PassRefPtr<StringImpl> insertInternal(PassRefPtr<StringImpl> impl,
                                      const CharType* charactersToInsert,
                                      unsigned lengthToInsert,
                                      unsigned position) {
  if (!lengthToInsert)
    return impl;

  DCHECK(charactersToInsert);
  UChar* data;  // FIXME: We should be able to create an 8 bit string here.
  RELEASE_ASSERT(lengthToInsert <=
                 std::numeric_limits<unsigned>::max() - impl->length());
  RefPtr<StringImpl> newImpl =
      StringImpl::createUninitialized(impl->length() + lengthToInsert, data);

  if (impl->is8Bit())
    StringImpl::copyChars(data, impl->characters8(), position);
  else
    StringImpl::copyChars(data, impl->characters16(), position);

  StringImpl::copyChars(data + position, charactersToInsert, lengthToInsert);

  if (impl->is8Bit())
    StringImpl::copyChars(data + position + lengthToInsert,
                          impl->characters8() + position,
                          impl->length() - position);
  else
    StringImpl::copyChars(data + position + lengthToInsert,
                          impl->characters16() + position,
                          impl->length() - position);

  return newImpl.release();
}

void String::insert(const StringView& string, unsigned position) {
  if (string.isEmpty()) {
    if (string.isNull())
      return;
    if (isNull())
      m_impl = string.toString().releaseImpl();
    return;
  }

  if (position >= length()) {
    if (string.is8Bit())
      append(string);
    else
      append(string);
    return;
  }

  DCHECK(m_impl);
  if (string.is8Bit())
    m_impl = insertInternal(m_impl.release(), string.characters8(),
                            string.length(), position);
  else
    m_impl = insertInternal(m_impl.release(), string.characters16(),
                            string.length(), position);
}

UChar32 String::characterStartingAt(unsigned i) const {
  if (!m_impl || i >= m_impl->length())
    return 0;
  return m_impl->characterStartingAt(i);
}

void String::ensure16Bit() {
  if (isNull())
    return;
  if (!is8Bit())
    return;
  if (unsigned length = this->length())
    m_impl =
        make16BitFrom8BitSource(m_impl->characters8(), length).releaseImpl();
  else
    m_impl = StringImpl::empty16Bit();
}

void String::truncate(unsigned length) {
  if (m_impl)
    m_impl = m_impl->truncate(length);
}

void String::remove(unsigned start, unsigned lengthToRemove) {
  if (m_impl)
    m_impl = m_impl->remove(start, lengthToRemove);
}

String String::substring(unsigned pos, unsigned len) const {
  if (!m_impl)
    return String();
  return m_impl->substring(pos, len);
}

String String::lower() const {
  if (!m_impl)
    return String();
  return m_impl->lower();
}

String String::upper() const {
  if (!m_impl)
    return String();
  return m_impl->upper();
}

String String::lower(const AtomicString& localeIdentifier) const {
  if (!m_impl)
    return String();
  return m_impl->lower(localeIdentifier);
}

String String::upper(const AtomicString& localeIdentifier) const {
  if (!m_impl)
    return String();
  return m_impl->upper(localeIdentifier);
}

String String::stripWhiteSpace() const {
  if (!m_impl)
    return String();
  return m_impl->stripWhiteSpace();
}

String String::stripWhiteSpace(IsWhiteSpaceFunctionPtr isWhiteSpace) const {
  if (!m_impl)
    return String();
  return m_impl->stripWhiteSpace(isWhiteSpace);
}

String String::simplifyWhiteSpace(StripBehavior stripBehavior) const {
  if (!m_impl)
    return String();
  return m_impl->simplifyWhiteSpace(stripBehavior);
}

String String::simplifyWhiteSpace(IsWhiteSpaceFunctionPtr isWhiteSpace,
                                  StripBehavior stripBehavior) const {
  if (!m_impl)
    return String();
  return m_impl->simplifyWhiteSpace(isWhiteSpace, stripBehavior);
}

String String::removeCharacters(CharacterMatchFunctionPtr findMatch) const {
  if (!m_impl)
    return String();
  return m_impl->removeCharacters(findMatch);
}

String String::foldCase() const {
  if (!m_impl)
    return String();
  return m_impl->foldCase();
}

String String::format(const char* format, ...) {
  va_list args;

  // TODO(esprehn): base uses 1024, maybe we should use a bigger size too.
  static const unsigned kDefaultSize = 256;
  Vector<char, kDefaultSize> buffer(kDefaultSize);

  va_start(args, format);
  int length = base::vsnprintf(buffer.data(), buffer.size(), format, args);
  va_end(args);

  // TODO(esprehn): This can only happen if there's an encoding error, what's
  // the locale set to inside blink? Can this happen? We should probably CHECK
  // instead.
  if (length < 0)
    return String();

  if (static_cast<unsigned>(length) >= buffer.size()) {
    // vsnprintf doesn't include the NUL terminator in the length so we need to
    // add space for it when growing.
    buffer.grow(length + 1);

    // We need to call va_end() and then va_start() each time we use args, as
    // the contents of args is undefined after the call to vsnprintf according
    // to http://man.cx/snprintf(3)
    //
    // Not calling va_end/va_start here happens to work on lots of systems, but
    // fails e.g. on 64bit Linux.
    va_start(args, format);
    length = base::vsnprintf(buffer.data(), buffer.size(), format, args);
    va_end(args);
  }

  CHECK_LT(static_cast<unsigned>(length), buffer.size());
  return String(reinterpret_cast<const LChar*>(buffer.data()), length);
}

template <typename IntegerType>
static String integerToString(IntegerType input) {
  IntegerToStringConverter<IntegerType> converter(input);
  return StringImpl::create(converter.characters8(), converter.length());
}

String String::number(int number) {
  return integerToString(number);
}

String String::number(unsigned number) {
  return integerToString(number);
}

String String::number(long number) {
  return integerToString(number);
}

String String::number(unsigned long number) {
  return integerToString(number);
}

String String::number(long long number) {
  return integerToString(number);
}

String String::number(unsigned long long number) {
  return integerToString(number);
}

String String::number(double number, unsigned precision) {
  NumberToStringBuffer buffer;
  return String(numberToFixedPrecisionString(number, precision, buffer));
}

String String::numberToStringECMAScript(double number) {
  NumberToStringBuffer buffer;
  return String(numberToString(number, buffer));
}

String String::numberToStringFixedWidth(double number, unsigned decimalPlaces) {
  NumberToStringBuffer buffer;
  return String(numberToFixedWidthString(number, decimalPlaces, buffer));
}

int String::toIntStrict(bool* ok, int base) const {
  if (!m_impl) {
    if (ok)
      *ok = false;
    return 0;
  }
  return m_impl->toIntStrict(ok, base);
}

unsigned String::toUIntStrict(bool* ok, int base) const {
  if (!m_impl) {
    if (ok)
      *ok = false;
    return 0;
  }
  return m_impl->toUIntStrict(ok, base);
}

int64_t String::toInt64Strict(bool* ok, int base) const {
  if (!m_impl) {
    if (ok)
      *ok = false;
    return 0;
  }
  return m_impl->toInt64Strict(ok, base);
}

uint64_t String::toUInt64Strict(bool* ok, int base) const {
  if (!m_impl) {
    if (ok)
      *ok = false;
    return 0;
  }
  return m_impl->toUInt64Strict(ok, base);
}

int String::toInt(bool* ok) const {
  if (!m_impl) {
    if (ok)
      *ok = false;
    return 0;
  }
  return m_impl->toInt(ok);
}

unsigned String::toUInt(bool* ok) const {
  if (!m_impl) {
    if (ok)
      *ok = false;
    return 0;
  }
  return m_impl->toUInt(ok);
}

int64_t String::toInt64(bool* ok) const {
  if (!m_impl) {
    if (ok)
      *ok = false;
    return 0;
  }
  return m_impl->toInt64(ok);
}

uint64_t String::toUInt64(bool* ok) const {
  if (!m_impl) {
    if (ok)
      *ok = false;
    return 0;
  }
  return m_impl->toUInt64(ok);
}

double String::toDouble(bool* ok) const {
  if (!m_impl) {
    if (ok)
      *ok = false;
    return 0.0;
  }
  return m_impl->toDouble(ok);
}

float String::toFloat(bool* ok) const {
  if (!m_impl) {
    if (ok)
      *ok = false;
    return 0.0f;
  }
  return m_impl->toFloat(ok);
}

String String::isolatedCopy() const {
  if (!m_impl)
    return String();
  return m_impl->isolatedCopy();
}

bool String::isSafeToSendToAnotherThread() const {
  return !m_impl || m_impl->isSafeToSendToAnotherThread();
}

void String::split(const StringView& separator,
                   bool allowEmptyEntries,
                   Vector<String>& result) const {
  result.clear();

  unsigned startPos = 0;
  size_t endPos;
  while ((endPos = find(separator, startPos)) != kNotFound) {
    if (allowEmptyEntries || startPos != endPos)
      result.push_back(substring(startPos, endPos - startPos));
    startPos = endPos + separator.length();
  }
  if (allowEmptyEntries || startPos != length())
    result.push_back(substring(startPos));
}

void String::split(UChar separator,
                   bool allowEmptyEntries,
                   Vector<String>& result) const {
  result.clear();

  unsigned startPos = 0;
  size_t endPos;
  while ((endPos = find(separator, startPos)) != kNotFound) {
    if (allowEmptyEntries || startPos != endPos)
      result.push_back(substring(startPos, endPos - startPos));
    startPos = endPos + 1;
  }
  if (allowEmptyEntries || startPos != length())
    result.push_back(substring(startPos));
}

CString String::ascii() const {
  // Printable ASCII characters 32..127 and the null character are
  // preserved, characters outside of this range are converted to '?'.

  unsigned length = this->length();
  if (!length) {
    char* characterBuffer;
    return CString::createUninitialized(length, characterBuffer);
  }

  if (this->is8Bit()) {
    const LChar* characters = this->characters8();

    char* characterBuffer;
    CString result = CString::createUninitialized(length, characterBuffer);

    for (unsigned i = 0; i < length; ++i) {
      LChar ch = characters[i];
      characterBuffer[i] = ch && (ch < 0x20 || ch > 0x7f) ? '?' : ch;
    }

    return result;
  }

  const UChar* characters = this->characters16();

  char* characterBuffer;
  CString result = CString::createUninitialized(length, characterBuffer);

  for (unsigned i = 0; i < length; ++i) {
    UChar ch = characters[i];
    characterBuffer[i] =
        ch && (ch < 0x20 || ch > 0x7f) ? '?' : static_cast<char>(ch);
  }

  return result;
}

CString String::latin1() const {
  // Basic Latin1 (ISO) encoding - Unicode characters 0..255 are
  // preserved, characters outside of this range are converted to '?'.

  unsigned length = this->length();

  if (!length)
    return CString("", 0);

  if (is8Bit())
    return CString(reinterpret_cast<const char*>(this->characters8()), length);

  const UChar* characters = this->characters16();

  char* characterBuffer;
  CString result = CString::createUninitialized(length, characterBuffer);

  for (unsigned i = 0; i < length; ++i) {
    UChar ch = characters[i];
    characterBuffer[i] = ch > 0xff ? '?' : static_cast<char>(ch);
  }

  return result;
}

// Helper to write a three-byte UTF-8 code point to the buffer, caller must
// check room is available.
static inline void putUTF8Triple(char*& buffer, UChar ch) {
  DCHECK_GE(ch, 0x0800);
  *buffer++ = static_cast<char>(((ch >> 12) & 0x0F) | 0xE0);
  *buffer++ = static_cast<char>(((ch >> 6) & 0x3F) | 0x80);
  *buffer++ = static_cast<char>((ch & 0x3F) | 0x80);
}

CString String::utf8(UTF8ConversionMode mode) const {
  unsigned length = this->length();

  if (!length)
    return CString("", 0);

  // Allocate a buffer big enough to hold all the characters
  // (an individual UTF-16 UChar can only expand to 3 UTF-8 bytes).
  // Optimization ideas, if we find this function is hot:
  //  * We could speculatively create a CStringImpl to contain 'length'
  //    characters, and resize if necessary (i.e. if the buffer contains
  //    non-ascii characters). (Alternatively, scan the buffer first for
  //    ascii characters, so we know this will be sufficient).
  //  * We could allocate a CStringImpl with an appropriate size to
  //    have a good chance of being able to write the string into the
  //    buffer without reallocing (say, 1.5 x length).
  if (length > std::numeric_limits<unsigned>::max() / 3)
    return CString();
  Vector<char, 1024> bufferVector(length * 3);

  char* buffer = bufferVector.data();

  if (is8Bit()) {
    const LChar* characters = this->characters8();

    ConversionResult result =
        convertLatin1ToUTF8(&characters, characters + length, &buffer,
                            buffer + bufferVector.size());
    // (length * 3) should be sufficient for any conversion
    DCHECK_NE(result, targetExhausted);
  } else {
    const UChar* characters = this->characters16();

    if (mode == StrictUTF8ConversionReplacingUnpairedSurrogatesWithFFFD) {
      const UChar* charactersEnd = characters + length;
      char* bufferEnd = buffer + bufferVector.size();
      while (characters < charactersEnd) {
        // Use strict conversion to detect unpaired surrogates.
        ConversionResult result = convertUTF16ToUTF8(&characters, charactersEnd,
                                                     &buffer, bufferEnd, true);
        DCHECK_NE(result, targetExhausted);
        // Conversion fails when there is an unpaired surrogate.  Put
        // replacement character (U+FFFD) instead of the unpaired
        // surrogate.
        if (result != conversionOK) {
          DCHECK_LE(0xD800, *characters);
          DCHECK_LE(*characters, 0xDFFF);
          // There should be room left, since one UChar hasn't been
          // converted.
          DCHECK_LE(buffer + 3, bufferEnd);
          putUTF8Triple(buffer, replacementCharacter);
          ++characters;
        }
      }
    } else {
      bool strict = mode == StrictUTF8Conversion;
      ConversionResult result =
          convertUTF16ToUTF8(&characters, characters + length, &buffer,
                             buffer + bufferVector.size(), strict);
      // (length * 3) should be sufficient for any conversion
      DCHECK_NE(result, targetExhausted);

      // Only produced from strict conversion.
      if (result == sourceIllegal) {
        DCHECK(strict);
        return CString();
      }

      // Check for an unconverted high surrogate.
      if (result == sourceExhausted) {
        if (strict)
          return CString();
        // This should be one unpaired high surrogate. Treat it the same
        // was as an unpaired high surrogate would have been handled in
        // the middle of a string with non-strict conversion - which is
        // to say, simply encode it to UTF-8.
        DCHECK_EQ(characters + 1, this->characters16() + length);
        DCHECK_GE(*characters, 0xD800);
        DCHECK_LE(*characters, 0xDBFF);
        // There should be room left, since one UChar hasn't been
        // converted.
        DCHECK_LE(buffer + 3, buffer + bufferVector.size());
        putUTF8Triple(buffer, *characters);
      }
    }
  }

  return CString(bufferVector.data(), buffer - bufferVector.data());
}

String String::make8BitFrom16BitSource(const UChar* source, size_t length) {
  if (!length)
    return emptyString();

  LChar* destination;
  String result = String::createUninitialized(length, destination);

  copyLCharsFromUCharSource(destination, source, length);

  return result;
}

String String::make16BitFrom8BitSource(const LChar* source, size_t length) {
  if (!length)
    return emptyString16Bit();

  UChar* destination;
  String result = String::createUninitialized(length, destination);

  StringImpl::copyChars(destination, source, length);

  return result;
}

String String::fromUTF8(const LChar* stringStart, size_t length) {
  RELEASE_ASSERT(length <= std::numeric_limits<unsigned>::max());

  if (!stringStart)
    return String();

  if (!length)
    return emptyString();

  if (charactersAreAllASCII(stringStart, length))
    return StringImpl::create(stringStart, length);

  Vector<UChar, 1024> buffer(length);
  UChar* bufferStart = buffer.data();

  UChar* bufferCurrent = bufferStart;
  const char* stringCurrent = reinterpret_cast<const char*>(stringStart);
  if (convertUTF8ToUTF16(
          &stringCurrent, reinterpret_cast<const char*>(stringStart + length),
          &bufferCurrent, bufferCurrent + buffer.size()) != conversionOK)
    return String();

  unsigned utf16Length = bufferCurrent - bufferStart;
  DCHECK_LT(utf16Length, length);
  return StringImpl::create(bufferStart, utf16Length);
}

String String::fromUTF8(const LChar* string) {
  if (!string)
    return String();
  return fromUTF8(string, strlen(reinterpret_cast<const char*>(string)));
}

String String::fromUTF8(const CString& s) {
  return fromUTF8(s.data());
}

String String::fromUTF8WithLatin1Fallback(const LChar* string, size_t size) {
  String utf8 = fromUTF8(string, size);
  if (!utf8)
    return String(string, size);
  return utf8;
}

const String& emptyString() {
  DEFINE_STATIC_LOCAL(String, emptyString, (StringImpl::empty()));
  return emptyString;
}

const String& emptyString16Bit() {
  DEFINE_STATIC_LOCAL(String, emptyString, (StringImpl::empty16Bit()));
  return emptyString;
}

std::ostream& operator<<(std::ostream& out, const String& string) {
  if (string.isNull())
    return out << "<null>";

  out << '"';
  for (unsigned index = 0; index < string.length(); ++index) {
    // Print shorthands for select cases.
    UChar character = string[index];
    switch (character) {
      case '\t':
        out << "\\t";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '"':
        out << "\\\"";
        break;
      case '\\':
        out << "\\\\";
        break;
      default:
        if (isASCIIPrintable(character)) {
          out << static_cast<char>(character);
        } else {
          // Print "\uXXXX" for control or non-ASCII characters.
          out << "\\u";
          out.width(4);
          out.fill('0');
          out.setf(std::ios_base::hex, std::ios_base::basefield);
          out.setf(std::ios::uppercase);
          out << character;
        }
        break;
    }
  }
  return out << '"';
}

#ifndef NDEBUG
void String::show() const {
  dataLogF("%s\n", asciiDebug(impl()).data());
}
#endif

}  // namespace WTF
