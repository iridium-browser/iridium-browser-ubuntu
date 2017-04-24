/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/platform/WebString.h"

#include "base/strings/string_util.h"
#include "wtf/text/ASCIIFastPath.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringUTF8Adaptor.h"
#include "wtf/text/StringView.h"
#include "wtf/text/WTFString.h"

#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)

STATIC_ASSERT_ENUM(WTF::LenientUTF8Conversion,
                   blink::WebString::UTF8ConversionMode::kLenient);
STATIC_ASSERT_ENUM(WTF::StrictUTF8Conversion,
                   blink::WebString::UTF8ConversionMode::kStrict);
STATIC_ASSERT_ENUM(
    WTF::StrictUTF8ConversionReplacingUnpairedSurrogatesWithFFFD,
    blink::WebString::UTF8ConversionMode::kStrictReplacingErrorsWithFFFD);

namespace blink {

void WebString::reset() {
  m_private.reset();
}

void WebString::assign(const WebString& other) {
  assign(other.m_private.get());
}

void WebString::assign(const WebUChar* data, size_t length) {
  assign(StringImpl::create8BitIfPossible(data, length).get());
}

size_t WebString::length() const {
  return m_private.isNull() ? 0 : m_private->length();
}

bool WebString::is8Bit() const {
  return m_private->is8Bit();
}

const WebLChar* WebString::data8() const {
  return !m_private.isNull() && is8Bit() ? m_private->characters8() : 0;
}

const WebUChar* WebString::data16() const {
  return !m_private.isNull() && !is8Bit() ? m_private->characters16() : 0;
}

std::string WebString::utf8(UTF8ConversionMode mode) const {
  StringUTF8Adaptor utf8(m_private.get(),
                         static_cast<WTF::UTF8ConversionMode>(mode));
  return std::string(utf8.data(), utf8.length());
}

WebString WebString::fromUTF8(const char* data, size_t length) {
  return String::fromUTF8(data, length);
}

WebString WebString::fromUTF16(const base::string16& s) {
  WebString string;
  string.assign(s.data(), s.length());
  return string;
}

WebString WebString::fromUTF16(const base::NullableString16& s) {
  WebString string;
  if (s.is_null())
    string.reset();
  else
    string.assign(s.string().data(), s.string().length());
  return string;
}

std::string WebString::latin1() const {
  String string(m_private.get());

  if (string.isEmpty())
    return std::string();

  if (string.is8Bit())
    return std::string(reinterpret_cast<const char*>(string.characters8()),
                       string.length());

  CString latin1 = string.latin1();
  return std::string(latin1.data(), latin1.length());
}

WebString WebString::fromLatin1(const WebLChar* data, size_t length) {
  return String(data, length);
}

std::string WebString::ascii() const {
  DCHECK(containsOnlyASCII());

  if (isEmpty())
    return std::string();

  if (m_private->is8Bit()) {
    return std::string(reinterpret_cast<const char*>(m_private->characters8()),
                       m_private->length());
  }

  return std::string(m_private->characters16(),
                     m_private->characters16() + m_private->length());
}

bool WebString::containsOnlyASCII() const {
  return String(m_private.get()).containsOnlyASCII();
}

WebString WebString::fromASCII(const std::string& s) {
  DCHECK(base::IsStringASCII(s));
  return fromLatin1(s);
}

bool WebString::equals(const WebString& s) const {
  return equal(m_private.get(), s.m_private.get());
}

bool WebString::equals(const char* characters, size_t length) const {
  return equal(m_private.get(), characters, length);
}

WebString::WebString(const WTF::String& s) : m_private(s.impl()) {}

WebString& WebString::operator=(const WTF::String& s) {
  assign(s.impl());
  return *this;
}

WebString::operator WTF::String() const {
  return m_private.get();
}

WebString::operator WTF::StringView() const {
  return StringView(m_private.get());
}

WebString::WebString(const WTF::AtomicString& s) {
  assign(s.getString());
}

WebString& WebString::operator=(const WTF::AtomicString& s) {
  assign(s.getString());
  return *this;
}

WebString::operator WTF::AtomicString() const {
  return WTF::AtomicString(m_private.get());
}

void WebString::assign(WTF::StringImpl* p) {
  m_private = p;
}

}  // namespace blink
