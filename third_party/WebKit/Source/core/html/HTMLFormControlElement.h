/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
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

#ifndef HTMLFormControlElement_h
#define HTMLFormControlElement_h

#include "core/CoreExport.h"
#include "core/html/FormAssociated.h"
#include "core/html/LabelableElement.h"
#include "core/html/ListedElement.h"

namespace blink {

class HTMLFormElement;
class ValidationMessageClient;

enum CheckValidityEventBehavior {
  CheckValidityDispatchNoEvent,
  CheckValidityDispatchInvalidEvent
};

// HTMLFormControlElement is the default implementation of
// ListedElement, and listed element implementations should use
// HTMLFormControlElement unless there is a special reason.
class CORE_EXPORT HTMLFormControlElement : public LabelableElement,
                                           public ListedElement,
                                           public FormAssociated {
  USING_GARBAGE_COLLECTED_MIXIN(HTMLFormControlElement);

 public:
  ~HTMLFormControlElement() override;
  DECLARE_VIRTUAL_TRACE();

  String formAction() const;
  void setFormAction(const AtomicString&);
  String formEnctype() const;
  void setFormEnctype(const AtomicString&);
  String formMethod() const;
  void setFormMethod(const AtomicString&);
  bool formNoValidate() const;

  void ancestorDisabledStateWasChanged();

  void reset();

  void dispatchChangeEvent();

  HTMLFormElement* formOwner() const final;

  bool isDisabledFormControl() const override;

  bool matchesEnabledPseudoClass() const override;

  bool isEnumeratable() const override { return false; }

  bool isRequired() const;

  const AtomicString& type() const { return formControlType(); }

  virtual const AtomicString& formControlType() const = 0;

  virtual bool canTriggerImplicitSubmission() const { return false; }

  virtual bool isSubmittableElement() { return true; }

  virtual String resultForDialogSubmit();

  // Return true if this control type can be a submit button.  This doesn't
  // check |disabled|, and this doesn't check if this is the first submit
  // button.
  virtual bool canBeSuccessfulSubmitButton() const { return false; }
  // Return true if this control can submit a form.
  // i.e. canBeSuccessfulSubmitButton() && !isDisabledFormControl().
  bool isSuccessfulSubmitButton() const;
  virtual bool isActivatedSubmit() const { return false; }
  virtual void setActivatedSubmit(bool) {}

  bool willValidate() const override;

  void updateVisibleValidationMessage();
  void hideVisibleValidationMessage();
  bool checkValidity(
      HeapVector<Member<HTMLFormControlElement>>* unhandledInvalidControls = 0,
      CheckValidityEventBehavior = CheckValidityDispatchInvalidEvent);
  bool reportValidity();
  // This must be called only after the caller check the element is focusable.
  void showValidationMessage();
  // This must be called when a validation constraint or control value is
  // changed.
  void setNeedsValidityCheck();
  void setCustomValidity(const String&) final;
  void findCustomValidationMessageTextDirection(const String& message,
                                                TextDirection& messageDir,
                                                String& subMessage,
                                                TextDirection& subMessageDir);

  bool isReadOnly() const;
  bool isDisabledOrReadOnly() const;

  bool isAutofocusable() const;

  bool isAutofilled() const { return m_isAutofilled; }
  void setAutofilled(bool = true);

  static HTMLFormControlElement* enclosingFormControlElement(Node*);

  String nameForAutofill() const;

  void copyNonAttributePropertiesFromElement(const Element&) override;

  FormAssociated* toFormAssociatedOrNull() override { return this; };
  void associateWith(HTMLFormElement*) override;

  bool blocksFormSubmission() const { return m_blocksFormSubmission; }
  void setBlocksFormSubmission(bool value) { m_blocksFormSubmission = value; }

 protected:
  HTMLFormControlElement(const QualifiedName& tagName, Document&);

  void attributeChanged(const AttributeModificationParams&) override;
  void parseAttribute(const AttributeModificationParams&) override;
  virtual void requiredAttributeChanged();
  virtual void disabledAttributeChanged();
  void attachLayoutTree(const AttachContext& = AttachContext()) override;
  InsertionNotificationRequest insertedInto(ContainerNode*) override;
  void removedFrom(ContainerNode*) override;
  void willChangeForm() override;
  void didChangeForm() override;
  void didMoveToNewDocument(Document& oldDocument) override;

  bool supportsFocus() const override;
  bool isKeyboardFocusable() const override;
  virtual bool shouldShowFocusRingOnMouseFocus() const;
  bool shouldHaveFocusAppearance() const final;
  void dispatchBlurEvent(Element* newFocusedElement,
                         WebFocusType,
                         InputDeviceCapabilities* sourceCapabilities) override;
  void dispatchFocusEvent(Element* oldFocusedElement,
                          WebFocusType,
                          InputDeviceCapabilities* sourceCapabilities) override;
  void willCallDefaultEventHandler(const Event&) final;

  void didRecalcStyle(StyleRecalcChange) override;

  // This must be called any time the result of willValidate() has changed.
  void setNeedsWillValidateCheck();
  virtual bool recalcWillValidate() const;

  virtual void resetImpl() {}
  virtual bool supportsAutofocus() const;

 private:
  bool isFormControlElement() const final { return true; }
  bool alwaysCreateUserAgentShadowRoot() const override { return true; }

  int tabIndex() const;

  bool isValidElement() override;
  bool matchesValidityPseudoClasses() const override;
  void updateAncestorDisabledState() const;

  bool isValidationMessageVisible() const;
  ValidationMessageClient* validationMessageClient() const;

  // Requests validity recalc for the form owner, if one exists.
  void formOwnerSetNeedsValidityCheck();
  // Requests validity recalc for all ancestor fieldsets, if exist.
  void fieldSetAncestorsSetNeedsValidityCheck(Node*);

  enum AncestorDisabledState {
    AncestorDisabledStateUnknown,
    AncestorDisabledStateEnabled,
    AncestorDisabledStateDisabled
  };
  mutable AncestorDisabledState m_ancestorDisabledState;
  enum DataListAncestorState { Unknown, InsideDataList, NotInsideDataList };
  mutable enum DataListAncestorState m_dataListAncestorState;

  bool m_isAutofilled : 1;
  bool m_hasValidationMessage : 1;
  // The initial value of m_willValidate depends on the derived class. We can't
  // initialize it with a virtual function in the constructor. m_willValidate
  // is not deterministic as long as m_willValidateInitialized is false.
  mutable bool m_willValidateInitialized : 1;
  mutable bool m_willValidate : 1;

  // Cache of valid().
  bool m_isValid : 1;
  bool m_validityIsDirty : 1;

  bool m_wasFocusedByMouse : 1;
  bool m_blocksFormSubmission : 1;
};

inline bool isHTMLFormControlElement(const Element& element) {
  return element.isFormControlElement();
}

DEFINE_HTMLELEMENT_TYPE_CASTS_WITH_FUNCTION(HTMLFormControlElement);
DEFINE_TYPE_CASTS(HTMLFormControlElement,
                  ListedElement,
                  control,
                  control->isFormControlElement(),
                  control.isFormControlElement());

}  // namespace blink

#endif
