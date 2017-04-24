// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSParserFastPaths.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSColorValue.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSInheritedValue.h"
#include "core/css/CSSInitialValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/StyleColor.h"
#include "core/css/parser/CSSParserIdioms.h"
#include "core/css/parser/CSSPropertyParser.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/text/StringToNumber.h"

namespace blink {

static inline bool isSimpleLengthPropertyID(CSSPropertyID propertyId,
                                            bool& acceptsNegativeNumbers) {
  switch (propertyId) {
    case CSSPropertyBlockSize:
    case CSSPropertyInlineSize:
    case CSSPropertyMinBlockSize:
    case CSSPropertyMinInlineSize:
    case CSSPropertyFontSize:
    case CSSPropertyGridColumnGap:
    case CSSPropertyGridRowGap:
    case CSSPropertyHeight:
    case CSSPropertyWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMinWidth:
    case CSSPropertyPaddingBottom:
    case CSSPropertyPaddingLeft:
    case CSSPropertyPaddingRight:
    case CSSPropertyPaddingTop:
    case CSSPropertyWebkitLogicalWidth:
    case CSSPropertyWebkitLogicalHeight:
    case CSSPropertyWebkitMinLogicalWidth:
    case CSSPropertyWebkitMinLogicalHeight:
    case CSSPropertyWebkitPaddingAfter:
    case CSSPropertyWebkitPaddingBefore:
    case CSSPropertyWebkitPaddingEnd:
    case CSSPropertyWebkitPaddingStart:
    case CSSPropertyShapeMargin:
    case CSSPropertyR:
    case CSSPropertyRx:
    case CSSPropertyRy:
      acceptsNegativeNumbers = false;
      return true;
    case CSSPropertyBottom:
    case CSSPropertyCx:
    case CSSPropertyCy:
    case CSSPropertyLeft:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginTop:
    case CSSPropertyOffsetDistance:
    case CSSPropertyRight:
    case CSSPropertyTop:
    case CSSPropertyWebkitMarginAfter:
    case CSSPropertyWebkitMarginBefore:
    case CSSPropertyWebkitMarginEnd:
    case CSSPropertyWebkitMarginStart:
    case CSSPropertyX:
    case CSSPropertyY:
      acceptsNegativeNumbers = true;
      return true;
    default:
      return false;
  }
}

template <typename CharacterType>
static inline bool parseSimpleLength(const CharacterType* characters,
                                     unsigned length,
                                     CSSPrimitiveValue::UnitType& unit,
                                     double& number) {
  if (length > 2 && (characters[length - 2] | 0x20) == 'p' &&
      (characters[length - 1] | 0x20) == 'x') {
    length -= 2;
    unit = CSSPrimitiveValue::UnitType::Pixels;
  } else if (length > 1 && characters[length - 1] == '%') {
    length -= 1;
    unit = CSSPrimitiveValue::UnitType::Percentage;
  }

  // We rely on charactersToDouble for validation as well. The function
  // will set "ok" to "false" if the entire passed-in character range does
  // not represent a double.
  bool ok;
  number = charactersToDouble(characters, length, &ok);
  if (!ok)
    return false;
  number = clampTo<double>(number, -std::numeric_limits<float>::max(),
                           std::numeric_limits<float>::max());
  return true;
}

static CSSValue* parseSimpleLengthValue(CSSPropertyID propertyId,
                                        const String& string,
                                        CSSParserMode cssParserMode) {
  ASSERT(!string.isEmpty());
  bool acceptsNegativeNumbers = false;

  // In @viewport, width and height are shorthands, not simple length values.
  if (isCSSViewportParsingEnabledForMode(cssParserMode) ||
      !isSimpleLengthPropertyID(propertyId, acceptsNegativeNumbers))
    return nullptr;

  unsigned length = string.length();
  double number;
  CSSPrimitiveValue::UnitType unit = CSSPrimitiveValue::UnitType::Number;

  if (string.is8Bit()) {
    if (!parseSimpleLength(string.characters8(), length, unit, number))
      return nullptr;
  } else {
    if (!parseSimpleLength(string.characters16(), length, unit, number))
      return nullptr;
  }

  if (unit == CSSPrimitiveValue::UnitType::Number) {
    if (cssParserMode == SVGAttributeMode)
      unit = CSSPrimitiveValue::UnitType::UserUnits;
    else if (!number)
      unit = CSSPrimitiveValue::UnitType::Pixels;
    else
      return nullptr;
  }

  if (number < 0 && !acceptsNegativeNumbers)
    return nullptr;

  return CSSPrimitiveValue::create(number, unit);
}

static inline bool isColorPropertyID(CSSPropertyID propertyId) {
  switch (propertyId) {
    case CSSPropertyCaretColor:
    case CSSPropertyColor:
    case CSSPropertyBackgroundColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor:
    case CSSPropertyFill:
    case CSSPropertyFloodColor:
    case CSSPropertyLightingColor:
    case CSSPropertyOutlineColor:
    case CSSPropertyStopColor:
    case CSSPropertyStroke:
    case CSSPropertyWebkitBorderAfterColor:
    case CSSPropertyWebkitBorderBeforeColor:
    case CSSPropertyWebkitBorderEndColor:
    case CSSPropertyWebkitBorderStartColor:
    case CSSPropertyColumnRuleColor:
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextStrokeColor:
    case CSSPropertyTextDecorationColor:
      return true;
    default:
      return false;
  }
}

// Returns the number of characters which form a valid double
// and are terminated by the given terminator character
template <typename CharacterType>
static int checkForValidDouble(const CharacterType* string,
                               const CharacterType* end,
                               const char terminator) {
  int length = end - string;
  if (length < 1)
    return 0;

  bool decimalMarkSeen = false;
  int processedLength = 0;

  for (int i = 0; i < length; ++i) {
    if (string[i] == terminator) {
      processedLength = i;
      break;
    }
    if (!isASCIIDigit(string[i])) {
      if (!decimalMarkSeen && string[i] == '.')
        decimalMarkSeen = true;
      else
        return 0;
    }
  }

  if (decimalMarkSeen && processedLength == 1)
    return 0;

  return processedLength;
}

// Returns the number of characters consumed for parsing a valid double
// terminated by the given terminator character
template <typename CharacterType>
static int parseDouble(const CharacterType* string,
                       const CharacterType* end,
                       const char terminator,
                       double& value) {
  int length = checkForValidDouble(string, end, terminator);
  if (!length)
    return 0;

  int position = 0;
  double localValue = 0;

  // The consumed characters here are guaranteed to be
  // ASCII digits with or without a decimal mark
  for (; position < length; ++position) {
    if (string[position] == '.')
      break;
    localValue = localValue * 10 + string[position] - '0';
  }

  if (++position == length) {
    value = localValue;
    return length;
  }

  double fraction = 0;
  double scale = 1;

  const double maxScale = 1000000;
  while (position < length && scale < maxScale) {
    fraction = fraction * 10 + string[position++] - '0';
    scale *= 10;
  }

  value = localValue + fraction / scale;
  return length;
}

template <typename CharacterType>
static bool parseColorIntOrPercentage(const CharacterType*& string,
                                      const CharacterType* end,
                                      const char terminator,
                                      CSSPrimitiveValue::UnitType& expect,
                                      int& value) {
  const CharacterType* current = string;
  double localValue = 0;
  bool negative = false;
  while (current != end && isHTMLSpace<CharacterType>(*current))
    current++;
  if (current != end && *current == '-') {
    negative = true;
    current++;
  }
  if (current == end || !isASCIIDigit(*current))
    return false;
  while (current != end && isASCIIDigit(*current)) {
    double newValue = localValue * 10 + *current++ - '0';
    if (newValue >= 255) {
      // Clamp values at 255.
      localValue = 255;
      while (current != end && isASCIIDigit(*current))
        ++current;
      break;
    }
    localValue = newValue;
  }

  if (current == end)
    return false;

  if (expect == CSSPrimitiveValue::UnitType::Number &&
      (*current == '.' || *current == '%'))
    return false;

  if (*current == '.') {
    // We already parsed the integral part, try to parse
    // the fraction part of the percentage value.
    double percentage = 0;
    int numCharactersParsed = parseDouble(current, end, '%', percentage);
    if (!numCharactersParsed)
      return false;
    current += numCharactersParsed;
    if (*current != '%')
      return false;
    localValue += percentage;
  }

  if (expect == CSSPrimitiveValue::UnitType::Percentage && *current != '%')
    return false;

  if (*current == '%') {
    expect = CSSPrimitiveValue::UnitType::Percentage;
    localValue = localValue / 100.0 * 256.0;
    // Clamp values at 255 for percentages over 100%
    if (localValue > 255)
      localValue = 255;
    current++;
  } else {
    expect = CSSPrimitiveValue::UnitType::Number;
  }

  while (current != end && isHTMLSpace<CharacterType>(*current))
    current++;
  if (current == end || *current++ != terminator)
    return false;
  // Clamp negative values at zero.
  value = negative ? 0 : static_cast<int>(localValue);
  string = current;
  return true;
}

template <typename CharacterType>
static inline bool isTenthAlpha(const CharacterType* string, const int length) {
  // "0.X"
  if (length == 3 && string[0] == '0' && string[1] == '.' &&
      isASCIIDigit(string[2]))
    return true;

  // ".X"
  if (length == 2 && string[0] == '.' && isASCIIDigit(string[1]))
    return true;

  return false;
}

template <typename CharacterType>
static inline bool parseAlphaValue(const CharacterType*& string,
                                   const CharacterType* end,
                                   const char terminator,
                                   int& value) {
  while (string != end && isHTMLSpace<CharacterType>(*string))
    string++;

  bool negative = false;

  if (string != end && *string == '-') {
    negative = true;
    string++;
  }

  value = 0;

  int length = end - string;
  if (length < 2)
    return false;

  if (string[length - 1] != terminator || !isASCIIDigit(string[length - 2]))
    return false;

  if (string[0] != '0' && string[0] != '1' && string[0] != '.') {
    if (checkForValidDouble(string, end, terminator)) {
      value = negative ? 0 : 255;
      string = end;
      return true;
    }
    return false;
  }

  if (length == 2 && string[0] != '.') {
    value = !negative && string[0] == '1' ? 255 : 0;
    string = end;
    return true;
  }

  if (isTenthAlpha(string, length - 1)) {
    // Fast conversions for 0.1 steps of alpha values between 0.0 and 0.9,
    // where 0.1 alpha is value 26 (25.5 rounded) and so on.
    static const int tenthAlphaValues[] = {0,   26,  51,  77,  102,
                                           128, 153, 179, 204, 230};
    value = negative ? 0 : tenthAlphaValues[string[length - 2] - '0'];
    string = end;
    return true;
  }

  double alpha = 0;
  if (!parseDouble(string, end, terminator, alpha))
    return false;
  value =
      negative ? 0 : static_cast<int>(roundf(std::min(alpha, 1.0) * 255.0f));
  string = end;
  return true;
}

template <typename CharacterType>
static inline bool mightBeRGBA(const CharacterType* characters,
                               unsigned length) {
  if (length < 5)
    return false;
  return characters[4] == '(' &&
         isASCIIAlphaCaselessEqual(characters[0], 'r') &&
         isASCIIAlphaCaselessEqual(characters[1], 'g') &&
         isASCIIAlphaCaselessEqual(characters[2], 'b') &&
         isASCIIAlphaCaselessEqual(characters[3], 'a');
}

template <typename CharacterType>
static inline bool mightBeRGB(const CharacterType* characters,
                              unsigned length) {
  if (length < 4)
    return false;
  return characters[3] == '(' &&
         isASCIIAlphaCaselessEqual(characters[0], 'r') &&
         isASCIIAlphaCaselessEqual(characters[1], 'g') &&
         isASCIIAlphaCaselessEqual(characters[2], 'b');
}

template <typename CharacterType>
static bool fastParseColorInternal(RGBA32& rgb,
                                   const CharacterType* characters,
                                   unsigned length,
                                   bool quirksMode) {
  CSSPrimitiveValue::UnitType expect = CSSPrimitiveValue::UnitType::Unknown;

  if (length >= 4 && characters[0] == '#')
    return Color::parseHexColor(characters + 1, length - 1, rgb);

  if (quirksMode && (length == 3 || length == 6)) {
    if (Color::parseHexColor(characters, length, rgb))
      return true;
  }

  // Try rgba() syntax.
  if (mightBeRGBA(characters, length)) {
    const CharacterType* current = characters + 5;
    const CharacterType* end = characters + length;
    int red;
    int green;
    int blue;
    int alpha;

    if (!parseColorIntOrPercentage(current, end, ',', expect, red))
      return false;
    if (!parseColorIntOrPercentage(current, end, ',', expect, green))
      return false;
    if (!parseColorIntOrPercentage(current, end, ',', expect, blue))
      return false;
    if (!parseAlphaValue(current, end, ')', alpha))
      return false;
    if (current != end)
      return false;
    rgb = makeRGBA(red, green, blue, alpha);
    return true;
  }

  // Try rgb() syntax.
  if (mightBeRGB(characters, length)) {
    const CharacterType* current = characters + 4;
    const CharacterType* end = characters + length;
    int red;
    int green;
    int blue;
    if (!parseColorIntOrPercentage(current, end, ',', expect, red))
      return false;
    if (!parseColorIntOrPercentage(current, end, ',', expect, green))
      return false;
    if (!parseColorIntOrPercentage(current, end, ')', expect, blue))
      return false;
    if (current != end)
      return false;
    rgb = makeRGB(red, green, blue);
    return true;
  }

  return false;
}

CSSValue* CSSParserFastPaths::parseColor(const String& string,
                                         CSSParserMode parserMode) {
  ASSERT(!string.isEmpty());
  CSSValueID valueID = cssValueKeywordID(string);
  if (StyleColor::isColorKeyword(valueID)) {
    if (!isValueAllowedInMode(valueID, parserMode))
      return nullptr;
    return CSSIdentifierValue::create(valueID);
  }

  RGBA32 color;
  bool quirksMode = isQuirksModeBehavior(parserMode);

  // Fast path for hex colors and rgb()/rgba() colors
  bool parseResult;
  if (string.is8Bit())
    parseResult = fastParseColorInternal(color, string.characters8(),
                                         string.length(), quirksMode);
  else
    parseResult = fastParseColorInternal(color, string.characters16(),
                                         string.length(), quirksMode);
  if (!parseResult)
    return nullptr;
  return CSSColorValue::create(color);
}

bool CSSParserFastPaths::isValidKeywordPropertyAndValue(
    CSSPropertyID propertyId,
    CSSValueID valueID,
    CSSParserMode parserMode) {
  if (valueID == CSSValueInvalid || !isValueAllowedInMode(valueID, parserMode))
    return false;

  // For range checks, enum ordering is defined by CSSValueKeywords.in.
  switch (propertyId) {
    case CSSPropertyAlignmentBaseline:
      return valueID == CSSValueAuto || valueID == CSSValueAlphabetic ||
             valueID == CSSValueBaseline || valueID == CSSValueMiddle ||
             (valueID >= CSSValueBeforeEdge && valueID <= CSSValueMathematical);
    case CSSPropertyAll:
      return false;  // Only accepts css-wide keywords
    case CSSPropertyBackgroundRepeatX:
    case CSSPropertyBackgroundRepeatY:
      return valueID == CSSValueRepeat || valueID == CSSValueNoRepeat;
    case CSSPropertyBorderCollapse:
      return valueID == CSSValueCollapse || valueID == CSSValueSeparate;
    case CSSPropertyBorderTopStyle:
    case CSSPropertyBorderRightStyle:
    case CSSPropertyBorderBottomStyle:
    case CSSPropertyBorderLeftStyle:
    case CSSPropertyWebkitBorderAfterStyle:
    case CSSPropertyWebkitBorderBeforeStyle:
    case CSSPropertyWebkitBorderEndStyle:
    case CSSPropertyWebkitBorderStartStyle:
    case CSSPropertyColumnRuleStyle:
      return valueID >= CSSValueNone && valueID <= CSSValueDouble;
    case CSSPropertyBoxSizing:
      return valueID == CSSValueBorderBox || valueID == CSSValueContentBox;
    case CSSPropertyBufferedRendering:
      return valueID == CSSValueAuto || valueID == CSSValueDynamic ||
             valueID == CSSValueStatic;
    case CSSPropertyCaptionSide:
      return valueID == CSSValueLeft || valueID == CSSValueRight ||
             valueID == CSSValueTop || valueID == CSSValueBottom;
    case CSSPropertyClear:
      return valueID == CSSValueNone || valueID == CSSValueLeft ||
             valueID == CSSValueRight || valueID == CSSValueBoth;
    case CSSPropertyClipRule:
    case CSSPropertyFillRule:
      return valueID == CSSValueNonzero || valueID == CSSValueEvenodd;
    case CSSPropertyColorInterpolation:
    case CSSPropertyColorInterpolationFilters:
      return valueID == CSSValueAuto || valueID == CSSValueSRGB ||
             valueID == CSSValueLinearRGB;
    case CSSPropertyColorRendering:
      return valueID == CSSValueAuto || valueID == CSSValueOptimizeSpeed ||
             valueID == CSSValueOptimizeQuality;
    case CSSPropertyDirection:
      return valueID == CSSValueLtr || valueID == CSSValueRtl;
    case CSSPropertyDisplay:
      return (valueID >= CSSValueInline && valueID <= CSSValueInlineFlex) ||
             valueID == CSSValueWebkitFlex ||
             valueID == CSSValueWebkitInlineFlex || valueID == CSSValueNone ||
             (RuntimeEnabledFeatures::cssGridLayoutEnabled() &&
              (valueID == CSSValueGrid || valueID == CSSValueInlineGrid)) ||
             (RuntimeEnabledFeatures::cssDisplayContentsEnabled() &&
              valueID == CSSValueContents);
    case CSSPropertyDominantBaseline:
      return valueID == CSSValueAuto || valueID == CSSValueAlphabetic ||
             valueID == CSSValueMiddle ||
             (valueID >= CSSValueUseScript && valueID <= CSSValueResetSize) ||
             (valueID >= CSSValueCentral && valueID <= CSSValueMathematical);
    case CSSPropertyEmptyCells:
      return valueID == CSSValueShow || valueID == CSSValueHide;
    case CSSPropertyFloat:
      return valueID == CSSValueLeft || valueID == CSSValueRight ||
             valueID == CSSValueNone;
    case CSSPropertyFontStyle:
      return valueID == CSSValueNormal || valueID == CSSValueItalic ||
             valueID == CSSValueOblique;
    case CSSPropertyFontStretch:
      return valueID == CSSValueNormal || (valueID >= CSSValueUltraCondensed &&
                                           valueID <= CSSValueUltraExpanded);
    case CSSPropertyImageRendering:
      return valueID == CSSValueAuto ||
             valueID == CSSValueWebkitOptimizeContrast ||
             valueID == CSSValuePixelated;
    case CSSPropertyIsolation:
      return valueID == CSSValueAuto || valueID == CSSValueIsolate;
    case CSSPropertyListStylePosition:
      return valueID == CSSValueInside || valueID == CSSValueOutside;
    case CSSPropertyListStyleType:
      return (valueID >= CSSValueDisc && valueID <= CSSValueKatakanaIroha) ||
             valueID == CSSValueNone;
    case CSSPropertyMaskType:
      return valueID == CSSValueLuminance || valueID == CSSValueAlpha;
    case CSSPropertyObjectFit:
      return valueID == CSSValueFill || valueID == CSSValueContain ||
             valueID == CSSValueCover || valueID == CSSValueNone ||
             valueID == CSSValueScaleDown;
    case CSSPropertyOutlineStyle:
      return valueID == CSSValueAuto || valueID == CSSValueNone ||
             (valueID >= CSSValueInset && valueID <= CSSValueDouble);
    case CSSPropertyOverflowAnchor:
      return valueID == CSSValueVisible || valueID == CSSValueNone ||
             valueID == CSSValueAuto;
    case CSSPropertyOverflowWrap:
    case CSSPropertyWordWrap:
      return valueID == CSSValueNormal || valueID == CSSValueBreakWord;
    case CSSPropertyOverflowX:
      return valueID == CSSValueVisible || valueID == CSSValueHidden ||
             valueID == CSSValueScroll || valueID == CSSValueAuto ||
             valueID == CSSValueOverlay;
    case CSSPropertyOverflowY:
      return valueID == CSSValueVisible || valueID == CSSValueHidden ||
             valueID == CSSValueScroll || valueID == CSSValueAuto ||
             valueID == CSSValueOverlay || valueID == CSSValueWebkitPagedX ||
             valueID == CSSValueWebkitPagedY;
    case CSSPropertyBreakAfter:
    case CSSPropertyBreakBefore:
      return valueID == CSSValueAuto || valueID == CSSValueAvoid ||
             valueID == CSSValueAvoidPage || valueID == CSSValuePage ||
             valueID == CSSValueLeft || valueID == CSSValueRight ||
             valueID == CSSValueRecto || valueID == CSSValueVerso ||
             valueID == CSSValueAvoidColumn || valueID == CSSValueColumn;
    case CSSPropertyBreakInside:
      return valueID == CSSValueAuto || valueID == CSSValueAvoid ||
             valueID == CSSValueAvoidPage || valueID == CSSValueAvoidColumn;
    case CSSPropertyPointerEvents:
      return valueID == CSSValueVisible || valueID == CSSValueNone ||
             valueID == CSSValueAll || valueID == CSSValueAuto ||
             (valueID >= CSSValueVisiblePainted &&
              valueID <= CSSValueBoundingBox);
    case CSSPropertyPosition:
      return valueID == CSSValueStatic || valueID == CSSValueRelative ||
             valueID == CSSValueAbsolute || valueID == CSSValueFixed ||
             (RuntimeEnabledFeatures::cssStickyPositionEnabled() &&
              valueID == CSSValueSticky);
    case CSSPropertyResize:
      return valueID == CSSValueNone || valueID == CSSValueBoth ||
             valueID == CSSValueHorizontal || valueID == CSSValueVertical ||
             valueID == CSSValueAuto;
    case CSSPropertyScrollBehavior:
      ASSERT(RuntimeEnabledFeatures::cssomSmoothScrollEnabled());
      return valueID == CSSValueAuto || valueID == CSSValueSmooth;
    case CSSPropertyShapeRendering:
      return valueID == CSSValueAuto || valueID == CSSValueOptimizeSpeed ||
             valueID == CSSValueCrispEdges ||
             valueID == CSSValueGeometricPrecision;
    case CSSPropertySpeak:
      return valueID == CSSValueNone || valueID == CSSValueNormal ||
             valueID == CSSValueSpellOut || valueID == CSSValueDigits ||
             valueID == CSSValueLiteralPunctuation ||
             valueID == CSSValueNoPunctuation;
    case CSSPropertyStrokeLinejoin:
      return valueID == CSSValueMiter || valueID == CSSValueRound ||
             valueID == CSSValueBevel;
    case CSSPropertyStrokeLinecap:
      return valueID == CSSValueButt || valueID == CSSValueRound ||
             valueID == CSSValueSquare;
    case CSSPropertyTableLayout:
      return valueID == CSSValueAuto || valueID == CSSValueFixed;
    case CSSPropertyTextAlign:
      return (valueID >= CSSValueWebkitAuto &&
              valueID <= CSSValueInternalCenter) ||
             valueID == CSSValueStart || valueID == CSSValueEnd;
    case CSSPropertyTextAlignLast:
      return (valueID >= CSSValueLeft && valueID <= CSSValueJustify) ||
             valueID == CSSValueStart || valueID == CSSValueEnd ||
             valueID == CSSValueAuto;
    case CSSPropertyTextAnchor:
      return valueID == CSSValueStart || valueID == CSSValueMiddle ||
             valueID == CSSValueEnd;
    case CSSPropertyTextCombineUpright:
      return valueID == CSSValueNone || valueID == CSSValueAll;
    case CSSPropertyTextDecorationStyle:
      ASSERT(RuntimeEnabledFeatures::css3TextDecorationsEnabled());
      return valueID == CSSValueSolid || valueID == CSSValueDouble ||
             valueID == CSSValueDotted || valueID == CSSValueDashed ||
             valueID == CSSValueWavy;
    case CSSPropertyTextJustify:
      ASSERT(RuntimeEnabledFeatures::css3TextEnabled());
      return valueID == CSSValueInterWord || valueID == CSSValueDistribute ||
             valueID == CSSValueAuto || valueID == CSSValueNone;
    case CSSPropertyTextOrientation:
      return valueID == CSSValueMixed || valueID == CSSValueUpright ||
             valueID == CSSValueSideways || valueID == CSSValueSidewaysRight;
    case CSSPropertyWebkitTextOrientation:
      return valueID == CSSValueSideways || valueID == CSSValueSidewaysRight ||
             valueID == CSSValueVerticalRight || valueID == CSSValueUpright;
    case CSSPropertyTextOverflow:
      return valueID == CSSValueClip || valueID == CSSValueEllipsis;
    case CSSPropertyTextRendering:
      return valueID == CSSValueAuto || valueID == CSSValueOptimizeSpeed ||
             valueID == CSSValueOptimizeLegibility ||
             valueID == CSSValueGeometricPrecision;
    case CSSPropertyTextTransform:  // capitalize | uppercase | lowercase | none
      return (valueID >= CSSValueCapitalize && valueID <= CSSValueLowercase) ||
             valueID == CSSValueNone;
    case CSSPropertyUnicodeBidi:
      return valueID == CSSValueNormal || valueID == CSSValueEmbed ||
             valueID == CSSValueBidiOverride ||
             valueID == CSSValueWebkitIsolate ||
             valueID == CSSValueWebkitIsolateOverride ||
             valueID == CSSValueWebkitPlaintext || valueID == CSSValueIsolate ||
             valueID == CSSValueIsolateOverride || valueID == CSSValuePlaintext;
    case CSSPropertyVectorEffect:
      return valueID == CSSValueNone || valueID == CSSValueNonScalingStroke;
    case CSSPropertyVisibility:
      return valueID == CSSValueVisible || valueID == CSSValueHidden ||
             valueID == CSSValueCollapse;
    case CSSPropertyWebkitAppRegion:
      return valueID >= CSSValueDrag && valueID <= CSSValueNoDrag;
    case CSSPropertyWebkitAppearance:
      return (valueID >= CSSValueCheckbox && valueID <= CSSValueTextarea) ||
             valueID == CSSValueNone;
    case CSSPropertyBackfaceVisibility:
      return valueID == CSSValueVisible || valueID == CSSValueHidden;
    case CSSPropertyMixBlendMode:
      return valueID == CSSValueNormal || valueID == CSSValueMultiply ||
             valueID == CSSValueScreen || valueID == CSSValueOverlay ||
             valueID == CSSValueDarken || valueID == CSSValueLighten ||
             valueID == CSSValueColorDodge || valueID == CSSValueColorBurn ||
             valueID == CSSValueHardLight || valueID == CSSValueSoftLight ||
             valueID == CSSValueDifference || valueID == CSSValueExclusion ||
             valueID == CSSValueHue || valueID == CSSValueSaturation ||
             valueID == CSSValueColor || valueID == CSSValueLuminosity;
    case CSSPropertyWebkitBoxAlign:
      return valueID == CSSValueStretch || valueID == CSSValueStart ||
             valueID == CSSValueEnd || valueID == CSSValueCenter ||
             valueID == CSSValueBaseline;
    case CSSPropertyWebkitBoxDecorationBreak:
      return valueID == CSSValueClone || valueID == CSSValueSlice;
    case CSSPropertyWebkitBoxDirection:
      return valueID == CSSValueNormal || valueID == CSSValueReverse;
    case CSSPropertyWebkitBoxLines:
      return valueID == CSSValueSingle || valueID == CSSValueMultiple;
    case CSSPropertyWebkitBoxOrient:
      return valueID == CSSValueHorizontal || valueID == CSSValueVertical ||
             valueID == CSSValueInlineAxis || valueID == CSSValueBlockAxis;
    case CSSPropertyWebkitBoxPack:
      return valueID == CSSValueStart || valueID == CSSValueEnd ||
             valueID == CSSValueCenter || valueID == CSSValueJustify;
    case CSSPropertyColumnFill:
      return valueID == CSSValueAuto || valueID == CSSValueBalance;
    case CSSPropertyAlignContent:
      // FIXME: Per CSS alignment, this property should accept an optional
      // <overflow-position>. We should share this parsing code with
      // 'justify-self'.
      return valueID == CSSValueFlexStart || valueID == CSSValueFlexEnd ||
             valueID == CSSValueCenter || valueID == CSSValueSpaceBetween ||
             valueID == CSSValueSpaceAround || valueID == CSSValueStretch;
    case CSSPropertyAlignItems:
      // FIXME: Per CSS alignment, this property should accept the same
      // arguments as 'justify-self' so we should share its parsing code.
      return valueID == CSSValueFlexStart || valueID == CSSValueFlexEnd ||
             valueID == CSSValueCenter || valueID == CSSValueBaseline ||
             valueID == CSSValueStretch;
    case CSSPropertyAlignSelf:
      // FIXME: Per CSS alignment, this property should accept the same
      // arguments as 'justify-self' so we should share its parsing code.
      return valueID == CSSValueAuto || valueID == CSSValueFlexStart ||
             valueID == CSSValueFlexEnd || valueID == CSSValueCenter ||
             valueID == CSSValueBaseline || valueID == CSSValueStretch;
    case CSSPropertyFlexDirection:
      return valueID == CSSValueRow || valueID == CSSValueRowReverse ||
             valueID == CSSValueColumn || valueID == CSSValueColumnReverse;
    case CSSPropertyFlexWrap:
      return valueID == CSSValueNowrap || valueID == CSSValueWrap ||
             valueID == CSSValueWrapReverse;
    case CSSPropertyHyphens:
#if OS(ANDROID) || OS(MACOSX)
      return valueID == CSSValueAuto || valueID == CSSValueNone ||
             valueID == CSSValueManual;
#else
      return valueID == CSSValueNone || valueID == CSSValueManual;
#endif
    case CSSPropertyJustifyContent:
      // FIXME: Per CSS alignment, this property should accept an optional
      // <overflow-position>. We should share this parsing code with
      // 'justify-self'.
      return valueID == CSSValueFlexStart || valueID == CSSValueFlexEnd ||
             valueID == CSSValueCenter || valueID == CSSValueSpaceBetween ||
             valueID == CSSValueSpaceAround;
    case CSSPropertyFontKerning:
      return valueID == CSSValueAuto || valueID == CSSValueNormal ||
             valueID == CSSValueNone;
    case CSSPropertyWebkitFontSmoothing:
      return valueID == CSSValueAuto || valueID == CSSValueNone ||
             valueID == CSSValueAntialiased ||
             valueID == CSSValueSubpixelAntialiased;
    case CSSPropertyWebkitLineBreak:
      return valueID == CSSValueAuto || valueID == CSSValueLoose ||
             valueID == CSSValueNormal || valueID == CSSValueStrict ||
             valueID == CSSValueAfterWhiteSpace;
    case CSSPropertyWebkitMarginAfterCollapse:
    case CSSPropertyWebkitMarginBeforeCollapse:
    case CSSPropertyWebkitMarginBottomCollapse:
    case CSSPropertyWebkitMarginTopCollapse:
      return valueID == CSSValueCollapse || valueID == CSSValueSeparate ||
             valueID == CSSValueDiscard;
    case CSSPropertyWebkitPrintColorAdjust:
      return valueID == CSSValueExact || valueID == CSSValueEconomy;
    case CSSPropertyWebkitRtlOrdering:
      return valueID == CSSValueLogical || valueID == CSSValueVisual;
    case CSSPropertyWebkitRubyPosition:
      return valueID == CSSValueBefore || valueID == CSSValueAfter;
    case CSSPropertyWebkitTextCombine:
      return valueID == CSSValueNone || valueID == CSSValueHorizontal;
    case CSSPropertyWebkitTextEmphasisPosition:
      return valueID == CSSValueOver || valueID == CSSValueUnder;
    case CSSPropertyWebkitTextSecurity:
      return valueID == CSSValueDisc || valueID == CSSValueCircle ||
             valueID == CSSValueSquare || valueID == CSSValueNone;
    case CSSPropertyTransformStyle:
      return valueID == CSSValueFlat || valueID == CSSValuePreserve3d;
    case CSSPropertyWebkitUserDrag:
      return valueID == CSSValueAuto || valueID == CSSValueNone ||
             valueID == CSSValueElement;
    case CSSPropertyWebkitUserModify:
      return valueID == CSSValueReadOnly || valueID == CSSValueReadWrite ||
             valueID == CSSValueReadWritePlaintextOnly;
    case CSSPropertyUserSelect:
      return valueID == CSSValueAuto || valueID == CSSValueNone ||
             valueID == CSSValueText || valueID == CSSValueAll;
    case CSSPropertyWebkitWritingMode:
      return valueID >= CSSValueHorizontalTb && valueID <= CSSValueVerticalLr;
    case CSSPropertyWritingMode:
      return valueID == CSSValueHorizontalTb || valueID == CSSValueVerticalRl ||
             valueID == CSSValueVerticalLr || valueID == CSSValueLrTb ||
             valueID == CSSValueRlTb || valueID == CSSValueTbRl ||
             valueID == CSSValueLr || valueID == CSSValueRl ||
             valueID == CSSValueTb;
    case CSSPropertyWhiteSpace:
      return valueID == CSSValueNormal || valueID == CSSValuePre ||
             valueID == CSSValuePreWrap || valueID == CSSValuePreLine ||
             valueID == CSSValueNowrap;
    case CSSPropertyWordBreak:
      return valueID == CSSValueNormal || valueID == CSSValueBreakAll ||
             valueID == CSSValueKeepAll || valueID == CSSValueBreakWord;
    case CSSPropertyScrollSnapType:
      ASSERT(RuntimeEnabledFeatures::cssScrollSnapPointsEnabled());
      return valueID == CSSValueNone || valueID == CSSValueMandatory ||
             valueID == CSSValueProximity;
    default:
      ASSERT_NOT_REACHED();
      return false;
  }
}

bool CSSParserFastPaths::isKeywordPropertyID(CSSPropertyID propertyId) {
  switch (propertyId) {
    case CSSPropertyAlignmentBaseline:
    case CSSPropertyAll:
    case CSSPropertyMixBlendMode:
    case CSSPropertyIsolation:
    case CSSPropertyBackgroundRepeatX:
    case CSSPropertyBackgroundRepeatY:
    case CSSPropertyBorderBottomStyle:
    case CSSPropertyBorderCollapse:
    case CSSPropertyBorderLeftStyle:
    case CSSPropertyBorderRightStyle:
    case CSSPropertyBorderTopStyle:
    case CSSPropertyBoxSizing:
    case CSSPropertyBufferedRendering:
    case CSSPropertyCaptionSide:
    case CSSPropertyClear:
    case CSSPropertyClipRule:
    case CSSPropertyColorInterpolation:
    case CSSPropertyColorInterpolationFilters:
    case CSSPropertyColorRendering:
    case CSSPropertyDirection:
    case CSSPropertyDisplay:
    case CSSPropertyDominantBaseline:
    case CSSPropertyEmptyCells:
    case CSSPropertyFillRule:
    case CSSPropertyFloat:
    case CSSPropertyFontStyle:
    case CSSPropertyFontStretch:
    case CSSPropertyHyphens:
    case CSSPropertyImageRendering:
    case CSSPropertyListStylePosition:
    case CSSPropertyListStyleType:
    case CSSPropertyMaskType:
    case CSSPropertyObjectFit:
    case CSSPropertyOutlineStyle:
    case CSSPropertyOverflowAnchor:
    case CSSPropertyOverflowWrap:
    case CSSPropertyOverflowX:
    case CSSPropertyOverflowY:
    case CSSPropertyBreakAfter:
    case CSSPropertyBreakBefore:
    case CSSPropertyBreakInside:
    case CSSPropertyPointerEvents:
    case CSSPropertyPosition:
    case CSSPropertyResize:
    case CSSPropertyScrollBehavior:
    case CSSPropertyShapeRendering:
    case CSSPropertySpeak:
    case CSSPropertyStrokeLinecap:
    case CSSPropertyStrokeLinejoin:
    case CSSPropertyTableLayout:
    case CSSPropertyTextAlign:
    case CSSPropertyTextAlignLast:
    case CSSPropertyTextAnchor:
    case CSSPropertyTextCombineUpright:
    case CSSPropertyTextDecorationStyle:
    case CSSPropertyTextJustify:
    case CSSPropertyTextOrientation:
    case CSSPropertyWebkitTextOrientation:
    case CSSPropertyTextOverflow:
    case CSSPropertyTextRendering:
    case CSSPropertyTextTransform:
    case CSSPropertyUnicodeBidi:
    case CSSPropertyVectorEffect:
    case CSSPropertyVisibility:
    case CSSPropertyWebkitAppRegion:
    case CSSPropertyWebkitAppearance:
    case CSSPropertyBackfaceVisibility:
    case CSSPropertyWebkitBorderAfterStyle:
    case CSSPropertyWebkitBorderBeforeStyle:
    case CSSPropertyWebkitBorderEndStyle:
    case CSSPropertyWebkitBorderStartStyle:
    case CSSPropertyWebkitBoxAlign:
    case CSSPropertyWebkitBoxDecorationBreak:
    case CSSPropertyWebkitBoxDirection:
    case CSSPropertyWebkitBoxLines:
    case CSSPropertyWebkitBoxOrient:
    case CSSPropertyWebkitBoxPack:
    case CSSPropertyColumnFill:
    case CSSPropertyColumnRuleStyle:
    case CSSPropertyFlexDirection:
    case CSSPropertyFlexWrap:
    case CSSPropertyFontKerning:
    case CSSPropertyWebkitFontSmoothing:
    case CSSPropertyWebkitLineBreak:
    case CSSPropertyWebkitMarginAfterCollapse:
    case CSSPropertyWebkitMarginBeforeCollapse:
    case CSSPropertyWebkitMarginBottomCollapse:
    case CSSPropertyWebkitMarginTopCollapse:
    case CSSPropertyWebkitPrintColorAdjust:
    case CSSPropertyWebkitRtlOrdering:
    case CSSPropertyWebkitRubyPosition:
    case CSSPropertyWebkitTextCombine:
    case CSSPropertyWebkitTextEmphasisPosition:
    case CSSPropertyWebkitTextSecurity:
    case CSSPropertyTransformStyle:
    case CSSPropertyWebkitUserDrag:
    case CSSPropertyWebkitUserModify:
    case CSSPropertyUserSelect:
    case CSSPropertyWebkitWritingMode:
    case CSSPropertyWhiteSpace:
    case CSSPropertyWordBreak:
    case CSSPropertyWordWrap:
    case CSSPropertyWritingMode:
    case CSSPropertyScrollSnapType:
      return true;
    case CSSPropertyJustifyContent:
    case CSSPropertyAlignContent:
    case CSSPropertyAlignItems:
    case CSSPropertyAlignSelf:
      return !RuntimeEnabledFeatures::cssGridLayoutEnabled();
    default:
      return false;
  }
}

static CSSValue* parseKeywordValue(CSSPropertyID propertyId,
                                   const String& string,
                                   CSSParserMode parserMode) {
  ASSERT(!string.isEmpty());

  if (!CSSParserFastPaths::isKeywordPropertyID(propertyId)) {
    // All properties accept the values of "initial" and "inherit".
    if (!equalIgnoringASCIICase(string, "initial") &&
        !equalIgnoringASCIICase(string, "inherit"))
      return nullptr;

    // Parse initial/inherit shorthands using the CSSPropertyParser.
    if (shorthandForProperty(propertyId).length())
      return nullptr;

    // Descriptors do not support css wide keywords.
    if (CSSPropertyMetadata::isDescriptorOnly(propertyId))
      return nullptr;
  }

  CSSValueID valueID = cssValueKeywordID(string);

  if (!valueID)
    return nullptr;

  if (valueID == CSSValueInherit)
    return CSSInheritedValue::create();
  if (valueID == CSSValueInitial)
    return CSSInitialValue::create();
  if (CSSParserFastPaths::isValidKeywordPropertyAndValue(propertyId, valueID,
                                                         parserMode))
    return CSSIdentifierValue::create(valueID);
  return nullptr;
}

template <typename CharType>
static bool parseTransformTranslateArguments(CharType*& pos,
                                             CharType* end,
                                             unsigned expectedCount,
                                             CSSFunctionValue* transformValue) {
  while (expectedCount) {
    size_t delimiter =
        WTF::find(pos, end - pos, expectedCount == 1 ? ')' : ',');
    if (delimiter == kNotFound)
      return false;
    unsigned argumentLength = static_cast<unsigned>(delimiter);
    CSSPrimitiveValue::UnitType unit = CSSPrimitiveValue::UnitType::Number;
    double number;
    if (!parseSimpleLength(pos, argumentLength, unit, number))
      return false;
    if (unit != CSSPrimitiveValue::UnitType::Pixels &&
        (number || unit != CSSPrimitiveValue::UnitType::Number))
      return false;
    transformValue->append(*CSSPrimitiveValue::create(
        number, CSSPrimitiveValue::UnitType::Pixels));
    pos += argumentLength + 1;
    --expectedCount;
  }
  return true;
}

template <typename CharType>
static bool parseTransformNumberArguments(CharType*& pos,
                                          CharType* end,
                                          unsigned expectedCount,
                                          CSSFunctionValue* transformValue) {
  while (expectedCount) {
    size_t delimiter =
        WTF::find(pos, end - pos, expectedCount == 1 ? ')' : ',');
    if (delimiter == kNotFound)
      return false;
    unsigned argumentLength = static_cast<unsigned>(delimiter);
    bool ok;
    double number = charactersToDouble(pos, argumentLength, &ok);
    if (!ok)
      return false;
    transformValue->append(*CSSPrimitiveValue::create(
        number, CSSPrimitiveValue::UnitType::Number));
    pos += argumentLength + 1;
    --expectedCount;
  }
  return true;
}

static const int kShortestValidTransformStringLength = 12;

template <typename CharType>
static CSSFunctionValue* parseSimpleTransformValue(CharType*& pos,
                                                   CharType* end) {
  if (end - pos < kShortestValidTransformStringLength)
    return nullptr;

  const bool isTranslate =
      toASCIILower(pos[0]) == 't' && toASCIILower(pos[1]) == 'r' &&
      toASCIILower(pos[2]) == 'a' && toASCIILower(pos[3]) == 'n' &&
      toASCIILower(pos[4]) == 's' && toASCIILower(pos[5]) == 'l' &&
      toASCIILower(pos[6]) == 'a' && toASCIILower(pos[7]) == 't' &&
      toASCIILower(pos[8]) == 'e';

  if (isTranslate) {
    CSSValueID transformType;
    unsigned expectedArgumentCount = 1;
    unsigned argumentStart = 11;
    CharType c9 = toASCIILower(pos[9]);
    if (c9 == 'x' && pos[10] == '(') {
      transformType = CSSValueTranslateX;
    } else if (c9 == 'y' && pos[10] == '(') {
      transformType = CSSValueTranslateY;
    } else if (c9 == 'z' && pos[10] == '(') {
      transformType = CSSValueTranslateZ;
    } else if (c9 == '(') {
      transformType = CSSValueTranslate;
      expectedArgumentCount = 2;
      argumentStart = 10;
    } else if (c9 == '3' && toASCIILower(pos[10]) == 'd' && pos[11] == '(') {
      transformType = CSSValueTranslate3d;
      expectedArgumentCount = 3;
      argumentStart = 12;
    } else {
      return nullptr;
    }
    pos += argumentStart;
    CSSFunctionValue* transformValue = CSSFunctionValue::create(transformType);
    if (!parseTransformTranslateArguments(pos, end, expectedArgumentCount,
                                          transformValue))
      return nullptr;
    return transformValue;
  }

  const bool isMatrix3d =
      toASCIILower(pos[0]) == 'm' && toASCIILower(pos[1]) == 'a' &&
      toASCIILower(pos[2]) == 't' && toASCIILower(pos[3]) == 'r' &&
      toASCIILower(pos[4]) == 'i' && toASCIILower(pos[5]) == 'x' &&
      pos[6] == '3' && toASCIILower(pos[7]) == 'd' && pos[8] == '(';

  if (isMatrix3d) {
    pos += 9;
    CSSFunctionValue* transformValue =
        CSSFunctionValue::create(CSSValueMatrix3d);
    if (!parseTransformNumberArguments(pos, end, 16, transformValue))
      return nullptr;
    return transformValue;
  }

  const bool isScale3d =
      toASCIILower(pos[0]) == 's' && toASCIILower(pos[1]) == 'c' &&
      toASCIILower(pos[2]) == 'a' && toASCIILower(pos[3]) == 'l' &&
      toASCIILower(pos[4]) == 'e' && pos[5] == '3' &&
      toASCIILower(pos[6]) == 'd' && pos[7] == '(';

  if (isScale3d) {
    pos += 8;
    CSSFunctionValue* transformValue =
        CSSFunctionValue::create(CSSValueScale3d);
    if (!parseTransformNumberArguments(pos, end, 3, transformValue))
      return nullptr;
    return transformValue;
  }

  return nullptr;
}

template <typename CharType>
static bool transformCanLikelyUseFastPath(const CharType* chars,
                                          unsigned length) {
  // Very fast scan that attempts to reject most transforms that couldn't
  // take the fast path. This avoids doing the malloc and string->double
  // conversions in parseSimpleTransformValue only to discard them when we
  // run into a transform component we don't understand.
  unsigned i = 0;
  while (i < length) {
    if (isCSSSpace(chars[i])) {
      ++i;
      continue;
    }
    if (length - i < kShortestValidTransformStringLength)
      return false;
    switch (toASCIILower(chars[i])) {
      case 't':
        // translate, translateX, translateY, translateZ, translate3d.
        if (toASCIILower(chars[i + 8]) != 'e')
          return false;
        i += 9;
        break;
      case 'm':
        // matrix3d.
        if (toASCIILower(chars[i + 7]) != 'd')
          return false;
        i += 8;
        break;
      case 's':
        // scale3d.
        if (toASCIILower(chars[i + 6]) != 'd')
          return false;
        i += 7;
        break;
      default:
        // All other things, ex. rotate.
        return false;
    }
    size_t argumentsEnd = WTF::find(chars, length, ')', i);
    if (argumentsEnd == kNotFound)
      return false;
    // Advance to the end of the arguments.
    i = argumentsEnd + 1;
  }
  return i == length;
}

template <typename CharType>
static CSSValueList* parseSimpleTransformList(const CharType* chars,
                                              unsigned length) {
  if (!transformCanLikelyUseFastPath(chars, length))
    return nullptr;
  const CharType*& pos = chars;
  const CharType* end = chars + length;
  CSSValueList* transformList = nullptr;
  while (pos < end) {
    while (pos < end && isCSSSpace(*pos))
      ++pos;
    if (pos >= end)
      break;
    CSSFunctionValue* transformValue = parseSimpleTransformValue(pos, end);
    if (!transformValue)
      return nullptr;
    if (!transformList)
      transformList = CSSValueList::createSpaceSeparated();
    transformList->append(*transformValue);
  }
  return transformList;
}

static CSSValue* parseSimpleTransform(CSSPropertyID propertyID,
                                      const String& string) {
  ASSERT(!string.isEmpty());

  if (propertyID != CSSPropertyTransform)
    return nullptr;
  if (string.is8Bit())
    return parseSimpleTransformList(string.characters8(), string.length());
  return parseSimpleTransformList(string.characters16(), string.length());
}

CSSValue* CSSParserFastPaths::maybeParseValue(CSSPropertyID propertyID,
                                              const String& string,
                                              CSSParserMode parserMode) {
  if (CSSValue* length = parseSimpleLengthValue(propertyID, string, parserMode))
    return length;
  if (isColorPropertyID(propertyID))
    return parseColor(string, parserMode);
  if (CSSValue* keyword = parseKeywordValue(propertyID, string, parserMode))
    return keyword;
  if (CSSValue* transform = parseSimpleTransform(propertyID, string))
    return transform;
  return nullptr;
}

}  // namespace blink
