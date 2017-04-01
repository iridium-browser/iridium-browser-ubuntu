/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef Character_h
#define Character_h

#include "platform/PlatformExport.h"
#include "platform/text/CharacterProperty.h"
#include "platform/text/TextDirection.h"
#include "platform/text/TextRun.h"
#include "wtf/ASCIICType.h"
#include "wtf/Allocator.h"
#include "wtf/HashSet.h"
#include "wtf/text/CharacterNames.h"
#include "wtf/text/WTFString.h"

namespace blink {

class PLATFORM_EXPORT Character {
  STATIC_ONLY(Character);

 public:
  static inline bool isInRange(UChar32 character,
                               UChar32 lowerBound,
                               UChar32 upperBound) {
    return character >= lowerBound && character <= upperBound;
  }

  static inline bool isUnicodeVariationSelector(UChar32 character) {
    // http://www.unicode.org/Public/UCD/latest/ucd/StandardizedVariants.html
    return isInRange(character, 0x180B,
                     0x180D)  // MONGOLIAN FREE VARIATION SELECTOR ONE to THREE
           ||
           isInRange(character, 0xFE00, 0xFE0F)  // VARIATION SELECTOR-1 to 16
           || isInRange(character, 0xE0100,
                        0xE01EF);  // VARIATION SELECTOR-17 to 256
  }

  static bool isCJKIdeographOrSymbol(UChar32);
  static bool isCJKIdeographOrSymbolBase(UChar32 c) {
    return isCJKIdeographOrSymbol(c) &&
           !(U_GET_GC_MASK(c) & (U_GC_M_MASK | U_GC_LM_MASK | U_GC_SK_MASK));
  }

  static unsigned expansionOpportunityCount(const LChar*,
                                            size_t length,
                                            TextDirection,
                                            bool& isAfterExpansion,
                                            const TextJustify);
  static unsigned expansionOpportunityCount(const UChar*,
                                            size_t length,
                                            TextDirection,
                                            bool& isAfterExpansion,
                                            const TextJustify);
  static unsigned expansionOpportunityCount(const TextRun& run,
                                            bool& isAfterExpansion) {
    if (run.is8Bit())
      return expansionOpportunityCount(run.characters8(), run.length(),
                                       run.direction(), isAfterExpansion,
                                       run.getTextJustify());
    return expansionOpportunityCount(run.characters16(), run.length(),
                                     run.direction(), isAfterExpansion,
                                     run.getTextJustify());
  }

  static bool isUprightInMixedVertical(UChar32 character);

  // https://html.spec.whatwg.org/multipage/scripting.html#prod-potentialcustomelementname
  static bool isPotentialCustomElementName8BitChar(LChar ch) {
    return isASCIILower(ch) || isASCIIDigit(ch) || ch == '-' || ch == '.' ||
           ch == '_' || ch == 0xb7 || (0xc0 <= ch && ch != 0xd7 && ch != 0xf7);
  }
  static bool isPotentialCustomElementNameChar(UChar32 character);

  static bool treatAsSpace(UChar32 c) {
    return c == spaceCharacter || c == tabulationCharacter ||
           c == newlineCharacter || c == noBreakSpaceCharacter;
  }
  static bool treatAsZeroWidthSpace(UChar32 c) {
    return treatAsZeroWidthSpaceInComplexScript(c) ||
           c == zeroWidthNonJoinerCharacter || c == zeroWidthJoinerCharacter;
  }
  static bool legacyTreatAsZeroWidthSpaceInComplexScript(UChar32 c) {
    return c < 0x20  // ASCII Control Characters
           ||
           (c >= 0x7F && c < 0xA0)  // ASCII Delete .. No-break spaceCharacter
           || treatAsZeroWidthSpaceInComplexScript(c);
  }
  static bool treatAsZeroWidthSpaceInComplexScript(UChar32 c) {
    return c == formFeedCharacter || c == carriageReturnCharacter ||
           c == softHyphenCharacter || c == zeroWidthSpaceCharacter ||
           (c >= leftToRightMarkCharacter && c <= rightToLeftMarkCharacter) ||
           (c >= leftToRightEmbedCharacter &&
            c <= rightToLeftOverrideCharacter) ||
           c == zeroWidthNoBreakSpaceCharacter ||
           c == objectReplacementCharacter;
  }
  static bool canReceiveTextEmphasis(UChar32);

  static bool isGraphemeExtended(UChar32 c) {
    // http://unicode.org/reports/tr29/#Extend
    return u_hasBinaryProperty(c, UCHAR_GRAPHEME_EXTEND);
  }

  // Returns true if the character has a Emoji property.
  // See http://www.unicode.org/Public/emoji/3.0/emoji-data.txt
  static bool isEmoji(UChar32);
  // Default presentation style according to:
  // http://www.unicode.org/reports/tr51/#Presentation_Style
  static bool isEmojiTextDefault(UChar32);
  static bool isEmojiEmojiDefault(UChar32);
  static bool isEmojiModifierBase(UChar32);
  static bool isEmojiKeycapBase(UChar32);
  static bool isRegionalIndicator(UChar32);
  static bool isModifier(UChar32 c) { return c >= 0x1F3FB && c <= 0x1F3FF; }

  static inline UChar normalizeSpaces(UChar character) {
    if (treatAsSpace(character))
      return spaceCharacter;

    if (treatAsZeroWidthSpace(character))
      return zeroWidthSpaceCharacter;

    return character;
  }

  static inline bool isNormalizedCanvasSpaceCharacter(UChar32 c) {
    // According to specification all space characters should be replaced with
    // 0x0020 space character.
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-canvas-element.html#text-preparation-algorithm
    // The space characters according to specification are : U+0020, U+0009,
    // U+000A, U+000C, and U+000D.
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/common-microsyntaxes.html#space-character
    // This function returns true for 0x000B also, so that this is backward
    // compatible.  Otherwise, the test
    // LayoutTests/canvas/philip/tests/2d.text.draw.space.collapse.space.html
    // will fail
    return c == 0x0009 || (c >= 0x000A && c <= 0x000D);
  }

  static String normalizeSpaces(const LChar*, unsigned length);
  static String normalizeSpaces(const UChar*, unsigned length);

  static bool isCommonOrInheritedScript(UChar32);
};

}  // namespace blink

#endif
