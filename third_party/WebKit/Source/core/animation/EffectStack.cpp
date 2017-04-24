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

#include "core/animation/EffectStack.h"

#include "core/animation/CompositorAnimations.h"
#include "core/animation/InvalidatableInterpolation.h"
#include "core/animation/css/CSSAnimations.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/NonCopyingSort.h"
#include <algorithm>

namespace blink {

namespace {

void copyToActiveInterpolationsMap(
    const Vector<RefPtr<Interpolation>>& source,
    EffectStack::PropertyHandleFilter propertyHandleFilter,
    ActiveInterpolationsMap& target) {
  for (const auto& interpolation : source) {
    PropertyHandle property = interpolation->getProperty();
    if (propertyHandleFilter && !propertyHandleFilter(property))
      continue;
    ActiveInterpolationsMap::AddResult entry =
        target.insert(property, ActiveInterpolations(1));
    ActiveInterpolations& activeInterpolations = entry.storedValue->value;
    if (!entry.isNewEntry &&
        (RuntimeEnabledFeatures::stackedCSSPropertyAnimationsEnabled() ||
         !property.isCSSProperty() || property.isPresentationAttribute()) &&
        interpolation->isInvalidatableInterpolation() &&
        toInvalidatableInterpolation(*interpolation)
            .dependsOnUnderlyingValue()) {
      activeInterpolations.push_back(interpolation.get());
    } else {
      activeInterpolations.at(0) = interpolation.get();
    }
  }
}

bool compareSampledEffects(const Member<SampledEffect>& sampledEffect1,
                           const Member<SampledEffect>& sampledEffect2) {
  DCHECK(sampledEffect1 && sampledEffect2);
  return sampledEffect1->sequenceNumber() < sampledEffect2->sequenceNumber();
}

void copyNewAnimationsToActiveInterpolationsMap(
    const HeapVector<Member<const InertEffect>>& newAnimations,
    EffectStack::PropertyHandleFilter propertyHandleFilter,
    ActiveInterpolationsMap& result) {
  for (const auto& newAnimation : newAnimations) {
    Vector<RefPtr<Interpolation>> sample;
    newAnimation->sample(sample);
    if (!sample.isEmpty())
      copyToActiveInterpolationsMap(sample, propertyHandleFilter, result);
  }
}

}  // namespace

EffectStack::EffectStack() {}

bool EffectStack::hasActiveAnimationsOnCompositor(
    CSSPropertyID property) const {
  for (const auto& sampledEffect : m_sampledEffects) {
    // TODO(dstockwell): move the playing check into AnimationEffectReadOnly and
    // expose both hasAnimations and hasActiveAnimations
    if (sampledEffect->effect() &&
        sampledEffect->effect()->animation()->playing() &&
        sampledEffect->effect()->hasActiveAnimationsOnCompositor(property))
      return true;
  }
  return false;
}

bool EffectStack::affectsProperties(PropertyHandleFilter filter) const {
  for (const auto& sampledEffect : m_sampledEffects) {
    for (const auto& interpolation : sampledEffect->interpolations()) {
      if (filter(interpolation->getProperty()))
        return true;
    }
  }
  return false;
}

ActiveInterpolationsMap EffectStack::activeInterpolations(
    EffectStack* effectStack,
    const HeapVector<Member<const InertEffect>>* newAnimations,
    const HeapHashSet<Member<const Animation>>* suppressedAnimations,
    KeyframeEffectReadOnly::Priority priority,
    PropertyHandleFilter propertyHandleFilter) {
  ActiveInterpolationsMap result;

  if (effectStack) {
    HeapVector<Member<SampledEffect>>& sampledEffects =
        effectStack->m_sampledEffects;
    // std::sort doesn't work with OwnPtrs
    nonCopyingSort(sampledEffects.begin(), sampledEffects.end(),
                   compareSampledEffects);
    effectStack->removeRedundantSampledEffects();
    for (const auto& sampledEffect : sampledEffects) {
      if (sampledEffect->priority() != priority ||
          (suppressedAnimations && sampledEffect->effect() &&
           suppressedAnimations->contains(
               sampledEffect->effect()->animation())))
        continue;
      copyToActiveInterpolationsMap(sampledEffect->interpolations(),
                                    propertyHandleFilter, result);
    }
  }

  if (newAnimations) {
    copyNewAnimationsToActiveInterpolationsMap(*newAnimations,
                                               propertyHandleFilter, result);
  }
  return result;
}

void EffectStack::removeRedundantSampledEffects() {
  HashSet<PropertyHandle> replacedProperties;
  for (size_t i = m_sampledEffects.size(); i--;) {
    SampledEffect& sampledEffect = *m_sampledEffects[i];
    if (sampledEffect.willNeverChange()) {
      sampledEffect.removeReplacedInterpolations(replacedProperties);
      sampledEffect.updateReplacedProperties(replacedProperties);
    }
  }

  size_t newSize = 0;
  for (auto& sampledEffect : m_sampledEffects) {
    if (!sampledEffect->interpolations().isEmpty())
      m_sampledEffects[newSize++].swap(sampledEffect);
    else if (sampledEffect->effect())
      sampledEffect->effect()->notifySampledEffectRemovedFromEffectStack();
  }
  m_sampledEffects.shrink(newSize);
}

DEFINE_TRACE(EffectStack) {
  visitor->trace(m_sampledEffects);
}

bool EffectStack::getAnimatedBoundingBox(FloatBox& box,
                                         CSSPropertyID property) const {
  FloatBox originalBox(box);
  for (const auto& sampledEffect : m_sampledEffects) {
    if (sampledEffect->effect() &&
        sampledEffect->effect()->affects(PropertyHandle(property))) {
      KeyframeEffectReadOnly* effect = sampledEffect->effect();
      const Timing& timing = effect->specifiedTiming();
      double startRange = 0;
      double endRange = 1;
      timing.timingFunction->range(&startRange, &endRange);
      FloatBox expandingBox(originalBox);
      if (!CompositorAnimations::getAnimatedBoundingBox(
              expandingBox, *effect->model(), startRange, endRange))
        return false;
      box.expandTo(expandingBox);
    }
  }
  return true;
}

}  // namespace blink
