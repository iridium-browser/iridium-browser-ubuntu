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

#ifndef KeyframeEffect_h
#define KeyframeEffect_h

#include "core/CoreExport.h"
#include "core/animation/AnimationEffectReadOnly.h"
#include "core/animation/EffectInput.h"
#include "core/animation/EffectModel.h"
#include "core/animation/TimingInput.h"
#include "platform/heap/Handle.h"
#include "wtf/RefPtr.h"

namespace blink {

class KeyframeEffectOptions;
class Dictionary;
class Element;
class ExceptionState;
class PropertyHandle;
class SampledEffect;

// Represents the effect of an Animation on an Element's properties.
// http://w3c.github.io/web-animations/#keyframe-effect
class CORE_EXPORT KeyframeEffect final : public AnimationEffectReadOnly {
    DEFINE_WRAPPERTYPEINFO();
public:
    enum Priority { DefaultPriority, TransitionPriority };

    static KeyframeEffect* create(Element*, EffectModel*, const Timing&, Priority = DefaultPriority, EventDelegate* = nullptr);
    // Web Animations API Bindings constructors.
    static KeyframeEffect* create(ExecutionContext*, Element*, const DictionarySequenceOrDictionary& effectInput, double duration, ExceptionState&);
    static KeyframeEffect* create(ExecutionContext*, Element*, const DictionarySequenceOrDictionary& effectInput, const KeyframeEffectOptions& timingInput, ExceptionState&);
    static KeyframeEffect* create(ExecutionContext*, Element*, const DictionarySequenceOrDictionary& effectInput, ExceptionState&);

    ~KeyframeEffect() override;

    bool isKeyframeEffect() const override { return true; }

    bool affects(PropertyHandle) const;
    const EffectModel* model() const { return m_model.get(); }
    EffectModel* model() { return m_model.get(); }
    void setModel(EffectModel* model) { m_model = model; }
    Priority getPriority() const { return m_priority; }
    Element* target() const { return m_target; }

    void notifySampledEffectRemovedFromAnimationStack();

    bool isCandidateForAnimationOnCompositor(double animationPlaybackRate) const;
    // Must only be called once.
    bool maybeStartAnimationOnCompositor(int group, double startTime, double timeOffset, double animationPlaybackRate);
    bool hasActiveAnimationsOnCompositor() const;
    bool hasActiveAnimationsOnCompositor(CSSPropertyID) const;
    bool cancelAnimationOnCompositor();
    void restartAnimationOnCompositor();
    void cancelIncompatibleAnimationsOnCompositor();
    void pauseAnimationForTestingOnCompositor(double pauseTime);

    void attachCompositedLayers();

    void setCompositorAnimationIdsForTesting(const Vector<int>& compositorAnimationIds) { m_compositorAnimationIds = compositorAnimationIds; }

    DECLARE_VIRTUAL_TRACE();

    void downgradeToNormal() { m_priority = DefaultPriority; }

protected:
    void applyEffects();
    void clearEffects();
    void updateChildrenAndEffects() const override;
    void attach(Animation*) override;
    void detach() override;
    void specifiedTimingChanged() override;
    double calculateTimeToEffectChange(bool forwards, double inheritedTime, double timeToNextIteration) const override;
    virtual bool hasIncompatibleStyle();
    bool hasMultipleTransformProperties() const;

private:
    KeyframeEffect(Element*, EffectModel*, const Timing&, Priority, EventDelegate*);

    Member<Element> m_target;
    Member<EffectModel> m_model;
    Member<SampledEffect> m_sampledEffect;

    Priority m_priority;

    Vector<int> m_compositorAnimationIds;

    friend class AnimationAnimationV8Test;
};

DEFINE_TYPE_CASTS(KeyframeEffect, AnimationEffectReadOnly, animationNode, animationNode->isKeyframeEffect(), animationNode.isKeyframeEffect());

} // namespace blink

#endif // KeyframeEffect_h
