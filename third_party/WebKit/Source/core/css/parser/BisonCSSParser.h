/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 - 2010  Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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

#ifndef BisonCSSParser_h
#define BisonCSSParser_h

#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/CoreExport.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSGradientValue.h"
#include "core/css/CSSProperty.h"
#include "core/css/CSSPropertySourceData.h"
#include "core/css/CSSSelector.h"
#include "core/css/MediaQuery.h"
#include "core/css/StylePropertySet.h"
#include "core/css/parser/BisonCSSTokenizer.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/css/parser/CSSParserObserver.h"
#include "core/css/parser/CSSParserValues.h"
#include "core/css/parser/CSSPropertyParser.h"
#include "platform/graphics/Color.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/TextPosition.h"

namespace blink {

class CSSSelectorList;
class CSSValue;
class Element;
class ImmutableStylePropertySet;
class MediaQueryExp;
class MediaQuerySet;
class MutableStylePropertySet;
class StyleColor;
class StyleRuleBase;
class StyleRuleKeyframe;
class StyleRuleKeyframes;
class StyleSheetContents;

// FIXME: This class is shared with BisonCSSTokenizer so should we rename it to CSSSourceLocation?
struct CSSParserLocation {
    unsigned offset;
    unsigned lineNumber;
    CSSParserString token;
};

class CORE_EXPORT BisonCSSParser {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(BisonCSSParser);
    friend inline int cssyylex(void*, BisonCSSParser*);
public:
    explicit BisonCSSParser(const CSSParserContext&);
    ~BisonCSSParser();

    void rollbackLastProperties(int num);
    void setCurrentProperty(CSSPropertyID);

    void parseSheet(StyleSheetContents*, const String&, const TextPosition& startPosition = TextPosition::minimumPosition(), CSSParserObserver* = 0, bool = false);
    PassRefPtrWillBeRawPtr<StyleRuleBase> parseRule(StyleSheetContents*, const String&);
    PassRefPtrWillBeRawPtr<StyleRuleKeyframe> parseKeyframeRule(StyleSheetContents*, const String&);
    bool parseSupportsCondition(const String&);
    static bool parseValue(MutableStylePropertySet*, CSSPropertyID, const String&, bool important, const CSSParserContext&);
    bool parseDeclaration(MutableStylePropertySet*, const String&, CSSParserObserver*, StyleSheetContents* contextStyleSheet);
    static PassRefPtrWillBeRawPtr<ImmutableStylePropertySet> parseInlineStyleDeclaration(const String&, Element*);
    PassOwnPtr<Vector<double>> parseKeyframeKeyList(const String&);
    bool parseAttributeMatchType(CSSSelector::AttributeMatchType&, const String&);

    bool parseValue(CSSPropertyID, bool important);
    void parseSelector(const String&, CSSSelectorList&);

    CSSParserSelector* createFloatingSelector();
    CSSParserSelector* createFloatingSelectorWithTagName(const QualifiedName&);
    PassOwnPtr<CSSParserSelector> sinkFloatingSelector(CSSParserSelector*);

    Vector<OwnPtr<CSSParserSelector>>* createFloatingSelectorVector();
    PassOwnPtr<Vector<OwnPtr<CSSParserSelector>>> sinkFloatingSelectorVector(Vector<OwnPtr<CSSParserSelector>>*);

    CSSParserValueList* createFloatingValueList();
    PassOwnPtr<CSSParserValueList> sinkFloatingValueList(CSSParserValueList*);

    CSSParserFunction* createFloatingFunction(const CSSParserString& name, PassOwnPtr<CSSParserValueList> args);

    CSSParserValue& sinkFloatingValue(CSSParserValue&);

    MediaQuerySet* createMediaQuerySet();
    StyleRuleBase* createImportRule(const CSSParserString&, MediaQuerySet*);
    StyleRuleKeyframe* createKeyframe(CSSParserValueList*);
    StyleRuleKeyframes* createKeyframesRule(const String&, PassOwnPtrWillBeRawPtr<WillBeHeapVector<RefPtrWillBeMember<StyleRuleKeyframe>>>, bool isPrefixed);

    typedef WillBeHeapVector<RefPtrWillBeMember<StyleRuleBase>> RuleList;
    StyleRuleBase* createMediaRule(MediaQuerySet*, RuleList*);
    RuleList* createRuleList();
    RuleList* appendRule(RuleList*, StyleRuleBase*);
    StyleRuleBase* createStyleRule(Vector<OwnPtr<CSSParserSelector>>* selectors);
    StyleRuleBase* createFontFaceRule();
    StyleRuleBase* createPageRule(PassOwnPtr<CSSParserSelector> pageSelector);
    StyleRuleBase* createMarginAtRule(CSSSelector::MarginBoxType);
    StyleRuleBase* createSupportsRule(bool conditionIsSupported, RuleList*);
    void markSupportsRuleHeaderStart();
    void markSupportsRuleHeaderEnd();
    PassRefPtrWillBeRawPtr<CSSRuleSourceData> popSupportsRuleData();
    StyleRuleBase* createHostRule(RuleList* rules);

    void startDeclarationsForMarginBox();
    void endDeclarationsForMarginBox();

    void startMediaValue();
    void endMediaValue();
    void startMediaQuery();
    MediaQueryExp* createFloatingMediaQueryExp(const AtomicString&, CSSParserValueList*);
    PassOwnPtrWillBeRawPtr<MediaQueryExp> sinkFloatingMediaQueryExp(MediaQueryExp*);
    WillBeHeapVector<OwnPtrWillBeMember<MediaQueryExp>>* createFloatingMediaQueryExpList();
    PassOwnPtrWillBeRawPtr<WillBeHeapVector<OwnPtrWillBeMember<MediaQueryExp>>> sinkFloatingMediaQueryExpList(WillBeHeapVector<OwnPtrWillBeMember<MediaQueryExp>>*);
    MediaQuery* createFloatingMediaQuery(MediaQuery::Restrictor, const AtomicString&, PassOwnPtrWillBeRawPtr<WillBeHeapVector<OwnPtrWillBeMember<MediaQueryExp>>>);
    MediaQuery* createFloatingMediaQuery(PassOwnPtrWillBeRawPtr<WillBeHeapVector<OwnPtrWillBeMember<MediaQueryExp>>>);
    MediaQuery* createFloatingNotAllQuery();
    PassOwnPtrWillBeRawPtr<MediaQuery> sinkFloatingMediaQuery(MediaQuery*);

    WillBeHeapVector<RefPtrWillBeMember<StyleRuleKeyframe>>* createFloatingKeyframeVector();
    PassOwnPtrWillBeRawPtr<WillBeHeapVector<RefPtrWillBeMember<StyleRuleKeyframe>>> sinkFloatingKeyframeVector(WillBeHeapVector<RefPtrWillBeMember<StyleRuleKeyframe>>*);

    void addNamespace(const AtomicString& prefix, const AtomicString& uri);
    QualifiedName determineNameInNamespace(const AtomicString& prefix, const AtomicString& localName);

    CSSParserSelector* rewriteSpecifiersWithElementName(const AtomicString& namespacePrefix, const AtomicString& elementName, CSSParserSelector*, bool isNamespacePlaceholder = false);
    CSSParserSelector* rewriteSpecifiersWithElementNameForCustomPseudoElement(const QualifiedName& tag, const AtomicString& elementName, CSSParserSelector* specifiers, bool tagIsForNamespaceRule);
    CSSParserSelector* rewriteSpecifiersWithElementNameForContentPseudoElement(const QualifiedName& tag, const AtomicString& elementName, CSSParserSelector* specifiers, bool tagIsForNamespaceRule);
    CSSParserSelector* rewriteSpecifiersWithNamespaceIfNeeded(CSSParserSelector*);
    CSSParserSelector* rewriteSpecifiers(CSSParserSelector*, CSSParserSelector*);
    CSSParserSelector* rewriteSpecifiersForShadowDistributed(CSSParserSelector* specifiers, CSSParserSelector* distributedPseudoElementSelector);

    void invalidBlockHit();

    Vector<OwnPtr<CSSParserSelector>>* reusableSelectorVector() { return &m_reusableSelectorVector; }

    void clearProperties();

    PassRefPtrWillBeRawPtr<ImmutableStylePropertySet> createStylePropertySet();

    CSSParserContext m_context;

    bool m_important;
    CSSPropertyID m_id;
    RawPtrWillBeMember<StyleSheetContents> m_styleSheet;
    RefPtrWillBeMember<StyleRuleBase> m_rule;
    RefPtrWillBeMember<StyleRuleKeyframe> m_keyframe;
    OwnPtr<CSSParserValueList> m_valueList;
    bool m_supportsCondition;

    WillBeHeapVector<CSSProperty, 256> m_parsedProperties;
    CSSSelectorList* m_selectorListForParseSelector;

    unsigned m_numParsedPropertiesBeforeMarginBox;

    bool m_hadSyntacticallyValidCSSRule;
    bool m_logErrors;
    bool m_ignoreErrors;

    AtomicString m_defaultNamespace;

    // tokenizer methods and data
    CSSParserObserver* m_observer;

    // Local functions which just call into CSSParserObserver if non-null.
    void startRule();
    void endRule(bool valid);
    void startRuleHeader(StyleRule::Type);
    void endRuleHeader();
    void startSelector();
    void endSelector();
    void startRuleBody();
    void startProperty();
    void endProperty(bool isImportantFound, bool isPropertyParsed, CSSParserError = NoCSSError);

    void endInvalidRuleHeader();
    void reportError(const CSSParserLocation&, CSSParserError = GeneralCSSError);
    void resumeErrorLogging() { m_ignoreErrors = false; }
    void setLocationLabel(const CSSParserLocation& location) { m_locationLabel = location; }
    const CSSParserLocation& lastLocationLabel() const { return m_locationLabel; }

    void tokenToLowerCase(CSSParserString& token);

    void markViewportRuleBodyStart() { m_inViewport = true; }
    void markViewportRuleBodyEnd() { m_inViewport = false; }
    StyleRuleBase* createViewportRule();

    CSSParserLocation currentLocation() { return m_tokenizer.currentLocation(); }

private:
    class StyleDeclarationScope {
        STACK_ALLOCATED();
        WTF_MAKE_NONCOPYABLE(StyleDeclarationScope);
    public:
        StyleDeclarationScope(BisonCSSParser* parser, const StylePropertySet* declaration)
            : m_parser(parser)
            , m_mode(declaration->cssParserMode())
        {
            if (isCSSViewportParsingEnabledForMode(m_mode)) {
                ASSERT(!m_parser->inViewport());
                m_parser->markViewportRuleBodyStart();
            }
        }

        ~StyleDeclarationScope()
        {
            if (isCSSViewportParsingEnabledForMode(m_mode))
                m_parser->markViewportRuleBodyEnd();
        }

    private:
        BisonCSSParser* m_parser;
        CSSParserMode m_mode;
    };

    inline void ensureLineEndings();

    void setStyleSheet(StyleSheetContents* styleSheet) { m_styleSheet = styleSheet; }

    bool inViewport() const { return m_inViewport; }

    void recheckAtKeyword(const UChar* str, int len);

    template<unsigned prefixLength, unsigned suffixLength>
    inline void setupParser(const char (&prefix)[prefixLength], const String& string, const char (&suffix)[suffixLength])
    {
        setupParser(prefix, prefixLength - 1, string, suffix, suffixLength - 1);
    }
    void setupParser(const char* prefix, unsigned prefixLength, const String&, const char* suffix, unsigned suffixLength);

    bool parseValue(MutableStylePropertySet*, CSSPropertyID, const String&, bool important);
    PassRefPtrWillBeRawPtr<ImmutableStylePropertySet> parseDeclaration(const String&, StyleSheetContents* contextStyleSheet);

    const String* m_source;
    TextPosition m_startPosition;
    StyleRule::Type m_ruleHeaderType;
    unsigned m_ruleHeaderStartOffset;
    int m_ruleHeaderStartLineNumber;
    OwnPtr<Vector<unsigned>> m_lineEndings;

    bool m_ruleHasHeader;

    bool m_allowImportRules;
    bool m_allowNamespaceDeclarations;

    bool m_inViewport;

    CSSParserLocation m_locationLabel;

    WillBeHeapVector<RefPtrWillBeMember<StyleRuleBase>> m_parsedRules;
    WillBeHeapVector<RefPtrWillBeMember<StyleRuleKeyframe>> m_parsedKeyframes;
    WillBeHeapVector<RefPtrWillBeMember<MediaQuerySet>> m_parsedMediaQuerySets;
    WillBeHeapVector<OwnPtrWillBeMember<RuleList>> m_parsedRuleLists;
    Vector<CSSParserSelector*> m_floatingSelectors;
    Vector<Vector<OwnPtr<CSSParserSelector>>*> m_floatingSelectorVectors;
    Vector<CSSParserValueList*> m_floatingValueLists;
    Vector<CSSParserFunction*> m_floatingFunctions;

    unsigned m_mediaQueryValueStartOffset;
    unsigned m_mediaQueryValueEndOffset;
    OwnPtrWillBeMember<MediaQuery> m_floatingMediaQuery;
    OwnPtrWillBeMember<MediaQueryExp> m_floatingMediaQueryExp;
    OwnPtrWillBeMember<WillBeHeapVector<OwnPtrWillBeMember<MediaQueryExp>>> m_floatingMediaQueryExpList;

    OwnPtrWillBeMember<WillBeHeapVector<RefPtrWillBeMember<StyleRuleKeyframe>>> m_floatingKeyframeVector;

    Vector<OwnPtr<CSSParserSelector>> m_reusableSelectorVector;

    OwnPtrWillBeMember<RuleSourceDataList> m_supportsRuleDataStack;

    BisonCSSTokenizer m_tokenizer;
};

inline int cssyylex(void* yylval, BisonCSSParser* parser)
{
    return parser->m_tokenizer.lex(yylval);
}

bool isValidNthToken(const CSSParserString&);

} // namespace blink

#endif // BisonCSSParser_h
