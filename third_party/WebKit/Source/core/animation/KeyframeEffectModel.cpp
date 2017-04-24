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

#include "core/animation/KeyframeEffectModel.h"

#include "core/animation/AnimationEffectReadOnly.h"
#include "core/animation/CompositorAnimations.h"
#include "core/animation/css/CSSAnimatableValueFactory.h"
#include "core/css/CSSPropertyEquality.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/frame/UseCounter.h"
#include "platform/animation/AnimationUtilities.h"
#include "platform/geometry/FloatBox.h"
#include "platform/transforms/TransformationMatrix.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/StringHash.h"

namespace blink {

PropertyHandleSet KeyframeEffectModelBase::properties() const {
  PropertyHandleSet result;
  for (const auto& keyframe : m_keyframes) {
    for (const auto& property : keyframe->properties())
      result.insert(property);
  }
  return result;
}

void KeyframeEffectModelBase::setFrames(KeyframeVector& keyframes) {
  // TODO(samli): Should also notify/invalidate the animation
  m_keyframes = keyframes;
  m_keyframeGroups = nullptr;
  m_interpolationEffect.clear();
  m_lastFraction = std::numeric_limits<double>::quiet_NaN();
}

bool KeyframeEffectModelBase::sample(
    int iteration,
    double fraction,
    double iterationDuration,
    Vector<RefPtr<Interpolation>>& result) const {
  DCHECK_GE(iteration, 0);
  DCHECK(!isNull(fraction));
  ensureKeyframeGroups();
  ensureInterpolationEffectPopulated();

  bool changed = iteration != m_lastIteration || fraction != m_lastFraction ||
                 iterationDuration != m_lastIterationDuration;
  m_lastIteration = iteration;
  m_lastFraction = fraction;
  m_lastIterationDuration = iterationDuration;
  m_interpolationEffect.getActiveInterpolations(fraction, iterationDuration,
                                                result);
  return changed;
}

bool KeyframeEffectModelBase::snapshotNeutralCompositorKeyframes(
    Element& element,
    const ComputedStyle& oldStyle,
    const ComputedStyle& newStyle,
    const ComputedStyle* parentStyle) const {
  bool updated = false;
  ensureKeyframeGroups();
  for (CSSPropertyID property : CompositorAnimations::compositableProperties) {
    if (CSSPropertyEquality::propertiesEqual(property, oldStyle, newStyle))
      continue;
    PropertySpecificKeyframeGroup* keyframeGroup =
        m_keyframeGroups->at(PropertyHandle(property));
    if (!keyframeGroup)
      continue;
    for (auto& keyframe : keyframeGroup->m_keyframes) {
      if (keyframe->isNeutral())
        updated |= keyframe->populateAnimatableValue(property, element,
                                                     newStyle, parentStyle);
    }
  }
  return updated;
}

bool KeyframeEffectModelBase::snapshotAllCompositorKeyframes(
    Element& element,
    const ComputedStyle& baseStyle,
    const ComputedStyle* parentStyle) const {
  m_needsCompositorKeyframesSnapshot = false;
  bool updated = false;
  bool hasNeutralCompositableKeyframe = false;
  ensureKeyframeGroups();
  for (CSSPropertyID property : CompositorAnimations::compositableProperties) {
    PropertySpecificKeyframeGroup* keyframeGroup =
        m_keyframeGroups->at(PropertyHandle(property));
    if (!keyframeGroup)
      continue;
    for (auto& keyframe : keyframeGroup->m_keyframes) {
      updated |= keyframe->populateAnimatableValue(property, element, baseStyle,
                                                   parentStyle);
      hasNeutralCompositableKeyframe |= keyframe->isNeutral();
    }
  }
  if (updated && hasNeutralCompositableKeyframe) {
    UseCounter::count(element.document(),
                      UseCounter::SyntheticKeyframesInCompositedCSSAnimation);
  }
  return updated;
}

KeyframeEffectModelBase::KeyframeVector
KeyframeEffectModelBase::normalizedKeyframes(const KeyframeVector& keyframes) {
  double lastOffset = 0;
  KeyframeVector result;
  result.reserveCapacity(keyframes.size());

  for (const auto& keyframe : keyframes) {
    double offset = keyframe->offset();
    if (!isNull(offset)) {
      DCHECK_GE(offset, 0);
      DCHECK_LE(offset, 1);
      DCHECK_GE(offset, lastOffset);
      lastOffset = offset;
    }
    result.push_back(keyframe->clone());
  }

  if (result.isEmpty())
    return result;

  if (isNull(result.back()->offset()))
    result.back()->setOffset(1);

  if (result.size() > 1 && isNull(result[0]->offset()))
    result.front()->setOffset(0);

  size_t lastIndex = 0;
  lastOffset = result.front()->offset();
  for (size_t i = 1; i < result.size(); ++i) {
    double offset = result[i]->offset();
    if (!isNull(offset)) {
      for (size_t j = 1; j < i - lastIndex; ++j)
        result[lastIndex + j]->setOffset(
            lastOffset + (offset - lastOffset) * j / (i - lastIndex));
      lastIndex = i;
      lastOffset = offset;
    }
  }

  return result;
}

bool KeyframeEffectModelBase::isTransformRelatedEffect() const {
  return affects(PropertyHandle(CSSPropertyTransform)) ||
         affects(PropertyHandle(CSSPropertyRotate)) ||
         affects(PropertyHandle(CSSPropertyScale)) ||
         affects(PropertyHandle(CSSPropertyTranslate));
}

void KeyframeEffectModelBase::ensureKeyframeGroups() const {
  if (m_keyframeGroups)
    return;

  m_keyframeGroups = WTF::wrapUnique(new KeyframeGroupMap);
  RefPtr<TimingFunction> zeroOffsetEasing = m_defaultKeyframeEasing;
  for (const auto& keyframe : normalizedKeyframes(getFrames())) {
    if (keyframe->offset() == 0)
      zeroOffsetEasing = &keyframe->easing();

    for (const PropertyHandle& property : keyframe->properties()) {
      KeyframeGroupMap::iterator groupIter = m_keyframeGroups->find(property);
      PropertySpecificKeyframeGroup* group;
      if (groupIter == m_keyframeGroups->end()) {
        group = m_keyframeGroups
                    ->insert(property,
                             WTF::wrapUnique(new PropertySpecificKeyframeGroup))
                    .storedValue->value.get();
      } else {
        group = groupIter->value.get();
      }

      group->appendKeyframe(keyframe->createPropertySpecificKeyframe(property));
    }
  }

  // Add synthetic keyframes.
  m_hasSyntheticKeyframes = false;
  for (const auto& entry : *m_keyframeGroups) {
    if (entry.value->addSyntheticKeyframeIfRequired(zeroOffsetEasing))
      m_hasSyntheticKeyframes = true;

    entry.value->removeRedundantKeyframes();
  }
}

void KeyframeEffectModelBase::ensureInterpolationEffectPopulated() const {
  if (m_interpolationEffect.isPopulated())
    return;

  for (const auto& entry : *m_keyframeGroups) {
    const PropertySpecificKeyframeVector& keyframes = entry.value->keyframes();
    for (size_t i = 0; i < keyframes.size() - 1; i++) {
      size_t startIndex = i;
      size_t endIndex = i + 1;
      double startOffset = keyframes[startIndex]->offset();
      double endOffset = keyframes[endIndex]->offset();
      double applyFrom = startOffset;
      double applyTo = endOffset;

      if (i == 0) {
        applyFrom = -std::numeric_limits<double>::infinity();
        DCHECK_EQ(startOffset, 0.0);
        if (endOffset == 0.0) {
          DCHECK_NE(keyframes[endIndex + 1]->offset(), 0.0);
          endIndex = startIndex;
        }
      }
      if (i == keyframes.size() - 2) {
        applyTo = std::numeric_limits<double>::infinity();
        DCHECK_EQ(endOffset, 1.0);
        if (startOffset == 1.0) {
          DCHECK_NE(keyframes[startIndex - 1]->offset(), 1.0);
          startIndex = endIndex;
        }
      }

      if (applyFrom != applyTo) {
        m_interpolationEffect.addInterpolationsFromKeyframes(
            entry.key, *keyframes[startIndex], *keyframes[endIndex], applyFrom,
            applyTo);
      }
      // else the interpolation will never be used in sampling
    }
  }

  m_interpolationEffect.setPopulated();
}

bool KeyframeEffectModelBase::isReplaceOnly() {
  ensureKeyframeGroups();
  for (const auto& entry : *m_keyframeGroups) {
    for (const auto& keyframe : entry.value->keyframes()) {
      if (keyframe->composite() != EffectModel::CompositeReplace)
        return false;
    }
  }
  return true;
}

Keyframe::PropertySpecificKeyframe::PropertySpecificKeyframe(
    double offset,
    PassRefPtr<TimingFunction> easing,
    EffectModel::CompositeOperation composite)
    : m_offset(offset), m_easing(easing), m_composite(composite) {
  DCHECK(!isNull(offset));
}

void KeyframeEffectModelBase::PropertySpecificKeyframeGroup::appendKeyframe(
    PassRefPtr<Keyframe::PropertySpecificKeyframe> keyframe) {
  DCHECK(m_keyframes.isEmpty() ||
         m_keyframes.back()->offset() <= keyframe->offset());
  m_keyframes.push_back(keyframe);
}

void KeyframeEffectModelBase::PropertySpecificKeyframeGroup::
    removeRedundantKeyframes() {
  // As an optimization, removes interior keyframes that have the same offset
  // as both their neighbours, as they will never be used by sample().
  // Note that synthetic keyframes must be added before this method is
  // called.
  DCHECK_GE(m_keyframes.size(), 2U);
  for (int i = m_keyframes.size() - 2; i > 0; --i) {
    double offset = m_keyframes[i]->offset();
    bool hasSameOffsetAsPreviousNeighbor =
        m_keyframes[i - 1]->offset() == offset;
    bool hasSameOffsetAsNextNeighbor = m_keyframes[i + 1]->offset() == offset;
    if (hasSameOffsetAsPreviousNeighbor && hasSameOffsetAsNextNeighbor)
      m_keyframes.remove(i);
  }
  DCHECK_GE(m_keyframes.size(), 2U);
}

bool KeyframeEffectModelBase::PropertySpecificKeyframeGroup::
    addSyntheticKeyframeIfRequired(
        PassRefPtr<TimingFunction> zeroOffsetEasing) {
  DCHECK(!m_keyframes.isEmpty());

  bool addedSyntheticKeyframe = false;

  if (m_keyframes.front()->offset() != 0.0) {
    m_keyframes.insert(0, m_keyframes.front()->neutralKeyframe(
                              0, std::move(zeroOffsetEasing)));
    addedSyntheticKeyframe = true;
  }
  if (m_keyframes.back()->offset() != 1.0) {
    appendKeyframe(m_keyframes.back()->neutralKeyframe(1, nullptr));
    addedSyntheticKeyframe = true;
  }

  return addedSyntheticKeyframe;
}

}  // namespace blink
