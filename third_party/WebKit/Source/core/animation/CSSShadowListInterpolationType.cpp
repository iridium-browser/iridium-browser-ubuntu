// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSShadowListInterpolationType.h"

#include "core/animation/ListInterpolationFunctions.h"
#include "core/animation/ShadowInterpolationFunctions.h"
#include "core/animation/ShadowListPropertyFunctions.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/ShadowList.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

InterpolationValue CSSShadowListInterpolationType::convertShadowList(
    const ShadowList* shadowList,
    double zoom) const {
  if (!shadowList)
    return createNeutralValue();
  const ShadowDataVector& shadows = shadowList->shadows();
  return ListInterpolationFunctions::createList(
      shadows.size(), [&shadows, zoom](size_t index) {
        return ShadowInterpolationFunctions::convertShadowData(shadows[index],
                                                               zoom);
      });
}

InterpolationValue CSSShadowListInterpolationType::createNeutralValue() const {
  return ListInterpolationFunctions::createEmptyList();
}

InterpolationValue CSSShadowListInterpolationType::maybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return createNeutralValue();
}

InterpolationValue CSSShadowListInterpolationType::maybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  return convertShadowList(
      ShadowListPropertyFunctions::getInitialShadowList(cssProperty()), 1);
}

class InheritedShadowListChecker : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<InheritedShadowListChecker> create(
      CSSPropertyID property,
      PassRefPtr<ShadowList> shadowList) {
    return WTF::wrapUnique(
        new InheritedShadowListChecker(property, std::move(shadowList)));
  }

 private:
  InheritedShadowListChecker(CSSPropertyID property,
                             PassRefPtr<ShadowList> shadowList)
      : m_property(property), m_shadowList(shadowList) {}

  bool isValid(const InterpolationEnvironment& environment,
               const InterpolationValue& underlying) const final {
    const ShadowList* inheritedShadowList =
        ShadowListPropertyFunctions::getShadowList(
            m_property, *environment.state().parentStyle());
    if (!inheritedShadowList && !m_shadowList)
      return true;
    if (!inheritedShadowList || !m_shadowList)
      return false;
    return *inheritedShadowList == *m_shadowList;
  }

  const CSSPropertyID m_property;
  RefPtr<ShadowList> m_shadowList;
};

InterpolationValue CSSShadowListInterpolationType::maybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversionCheckers) const {
  if (!state.parentStyle())
    return nullptr;
  const ShadowList* inheritedShadowList =
      ShadowListPropertyFunctions::getShadowList(cssProperty(),
                                                 *state.parentStyle());
  conversionCheckers.push_back(InheritedShadowListChecker::create(
      cssProperty(),
      const_cast<ShadowList*>(inheritedShadowList)));  // Take ref.
  return convertShadowList(inheritedShadowList,
                           state.parentStyle()->effectiveZoom());
}

InterpolationValue CSSShadowListInterpolationType::maybeConvertValue(
    const CSSValue& value,
    const StyleResolverState&,
    ConversionCheckers&) const {
  if (value.isIdentifierValue() &&
      toCSSIdentifierValue(value).getValueID() == CSSValueNone)
    return createNeutralValue();

  if (!value.isBaseValueList())
    return nullptr;

  const CSSValueList& valueList = toCSSValueList(value);
  return ListInterpolationFunctions::createList(
      valueList.length(), [&valueList](size_t index) {
        return ShadowInterpolationFunctions::maybeConvertCSSValue(
            valueList.item(index));
      });
}

PairwiseInterpolationValue CSSShadowListInterpolationType::maybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  return ListInterpolationFunctions::maybeMergeSingles(
      std::move(start), std::move(end),
      ShadowInterpolationFunctions::maybeMergeSingles);
}

InterpolationValue
CSSShadowListInterpolationType::maybeConvertStandardPropertyUnderlyingValue(
    const StyleResolverState& state) const {
  if (!state.style())
    return nullptr;
  return convertShadowList(
      ShadowListPropertyFunctions::getShadowList(cssProperty(), *state.style()),
      state.style()->effectiveZoom());
}

void CSSShadowListInterpolationType::composite(
    UnderlyingValueOwner& underlyingValueOwner,
    double underlyingFraction,
    const InterpolationValue& value,
    double interpolationFraction) const {
  ListInterpolationFunctions::composite(
      underlyingValueOwner, underlyingFraction, *this, value,
      ShadowInterpolationFunctions::nonInterpolableValuesAreCompatible,
      ShadowInterpolationFunctions::composite);
}

static PassRefPtr<ShadowList> createShadowList(
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue* nonInterpolableValue,
    const StyleResolverState& state) {
  const InterpolableList& interpolableList =
      toInterpolableList(interpolableValue);
  size_t length = interpolableList.length();
  if (length == 0)
    return nullptr;
  const NonInterpolableList& nonInterpolableList =
      toNonInterpolableList(*nonInterpolableValue);
  ShadowDataVector shadows;
  for (size_t i = 0; i < length; i++)
    shadows.push_back(ShadowInterpolationFunctions::createShadowData(
        *interpolableList.get(i), nonInterpolableList.get(i), state));
  return ShadowList::adopt(shadows);
}

void CSSShadowListInterpolationType::applyStandardPropertyValue(
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue* nonInterpolableValue,
    StyleResolverState& state) const {
  ShadowListPropertyFunctions::setShadowList(
      cssProperty(), *state.style(),
      createShadowList(interpolableValue, nonInterpolableValue, state));
}

}  // namespace blink
