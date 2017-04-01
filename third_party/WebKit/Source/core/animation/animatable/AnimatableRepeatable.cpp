/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/animation/animatable/AnimatableRepeatable.h"

#include "wtf/MathExtras.h"

namespace blink {

bool AnimatableRepeatable::usesDefaultInterpolationWith(
    const AnimatableValue* value) const {
  const Vector<RefPtr<AnimatableValue>>& fromValues = m_values;
  const Vector<RefPtr<AnimatableValue>>& toValues =
      toAnimatableRepeatable(value)->m_values;
  DCHECK(!fromValues.isEmpty() && !toValues.isEmpty());
  size_t size = lowestCommonMultiple(fromValues.size(), toValues.size());
  DCHECK_GT(size, 0U);
  for (size_t i = 0; i < size; ++i) {
    const AnimatableValue* from = fromValues[i % fromValues.size()].get();
    const AnimatableValue* to = toValues[i % toValues.size()].get();
    // Spec: If a pair of values cannot be interpolated, then the lists are not
    // interpolable.
    if (AnimatableValue::usesDefaultInterpolation(from, to))
      return true;
  }
  return false;
}

bool AnimatableRepeatable::interpolateLists(
    const Vector<RefPtr<AnimatableValue>>& fromValues,
    const Vector<RefPtr<AnimatableValue>>& toValues,
    double fraction,
    Vector<RefPtr<AnimatableValue>>& interpolatedValues) {
  // Interpolation behaviour spec:
  // http://www.w3.org/TR/css3-transitions/#animtype-repeatable-list
  DCHECK(interpolatedValues.isEmpty());
  DCHECK(!fromValues.isEmpty() && !toValues.isEmpty());
  size_t size = lowestCommonMultiple(fromValues.size(), toValues.size());
  DCHECK_GT(size, 0U);
  for (size_t i = 0; i < size; ++i) {
    const AnimatableValue* from = fromValues[i % fromValues.size()].get();
    const AnimatableValue* to = toValues[i % toValues.size()].get();
    // Spec: If a pair of values cannot be interpolated, then the lists are not
    // interpolable.
    if (AnimatableValue::usesDefaultInterpolation(from, to))
      return false;
    interpolatedValues.push_back(interpolate(from, to, fraction));
  }
  return true;
}

PassRefPtr<AnimatableValue> AnimatableRepeatable::interpolateTo(
    const AnimatableValue* value,
    double fraction) const {
  Vector<RefPtr<AnimatableValue>> interpolatedValues;
  bool success =
      interpolateLists(m_values, toAnimatableRepeatable(value)->m_values,
                       fraction, interpolatedValues);
  if (success)
    return create(interpolatedValues);
  return defaultInterpolateTo(this, value, fraction);
}

bool AnimatableRepeatable::equalTo(const AnimatableValue* value) const {
  const Vector<RefPtr<AnimatableValue>>& otherValues =
      toAnimatableRepeatable(value)->m_values;
  if (m_values.size() != otherValues.size())
    return false;
  for (size_t i = 0; i < m_values.size(); ++i) {
    if (!m_values[i]->equals(otherValues[i].get()))
      return false;
  }
  return true;
}

}  // namespace blink
