// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/ShadowInterpolationFunctions.h"

#include "core/animation/CSSColorInterpolationType.h"
#include "core/animation/InterpolationValue.h"
#include "core/animation/LengthInterpolationFunctions.h"
#include "core/animation/NonInterpolableValue.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSShadowValue.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/ShadowData.h"
#include "platform/geometry/FloatPoint.h"
#include <memory>

namespace blink {

enum ShadowComponentIndex : unsigned {
  ShadowX,
  ShadowY,
  ShadowBlur,
  ShadowSpread,
  ShadowColor,
  ShadowComponentIndexCount,
};

class ShadowNonInterpolableValue : public NonInterpolableValue {
 public:
  ~ShadowNonInterpolableValue() final {}

  static PassRefPtr<ShadowNonInterpolableValue> create(
      ShadowStyle shadowStyle) {
    return adoptRef(new ShadowNonInterpolableValue(shadowStyle));
  }

  ShadowStyle style() const { return m_style; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  ShadowNonInterpolableValue(ShadowStyle shadowStyle) : m_style(shadowStyle) {}

  ShadowStyle m_style;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(ShadowNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(ShadowNonInterpolableValue);

bool ShadowInterpolationFunctions::nonInterpolableValuesAreCompatible(
    const NonInterpolableValue* a,
    const NonInterpolableValue* b) {
  return toShadowNonInterpolableValue(*a).style() ==
         toShadowNonInterpolableValue(*b).style();
}

PairwiseInterpolationValue ShadowInterpolationFunctions::maybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) {
  if (!nonInterpolableValuesAreCompatible(start.nonInterpolableValue.get(),
                                          end.nonInterpolableValue.get()))
    return nullptr;
  return PairwiseInterpolationValue(std::move(start.interpolableValue),
                                    std::move(end.interpolableValue),
                                    std::move(start.nonInterpolableValue));
}

InterpolationValue ShadowInterpolationFunctions::convertShadowData(
    const ShadowData& shadowData,
    double zoom) {
  std::unique_ptr<InterpolableList> interpolableList =
      InterpolableList::create(ShadowComponentIndexCount);
  interpolableList->set(ShadowX,
                        LengthInterpolationFunctions::createInterpolablePixels(
                            shadowData.x() / zoom));
  interpolableList->set(ShadowY,
                        LengthInterpolationFunctions::createInterpolablePixels(
                            shadowData.y() / zoom));
  interpolableList->set(ShadowBlur,
                        LengthInterpolationFunctions::createInterpolablePixels(
                            shadowData.blur() / zoom));
  interpolableList->set(ShadowSpread,
                        LengthInterpolationFunctions::createInterpolablePixels(
                            shadowData.spread() / zoom));
  interpolableList->set(
      ShadowColor,
      CSSColorInterpolationType::createInterpolableColor(shadowData.color()));
  return InterpolationValue(
      std::move(interpolableList),
      ShadowNonInterpolableValue::create(shadowData.style()));
}

InterpolationValue ShadowInterpolationFunctions::maybeConvertCSSValue(
    const CSSValue& value) {
  if (!value.isShadowValue())
    return nullptr;
  const CSSShadowValue& shadow = toCSSShadowValue(value);

  ShadowStyle style = Normal;
  if (shadow.style) {
    if (shadow.style->getValueID() == CSSValueInset)
      style = Inset;
    else
      return nullptr;
  }

  std::unique_ptr<InterpolableList> interpolableList =
      InterpolableList::create(ShadowComponentIndexCount);
  static_assert(ShadowX == 0, "Enum ordering check.");
  static_assert(ShadowY == 1, "Enum ordering check.");
  static_assert(ShadowBlur == 2, "Enum ordering check.");
  static_assert(ShadowSpread == 3, "Enum ordering check.");
  const CSSPrimitiveValue* lengths[] = {
      shadow.x.get(), shadow.y.get(), shadow.blur.get(), shadow.spread.get(),
  };
  for (size_t i = 0; i < WTF_ARRAY_LENGTH(lengths); i++) {
    if (lengths[i]) {
      InterpolationValue lengthField =
          LengthInterpolationFunctions::maybeConvertCSSValue(*lengths[i]);
      if (!lengthField)
        return nullptr;
      DCHECK(!lengthField.nonInterpolableValue);
      interpolableList->set(i, std::move(lengthField.interpolableValue));
    } else {
      interpolableList->set(
          i, LengthInterpolationFunctions::createInterpolablePixels(0));
    }
  }

  if (shadow.color) {
    std::unique_ptr<InterpolableValue> interpolableColor =
        CSSColorInterpolationType::maybeCreateInterpolableColor(*shadow.color);
    if (!interpolableColor)
      return nullptr;
    interpolableList->set(ShadowColor, std::move(interpolableColor));
  } else {
    interpolableList->set(ShadowColor,
                          CSSColorInterpolationType::createInterpolableColor(
                              StyleColor::currentColor()));
  }

  return InterpolationValue(std::move(interpolableList),
                            ShadowNonInterpolableValue::create(style));
}

std::unique_ptr<InterpolableValue>
ShadowInterpolationFunctions::createNeutralInterpolableValue() {
  return convertShadowData(ShadowData::neutralValue(), 1).interpolableValue;
}

void ShadowInterpolationFunctions::composite(
    std::unique_ptr<InterpolableValue>& underlyingInterpolableValue,
    RefPtr<NonInterpolableValue>& underlyingNonInterpolableValue,
    double underlyingFraction,
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue* nonInterpolableValue) {
  DCHECK(nonInterpolableValuesAreCompatible(
      underlyingNonInterpolableValue.get(), nonInterpolableValue));
  InterpolableList& underlyingInterpolableList =
      toInterpolableList(*underlyingInterpolableValue);
  const InterpolableList& interpolableList =
      toInterpolableList(interpolableValue);
  underlyingInterpolableList.scaleAndAdd(underlyingFraction, interpolableList);
}

ShadowData ShadowInterpolationFunctions::createShadowData(
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue* nonInterpolableValue,
    const StyleResolverState& state) {
  const InterpolableList& interpolableList =
      toInterpolableList(interpolableValue);
  const ShadowNonInterpolableValue& shadowNonInterpolableValue =
      toShadowNonInterpolableValue(*nonInterpolableValue);
  const CSSToLengthConversionData& conversionData =
      state.cssToLengthConversionData();
  Length shadowX = LengthInterpolationFunctions::createLength(
      *interpolableList.get(ShadowX), nullptr, conversionData, ValueRangeAll);
  Length shadowY = LengthInterpolationFunctions::createLength(
      *interpolableList.get(ShadowY), nullptr, conversionData, ValueRangeAll);
  Length shadowBlur = LengthInterpolationFunctions::createLength(
      *interpolableList.get(ShadowBlur), nullptr, conversionData,
      ValueRangeNonNegative);
  Length shadowSpread = LengthInterpolationFunctions::createLength(
      *interpolableList.get(ShadowSpread), nullptr, conversionData,
      ValueRangeAll);
  DCHECK(shadowX.isFixed() && shadowY.isFixed() && shadowBlur.isFixed() &&
         shadowSpread.isFixed());
  return ShadowData(FloatPoint(shadowX.value(), shadowY.value()),
                    shadowBlur.value(), shadowSpread.value(),
                    shadowNonInterpolableValue.style(),
                    CSSColorInterpolationType::resolveInterpolableColor(
                        *interpolableList.get(ShadowColor), state));
}

}  // namespace blink
