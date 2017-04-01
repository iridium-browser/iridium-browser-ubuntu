// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSTextIndentInterpolationType.h"

#include "core/animation/LengthInterpolationFunctions.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/ComputedStyle.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

struct IndentMode {
  IndentMode(const TextIndentLine line, const TextIndentType type)
      : line(line), type(type) {}
  explicit IndentMode(const ComputedStyle& style)
      : line(style.getTextIndentLine()), type(style.getTextIndentType()) {}

  bool operator==(const IndentMode& other) const {
    return line == other.line && type == other.type;
  }
  bool operator!=(const IndentMode& other) const { return !(*this == other); }

  const TextIndentLine line;
  const TextIndentType type;
};

}  // namespace

class CSSTextIndentNonInterpolableValue : public NonInterpolableValue {
 public:
  static PassRefPtr<CSSTextIndentNonInterpolableValue> create(
      PassRefPtr<NonInterpolableValue> lengthNonInterpolableValue,
      const IndentMode& mode) {
    return adoptRef(new CSSTextIndentNonInterpolableValue(
        std::move(lengthNonInterpolableValue), mode));
  }

  const NonInterpolableValue* lengthNonInterpolableValue() const {
    return m_lengthNonInterpolableValue.get();
  }
  RefPtr<NonInterpolableValue>& lengthNonInterpolableValue() {
    return m_lengthNonInterpolableValue;
  }
  const IndentMode& mode() const { return m_mode; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSTextIndentNonInterpolableValue(
      PassRefPtr<NonInterpolableValue> lengthNonInterpolableValue,
      const IndentMode& mode)
      : m_lengthNonInterpolableValue(lengthNonInterpolableValue),
        m_mode(mode) {}

  RefPtr<NonInterpolableValue> m_lengthNonInterpolableValue;
  const IndentMode m_mode;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSTextIndentNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSTextIndentNonInterpolableValue);

namespace {

class UnderlyingIndentModeChecker
    : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<UnderlyingIndentModeChecker> create(
      const IndentMode& mode) {
    return WTF::wrapUnique(new UnderlyingIndentModeChecker(mode));
  }

  bool isValid(const InterpolationEnvironment&,
               const InterpolationValue& underlying) const final {
    return m_mode ==
           toCSSTextIndentNonInterpolableValue(*underlying.nonInterpolableValue)
               .mode();
  }

 private:
  UnderlyingIndentModeChecker(const IndentMode& mode) : m_mode(mode) {}

  const IndentMode m_mode;
};

class InheritedIndentModeChecker : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<InheritedIndentModeChecker> create(
      const IndentMode& mode) {
    return WTF::wrapUnique(new InheritedIndentModeChecker(mode));
  }

  bool isValid(const InterpolationEnvironment& environment,
               const InterpolationValue&) const final {
    return m_mode == IndentMode(*environment.state().parentStyle());
  }

 private:
  InheritedIndentModeChecker(const IndentMode& mode) : m_mode(mode) {}

  const IndentMode m_mode;
};

InterpolationValue createValue(const Length& length,
                               const IndentMode& mode,
                               double zoom) {
  InterpolationValue convertedLength =
      LengthInterpolationFunctions::maybeConvertLength(length, zoom);
  DCHECK(convertedLength);
  return InterpolationValue(
      std::move(convertedLength.interpolableValue),
      CSSTextIndentNonInterpolableValue::create(
          std::move(convertedLength.nonInterpolableValue), mode));
}

}  // namespace

InterpolationValue CSSTextIndentInterpolationType::maybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversionCheckers) const {
  IndentMode mode =
      toCSSTextIndentNonInterpolableValue(*underlying.nonInterpolableValue)
          .mode();
  conversionCheckers.push_back(UnderlyingIndentModeChecker::create(mode));
  return createValue(Length(0, Fixed), mode, 1);
}

InterpolationValue CSSTextIndentInterpolationType::maybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  IndentMode mode(ComputedStyle::initialTextIndentLine(),
                  ComputedStyle::initialTextIndentType());
  return createValue(ComputedStyle::initialTextIndent(), mode, 1);
}

InterpolationValue CSSTextIndentInterpolationType::maybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversionCheckers) const {
  const ComputedStyle& parentStyle = *state.parentStyle();
  IndentMode mode(parentStyle);
  conversionCheckers.push_back(InheritedIndentModeChecker::create(mode));
  return createValue(parentStyle.textIndent(), mode,
                     parentStyle.effectiveZoom());
}

InterpolationValue CSSTextIndentInterpolationType::maybeConvertValue(
    const CSSValue& value,
    const StyleResolverState&,
    ConversionCheckers&) const {
  InterpolationValue length = nullptr;
  TextIndentLine line = ComputedStyle::initialTextIndentLine();
  TextIndentType type = ComputedStyle::initialTextIndentType();

  for (const auto& item : toCSSValueList(value)) {
    if (item->isIdentifierValue() &&
        toCSSIdentifierValue(*item).getValueID() == CSSValueEachLine)
      line = TextIndentEachLine;
    else if (item->isIdentifierValue() &&
             toCSSIdentifierValue(*item).getValueID() == CSSValueHanging)
      type = TextIndentHanging;
    else
      length = LengthInterpolationFunctions::maybeConvertCSSValue(*item);
  }
  DCHECK(length);

  return InterpolationValue(
      std::move(length.interpolableValue),
      CSSTextIndentNonInterpolableValue::create(
          std::move(length.nonInterpolableValue), IndentMode(line, type)));
}

InterpolationValue
CSSTextIndentInterpolationType::maybeConvertStandardPropertyUnderlyingValue(
    const StyleResolverState& state) const {
  const ComputedStyle& style = *state.style();
  return createValue(style.textIndent(), IndentMode(style),
                     style.effectiveZoom());
}

PairwiseInterpolationValue CSSTextIndentInterpolationType::maybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  CSSTextIndentNonInterpolableValue& startNonInterpolableValue =
      toCSSTextIndentNonInterpolableValue(*start.nonInterpolableValue);
  CSSTextIndentNonInterpolableValue& endNonInterpolableValue =
      toCSSTextIndentNonInterpolableValue(*end.nonInterpolableValue);

  if (startNonInterpolableValue.mode() != endNonInterpolableValue.mode())
    return nullptr;

  PairwiseInterpolationValue result =
      LengthInterpolationFunctions::mergeSingles(
          InterpolationValue(
              std::move(start.interpolableValue),
              std::move(
                  startNonInterpolableValue.lengthNonInterpolableValue())),
          InterpolationValue(
              std::move(end.interpolableValue),
              std::move(endNonInterpolableValue.lengthNonInterpolableValue())));
  result.nonInterpolableValue = CSSTextIndentNonInterpolableValue::create(
      std::move(result.nonInterpolableValue), startNonInterpolableValue.mode());
  return result;
}

void CSSTextIndentInterpolationType::composite(
    UnderlyingValueOwner& underlyingValueOwner,
    double underlyingFraction,
    const InterpolationValue& value,
    double interpolationFraction) const {
  const IndentMode& underlyingMode =
      toCSSTextIndentNonInterpolableValue(
          *underlyingValueOwner.value().nonInterpolableValue)
          .mode();
  const CSSTextIndentNonInterpolableValue& nonInterpolableValue =
      toCSSTextIndentNonInterpolableValue(*value.nonInterpolableValue);
  const IndentMode& mode = nonInterpolableValue.mode();

  if (underlyingMode != mode) {
    underlyingValueOwner.set(*this, value);
    return;
  }

  LengthInterpolationFunctions::composite(
      underlyingValueOwner.mutableValue().interpolableValue,
      toCSSTextIndentNonInterpolableValue(
          *underlyingValueOwner.mutableValue().nonInterpolableValue)
          .lengthNonInterpolableValue(),
      underlyingFraction, *value.interpolableValue,
      nonInterpolableValue.lengthNonInterpolableValue());
}

void CSSTextIndentInterpolationType::applyStandardPropertyValue(
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue* nonInterpolableValue,
    StyleResolverState& state) const {
  const CSSTextIndentNonInterpolableValue& cssTextIndentNonInterpolableValue =
      toCSSTextIndentNonInterpolableValue(*nonInterpolableValue);
  ComputedStyle& style = *state.style();
  style.setTextIndent(LengthInterpolationFunctions::createLength(
      interpolableValue,
      cssTextIndentNonInterpolableValue.lengthNonInterpolableValue(),
      state.cssToLengthConversionData(), ValueRangeAll));

  const IndentMode& mode = cssTextIndentNonInterpolableValue.mode();
  style.setTextIndentLine(mode.line);
  style.setTextIndentType(mode.type);
}

}  // namespace blink
