// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/KeyframeEffectReadOnly.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/animation/Animation.h"
#include "core/animation/CompositorAnimations.h"
#include "core/animation/EffectInput.h"
#include "core/animation/ElementAnimations.h"
#include "core/animation/Interpolation.h"
#include "core/animation/KeyframeEffect.h"
#include "core/animation/KeyframeEffectOptions.h"
#include "core/animation/PropertyHandle.h"
#include "core/animation/SampledEffect.h"
#include "core/animation/TimingInput.h"
#include "core/dom/Element.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/frame/UseCounter.h"
#include "core/paint/PaintLayer.h"
#include "core/svg/SVGElement.h"

namespace blink {

KeyframeEffectReadOnly* KeyframeEffectReadOnly::create(
    Element* target,
    EffectModel* model,
    const Timing& timing,
    Priority priority,
    EventDelegate* eventDelegate) {
  return new KeyframeEffectReadOnly(target, model, timing, priority,
                                    eventDelegate);
}

KeyframeEffectReadOnly* KeyframeEffectReadOnly::create(
    ExecutionContext* executionContext,
    Element* element,
    const DictionarySequenceOrDictionary& effectInput,
    double duration,
    ExceptionState& exceptionState) {
  DCHECK(RuntimeEnabledFeatures::webAnimationsAPIEnabled());
  if (element) {
    UseCounter::count(
        element->document(),
        UseCounter::AnimationConstructorKeyframeListEffectObjectTiming);
  }
  Timing timing;
  if (!TimingInput::convert(duration, timing, exceptionState))
    return nullptr;
  return create(element, EffectInput::convert(element, effectInput,
                                              executionContext, exceptionState),
                timing);
}

KeyframeEffectReadOnly* KeyframeEffectReadOnly::create(
    ExecutionContext* executionContext,
    Element* element,
    const DictionarySequenceOrDictionary& effectInput,
    const KeyframeEffectOptions& timingInput,
    ExceptionState& exceptionState) {
  DCHECK(RuntimeEnabledFeatures::webAnimationsAPIEnabled());
  if (element) {
    UseCounter::count(
        element->document(),
        UseCounter::AnimationConstructorKeyframeListEffectObjectTiming);
  }
  Timing timing;
  Document* document = element ? &element->document() : nullptr;
  if (!TimingInput::convert(timingInput, timing, document, exceptionState))
    return nullptr;
  return create(element, EffectInput::convert(element, effectInput,
                                              executionContext, exceptionState),
                timing);
}

KeyframeEffectReadOnly* KeyframeEffectReadOnly::create(
    ExecutionContext* executionContext,
    Element* element,
    const DictionarySequenceOrDictionary& effectInput,
    ExceptionState& exceptionState) {
  DCHECK(RuntimeEnabledFeatures::webAnimationsAPIEnabled());
  if (element) {
    UseCounter::count(
        element->document(),
        UseCounter::AnimationConstructorKeyframeListEffectNoTiming);
  }
  return create(element, EffectInput::convert(element, effectInput,
                                              executionContext, exceptionState),
                Timing());
}

KeyframeEffectReadOnly::KeyframeEffectReadOnly(Element* target,
                                               EffectModel* model,
                                               const Timing& timing,
                                               Priority priority,
                                               EventDelegate* eventDelegate)
    : AnimationEffectReadOnly(timing, eventDelegate),
      m_target(target),
      m_model(model),
      m_sampledEffect(nullptr),
      m_priority(priority) {}

void KeyframeEffectReadOnly::attach(Animation* animation) {
  if (m_target) {
    m_target->ensureElementAnimations().animations().add(animation);
    m_target->setNeedsAnimationStyleRecalc();
    if (RuntimeEnabledFeatures::webAnimationsSVGEnabled() &&
        m_target->isSVGElement())
      toSVGElement(m_target)->setWebAnimationsPending();
  }
  AnimationEffectReadOnly::attach(animation);
}

void KeyframeEffectReadOnly::detach() {
  if (m_target)
    m_target->elementAnimations()->animations().remove(animation());
  if (m_sampledEffect)
    clearEffects();
  AnimationEffectReadOnly::detach();
}

void KeyframeEffectReadOnly::specifiedTimingChanged() {
  if (animation()) {
    // FIXME: Needs to consider groups when added.
    DCHECK_EQ(animation()->effect(), this);
    animation()->setCompositorPending(true);
  }
}

static EffectStack& ensureEffectStack(Element* element) {
  return element->ensureElementAnimations().effectStack();
}

bool KeyframeEffectReadOnly::hasMultipleTransformProperties() const {
  if (!m_target->computedStyle())
    return false;

  unsigned transformPropertyCount = 0;
  if (m_target->computedStyle()->hasTransformOperations())
    transformPropertyCount++;
  if (m_target->computedStyle()->rotate())
    transformPropertyCount++;
  if (m_target->computedStyle()->scale())
    transformPropertyCount++;
  if (m_target->computedStyle()->translate())
    transformPropertyCount++;
  return transformPropertyCount > 1;
}

// Returns true if transform, translate, rotate or scale is composited
// and a motion path or other transform properties
// has been introduced on the element
bool KeyframeEffectReadOnly::hasIncompatibleStyle() {
  if (!m_target->computedStyle())
    return false;

  bool affectsTransform =
      animation()->affects(*m_target, CSSPropertyTransform) ||
      animation()->affects(*m_target, CSSPropertyScale) ||
      animation()->affects(*m_target, CSSPropertyRotate) ||
      animation()->affects(*m_target, CSSPropertyTranslate);

  if (animation()->hasActiveAnimationsOnCompositor()) {
    if (m_target->computedStyle()->hasOffset() && affectsTransform)
      return true;
    return hasMultipleTransformProperties();
  }

  return false;
}

void KeyframeEffectReadOnly::applyEffects() {
  DCHECK(isInEffect());
  DCHECK(animation());
  if (!m_target || !m_model)
    return;

  if (hasIncompatibleStyle())
    animation()->cancelAnimationOnCompositor();

  double iteration = currentIteration();
  DCHECK_GE(iteration, 0);
  bool changed = false;
  if (m_sampledEffect) {
    changed = m_model->sample(clampTo<int>(iteration, 0), progress(),
                              iterationDuration(),
                              m_sampledEffect->mutableInterpolations());
  } else {
    Vector<RefPtr<Interpolation>> interpolations;
    m_model->sample(clampTo<int>(iteration, 0), progress(), iterationDuration(),
                    interpolations);
    if (!interpolations.isEmpty()) {
      SampledEffect* sampledEffect = SampledEffect::create(this);
      sampledEffect->mutableInterpolations().swap(interpolations);
      m_sampledEffect = sampledEffect;
      ensureEffectStack(m_target).add(sampledEffect);
      changed = true;
    } else {
      return;
    }
  }

  if (changed) {
    m_target->setNeedsAnimationStyleRecalc();
    if (RuntimeEnabledFeatures::webAnimationsSVGEnabled() &&
        m_target->isSVGElement())
      toSVGElement(*m_target).setWebAnimationsPending();
  }
}

void KeyframeEffectReadOnly::clearEffects() {
  DCHECK(animation());
  DCHECK(m_sampledEffect);

  m_sampledEffect->clear();
  m_sampledEffect = nullptr;
  restartAnimationOnCompositor();
  m_target->setNeedsAnimationStyleRecalc();
  if (RuntimeEnabledFeatures::webAnimationsSVGEnabled() &&
      m_target->isSVGElement())
    toSVGElement(*m_target).clearWebAnimatedAttributes();
  invalidate();
}

void KeyframeEffectReadOnly::updateChildrenAndEffects() const {
  if (!m_model)
    return;
  DCHECK(animation());
  if (isInEffect() && !animation()->effectSuppressed())
    const_cast<KeyframeEffectReadOnly*>(this)->applyEffects();
  else if (m_sampledEffect)
    const_cast<KeyframeEffectReadOnly*>(this)->clearEffects();
}

double KeyframeEffectReadOnly::calculateTimeToEffectChange(
    bool forwards,
    double localTime,
    double timeToNextIteration) const {
  const double startTime = specifiedTiming().startDelay;
  const double endTimeMinusEndDelay = startTime + activeDurationInternal();
  const double endTime = endTimeMinusEndDelay + specifiedTiming().endDelay;
  const double afterTime = std::min(endTimeMinusEndDelay, endTime);

  switch (getPhase()) {
    case PhaseNone:
      return std::numeric_limits<double>::infinity();
    case PhaseBefore:
      DCHECK_GE(startTime, localTime);
      return forwards ? startTime - localTime
                      : std::numeric_limits<double>::infinity();
    case PhaseActive:
      if (forwards) {
        // Need service to apply fill / fire events.
        const double timeToEnd = afterTime - localTime;
        if (requiresIterationEvents()) {
          return std::min(timeToEnd, timeToNextIteration);
        }
        return timeToEnd;
      }
      return 0;
    case PhaseAfter:
      DCHECK_GE(localTime, afterTime);
      // If this KeyframeEffect is still in effect then it will need to update
      // when its parent goes out of effect. We have no way of knowing when
      // that will be, however, so the parent will need to supply it.
      return forwards ? std::numeric_limits<double>::infinity()
                      : localTime - afterTime;
    default:
      NOTREACHED();
      return std::numeric_limits<double>::infinity();
  }
}

void KeyframeEffectReadOnly::notifySampledEffectRemovedFromEffectStack() {
  m_sampledEffect = nullptr;
}

bool KeyframeEffectReadOnly::isCandidateForAnimationOnCompositor(
    double animationPlaybackRate) const {
  // Do not put transforms on compositor if more than one of them are defined
  // in computed style because they need to be explicitly ordered
  if (!model() || !m_target ||
      (m_target->computedStyle() && m_target->computedStyle()->hasOffset()) ||
      hasMultipleTransformProperties())
    return false;

  return CompositorAnimations::isCandidateForAnimationOnCompositor(
      specifiedTiming(), *m_target, animation(), *model(),
      animationPlaybackRate);
}

bool KeyframeEffectReadOnly::maybeStartAnimationOnCompositor(
    int group,
    double startTime,
    double currentTime,
    double animationPlaybackRate) {
  DCHECK(!hasActiveAnimationsOnCompositor());
  if (!isCandidateForAnimationOnCompositor(animationPlaybackRate))
    return false;
  if (!CompositorAnimations::canStartAnimationOnCompositor(*m_target))
    return false;
  CompositorAnimations::startAnimationOnCompositor(
      *m_target, group, startTime, currentTime, specifiedTiming(), *animation(),
      *model(), m_compositorAnimationIds, animationPlaybackRate);
  DCHECK(!m_compositorAnimationIds.isEmpty());
  return true;
}

bool KeyframeEffectReadOnly::hasActiveAnimationsOnCompositor() const {
  return !m_compositorAnimationIds.isEmpty();
}

bool KeyframeEffectReadOnly::hasActiveAnimationsOnCompositor(
    CSSPropertyID property) const {
  return hasActiveAnimationsOnCompositor() && affects(PropertyHandle(property));
}

bool KeyframeEffectReadOnly::affects(PropertyHandle property) const {
  return m_model && m_model->affects(property);
}

bool KeyframeEffectReadOnly::cancelAnimationOnCompositor() {
  // FIXME: cancelAnimationOnCompositor is called from withins style recalc.
  // This queries compositingState, which is not necessarily up to date.
  // https://code.google.com/p/chromium/issues/detail?id=339847
  DisableCompositingQueryAsserts disabler;
  if (!hasActiveAnimationsOnCompositor())
    return false;
  if (!m_target || !m_target->layoutObject())
    return false;
  DCHECK(animation());
  for (const auto& compositorAnimationId : m_compositorAnimationIds) {
    CompositorAnimations::cancelAnimationOnCompositor(*m_target, *animation(),
                                                      compositorAnimationId);
  }
  m_compositorAnimationIds.clear();
  return true;
}

void KeyframeEffectReadOnly::restartAnimationOnCompositor() {
  if (cancelAnimationOnCompositor())
    animation()->setCompositorPending(true);
}

void KeyframeEffectReadOnly::cancelIncompatibleAnimationsOnCompositor() {
  if (m_target && animation() && model()) {
    CompositorAnimations::cancelIncompatibleAnimationsOnCompositor(
        *m_target, *animation(), *model());
  }
}

void KeyframeEffectReadOnly::pauseAnimationForTestingOnCompositor(
    double pauseTime) {
  DCHECK(hasActiveAnimationsOnCompositor());
  if (!m_target || !m_target->layoutObject())
    return;
  DCHECK(animation());
  for (const auto& compositorAnimationId : m_compositorAnimationIds) {
    CompositorAnimations::pauseAnimationForTestingOnCompositor(
        *m_target, *animation(), compositorAnimationId, pauseTime);
  }
}

void KeyframeEffectReadOnly::attachCompositedLayers() {
  DCHECK(m_target);
  DCHECK(animation());
  CompositorAnimations::attachCompositedLayers(*m_target, *animation());
}

DEFINE_TRACE(KeyframeEffectReadOnly) {
  visitor->trace(m_target);
  visitor->trace(m_model);
  visitor->trace(m_sampledEffect);
  AnimationEffectReadOnly::trace(visitor);
}

}  // namespace blink
