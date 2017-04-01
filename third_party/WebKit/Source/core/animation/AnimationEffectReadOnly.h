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

#ifndef AnimationEffectReadOnly_h
#define AnimationEffectReadOnly_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/animation/Timing.h"
#include "platform/heap/Handle.h"

namespace blink {

class Animation;
class AnimationEffectReadOnly;
class AnimationEffectTimingReadOnly;
class ComputedTimingProperties;

enum TimingUpdateReason { TimingUpdateOnDemand, TimingUpdateForAnimationFrame };

static inline bool isNull(double value) {
  return std::isnan(value);
}

static inline double nullValue() {
  return std::numeric_limits<double>::quiet_NaN();
}

// Represents the content of an Animation and its fractional timing state.
// http://w3c.github.io/web-animations/#animation-effect
class CORE_EXPORT AnimationEffectReadOnly
    : public GarbageCollectedFinalized<AnimationEffectReadOnly>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  friend class Animation;  // Calls attach/detach, updateInheritedTime.
 public:
  // Note that logic in CSSAnimations depends on the order of these values.
  enum Phase {
    PhaseBefore,
    PhaseActive,
    PhaseAfter,
    PhaseNone,
  };

  class EventDelegate : public GarbageCollectedFinalized<EventDelegate> {
   public:
    virtual ~EventDelegate() {}
    virtual bool requiresIterationEvents(const AnimationEffectReadOnly&) = 0;
    virtual void onEventCondition(const AnimationEffectReadOnly&) = 0;
    DEFINE_INLINE_VIRTUAL_TRACE() {}
  };

  virtual ~AnimationEffectReadOnly() {}

  virtual bool isKeyframeEffectReadOnly() const { return false; }
  virtual bool isKeyframeEffect() const { return false; }
  virtual bool isInertEffect() const { return false; }

  Phase getPhase() const { return ensureCalculated().phase; }
  bool isCurrent() const { return ensureCalculated().isCurrent; }
  bool isInEffect() const { return ensureCalculated().isInEffect; }
  bool isInPlay() const { return ensureCalculated().isInPlay; }
  double currentIteration() const {
    return ensureCalculated().currentIteration;
  }
  double progress() const { return ensureCalculated().progress; }
  double timeToForwardsEffectChange() const {
    return ensureCalculated().timeToForwardsEffectChange;
  }
  double timeToReverseEffectChange() const {
    return ensureCalculated().timeToReverseEffectChange;
  }

  double iterationDuration() const;
  double activeDurationInternal() const;
  double endTimeInternal() const {
    return specifiedTiming().startDelay + activeDurationInternal() +
           specifiedTiming().endDelay;
  }

  const Animation* animation() const { return m_animation; }
  Animation* animation() { return m_animation; }
  const Timing& specifiedTiming() const { return m_timing; }
  virtual AnimationEffectTimingReadOnly* timing();
  void updateSpecifiedTiming(const Timing&);

  void getComputedTiming(ComputedTimingProperties&);
  ComputedTimingProperties getComputedTiming();

  DECLARE_VIRTUAL_TRACE();

 protected:
  explicit AnimationEffectReadOnly(const Timing&, EventDelegate* = nullptr);

  // When AnimationEffectReadOnly receives a new inherited time via
  // updateInheritedTime it will (if necessary) recalculate timings and (if
  // necessary) call updateChildrenAndEffects.
  void updateInheritedTime(double inheritedTime, TimingUpdateReason) const;
  void invalidate() const { m_needsUpdate = true; }
  bool requiresIterationEvents() const {
    return m_eventDelegate && m_eventDelegate->requiresIterationEvents(*this);
  }
  void clearEventDelegate() { m_eventDelegate = nullptr; }

  virtual void attach(Animation* animation) { m_animation = animation; }

  virtual void detach() {
    DCHECK(m_animation);
    m_animation = nullptr;
  }

  double repeatedDuration() const;

  virtual void updateChildrenAndEffects() const = 0;
  virtual double intrinsicIterationDuration() const { return 0; }
  virtual double calculateTimeToEffectChange(
      bool forwards,
      double localTime,
      double timeToNextIteration) const = 0;
  virtual void specifiedTimingChanged() {}

  Member<Animation> m_animation;
  Timing m_timing;
  Member<EventDelegate> m_eventDelegate;

  mutable struct CalculatedTiming {
    DISALLOW_NEW();
    Phase phase;
    double currentIteration;
    double progress;
    bool isCurrent;
    bool isInEffect;
    bool isInPlay;
    double localTime;
    double timeToForwardsEffectChange;
    double timeToReverseEffectChange;
  } m_calculated;
  mutable bool m_needsUpdate;
  mutable double m_lastUpdateTime;
  String m_name;

  const CalculatedTiming& ensureCalculated() const;
};

}  // namespace blink

#endif  // AnimationEffectReadOnly_h
