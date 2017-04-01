/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Motorola Mobility, Inc.  All rights reserved.
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

#include "core/html/HTMLOptionElement.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/Document.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/ScriptLoader.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLDataListElement.h"
#include "core/html/HTMLOptGroupElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/layout/LayoutTheme.h"
#include "core/style/ComputedStyle.h"
#include "wtf/Vector.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

using namespace HTMLNames;

HTMLOptionElement::HTMLOptionElement(Document& document)
    : HTMLElement(optionTag, document), m_isSelected(false) {
  setHasCustomStyleCallbacks();
}

// An explicit empty destructor should be in HTMLOptionElement.cpp, because
// if an implicit destructor is used or an empty destructor is defined in
// HTMLOptionElement.h, when including HTMLOptionElement.h,
// msvc tries to expand the destructor and causes
// a compile error because of lack of ComputedStyle definition.
HTMLOptionElement::~HTMLOptionElement() {}

HTMLOptionElement* HTMLOptionElement::create(Document& document) {
  HTMLOptionElement* option = new HTMLOptionElement(document);
  option->ensureUserAgentShadowRoot();
  return option;
}

HTMLOptionElement* HTMLOptionElement::createForJSConstructor(
    Document& document,
    const String& data,
    const AtomicString& value,
    bool defaultSelected,
    bool selected,
    ExceptionState& exceptionState) {
  HTMLOptionElement* element = new HTMLOptionElement(document);
  element->ensureUserAgentShadowRoot();
  element->appendChild(Text::create(document, data.isNull() ? "" : data),
                       exceptionState);
  if (exceptionState.hadException())
    return nullptr;

  if (!value.isNull())
    element->setValue(value);
  if (defaultSelected)
    element->setAttribute(selectedAttr, emptyAtom);
  element->setSelected(selected);

  return element;
}

void HTMLOptionElement::attachLayoutTree(const AttachContext& context) {
  AttachContext optionContext(context);
  if (context.resolvedStyle) {
    DCHECK(!m_style || m_style == context.resolvedStyle);
    m_style = context.resolvedStyle;
  } else if (parentComputedStyle()) {
    updateNonComputedStyle();
    optionContext.resolvedStyle = m_style.get();
  }
  HTMLElement::attachLayoutTree(optionContext);
}

void HTMLOptionElement::detachLayoutTree(const AttachContext& context) {
  m_style.clear();
  HTMLElement::detachLayoutTree(context);
}

bool HTMLOptionElement::supportsFocus() const {
  HTMLSelectElement* select = ownerSelectElement();
  if (select && select->usesMenuList())
    return false;
  return HTMLElement::supportsFocus();
}

bool HTMLOptionElement::matchesDefaultPseudoClass() const {
  return fastHasAttribute(selectedAttr);
}

bool HTMLOptionElement::matchesEnabledPseudoClass() const {
  return !isDisabledFormControl();
}

String HTMLOptionElement::displayLabel() const {
  Document& document = this->document();
  String text;

  // WinIE does not use the label attribute, so as a quirk, we ignore it.
  if (!document.inQuirksMode())
    text = fastGetAttribute(labelAttr);

  // FIXME: The following treats an element with the label attribute set to
  // the empty string the same as an element with no label attribute at all.
  // Is that correct? If it is, then should the label function work the same
  // way?
  if (text.isEmpty())
    text = collectOptionInnerText();

  return text.stripWhiteSpace(isHTMLSpace<UChar>)
      .simplifyWhiteSpace(isHTMLSpace<UChar>);
}

String HTMLOptionElement::text() const {
  return collectOptionInnerText()
      .stripWhiteSpace(isHTMLSpace<UChar>)
      .simplifyWhiteSpace(isHTMLSpace<UChar>);
}

void HTMLOptionElement::setText(const String& text,
                                ExceptionState& exceptionState) {
  // Changing the text causes a recalc of a select's items, which will reset the
  // selected index to the first item if the select is single selection with a
  // menu list.  We attempt to preserve the selected item.
  HTMLSelectElement* select = ownerSelectElement();
  bool selectIsMenuList = select && select->usesMenuList();
  int oldSelectedIndex = selectIsMenuList ? select->selectedIndex() : -1;

  if (hasOneTextChild()) {
    toText(firstChild())->setData(text);
  } else {
    removeChildren();
    appendChild(Text::create(document(), text), exceptionState);
  }

  if (selectIsMenuList && select->selectedIndex() != oldSelectedIndex)
    select->setSelectedIndex(oldSelectedIndex);
}

void HTMLOptionElement::accessKeyAction(bool) {
  if (HTMLSelectElement* select = ownerSelectElement())
    select->selectOptionByAccessKey(this);
}

int HTMLOptionElement::index() const {
  // It would be faster to cache the index, but harder to get it right in all
  // cases.

  HTMLSelectElement* selectElement = ownerSelectElement();
  if (!selectElement)
    return 0;

  int optionIndex = 0;
  for (const auto& option : selectElement->optionList()) {
    if (option == this)
      return optionIndex;
    ++optionIndex;
  }

  return 0;
}

int HTMLOptionElement::listIndex() const {
  if (HTMLSelectElement* selectElement = ownerSelectElement())
    return selectElement->listIndexForOption(*this);
  return -1;
}

void HTMLOptionElement::parseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  if (name == valueAttr) {
    if (HTMLDataListElement* dataList = ownerDataListElement())
      dataList->optionElementChildrenChanged();
  } else if (name == disabledAttr) {
    if (params.oldValue.isNull() != params.newValue.isNull()) {
      pseudoStateChanged(CSSSelector::PseudoDisabled);
      pseudoStateChanged(CSSSelector::PseudoEnabled);
      if (layoutObject())
        LayoutTheme::theme().controlStateChanged(*layoutObject(),
                                                 EnabledControlState);
    }
  } else if (name == selectedAttr) {
    if (params.oldValue.isNull() != params.newValue.isNull() && !m_isDirty)
      setSelected(!params.newValue.isNull());
    pseudoStateChanged(CSSSelector::PseudoDefault);
  } else if (name == labelAttr) {
    updateLabel();
  } else {
    HTMLElement::parseAttribute(params);
  }
}

String HTMLOptionElement::value() const {
  const AtomicString& value = fastGetAttribute(valueAttr);
  if (!value.isNull())
    return value;
  return collectOptionInnerText()
      .stripWhiteSpace(isHTMLSpace<UChar>)
      .simplifyWhiteSpace(isHTMLSpace<UChar>);
}

void HTMLOptionElement::setValue(const AtomicString& value) {
  setAttribute(valueAttr, value);
}

bool HTMLOptionElement::selected() const {
  return m_isSelected;
}

void HTMLOptionElement::setSelected(bool selected) {
  if (m_isSelected == selected)
    return;

  setSelectedState(selected);

  if (HTMLSelectElement* select = ownerSelectElement())
    select->optionSelectionStateChanged(this, selected);
}

bool HTMLOptionElement::selectedForBinding() const {
  return selected();
}

void HTMLOptionElement::setSelectedForBinding(bool selected) {
  bool wasSelected = m_isSelected;
  setSelected(selected);

  // As of December 2015, the HTML specification says the dirtiness becomes
  // true by |selected| setter unconditionally. However it caused a real bug,
  // crbug.com/570367, and is not compatible with other browsers.
  // Firefox seems not to set dirtiness if an option is owned by a select
  // element and selectedness is not changed.
  if (ownerSelectElement() && wasSelected == m_isSelected)
    return;

  m_isDirty = true;
}

void HTMLOptionElement::setSelectedState(bool selected) {
  if (m_isSelected == selected)
    return;

  m_isSelected = selected;
  pseudoStateChanged(CSSSelector::PseudoChecked);

  if (HTMLSelectElement* select = ownerSelectElement()) {
    select->invalidateSelectedItems();

    if (AXObjectCache* cache = document().existingAXObjectCache()) {
      // If there is a layoutObject (most common), fire accessibility
      // notifications only when it's a listbox (and not a menu list). If
      // there's no layoutObject, fire them anyway just to be safe (to make sure
      // the AX tree is in sync).
      if (!select->layoutObject() || select->layoutObject()->isListBox()) {
        cache->listboxOptionStateChanged(this);
        cache->listboxSelectedChildrenChanged(select);
      }
    }
  }
}

void HTMLOptionElement::setDirty(bool value) {
  m_isDirty = true;
}

void HTMLOptionElement::childrenChanged(const ChildrenChange& change) {
  if (HTMLDataListElement* dataList = ownerDataListElement())
    dataList->optionElementChildrenChanged();
  else if (HTMLSelectElement* select = ownerSelectElement())
    select->optionElementChildrenChanged(*this);
  updateLabel();
  HTMLElement::childrenChanged(change);
}

HTMLDataListElement* HTMLOptionElement::ownerDataListElement() const {
  return Traversal<HTMLDataListElement>::firstAncestor(*this);
}

HTMLSelectElement* HTMLOptionElement::ownerSelectElement() const {
  if (!parentNode())
    return nullptr;
  if (isHTMLSelectElement(*parentNode()))
    return toHTMLSelectElement(parentNode());
  if (!isHTMLOptGroupElement(*parentNode()))
    return nullptr;
  Node* grandParent = parentNode()->parentNode();
  return isHTMLSelectElement(grandParent) ? toHTMLSelectElement(grandParent)
                                          : nullptr;
}

String HTMLOptionElement::label() const {
  const AtomicString& label = fastGetAttribute(labelAttr);
  if (!label.isNull())
    return label;
  return collectOptionInnerText()
      .stripWhiteSpace(isHTMLSpace<UChar>)
      .simplifyWhiteSpace(isHTMLSpace<UChar>);
}

void HTMLOptionElement::setLabel(const AtomicString& label) {
  setAttribute(labelAttr, label);
}

void HTMLOptionElement::updateNonComputedStyle() {
  m_style = originalStyleForLayoutObject();
  if (HTMLSelectElement* select = ownerSelectElement())
    select->updateListOnLayoutObject();
}

ComputedStyle* HTMLOptionElement::nonLayoutObjectComputedStyle() const {
  return m_style.get();
}

PassRefPtr<ComputedStyle> HTMLOptionElement::customStyleForLayoutObject() {
  updateNonComputedStyle();
  return m_style;
}

String HTMLOptionElement::textIndentedToRespectGroupLabel() const {
  ContainerNode* parent = parentNode();
  if (parent && isHTMLOptGroupElement(*parent))
    return "    " + displayLabel();
  return displayLabel();
}

bool HTMLOptionElement::ownElementDisabled() const {
  return fastHasAttribute(disabledAttr);
}

bool HTMLOptionElement::isDisabledFormControl() const {
  if (ownElementDisabled())
    return true;
  if (Element* parent = parentElement())
    return isHTMLOptGroupElement(*parent) && parent->isDisabledFormControl();
  return false;
}

String HTMLOptionElement::defaultToolTip() const {
  if (HTMLSelectElement* select = ownerSelectElement())
    return select->defaultToolTip();
  return String();
}

Node::InsertionNotificationRequest HTMLOptionElement::insertedInto(
    ContainerNode* insertionPoint) {
  HTMLElement::insertedInto(insertionPoint);
  if (HTMLSelectElement* select = ownerSelectElement()) {
    if (insertionPoint == select || (isHTMLOptGroupElement(*insertionPoint) &&
                                     insertionPoint->parentNode() == select))
      select->optionInserted(*this, m_isSelected);
  }
  return InsertionDone;
}

void HTMLOptionElement::removedFrom(ContainerNode* insertionPoint) {
  if (isHTMLSelectElement(*insertionPoint)) {
    if (!parentNode() || isHTMLOptGroupElement(*parentNode()))
      toHTMLSelectElement(insertionPoint)->optionRemoved(*this);
  } else if (isHTMLOptGroupElement(*insertionPoint)) {
    Node* parent = insertionPoint->parentNode();
    if (isHTMLSelectElement(parent))
      toHTMLSelectElement(parent)->optionRemoved(*this);
  }
  HTMLElement::removedFrom(insertionPoint);
}

String HTMLOptionElement::collectOptionInnerText() const {
  StringBuilder text;
  for (Node* node = firstChild(); node;) {
    if (node->isTextNode())
      text.append(node->nodeValue());
    // Text nodes inside script elements are not part of the option text.
    if (node->isElementNode() && toScriptLoaderIfPossible(toElement(node)))
      node = NodeTraversal::nextSkippingChildren(*node, this);
    else
      node = NodeTraversal::next(*node, this);
  }
  return text.toString();
}

HTMLFormElement* HTMLOptionElement::form() const {
  if (HTMLSelectElement* selectElement = ownerSelectElement())
    return selectElement->formOwner();

  return nullptr;
}

void HTMLOptionElement::didAddUserAgentShadowRoot(ShadowRoot& root) {
  updateLabel();
}

void HTMLOptionElement::updateLabel() {
  if (ShadowRoot* root = userAgentShadowRoot())
    root->setTextContent(displayLabel());
}

bool HTMLOptionElement::spatialNavigationFocused() const {
  HTMLSelectElement* select = ownerSelectElement();
  if (!select || !select->isFocused())
    return false;
  return select->spatialNavigationFocusedOption() == this;
}

bool HTMLOptionElement::isDisplayNone() const {
  // If m_style is not set, then the node is still unattached.
  // We have to wait till it gets attached to read the display property.
  if (!m_style)
    return false;

  if (m_style->display() != EDisplay::None) {
    // We need to check the parent's display property.  Parent's
    // display:none doesn't override children's display properties in
    // ComputedStyle.
    Element* parent = parentElement();
    DCHECK(parent);
    if (isHTMLOptGroupElement(*parent)) {
      const ComputedStyle* parentStyle = parent->computedStyle()
                                             ? parent->computedStyle()
                                             : parent->ensureComputedStyle();
      return !parentStyle || parentStyle->display() == EDisplay::None;
    }
  }
  return m_style->display() == EDisplay::None;
}

String HTMLOptionElement::innerText() {
  // A workaround for crbug.com/424578. We add ShadowRoot to an OPTION, but
  // innerText behavior for Shadow DOM is unclear.  We just return the same
  // string before adding ShadowRoot.
  return textContent();
}

}  // namespace blink
