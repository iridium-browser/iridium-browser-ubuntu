/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#ifndef SelectorChecker_h
#define SelectorChecker_h

#include "core/css/CSSSelector.h"
#include "core/dom/Element.h"
#include "platform/scroll/ScrollTypes.h"

namespace blink {

class CSSSelector;
class ContainerNode;
class Element;
class LayoutScrollbar;
class ComputedStyle;

class SelectorChecker {
    WTF_MAKE_NONCOPYABLE(SelectorChecker);
public:
    enum Match { SelectorMatches, SelectorFailsLocally, SelectorFailsAllSiblings, SelectorFailsCompletely };
    enum VisitedMatchType { VisitedMatchDisabled, VisitedMatchEnabled };
    enum Mode { ResolvingStyle = 0, CollectingStyleRules, CollectingCSSRules, QueryingRules, SharingRules };
    SelectorChecker(Document&, Mode);

    struct SelectorCheckingContext {
        STACK_ALLOCATED();
    public:
        // Initial selector constructor
        SelectorCheckingContext(const CSSSelector& selector, Element* element, VisitedMatchType visitedMatchType)
            : selector(&selector)
            , element(element)
            , previousElement(nullptr)
            , scope(nullptr)
            , visitedMatchType(visitedMatchType)
            , pseudoId(NOPSEUDO)
            , elementStyle(0)
            , scrollbar(nullptr)
            , scrollbarPart(NoPart)
            , isSubSelector(false)
            , hasScrollbarPseudo(false)
            , hasSelectionPseudo(false)
            , isUARule(false)
            , scopeContainsLastMatchedElement(false)
            , treatShadowHostAsNormalScope(false)
        {
        }

        const CSSSelector* selector;
        RawPtrWillBeMember<Element> element;
        RawPtrWillBeMember<Element> previousElement;
        RawPtrWillBeMember<const ContainerNode> scope;
        VisitedMatchType visitedMatchType;
        PseudoId pseudoId;
        ComputedStyle* elementStyle;
        RawPtrWillBeMember<LayoutScrollbar> scrollbar;
        ScrollbarPart scrollbarPart;
        unsigned isSubSelector : 1;
        unsigned hasScrollbarPseudo : 1;
        unsigned hasSelectionPseudo : 1;
        unsigned isUARule : 1;
        unsigned scopeContainsLastMatchedElement : 1;
        unsigned treatShadowHostAsNormalScope : 1;
    };

    struct MatchResult {
        MatchResult()
            : dynamicPseudo(NOPSEUDO)
            , specificity(0) { }

        PseudoId dynamicPseudo;
        unsigned specificity;
    };

    template<typename SiblingTraversalStrategy>
    Match match(const SelectorCheckingContext&, const SiblingTraversalStrategy&, MatchResult* = 0) const;

    template<typename SiblingTraversalStrategy>
    bool checkOne(const SelectorCheckingContext&, const SiblingTraversalStrategy&, unsigned* specificity = 0) const;

    bool strictParsing() const { return m_strictParsing; }

    Mode mode() const { return m_mode; }

    static bool tagMatches(const Element&, const QualifiedName&);
    static bool isCommonPseudoClassSelector(const CSSSelector&);
    static bool matchesFocusPseudoClass(const Element&);
    static bool matchesSpatialNavigationFocusPseudoClass(const Element&);
    static bool matchesListBoxPseudoClass(const Element&);

    enum LinkMatchMask { MatchLink = 1, MatchVisited = 2, MatchAll = MatchLink | MatchVisited };
    static unsigned determineLinkMatchType(const CSSSelector&);

    static bool isHostInItsShadowTree(const Element&, const ContainerNode* scope);

private:
    template<typename SiblingTraversalStrategy>
    Match matchForSubSelector(const SelectorCheckingContext&, const SiblingTraversalStrategy&, MatchResult*) const;
    template<typename SiblingTraversalStrategy>
    Match matchForRelation(const SelectorCheckingContext&, const SiblingTraversalStrategy&, MatchResult*) const;
    template<typename SiblingTraversalStrategy>
    Match matchForShadowDistributed(const Element*, const SiblingTraversalStrategy&, SelectorCheckingContext& nextContext, MatchResult* = 0) const;
    template<typename SiblingTraversalStrategy>
    Match matchForPseudoShadow(const ContainerNode*, const SelectorCheckingContext&, const SiblingTraversalStrategy&, MatchResult*) const;
    template<typename SiblingTraversalStrategy>
    bool checkPseudoClass(const SelectorCheckingContext&, const SiblingTraversalStrategy&, unsigned* specificity) const;
    template<typename SiblingTraversalStrategy>
    bool checkPseudoElement(const SelectorCheckingContext&, const SiblingTraversalStrategy&) const;

    bool checkScrollbarPseudoClass(const SelectorCheckingContext&, Document*, const CSSSelector&) const;
    template<typename SiblingTraversalStrategy>
    bool checkPseudoHost(const SelectorCheckingContext&, const SiblingTraversalStrategy&, unsigned*) const;

    static bool isFrameFocused(const Element&);
    bool shouldMatchHoverOrActive(const SelectorCheckingContext&) const;

    bool m_strictParsing;
    Mode m_mode;
};

inline bool SelectorChecker::isCommonPseudoClassSelector(const CSSSelector& selector)
{
    if (selector.match() != CSSSelector::PseudoClass)
        return false;
    CSSSelector::PseudoType pseudoType = selector.pseudoType();
    return pseudoType == CSSSelector::PseudoLink
        || pseudoType == CSSSelector::PseudoAnyLink
        || pseudoType == CSSSelector::PseudoVisited
        || pseudoType == CSSSelector::PseudoFocus;
}

inline bool SelectorChecker::tagMatches(const Element& element, const QualifiedName& tagQName)
{
    if (tagQName == anyQName())
        return true;
    const AtomicString& localName = tagQName.localName();
    if (localName != starAtom && localName != element.localName())
        return false;
    const AtomicString& namespaceURI = tagQName.namespaceURI();
    return namespaceURI == starAtom || namespaceURI == element.namespaceURI();
}

inline bool SelectorChecker::isHostInItsShadowTree(const Element& element, const ContainerNode* scope)
{
    return scope && scope->isInShadowTree() && scope->shadowHost() == element;
}

inline bool SelectorChecker::shouldMatchHoverOrActive(const SelectorCheckingContext& context) const
{
    // If we're in quirks mode, then :hover and :active should never match anchors with no
    // href and *:hover and *:active should not match anything. This is specified in
    // https://quirks.spec.whatwg.org/#the-:active-and-:hover-quirk
    return m_strictParsing || context.isSubSelector || (context.selector->relation() == CSSSelector::SubSelector && context.selector->tagHistory()) || context.element->isLink();
}

}

#endif
