// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyParserHelpers_h
#define CSSPropertyParserHelpers_h

#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "platform/Length.h"  // For ValueRange
#include "platform/heap/Handle.h"

namespace blink {

class CSSParserContext;
class CSSStringValue;
class CSSURIValue;
class CSSValuePair;

// When these functions are successful, they will consume all the relevant
// tokens from the range and also consume any whitespace which follows. When
// the start of the range doesn't match the type we're looking for, the range
// will not be modified.
namespace CSSPropertyParserHelpers {

void complete4Sides(CSSValue* side[4]);

// TODO(timloh): These should probably just be consumeComma and consumeSlash.
bool consumeCommaIncludingWhitespace(CSSParserTokenRange&);
bool consumeSlashIncludingWhitespace(CSSParserTokenRange&);
// consumeFunction expects the range starts with a FunctionToken.
CSSParserTokenRange consumeFunction(CSSParserTokenRange&);

enum class UnitlessQuirk { Allow, Forbid };

CSSPrimitiveValue* consumeInteger(
    CSSParserTokenRange&,
    double minimumValue = -std::numeric_limits<double>::max());
CSSPrimitiveValue* consumePositiveInteger(CSSParserTokenRange&);
bool consumeNumberRaw(CSSParserTokenRange&, double& result);
CSSPrimitiveValue* consumeNumber(CSSParserTokenRange&, ValueRange);
CSSPrimitiveValue* consumeLength(CSSParserTokenRange&,
                                 CSSParserMode,
                                 ValueRange,
                                 UnitlessQuirk = UnitlessQuirk::Forbid);
CSSPrimitiveValue* consumePercent(CSSParserTokenRange&, ValueRange);
CSSPrimitiveValue* consumeLengthOrPercent(
    CSSParserTokenRange&,
    CSSParserMode,
    ValueRange,
    UnitlessQuirk = UnitlessQuirk::Forbid);
CSSPrimitiveValue* consumeAngle(CSSParserTokenRange&);
CSSPrimitiveValue* consumeTime(CSSParserTokenRange&, ValueRange);
CSSPrimitiveValue* consumeResolution(CSSParserTokenRange&);

CSSIdentifierValue* consumeIdent(CSSParserTokenRange&);
CSSIdentifierValue* consumeIdentRange(CSSParserTokenRange&,
                                      CSSValueID lower,
                                      CSSValueID upper);
template <CSSValueID, CSSValueID...>
inline bool identMatches(CSSValueID id);
template <CSSValueID... allowedIdents>
CSSIdentifierValue* consumeIdent(CSSParserTokenRange&);

CSSCustomIdentValue* consumeCustomIdent(CSSParserTokenRange&);
CSSStringValue* consumeString(CSSParserTokenRange&);
StringView consumeUrlAsStringView(CSSParserTokenRange&);
CSSURIValue* consumeUrl(CSSParserTokenRange&, const CSSParserContext*);

CSSValue* consumeColor(CSSParserTokenRange&,
                       CSSParserMode,
                       bool acceptQuirkyColors = false);

CSSValue* consumeLineWidth(CSSParserTokenRange&, CSSParserMode, UnitlessQuirk);

CSSValuePair* consumePosition(CSSParserTokenRange&,
                              CSSParserMode,
                              UnitlessQuirk);
bool consumePosition(CSSParserTokenRange&,
                     CSSParserMode,
                     UnitlessQuirk,
                     CSSValue*& resultX,
                     CSSValue*& resultY);
bool consumeOneOrTwoValuedPosition(CSSParserTokenRange&,
                                   CSSParserMode,
                                   UnitlessQuirk,
                                   CSSValue*& resultX,
                                   CSSValue*& resultY);

enum class ConsumeGeneratedImagePolicy { Allow, Forbid };

CSSValue* consumeImage(
    CSSParserTokenRange&,
    const CSSParserContext*,
    ConsumeGeneratedImagePolicy = ConsumeGeneratedImagePolicy::Allow);
CSSValue* consumeImageOrNone(CSSParserTokenRange&, const CSSParserContext*);

bool isCSSWideKeyword(StringView);

CSSIdentifierValue* consumeShapeBox(CSSParserTokenRange&);

// Template implementations are at the bottom of the file for readability.

template <typename... emptyBaseCase>
inline bool identMatches(CSSValueID id) {
  return false;
}
template <CSSValueID head, CSSValueID... tail>
inline bool identMatches(CSSValueID id) {
  return id == head || identMatches<tail...>(id);
}

template <CSSValueID... names>
CSSIdentifierValue* consumeIdent(CSSParserTokenRange& range) {
  if (range.peek().type() != IdentToken ||
      !identMatches<names...>(range.peek().id()))
    return nullptr;
  return CSSIdentifierValue::create(range.consumeIncludingWhitespace().id());
}

}  // namespace CSSPropertyParserHelpers

}  // namespace blink

#endif  // CSSPropertyParserHelpers_h
