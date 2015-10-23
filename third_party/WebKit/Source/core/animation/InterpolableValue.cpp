// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/InterpolableValue.h"

namespace blink {

DEFINE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(InterpolableValue);

void InterpolableNumber::interpolate(const InterpolableValue &to, const double progress, InterpolableValue& result) const
{
    const InterpolableNumber& toNumber = toInterpolableNumber(to);
    InterpolableNumber& resultNumber = toInterpolableNumber(result);

    if (progress == 0 || m_value == toNumber.m_value)
        resultNumber.m_value = m_value;
    else if (progress == 1)
        resultNumber.m_value = toNumber.m_value;
    else
        resultNumber.m_value = m_value * (1 - progress) + toNumber.m_value * progress;
}

void InterpolableBool::interpolate(const InterpolableValue &to, const double progress, InterpolableValue& result) const
{
    const InterpolableBool& toBool = toInterpolableBool(to);
    InterpolableBool& resultBool = toInterpolableBool(result);

    if (progress < 0.5)
        resultBool.m_value = m_value;
    else
        resultBool.m_value = toBool.m_value;
}

void InterpolableList::interpolate(const InterpolableValue& to, const double progress, InterpolableValue& result) const
{
    const InterpolableList& toList = toInterpolableList(to);
    InterpolableList& resultList = toInterpolableList(result);

    ASSERT(toList.m_size == m_size);
    ASSERT(resultList.m_size == m_size);

    for (size_t i = 0; i < m_size; i++) {
        ASSERT(m_values[i]);
        ASSERT(toList.m_values[i]);
        m_values[i]->interpolate(*(toList.m_values[i]), progress, *(resultList.m_values[i]));
    }
}

void InterpolableNumber::scaleAndAdd(double scale, const InterpolableValue& other)
{
    m_value = m_value * scale + toInterpolableNumber(other).m_value;
}

void InterpolableList::scaleAndAdd(double scale, const InterpolableValue& other)
{
    const InterpolableList& otherList = toInterpolableList(other);
    ASSERT(otherList.m_size == m_size);
    for (size_t i = 0; i < m_size; i++)
        m_values[i]->scaleAndAdd(scale, *otherList.m_values[i]);
}

DEFINE_TRACE(InterpolableList)
{
    visitor->trace(m_values);
    InterpolableValue::trace(visitor);
}

void InterpolableAnimatableValue::interpolate(const InterpolableValue& to, const double progress, InterpolableValue& result) const
{
    const InterpolableAnimatableValue& toValue = toInterpolableAnimatableValue(to);
    InterpolableAnimatableValue& resultValue = toInterpolableAnimatableValue(result);
    if (progress == 0)
        resultValue.m_value = m_value;
    if (progress == 1)
        resultValue.m_value = toValue.m_value;
    resultValue.m_value = AnimatableValue::interpolate(m_value.get(), toValue.m_value.get(), progress);
}

DEFINE_TRACE(InterpolableAnimatableValue)
{
    visitor->trace(m_value);
    InterpolableValue::trace(visitor);
}

}
