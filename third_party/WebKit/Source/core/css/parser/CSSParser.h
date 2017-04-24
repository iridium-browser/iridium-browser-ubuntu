// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParser_h
#define CSSParser_h

#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/css/StylePropertySet.h"
#include "core/css/parser/CSSParserContext.h"
#include <memory>

namespace blink {

class Color;
class CSSParserObserver;
class CSSParserTokenRange;
class CSSSelectorList;
class Element;
class ImmutableStylePropertySet;
class StyleRuleBase;
class StyleRuleKeyframe;
class StyleSheetContents;
class CSSValue;

// This class serves as the public API for the css/parser subsystem
class CORE_EXPORT CSSParser {
  STATIC_ONLY(CSSParser);

 public:
  // As well as regular rules, allows @import and @namespace but not @charset
  static StyleRuleBase* parseRule(const CSSParserContext*,
                                  StyleSheetContents*,
                                  const String&);
  static void parseSheet(const CSSParserContext*,
                         StyleSheetContents*,
                         const String&,
                         bool deferPropertyParsing = false);
  static CSSSelectorList parseSelector(const CSSParserContext*,
                                       StyleSheetContents*,
                                       const String&);
  static CSSSelectorList parsePageSelector(const CSSParserContext*,
                                           StyleSheetContents*,
                                           const String&);
  static bool parseDeclarationList(const CSSParserContext*,
                                   MutableStylePropertySet*,
                                   const String&);

  static MutableStylePropertySet::SetResult parseValue(
      MutableStylePropertySet*,
      CSSPropertyID unresolvedProperty,
      const String&,
      bool important);
  static MutableStylePropertySet::SetResult parseValue(
      MutableStylePropertySet*,
      CSSPropertyID unresolvedProperty,
      const String&,
      bool important,
      StyleSheetContents*);

  static MutableStylePropertySet::SetResult parseValueForCustomProperty(
      MutableStylePropertySet*,
      const AtomicString& propertyName,
      const PropertyRegistry*,
      const String& value,
      bool important,
      StyleSheetContents*,
      bool isAnimationTainted);
  static ImmutableStylePropertySet* parseCustomPropertySet(CSSParserTokenRange);

  // This is for non-shorthands only
  static const CSSValue* parseSingleValue(
      CSSPropertyID,
      const String&,
      const CSSParserContext* = strictCSSParserContext());

  static const CSSValue* parseFontFaceDescriptor(CSSPropertyID,
                                                 const String&,
                                                 const CSSParserContext*);

  static ImmutableStylePropertySet* parseInlineStyleDeclaration(const String&,
                                                                Element*);

  static std::unique_ptr<Vector<double>> parseKeyframeKeyList(const String&);
  static StyleRuleKeyframe* parseKeyframeRule(const CSSParserContext*,
                                              const String&);

  static bool parseSupportsCondition(const String&);

  // The color will only be changed when string contains a valid CSS color, so
  // callers can set it to a default color and ignore the boolean result.
  static bool parseColor(Color&, const String&, bool strict = false);
  static bool parseSystemColor(Color&, const String&);

  static void parseSheetForInspector(const CSSParserContext*,
                                     StyleSheetContents*,
                                     const String&,
                                     CSSParserObserver&);
  static void parseDeclarationListForInspector(const CSSParserContext*,
                                               const String&,
                                               CSSParserObserver&);

 private:
  static MutableStylePropertySet::SetResult parseValue(
      MutableStylePropertySet*,
      CSSPropertyID unresolvedProperty,
      const String&,
      bool important,
      const CSSParserContext*);
};

}  // namespace blink

#endif  // CSSParser_h
