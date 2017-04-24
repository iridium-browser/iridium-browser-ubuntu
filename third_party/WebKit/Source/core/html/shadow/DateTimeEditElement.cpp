/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "core/html/shadow/DateTimeEditElement.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/Text.h"
#include "core/events/MouseEvent.h"
#include "core/html/forms/DateTimeFieldsState.h"
#include "core/html/shadow/DateTimeFieldElements.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/style/ComputedStyle.h"
#include "core/style/StyleInheritedData.h"
#include "platform/fonts/FontCache.h"
#include "platform/text/DateTimeFormat.h"
#include "platform/text/PlatformLocale.h"
#include "wtf/DateMath.h"

namespace blink {

using namespace HTMLNames;
using namespace WTF::Unicode;

class DateTimeEditBuilder : private DateTimeFormat::TokenHandler {
 public:
  // The argument objects must be alive until this object dies.
  DateTimeEditBuilder(DateTimeEditElement&,
                      const DateTimeEditElement::LayoutParameters&,
                      const DateComponents&);

  bool build(const String&);

 private:
  bool needMillisecondField() const;
  bool shouldAMPMFieldDisabled() const;
  bool shouldDayOfMonthFieldDisabled() const;
  bool shouldHourFieldDisabled() const;
  bool shouldMillisecondFieldDisabled() const;
  bool shouldMinuteFieldDisabled() const;
  bool shouldSecondFieldDisabled() const;
  bool shouldYearFieldDisabled() const;
  inline const StepRange& stepRange() const { return m_parameters.stepRange; }
  DateTimeNumericFieldElement::Step createStep(double msPerFieldUnit,
                                               double msPerFieldSize) const;

  // DateTimeFormat::TokenHandler functions.
  void visitField(DateTimeFormat::FieldType, int) final;
  void visitLiteral(const String&) final;

  DateTimeEditElement& editElement() const;

  Member<DateTimeEditElement> m_editElement;
  const DateComponents m_dateValue;
  const DateTimeEditElement::LayoutParameters& m_parameters;
  DateTimeNumericFieldElement::Range m_dayRange;
  DateTimeNumericFieldElement::Range m_hour23Range;
  DateTimeNumericFieldElement::Range m_minuteRange;
  DateTimeNumericFieldElement::Range m_secondRange;
  DateTimeNumericFieldElement::Range m_millisecondRange;
};

DateTimeEditBuilder::DateTimeEditBuilder(
    DateTimeEditElement& element,
    const DateTimeEditElement::LayoutParameters& layoutParameters,
    const DateComponents& dateValue)
    : m_editElement(&element),
      m_dateValue(dateValue),
      m_parameters(layoutParameters),
      m_dayRange(1, 31),
      m_hour23Range(0, 23),
      m_minuteRange(0, 59),
      m_secondRange(0, 59),
      m_millisecondRange(0, 999) {
  if (m_dateValue.getType() == DateComponents::Date ||
      m_dateValue.getType() == DateComponents::DateTimeLocal) {
    if (m_parameters.minimum.getType() != DateComponents::Invalid &&
        m_parameters.maximum.getType() != DateComponents::Invalid &&
        m_parameters.minimum.fullYear() == m_parameters.maximum.fullYear() &&
        m_parameters.minimum.month() == m_parameters.maximum.month() &&
        m_parameters.minimum.monthDay() <= m_parameters.maximum.monthDay()) {
      m_dayRange.minimum = m_parameters.minimum.monthDay();
      m_dayRange.maximum = m_parameters.maximum.monthDay();
    }
  }

  if (m_dateValue.getType() == DateComponents::Time ||
      m_dayRange.isSingleton()) {
    if (m_parameters.minimum.getType() != DateComponents::Invalid &&
        m_parameters.maximum.getType() != DateComponents::Invalid &&
        m_parameters.minimum.hour() <= m_parameters.maximum.hour()) {
      m_hour23Range.minimum = m_parameters.minimum.hour();
      m_hour23Range.maximum = m_parameters.maximum.hour();
    }
  }

  if (m_hour23Range.isSingleton() &&
      m_parameters.minimum.minute() <= m_parameters.maximum.minute()) {
    m_minuteRange.minimum = m_parameters.minimum.minute();
    m_minuteRange.maximum = m_parameters.maximum.minute();
  }
  if (m_minuteRange.isSingleton() &&
      m_parameters.minimum.second() <= m_parameters.maximum.second()) {
    m_secondRange.minimum = m_parameters.minimum.second();
    m_secondRange.maximum = m_parameters.maximum.second();
  }
  if (m_secondRange.isSingleton() &&
      m_parameters.minimum.millisecond() <=
          m_parameters.maximum.millisecond()) {
    m_millisecondRange.minimum = m_parameters.minimum.millisecond();
    m_millisecondRange.maximum = m_parameters.maximum.millisecond();
  }
}

bool DateTimeEditBuilder::build(const String& formatString) {
  editElement().resetFields();
  return DateTimeFormat::parse(formatString, *this);
}

bool DateTimeEditBuilder::needMillisecondField() const {
  return m_dateValue.millisecond() ||
         !stepRange()
              .minimum()
              .remainder(static_cast<int>(msPerSecond))
              .isZero() ||
         !stepRange().step().remainder(static_cast<int>(msPerSecond)).isZero();
}

void DateTimeEditBuilder::visitField(DateTimeFormat::FieldType fieldType,
                                     int count) {
  const int countForAbbreviatedMonth = 3;
  const int countForFullMonth = 4;
  const int countForNarrowMonth = 5;
  Document& document = editElement().document();

  switch (fieldType) {
    case DateTimeFormat::FieldTypeDayOfMonth: {
      DateTimeFieldElement* field = DateTimeDayFieldElement::create(
          document, editElement(), m_parameters.placeholderForDay, m_dayRange);
      editElement().addField(field);
      if (shouldDayOfMonthFieldDisabled()) {
        field->setValueAsDate(m_dateValue);
        field->setDisabled();
      }
      return;
    }

    case DateTimeFormat::FieldTypeHour11: {
      DateTimeNumericFieldElement::Step step =
          createStep(msPerHour, msPerHour * 12);
      DateTimeFieldElement* field = DateTimeHour11FieldElement::create(
          document, editElement(), m_hour23Range, step);
      editElement().addField(field);
      if (shouldHourFieldDisabled()) {
        field->setValueAsDate(m_dateValue);
        field->setDisabled();
      }
      return;
    }

    case DateTimeFormat::FieldTypeHour12: {
      DateTimeNumericFieldElement::Step step =
          createStep(msPerHour, msPerHour * 12);
      DateTimeFieldElement* field = DateTimeHour12FieldElement::create(
          document, editElement(), m_hour23Range, step);
      editElement().addField(field);
      if (shouldHourFieldDisabled()) {
        field->setValueAsDate(m_dateValue);
        field->setDisabled();
      }
      return;
    }

    case DateTimeFormat::FieldTypeHour23: {
      DateTimeNumericFieldElement::Step step = createStep(msPerHour, msPerDay);
      DateTimeFieldElement* field = DateTimeHour23FieldElement::create(
          document, editElement(), m_hour23Range, step);
      editElement().addField(field);
      if (shouldHourFieldDisabled()) {
        field->setValueAsDate(m_dateValue);
        field->setDisabled();
      }
      return;
    }

    case DateTimeFormat::FieldTypeHour24: {
      DateTimeNumericFieldElement::Step step = createStep(msPerHour, msPerDay);
      DateTimeFieldElement* field = DateTimeHour24FieldElement::create(
          document, editElement(), m_hour23Range, step);
      editElement().addField(field);
      if (shouldHourFieldDisabled()) {
        field->setValueAsDate(m_dateValue);
        field->setDisabled();
      }
      return;
    }

    case DateTimeFormat::FieldTypeMinute: {
      DateTimeNumericFieldElement::Step step =
          createStep(msPerMinute, msPerHour);
      DateTimeNumericFieldElement* field = DateTimeMinuteFieldElement::create(
          document, editElement(), m_minuteRange, step);
      editElement().addField(field);
      if (shouldMinuteFieldDisabled()) {
        field->setValueAsDate(m_dateValue);
        field->setDisabled();
      }
      return;
    }

    case DateTimeFormat::FieldTypeMonth:  // Fallthrough.
    case DateTimeFormat::FieldTypeMonthStandAlone: {
      int minMonth = 0, maxMonth = 11;
      if (m_parameters.minimum.getType() != DateComponents::Invalid &&
          m_parameters.maximum.getType() != DateComponents::Invalid &&
          m_parameters.minimum.fullYear() == m_parameters.maximum.fullYear() &&
          m_parameters.minimum.month() <= m_parameters.maximum.month()) {
        minMonth = m_parameters.minimum.month();
        maxMonth = m_parameters.maximum.month();
      }
      DateTimeFieldElement* field;
      switch (count) {
        case countForNarrowMonth:  // Fallthrough.
        case countForAbbreviatedMonth:
          field = DateTimeSymbolicMonthFieldElement::create(
              document, editElement(),
              fieldType == DateTimeFormat::FieldTypeMonth
                  ? m_parameters.locale.shortMonthLabels()
                  : m_parameters.locale.shortStandAloneMonthLabels(),
              minMonth, maxMonth);
          break;
        case countForFullMonth:
          field = DateTimeSymbolicMonthFieldElement::create(
              document, editElement(),
              fieldType == DateTimeFormat::FieldTypeMonth
                  ? m_parameters.locale.monthLabels()
                  : m_parameters.locale.standAloneMonthLabels(),
              minMonth, maxMonth);
          break;
        default:
          field = DateTimeMonthFieldElement::create(
              document, editElement(), m_parameters.placeholderForMonth,
              DateTimeNumericFieldElement::Range(minMonth + 1, maxMonth + 1));
          break;
      }
      editElement().addField(field);
      if (minMonth == maxMonth && minMonth == m_dateValue.month() &&
          m_dateValue.getType() != DateComponents::Month) {
        field->setValueAsDate(m_dateValue);
        field->setDisabled();
      }
      return;
    }

    case DateTimeFormat::FieldTypePeriod: {
      DateTimeFieldElement* field = DateTimeAMPMFieldElement::create(
          document, editElement(), m_parameters.locale.timeAMPMLabels());
      editElement().addField(field);
      if (shouldAMPMFieldDisabled()) {
        field->setValueAsDate(m_dateValue);
        field->setDisabled();
      }
      return;
    }

    case DateTimeFormat::FieldTypeSecond: {
      DateTimeNumericFieldElement::Step step =
          createStep(msPerSecond, msPerMinute);
      DateTimeNumericFieldElement* field = DateTimeSecondFieldElement::create(
          document, editElement(), m_secondRange, step);
      editElement().addField(field);
      if (shouldSecondFieldDisabled()) {
        field->setValueAsDate(m_dateValue);
        field->setDisabled();
      }

      if (needMillisecondField()) {
        visitLiteral(m_parameters.locale.localizedDecimalSeparator());
        visitField(DateTimeFormat::FieldTypeFractionalSecond, 3);
      }
      return;
    }

    case DateTimeFormat::FieldTypeFractionalSecond: {
      DateTimeNumericFieldElement::Step step = createStep(1, msPerSecond);
      DateTimeNumericFieldElement* field =
          DateTimeMillisecondFieldElement::create(document, editElement(),
                                                  m_millisecondRange, step);
      editElement().addField(field);
      if (shouldMillisecondFieldDisabled()) {
        field->setValueAsDate(m_dateValue);
        field->setDisabled();
      }
      return;
    }

    case DateTimeFormat::FieldTypeWeekOfYear: {
      DateTimeNumericFieldElement::Range range(
          DateComponents::minimumWeekNumber, DateComponents::maximumWeekNumber);
      if (m_parameters.minimum.getType() != DateComponents::Invalid &&
          m_parameters.maximum.getType() != DateComponents::Invalid &&
          m_parameters.minimum.fullYear() == m_parameters.maximum.fullYear() &&
          m_parameters.minimum.week() <= m_parameters.maximum.week()) {
        range.minimum = m_parameters.minimum.week();
        range.maximum = m_parameters.maximum.week();
      }
      editElement().addField(
          DateTimeWeekFieldElement::create(document, editElement(), range));
      return;
    }

    case DateTimeFormat::FieldTypeYear: {
      DateTimeYearFieldElement::Parameters yearParams;
      if (m_parameters.minimum.getType() == DateComponents::Invalid) {
        yearParams.minimumYear = DateComponents::minimumYear();
        yearParams.minIsSpecified = false;
      } else {
        yearParams.minimumYear = m_parameters.minimum.fullYear();
        yearParams.minIsSpecified = true;
      }
      if (m_parameters.maximum.getType() == DateComponents::Invalid) {
        yearParams.maximumYear = DateComponents::maximumYear();
        yearParams.maxIsSpecified = false;
      } else {
        yearParams.maximumYear = m_parameters.maximum.fullYear();
        yearParams.maxIsSpecified = true;
      }
      if (yearParams.minimumYear > yearParams.maximumYear) {
        std::swap(yearParams.minimumYear, yearParams.maximumYear);
        std::swap(yearParams.minIsSpecified, yearParams.maxIsSpecified);
      }
      yearParams.placeholder = m_parameters.placeholderForYear;
      DateTimeFieldElement* field =
          DateTimeYearFieldElement::create(document, editElement(), yearParams);
      editElement().addField(field);
      if (shouldYearFieldDisabled()) {
        field->setValueAsDate(m_dateValue);
        field->setDisabled();
      }
      return;
    }

    default:
      return;
  }
}

bool DateTimeEditBuilder::shouldAMPMFieldDisabled() const {
  return shouldHourFieldDisabled() ||
         (m_hour23Range.minimum < 12 && m_hour23Range.maximum < 12 &&
          m_dateValue.hour() < 12) ||
         (m_hour23Range.minimum >= 12 && m_hour23Range.maximum >= 12 &&
          m_dateValue.hour() >= 12);
}

bool DateTimeEditBuilder::shouldDayOfMonthFieldDisabled() const {
  return m_dayRange.isSingleton() &&
         m_dayRange.minimum == m_dateValue.monthDay() &&
         m_dateValue.getType() != DateComponents::Date;
}

bool DateTimeEditBuilder::shouldHourFieldDisabled() const {
  if (m_hour23Range.isSingleton() &&
      m_hour23Range.minimum == m_dateValue.hour() &&
      !(shouldMinuteFieldDisabled() && shouldSecondFieldDisabled() &&
        shouldMillisecondFieldDisabled()))
    return true;

  if (m_dateValue.getType() == DateComponents::Time)
    return false;
  DCHECK_EQ(m_dateValue.getType(), DateComponents::DateTimeLocal);

  if (shouldDayOfMonthFieldDisabled()) {
    DCHECK_EQ(m_parameters.minimum.fullYear(), m_parameters.maximum.fullYear());
    DCHECK_EQ(m_parameters.minimum.month(), m_parameters.maximum.month());
    return false;
  }

  const Decimal decimalMsPerDay(static_cast<int>(msPerDay));
  Decimal hourPartOfMinimum =
      (stepRange().stepBase().abs().remainder(decimalMsPerDay) /
       static_cast<int>(msPerHour))
          .floor();
  return hourPartOfMinimum == m_dateValue.hour() &&
         stepRange().step().remainder(decimalMsPerDay).isZero();
}

bool DateTimeEditBuilder::shouldMillisecondFieldDisabled() const {
  if (m_millisecondRange.isSingleton() &&
      m_millisecondRange.minimum == m_dateValue.millisecond())
    return true;

  const Decimal decimalMsPerSecond(static_cast<int>(msPerSecond));
  return stepRange().stepBase().abs().remainder(decimalMsPerSecond) ==
             m_dateValue.millisecond() &&
         stepRange().step().remainder(decimalMsPerSecond).isZero();
}

bool DateTimeEditBuilder::shouldMinuteFieldDisabled() const {
  if (m_minuteRange.isSingleton() &&
      m_minuteRange.minimum == m_dateValue.minute())
    return true;

  const Decimal decimalMsPerHour(static_cast<int>(msPerHour));
  Decimal minutePartOfMinimum =
      (stepRange().stepBase().abs().remainder(decimalMsPerHour) /
       static_cast<int>(msPerMinute))
          .floor();
  return minutePartOfMinimum == m_dateValue.minute() &&
         stepRange().step().remainder(decimalMsPerHour).isZero();
}

bool DateTimeEditBuilder::shouldSecondFieldDisabled() const {
  if (m_secondRange.isSingleton() &&
      m_secondRange.minimum == m_dateValue.second())
    return true;

  const Decimal decimalMsPerMinute(static_cast<int>(msPerMinute));
  Decimal secondPartOfMinimum =
      (stepRange().stepBase().abs().remainder(decimalMsPerMinute) /
       static_cast<int>(msPerSecond))
          .floor();
  return secondPartOfMinimum == m_dateValue.second() &&
         stepRange().step().remainder(decimalMsPerMinute).isZero();
}

bool DateTimeEditBuilder::shouldYearFieldDisabled() const {
  return m_parameters.minimum.getType() != DateComponents::Invalid &&
         m_parameters.maximum.getType() != DateComponents::Invalid &&
         m_parameters.minimum.fullYear() == m_parameters.maximum.fullYear() &&
         m_parameters.minimum.fullYear() == m_dateValue.fullYear();
}

void DateTimeEditBuilder::visitLiteral(const String& text) {
  DEFINE_STATIC_LOCAL(AtomicString, textPseudoId,
                      ("-webkit-datetime-edit-text"));
  DCHECK_GT(text.length(), 0u);
  HTMLDivElement* element = HTMLDivElement::create(editElement().document());
  element->setShadowPseudoId(textPseudoId);
  if (m_parameters.locale.isRTL() && text.length()) {
    CharDirection dir = direction(text[0]);
    if (dir == SegmentSeparator || dir == WhiteSpaceNeutral ||
        dir == OtherNeutral)
      element->appendChild(Text::create(editElement().document(),
                                        String(&rightToLeftMarkCharacter, 1)));
  }
  element->appendChild(Text::create(editElement().document(), text));
  editElement().fieldsWrapperElement()->appendChild(element);
}

DateTimeEditElement& DateTimeEditBuilder::editElement() const {
  return *m_editElement;
}

DateTimeNumericFieldElement::Step DateTimeEditBuilder::createStep(
    double msPerFieldUnit,
    double msPerFieldSize) const {
  const Decimal msPerFieldUnitDecimal(static_cast<int>(msPerFieldUnit));
  const Decimal msPerFieldSizeDecimal(static_cast<int>(msPerFieldSize));
  Decimal stepMilliseconds = stepRange().step();
  DCHECK(!msPerFieldUnitDecimal.isZero());
  DCHECK(!msPerFieldSizeDecimal.isZero());
  DCHECK(!stepMilliseconds.isZero());

  DateTimeNumericFieldElement::Step step(1, 0);

  if (stepMilliseconds.remainder(msPerFieldSizeDecimal).isZero())
    stepMilliseconds = msPerFieldSizeDecimal;

  if (msPerFieldSizeDecimal.remainder(stepMilliseconds).isZero() &&
      stepMilliseconds.remainder(msPerFieldUnitDecimal).isZero()) {
    step.step =
        static_cast<int>((stepMilliseconds / msPerFieldUnitDecimal).toDouble());
    step.stepBase = static_cast<int>(
        (stepRange().stepBase() / msPerFieldUnitDecimal)
            .floor()
            .remainder(msPerFieldSizeDecimal / msPerFieldUnitDecimal)
            .toDouble());
  }
  return step;
}

// ----------------------------

DateTimeEditElement::EditControlOwner::~EditControlOwner() {}

DateTimeEditElement::DateTimeEditElement(Document& document,
                                         EditControlOwner& editControlOwner)
    : HTMLDivElement(document), m_editControlOwner(&editControlOwner) {
  setHasCustomStyleCallbacks();
}

DateTimeEditElement::~DateTimeEditElement() {}

DEFINE_TRACE(DateTimeEditElement) {
  visitor->trace(m_fields);
  visitor->trace(m_editControlOwner);
  HTMLDivElement::trace(visitor);
}

inline Element* DateTimeEditElement::fieldsWrapperElement() const {
  DCHECK(firstChild());
  return toElementOrDie(firstChild());
}

void DateTimeEditElement::addField(DateTimeFieldElement* field) {
  if (m_fields.size() >= maximumNumberOfFields)
    return;
  m_fields.push_back(field);
  fieldsWrapperElement()->appendChild(field);
}

bool DateTimeEditElement::anyEditableFieldsHaveValues() const {
  for (const auto& field : m_fields) {
    if (!field->isDisabled() && field->hasValue())
      return true;
  }
  return false;
}

void DateTimeEditElement::blurByOwner() {
  if (DateTimeFieldElement* field = focusedField())
    field->blur();
}

DateTimeEditElement* DateTimeEditElement::create(
    Document& document,
    EditControlOwner& editControlOwner) {
  DateTimeEditElement* container =
      new DateTimeEditElement(document, editControlOwner);
  container->setShadowPseudoId(AtomicString("-webkit-datetime-edit"));
  container->setAttribute(idAttr, ShadowElementNames::dateTimeEdit());
  return container;
}

PassRefPtr<ComputedStyle> DateTimeEditElement::customStyleForLayoutObject() {
  // FIXME: This is a kind of layout. We might want to introduce new
  // layoutObject.
  RefPtr<ComputedStyle> originalStyle = originalStyleForLayoutObject();
  RefPtr<ComputedStyle> style = ComputedStyle::clone(*originalStyle);
  float width = 0;
  for (Node* child = fieldsWrapperElement()->firstChild(); child;
       child = child->nextSibling()) {
    if (!child->isElementNode())
      continue;
    Element* childElement = toElement(child);
    if (childElement->isDateTimeFieldElement()) {
      // We need to pass the ComputedStyle of this element because child
      // elements can't resolve inherited style at this timing.
      width += static_cast<DateTimeFieldElement*>(childElement)
                   ->maximumWidth(*style);
    } else {
      // ::-webkit-datetime-edit-text case. It has no
      // border/padding/margin in html.css.
      width += DateTimeFieldElement::computeTextWidth(
          *style, childElement->textContent());
    }
  }
  style->setWidth(Length(ceilf(width), Fixed));
  style->setUnique();
  return style.release();
}

void DateTimeEditElement::didBlurFromField() {
  if (m_editControlOwner)
    m_editControlOwner->didBlurFromControl();
}

void DateTimeEditElement::didFocusOnField() {
  if (m_editControlOwner)
    m_editControlOwner->didFocusOnControl();
}

void DateTimeEditElement::disabledStateChanged() {
  updateUIState();
}

DateTimeFieldElement* DateTimeEditElement::fieldAt(size_t fieldIndex) const {
  return fieldIndex < m_fields.size() ? m_fields[fieldIndex].get() : 0;
}

size_t DateTimeEditElement::fieldIndexOf(
    const DateTimeFieldElement& field) const {
  for (size_t fieldIndex = 0; fieldIndex < m_fields.size(); ++fieldIndex) {
    if (m_fields[fieldIndex] == &field)
      return fieldIndex;
  }
  return invalidFieldIndex;
}

void DateTimeEditElement::focusIfNoFocus() {
  if (focusedFieldIndex() != invalidFieldIndex)
    return;
  focusOnNextFocusableField(0);
}

void DateTimeEditElement::focusByOwner(Element* oldFocusedElement) {
  if (oldFocusedElement && oldFocusedElement->isDateTimeFieldElement()) {
    DateTimeFieldElement* oldFocusedField =
        static_cast<DateTimeFieldElement*>(oldFocusedElement);
    size_t index = fieldIndexOf(*oldFocusedField);
    document().updateStyleAndLayoutTreeForNode(oldFocusedField);
    if (index != invalidFieldIndex && oldFocusedField->isFocusable()) {
      oldFocusedField->focus();
      return;
    }
  }
  focusOnNextFocusableField(0);
}

DateTimeFieldElement* DateTimeEditElement::focusedField() const {
  return fieldAt(focusedFieldIndex());
}

size_t DateTimeEditElement::focusedFieldIndex() const {
  Element* const focusedFieldElement = document().focusedElement();
  for (size_t fieldIndex = 0; fieldIndex < m_fields.size(); ++fieldIndex) {
    if (m_fields[fieldIndex] == focusedFieldElement)
      return fieldIndex;
  }
  return invalidFieldIndex;
}

void DateTimeEditElement::fieldValueChanged() {
  if (m_editControlOwner)
    m_editControlOwner->editControlValueChanged();
}

bool DateTimeEditElement::focusOnNextFocusableField(size_t startIndex) {
  document().updateStyleAndLayoutTreeIgnorePendingStylesheets();
  for (size_t fieldIndex = startIndex; fieldIndex < m_fields.size();
       ++fieldIndex) {
    if (m_fields[fieldIndex]->isFocusable()) {
      m_fields[fieldIndex]->focus();
      return true;
    }
  }
  return false;
}

bool DateTimeEditElement::focusOnNextField(const DateTimeFieldElement& field) {
  const size_t startFieldIndex = fieldIndexOf(field);
  if (startFieldIndex == invalidFieldIndex)
    return false;
  return focusOnNextFocusableField(startFieldIndex + 1);
}

bool DateTimeEditElement::focusOnPreviousField(
    const DateTimeFieldElement& field) {
  const size_t startFieldIndex = fieldIndexOf(field);
  if (startFieldIndex == invalidFieldIndex)
    return false;
  document().updateStyleAndLayoutTreeIgnorePendingStylesheets();
  size_t fieldIndex = startFieldIndex;
  while (fieldIndex > 0) {
    --fieldIndex;
    if (m_fields[fieldIndex]->isFocusable()) {
      m_fields[fieldIndex]->focus();
      return true;
    }
  }
  return false;
}

bool DateTimeEditElement::isDateTimeEditElement() const {
  return true;
}

bool DateTimeEditElement::isDisabled() const {
  return m_editControlOwner && m_editControlOwner->isEditControlOwnerDisabled();
}

bool DateTimeEditElement::isFieldOwnerDisabled() const {
  return isDisabled();
}

bool DateTimeEditElement::isFieldOwnerReadOnly() const {
  return isReadOnly();
}

bool DateTimeEditElement::isReadOnly() const {
  return m_editControlOwner && m_editControlOwner->isEditControlOwnerReadOnly();
}

void DateTimeEditElement::layout(const LayoutParameters& layoutParameters,
                                 const DateComponents& dateValue) {
  // TODO(tkent): We assume this function never dispatches events. However this
  // can dispatch 'blur' event in Node::removeChild().

  DEFINE_STATIC_LOCAL(AtomicString, fieldsWrapperPseudoId,
                      ("-webkit-datetime-edit-fields-wrapper"));
  if (!hasChildren()) {
    HTMLDivElement* element = HTMLDivElement::create(document());
    element->setShadowPseudoId(fieldsWrapperPseudoId);
    appendChild(element);
  }
  Element* fieldsWrapper = fieldsWrapperElement();

  size_t focusedFieldIndex = this->focusedFieldIndex();
  DateTimeFieldElement* const focusedField = fieldAt(focusedFieldIndex);
  const AtomicString focusedFieldId =
      focusedField ? focusedField->shadowPseudoId() : nullAtom;

  DateTimeEditBuilder builder(*this, layoutParameters, dateValue);
  Node* lastChildToBeRemoved = fieldsWrapper->lastChild();
  if (!builder.build(layoutParameters.dateTimeFormat) || m_fields.isEmpty()) {
    lastChildToBeRemoved = fieldsWrapper->lastChild();
    builder.build(layoutParameters.fallbackDateTimeFormat);
  }

  if (focusedFieldIndex != invalidFieldIndex) {
    for (size_t fieldIndex = 0; fieldIndex < m_fields.size(); ++fieldIndex) {
      if (m_fields[fieldIndex]->shadowPseudoId() == focusedFieldId) {
        focusedFieldIndex = fieldIndex;
        break;
      }
    }
    if (DateTimeFieldElement* field =
            fieldAt(std::min(focusedFieldIndex, m_fields.size() - 1)))
      field->focus();
  }

  if (lastChildToBeRemoved) {
    for (Node* childNode = fieldsWrapper->firstChild(); childNode;
         childNode = fieldsWrapper->firstChild()) {
      fieldsWrapper->removeChild(childNode);
      if (childNode == lastChildToBeRemoved)
        break;
    }
    setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(
                                                StyleChangeReason::Control));
  }
}

AtomicString DateTimeEditElement::localeIdentifier() const {
  return m_editControlOwner ? m_editControlOwner->localeIdentifier() : nullAtom;
}

void DateTimeEditElement::fieldDidChangeValueByKeyboard() {
  if (m_editControlOwner)
    m_editControlOwner->editControlDidChangeValueByKeyboard();
}

void DateTimeEditElement::readOnlyStateChanged() {
  updateUIState();
}

void DateTimeEditElement::resetFields() {
  for (const auto& field : m_fields)
    field->removeEventHandler();
  m_fields.shrink(0);
}

void DateTimeEditElement::defaultEventHandler(Event* event) {
  // In case of control owner forward event to control, e.g. DOM
  // dispatchEvent method.
  if (DateTimeFieldElement* field = focusedField()) {
    field->defaultEventHandler(event);
    if (event->defaultHandled())
      return;
  }

  HTMLDivElement::defaultEventHandler(event);
}

void DateTimeEditElement::setValueAsDate(
    const LayoutParameters& layoutParameters,
    const DateComponents& date) {
  layout(layoutParameters, date);
  for (const auto& field : m_fields)
    field->setValueAsDate(date);
}

void DateTimeEditElement::setValueAsDateTimeFieldsState(
    const DateTimeFieldsState& dateTimeFieldsState) {
  for (const auto& field : m_fields)
    field->setValueAsDateTimeFieldsState(dateTimeFieldsState);
}

void DateTimeEditElement::setEmptyValue(
    const LayoutParameters& layoutParameters,
    const DateComponents& dateForReadOnlyField) {
  layout(layoutParameters, dateForReadOnlyField);
  for (const auto& field : m_fields)
    field->setEmptyValue(DateTimeFieldElement::DispatchNoEvent);
}

bool DateTimeEditElement::hasFocusedField() {
  return focusedFieldIndex() != invalidFieldIndex;
}

void DateTimeEditElement::setOnlyYearMonthDay(const DateComponents& date) {
  DCHECK_EQ(date.getType(), DateComponents::Date);

  if (!m_editControlOwner)
    return;

  DateTimeFieldsState dateTimeFieldsState = valueAsDateTimeFieldsState();
  dateTimeFieldsState.setYear(date.fullYear());
  dateTimeFieldsState.setMonth(date.month() + 1);
  dateTimeFieldsState.setDayOfMonth(date.monthDay());
  setValueAsDateTimeFieldsState(dateTimeFieldsState);
  m_editControlOwner->editControlValueChanged();
}

void DateTimeEditElement::stepDown() {
  if (DateTimeFieldElement* const field = focusedField())
    field->stepDown();
}

void DateTimeEditElement::stepUp() {
  if (DateTimeFieldElement* const field = focusedField())
    field->stepUp();
}

void DateTimeEditElement::updateUIState() {
  if (isDisabled()) {
    if (DateTimeFieldElement* field = focusedField())
      field->blur();
  }
}

String DateTimeEditElement::value() const {
  if (!m_editControlOwner)
    return emptyString;
  return m_editControlOwner->formatDateTimeFieldsState(
      valueAsDateTimeFieldsState());
}

DateTimeFieldsState DateTimeEditElement::valueAsDateTimeFieldsState() const {
  DateTimeFieldsState dateTimeFieldsState;
  for (const auto& field : m_fields)
    field->populateDateTimeFieldsState(dateTimeFieldsState);
  return dateTimeFieldsState;
}

}  // namespace blink
