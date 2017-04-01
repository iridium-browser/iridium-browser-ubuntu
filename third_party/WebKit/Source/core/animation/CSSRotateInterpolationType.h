// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSRotateInterpolationType_h
#define CSSRotateInterpolationType_h

#include "core/animation/CSSInterpolationType.h"

namespace blink {

class CSSRotateInterpolationType : public CSSInterpolationType {
 public:
  CSSRotateInterpolationType(PropertyHandle property)
      : CSSInterpolationType(property) {
    DCHECK_EQ(cssProperty(), CSSPropertyRotate);
  }

  InterpolationValue maybeConvertStandardPropertyUnderlyingValue(
      const StyleResolverState&) const final;
  PairwiseInterpolationValue maybeMergeSingles(
      InterpolationValue&& start,
      InterpolationValue&& end) const final;
  void composite(UnderlyingValueOwner&,
                 double underlyingFraction,
                 const InterpolationValue&,
                 double interpolationFraction) const final;
  void applyStandardPropertyValue(const InterpolableValue&,
                                  const NonInterpolableValue*,
                                  StyleResolverState&) const final;

 private:
  InterpolationValue maybeConvertNeutral(const InterpolationValue& underlying,
                                         ConversionCheckers&) const final;
  InterpolationValue maybeConvertInitial(const StyleResolverState&,
                                         ConversionCheckers&) const final;
  InterpolationValue maybeConvertInherit(const StyleResolverState&,
                                         ConversionCheckers&) const final;
  InterpolationValue maybeConvertValue(const CSSValue&,
                                       const StyleResolverState&,
                                       ConversionCheckers&) const final;
  void additiveKeyframeHook(InterpolationValue&) const final;
};

}  // namespace blink

#endif  // CSSRotateInterpolationType_h
