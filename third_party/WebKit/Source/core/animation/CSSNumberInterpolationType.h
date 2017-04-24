// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSNumberInterpolationType_h
#define CSSNumberInterpolationType_h

#include "core/animation/CSSInterpolationType.h"

namespace blink {

class CSSNumberInterpolationType : public CSSInterpolationType {
 public:
  CSSNumberInterpolationType(PropertyHandle property)
      : CSSInterpolationType(property) {}

  InterpolationValue maybeConvertStandardPropertyUnderlyingValue(
      const ComputedStyle&) const final;
  void applyStandardPropertyValue(const InterpolableValue&,
                                  const NonInterpolableValue*,
                                  StyleResolverState&) const final;

  const CSSValue* createCSSValue(const InterpolableValue&,
                                 const NonInterpolableValue*,
                                 const StyleResolverState&) const final;

 private:
  InterpolationValue createNumberValue(double number) const;
  InterpolationValue maybeConvertNeutral(const InterpolationValue& underlying,
                                         ConversionCheckers&) const final;
  InterpolationValue maybeConvertInitial(const StyleResolverState&,
                                         ConversionCheckers&) const final;
  InterpolationValue maybeConvertInherit(const StyleResolverState&,
                                         ConversionCheckers&) const final;
  InterpolationValue maybeConvertValue(const CSSValue&,
                                       const StyleResolverState*,
                                       ConversionCheckers&) const final;
};

}  // namespace blink

#endif  // CSSNumberInterpolationType_h
