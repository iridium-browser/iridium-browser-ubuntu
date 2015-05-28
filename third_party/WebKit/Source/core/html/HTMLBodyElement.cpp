/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#include "config.h"
#include "core/html/HTMLBodyElement.h"

#include "bindings/core/v8/ScriptEventListener.h"
#include "core/CSSValueKeywords.h"
#include "core/HTMLNames.h"
#include "core/css/CSSImageValue.h"
#include "core/css/StylePropertySet.h"
#include "core/css/parser/CSSParser.h"
#include "core/dom/Attribute.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/ScrollToOptions.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/layout/LayoutBox.h"

namespace blink {

using namespace HTMLNames;

inline HTMLBodyElement::HTMLBodyElement(Document& document)
    : HTMLElement(bodyTag, document)
{
}

DEFINE_NODE_FACTORY(HTMLBodyElement)

HTMLBodyElement::~HTMLBodyElement()
{
}

bool HTMLBodyElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == backgroundAttr || name == marginwidthAttr || name == leftmarginAttr || name == marginheightAttr || name == topmarginAttr || name == bgcolorAttr || name == textAttr)
        return true;
    return HTMLElement::isPresentationAttribute(name);
}

void HTMLBodyElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStylePropertySet* style)
{
    if (name == backgroundAttr) {
        String url = stripLeadingAndTrailingHTMLSpaces(value);
        if (!url.isEmpty()) {
            RefPtrWillBeRawPtr<CSSImageValue> imageValue = CSSImageValue::create(url, document().completeURL(url));
            imageValue->setInitiator(localName());
            imageValue->setReferrer(Referrer(document().outgoingReferrer(), document().referrerPolicy()));
            style->setProperty(CSSProperty(CSSPropertyBackgroundImage, imageValue.release()));
        }
    } else if (name == marginwidthAttr || name == leftmarginAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
    } else if (name == marginheightAttr || name == topmarginAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
    } else if (name == bgcolorAttr) {
        addHTMLColorToStyle(style, CSSPropertyBackgroundColor, value);
    } else if (name == textAttr) {
        addHTMLColorToStyle(style, CSSPropertyColor, value);
    } else {
        HTMLElement::collectStyleForPresentationAttribute(name, value, style);
    }
}

void HTMLBodyElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == vlinkAttr || name == alinkAttr || name == linkAttr) {
        if (value.isNull()) {
            if (name == linkAttr)
                document().textLinkColors().resetLinkColor();
            else if (name == vlinkAttr)
                document().textLinkColors().resetVisitedLinkColor();
            else
                document().textLinkColors().resetActiveLinkColor();
        } else {
            RGBA32 color;
            if (CSSParser::parseColor(color, value, !document().inQuirksMode())) {
                if (name == linkAttr)
                    document().textLinkColors().setLinkColor(color);
                else if (name == vlinkAttr)
                    document().textLinkColors().setVisitedLinkColor(color);
                else
                    document().textLinkColors().setActiveLinkColor(color);
            }
        }

        setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::LinkColorChange));
    } else if (name == onloadAttr)
        document().setWindowAttributeEventListener(EventTypeNames::load, createAttributeEventListener(document().frame(), name, value, eventParameterName()));
    else if (name == onbeforeunloadAttr)
        document().setWindowAttributeEventListener(EventTypeNames::beforeunload, createAttributeEventListener(document().frame(), name, value, eventParameterName()));
    else if (name == onunloadAttr)
        document().setWindowAttributeEventListener(EventTypeNames::unload, createAttributeEventListener(document().frame(), name, value, eventParameterName()));
    else if (name == onpagehideAttr)
        document().setWindowAttributeEventListener(EventTypeNames::pagehide, createAttributeEventListener(document().frame(), name, value, eventParameterName()));
    else if (name == onpageshowAttr)
        document().setWindowAttributeEventListener(EventTypeNames::pageshow, createAttributeEventListener(document().frame(), name, value, eventParameterName()));
    else if (name == onpopstateAttr)
        document().setWindowAttributeEventListener(EventTypeNames::popstate, createAttributeEventListener(document().frame(), name, value, eventParameterName()));
    else if (name == onblurAttr)
        document().setWindowAttributeEventListener(EventTypeNames::blur, createAttributeEventListener(document().frame(), name, value, eventParameterName()));
    else if (name == onerrorAttr)
        document().setWindowAttributeEventListener(EventTypeNames::error, createAttributeEventListener(document().frame(), name, value, eventParameterName()));
    else if (name == onfocusAttr)
        document().setWindowAttributeEventListener(EventTypeNames::focus, createAttributeEventListener(document().frame(), name, value, eventParameterName()));
    else if (RuntimeEnabledFeatures::orientationEventEnabled() && name == onorientationchangeAttr)
        document().setWindowAttributeEventListener(EventTypeNames::orientationchange, createAttributeEventListener(document().frame(), name, value, eventParameterName()));
    else if (name == onhashchangeAttr)
        document().setWindowAttributeEventListener(EventTypeNames::hashchange, createAttributeEventListener(document().frame(), name, value, eventParameterName()));
    else if (name == onmessageAttr)
        document().setWindowAttributeEventListener(EventTypeNames::message, createAttributeEventListener(document().frame(), name, value, eventParameterName()));
    else if (name == onresizeAttr)
        document().setWindowAttributeEventListener(EventTypeNames::resize, createAttributeEventListener(document().frame(), name, value, eventParameterName()));
    else if (name == onscrollAttr)
        document().setWindowAttributeEventListener(EventTypeNames::scroll, createAttributeEventListener(document().frame(), name, value, eventParameterName()));
    else if (name == onselectionchangeAttr)
        document().setAttributeEventListener(EventTypeNames::selectionchange, createAttributeEventListener(document().frame(), name, value, eventParameterName()));
    else if (name == onstorageAttr)
        document().setWindowAttributeEventListener(EventTypeNames::storage, createAttributeEventListener(document().frame(), name, value, eventParameterName()));
    else if (name == ononlineAttr)
        document().setWindowAttributeEventListener(EventTypeNames::online, createAttributeEventListener(document().frame(), name, value, eventParameterName()));
    else if (name == onofflineAttr)
        document().setWindowAttributeEventListener(EventTypeNames::offline, createAttributeEventListener(document().frame(), name, value, eventParameterName()));
    else if (name == onlanguagechangeAttr)
        document().setWindowAttributeEventListener(EventTypeNames::languagechange, createAttributeEventListener(document().frame(), name, value, eventParameterName()));
    else
        HTMLElement::parseAttribute(name, value);
}

Node::InsertionNotificationRequest HTMLBodyElement::insertedInto(ContainerNode* insertionPoint)
{
    HTMLElement::insertedInto(insertionPoint);
    return InsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLBodyElement::didNotifySubtreeInsertionsToDocument()
{
    // FIXME: It's surprising this is web compatible since it means a
    // marginwidth and marginheight attribute can magically appear on the <body>
    // of all documents embedded through <iframe> or <frame>.
    HTMLFrameOwnerElement* ownerElement = document().ownerElement();
    if (!isHTMLFrameElementBase(ownerElement))
        return;
    HTMLFrameElementBase& ownerFrameElement = toHTMLFrameElementBase(*ownerElement);
    int marginWidth = ownerFrameElement.marginWidth();
    int marginHeight = ownerFrameElement.marginHeight();
    if (marginWidth != -1)
        setIntegralAttribute(marginwidthAttr, marginWidth);
    if (marginHeight != -1)
        setIntegralAttribute(marginheightAttr, marginHeight);
}

bool HTMLBodyElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == backgroundAttr || HTMLElement::isURLAttribute(attribute);
}

bool HTMLBodyElement::hasLegalLinkAttribute(const QualifiedName& name) const
{
    return name == backgroundAttr || HTMLElement::hasLegalLinkAttribute(name);
}

const QualifiedName& HTMLBodyElement::subResourceAttributeName() const
{
    return backgroundAttr;
}

bool HTMLBodyElement::supportsFocus() const
{
    // This override is needed because the inherited method bails if the parent is editable.
    // The <body> should be focusable even if <html> is editable.
    return hasEditableStyle() || HTMLElement::supportsFocus();
}

static int adjustForZoom(int value, Document* document)
{
    LocalFrame* frame = document->frame();
    float zoomFactor = frame->pageZoomFactor();
    if (zoomFactor == 1)
        return value;
    // Needed because of truncation (rather than rounding) when scaling up.
    if (zoomFactor > 1)
        value++;
    return static_cast<int>(value / zoomFactor);
}

// Blink, Gecko and Presto's quirks mode implementations of overflow set to the
// body element differ from IE's: the formers can create a scrollable area for the
// body element that is not the same as the root elements's one. On IE's quirks mode
// though, as body is the root element, body's and the root element's scrollable areas,
// if any, are the same.
// In order words, a <body> will only have an overflow clip (that differs from
// documentElement's) if  both html and body nodes have its overflow set to either hidden,
// auto or scroll.
// That said, Blink's {set}scroll{Top,Left} behaviors match Gecko's: even if there is a non-overflown
// scrollable area, scrolling should not get propagated to the viewport in neither strict
// or quirks modes.
double HTMLBodyElement::scrollLeft()
{
    Document& document = this->document();
    document.updateLayoutIgnorePendingStylesheets();

    if (RuntimeEnabledFeatures::scrollTopLeftInteropEnabled()) {
        LayoutBox* render = layoutBox();
        if (!render)
            return 0;
        if (render->hasOverflowClip())
            return adjustScrollForAbsoluteZoom(render->scrollLeft(), *render);
        if (!document.inQuirksMode())
            return 0;
    }

    if (LocalDOMWindow* window = document.domWindow())
        return window->scrollX();
    return 0;
}

void HTMLBodyElement::setScrollLeft(double scrollLeft)
{
    Document& document = this->document();
    document.updateLayoutIgnorePendingStylesheets();

    if (std::isnan(scrollLeft))
        return;

    if (RuntimeEnabledFeatures::scrollTopLeftInteropEnabled()) {
        LayoutBox* render = layoutBox();
        if (!render)
            return;
        if (render->hasOverflowClip()) {
            // FIXME: Investigate how are other browsers casting to int (rounding, ceiling, ...).
            render->setScrollLeft(static_cast<int>(scrollLeft * render->style()->effectiveZoom()));
            return;
        }
        if (!document.inQuirksMode())
            return;
    }

    if (LocalDOMWindow* window = document.domWindow())
        window->scrollTo(scrollLeft, window->scrollY());
}

double HTMLBodyElement::scrollTop()
{
    Document& document = this->document();
    document.updateLayoutIgnorePendingStylesheets();

    if (RuntimeEnabledFeatures::scrollTopLeftInteropEnabled()) {
        LayoutBox* render = layoutBox();
        if (!render)
            return 0;
        if (render->hasOverflowClip())
            return adjustLayoutUnitForAbsoluteZoom(render->scrollTop(), *render);
        if (!document.inQuirksMode())
            return 0;
    }

    if (LocalDOMWindow* window = document.domWindow())
        return window->scrollY();
    return 0;
}

void HTMLBodyElement::setScrollTop(double scrollTop)
{
    Document& document = this->document();
    document.updateLayoutIgnorePendingStylesheets();

    if (std::isnan(scrollTop))
        return;

    if (RuntimeEnabledFeatures::scrollTopLeftInteropEnabled()) {
        LayoutBox* render = layoutBox();
        if (!render)
            return;
        if (render->hasOverflowClip()) {
            // FIXME: Investigate how are other browsers casting to int (rounding, ceiling, ...).
            render->setScrollTop(static_cast<int>(scrollTop * render->style()->effectiveZoom()));
            return;
        }
        if (!document.inQuirksMode())
            return;
    }

    if (LocalDOMWindow* window = document.domWindow())
        window->scrollTo(window->scrollX(), scrollTop);
}

int HTMLBodyElement::scrollHeight()
{
    // Update the document's layout.
    Document& document = this->document();
    document.updateLayoutIgnorePendingStylesheets();
    FrameView* view = document.view();
    return view ? adjustForZoom(view->contentsHeight(), &document) : 0;
}

int HTMLBodyElement::scrollWidth()
{
    // Update the document's layout.
    Document& document = this->document();
    document.updateLayoutIgnorePendingStylesheets();
    FrameView* view = document.view();
    return view ? adjustForZoom(view->contentsWidth(), &document) : 0;
}

void HTMLBodyElement::scrollBy(const ScrollToOptions& scrollToOptions)
{
    Document& document = this->document();

    // FIXME: This should be removed once scroll updates are processed only after
    // the compositing update. See http://crbug.com/420741.
    document.updateLayoutIgnorePendingStylesheets();

    if (RuntimeEnabledFeatures::scrollTopLeftInteropEnabled()) {
        LayoutBox* render = layoutBox();
        if (!render)
            return;
        if (render->hasOverflowClip()) {
            scrollLayoutBoxBy(scrollToOptions);
            return;
        }
        if (!document.inQuirksMode())
            return;
    }

    scrollFrameBy(scrollToOptions);
}

void HTMLBodyElement::scrollTo(const ScrollToOptions& scrollToOptions)
{
    Document& document = this->document();

    // FIXME: This should be removed once scroll updates are processed only after
    // the compositing update. See http://crbug.com/420741.
    document.updateLayoutIgnorePendingStylesheets();

    if (RuntimeEnabledFeatures::scrollTopLeftInteropEnabled()) {
        LayoutBox* render = layoutBox();
        if (!render)
            return;
        if (render->hasOverflowClip()) {
            scrollLayoutBoxTo(scrollToOptions);
            return;
        }
        if (!document.inQuirksMode())
            return;
    }

    scrollFrameTo(scrollToOptions);
}

} // namespace blink
