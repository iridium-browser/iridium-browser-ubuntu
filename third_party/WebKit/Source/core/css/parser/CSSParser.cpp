// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/parser/CSSParser.h"

#include "core/css/CSSKeyframeRule.h"
#include "core/css/StyleColor.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParserFastPaths.h"
#include "core/css/parser/CSSParserImpl.h"
#include "core/css/parser/CSSSelectorParser.h"
#include "core/css/parser/CSSSupportsParser.h"
#include "core/css/parser/CSSTokenizer.h"
#include "core/layout/LayoutTheme.h"

namespace blink {

bool CSSParser::parseDeclarationList(const CSSParserContext& context, MutableStylePropertySet* propertySet, const String& declaration, CSSParserObserver* observer, StyleSheetContents* styleSheet)
{
    if (RuntimeEnabledFeatures::newCSSParserEnabled()) {
        if (observer) {
            CSSParserImpl::parseDeclarationListForInspector(declaration, context, *observer);
            return true;
        }
        return CSSParserImpl::parseDeclarationList(propertySet, declaration, context);
    }
    return BisonCSSParser(context).parseDeclaration(propertySet, declaration, observer, styleSheet);
}

void CSSParser::parseSelector(const CSSParserContext& context, const String& selector, CSSSelectorList& selectorList)
{
    if (RuntimeEnabledFeatures::newCSSParserEnabled()) {
        CSSTokenizer::Scope scope(selector);
        CSSSelectorParser::parseSelector(scope.tokenRange(), context, starAtom, nullptr, selectorList);
        return;
    }
    BisonCSSParser(context).parseSelector(selector, selectorList);
}

PassRefPtrWillBeRawPtr<StyleRuleBase> CSSParser::parseRule(const CSSParserContext& context, StyleSheetContents* styleSheet, const String& rule)
{
    if (RuntimeEnabledFeatures::newCSSParserEnabled())
        return CSSParserImpl::parseRule(rule, context, styleSheet, CSSParserImpl::AllowImportRules);
    return BisonCSSParser(context).parseRule(styleSheet, rule);
}

void CSSParser::parseSheet(const CSSParserContext& context, StyleSheetContents* styleSheet, const String& text, const TextPosition& startPosition, CSSParserObserver* observer, bool logErrors)
{
    if (RuntimeEnabledFeatures::newCSSParserEnabled()) {
        if (observer)
            return CSSParserImpl::parseStyleSheetForInspector(text, context, *observer);
        return CSSParserImpl::parseStyleSheet(text, context, styleSheet);
    }
    BisonCSSParser(context).parseSheet(styleSheet, text, startPosition, observer, logErrors);
}

bool CSSParser::parseValue(MutableStylePropertySet* declaration, CSSPropertyID unresolvedProperty, const String& string, bool important, CSSParserMode parserMode, StyleSheetContents* styleSheet)
{
    if (string.isEmpty())
        return false;
    CSSPropertyID resolvedProperty = resolveCSSPropertyID(unresolvedProperty);
    RefPtrWillBeRawPtr<CSSValue> value = CSSParserFastPaths::maybeParseValue(resolvedProperty, string, parserMode);
    if (value)
        return declaration->setProperty(CSSProperty(resolvedProperty, value.release(), important));
    CSSParserContext context(parserMode, 0);
    if (styleSheet) {
        context = styleSheet->parserContext();
        context.setMode(parserMode);
    }
    return parseValue(declaration, unresolvedProperty, string, important, context);
}

bool CSSParser::parseValue(MutableStylePropertySet* declaration, CSSPropertyID unresolvedProperty, const String& string, bool important, const CSSParserContext& context)
{
    if (RuntimeEnabledFeatures::newCSSParserEnabled())
        return CSSParserImpl::parseValue(declaration, unresolvedProperty, string, important, context);
    return BisonCSSParser::parseValue(declaration, unresolvedProperty, string, important, context);
}

PassRefPtrWillBeRawPtr<CSSValue> CSSParser::parseSingleValue(CSSPropertyID propertyID, const String& string, const CSSParserContext& context)
{
    if (string.isEmpty())
        return nullptr;
    if (RefPtrWillBeRawPtr<CSSValue> value = CSSParserFastPaths::maybeParseValue(propertyID, string, context.mode()))
        return value;
    RefPtrWillBeRawPtr<MutableStylePropertySet> stylePropertySet = MutableStylePropertySet::create();
    bool changed = parseValue(stylePropertySet.get(), propertyID, string, false, context);
    ASSERT_UNUSED(changed, changed == stylePropertySet->hasProperty(propertyID));
    return stylePropertySet->getPropertyCSSValue(propertyID);
}

PassRefPtrWillBeRawPtr<ImmutableStylePropertySet> CSSParser::parseInlineStyleDeclaration(const String& styleString, Element* element)
{
    if (RuntimeEnabledFeatures::newCSSParserEnabled())
        return CSSParserImpl::parseInlineStyleDeclaration(styleString, element);
    return BisonCSSParser::parseInlineStyleDeclaration(styleString, element);
}

PassOwnPtr<Vector<double>> CSSParser::parseKeyframeKeyList(const String& keyList)
{
    if (RuntimeEnabledFeatures::newCSSParserEnabled())
        return CSSParserImpl::parseKeyframeKeyList(keyList);
    return BisonCSSParser(strictCSSParserContext()).parseKeyframeKeyList(keyList);
}

PassRefPtrWillBeRawPtr<StyleRuleKeyframe> CSSParser::parseKeyframeRule(const CSSParserContext& context, StyleSheetContents* styleSheet, const String& rule)
{
    if (RuntimeEnabledFeatures::newCSSParserEnabled()) {
        RefPtrWillBeRawPtr<StyleRuleBase> keyframe = CSSParserImpl::parseRule(rule, context, nullptr, CSSParserImpl::KeyframeRules);
        return toStyleRuleKeyframe(keyframe.get());
    }
    return BisonCSSParser(context).parseKeyframeRule(styleSheet, rule);
}

bool CSSParser::parseSupportsCondition(const String& condition)
{
    if (RuntimeEnabledFeatures::newCSSParserEnabled()) {
        CSSTokenizer::Scope scope(condition);
        CSSParserImpl parser(strictCSSParserContext());
        return CSSSupportsParser::supportsCondition(scope.tokenRange(), parser) == CSSSupportsParser::Supported;
    }
    return BisonCSSParser(CSSParserContext(HTMLStandardMode, 0)).parseSupportsCondition(condition);
}

bool CSSParser::parseColor(RGBA32& color, const String& string, bool strict)
{
    if (string.isEmpty())
        return false;

    // First try creating a color specified by name, rgba(), rgb() or "#" syntax.
    if (CSSPropertyParser::fastParseColor(color, string, strict))
        return true;

    // In case the fast-path parser didn't understand the color, try the full parser.
    RefPtrWillBeRawPtr<MutableStylePropertySet> stylePropertySet = MutableStylePropertySet::create();
    // FIXME: The old CSS parser is only working in strict mode ignoring the strict parameter.
    // It needs to be investigated why.
    if (!parseValue(stylePropertySet.get(), CSSPropertyColor, string, false, strictCSSParserContext()))
        return false;

    RefPtrWillBeRawPtr<CSSValue> value = stylePropertySet->getPropertyCSSValue(CSSPropertyColor);
    if (!value || !value->isPrimitiveValue())
        return false;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value.get());
    if (!primitiveValue->isRGBColor())
        return false;

    color = primitiveValue->getRGBA32Value();
    return true;
}

StyleColor CSSParser::colorFromRGBColorString(const String& string)
{
    // FIXME: Rework css parser so it is more SVG aware.
    RGBA32 color;
    if (parseColor(color, string.stripWhiteSpace()))
        return StyleColor(color);
    // FIXME: This branch catches the string currentColor, but we should error if we have an illegal color value.
    return StyleColor::currentColor();
}

bool CSSParser::parseSystemColor(RGBA32& color, const String& colorString)
{
    CSSParserString cssColor;
    cssColor.init(colorString);
    CSSValueID id = cssValueKeywordID(cssColor);
    if (!CSSPropertyParser::isSystemColor(id))
        return false;

    Color parsedColor = LayoutTheme::theme().systemColor(id);
    color = parsedColor.rgb();
    return true;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSParser::parseFontFaceDescriptor(CSSPropertyID propertyID, const String& propertyValue, const CSSParserContext& context)
{
    StringBuilder builder;
    builder.appendLiteral("@font-face { ");
    builder.append(getPropertyNameString(propertyID));
    builder.appendLiteral(" : ");
    builder.append(propertyValue);
    builder.appendLiteral("; }");
    RefPtrWillBeRawPtr<StyleRuleBase> rule = parseRule(context, 0, builder.toString());
    if (!rule || !rule->isFontFaceRule())
        return nullptr;
    return toStyleRuleFontFace(rule.get())->properties().getPropertyCSSValue(propertyID);
}

} // namespace blink
