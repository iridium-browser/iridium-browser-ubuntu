// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParserImpl_h
#define CSSParserImpl_h

#include "core/CSSPropertyNames.h"
#include "core/css/CSSProperty.h"
#include "core/css/CSSPropertySourceData.h"
#include "core/css/StylePropertySet.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class CSSLazyParsingState;
class CSSParserContext;
class CSSParserObserver;
class CSSParserObserverWrapper;
class StyleRule;
class StyleRuleBase;
class StyleRuleCharset;
class StyleRuleFontFace;
class StyleRuleImport;
class StyleRuleKeyframe;
class StyleRuleKeyframes;
class StyleRuleMedia;
class StyleRuleNamespace;
class StyleRulePage;
class StyleRuleSupports;
class StyleRuleViewport;
class StyleSheetContents;
class Element;

class CSSParserImpl {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(CSSParserImpl);

 public:
  CSSParserImpl(const CSSParserContext*, StyleSheetContents* = nullptr);

  enum AllowedRulesType {
    // As per css-syntax, css-cascade and css-namespaces, @charset rules
    // must come first, followed by @import then @namespace.
    // AllowImportRules actually means we allow @import and any rules thay
    // may follow it, i.e. @namespace rules and regular rules.
    // AllowCharsetRules and AllowNamespaceRules behave similarly.
    AllowCharsetRules,
    AllowImportRules,
    AllowNamespaceRules,
    RegularRules,
    KeyframeRules,
    ApplyRules,  // For @apply inside style rules
    NoRules,     // For parsing at-rules inside declaration lists
  };

  static MutableStylePropertySet::SetResult parseValue(MutableStylePropertySet*,
                                                       CSSPropertyID,
                                                       const String&,
                                                       bool important,
                                                       const CSSParserContext*);
  static MutableStylePropertySet::SetResult parseVariableValue(
      MutableStylePropertySet*,
      const AtomicString& propertyName,
      const PropertyRegistry*,
      const String&,
      bool important,
      const CSSParserContext*,
      bool isAnimationTainted);
  static ImmutableStylePropertySet* parseInlineStyleDeclaration(const String&,
                                                                Element*);
  static bool parseDeclarationList(MutableStylePropertySet*,
                                   const String&,
                                   const CSSParserContext*);
  static StyleRuleBase* parseRule(const String&,
                                  const CSSParserContext*,
                                  StyleSheetContents*,
                                  AllowedRulesType);
  static void parseStyleSheet(const String&,
                              const CSSParserContext*,
                              StyleSheetContents*,
                              bool deferPropertyParsing = false);
  static CSSSelectorList parsePageSelector(CSSParserTokenRange,
                                           StyleSheetContents*);

  static ImmutableStylePropertySet* parseCustomPropertySet(CSSParserTokenRange);

  static std::unique_ptr<Vector<double>> parseKeyframeKeyList(const String&);

  bool supportsDeclaration(CSSParserTokenRange&);

  static void parseDeclarationListForInspector(const String&,
                                               const CSSParserContext*,
                                               CSSParserObserver&);
  static void parseStyleSheetForInspector(const String&,
                                          const CSSParserContext*,
                                          StyleSheetContents*,
                                          CSSParserObserver&);

  static StylePropertySet* parseDeclarationListForLazyStyle(
      CSSParserTokenRange block,
      const CSSParserContext*);

 private:
  enum RuleListType { TopLevelRuleList, RegularRuleList, KeyframesRuleList };

  // Returns whether the first encountered rule was valid
  template <typename T>
  bool consumeRuleList(CSSParserTokenRange, RuleListType, T callback);

  // These two functions update the range they're given
  StyleRuleBase* consumeAtRule(CSSParserTokenRange&, AllowedRulesType);
  StyleRuleBase* consumeQualifiedRule(CSSParserTokenRange&, AllowedRulesType);

  static StyleRuleCharset* consumeCharsetRule(CSSParserTokenRange prelude);
  StyleRuleImport* consumeImportRule(CSSParserTokenRange prelude);
  StyleRuleNamespace* consumeNamespaceRule(CSSParserTokenRange prelude);
  StyleRuleMedia* consumeMediaRule(CSSParserTokenRange prelude,
                                   CSSParserTokenRange block);
  StyleRuleSupports* consumeSupportsRule(CSSParserTokenRange prelude,
                                         CSSParserTokenRange block);
  StyleRuleViewport* consumeViewportRule(CSSParserTokenRange prelude,
                                         CSSParserTokenRange block);
  StyleRuleFontFace* consumeFontFaceRule(CSSParserTokenRange prelude,
                                         CSSParserTokenRange block);
  StyleRuleKeyframes* consumeKeyframesRule(bool webkitPrefixed,
                                           CSSParserTokenRange prelude,
                                           CSSParserTokenRange block);
  StyleRulePage* consumePageRule(CSSParserTokenRange prelude,
                                 CSSParserTokenRange block);
  // Updates m_parsedProperties
  void consumeApplyRule(CSSParserTokenRange prelude);

  StyleRuleKeyframe* consumeKeyframeStyleRule(CSSParserTokenRange prelude,
                                              CSSParserTokenRange block);
  StyleRule* consumeStyleRule(CSSParserTokenRange prelude,
                              CSSParserTokenRange block);

  void consumeDeclarationList(CSSParserTokenRange, StyleRule::RuleType);
  void consumeDeclaration(CSSParserTokenRange, StyleRule::RuleType);
  void consumeDeclarationValue(CSSParserTokenRange,
                               CSSPropertyID,
                               bool important,
                               StyleRule::RuleType);
  void consumeVariableValue(CSSParserTokenRange,
                            const AtomicString& propertyName,
                            bool important,
                            bool isAnimationTainted);

  static std::unique_ptr<Vector<double>> consumeKeyframeKeyList(
      CSSParserTokenRange);

  // FIXME: Can we build StylePropertySets directly?
  // FIXME: Investigate using a smaller inline buffer
  HeapVector<CSSProperty, 256> m_parsedProperties;

  Member<const CSSParserContext> m_context;
  Member<StyleSheetContents> m_styleSheet;

  // For the inspector
  CSSParserObserverWrapper* m_observerWrapper;

  Member<CSSLazyParsingState> m_lazyState;
};

}  // namespace blink

#endif  // CSSParserImpl_h
