/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 - 2010  Torch Mobile (Beijing) Co. Ltd. All rights
 * reserved.
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

#ifndef CSSPropertyParser_h
#define CSSPropertyParser_h

#include "core/css/StyleRule.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "wtf/text/StringView.h"

namespace blink {

class CSSProperty;
class CSSValue;
class StylePropertyShorthand;

// Inputs: PropertyID, isImportant bool, CSSParserTokenRange.
// Outputs: Vector of CSSProperties

class CSSPropertyParser {
  WTF_MAKE_NONCOPYABLE(CSSPropertyParser);
  STACK_ALLOCATED();

 public:
  static bool parseValue(CSSPropertyID,
                         bool important,
                         const CSSParserTokenRange&,
                         const CSSParserContext*,
                         HeapVector<CSSProperty, 256>&,
                         StyleRule::RuleType);

  // Parses a non-shorthand CSS property
  static const CSSValue* parseSingleValue(CSSPropertyID,
                                          const CSSParserTokenRange&,
                                          const CSSParserContext*);

 private:
  CSSPropertyParser(const CSSParserTokenRange&,
                    const CSSParserContext*,
                    HeapVector<CSSProperty, 256>*);

  // TODO(timloh): Rename once the CSSParserValue-based parseValue is removed
  bool parseValueStart(CSSPropertyID unresolvedProperty, bool important);
  bool consumeCSSWideKeyword(CSSPropertyID unresolvedProperty, bool important);
  const CSSValue* parseSingleValue(CSSPropertyID,
                                   CSSPropertyID = CSSPropertyInvalid);

  bool inQuirksMode() const { return isQuirksModeBehavior(m_context->mode()); }

  bool parseViewportDescriptor(CSSPropertyID propId, bool important);
  bool parseFontFaceDescriptor(CSSPropertyID);

  void addProperty(CSSPropertyID,
                   CSSPropertyID,
                   const CSSValue&,
                   bool important,
                   bool implicit = false);
  void addExpandedPropertyForValue(CSSPropertyID propId, const CSSValue&, bool);

  bool consumeBorder(bool important);

  bool parseShorthand(CSSPropertyID, bool important);
  bool consumeShorthandGreedily(const StylePropertyShorthand&, bool important);
  bool consume4Values(const StylePropertyShorthand&, bool important);

  // Legacy parsing allows <string>s for animation-name
  bool consumeAnimationShorthand(const StylePropertyShorthand&,
                                 bool useLegacyParsing,
                                 bool important);
  bool consumeBackgroundShorthand(const StylePropertyShorthand&,
                                  bool important);
  bool consumeOffsetShorthand(bool important);

  bool consumeColumns(bool important);

  bool consumeGridItemPositionShorthand(CSSPropertyID, bool important);
  bool consumeGridTemplateRowsAndAreasAndColumns(CSSPropertyID, bool important);
  bool consumeGridTemplateShorthand(CSSPropertyID, bool important);
  bool consumeGridShorthand(bool important);
  bool consumeGridAreaShorthand(bool important);

  bool consumeFont(bool important);
  bool consumeFontVariantShorthand(bool important);
  bool consumeSystemFont(bool important);

  bool consumeBorderSpacing(bool important);

  // CSS3 Parsing Routines (for properties specific to CSS3)
  bool consumeBorderImage(CSSPropertyID, bool important);

  bool consumeFlex(bool important);

  bool consumeLegacyBreakProperty(CSSPropertyID, bool important);

 private:
  // Inputs:
  CSSParserTokenRange m_range;
  Member<const CSSParserContext> m_context;
  // Outputs:
  HeapVector<CSSProperty, 256>* m_parsedProperties;
};

CSSPropertyID unresolvedCSSPropertyID(StringView);
CSSValueID cssValueKeywordID(StringView);

}  // namespace blink

#endif  // CSSPropertyParser_h
