// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SVGTransformListInterpolationType.h"

#include "core/animation/InterpolableValue.h"
#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/NonInterpolableValue.h"
#include "core/animation/StringKeyframe.h"
#include "core/svg/SVGTransform.h"
#include "core/svg/SVGTransformList.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class SVGTransformNonInterpolableValue : public NonInterpolableValue {
 public:
  virtual ~SVGTransformNonInterpolableValue() {}

  static PassRefPtr<SVGTransformNonInterpolableValue> create(
      Vector<SVGTransformType>& transformTypes) {
    return adoptRef(new SVGTransformNonInterpolableValue(transformTypes));
  }

  const Vector<SVGTransformType>& transformTypes() const {
    return m_transformTypes;
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  SVGTransformNonInterpolableValue(Vector<SVGTransformType>& transformTypes) {
    m_transformTypes.swap(transformTypes);
  }

  Vector<SVGTransformType> m_transformTypes;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(SVGTransformNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(SVGTransformNonInterpolableValue);

namespace {

std::unique_ptr<InterpolableValue> translateToInterpolableValue(
    SVGTransform* transform) {
  FloatPoint translate = transform->translate();
  std::unique_ptr<InterpolableList> result = InterpolableList::create(2);
  result->set(0, InterpolableNumber::create(translate.x()));
  result->set(1, InterpolableNumber::create(translate.y()));
  return std::move(result);
}

SVGTransform* translateFromInterpolableValue(const InterpolableValue& value) {
  const InterpolableList& list = toInterpolableList(value);

  SVGTransform* transform = SVGTransform::create(kSvgTransformTranslate);
  transform->setTranslate(toInterpolableNumber(list.get(0))->value(),
                          toInterpolableNumber(list.get(1))->value());
  return transform;
}

std::unique_ptr<InterpolableValue> scaleToInterpolableValue(
    SVGTransform* transform) {
  FloatSize scale = transform->scale();
  std::unique_ptr<InterpolableList> result = InterpolableList::create(2);
  result->set(0, InterpolableNumber::create(scale.width()));
  result->set(1, InterpolableNumber::create(scale.height()));
  return std::move(result);
}

SVGTransform* scaleFromInterpolableValue(const InterpolableValue& value) {
  const InterpolableList& list = toInterpolableList(value);

  SVGTransform* transform = SVGTransform::create(kSvgTransformScale);
  transform->setScale(toInterpolableNumber(list.get(0))->value(),
                      toInterpolableNumber(list.get(1))->value());
  return transform;
}

std::unique_ptr<InterpolableValue> rotateToInterpolableValue(
    SVGTransform* transform) {
  FloatPoint rotationCenter = transform->rotationCenter();
  std::unique_ptr<InterpolableList> result = InterpolableList::create(3);
  result->set(0, InterpolableNumber::create(transform->angle()));
  result->set(1, InterpolableNumber::create(rotationCenter.x()));
  result->set(2, InterpolableNumber::create(rotationCenter.y()));
  return std::move(result);
}

SVGTransform* rotateFromInterpolableValue(const InterpolableValue& value) {
  const InterpolableList& list = toInterpolableList(value);

  SVGTransform* transform = SVGTransform::create(kSvgTransformRotate);
  transform->setRotate(toInterpolableNumber(list.get(0))->value(),
                       toInterpolableNumber(list.get(1))->value(),
                       toInterpolableNumber(list.get(2))->value());
  return transform;
}

std::unique_ptr<InterpolableValue> skewXToInterpolableValue(
    SVGTransform* transform) {
  return InterpolableNumber::create(transform->angle());
}

SVGTransform* skewXFromInterpolableValue(const InterpolableValue& value) {
  SVGTransform* transform = SVGTransform::create(kSvgTransformSkewx);
  transform->setSkewX(toInterpolableNumber(value).value());
  return transform;
}

std::unique_ptr<InterpolableValue> skewYToInterpolableValue(
    SVGTransform* transform) {
  return InterpolableNumber::create(transform->angle());
}

SVGTransform* skewYFromInterpolableValue(const InterpolableValue& value) {
  SVGTransform* transform = SVGTransform::create(kSvgTransformSkewy);
  transform->setSkewY(toInterpolableNumber(value).value());
  return transform;
}

std::unique_ptr<InterpolableValue> toInterpolableValue(
    SVGTransform* transform,
    SVGTransformType transformType) {
  switch (transformType) {
    case kSvgTransformTranslate:
      return translateToInterpolableValue(transform);
    case kSvgTransformScale:
      return scaleToInterpolableValue(transform);
    case kSvgTransformRotate:
      return rotateToInterpolableValue(transform);
    case kSvgTransformSkewx:
      return skewXToInterpolableValue(transform);
    case kSvgTransformSkewy:
      return skewYToInterpolableValue(transform);
    case kSvgTransformMatrix:
    case kSvgTransformUnknown:
      NOTREACHED();
  }
  NOTREACHED();
  return nullptr;
}

SVGTransform* fromInterpolableValue(const InterpolableValue& value,
                                    SVGTransformType transformType) {
  switch (transformType) {
    case kSvgTransformTranslate:
      return translateFromInterpolableValue(value);
    case kSvgTransformScale:
      return scaleFromInterpolableValue(value);
    case kSvgTransformRotate:
      return rotateFromInterpolableValue(value);
    case kSvgTransformSkewx:
      return skewXFromInterpolableValue(value);
    case kSvgTransformSkewy:
      return skewYFromInterpolableValue(value);
    case kSvgTransformMatrix:
    case kSvgTransformUnknown:
      NOTREACHED();
  }
  NOTREACHED();
  return nullptr;
}

const Vector<SVGTransformType>& getTransformTypes(
    const InterpolationValue& value) {
  return toSVGTransformNonInterpolableValue(*value.nonInterpolableValue)
      .transformTypes();
}

class SVGTransformListChecker : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<SVGTransformListChecker> create(
      const InterpolationValue& underlying) {
    return WTF::wrapUnique(new SVGTransformListChecker(underlying));
  }

  bool isValid(const InterpolationEnvironment&,
               const InterpolationValue& underlying) const final {
    // TODO(suzyh): change maybeConvertSingle so we don't have to recalculate
    // for changes to the interpolable values
    if (!underlying && !m_underlying)
      return true;
    if (!underlying || !m_underlying)
      return false;
    return m_underlying.interpolableValue->equals(
               *underlying.interpolableValue) &&
           getTransformTypes(m_underlying) == getTransformTypes(underlying);
  }

 private:
  SVGTransformListChecker(const InterpolationValue& underlying)
      : m_underlying(underlying.clone()) {}

  const InterpolationValue m_underlying;
};

}  // namespace

InterpolationValue SVGTransformListInterpolationType::maybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  NOTREACHED();
  // This function is no longer called, because maybeConvertSingle has been
  // overridden.
  return nullptr;
}

InterpolationValue SVGTransformListInterpolationType::maybeConvertSVGValue(
    const SVGPropertyBase& svgValue) const {
  if (svgValue.type() != AnimatedTransformList)
    return nullptr;

  const SVGTransformList& svgList = toSVGTransformList(svgValue);
  std::unique_ptr<InterpolableList> result =
      InterpolableList::create(svgList.length());

  Vector<SVGTransformType> transformTypes;
  for (size_t i = 0; i < svgList.length(); i++) {
    const SVGTransform* transform = svgList.at(i);
    SVGTransformType transformType(transform->transformType());
    if (transformType == kSvgTransformMatrix) {
      // TODO(ericwilligers): Support matrix interpolation.
      return nullptr;
    }
    result->set(i, toInterpolableValue(transform->clone(), transformType));
    transformTypes.push_back(transformType);
  }
  return InterpolationValue(
      std::move(result),
      SVGTransformNonInterpolableValue::create(transformTypes));
}

InterpolationValue SVGTransformListInterpolationType::maybeConvertSingle(
    const PropertySpecificKeyframe& keyframe,
    const InterpolationEnvironment& environment,
    const InterpolationValue& underlying,
    ConversionCheckers& conversionCheckers) const {
  Vector<SVGTransformType> types;
  Vector<std::unique_ptr<InterpolableValue>> interpolableParts;

  if (keyframe.composite() == EffectModel::CompositeAdd) {
    if (underlying) {
      types.appendVector(getTransformTypes(underlying));
      interpolableParts.push_back(underlying.interpolableValue->clone());
    }
    conversionCheckers.push_back(SVGTransformListChecker::create(underlying));
  } else {
    DCHECK(!keyframe.isNeutral());
  }

  if (!keyframe.isNeutral()) {
    SVGPropertyBase* svgValue = environment.svgBaseValue().cloneForAnimation(
        toSVGPropertySpecificKeyframe(keyframe).value());
    InterpolationValue value = maybeConvertSVGValue(*svgValue);
    if (!value)
      return nullptr;
    types.appendVector(getTransformTypes(value));
    interpolableParts.push_back(std::move(value.interpolableValue));
  }

  std::unique_ptr<InterpolableList> interpolableList =
      InterpolableList::create(types.size());
  size_t interpolableListIndex = 0;
  for (auto& part : interpolableParts) {
    InterpolableList& list = toInterpolableList(*part);
    for (size_t i = 0; i < list.length(); ++i) {
      interpolableList->set(interpolableListIndex,
                            std::move(list.getMutable(i)));
      ++interpolableListIndex;
    }
  }

  return InterpolationValue(std::move(interpolableList),
                            SVGTransformNonInterpolableValue::create(types));
}

SVGPropertyBase* SVGTransformListInterpolationType::appliedSVGValue(
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue* nonInterpolableValue) const {
  SVGTransformList* result = SVGTransformList::create();
  const InterpolableList& list = toInterpolableList(interpolableValue);
  const Vector<SVGTransformType>& transformTypes =
      toSVGTransformNonInterpolableValue(nonInterpolableValue)
          ->transformTypes();
  for (size_t i = 0; i < list.length(); ++i)
    result->append(fromInterpolableValue(*list.get(i), transformTypes.at(i)));
  return result;
}

PairwiseInterpolationValue SVGTransformListInterpolationType::maybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  if (getTransformTypes(start) != getTransformTypes(end))
    return nullptr;

  return PairwiseInterpolationValue(std::move(start.interpolableValue),
                                    std::move(end.interpolableValue),
                                    std::move(end.nonInterpolableValue));
}

void SVGTransformListInterpolationType::composite(
    UnderlyingValueOwner& underlyingValueOwner,
    double underlyingFraction,
    const InterpolationValue& value,
    double interpolationFraction) const {
  underlyingValueOwner.set(*this, value);
}

}  // namespace blink
