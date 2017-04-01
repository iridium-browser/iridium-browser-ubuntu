/*
 * Copyright (C) 2007, 2008 Apple, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "wtf/text/TextCodecUserDefined.h"

#include "wtf/PtrUtil.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuffer.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace WTF {

void TextCodecUserDefined::registerEncodingNames(
    EncodingNameRegistrar registrar) {
  registrar("x-user-defined", "x-user-defined");
}

static std::unique_ptr<TextCodec> newStreamingTextDecoderUserDefined(
    const TextEncoding&,
    const void*) {
  return WTF::wrapUnique(new TextCodecUserDefined);
}

void TextCodecUserDefined::registerCodecs(TextCodecRegistrar registrar) {
  registrar("x-user-defined", newStreamingTextDecoderUserDefined, 0);
}

String TextCodecUserDefined::decode(const char* bytes,
                                    size_t length,
                                    FlushBehavior,
                                    bool,
                                    bool&) {
  StringBuilder result;
  result.reserveCapacity(length);

  for (size_t i = 0; i < length; ++i) {
    signed char c = bytes[i];
    result.append(static_cast<UChar>(c & 0xF7FF));
  }

  return result.toString();
}

template <typename CharType>
static CString encodeComplexUserDefined(const CharType* characters,
                                        size_t length,
                                        UnencodableHandling handling) {
  size_t targetLength = length;
  Vector<char> result(targetLength);
  char* bytes = result.data();

  size_t resultLength = 0;
  for (size_t i = 0; i < length;) {
    UChar32 c;
    // TODO(jsbell): Will the input for x-user-defined ever be LChars?
    U16_NEXT(characters, i, length, c);
    // If the input was a surrogate pair (non-BMP character) then we
    // overestimated the length.
    if (c > 0xffff)
      --targetLength;
    signed char signedByte = static_cast<signed char>(c);
    if ((signedByte & 0xF7FF) == c) {
      bytes[resultLength++] = signedByte;
    } else {
      // No way to encode this character with x-user-defined.
      UnencodableReplacementArray replacement;
      int replacementLength =
          TextCodec::getUnencodableReplacement(c, handling, replacement);
      DCHECK_GT(replacementLength, 0);
      // Only one char was initially reserved per input character, so grow if
      // necessary. Note that in the case of surrogate pairs and
      // QuestionMarksForUnencodables the result length may be shorter than
      // the input length.
      targetLength += replacementLength - 1;
      if (targetLength > result.size()) {
        result.grow(targetLength);
        bytes = result.data();
      }
      memcpy(bytes + resultLength, replacement, replacementLength);
      resultLength += replacementLength;
    }
  }

  return CString(bytes, resultLength);
}

template <typename CharType>
CString TextCodecUserDefined::encodeCommon(const CharType* characters,
                                           size_t length,
                                           UnencodableHandling handling) {
  char* bytes;
  CString result = CString::createUninitialized(length, bytes);

  // Convert the string a fast way and simultaneously do an efficient check to
  // see if it's all ASCII.
  UChar ored = 0;
  for (size_t i = 0; i < length; ++i) {
    UChar c = characters[i];
    bytes[i] = static_cast<char>(c);
    ored |= c;
  }

  if (!(ored & 0xFF80))
    return result;

  // If it wasn't all ASCII, call the function that handles more-complex cases.
  return encodeComplexUserDefined(characters, length, handling);
}

CString TextCodecUserDefined::encode(const UChar* characters,
                                     size_t length,
                                     UnencodableHandling handling) {
  return encodeCommon(characters, length, handling);
}

CString TextCodecUserDefined::encode(const LChar* characters,
                                     size_t length,
                                     UnencodableHandling handling) {
  return encodeCommon(characters, length, handling);
}

}  // namespace WTF
