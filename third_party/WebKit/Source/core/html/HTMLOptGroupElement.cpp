/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010 Apple Inc. All rights
 * reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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

#include "core/html/HTMLOptGroupElement.h"

#include "core/HTMLNames.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/Text.h"
#include "core/editing/EditingUtilities.h"
#include "core/html/HTMLContentElement.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/style/ComputedStyle.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/CharacterNames.h"

namespace blink {

using namespace HTMLNames;

inline HTMLOptGroupElement::HTMLOptGroupElement(Document& document)
    : HTMLElement(optgroupTag, document) {
}

// An explicit empty destructor should be in HTMLOptGroupElement.cpp, because
// if an implicit destructor is used or an empty destructor is defined in
// HTMLOptGroupElement.h, when including HTMLOptGroupElement.h,
// msvc tries to expand the destructor and causes
// a compile error because of lack of ComputedStyle definition.
HTMLOptGroupElement::~HTMLOptGroupElement() {}

HTMLOptGroupElement* HTMLOptGroupElement::create(Document& document) {
  HTMLOptGroupElement* optGroupElement = new HTMLOptGroupElement(document);
  optGroupElement->ensureUserAgentShadowRoot();
  return optGroupElement;
}

bool HTMLOptGroupElement::isDisabledFormControl() const {
  return fastHasAttribute(disabledAttr);
}

void HTMLOptGroupElement::parseAttribute(
    const AttributeModificationParams& params) {
  HTMLElement::parseAttribute(params);

  if (params.name == disabledAttr) {
    pseudoStateChanged(CSSSelector::PseudoDisabled);
    pseudoStateChanged(CSSSelector::PseudoEnabled);
  } else if (params.name == labelAttr) {
    updateGroupLabel();
  }
}

bool HTMLOptGroupElement::supportsFocus() const {
  HTMLSelectElement* select = ownerSelectElement();
  if (select && select->usesMenuList())
    return false;
  return HTMLElement::supportsFocus();
}

bool HTMLOptGroupElement::matchesEnabledPseudoClass() const {
  return !isDisabledFormControl();
}

Node::InsertionNotificationRequest HTMLOptGroupElement::insertedInto(
    ContainerNode* insertionPoint) {
  HTMLElement::insertedInto(insertionPoint);
  if (HTMLSelectElement* select = ownerSelectElement()) {
    if (insertionPoint == select)
      select->optGroupInsertedOrRemoved(*this);
  }
  return InsertionDone;
}

void HTMLOptGroupElement::removedFrom(ContainerNode* insertionPoint) {
  if (isHTMLSelectElement(*insertionPoint)) {
    if (!parentNode())
      toHTMLSelectElement(insertionPoint)->optGroupInsertedOrRemoved(*this);
  }
  HTMLElement::removedFrom(insertionPoint);
}

String HTMLOptGroupElement::groupLabelText() const {
  String itemText = getAttribute(labelAttr);

  // In WinIE, leading and trailing whitespace is ignored in options and
  // optgroups. We match this behavior.
  itemText = itemText.stripWhiteSpace();
  // We want to collapse our whitespace too.  This will match other browsers.
  itemText = itemText.simplifyWhiteSpace();

  return itemText;
}

HTMLSelectElement* HTMLOptGroupElement::ownerSelectElement() const {
  // TODO(tkent): We should return only the parent <select>.
  return Traversal<HTMLSelectElement>::firstAncestor(*this);
}

String HTMLOptGroupElement::defaultToolTip() const {
  if (HTMLSelectElement* select = ownerSelectElement())
    return select->defaultToolTip();
  return String();
}

void HTMLOptGroupElement::accessKeyAction(bool) {
  HTMLSelectElement* select = ownerSelectElement();
  // send to the parent to bring focus to the list box
  if (select && !select->isFocused())
    select->accessKeyAction(false);
}

void HTMLOptGroupElement::didAddUserAgentShadowRoot(ShadowRoot& root) {
  DEFINE_STATIC_LOCAL(AtomicString, labelPadding, ("0 2px 1px 2px"));
  DEFINE_STATIC_LOCAL(AtomicString, labelMinHeight, ("1.2em"));
  HTMLDivElement* label = HTMLDivElement::create(document());
  label->setAttribute(roleAttr, AtomicString("group"));
  label->setAttribute(aria_labelAttr, AtomicString());
  label->setInlineStyleProperty(CSSPropertyPadding, labelPadding);
  label->setInlineStyleProperty(CSSPropertyMinHeight, labelMinHeight);
  label->setIdAttribute(ShadowElementNames::optGroupLabel());
  root.appendChild(label);

  HTMLContentElement* content = HTMLContentElement::create(document());
  content->setAttribute(selectAttr, "option,hr");
  root.appendChild(content);
}

void HTMLOptGroupElement::updateGroupLabel() {
  const String& labelText = groupLabelText();
  HTMLDivElement& label = optGroupLabelElement();
  label.setTextContent(labelText);
  label.setAttribute(aria_labelAttr, AtomicString(labelText));
}

HTMLDivElement& HTMLOptGroupElement::optGroupLabelElement() const {
  return *toHTMLDivElementOrDie(userAgentShadowRoot()->getElementById(
      ShadowElementNames::optGroupLabel()));
}

}  // namespace blink
