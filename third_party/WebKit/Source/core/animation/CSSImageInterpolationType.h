// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSImageInterpolationType_h
#define CSSImageInterpolationType_h

#include "core/animation/CSSInterpolationType.h"

namespace blink {

class StyleImage;

class CSSImageInterpolationType : public CSSInterpolationType {
 public:
  CSSImageInterpolationType(PropertyHandle property)
      : CSSInterpolationType(property) {}

  InterpolationValue maybeConvertStandardPropertyUnderlyingValue(
      const ComputedStyle&) const final;
  void composite(UnderlyingValueOwner&,
                 double underlyingFraction,
                 const InterpolationValue&,
                 double interpolationFraction) const final;
  void applyStandardPropertyValue(const InterpolableValue&,
                                  const NonInterpolableValue*,
                                  StyleResolverState&) const final;

  static InterpolationValue maybeConvertCSSValue(const CSSValue&,
                                                 bool acceptGradients);
  static InterpolationValue maybeConvertStyleImage(const StyleImage&,
                                                   bool acceptGradients);
  static InterpolationValue maybeConvertStyleImage(const StyleImage* image,
                                                   bool acceptGradients) {
    return image ? maybeConvertStyleImage(*image, acceptGradients) : nullptr;
  }
  static PairwiseInterpolationValue staticMergeSingleConversions(
      InterpolationValue&& start,
      InterpolationValue&& end);
  static CSSValue* createCSSValue(const InterpolableValue&,
                                  const NonInterpolableValue*);
  static StyleImage* resolveStyleImage(CSSPropertyID,
                                       const InterpolableValue&,
                                       const NonInterpolableValue*,
                                       StyleResolverState&);
  static bool equalNonInterpolableValues(const NonInterpolableValue*,
                                         const NonInterpolableValue*);

 private:
  InterpolationValue maybeConvertNeutral(const InterpolationValue& underlying,
                                         ConversionCheckers&) const final;
  InterpolationValue maybeConvertInitial(const StyleResolverState&,
                                         ConversionCheckers&) const final;
  InterpolationValue maybeConvertInherit(const StyleResolverState&,
                                         ConversionCheckers&) const final;
  InterpolationValue maybeConvertValue(const CSSValue&,
                                       const StyleResolverState*,
                                       ConversionCheckers&) const final;

  PairwiseInterpolationValue maybeMergeSingles(
      InterpolationValue&& start,
      InterpolationValue&& end) const final {
    return staticMergeSingleConversions(std::move(start), std::move(end));
  }
};

}  // namespace blink

#endif  // CSSImageInterpolationType_h
