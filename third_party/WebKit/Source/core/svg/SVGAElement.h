/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#ifndef SVGAElement_h
#define SVGAElement_h

#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGGraphicsElement.h"
#include "core/svg/SVGURIReference.h"

namespace blink {

class SVGAElement final : public SVGGraphicsElement,
                          public SVGURIReference {
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(SVGAElement);
public:
    DECLARE_NODE_FACTORY(SVGAElement);
    SVGAnimatedString* svgTarget() { return m_svgTarget.get(); }

    DECLARE_VIRTUAL_TRACE();

private:
    explicit SVGAElement(Document&);

    virtual String title() const override;

    virtual void svgAttributeChanged(const QualifiedName&) override;

    virtual LayoutObject* createLayoutObject(const ComputedStyle&) override;

    virtual void defaultEventHandler(Event*) override;

    virtual bool isLiveLink() const override { return isLink(); }

    virtual bool supportsFocus() const override;
    virtual bool shouldHaveFocusAppearance() const override final;
    virtual void dispatchFocusEvent(Element* oldFocusedElement, WebFocusType) override;
    virtual void dispatchBlurEvent(Element* newFocusedElement, WebFocusType) override;
    virtual bool isMouseFocusable() const override;
    virtual bool isKeyboardFocusable() const override;
    virtual bool isURLAttribute(const Attribute&) const override;
    virtual bool canStartSelection() const override;
    virtual short tabIndex() const override;

    virtual bool willRespondToMouseClickEvents() override;

    RefPtrWillBeMember<SVGAnimatedString> m_svgTarget;
    bool m_wasFocusedByMouse;
};

} // namespace blink

#endif // SVGAElement_h
