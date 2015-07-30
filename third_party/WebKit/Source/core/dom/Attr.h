/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef Attr_h
#define Attr_h

#include "core/dom/ContainerNode.h"
#include "core/dom/QualifiedName.h"

namespace blink {

// Attr can have Text children
// therefore it has to be a fullblown Node. The plan
// is to dynamically allocate a textchild and store the
// resulting nodevalue in the attribute upon
// destruction. however, this is not yet implemented.

class Attr final : public ContainerNode {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<Attr> create(Element&, const QualifiedName&);
    static PassRefPtrWillBeRawPtr<Attr> create(Document&, const QualifiedName&, const AtomicString& value);
    virtual ~Attr();

    String name() const { return m_name.toString(); }
    bool specified() const { return true; }
    Element* ownerElement() const { return m_element; }

    const AtomicString& value() const;
    void setValue(const AtomicString&);

    const AtomicString& valueForBindings() const;
    void setValueForBindings(const AtomicString&);

    const QualifiedName qualifiedName() const;

    void attachToElement(Element*, const AtomicString&);
    void detachFromElementWithValue(const AtomicString&);

    virtual const AtomicString& localName() const override { return m_name.localName(); }
    virtual const AtomicString& namespaceURI() const override { return m_name.namespaceURI(); }
    const AtomicString& prefix() const { return m_name.prefix(); }

    DECLARE_VIRTUAL_TRACE();

private:
    Attr(Element&, const QualifiedName&);
    Attr(Document&, const QualifiedName&, const AtomicString& value);

    bool isElementNode() const = delete; // This will catch anyone doing an unnecessary check.

    void createTextChild();

    void setValueInternal(const AtomicString&);

    virtual String nodeName() const override { return name(); }
    virtual NodeType nodeType() const override { return ATTRIBUTE_NODE; }

    virtual String nodeValue() const override { return value(); }
    virtual void setNodeValue(const String&) override;
    virtual PassRefPtrWillBeRawPtr<Node> cloneNode(bool deep = true) override;

    virtual bool isAttributeNode() const override { return true; }
    virtual bool childTypeAllowed(NodeType) const override;

    virtual void childrenChanged(const ChildrenChange&) override;

    void updateElementAttribute(const AtomicString&);

    // Attr wraps either an element/name, or a name/value pair (when it's a standalone Node.)
    // Note that m_name is always set, but m_element/m_standaloneValue may be null.
    RawPtrWillBeMember<Element> m_element;
    QualifiedName m_name;
    // Holds the value if it is a standalone Node, or the local name of the
    // attribute it is attached to on an Element. The latter may (letter case)
    // differ from m_name's local name. As these two modes are non-overlapping,
    // use a single field.
    AtomicString m_standaloneValueOrAttachedLocalName;
    unsigned m_ignoreChildrenChanged;
};

DEFINE_NODE_TYPE_CASTS(Attr, isAttributeNode());

} // namespace blink

#endif // Attr_h
