/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/forms/TextFieldInputType.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/events/BeforeTextInsertedEvent.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/TextEvent.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalFrame.h"
#include "core/html/FormData.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/html/shadow/TextControlInnerElements.h"
#include "core/layout/LayoutDetailsMarker.h"
#include "core/layout/LayoutTextControlSingleLine.h"
#include "core/layout/LayoutTheme.h"
#include "core/page/ChromeClient.h"
#include "core/paint/PaintLayer.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "wtf/text/WTFString.h"

namespace blink {

using namespace HTMLNames;

class DataListIndicatorElement final : public HTMLDivElement {
 private:
  inline DataListIndicatorElement(Document& document)
      : HTMLDivElement(document) {}
  inline HTMLInputElement* hostInput() const {
    return toHTMLInputElement(ownerShadowHost());
  }

  LayoutObject* createLayoutObject(const ComputedStyle&) override {
    return new LayoutDetailsMarker(this);
  }

  EventDispatchHandlingState* preDispatchEventHandler(Event* event) override {
    // Chromium opens autofill popup in a mousedown event listener
    // associated to the document. We don't want to open it in this case
    // because we opens a datalist chooser later.
    // FIXME: We should dispatch mousedown events even in such case.
    if (event->type() == EventTypeNames::mousedown)
      event->stopPropagation();
    return nullptr;
  }

  void defaultEventHandler(Event* event) override {
    DCHECK(document().isActive());
    if (event->type() != EventTypeNames::click)
      return;
    HTMLInputElement* host = hostInput();
    if (host && !host->isDisabledOrReadOnly()) {
      document().frameHost()->chromeClient().openTextDataListChooser(*host);
      event->setDefaultHandled();
    }
  }

  bool willRespondToMouseClickEvents() override {
    return hostInput() && !hostInput()->isDisabledOrReadOnly() &&
           document().isActive();
  }

 public:
  static DataListIndicatorElement* create(Document& document) {
    DataListIndicatorElement* element = new DataListIndicatorElement(document);
    element->setShadowPseudoId(
        AtomicString("-webkit-calendar-picker-indicator"));
    element->setAttribute(idAttr, ShadowElementNames::pickerIndicator());
    return element;
  }
};

TextFieldInputType::TextFieldInputType(HTMLInputElement& element)
    : InputType(element), InputTypeView(element) {}

TextFieldInputType::~TextFieldInputType() {}

DEFINE_TRACE(TextFieldInputType) {
  InputTypeView::trace(visitor);
  InputType::trace(visitor);
}

InputTypeView* TextFieldInputType::createView() {
  return this;
}

InputType::ValueMode TextFieldInputType::valueMode() const {
  return ValueMode::kValue;
}

SpinButtonElement* TextFieldInputType::spinButtonElement() const {
  return toSpinButtonElementOrDie(
      element().userAgentShadowRoot()->getElementById(
          ShadowElementNames::spinButton()));
}

bool TextFieldInputType::shouldShowFocusRingOnMouseFocus() const {
  return true;
}

bool TextFieldInputType::isTextField() const {
  return true;
}

bool TextFieldInputType::valueMissing(const String& value) const {
  return element().isRequired() && value.isEmpty();
}

bool TextFieldInputType::canSetSuggestedValue() {
  return true;
}

void TextFieldInputType::setValue(const String& sanitizedValue,
                                  bool valueChanged,
                                  TextFieldEventBehavior eventBehavior) {
  // We don't use InputType::setValue.  TextFieldInputType dispatches events
  // different way from InputType::setValue.
  element().setNonAttributeValue(sanitizedValue);

  if (valueChanged)
    element().updateView();

  unsigned max = visibleValue().length();
  element().setSelectionRange(max, max);

  if (!valueChanged)
    return;

  switch (eventBehavior) {
    case DispatchChangeEvent:
      // If the user is still editing this field, dispatch an input event rather
      // than a change event.  The change event will be dispatched when editing
      // finishes.
      if (element().isFocused())
        element().dispatchFormControlInputEvent();
      else
        element().dispatchFormControlChangeEvent();
      break;

    case DispatchInputAndChangeEvent: {
      element().dispatchFormControlInputEvent();
      element().dispatchFormControlChangeEvent();
      break;
    }

    case DispatchNoEvent:
      // We need to update textAsOfLastFormControlChangeEvent for |value| IDL
      // setter without focus because input-assist features use setValue("...",
      // DispatchChangeEvent) without setting focus.
      if (!element().isFocused())
        element().setTextAsOfLastFormControlChangeEvent(element().value());
      break;
  }
}

void TextFieldInputType::handleKeydownEvent(KeyboardEvent* event) {
  if (!element().isFocused())
    return;
  if (ChromeClient* chromeClient = this->chromeClient()) {
    chromeClient->handleKeyboardEventOnTextField(element(), *event);
    return;
  }
  event->setDefaultHandled();
}

void TextFieldInputType::handleKeydownEventForSpinButton(KeyboardEvent* event) {
  if (element().isDisabledOrReadOnly())
    return;
  const String& key = event->key();
  if (key == "ArrowUp")
    spinButtonStepUp();
  else if (key == "ArrowDown" && !event->altKey())
    spinButtonStepDown();
  else
    return;
  element().dispatchFormControlChangeEvent();
  event->setDefaultHandled();
}

void TextFieldInputType::forwardEvent(Event* event) {
  if (SpinButtonElement* spinButton = spinButtonElement()) {
    spinButton->forwardEvent(event);
    if (event->defaultHandled())
      return;
  }

  if (element().layoutObject() &&
      (event->isMouseEvent() || event->isDragEvent() ||
       event->hasInterface(EventNames::WheelEvent) ||
       event->type() == EventTypeNames::blur ||
       event->type() == EventTypeNames::focus)) {
    LayoutTextControlSingleLine* layoutTextControl =
        toLayoutTextControlSingleLine(element().layoutObject());
    if (event->type() == EventTypeNames::blur) {
      if (LayoutBox* innerEditorLayoutObject =
              element().innerEditorElement()->layoutBox()) {
        // FIXME: This class has no need to know about PaintLayer!
        if (PaintLayer* innerLayer = innerEditorLayoutObject->layer()) {
          if (PaintLayerScrollableArea* innerScrollableArea =
                  innerLayer->getScrollableArea()) {
            innerScrollableArea->setScrollOffset(ScrollOffset(0, 0),
                                                 ProgrammaticScroll);
          }
        }
      }

      layoutTextControl->capsLockStateMayHaveChanged();
    } else if (event->type() == EventTypeNames::focus) {
      layoutTextControl->capsLockStateMayHaveChanged();
    }

    element().forwardEvent(event);
  }
}

void TextFieldInputType::handleFocusEvent(Element* oldFocusedNode,
                                          WebFocusType focusType) {
  InputTypeView::handleFocusEvent(oldFocusedNode, focusType);
  element().beginEditing();
}

void TextFieldInputType::handleBlurEvent() {
  InputTypeView::handleBlurEvent();
  element().endEditing();
  if (SpinButtonElement* spinButton = spinButtonElement())
    spinButton->releaseCapture();
}

bool TextFieldInputType::shouldSubmitImplicitly(Event* event) {
  return (event->type() == EventTypeNames::textInput &&
          event->hasInterface(EventNames::TextEvent) &&
          toTextEvent(event)->data() == "\n") ||
         InputTypeView::shouldSubmitImplicitly(event);
}

LayoutObject* TextFieldInputType::createLayoutObject(
    const ComputedStyle&) const {
  return new LayoutTextControlSingleLine(&element());
}

bool TextFieldInputType::shouldHaveSpinButton() const {
  return LayoutTheme::theme().shouldHaveSpinButton(&element());
}

void TextFieldInputType::createShadowSubtree() {
  DCHECK(element().shadow());
  ShadowRoot* shadowRoot = element().userAgentShadowRoot();
  DCHECK(!shadowRoot->hasChildren());

  Document& document = element().document();
  bool shouldHaveSpinButton = this->shouldHaveSpinButton();
  bool shouldHaveDataListIndicator = element().hasValidDataListOptions();
  bool createsContainer =
      shouldHaveSpinButton || shouldHaveDataListIndicator || needsContainer();

  TextControlInnerEditorElement* innerEditor =
      TextControlInnerEditorElement::create(document);
  if (!createsContainer) {
    shadowRoot->appendChild(innerEditor);
    return;
  }

  TextControlInnerContainer* container =
      TextControlInnerContainer::create(document);
  container->setShadowPseudoId(
      AtomicString("-webkit-textfield-decoration-container"));
  shadowRoot->appendChild(container);

  EditingViewPortElement* editingViewPort =
      EditingViewPortElement::create(document);
  editingViewPort->appendChild(innerEditor);
  container->appendChild(editingViewPort);

  if (shouldHaveDataListIndicator)
    container->appendChild(DataListIndicatorElement::create(document));
  // FIXME: Because of a special handling for a spin button in
  // LayoutTextControlSingleLine, we need to put it to the last position. It's
  // inconsistent with multiple-fields date/time types.
  if (shouldHaveSpinButton)
    container->appendChild(SpinButtonElement::create(document, *this));

  // See listAttributeTargetChanged too.
}

Element* TextFieldInputType::containerElement() const {
  return element().userAgentShadowRoot()->getElementById(
      ShadowElementNames::textFieldContainer());
}

void TextFieldInputType::destroyShadowSubtree() {
  InputTypeView::destroyShadowSubtree();
  if (SpinButtonElement* spinButton = spinButtonElement())
    spinButton->removeSpinButtonOwner();
}

void TextFieldInputType::listAttributeTargetChanged() {
  if (ChromeClient* chromeClient = this->chromeClient())
    chromeClient->textFieldDataListChanged(element());
  Element* picker = element().userAgentShadowRoot()->getElementById(
      ShadowElementNames::pickerIndicator());
  bool didHavePickerIndicator = picker;
  bool willHavePickerIndicator = element().hasValidDataListOptions();
  if (didHavePickerIndicator == willHavePickerIndicator)
    return;
  EventDispatchForbiddenScope::AllowUserAgentEvents allowEvents;
  if (willHavePickerIndicator) {
    Document& document = element().document();
    if (Element* container = containerElement()) {
      container->insertBefore(DataListIndicatorElement::create(document),
                              spinButtonElement());
    } else {
      // FIXME: The following code is similar to createShadowSubtree(),
      // but they are different. We should simplify the code by making
      // containerElement mandatory.
      Element* rpContainer = TextControlInnerContainer::create(document);
      rpContainer->setShadowPseudoId(
          AtomicString("-webkit-textfield-decoration-container"));
      Element* innerEditor = element().innerEditorElement();
      innerEditor->parentNode()->replaceChild(rpContainer, innerEditor);
      Element* editingViewPort = EditingViewPortElement::create(document);
      editingViewPort->appendChild(innerEditor);
      rpContainer->appendChild(editingViewPort);
      rpContainer->appendChild(DataListIndicatorElement::create(document));
      if (element().document().focusedElement() == element())
        element().updateFocusAppearance(SelectionBehaviorOnFocus::Restore);
    }
  } else {
    picker->remove(ASSERT_NO_EXCEPTION);
  }
}

void TextFieldInputType::attributeChanged() {
  // FIXME: Updating on any attribute update should be unnecessary. We should
  // figure out what attributes affect.
  updateView();
}

void TextFieldInputType::disabledAttributeChanged() {
  if (SpinButtonElement* spinButton = spinButtonElement())
    spinButton->releaseCapture();
}

void TextFieldInputType::readonlyAttributeChanged() {
  if (SpinButtonElement* spinButton = spinButtonElement())
    spinButton->releaseCapture();
}

bool TextFieldInputType::supportsReadOnly() const {
  return true;
}

static bool isASCIILineBreak(UChar c) {
  return c == '\r' || c == '\n';
}

static String limitLength(const String& string, unsigned maxLength) {
  unsigned newLength = std::min(maxLength, string.length());
  if (newLength == string.length())
    return string;
  if (newLength > 0 && U16_IS_LEAD(string[newLength - 1]))
    --newLength;
  return string.left(newLength);
}

String TextFieldInputType::sanitizeValue(const String& proposedValue) const {
  return limitLength(proposedValue.removeCharacters(isASCIILineBreak),
                     std::numeric_limits<int>::max());
}

void TextFieldInputType::handleBeforeTextInsertedEvent(
    BeforeTextInsertedEvent* event) {
  // Make sure that the text to be inserted will not violate the maxLength.

  // We use HTMLInputElement::innerEditorValue() instead of
  // HTMLInputElement::value() because they can be mismatched by
  // sanitizeValue() in HTMLInputElement::subtreeHasChanged() in some cases.
  unsigned oldLength = element().innerEditorValue().length();

  // selectionLength represents the selection length of this text field to be
  // removed by this insertion.
  // If the text field has no focus, we don't need to take account of the
  // selection length. The selection is the source of text drag-and-drop in
  // that case, and nothing in the text field will be removed.
  unsigned selectionLength = 0;
  if (element().isFocused()) {
    // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
    // needs to be audited.  See http://crbug.com/590369 for more details.
    element().document().updateStyleAndLayoutIgnorePendingStylesheets();

    selectionLength =
        element().document().frame()->selection().selectedText().length();
  }
  DCHECK_GE(oldLength, selectionLength);

  // Selected characters will be removed by the next text event.
  unsigned baseLength = oldLength - selectionLength;
  unsigned maxLength;
  if (this->maxLength() < 0)
    maxLength = std::numeric_limits<int>::max();
  else
    maxLength = static_cast<unsigned>(this->maxLength());
  unsigned appendableLength =
      maxLength > baseLength ? maxLength - baseLength : 0;

  // Truncate the inserted text to avoid violating the maxLength and other
  // constraints.
  String eventText = event->text();
  unsigned textLength = eventText.length();
  while (textLength > 0 && isASCIILineBreak(eventText[textLength - 1]))
    textLength--;
  eventText.truncate(textLength);
  eventText.replace("\r\n", " ");
  eventText.replace('\r', ' ');
  eventText.replace('\n', ' ');

  event->setText(limitLength(eventText, appendableLength));
}

bool TextFieldInputType::shouldRespectListAttribute() {
  return true;
}

void TextFieldInputType::updatePlaceholderText() {
  if (!supportsPlaceholder())
    return;
  HTMLElement* placeholder = element().placeholderElement();
  String placeholderText = element().strippedPlaceholder();
  if (placeholderText.isEmpty()) {
    if (placeholder)
      placeholder->remove(ASSERT_NO_EXCEPTION);
    return;
  }
  if (!placeholder) {
    HTMLElement* newElement = HTMLDivElement::create(element().document());
    placeholder = newElement;
    placeholder->setShadowPseudoId(AtomicString("-webkit-input-placeholder"));
    placeholder->setInlineStyleProperty(
        CSSPropertyDisplay,
        element().isPlaceholderVisible() ? CSSValueBlock : CSSValueNone, true);
    placeholder->setAttribute(idAttr, ShadowElementNames::placeholder());
    Element* container = containerElement();
    Node* previous = container ? container : element().innerEditorElement();
    previous->parentNode()->insertBefore(placeholder, previous);
    SECURITY_DCHECK(placeholder->parentNode() == previous->parentNode());
  }
  placeholder->setTextContent(placeholderText);
}

void TextFieldInputType::appendToFormData(FormData& formData) const {
  InputType::appendToFormData(formData);
  const AtomicString& dirnameAttrValue =
      element().fastGetAttribute(dirnameAttr);
  if (!dirnameAttrValue.isNull())
    formData.append(dirnameAttrValue, element().directionForFormData());
}

String TextFieldInputType::convertFromVisibleValue(
    const String& visibleValue) const {
  return visibleValue;
}

void TextFieldInputType::subtreeHasChanged() {
  bool wasChanged = element().wasChangedSinceLastFormControlChangeEvent();
  element().setChangedSinceLastFormControlChangeEvent(true);

  element().setValueFromRenderer(sanitizeUserInputValue(
      convertFromVisibleValue(element().innerEditorValue())));
  element().updatePlaceholderVisibility();
  element().pseudoStateChanged(CSSSelector::PseudoValid);
  element().pseudoStateChanged(CSSSelector::PseudoInvalid);
  element().pseudoStateChanged(CSSSelector::PseudoInRange);
  element().pseudoStateChanged(CSSSelector::PseudoOutOfRange);

  didSetValueByUserEdit(wasChanged ? ValueChangeStateChanged
                                   : ValueChangeStateNone);
}

void TextFieldInputType::didSetValueByUserEdit(ValueChangeState state) {
  if (!element().isFocused())
    return;
  if (ChromeClient* chromeClient = this->chromeClient())
    chromeClient->didChangeValueInTextField(element());
}

void TextFieldInputType::spinButtonStepDown() {
  stepUpFromLayoutObject(-1);
}

void TextFieldInputType::spinButtonStepUp() {
  stepUpFromLayoutObject(1);
}

void TextFieldInputType::updateView() {
  if (!element().suggestedValue().isNull()) {
    element().setInnerEditorValue(element().suggestedValue());
    element().updatePlaceholderVisibility();
  } else if (element().needsToUpdateViewValue()) {
    // Update the view only if needsToUpdateViewValue is true. It protects
    // an unacceptable view value from being overwritten with the DOM value.
    //
    // e.g. <input type=number> has a view value "abc", and input.max is
    // updated. In this case, updateView() is called but we should not
    // update the view value.
    element().setInnerEditorValue(visibleValue());
    element().updatePlaceholderVisibility();
  }
}

void TextFieldInputType::focusAndSelectSpinButtonOwner() {
  element().focus();
  element().setSelectionRange(0, std::numeric_limits<int>::max());
}

bool TextFieldInputType::shouldSpinButtonRespondToMouseEvents() {
  return !element().isDisabledOrReadOnly();
}

bool TextFieldInputType::shouldSpinButtonRespondToWheelEvents() {
  return shouldSpinButtonRespondToMouseEvents() && element().isFocused();
}

void TextFieldInputType::spinButtonDidReleaseMouseCapture(
    SpinButtonElement::EventDispatch eventDispatch) {
  if (eventDispatch == SpinButtonElement::EventDispatchAllowed)
    element().dispatchFormControlChangeEvent();
}

}  // namespace blink
