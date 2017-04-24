/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#include "core/html/HTMLFormControlElement.h"

#include "core/dom/ElementTraversal.h"
#include "core/events/Event.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLDataListElement.h"
#include "core/html/HTMLFieldSetElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLLegendElement.h"
#include "core/html/ValidityState.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTheme.h"
#include "core/page/Page.h"
#include "core/page/ValidationMessageClient.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/text/BidiTextRun.h"
#include "wtf/Vector.h"

namespace blink {

using namespace HTMLNames;

HTMLFormControlElement::HTMLFormControlElement(const QualifiedName& tagName,
                                               Document& document)
    : LabelableElement(tagName, document),
      m_ancestorDisabledState(AncestorDisabledStateUnknown),
      m_dataListAncestorState(Unknown),
      m_isAutofilled(false),
      m_hasValidationMessage(false),
      m_willValidateInitialized(false),
      m_willValidate(true),
      m_isValid(true),
      m_validityIsDirty(false),
      m_wasFocusedByMouse(false),
      m_blocksFormSubmission(false) {
  setHasCustomStyleCallbacks();
}

HTMLFormControlElement::~HTMLFormControlElement() {}

DEFINE_TRACE(HTMLFormControlElement) {
  ListedElement::trace(visitor);
  LabelableElement::trace(visitor);
}

String HTMLFormControlElement::formAction() const {
  const AtomicString& action = fastGetAttribute(formactionAttr);
  if (action.isEmpty())
    return document().url();
  return document().completeURL(stripLeadingAndTrailingHTMLSpaces(action));
}

void HTMLFormControlElement::setFormAction(const AtomicString& value) {
  setAttribute(formactionAttr, value);
}

String HTMLFormControlElement::formEnctype() const {
  const AtomicString& formEnctypeAttr = fastGetAttribute(formenctypeAttr);
  if (formEnctypeAttr.isNull())
    return emptyString;
  return FormSubmission::Attributes::parseEncodingType(formEnctypeAttr);
}

void HTMLFormControlElement::setFormEnctype(const AtomicString& value) {
  setAttribute(formenctypeAttr, value);
}

String HTMLFormControlElement::formMethod() const {
  const AtomicString& formMethodAttr = fastGetAttribute(formmethodAttr);
  if (formMethodAttr.isNull())
    return emptyString;
  return FormSubmission::Attributes::methodString(
      FormSubmission::Attributes::parseMethodType(formMethodAttr));
}

void HTMLFormControlElement::setFormMethod(const AtomicString& value) {
  setAttribute(formmethodAttr, value);
}

bool HTMLFormControlElement::formNoValidate() const {
  return fastHasAttribute(formnovalidateAttr);
}

void HTMLFormControlElement::updateAncestorDisabledState() const {
  HTMLFieldSetElement* highestDisabledFieldSetAncestor = nullptr;
  ContainerNode* highestLegendAncestor = nullptr;
  for (HTMLElement* ancestor = Traversal<HTMLElement>::firstAncestor(*this);
       ancestor; ancestor = Traversal<HTMLElement>::firstAncestor(*ancestor)) {
    if (isHTMLLegendElement(*ancestor))
      highestLegendAncestor = ancestor;
    if (isHTMLFieldSetElement(*ancestor) && ancestor->isDisabledFormControl())
      highestDisabledFieldSetAncestor = toHTMLFieldSetElement(ancestor);
  }
  m_ancestorDisabledState =
      (highestDisabledFieldSetAncestor &&
       !(highestLegendAncestor &&
         highestLegendAncestor == highestDisabledFieldSetAncestor->legend()))
          ? AncestorDisabledStateDisabled
          : AncestorDisabledStateEnabled;
}

void HTMLFormControlElement::ancestorDisabledStateWasChanged() {
  m_ancestorDisabledState = AncestorDisabledStateUnknown;
  disabledAttributeChanged();
}

void HTMLFormControlElement::reset() {
  setAutofilled(false);
  resetImpl();
}

void HTMLFormControlElement::attributeChanged(
    const AttributeModificationParams& params) {
  HTMLElement::attributeChanged(params);
  if (params.name == disabledAttr &&
      params.oldValue.isNull() != params.newValue.isNull()) {
    disabledAttributeChanged();
    if (params.reason == AttributeModificationReason::kDirectly &&
        isDisabledFormControl() && adjustedFocusedElementInTreeScope() == this)
      blur();
  }
}

void HTMLFormControlElement::parseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  if (name == formAttr) {
    formAttributeChanged();
    UseCounter::count(document(), UseCounter::FormAttribute);
  } else if (name == readonlyAttr) {
    if (params.oldValue.isNull() != params.newValue.isNull()) {
      setNeedsWillValidateCheck();
      pseudoStateChanged(CSSSelector::PseudoReadOnly);
      pseudoStateChanged(CSSSelector::PseudoReadWrite);
      if (layoutObject())
        LayoutTheme::theme().controlStateChanged(*layoutObject(),
                                                 ReadOnlyControlState);
    }
  } else if (name == requiredAttr) {
    if (params.oldValue.isNull() != params.newValue.isNull())
      requiredAttributeChanged();
    UseCounter::count(document(), UseCounter::RequiredAttribute);
  } else if (name == autofocusAttr) {
    HTMLElement::parseAttribute(params);
    UseCounter::count(document(), UseCounter::AutoFocusAttribute);
  } else {
    HTMLElement::parseAttribute(params);
  }
}

void HTMLFormControlElement::disabledAttributeChanged() {
  // Don't blur in this function because this is called for descendants of
  // <fieldset> while tree traversal.
  EventDispatchForbiddenScope eventForbidden;

  setNeedsWillValidateCheck();
  pseudoStateChanged(CSSSelector::PseudoDisabled);
  pseudoStateChanged(CSSSelector::PseudoEnabled);
  if (layoutObject())
    LayoutTheme::theme().controlStateChanged(*layoutObject(),
                                             EnabledControlState);
}

void HTMLFormControlElement::requiredAttributeChanged() {
  setNeedsValidityCheck();
  pseudoStateChanged(CSSSelector::PseudoRequired);
  pseudoStateChanged(CSSSelector::PseudoOptional);
}

bool HTMLFormControlElement::isReadOnly() const {
  return fastHasAttribute(HTMLNames::readonlyAttr);
}

bool HTMLFormControlElement::isDisabledOrReadOnly() const {
  return isDisabledFormControl() || isReadOnly();
}

bool HTMLFormControlElement::supportsAutofocus() const {
  return false;
}

bool HTMLFormControlElement::isAutofocusable() const {
  return fastHasAttribute(autofocusAttr) && supportsAutofocus();
}

void HTMLFormControlElement::setAutofilled(bool autofilled) {
  if (autofilled == m_isAutofilled)
    return;

  m_isAutofilled = autofilled;
  pseudoStateChanged(CSSSelector::PseudoAutofill);
}

static bool shouldAutofocusOnAttach(const HTMLFormControlElement* element) {
  if (!element->isAutofocusable())
    return false;
  if (element->document().isSandboxed(SandboxAutomaticFeatures)) {
    // FIXME: This message should be moved off the console once a solution to
    // https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
    element->document().addConsoleMessage(ConsoleMessage::create(
        SecurityMessageSource, ErrorMessageLevel,
        "Blocked autofocusing on a form control because the form's frame is "
        "sandboxed and the 'allow-scripts' permission is not set."));
    return false;
  }

  return true;
}

void HTMLFormControlElement::attachLayoutTree(const AttachContext& context) {
  HTMLElement::attachLayoutTree(context);

  if (!layoutObject())
    return;

  // The call to updateFromElement() needs to go after the call through
  // to the base class's attachLayoutTree() because that can sometimes do a
  // close on the layoutObject.
  layoutObject()->updateFromElement();

  // FIXME: Autofocus handling should be moved to insertedInto according to
  // the standard.
  if (shouldAutofocusOnAttach(this))
    document().setAutofocusElement(this);
}

void HTMLFormControlElement::didMoveToNewDocument(Document& oldDocument) {
  ListedElement::didMoveToNewDocument(oldDocument);
  HTMLElement::didMoveToNewDocument(oldDocument);
}

Node::InsertionNotificationRequest HTMLFormControlElement::insertedInto(
    ContainerNode* insertionPoint) {
  m_ancestorDisabledState = AncestorDisabledStateUnknown;
  m_dataListAncestorState = Unknown;
  setNeedsWillValidateCheck();
  HTMLElement::insertedInto(insertionPoint);
  ListedElement::insertedInto(insertionPoint);
  fieldSetAncestorsSetNeedsValidityCheck(insertionPoint);

  // Trigger for elements outside of forms.
  if (!formOwner() && insertionPoint->isConnected())
    document().didAssociateFormControl(this);

  return InsertionDone;
}

void HTMLFormControlElement::removedFrom(ContainerNode* insertionPoint) {
  fieldSetAncestorsSetNeedsValidityCheck(insertionPoint);
  hideVisibleValidationMessage();
  m_hasValidationMessage = false;
  m_ancestorDisabledState = AncestorDisabledStateUnknown;
  m_dataListAncestorState = Unknown;
  setNeedsWillValidateCheck();
  HTMLElement::removedFrom(insertionPoint);
  ListedElement::removedFrom(insertionPoint);
}

void HTMLFormControlElement::willChangeForm() {
  ListedElement::willChangeForm();
  formOwnerSetNeedsValidityCheck();
  if (formOwner() && canBeSuccessfulSubmitButton())
    formOwner()->invalidateDefaultButtonStyle();
}

void HTMLFormControlElement::didChangeForm() {
  ListedElement::didChangeForm();
  formOwnerSetNeedsValidityCheck();
  if (formOwner() && isConnected() && canBeSuccessfulSubmitButton())
    formOwner()->invalidateDefaultButtonStyle();
}

void HTMLFormControlElement::formOwnerSetNeedsValidityCheck() {
  if (HTMLFormElement* form = formOwner()) {
    form->pseudoStateChanged(CSSSelector::PseudoValid);
    form->pseudoStateChanged(CSSSelector::PseudoInvalid);
  }
}

void HTMLFormControlElement::fieldSetAncestorsSetNeedsValidityCheck(
    Node* node) {
  if (!node)
    return;
  for (HTMLFieldSetElement* fieldSet =
           Traversal<HTMLFieldSetElement>::firstAncestorOrSelf(*node);
       fieldSet;
       fieldSet = Traversal<HTMLFieldSetElement>::firstAncestor(*fieldSet)) {
    fieldSet->pseudoStateChanged(CSSSelector::PseudoValid);
    fieldSet->pseudoStateChanged(CSSSelector::PseudoInvalid);
  }
}

void HTMLFormControlElement::dispatchChangeEvent() {
  dispatchScopedEvent(Event::createBubble(EventTypeNames::change));
}

HTMLFormElement* HTMLFormControlElement::formOwner() const {
  return ListedElement::form();
}

bool HTMLFormControlElement::isDisabledFormControl() const {
  if (fastHasAttribute(disabledAttr))
    return true;

  if (m_ancestorDisabledState == AncestorDisabledStateUnknown)
    updateAncestorDisabledState();
  return m_ancestorDisabledState == AncestorDisabledStateDisabled;
}

bool HTMLFormControlElement::matchesEnabledPseudoClass() const {
  return !isDisabledFormControl();
}

bool HTMLFormControlElement::isRequired() const {
  return fastHasAttribute(requiredAttr);
}

String HTMLFormControlElement::resultForDialogSubmit() {
  return fastGetAttribute(valueAttr);
}

void HTMLFormControlElement::didRecalcStyle() {
  if (LayoutObject* layoutObject = this->layoutObject())
    layoutObject->updateFromElement();
}

bool HTMLFormControlElement::supportsFocus() const {
  return !isDisabledFormControl();
}

bool HTMLFormControlElement::isKeyboardFocusable() const {
  // Skip tabIndex check in a parent class.
  return isFocusable();
}

bool HTMLFormControlElement::shouldShowFocusRingOnMouseFocus() const {
  return false;
}

bool HTMLFormControlElement::shouldHaveFocusAppearance() const {
  return !m_wasFocusedByMouse || shouldShowFocusRingOnMouseFocus();
}

void HTMLFormControlElement::dispatchFocusEvent(
    Element* oldFocusedElement,
    WebFocusType type,
    InputDeviceCapabilities* sourceCapabilities) {
  if (type != WebFocusTypePage)
    m_wasFocusedByMouse = type == WebFocusTypeMouse;
  // ContainerNode::handleStyleChangeOnFocusStateChange() will inform
  // LayoutTheme about the focus state change.
  HTMLElement::dispatchFocusEvent(oldFocusedElement, type, sourceCapabilities);
}

void HTMLFormControlElement::willCallDefaultEventHandler(const Event& event) {
  if (!m_wasFocusedByMouse)
    return;
  if (!event.isKeyboardEvent() || event.type() != EventTypeNames::keydown)
    return;

  bool oldShouldHaveFocusAppearance = shouldHaveFocusAppearance();
  m_wasFocusedByMouse = false;

  // Change of m_wasFocusByMouse may affect shouldHaveFocusAppearance() and
  // LayoutTheme::isFocused().  Inform LayoutTheme if
  // shouldHaveFocusAppearance() changes.
  if (oldShouldHaveFocusAppearance != shouldHaveFocusAppearance() &&
      layoutObject())
    LayoutTheme::theme().controlStateChanged(*layoutObject(),
                                             FocusControlState);
}

int HTMLFormControlElement::tabIndex() const {
  // Skip the supportsFocus check in HTMLElement.
  return Element::tabIndex();
}

bool HTMLFormControlElement::recalcWillValidate() const {
  if (m_dataListAncestorState == Unknown) {
    if (Traversal<HTMLDataListElement>::firstAncestor(*this))
      m_dataListAncestorState = InsideDataList;
    else
      m_dataListAncestorState = NotInsideDataList;
  }
  return m_dataListAncestorState == NotInsideDataList &&
         !isDisabledOrReadOnly();
}

bool HTMLFormControlElement::willValidate() const {
  if (!m_willValidateInitialized || m_dataListAncestorState == Unknown) {
    const_cast<HTMLFormControlElement*>(this)->setNeedsWillValidateCheck();
  } else {
    // If the following assertion fails, setNeedsWillValidateCheck() is not
    // called correctly when something which changes recalcWillValidate() result
    // is updated.
    DCHECK_EQ(m_willValidate, recalcWillValidate());
  }
  return m_willValidate;
}

void HTMLFormControlElement::setNeedsWillValidateCheck() {
  // We need to recalculate willValidate immediately because willValidate change
  // can causes style change.
  bool newWillValidate = recalcWillValidate();
  if (m_willValidateInitialized && m_willValidate == newWillValidate)
    return;
  m_willValidateInitialized = true;
  m_willValidate = newWillValidate;
  // Needs to force setNeedsValidityCheck() to invalidate validity state of
  // FORM/FIELDSET. If this element updates willValidate twice and
  // isValidElement() is not called between them, the second call of this
  // function still has m_validityIsDirty==true, which means
  // setNeedsValidityCheck() doesn't invalidate validity state of
  // FORM/FIELDSET.
  m_validityIsDirty = false;
  setNeedsValidityCheck();
  // No need to trigger style recalculation here because
  // setNeedsValidityCheck() does it in the right away. This relies on
  // the assumption that valid() is always true if willValidate() is false.

  if (!m_willValidate)
    hideVisibleValidationMessage();
}

void HTMLFormControlElement::findCustomValidationMessageTextDirection(
    const String& message,
    TextDirection& messageDir,
    String& subMessage,
    TextDirection& subMessageDir) {
  messageDir = determineDirectionality(message);
  if (!subMessage.isEmpty())
    subMessageDir = layoutObject()->style()->direction();
}

void HTMLFormControlElement::updateVisibleValidationMessage() {
  Page* page = document().page();
  if (!page || !page->isPageVisible() || document().unloadStarted())
    return;
  String message;
  if (layoutObject() && willValidate())
    message = validationMessage().stripWhiteSpace();

  m_hasValidationMessage = true;
  ValidationMessageClient* client = &page->validationMessageClient();
  TextDirection messageDir = TextDirection::kLtr;
  TextDirection subMessageDir = TextDirection::kLtr;
  String subMessage = validationSubMessage().stripWhiteSpace();
  if (message.isEmpty())
    client->hideValidationMessage(*this);
  else
    findCustomValidationMessageTextDirection(message, messageDir, subMessage,
                                             subMessageDir);
  client->showValidationMessage(*this, message, messageDir, subMessage,
                                subMessageDir);
}

void HTMLFormControlElement::hideVisibleValidationMessage() {
  if (!m_hasValidationMessage)
    return;

  if (ValidationMessageClient* client = validationMessageClient())
    client->hideValidationMessage(*this);
}

bool HTMLFormControlElement::isValidationMessageVisible() const {
  if (!m_hasValidationMessage)
    return false;

  ValidationMessageClient* client = validationMessageClient();
  if (!client)
    return false;

  return client->isValidationMessageVisible(*this);
}

ValidationMessageClient* HTMLFormControlElement::validationMessageClient()
    const {
  Page* page = document().page();
  if (!page)
    return nullptr;

  return &page->validationMessageClient();
}

bool HTMLFormControlElement::checkValidity(
    HeapVector<Member<HTMLFormControlElement>>* unhandledInvalidControls,
    CheckValidityEventBehavior eventBehavior) {
  if (!willValidate())
    return true;
  if (isValidElement())
    return true;
  if (eventBehavior != CheckValidityDispatchInvalidEvent)
    return false;
  Document* originalDocument = &document();
  DispatchEventResult dispatchResult =
      dispatchEvent(Event::createCancelable(EventTypeNames::invalid));
  if (dispatchResult == DispatchEventResult::NotCanceled &&
      unhandledInvalidControls && isConnected() &&
      originalDocument == document())
    unhandledInvalidControls->push_back(this);
  return false;
}

void HTMLFormControlElement::showValidationMessage() {
  scrollIntoViewIfNeeded(false);
  focus();
  updateVisibleValidationMessage();
}

bool HTMLFormControlElement::reportValidity() {
  HeapVector<Member<HTMLFormControlElement>> unhandledInvalidControls;
  bool isValid = checkValidity(&unhandledInvalidControls,
                               CheckValidityDispatchInvalidEvent);
  if (isValid || unhandledInvalidControls.isEmpty())
    return isValid;
  DCHECK_EQ(unhandledInvalidControls.size(), 1u);
  DCHECK_EQ(unhandledInvalidControls[0].get(), this);
  // Update layout now before calling isFocusable(), which has
  // !layoutObject()->needsLayout() assertion.
  document().updateStyleAndLayoutIgnorePendingStylesheets();
  if (isFocusable()) {
    showValidationMessage();
    return false;
  }
  if (document().frame()) {
    String message(
        "An invalid form control with name='%name' is not focusable.");
    message.replace("%name", name());
    document().addConsoleMessage(ConsoleMessage::create(
        RenderingMessageSource, ErrorMessageLevel, message));
  }
  return false;
}

bool HTMLFormControlElement::matchesValidityPseudoClasses() const {
  return willValidate();
}

bool HTMLFormControlElement::isValidElement() {
  if (m_validityIsDirty) {
    m_isValid = !willValidate() || valid();
    m_validityIsDirty = false;
  } else {
    // If the following assertion fails, setNeedsValidityCheck() is not
    // called correctly when something which changes validity is updated.
    DCHECK_EQ(m_isValid, (!willValidate() || valid()));
  }
  return m_isValid;
}

void HTMLFormControlElement::setNeedsValidityCheck() {
  if (!m_validityIsDirty) {
    m_validityIsDirty = true;
    formOwnerSetNeedsValidityCheck();
    fieldSetAncestorsSetNeedsValidityCheck(parentNode());
    pseudoStateChanged(CSSSelector::PseudoValid);
    pseudoStateChanged(CSSSelector::PseudoInvalid);
  }

  pseudoStateChanged(CSSSelector::PseudoInRange);
  pseudoStateChanged(CSSSelector::PseudoOutOfRange);

  // Updates only if this control already has a validation message.
  if (isValidationMessageVisible()) {
    // Calls updateVisibleValidationMessage() even if m_isValid is not
    // changed because a validation message can be changed.
    updateVisibleValidationMessage();
  }
}

void HTMLFormControlElement::setCustomValidity(const String& error) {
  ListedElement::setCustomValidity(error);
  setNeedsValidityCheck();
}

void HTMLFormControlElement::dispatchBlurEvent(
    Element* newFocusedElement,
    WebFocusType type,
    InputDeviceCapabilities* sourceCapabilities) {
  if (type != WebFocusTypePage)
    m_wasFocusedByMouse = false;
  HTMLElement::dispatchBlurEvent(newFocusedElement, type, sourceCapabilities);
  hideVisibleValidationMessage();
}

bool HTMLFormControlElement::isSuccessfulSubmitButton() const {
  return canBeSuccessfulSubmitButton() && !isDisabledFormControl();
}

HTMLFormControlElement* HTMLFormControlElement::enclosingFormControlElement(
    Node* node) {
  if (!node)
    return nullptr;
  return Traversal<HTMLFormControlElement>::firstAncestorOrSelf(*node);
}

String HTMLFormControlElement::nameForAutofill() const {
  String fullName = name();
  String trimmedName = fullName.stripWhiteSpace();
  if (!trimmedName.isEmpty())
    return trimmedName;
  fullName = getIdAttribute();
  trimmedName = fullName.stripWhiteSpace();
  return trimmedName;
}

void HTMLFormControlElement::copyNonAttributePropertiesFromElement(
    const Element& source) {
  HTMLElement::copyNonAttributePropertiesFromElement(source);
  setNeedsValidityCheck();
}

void HTMLFormControlElement::associateWith(HTMLFormElement* form) {
  associateByParser(form);
};

}  // namespace blink
