// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPositionAxisListInterpolationType_h
#define CSSPositionAxisListInterpolationType_h

#include "core/animation/CSSLengthListInterpolationType.h"

namespace blink {

class CSSPositionAxisListInterpolationType
    : public CSSLengthListInterpolationType {
 public:
  CSSPositionAxisListInterpolationType(PropertyHandle property)
      : CSSLengthListInterpolationType(property) {}

  static InterpolationValue ConvertPositionAxisCSSValue(const CSSValue&);

 private:
  InterpolationValue MaybeConvertValue(const CSSValue&,
                                       const StyleResolverState*,
                                       ConversionCheckers&) const final;
};

}  // namespace blink

#endif  // CSSPositionAxisListInterpolationType_h
