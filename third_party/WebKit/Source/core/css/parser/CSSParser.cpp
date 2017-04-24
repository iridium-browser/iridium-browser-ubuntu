// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSParser.h"

#include "core/css/CSSColorValue.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/StyleColor.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParserFastPaths.h"
#include "core/css/parser/CSSParserImpl.h"
#include "core/css/parser/CSSPropertyParser.h"
#include "core/css/parser/CSSSelectorParser.h"
#include "core/css/parser/CSSSupportsParser.h"
#include "core/css/parser/CSSTokenizer.h"
#include "core/css/parser/CSSVariableParser.h"
#include "core/layout/LayoutTheme.h"
#include <memory>

namespace blink {

bool CSSParser::parseDeclarationList(const CSSParserContext* context,
                                     MutableStylePropertySet* propertySet,
                                     const String& declaration) {
  return CSSParserImpl::parseDeclarationList(propertySet, declaration, context);
}

void CSSParser::parseDeclarationListForInspector(
    const CSSParserContext* context,
    const String& declaration,
    CSSParserObserver& observer) {
  CSSParserImpl::parseDeclarationListForInspector(declaration, context,
                                                  observer);
}

CSSSelectorList CSSParser::parseSelector(const CSSParserContext* context,
                                         StyleSheetContents* styleSheetContents,
                                         const String& selector) {
  CSSTokenizer tokenizer(selector);
  return CSSSelectorParser::parseSelector(tokenizer.tokenRange(), context,
                                          styleSheetContents);
}

CSSSelectorList CSSParser::parsePageSelector(
    const CSSParserContext* context,
    StyleSheetContents* styleSheetContents,
    const String& selector) {
  CSSTokenizer tokenizer(selector);
  return CSSParserImpl::parsePageSelector(tokenizer.tokenRange(),
                                          styleSheetContents);
}

StyleRuleBase* CSSParser::parseRule(const CSSParserContext* context,
                                    StyleSheetContents* styleSheet,
                                    const String& rule) {
  return CSSParserImpl::parseRule(rule, context, styleSheet,
                                  CSSParserImpl::AllowImportRules);
}

void CSSParser::parseSheet(const CSSParserContext* context,
                           StyleSheetContents* styleSheet,
                           const String& text,
                           bool deferPropertyParsing) {
  return CSSParserImpl::parseStyleSheet(text, context, styleSheet,
                                        deferPropertyParsing);
}

void CSSParser::parseSheetForInspector(const CSSParserContext* context,
                                       StyleSheetContents* styleSheet,
                                       const String& text,
                                       CSSParserObserver& observer) {
  return CSSParserImpl::parseStyleSheetForInspector(text, context, styleSheet,
                                                    observer);
}

MutableStylePropertySet::SetResult CSSParser::parseValue(
    MutableStylePropertySet* declaration,
    CSSPropertyID unresolvedProperty,
    const String& string,
    bool important) {
  return parseValue(declaration, unresolvedProperty, string, important,
                    static_cast<StyleSheetContents*>(nullptr));
}

MutableStylePropertySet::SetResult CSSParser::parseValue(
    MutableStylePropertySet* declaration,
    CSSPropertyID unresolvedProperty,
    const String& string,
    bool important,
    StyleSheetContents* styleSheet) {
  if (string.isEmpty()) {
    bool didParse = false;
    bool didChange = false;
    return MutableStylePropertySet::SetResult{didParse, didChange};
  }

  CSSPropertyID resolvedProperty = resolveCSSPropertyID(unresolvedProperty);
  CSSParserMode parserMode = declaration->cssParserMode();
  CSSValue* value =
      CSSParserFastPaths::maybeParseValue(resolvedProperty, string, parserMode);
  if (value) {
    bool didParse = true;
    bool didChange = declaration->setProperty(
        CSSProperty(resolvedProperty, *value, important));
    return MutableStylePropertySet::SetResult{didParse, didChange};
  }
  CSSParserContext* context;
  if (styleSheet) {
    context = CSSParserContext::create(styleSheet->parserContext(), nullptr);
    context->setMode(parserMode);
  } else {
    context = CSSParserContext::create(parserMode);
  }
  return parseValue(declaration, unresolvedProperty, string, important,
                    context);
}

MutableStylePropertySet::SetResult CSSParser::parseValueForCustomProperty(
    MutableStylePropertySet* declaration,
    const AtomicString& propertyName,
    const PropertyRegistry* registry,
    const String& value,
    bool important,
    StyleSheetContents* styleSheet,
    bool isAnimationTainted) {
  DCHECK(CSSVariableParser::isValidVariableName(propertyName));
  if (value.isEmpty()) {
    bool didParse = false;
    bool didChange = false;
    return MutableStylePropertySet::SetResult{didParse, didChange};
  }
  CSSParserMode parserMode = declaration->cssParserMode();
  CSSParserContext* context;
  if (styleSheet) {
    context = CSSParserContext::create(styleSheet->parserContext(), nullptr);
    context->setMode(parserMode);
  } else {
    context = CSSParserContext::create(parserMode);
  }
  return CSSParserImpl::parseVariableValue(declaration, propertyName, registry,
                                           value, important, context,
                                           isAnimationTainted);
}

ImmutableStylePropertySet* CSSParser::parseCustomPropertySet(
    CSSParserTokenRange range) {
  return CSSParserImpl::parseCustomPropertySet(range);
}

MutableStylePropertySet::SetResult CSSParser::parseValue(
    MutableStylePropertySet* declaration,
    CSSPropertyID unresolvedProperty,
    const String& string,
    bool important,
    const CSSParserContext* context) {
  return CSSParserImpl::parseValue(declaration, unresolvedProperty, string,
                                   important, context);
}

const CSSValue* CSSParser::parseSingleValue(CSSPropertyID propertyID,
                                            const String& string,
                                            const CSSParserContext* context) {
  if (string.isEmpty())
    return nullptr;
  if (CSSValue* value = CSSParserFastPaths::maybeParseValue(propertyID, string,
                                                            context->mode()))
    return value;
  CSSTokenizer tokenizer(string);
  return CSSPropertyParser::parseSingleValue(propertyID, tokenizer.tokenRange(),
                                             context);
}

ImmutableStylePropertySet* CSSParser::parseInlineStyleDeclaration(
    const String& styleString,
    Element* element) {
  return CSSParserImpl::parseInlineStyleDeclaration(styleString, element);
}

std::unique_ptr<Vector<double>> CSSParser::parseKeyframeKeyList(
    const String& keyList) {
  return CSSParserImpl::parseKeyframeKeyList(keyList);
}

StyleRuleKeyframe* CSSParser::parseKeyframeRule(const CSSParserContext* context,
                                                const String& rule) {
  StyleRuleBase* keyframe = CSSParserImpl::parseRule(
      rule, context, nullptr, CSSParserImpl::KeyframeRules);
  return toStyleRuleKeyframe(keyframe);
}

bool CSSParser::parseSupportsCondition(const String& condition) {
  CSSTokenizer tokenizer(condition);
  CSSParserImpl parser(strictCSSParserContext());
  return CSSSupportsParser::supportsCondition(tokenizer.tokenRange(), parser) ==
         CSSSupportsParser::Supported;
}

bool CSSParser::parseColor(Color& color, const String& string, bool strict) {
  if (string.isEmpty())
    return false;

  // The regular color parsers don't resolve named colors, so explicitly
  // handle these first.
  Color namedColor;
  if (namedColor.setNamedColor(string)) {
    color = namedColor;
    return true;
  }

  const CSSValue* value = CSSParserFastPaths::parseColor(
      string, strict ? HTMLStandardMode : HTMLQuirksMode);
  // TODO(timloh): Why is this always strict mode?
  if (!value)
    value =
        parseSingleValue(CSSPropertyColor, string, strictCSSParserContext());

  if (!value || !value->isColorValue())
    return false;
  color = toCSSColorValue(*value).value();
  return true;
}

bool CSSParser::parseSystemColor(Color& color, const String& colorString) {
  CSSValueID id = cssValueKeywordID(colorString);
  if (!StyleColor::isSystemColor(id))
    return false;

  color = LayoutTheme::theme().systemColor(id);
  return true;
}

const CSSValue* CSSParser::parseFontFaceDescriptor(
    CSSPropertyID propertyID,
    const String& propertyValue,
    const CSSParserContext* context) {
  StringBuilder builder;
  builder.append("@font-face { ");
  builder.append(getPropertyNameString(propertyID));
  builder.append(" : ");
  builder.append(propertyValue);
  builder.append("; }");
  StyleRuleBase* rule = parseRule(context, nullptr, builder.toString());
  if (!rule || !rule->isFontFaceRule())
    return nullptr;
  return toStyleRuleFontFace(rule)->properties().getPropertyCSSValue(
      propertyID);
}

}  // namespace blink
