// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSValueInterpolationType_h
#define CSSValueInterpolationType_h

#include "core/animation/InterpolationType.h"

namespace blink {

// Never supports pairwise conversion while always supporting single conversion.
// A catch all for default for CSSValues.
class CSSValueInterpolationType : public InterpolationType {
public:
    CSSValueInterpolationType(CSSPropertyID property)
        : InterpolationType(property)
    { }

    PassOwnPtrWillBeRawPtr<PairwisePrimitiveInterpolation> maybeConvertPairwise(const CSSPropertySpecificKeyframe& startKeyframe, const CSSPropertySpecificKeyframe& endKeyframe, const StyleResolverState* state, ConversionCheckers&) const final
    {
        return nullptr;
    }

    PassOwnPtrWillBeRawPtr<InterpolationValue> maybeConvertSingle(const CSSPropertySpecificKeyframe&, const StyleResolverState*, ConversionCheckers&) const final;

    PassOwnPtrWillBeRawPtr<InterpolationValue> maybeConvertUnderlyingValue(const StyleResolverState&) const final
    {
        return nullptr;
    }

    void apply(const InterpolableValue&, const NonInterpolableValue*, StyleResolverState&) const final;
};

} // namespace blink

#endif // CSSValueInterpolationType_h
