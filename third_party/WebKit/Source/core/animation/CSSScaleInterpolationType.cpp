// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSScaleInterpolationType.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/StyleResolverState.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

struct Scale {
  Scale(double x, double y, double z) { init(x, y, z); }
  explicit Scale(const ScaleTransformOperation* scale) {
    if (scale)
      init(scale->x(), scale->y(), scale->z());
    else
      init(1, 1, 1);
  }
  explicit Scale(const InterpolableValue& value) {
    const InterpolableList& list = toInterpolableList(value);
    init(toInterpolableNumber(*list.get(0)).value(),
         toInterpolableNumber(*list.get(1)).value(),
         toInterpolableNumber(*list.get(2)).value());
  }

  void init(double x, double y, double z) {
    array[0] = x;
    array[1] = y;
    array[2] = z;
  }

  InterpolationValue createInterpolationValue() const;

  bool operator==(const Scale& other) const {
    for (size_t i = 0; i < 3; i++) {
      if (array[i] != other.array[i])
        return false;
    }
    return true;
  }

  double array[3];
};

class InheritedScaleChecker : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<InheritedScaleChecker> create(const Scale& scale) {
    return WTF::wrapUnique(new InheritedScaleChecker(scale));
  }

 private:
  InheritedScaleChecker(const Scale& scale) : m_scale(scale) {}

  bool isValid(const InterpolationEnvironment& environment,
               const InterpolationValue&) const final {
    return m_scale == Scale(environment.state().parentStyle()->scale());
  }

  const Scale m_scale;
};

}  // namespace

class CSSScaleNonInterpolableValue : public NonInterpolableValue {
 public:
  ~CSSScaleNonInterpolableValue() final {}

  static PassRefPtr<CSSScaleNonInterpolableValue> create(const Scale& scale) {
    return adoptRef(
        new CSSScaleNonInterpolableValue(scale, scale, false, false));
  }

  static PassRefPtr<CSSScaleNonInterpolableValue> merge(
      const CSSScaleNonInterpolableValue& start,
      const CSSScaleNonInterpolableValue& end) {
    return adoptRef(new CSSScaleNonInterpolableValue(start.start(), end.end(),
                                                     start.isStartAdditive(),
                                                     end.isEndAdditive()));
  }

  const Scale& start() const { return m_start; }
  const Scale& end() const { return m_end; }
  bool isStartAdditive() const { return m_isStartAdditive; }
  bool isEndAdditive() const { return m_isEndAdditive; }

  void setIsAdditive() {
    m_isStartAdditive = true;
    m_isEndAdditive = true;
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSScaleNonInterpolableValue(const Scale& start,
                               const Scale& end,
                               bool isStartAdditive,
                               bool isEndAdditive)
      : m_start(start),
        m_end(end),
        m_isStartAdditive(isStartAdditive),
        m_isEndAdditive(isEndAdditive) {}

  const Scale m_start;
  const Scale m_end;
  bool m_isStartAdditive;
  bool m_isEndAdditive;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSScaleNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSScaleNonInterpolableValue);

InterpolationValue Scale::createInterpolationValue() const {
  std::unique_ptr<InterpolableList> list = InterpolableList::create(3);
  for (size_t i = 0; i < 3; i++)
    list->set(i, InterpolableNumber::create(array[i]));
  return InterpolationValue(std::move(list),
                            CSSScaleNonInterpolableValue::create(*this));
}

InterpolationValue CSSScaleInterpolationType::maybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return Scale(1, 1, 1).createInterpolationValue();
}

InterpolationValue CSSScaleInterpolationType::maybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  return Scale(1, 1, 1).createInterpolationValue();
}

InterpolationValue CSSScaleInterpolationType::maybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversionCheckers) const {
  Scale inheritedScale(state.parentStyle()->scale());
  conversionCheckers.push_back(InheritedScaleChecker::create(inheritedScale));
  return inheritedScale.createInterpolationValue();
}

InterpolationValue CSSScaleInterpolationType::maybeConvertValue(
    const CSSValue& value,
    const StyleResolverState&,
    ConversionCheckers&) const {
  Scale scale(1, 1, 1);
  if (!value.isBaseValueList())
    return nullptr;

  const CSSValueList& list = toCSSValueList(value);
  if (list.length() < 1 || list.length() > 3)
    return nullptr;

  for (size_t i = 0; i < list.length(); i++) {
    const CSSValue& item = list.item(i);
    if (!item.isPrimitiveValue() || !toCSSPrimitiveValue(item).isNumber())
      return nullptr;
    scale.array[i] = toCSSPrimitiveValue(item).getDoubleValue();
  }

  return scale.createInterpolationValue();
}

void CSSScaleInterpolationType::additiveKeyframeHook(
    InterpolationValue& value) const {
  toCSSScaleNonInterpolableValue(*value.nonInterpolableValue).setIsAdditive();
}

PairwiseInterpolationValue CSSScaleInterpolationType::maybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  return PairwiseInterpolationValue(
      std::move(start.interpolableValue), std::move(end.interpolableValue),
      CSSScaleNonInterpolableValue::merge(
          toCSSScaleNonInterpolableValue(*start.nonInterpolableValue),
          toCSSScaleNonInterpolableValue(*end.nonInterpolableValue)));
}

InterpolationValue
CSSScaleInterpolationType::maybeConvertStandardPropertyUnderlyingValue(
    const StyleResolverState& state) const {
  return Scale(state.style()->scale()).createInterpolationValue();
}

void CSSScaleInterpolationType::composite(
    UnderlyingValueOwner& underlyingValueOwner,
    double underlyingFraction,
    const InterpolationValue& value,
    double interpolationFraction) const {
  const CSSScaleNonInterpolableValue& metadata =
      toCSSScaleNonInterpolableValue(*value.nonInterpolableValue);
  DCHECK(metadata.isStartAdditive() || metadata.isEndAdditive());

  InterpolableList& underlyingList = toInterpolableList(
      *underlyingValueOwner.mutableValue().interpolableValue);
  for (size_t i = 0; i < 3; i++) {
    InterpolableNumber& underlying =
        toInterpolableNumber(*underlyingList.getMutable(i));
    double start = metadata.start().array[i] *
                   (metadata.isStartAdditive() ? underlying.value() : 1);
    double end = metadata.end().array[i] *
                 (metadata.isEndAdditive() ? underlying.value() : 1);
    underlying.set(blend(start, end, interpolationFraction));
  }
}

void CSSScaleInterpolationType::applyStandardPropertyValue(
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue*,
    StyleResolverState& state) const {
  Scale scale(interpolableValue);
  state.style()->setScale(ScaleTransformOperation::create(
      scale.array[0], scale.array[1], scale.array[2],
      TransformOperation::Scale3D));
}

}  // namespace blink
