// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/FilterInterpolationFunctions.h"

#include "core/animation/LengthInterpolationFunctions.h"
#include "core/animation/ShadowInterpolationFunctions.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/resolver/FilterOperationResolver.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/FilterOperations.h"
#include "core/style/ShadowData.h"
#include <memory>

namespace blink {

class FilterNonInterpolableValue : public NonInterpolableValue {
 public:
  static PassRefPtr<FilterNonInterpolableValue> create(
      FilterOperation::OperationType type,
      PassRefPtr<NonInterpolableValue> typeNonInterpolableValue) {
    return adoptRef(new FilterNonInterpolableValue(
        type, std::move(typeNonInterpolableValue)));
  }

  FilterOperation::OperationType operationType() const { return m_type; }
  const NonInterpolableValue* typeNonInterpolableValue() const {
    return m_typeNonInterpolableValue.get();
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  FilterNonInterpolableValue(
      FilterOperation::OperationType type,
      PassRefPtr<NonInterpolableValue> typeNonInterpolableValue)
      : m_type(type), m_typeNonInterpolableValue(typeNonInterpolableValue) {}

  const FilterOperation::OperationType m_type;
  RefPtr<NonInterpolableValue> m_typeNonInterpolableValue;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(FilterNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(FilterNonInterpolableValue);

namespace {

double defaultParameter(FilterOperation::OperationType type) {
  switch (type) {
    case FilterOperation::BRIGHTNESS:
    case FilterOperation::GRAYSCALE:
    case FilterOperation::SATURATE:
    case FilterOperation::SEPIA:
      return 1;

    case FilterOperation::CONTRAST:
    case FilterOperation::HUE_ROTATE:
    case FilterOperation::INVERT:
    case FilterOperation::OPACITY:
      return 0;

    default:
      NOTREACHED();
      return 0;
  }
}

double clampParameter(double value, FilterOperation::OperationType type) {
  switch (type) {
    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST:
    case FilterOperation::SATURATE:
      return clampTo<double>(value, 0);

    case FilterOperation::GRAYSCALE:
    case FilterOperation::INVERT:
    case FilterOperation::OPACITY:
    case FilterOperation::SEPIA:
      return clampTo<double>(value, 0, 1);

    case FilterOperation::HUE_ROTATE:
      return value;

    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace

InterpolationValue FilterInterpolationFunctions::maybeConvertCSSFilter(
    const CSSValue& value) {
  if (value.isURIValue())
    return nullptr;

  const CSSFunctionValue& filter = toCSSFunctionValue(value);
  DCHECK_LE(filter.length(), 1u);
  FilterOperation::OperationType type =
      FilterOperationResolver::filterOperationForType(filter.functionType());
  InterpolationValue result = nullptr;

  switch (type) {
    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST:
    case FilterOperation::GRAYSCALE:
    case FilterOperation::INVERT:
    case FilterOperation::OPACITY:
    case FilterOperation::SATURATE:
    case FilterOperation::SEPIA: {
      double amount = defaultParameter(type);
      if (filter.length() == 1) {
        const CSSPrimitiveValue& firstValue =
            toCSSPrimitiveValue(filter.item(0));
        amount = firstValue.getDoubleValue();
        if (firstValue.isPercentage())
          amount /= 100;
      }
      result.interpolableValue = InterpolableNumber::create(amount);
      break;
    }

    case FilterOperation::HUE_ROTATE: {
      double angle = defaultParameter(type);
      if (filter.length() == 1)
        angle = toCSSPrimitiveValue(filter.item(0)).computeDegrees();
      result.interpolableValue = InterpolableNumber::create(angle);
      break;
    }

    case FilterOperation::BLUR: {
      if (filter.length() == 0)
        result.interpolableValue =
            LengthInterpolationFunctions::createNeutralInterpolableValue();
      else
        result =
            LengthInterpolationFunctions::maybeConvertCSSValue(filter.item(0));
      break;
    }

    case FilterOperation::DROP_SHADOW: {
      result =
          ShadowInterpolationFunctions::maybeConvertCSSValue(filter.item(0));
      break;
    }

    default:
      NOTREACHED();
      return nullptr;
  }

  if (!result)
    return nullptr;

  result.nonInterpolableValue = FilterNonInterpolableValue::create(
      type, std::move(result.nonInterpolableValue));
  return result;
}

InterpolationValue FilterInterpolationFunctions::maybeConvertFilter(
    const FilterOperation& filter,
    double zoom) {
  InterpolationValue result = nullptr;

  switch (filter.type()) {
    case FilterOperation::GRAYSCALE:
    case FilterOperation::HUE_ROTATE:
    case FilterOperation::SATURATE:
    case FilterOperation::SEPIA:
      result.interpolableValue = InterpolableNumber::create(
          toBasicColorMatrixFilterOperation(filter).amount());
      break;

    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST:
    case FilterOperation::INVERT:
    case FilterOperation::OPACITY:
      result.interpolableValue = InterpolableNumber::create(
          toBasicComponentTransferFilterOperation(filter).amount());
      break;

    case FilterOperation::BLUR:
      result = LengthInterpolationFunctions::maybeConvertLength(
          toBlurFilterOperation(filter).stdDeviation(), zoom);
      break;

    case FilterOperation::DROP_SHADOW: {
      result = ShadowInterpolationFunctions::convertShadowData(
          toDropShadowFilterOperation(filter).shadow(), zoom);
      break;
    }

    case FilterOperation::REFERENCE:
      return nullptr;

    default:
      NOTREACHED();
      return nullptr;
  }

  if (!result)
    return nullptr;

  result.nonInterpolableValue = FilterNonInterpolableValue::create(
      filter.type(), std::move(result.nonInterpolableValue));
  return result;
}

std::unique_ptr<InterpolableValue>
FilterInterpolationFunctions::createNoneValue(
    const NonInterpolableValue& untypedValue) {
  switch (toFilterNonInterpolableValue(untypedValue).operationType()) {
    case FilterOperation::GRAYSCALE:
    case FilterOperation::INVERT:
    case FilterOperation::SEPIA:
    case FilterOperation::HUE_ROTATE:
      return InterpolableNumber::create(0);

    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST:
    case FilterOperation::OPACITY:
    case FilterOperation::SATURATE:
      return InterpolableNumber::create(1);

    case FilterOperation::BLUR:
      return LengthInterpolationFunctions::createNeutralInterpolableValue();

    case FilterOperation::DROP_SHADOW:
      return ShadowInterpolationFunctions::createNeutralInterpolableValue();

    default:
      NOTREACHED();
      return nullptr;
  }
}

bool FilterInterpolationFunctions::filtersAreCompatible(
    const NonInterpolableValue& a,
    const NonInterpolableValue& b) {
  return toFilterNonInterpolableValue(a).operationType() ==
         toFilterNonInterpolableValue(b).operationType();
}

FilterOperation* FilterInterpolationFunctions::createFilter(
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue& untypedNonInterpolableValue,
    const StyleResolverState& state) {
  const FilterNonInterpolableValue& nonInterpolableValue =
      toFilterNonInterpolableValue(untypedNonInterpolableValue);
  FilterOperation::OperationType type = nonInterpolableValue.operationType();

  switch (type) {
    case FilterOperation::GRAYSCALE:
    case FilterOperation::HUE_ROTATE:
    case FilterOperation::SATURATE:
    case FilterOperation::SEPIA: {
      double value =
          clampParameter(toInterpolableNumber(interpolableValue).value(), type);
      return BasicColorMatrixFilterOperation::create(value, type);
    }

    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST:
    case FilterOperation::INVERT:
    case FilterOperation::OPACITY: {
      double value =
          clampParameter(toInterpolableNumber(interpolableValue).value(), type);
      return BasicComponentTransferFilterOperation::create(value, type);
    }

    case FilterOperation::BLUR: {
      Length stdDeviation = LengthInterpolationFunctions::createLength(
          interpolableValue, nonInterpolableValue.typeNonInterpolableValue(),
          state.cssToLengthConversionData(), ValueRangeNonNegative);
      return BlurFilterOperation::create(stdDeviation);
    }

    case FilterOperation::DROP_SHADOW: {
      ShadowData shadowData = ShadowInterpolationFunctions::createShadowData(
          interpolableValue, nonInterpolableValue.typeNonInterpolableValue(),
          state);
      if (shadowData.color().isCurrentColor())
        shadowData.overrideColor(Color::black);
      return DropShadowFilterOperation::create(shadowData);
    }

    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace blink
