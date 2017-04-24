/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010 Apple Inc. All rights
 * reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
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

#include "core/html/HTMLTextAreaElement.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/CSSValueKeywords.h"
#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/events/BeforeTextInsertedEvent.h"
#include "core/events/Event.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/html/FormData.h"
#include "core/html/forms/FormController.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/html/shadow/TextControlInnerElements.h"
#include "core/layout/LayoutTextControlMultiLine.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "platform/text/PlatformLocale.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

using namespace HTMLNames;

static const unsigned defaultRows = 2;
static const unsigned defaultCols = 20;

static inline unsigned computeLengthForAPIValue(const String& text) {
  unsigned length = text.length();
  unsigned crlfCount = 0;
  for (unsigned i = 0; i < length; ++i) {
    if (text[i] == '\r' && i + 1 < length && text[i + 1] == '\n')
      crlfCount++;
  }
  return text.length() - crlfCount;
}

HTMLTextAreaElement::HTMLTextAreaElement(Document& document)
    : TextControlElement(textareaTag, document),
      m_rows(defaultRows),
      m_cols(defaultCols),
      m_wrap(SoftWrap),
      m_isDirty(false),
      m_valueIsUpToDate(true),
      m_isPlaceholderVisible(false) {}

HTMLTextAreaElement* HTMLTextAreaElement::create(Document& document) {
  HTMLTextAreaElement* textArea = new HTMLTextAreaElement(document);
  textArea->ensureUserAgentShadowRoot();
  return textArea;
}

void HTMLTextAreaElement::didAddUserAgentShadowRoot(ShadowRoot& root) {
  root.appendChild(TextControlInnerEditorElement::create(document()));
}

const AtomicString& HTMLTextAreaElement::formControlType() const {
  DEFINE_STATIC_LOCAL(const AtomicString, textarea, ("textarea"));
  return textarea;
}

FormControlState HTMLTextAreaElement::saveFormControlState() const {
  return m_isDirty ? FormControlState(value()) : FormControlState();
}

void HTMLTextAreaElement::restoreFormControlState(
    const FormControlState& state) {
  setValue(state[0]);
}

void HTMLTextAreaElement::childrenChanged(const ChildrenChange& change) {
  HTMLElement::childrenChanged(change);
  setLastChangeWasNotUserEdit();
  if (m_isDirty)
    setInnerEditorValue(value());
  else
    setNonDirtyValue(defaultValue());
}

bool HTMLTextAreaElement::isPresentationAttribute(
    const QualifiedName& name) const {
  if (name == alignAttr) {
    // Don't map 'align' attribute.  This matches what Firefox, Opera and IE do.
    // See http://bugs.webkit.org/show_bug.cgi?id=7075
    return false;
  }

  if (name == wrapAttr)
    return true;
  return TextControlElement::isPresentationAttribute(name);
}

void HTMLTextAreaElement::collectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableStylePropertySet* style) {
  if (name == wrapAttr) {
    if (shouldWrapText()) {
      addPropertyToPresentationAttributeStyle(style, CSSPropertyWhiteSpace,
                                              CSSValuePreWrap);
      addPropertyToPresentationAttributeStyle(style, CSSPropertyWordWrap,
                                              CSSValueBreakWord);
    } else {
      addPropertyToPresentationAttributeStyle(style, CSSPropertyWhiteSpace,
                                              CSSValuePre);
      addPropertyToPresentationAttributeStyle(style, CSSPropertyWordWrap,
                                              CSSValueNormal);
    }
  } else {
    TextControlElement::collectStyleForPresentationAttribute(name, value,
                                                             style);
  }
}

void HTMLTextAreaElement::parseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  const AtomicString& value = params.newValue;
  if (name == rowsAttr) {
    unsigned rows = 0;
    if (value.isEmpty() || !parseHTMLNonNegativeInteger(value, rows) ||
        rows <= 0)
      rows = defaultRows;
    if (m_rows != rows) {
      m_rows = rows;
      if (layoutObject())
        layoutObject()
            ->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
                LayoutInvalidationReason::AttributeChanged);
    }
  } else if (name == colsAttr) {
    unsigned cols = 0;
    if (value.isEmpty() || !parseHTMLNonNegativeInteger(value, cols) ||
        cols <= 0)
      cols = defaultCols;
    if (m_cols != cols) {
      m_cols = cols;
      if (LayoutObject* layoutObject = this->layoutObject())
        layoutObject->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
            LayoutInvalidationReason::AttributeChanged);
    }
  } else if (name == wrapAttr) {
    // The virtual/physical values were a Netscape extension of HTML 3.0, now
    // deprecated.  The soft/hard /off values are a recommendation for HTML 4
    // extension by IE and NS 4.
    WrapMethod wrap;
    if (equalIgnoringCase(value, "physical") ||
        equalIgnoringCase(value, "hard") || equalIgnoringCase(value, "on"))
      wrap = HardWrap;
    else if (equalIgnoringCase(value, "off"))
      wrap = NoWrap;
    else
      wrap = SoftWrap;
    if (wrap != m_wrap) {
      m_wrap = wrap;
      if (LayoutObject* layoutObject = this->layoutObject())
        layoutObject->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
            LayoutInvalidationReason::AttributeChanged);
    }
  } else if (name == accesskeyAttr) {
    // ignore for the moment
  } else if (name == maxlengthAttr) {
    UseCounter::count(document(), UseCounter::TextAreaMaxLength);
    setNeedsValidityCheck();
  } else if (name == minlengthAttr) {
    UseCounter::count(document(), UseCounter::TextAreaMinLength);
    setNeedsValidityCheck();
  } else {
    TextControlElement::parseAttribute(params);
  }
}

LayoutObject* HTMLTextAreaElement::createLayoutObject(const ComputedStyle&) {
  return new LayoutTextControlMultiLine(this);
}

void HTMLTextAreaElement::appendToFormData(FormData& formData) {
  if (name().isEmpty())
    return;

  document().updateStyleAndLayout();

  const String& text =
      (m_wrap == HardWrap) ? valueWithHardLineBreaks() : value();
  formData.append(name(), text);

  const AtomicString& dirnameAttrValue = fastGetAttribute(dirnameAttr);
  if (!dirnameAttrValue.isNull())
    formData.append(dirnameAttrValue, directionForFormData());
}

void HTMLTextAreaElement::resetImpl() {
  setNonDirtyValue(defaultValue());
}

bool HTMLTextAreaElement::hasCustomFocusLogic() const {
  return true;
}

bool HTMLTextAreaElement::isKeyboardFocusable() const {
  // If a given text area can be focused at all, then it will always be keyboard
  // focusable.
  return isFocusable();
}

bool HTMLTextAreaElement::shouldShowFocusRingOnMouseFocus() const {
  return true;
}

void HTMLTextAreaElement::updateFocusAppearance(
    SelectionBehaviorOnFocus selectionBehavior) {
  switch (selectionBehavior) {
    case SelectionBehaviorOnFocus::Reset:  // Fallthrough.
    case SelectionBehaviorOnFocus::Restore:
      restoreCachedSelection();
      break;
    case SelectionBehaviorOnFocus::None:
      return;
  }
  if (document().frame())
    document().frame()->selection().revealSelection();
}

void HTMLTextAreaElement::defaultEventHandler(Event* event) {
  if (layoutObject() && (event->isMouseEvent() || event->isDragEvent() ||
                         event->hasInterface(EventNames::WheelEvent) ||
                         event->type() == EventTypeNames::blur))
    forwardEvent(event);
  else if (layoutObject() && event->isBeforeTextInsertedEvent())
    handleBeforeTextInsertedEvent(static_cast<BeforeTextInsertedEvent*>(event));

  TextControlElement::defaultEventHandler(event);
}

void HTMLTextAreaElement::handleFocusEvent(Element*, WebFocusType) {
  if (LocalFrame* frame = document().frame())
    frame->spellChecker().didBeginEditing(this);
}

void HTMLTextAreaElement::subtreeHasChanged() {
#if DCHECK_IS_ON()
  // The innerEditor should have either Text nodes or a placeholder break
  // element. If we see other nodes, it's a bug in editing code and we should
  // fix it.
  Element* innerEditor = innerEditorElement();
  for (Node& node : NodeTraversal::descendantsOf(*innerEditor)) {
    if (node.isTextNode())
      continue;
    DCHECK(isHTMLBRElement(node));
    DCHECK_EQ(&node, innerEditor->lastChild());
  }
#endif
  addPlaceholderBreakElementIfNecessary();
  setChangedSinceLastFormControlChangeEvent(true);
  m_valueIsUpToDate = false;
  setNeedsValidityCheck();
  setAutofilled(false);
  updatePlaceholderVisibility();

  if (!isFocused())
    return;

  // When typing in a textarea, childrenChanged is not called, so we need to
  // force the directionality check.
  calculateAndAdjustDirectionality();

  DCHECK(document().isActive());
  document().page()->chromeClient().didChangeValueInTextField(*this);
}

void HTMLTextAreaElement::handleBeforeTextInsertedEvent(
    BeforeTextInsertedEvent* event) const {
  DCHECK(event);
  DCHECK(layoutObject());
  int signedMaxLength = maxLength();
  if (signedMaxLength < 0)
    return;
  unsigned unsignedMaxLength = static_cast<unsigned>(signedMaxLength);

  const String& currentValue = innerEditorValue();
  unsigned currentLength = computeLengthForAPIValue(currentValue);
  if (currentLength + computeLengthForAPIValue(event->text()) <
      unsignedMaxLength)
    return;

  // selectionLength represents the selection length of this text field to be
  // removed by this insertion.
  // If the text field has no focus, we don't need to take account of the
  // selection length. The selection is the source of text drag-and-drop in
  // that case, and nothing in the text field will be removed.
  unsigned selectionLength = 0;
  if (isFocused()) {
    // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
    // needs to be audited.  See http://crbug.com/590369 for more details.
    document().updateStyleAndLayoutIgnorePendingStylesheets();

    selectionLength = computeLengthForAPIValue(
        document().frame()->selection().selectedText());
  }
  DCHECK_GE(currentLength, selectionLength);
  unsigned baseLength = currentLength - selectionLength;
  unsigned appendableLength =
      unsignedMaxLength > baseLength ? unsignedMaxLength - baseLength : 0;
  event->setText(sanitizeUserInputValue(event->text(), appendableLength));
}

String HTMLTextAreaElement::sanitizeUserInputValue(const String& proposedValue,
                                                   unsigned maxLength) {
  unsigned submissionLength = 0;
  unsigned i = 0;
  for (; i < proposedValue.length(); ++i) {
    if (proposedValue[i] == '\r' && i + 1 < proposedValue.length() &&
        proposedValue[i + 1] == '\n')
      continue;
    ++submissionLength;
    if (submissionLength == maxLength) {
      ++i;
      break;
    }
    if (submissionLength > maxLength)
      break;
  }
  if (i > 0 && U16_IS_LEAD(proposedValue[i - 1]))
    --i;
  return proposedValue.left(i);
}

void HTMLTextAreaElement::updateValue() const {
  if (m_valueIsUpToDate)
    return;

  m_value = innerEditorValue();
  const_cast<HTMLTextAreaElement*>(this)->m_valueIsUpToDate = true;
  const_cast<HTMLTextAreaElement*>(this)->notifyFormStateChanged();
  m_isDirty = true;
  const_cast<HTMLTextAreaElement*>(this)->updatePlaceholderVisibility();
}

String HTMLTextAreaElement::value() const {
  updateValue();
  return m_value;
}

void HTMLTextAreaElement::setValue(const String& value,
                                   TextFieldEventBehavior eventBehavior) {
  setValueCommon(value, eventBehavior);
  m_isDirty = true;
}

void HTMLTextAreaElement::setNonDirtyValue(const String& value) {
  setValueCommon(value, DispatchNoEvent, SetSeletion);
  m_isDirty = false;
}

void HTMLTextAreaElement::setValueCommon(const String& newValue,
                                         TextFieldEventBehavior eventBehavior,
                                         SetValueCommonOption setValueOption) {
  // Code elsewhere normalizes line endings added by the user via the keyboard
  // or pasting.  We normalize line endings coming from JavaScript here.
  String normalizedValue = newValue.isNull() ? "" : newValue;
  normalizedValue.replace("\r\n", "\n");
  normalizedValue.replace('\r', '\n');

  // Return early because we don't want to trigger other side effects
  // when the value isn't changing.
  // FIXME: Simple early return doesn't match the Firefox ever.
  // Remove these lines.
  if (normalizedValue == value()) {
    if (setValueOption == SetSeletion) {
      setNeedsValidityCheck();
      if (isFinishedParsingChildren()) {
        // Set the caret to the end of the text value except for initialize.
        unsigned endOfString = m_value.length();
        setSelectionRange(endOfString, endOfString);
      }
    }
    return;
  }

  m_value = normalizedValue;
  setInnerEditorValue(m_value);
  if (eventBehavior == DispatchNoEvent)
    setLastChangeWasNotUserEdit();
  updatePlaceholderVisibility();
  setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(
                                              StyleChangeReason::ControlValue));
  m_suggestedValue = String();
  setNeedsValidityCheck();
  if (isFinishedParsingChildren()) {
    // Set the caret to the end of the text value except for initialize.
    unsigned endOfString = m_value.length();
    setSelectionRange(endOfString, endOfString);
  }

  notifyFormStateChanged();
  switch (eventBehavior) {
    case DispatchChangeEvent:
      dispatchFormControlChangeEvent();
      break;

    case DispatchInputAndChangeEvent:
      dispatchFormControlInputEvent();
      dispatchFormControlChangeEvent();
      break;

    case DispatchNoEvent:
      // We need to update textAsOfLastFormControlChangeEvent for |value| IDL
      // setter without focus because input-assist features use setValue("...",
      // DispatchChangeEvent) without setting focus.
      if (!isFocused())
        setTextAsOfLastFormControlChangeEvent(normalizedValue);
      break;
  }
}

void HTMLTextAreaElement::setInnerEditorValue(const String& value) {
  TextControlElement::setInnerEditorValue(value);
  m_valueIsUpToDate = true;
}

String HTMLTextAreaElement::defaultValue() const {
  StringBuilder value;

  // Since there may be comments, ignore nodes other than text nodes.
  for (Node* n = firstChild(); n; n = n->nextSibling()) {
    if (n->isTextNode())
      value.append(toText(n)->data());
  }

  return value.toString();
}

void HTMLTextAreaElement::setDefaultValue(const String& defaultValue) {
  // To preserve comments, remove only the text nodes, then add a single text
  // node.
  HeapVector<Member<Node>> textNodes;
  for (Node* n = firstChild(); n; n = n->nextSibling()) {
    if (n->isTextNode())
      textNodes.push_back(n);
  }
  for (const auto& text : textNodes)
    removeChild(text.get(), IGNORE_EXCEPTION_FOR_TESTING);

  // Normalize line endings.
  String value = defaultValue;
  value.replace("\r\n", "\n");
  value.replace('\r', '\n');

  insertBefore(document().createTextNode(value), firstChild(),
               IGNORE_EXCEPTION_FOR_TESTING);

  if (!m_isDirty)
    setNonDirtyValue(value);
}

String HTMLTextAreaElement::suggestedValue() const {
  return m_suggestedValue;
}

void HTMLTextAreaElement::setSuggestedValue(const String& value) {
  m_suggestedValue = value;

  if (!value.isNull())
    setInnerEditorValue(m_suggestedValue);
  else
    setInnerEditorValue(m_value);
  updatePlaceholderVisibility();
  setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(
                                              StyleChangeReason::ControlValue));
}

String HTMLTextAreaElement::validationMessage() const {
  if (!willValidate())
    return String();

  if (customError())
    return customValidationMessage();

  if (valueMissing())
    return locale().queryString(WebLocalizedString::ValidationValueMissing);

  if (tooLong())
    return locale().validationMessageTooLongText(value().length(), maxLength());

  if (tooShort())
    return locale().validationMessageTooShortText(value().length(),
                                                  minLength());

  return String();
}

bool HTMLTextAreaElement::valueMissing() const {
  // We should not call value() for performance.
  return willValidate() && valueMissing(nullptr);
}

bool HTMLTextAreaElement::valueMissing(const String* value) const {
  return isRequiredFormControl() && !isDisabledOrReadOnly() &&
         (value ? *value : this->value()).isEmpty();
}

bool HTMLTextAreaElement::tooLong() const {
  // We should not call value() for performance.
  return willValidate() && tooLong(nullptr, CheckDirtyFlag);
}

bool HTMLTextAreaElement::tooShort() const {
  // We should not call value() for performance.
  return willValidate() && tooShort(nullptr, CheckDirtyFlag);
}

bool HTMLTextAreaElement::tooLong(const String* value,
                                  NeedsToCheckDirtyFlag check) const {
  // Return false for the default value or value set by script even if it is
  // longer than maxLength.
  if (check == CheckDirtyFlag && !lastChangeWasUserEdit())
    return false;

  int max = maxLength();
  if (max < 0)
    return false;
  unsigned len =
      value ? computeLengthForAPIValue(*value) : this->value().length();
  return len > static_cast<unsigned>(max);
}

bool HTMLTextAreaElement::tooShort(const String* value,
                                   NeedsToCheckDirtyFlag check) const {
  // Return false for the default value or value set by script even if it is
  // shorter than minLength.
  if (check == CheckDirtyFlag && !lastChangeWasUserEdit())
    return false;

  int min = minLength();
  if (min <= 0)
    return false;
  // An empty string is excluded from minlength check.
  unsigned len =
      value ? computeLengthForAPIValue(*value) : this->value().length();
  return len > 0 && len < static_cast<unsigned>(min);
}

bool HTMLTextAreaElement::isValidValue(const String& candidate) const {
  return !valueMissing(&candidate) && !tooLong(&candidate, IgnoreDirtyFlag) &&
         !tooShort(&candidate, IgnoreDirtyFlag);
}

void HTMLTextAreaElement::accessKeyAction(bool) {
  focus();
}

void HTMLTextAreaElement::setCols(unsigned cols) {
  setUnsignedIntegralAttribute(colsAttr, cols ? cols : defaultCols);
}

void HTMLTextAreaElement::setRows(unsigned rows) {
  setUnsignedIntegralAttribute(rowsAttr, rows ? rows : defaultRows);
}

bool HTMLTextAreaElement::matchesReadOnlyPseudoClass() const {
  return isReadOnly();
}

bool HTMLTextAreaElement::matchesReadWritePseudoClass() const {
  return !isReadOnly();
}

void HTMLTextAreaElement::setPlaceholderVisibility(bool visible) {
  m_isPlaceholderVisible = visible;
}

void HTMLTextAreaElement::updatePlaceholderText() {
  HTMLElement* placeholder = placeholderElement();
  const AtomicString& placeholderText = fastGetAttribute(placeholderAttr);
  if (placeholderText.isEmpty()) {
    if (placeholder)
      userAgentShadowRoot()->removeChild(placeholder);
    return;
  }
  if (!placeholder) {
    HTMLDivElement* newElement = HTMLDivElement::create(document());
    placeholder = newElement;
    placeholder->setShadowPseudoId(AtomicString("-webkit-input-placeholder"));
    placeholder->setAttribute(idAttr, ShadowElementNames::placeholder());
    placeholder->setInlineStyleProperty(
        CSSPropertyDisplay,
        isPlaceholderVisible() ? CSSValueBlock : CSSValueNone, true);
    userAgentShadowRoot()->insertBefore(placeholder, innerEditorElement());
  }
  placeholder->setTextContent(placeholderText);
}

bool HTMLTextAreaElement::isInteractiveContent() const {
  return true;
}

bool HTMLTextAreaElement::supportsAutofocus() const {
  return true;
}

const AtomicString& HTMLTextAreaElement::defaultAutocapitalize() const {
  DEFINE_STATIC_LOCAL(const AtomicString, sentences, ("sentences"));
  return sentences;
}

void HTMLTextAreaElement::copyNonAttributePropertiesFromElement(
    const Element& source) {
  const HTMLTextAreaElement& sourceElement =
      static_cast<const HTMLTextAreaElement&>(source);
  setValueCommon(sourceElement.value(), DispatchNoEvent, SetSeletion);
  m_isDirty = sourceElement.m_isDirty;
  TextControlElement::copyNonAttributePropertiesFromElement(source);
}

}  // namespace blink
