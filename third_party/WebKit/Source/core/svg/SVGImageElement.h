/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#ifndef SVGImageElement_h
#define SVGImageElement_h

#include "core/SVGNames.h"
#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGAnimatedPreserveAspectRatio.h"
#include "core/svg/SVGGraphicsElement.h"
#include "core/svg/SVGImageLoader.h"
#include "core/svg/SVGURIReference.h"
#include "platform/heap/Handle.h"

namespace blink {

class SVGImageElement final : public SVGGraphicsElement,
                              public SVGURIReference {
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(SVGImageElement);
public:
    DECLARE_NODE_FACTORY(SVGImageElement);
    DECLARE_VIRTUAL_TRACE();

    bool currentFrameHasSingleSecurityOrigin() const;

    SVGAnimatedLength* x() const { return m_x.get(); }
    SVGAnimatedLength* y() const { return m_y.get(); }
    SVGAnimatedLength* width() const { return m_width.get(); }
    SVGAnimatedLength* height() const { return m_height.get(); }
    SVGAnimatedPreserveAspectRatio* preserveAspectRatio() { return m_preserveAspectRatio.get(); }

private:
    explicit SVGImageElement(Document&);

    virtual bool isStructurallyExternal() const override { return !hrefString().isNull(); }

    virtual bool isPresentationAttribute(const QualifiedName&) const override;
    virtual bool isPresentationAttributeWithSVGDOM(const QualifiedName&) const override;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) override;

    virtual void svgAttributeChanged(const QualifiedName&) override;

    virtual void attach(const AttachContext& = AttachContext()) override;
    virtual InsertionNotificationRequest insertedInto(ContainerNode*) override;

    virtual LayoutObject* createLayoutObject(const ComputedStyle&) override;

    virtual const AtomicString imageSourceURL() const override;

    virtual bool haveLoadedRequiredResources() override;

    virtual bool selfHasRelativeLengths() const override;
    virtual void didMoveToNewDocument(Document& oldDocument) override;
    SVGImageLoader& imageLoader() { return *m_imageLoader; }

    RefPtrWillBeMember<SVGAnimatedLength> m_x;
    RefPtrWillBeMember<SVGAnimatedLength> m_y;
    RefPtrWillBeMember<SVGAnimatedLength> m_width;
    RefPtrWillBeMember<SVGAnimatedLength> m_height;
    RefPtrWillBeMember<SVGAnimatedPreserveAspectRatio> m_preserveAspectRatio;

    OwnPtrWillBeMember<SVGImageLoader> m_imageLoader;
    bool m_needsLoaderURIUpdate : 1;
};

} // namespace blink

#endif // SVGImageElement_h
