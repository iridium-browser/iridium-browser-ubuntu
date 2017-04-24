// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSImageListInterpolationType.h"

#include "core/animation/CSSImageInterpolationType.h"
#include "core/animation/ImageListPropertyFunctions.h"
#include "core/animation/ListInterpolationFunctions.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/StyleResolverState.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class UnderlyingImageListChecker : public InterpolationType::ConversionChecker {
 public:
  ~UnderlyingImageListChecker() final {}

  static std::unique_ptr<UnderlyingImageListChecker> create(
      const InterpolationValue& underlying) {
    return WTF::wrapUnique(new UnderlyingImageListChecker(underlying));
  }

 private:
  UnderlyingImageListChecker(const InterpolationValue& underlying)
      : m_underlying(underlying.clone()) {}

  bool isValid(const InterpolationEnvironment&,
               const InterpolationValue& underlying) const final {
    return ListInterpolationFunctions::equalValues(
        m_underlying, underlying,
        CSSImageInterpolationType::equalNonInterpolableValues);
  }

  const InterpolationValue m_underlying;
};

InterpolationValue CSSImageListInterpolationType::maybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversionCheckers) const {
  conversionCheckers.push_back(UnderlyingImageListChecker::create(underlying));
  return underlying.clone();
}

InterpolationValue CSSImageListInterpolationType::maybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers& conversionCheckers) const {
  StyleImageList initialImageList;
  ImageListPropertyFunctions::getInitialImageList(cssProperty(),
                                                  initialImageList);
  return maybeConvertStyleImageList(initialImageList);
}

InterpolationValue CSSImageListInterpolationType::maybeConvertStyleImageList(
    const StyleImageList& imageList) const {
  if (imageList.size() == 0)
    return nullptr;

  return ListInterpolationFunctions::createList(
      imageList.size(), [&imageList](size_t index) {
        return CSSImageInterpolationType::maybeConvertStyleImage(
            imageList[index].get(), false);
      });
}

class InheritedImageListChecker : public InterpolationType::ConversionChecker {
 public:
  ~InheritedImageListChecker() final {}

  static std::unique_ptr<InheritedImageListChecker> create(
      CSSPropertyID property,
      const StyleImageList& inheritedImageList) {
    return WTF::wrapUnique(
        new InheritedImageListChecker(property, inheritedImageList));
  }

 private:
  InheritedImageListChecker(CSSPropertyID property,
                            const StyleImageList& inheritedImageList)
      : m_property(property), m_inheritedImageList(inheritedImageList) {}

  bool isValid(const InterpolationEnvironment& environment,
               const InterpolationValue& underlying) const final {
    StyleImageList inheritedImageList;
    ImageListPropertyFunctions::getImageList(
        m_property, *environment.state().parentStyle(), inheritedImageList);
    return m_inheritedImageList == inheritedImageList;
  }

  CSSPropertyID m_property;
  StyleImageList m_inheritedImageList;
};

InterpolationValue CSSImageListInterpolationType::maybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversionCheckers) const {
  if (!state.parentStyle())
    return nullptr;

  StyleImageList inheritedImageList;
  ImageListPropertyFunctions::getImageList(cssProperty(), *state.parentStyle(),
                                           inheritedImageList);
  conversionCheckers.push_back(
      InheritedImageListChecker::create(cssProperty(), inheritedImageList));
  return maybeConvertStyleImageList(inheritedImageList);
}

InterpolationValue CSSImageListInterpolationType::maybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  if (value.isIdentifierValue() &&
      toCSSIdentifierValue(value).getValueID() == CSSValueNone)
    return nullptr;

  CSSValueList* tempList = nullptr;
  if (!value.isBaseValueList()) {
    tempList = CSSValueList::createCommaSeparated();
    tempList->append(value);
  }
  const CSSValueList& valueList = tempList ? *tempList : toCSSValueList(value);

  const size_t length = valueList.length();
  std::unique_ptr<InterpolableList> interpolableList =
      InterpolableList::create(length);
  Vector<RefPtr<NonInterpolableValue>> nonInterpolableValues(length);
  for (size_t i = 0; i < length; i++) {
    InterpolationValue component =
        CSSImageInterpolationType::maybeConvertCSSValue(valueList.item(i),
                                                        false);
    if (!component)
      return nullptr;
    interpolableList->set(i, std::move(component.interpolableValue));
    nonInterpolableValues[i] = std::move(component.nonInterpolableValue);
  }
  return InterpolationValue(
      std::move(interpolableList),
      NonInterpolableList::create(std::move(nonInterpolableValues)));
}

PairwiseInterpolationValue CSSImageListInterpolationType::maybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  return ListInterpolationFunctions::maybeMergeSingles(
      std::move(start), std::move(end),
      CSSImageInterpolationType::staticMergeSingleConversions);
}

InterpolationValue
CSSImageListInterpolationType::maybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  StyleImageList underlyingImageList;
  ImageListPropertyFunctions::getImageList(cssProperty(), style,
                                           underlyingImageList);
  return maybeConvertStyleImageList(underlyingImageList);
}

void CSSImageListInterpolationType::composite(
    UnderlyingValueOwner& underlyingValueOwner,
    double underlyingFraction,
    const InterpolationValue& value,
    double interpolationFraction) const {
  underlyingValueOwner.set(*this, value);
}

void CSSImageListInterpolationType::applyStandardPropertyValue(
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue* nonInterpolableValue,
    StyleResolverState& state) const {
  const InterpolableList& interpolableList =
      toInterpolableList(interpolableValue);
  const size_t length = interpolableList.length();
  DCHECK_GT(length, 0U);
  const NonInterpolableList& nonInterpolableList =
      toNonInterpolableList(*nonInterpolableValue);
  DCHECK_EQ(nonInterpolableList.length(), length);
  StyleImageList imageList(length);
  for (size_t i = 0; i < length; i++) {
    imageList[i] = CSSImageInterpolationType::resolveStyleImage(
        cssProperty(), *interpolableList.get(i), nonInterpolableList.get(i),
        state);
  }
  ImageListPropertyFunctions::setImageList(cssProperty(), *state.style(),
                                           imageList);
}

}  // namespace blink
