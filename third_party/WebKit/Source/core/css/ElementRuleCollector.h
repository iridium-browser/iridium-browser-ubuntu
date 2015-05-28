/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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
 *
 */

#ifndef ElementRuleCollector_h
#define ElementRuleCollector_h

#include "core/css/PseudoStyleRequest.h"
#include "core/css/SelectorChecker.h"
#include "core/css/resolver/ElementResolveContext.h"
#include "core/css/resolver/MatchRequest.h"
#include "core/css/resolver/MatchResult.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace blink {

class CSSStyleSheet;
class CSSRuleList;
class RuleData;
class RuleSet;
class SelectorFilter;
class StaticCSSRuleList;

typedef unsigned CascadeOrder;

const CascadeOrder ignoreCascadeOrder = 0;

class MatchedRule {
    ALLOW_ONLY_INLINE_ALLOCATION();
public:
    MatchedRule(const RuleData* ruleData, unsigned specificity, CascadeOrder cascadeOrder, unsigned styleSheetIndex, const CSSStyleSheet* parentStyleSheet)
        : m_ruleData(ruleData)
        , m_specificity(specificity)
        , m_parentStyleSheet(parentStyleSheet)
    {
        ASSERT(m_ruleData);
        static const unsigned BitsForPositionInRuleData = 18;
        static const unsigned BitsForStyleSheetIndex = 32;
        m_position = ((uint64_t)cascadeOrder << (BitsForStyleSheetIndex + BitsForPositionInRuleData)) + ((uint64_t)styleSheetIndex << BitsForPositionInRuleData)+ m_ruleData->position();
    }

    const RuleData* ruleData() const { return m_ruleData; }
    uint64_t position() const { return m_position; }
    unsigned specificity() const { return ruleData()->specificity() + m_specificity; }
    const CSSStyleSheet* parentStyleSheet() const { return m_parentStyleSheet; }
    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_parentStyleSheet);
    }

private:
    // FIXME: Oilpan: RuleData is in the oilpan heap and this pointer
    // really should be traced. However, RuleData objects are
    // allocated inside larger TerminatedArray objects and we cannot
    // trace a raw rule data pointer at this point.
    const RuleData* m_ruleData;
    unsigned m_specificity;
    uint64_t m_position;
    RawPtrWillBeMember<const CSSStyleSheet> m_parentStyleSheet;
};

} // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::MatchedRule);

namespace blink {

// FIXME: oilpan: when transition types are gone this class can be replaced with HeapVector.
class StyleRuleList final : public RefCountedWillBeGarbageCollected<StyleRuleList> {
public:
    static PassRefPtrWillBeRawPtr<StyleRuleList> create() { return adoptRefWillBeNoop(new StyleRuleList()); }

    DEFINE_INLINE_TRACE()
    {
#if ENABLE(OILPAN)
        visitor->trace(m_list);
#endif
    }

    WillBeHeapVector<RawPtrWillBeMember<StyleRule>> m_list;
};

// ElementRuleCollector is designed to be used as a stack object.
// Create one, ask what rules the ElementResolveContext matches
// and then let it go out of scope.
// FIXME: Currently it modifies the ComputedStyle but should not!
class ElementRuleCollector {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(ElementRuleCollector);
public:
    ElementRuleCollector(const ElementResolveContext&, const SelectorFilter&, ComputedStyle* = 0);
    ~ElementRuleCollector();

    void setMode(SelectorChecker::Mode mode) { m_mode = mode; }
    void setPseudoStyleRequest(const PseudoStyleRequest& request) { m_pseudoStyleRequest = request; }
    void setSameOriginOnly(bool f) { m_sameOriginOnly = f; }

    void setMatchingUARules(bool matchingUARules) { m_matchingUARules = matchingUARules; }
    void setScopeContainsLastMatchedElement(bool scopeContainsLastMatchedElement) { m_scopeContainsLastMatchedElement = scopeContainsLastMatchedElement; }
    bool scopeContainsLastMatchedElement() const { return m_scopeContainsLastMatchedElement; }
    bool hasAnyMatchingRules(RuleSet*);

    MatchResult& matchedResult();
    PassRefPtrWillBeRawPtr<StyleRuleList> matchedStyleRuleList();
    PassRefPtrWillBeRawPtr<CSSRuleList> matchedCSSRuleList();

    void collectMatchingRules(const MatchRequest&, RuleRange&, CascadeOrder = ignoreCascadeOrder, bool matchingTreeBoundaryRules = false);
    void collectMatchingShadowHostRules(const MatchRequest&, RuleRange&, CascadeOrder = ignoreCascadeOrder, bool matchingTreeBoundaryRules = false);
    void sortAndTransferMatchedRules();
    void clearMatchedRules();
    void addElementStyleProperties(const StylePropertySet*, bool isCacheable = true);

private:
    void collectRuleIfMatches(const RuleData&, CascadeOrder, const MatchRequest&, RuleRange&);

    template<typename RuleDataListType>
    void collectMatchingRulesForList(const RuleDataListType* rules, CascadeOrder cascadeOrder, const MatchRequest& matchRequest, RuleRange& ruleRange)
    {
        if (!rules)
            return;

        for (const auto& rule : *rules)
            collectRuleIfMatches(rule, cascadeOrder, matchRequest, ruleRange);
    }

    bool ruleMatches(const RuleData&, const ContainerNode* scope, SelectorChecker::MatchResult*);

    CSSRuleList* nestedRuleList(CSSRule*);
    template<class CSSRuleCollection>
    CSSRule* findStyleRule(CSSRuleCollection*, StyleRule*);
    void appendCSSOMWrapperForRule(CSSStyleSheet*, StyleRule*);

    void sortMatchedRules();
    void addMatchedRule(const RuleData*, unsigned specificity, CascadeOrder, unsigned styleSheetIndex, const CSSStyleSheet* parentStyleSheet);

    StaticCSSRuleList* ensureRuleList();
    StyleRuleList* ensureStyleRuleList();

private:
    const ElementResolveContext& m_context;
    const SelectorFilter& m_selectorFilter;
    RefPtr<ComputedStyle> m_style; // FIXME: This can be mutated during matching!

    PseudoStyleRequest m_pseudoStyleRequest;
    SelectorChecker::Mode m_mode;
    bool m_canUseFastReject;
    bool m_sameOriginOnly;
    bool m_matchingUARules;
    bool m_scopeContainsLastMatchedElement;

    WillBeHeapVector<MatchedRule, 32> m_matchedRules;

    // Output.
    RefPtrWillBeMember<StaticCSSRuleList> m_cssRuleList;
    RefPtrWillBeMember<StyleRuleList> m_styleRuleList;
    MatchResult m_result;
};

} // namespace blink

#endif // ElementRuleCollector_h
