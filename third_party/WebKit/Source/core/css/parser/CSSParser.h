// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParser_h
#define CSSParser_h

#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/css/CSSValue.h"
#include "core/css/parser/CSSParserMode.h"
#include "platform/graphics/Color.h"

namespace blink {

class CSSParserObserver;
class CSSSelectorList;
class Element;
class ImmutableStylePropertySet;
class MutableStylePropertySet;
class StyleColor;
class StyleRuleBase;
class StyleRuleKeyframe;
class StyleSheetContents;

// This class serves as the public API for the css/parser subsystem
class CORE_EXPORT CSSParser {
    STATIC_ONLY(CSSParser);
public:
    // As well as regular rules, allows @import and @namespace but not @charset
    static PassRefPtrWillBeRawPtr<StyleRuleBase> parseRule(const CSSParserContext&, StyleSheetContents*, const String&);
    // TODO(timloh): Split into parseSheet and parseSheetForInspector
    static void parseSheet(const CSSParserContext&, StyleSheetContents*, const String&, const TextPosition& startPosition, CSSParserObserver*, bool logErrors = false);
    static void parseSelector(const CSSParserContext&, const String&, CSSSelectorList&);
    // TODO(timloh): Split into parseDeclarationList and parseDeclarationListForInspector
    static bool parseDeclarationList(const CSSParserContext&, MutableStylePropertySet*, const String&, CSSParserObserver*, StyleSheetContents* contextStyleSheet);
    // Returns whether anything was changed.
    static bool parseValue(MutableStylePropertySet*, CSSPropertyID unresolvedProperty, const String&, bool important, CSSParserMode, StyleSheetContents*);

    // This is for non-shorthands only
    static PassRefPtrWillBeRawPtr<CSSValue> parseSingleValue(CSSPropertyID, const String&, const CSSParserContext& = strictCSSParserContext());

    static PassRefPtrWillBeRawPtr<CSSValue> parseFontFaceDescriptor(CSSPropertyID, const String&, const CSSParserContext&);

    static PassRefPtrWillBeRawPtr<ImmutableStylePropertySet> parseInlineStyleDeclaration(const String&, Element*);

    static PassOwnPtr<Vector<double>> parseKeyframeKeyList(const String&);
    static PassRefPtrWillBeRawPtr<StyleRuleKeyframe> parseKeyframeRule(const CSSParserContext&, StyleSheetContents*, const String&);

    static bool parseSupportsCondition(const String&);

    // The color will only be changed when string contains a valid CSS color, so callers
    // can set it to a default color and ignore the boolean result.
    static bool parseColor(RGBA32& color, const String&, bool strict = false);
    static bool parseSystemColor(RGBA32& color, const String&);
    static StyleColor colorFromRGBColorString(const String&);

private:
    static bool parseValue(MutableStylePropertySet*, CSSPropertyID unresolvedProperty, const String&, bool important, const CSSParserContext&);
};

// TODO(timloh): It's weird that these are declared here but defined in CSSPropertyParser.h
CSSPropertyID unresolvedCSSPropertyID(const String&);
CSSPropertyID cssPropertyID(const String&);

} // namespace blink

#endif // CSSParser_h
