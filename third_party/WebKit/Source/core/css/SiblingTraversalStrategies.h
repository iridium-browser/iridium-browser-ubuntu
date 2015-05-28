/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012, Google, Inc. All rights reserved.
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

#ifndef SiblingTraversalStrategies_h
#define SiblingTraversalStrategies_h

#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/style/ComputedStyle.h"

namespace blink {

class DOMSiblingTraversalStrategy {
public:
    bool isFirstChild(Element&) const;
    bool isLastChild(Element&) const;
    bool isFirstOfType(Element&, const QualifiedName&) const;
    bool isLastOfType(Element&, const QualifiedName&) const;

    int countElementsBefore(Element&) const;
    int countElementsAfter(Element&) const;
    int countElementsOfTypeBefore(Element&, const QualifiedName&) const;
    int countElementsOfTypeAfter(Element&, const QualifiedName&) const;

private:
    class HasTagName {
    public:
        explicit HasTagName(const QualifiedName& tagName) : m_tagName(tagName) { }
        bool operator() (const Element& element) const { return element.hasTagName(m_tagName); }
    private:
        const QualifiedName& m_tagName;
    };
};

inline bool DOMSiblingTraversalStrategy::isFirstChild(Element& element) const
{
    return !ElementTraversal::previousSibling(element);
}

inline bool DOMSiblingTraversalStrategy::isLastChild(Element& element) const
{
    return !ElementTraversal::nextSibling(element);
}

inline bool DOMSiblingTraversalStrategy::isFirstOfType(Element& element, const QualifiedName& type) const
{
    return !ElementTraversal::previousSibling(element, HasTagName(type));
}

inline bool DOMSiblingTraversalStrategy::isLastOfType(Element& element, const QualifiedName& type) const
{
    return !ElementTraversal::nextSibling(element, HasTagName(type));
}

inline int DOMSiblingTraversalStrategy::countElementsBefore(Element& element) const
{
    int count = 0;
    for (const Element* sibling = ElementTraversal::previousSibling(element); sibling; sibling = ElementTraversal::previousSibling(*sibling))
        count++;

    return count;
}

inline int DOMSiblingTraversalStrategy::countElementsOfTypeBefore(Element& element, const QualifiedName& type) const
{
    int count = 0;
    for (const Element* sibling = ElementTraversal::previousSibling(element, HasTagName(type)); sibling; sibling = ElementTraversal::previousSibling(*sibling, HasTagName(type)))
        ++count;
    return count;
}

inline int DOMSiblingTraversalStrategy::countElementsAfter(Element& element) const
{
    int count = 0;
    for (const Element* sibling = ElementTraversal::nextSibling(element); sibling; sibling = ElementTraversal::nextSibling(*sibling))
        ++count;
    return count;
}

inline int DOMSiblingTraversalStrategy::countElementsOfTypeAfter(Element& element, const QualifiedName& type) const
{
    int count = 0;
    for (const Element* sibling = ElementTraversal::nextSibling(element, HasTagName(type)); sibling; sibling = ElementTraversal::nextSibling(*sibling, HasTagName(type)))
        ++count;
    return count;
}

class ShadowDOMSiblingTraversalStrategy final {
    STACK_ALLOCATED();
public:
    ShadowDOMSiblingTraversalStrategy(const WillBeHeapVector<RawPtrWillBeMember<Node>, 32>& siblings, int nth)
        : m_siblings(siblings)
        , m_nth(nth)
    {
    }

    bool isFirstChild(Element&) const;
    bool isLastChild(Element&) const;
    bool isFirstOfType(Element&, const QualifiedName&) const;
    bool isLastOfType(Element&, const QualifiedName&) const;

    int countElementsBefore(Element&) const;
    int countElementsAfter(Element&) const;
    int countElementsOfTypeBefore(Element&, const QualifiedName&) const;
    int countElementsOfTypeAfter(Element&, const QualifiedName&) const;

private:
    const WillBeHeapVector<RawPtrWillBeMember<Node>, 32>& m_siblings;
    int m_nth;
};

inline bool ShadowDOMSiblingTraversalStrategy::isFirstChild(Element& element) const
{
    ASSERT(element == toElement(m_siblings[m_nth]));

    for (int i = m_nth - 1; i >= 0; --i) {
        if (m_siblings[i]->isElementNode())
            return false;
    }

    return true;
}

inline bool ShadowDOMSiblingTraversalStrategy::isLastChild(Element& element) const
{
    ASSERT(element == toElement(m_siblings[m_nth]));

    for (size_t i = m_nth + 1; i < m_siblings.size(); ++i) {
        if (m_siblings[i]->isElementNode())
            return false;
    }

    return true;
}

inline bool ShadowDOMSiblingTraversalStrategy::isFirstOfType(Element& element, const QualifiedName& type) const
{
    ASSERT(element == toElement(m_siblings[m_nth]));

    for (int i = m_nth - 1; i >= 0; --i) {
        if (m_siblings[i]->isElementNode() && toElement(m_siblings[i])->hasTagName(type))
            return false;
    }

    return true;
}

inline bool ShadowDOMSiblingTraversalStrategy::isLastOfType(Element& element, const QualifiedName& type) const
{
    ASSERT(element == toElement(m_siblings[m_nth]));

    for (size_t i = m_nth + 1; i < m_siblings.size(); ++i) {
        if (m_siblings[i]->isElementNode() && toElement(m_siblings[i])->hasTagName(type))
            return false;
    }

    return true;
}

inline int ShadowDOMSiblingTraversalStrategy::countElementsBefore(Element& element) const
{
    ASSERT(element == toElement(m_siblings[m_nth]));

    int count = 0;
    for (int i = m_nth - 1; i >= 0; --i) {
        if (m_siblings[i]->isElementNode())
            ++count;
    }

    return count;
}

inline int ShadowDOMSiblingTraversalStrategy::countElementsAfter(Element& element) const
{
    ASSERT(element == toElement(m_siblings[m_nth]));

    int count = 0;
    for (size_t i = m_nth + 1; i < m_siblings.size(); ++i) {
        if (m_siblings[i]->isElementNode())
            return ++count;
    }

    return count;
}

inline int ShadowDOMSiblingTraversalStrategy::countElementsOfTypeBefore(Element& element, const QualifiedName& type) const
{
    ASSERT(element == toElement(m_siblings[m_nth]));

    int count = 0;
    for (int i = m_nth - 1; i >= 0; --i) {
        if (m_siblings[i]->isElementNode() && toElement(m_siblings[i])->hasTagName(type))
            ++count;
    }

    return count;
}

inline int ShadowDOMSiblingTraversalStrategy::countElementsOfTypeAfter(Element& element, const QualifiedName& type) const
{
    ASSERT(element == toElement(m_siblings[m_nth]));

    int count = 0;
    for (size_t i = m_nth + 1; i < m_siblings.size(); ++i) {
        if (m_siblings[i]->isElementNode() && toElement(m_siblings[i])->hasTagName(type))
            return ++count;
    }

    return count;
}

}

#endif
