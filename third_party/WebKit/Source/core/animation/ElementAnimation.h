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

#ifndef ElementAnimation_h
#define ElementAnimation_h

#include "bindings/core/v8/UnionTypesCore.h"
#include "core/animation/Animation.h"
#include "core/animation/AnimationTimeline.h"
#include "core/animation/EffectInput.h"
#include "core/animation/ElementAnimations.h"
#include "core/animation/TimingInput.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "platform/RuntimeEnabledFeatures.h"


namespace blink {

class Dictionary;

class ElementAnimation {
public:
    static AnimationPlayer* animate(Element& element, const AnimationEffectOrDictionarySequence& effectInput, double duration, ExceptionState& exceptionState)
    {
        RefPtrWillBeRawPtr<AnimationEffect> effect = EffectInput::convert(&element, effectInput, exceptionState);
        if (exceptionState.hadException())
            return 0;
        return animateInternal(element, effect, TimingInput::convert(duration));
    }

    static AnimationPlayer* animate(Element& element, const AnimationEffectOrDictionarySequence& effectInput, const AnimationTimingProperties& timingInput, ExceptionState& exceptionState)
    {
        RefPtrWillBeRawPtr<AnimationEffect> effect = EffectInput::convert(&element, effectInput, exceptionState);
        if (exceptionState.hadException())
            return 0;
        return animateInternal(element, effect, TimingInput::convert(timingInput));
    }

    static AnimationPlayer* animate(Element& element, const AnimationEffectOrDictionarySequence& effectInput, ExceptionState& exceptionState)
    {
        RefPtrWillBeRawPtr<AnimationEffect> effect = EffectInput::convert(&element, effectInput, exceptionState);
        if (exceptionState.hadException())
            return 0;
        return animateInternal(element, effect, Timing());
    }

    static WillBeHeapVector<RefPtrWillBeMember<AnimationPlayer>> getAnimationPlayers(Element& element)
    {
        WillBeHeapVector<RefPtrWillBeMember<AnimationPlayer>> animationPlayers;

        if (!element.hasAnimations())
            return animationPlayers;

        for (const auto& player : element.document().timeline().getAnimationPlayers()) {
            ASSERT(player->source());
            if (toAnimation(player->source())->target() == element && (player->source()->isCurrent() || player->source()->isInEffect()))
                animationPlayers.append(player);
        }
        return animationPlayers;
    }

private:
    static AnimationPlayer* animateInternal(Element& element, PassRefPtrWillBeRawPtr<AnimationEffect> effect, const Timing& timing)
    {
        RefPtrWillBeRawPtr<Animation> animation = Animation::create(&element, effect, timing);
        return element.document().timeline().play(animation.get());
    }
};

} // namespace blink

#endif // ElementAnimation_h
