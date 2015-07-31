/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#ifndef SVGUseElement_h
#define SVGUseElement_h

#include "core/SVGNames.h"
#include "core/fetch/DocumentResource.h"
#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGGraphicsElement.h"
#include "core/svg/SVGURIReference.h"
#include "platform/heap/Handle.h"

namespace blink {

class DocumentResource;

class SVGUseElement final : public SVGGraphicsElement,
                            public SVGURIReference,
                            public DocumentResourceClient {
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(SVGUseElement);
public:
    static PassRefPtrWillBeRawPtr<SVGUseElement> create(Document&);
    virtual ~SVGUseElement();

    void invalidateShadowTree();

    LayoutObject* layoutObjectClipChild() const;

    SVGAnimatedLength* x() const { return m_x.get(); }
    SVGAnimatedLength* y() const { return m_y.get(); }
    SVGAnimatedLength* width() const { return m_width.get(); }
    SVGAnimatedLength* height() const { return m_height.get(); }

    virtual void buildPendingResource() override;

    DECLARE_VIRTUAL_TRACE();

private:
    explicit SVGUseElement(Document&);

    virtual bool isPresentationAttribute(const QualifiedName&) const override;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) override;
    virtual bool isPresentationAttributeWithSVGDOM(const QualifiedName&) const override;

    virtual bool isStructurallyExternal() const override { return !hrefString().isNull() && isExternalURIReference(hrefString(), document()); }

    virtual InsertionNotificationRequest insertedInto(ContainerNode*) override;
    virtual void removedFrom(ContainerNode*) override;

    virtual void svgAttributeChanged(const QualifiedName&) override;

    virtual LayoutObject* createLayoutObject(const ComputedStyle&) override;
    virtual void toClipPath(Path&) override;

    void clearResourceReferences();
    void buildShadowAndInstanceTree(SVGElement* target);

    void scheduleShadowTreeRecreation();
    virtual bool haveLoadedRequiredResources() override { return !isStructurallyExternal() || m_haveFiredLoadEvent; }

    virtual bool selfHasRelativeLengths() const override;

    // Instance tree handling
    bool buildShadowTree(SVGElement* target, SVGElement* targetInstance, bool foundUse);
    bool hasCycleUseReferencing(SVGUseElement*, ContainerNode* targetInstance, SVGElement*& newTarget);
    bool expandUseElementsInShadowTree(SVGElement*);
    void expandSymbolElementsInShadowTree(SVGElement*);

    void transferUseAttributesToReplacedElement(SVGElement* from, SVGElement* to) const;

    void invalidateDependentShadowTrees();

    bool resourceIsStillLoading();
    Document* externalDocument() const;
    bool instanceTreeIsLoading(SVGElement*);
    virtual void notifyFinished(Resource*) override;
    TreeScope* referencedScope() const;
    void setDocumentResource(ResourcePtr<DocumentResource>);

    virtual Timer<SVGElement>* svgLoadEventTimer() override { return &m_svgLoadEventTimer; }

    RefPtrWillBeMember<SVGAnimatedLength> m_x;
    RefPtrWillBeMember<SVGAnimatedLength> m_y;
    RefPtrWillBeMember<SVGAnimatedLength> m_width;
    RefPtrWillBeMember<SVGAnimatedLength> m_height;

    bool m_haveFiredLoadEvent;
    bool m_needsShadowTreeRecreation;
    RefPtrWillBeMember<SVGElement> m_targetElementInstance;
    ResourcePtr<DocumentResource> m_resource;
    Timer<SVGElement> m_svgLoadEventTimer;
};

} // namespace blink

#endif // SVGUseElement_h
