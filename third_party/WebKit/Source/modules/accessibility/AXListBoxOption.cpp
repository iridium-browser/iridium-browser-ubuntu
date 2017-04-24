/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/accessibility/AXListBoxOption.h"

#include "core/html/HTMLOptionElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/layout/LayoutObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"

namespace blink {

using namespace HTMLNames;

AXListBoxOption::AXListBoxOption(LayoutObject* layoutObject,
                                 AXObjectCacheImpl& axObjectCache)
    : AXLayoutObject(layoutObject, axObjectCache) {}

AXListBoxOption::~AXListBoxOption() {}

AXListBoxOption* AXListBoxOption::create(LayoutObject* layoutObject,
                                         AXObjectCacheImpl& axObjectCache) {
  return new AXListBoxOption(layoutObject, axObjectCache);
}

AccessibilityRole AXListBoxOption::determineAccessibilityRole() {
  if ((m_ariaRole = determineAriaRoleAttribute()) != UnknownRole)
    return m_ariaRole;

  // http://www.w3.org/TR/wai-aria/complete#presentation
  // ARIA spec says that the presentation role causes a given element to be
  // treated as having no role or to be removed from the accessibility tree, but
  // does not cause the content contained within the element to be removed from
  // the accessibility tree.
  if (isParentPresentationalRole())
    return StaticTextRole;

  return ListBoxOptionRole;
}

bool AXListBoxOption::isParentPresentationalRole() const {
  AXObject* parent = parentObject();
  if (!parent)
    return false;

  LayoutObject* layoutObject = parent->getLayoutObject();
  if (!layoutObject)
    return false;

  if (layoutObject->isListBox() && parent->hasInheritedPresentationalRole())
    return true;

  return false;
}

bool AXListBoxOption::isEnabled() const {
  if (!getNode())
    return false;

  if (equalIgnoringCase(getAttribute(aria_disabledAttr), "true"))
    return false;

  if (toElement(getNode())->hasAttribute(disabledAttr))
    return false;

  return true;
}

bool AXListBoxOption::isSelected() const {
  return isHTMLOptionElement(getNode()) &&
         toHTMLOptionElement(getNode())->selected();
}

bool AXListBoxOption::isSelectedOptionActive() const {
  HTMLSelectElement* listBoxParentNode = listBoxOptionParentNode();
  if (!listBoxParentNode)
    return false;

  return listBoxParentNode->activeSelectionEnd() == getNode();
}

bool AXListBoxOption::computeAccessibilityIsIgnored(
    IgnoredReasons* ignoredReasons) const {
  if (!getNode())
    return true;

  if (accessibilityIsIgnoredByDefault(ignoredReasons))
    return true;

  return false;
}

bool AXListBoxOption::canSetSelectedAttribute() const {
  if (!isHTMLOptionElement(getNode()))
    return false;

  if (toHTMLOptionElement(getNode())->isDisabledFormControl())
    return false;

  HTMLSelectElement* selectElement = listBoxOptionParentNode();
  if (selectElement && selectElement->isDisabledFormControl())
    return false;

  return true;
}

String AXListBoxOption::textAlternative(bool recursive,
                                        bool inAriaLabelledByTraversal,
                                        AXObjectSet& visited,
                                        AXNameFrom& nameFrom,
                                        AXRelatedObjectVector* relatedObjects,
                                        NameSources* nameSources) const {
  // If nameSources is non-null, relatedObjects is used in filling it in, so it
  // must be non-null as well.
  if (nameSources)
    ASSERT(relatedObjects);

  if (!getNode())
    return String();

  bool foundTextAlternative = false;
  String textAlternative = ariaTextAlternative(
      recursive, inAriaLabelledByTraversal, visited, nameFrom, relatedObjects,
      nameSources, &foundTextAlternative);
  if (foundTextAlternative && !nameSources)
    return textAlternative;

  nameFrom = AXNameFromContents;
  textAlternative = toHTMLOptionElement(getNode())->displayLabel();
  if (nameSources) {
    nameSources->push_back(NameSource(foundTextAlternative));
    nameSources->back().type = nameFrom;
    nameSources->back().text = textAlternative;
    foundTextAlternative = true;
  }

  return textAlternative;
}

void AXListBoxOption::setSelected(bool selected) {
  HTMLSelectElement* selectElement = listBoxOptionParentNode();
  if (!selectElement)
    return;

  if (!canSetSelectedAttribute())
    return;

  bool isOptionSelected = isSelected();
  if ((isOptionSelected && selected) || (!isOptionSelected && !selected))
    return;

  selectElement->selectOptionByAccessKey(toHTMLOptionElement(getNode()));
}

HTMLSelectElement* AXListBoxOption::listBoxOptionParentNode() const {
  if (!getNode())
    return 0;

  if (isHTMLOptionElement(getNode()))
    return toHTMLOptionElement(getNode())->ownerSelectElement();

  return 0;
}

}  // namespace blink
