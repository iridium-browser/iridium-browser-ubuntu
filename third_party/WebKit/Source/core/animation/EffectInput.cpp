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

#include "config.h"
#include "core/animation/EffectInput.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/UnionTypesCore.h"
#include "core/animation/AnimationInputHelpers.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/StringKeyframe.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/NodeComputedStyle.h"
#include "wtf/NonCopyingSort.h"

namespace blink {

PassRefPtrWillBeRawPtr<AnimationEffect> EffectInput::convert(Element* element, const Vector<Dictionary>& keyframeDictionaryVector, ExceptionState& exceptionState)
{
    // FIXME: Remove the dependency on element.
    if (!element)
        return nullptr;

    StyleSheetContents* styleSheetContents = element->document().elementSheet().contents();
    StringKeyframeVector keyframes;
    double lastOffset = 0;

    for (const auto& keyframeDictionary : keyframeDictionaryVector) {
        RefPtrWillBeRawPtr<StringKeyframe> keyframe = StringKeyframe::create();

        ScriptValue scriptValue;
        bool frameHasOffset = DictionaryHelper::get(keyframeDictionary, "offset", scriptValue) && !scriptValue.isNull();

        if (frameHasOffset) {
            double offset;
            DictionaryHelper::get(keyframeDictionary, "offset", offset);

            // Keyframes with offsets outside the range [0.0, 1.0] are an error.
            if (std::isnan(offset)) {
                exceptionState.throwDOMException(InvalidModificationError, "Non numeric offset provided");
            }

            if (offset < 0 || offset > 1) {
                exceptionState.throwDOMException(InvalidModificationError, "Offsets provided outside the range [0, 1]");
                return nullptr;
            }

            if (offset < lastOffset) {
                exceptionState.throwDOMException(InvalidModificationError, "Keyframes with specified offsets are not sorted");
                return nullptr;
            }

            lastOffset = offset;

            keyframe->setOffset(offset);
        }
        keyframes.append(keyframe);

        String compositeString;
        DictionaryHelper::get(keyframeDictionary, "composite", compositeString);
        if (compositeString == "add")
            keyframe->setComposite(AnimationEffect::CompositeAdd);

        String timingFunctionString;
        if (DictionaryHelper::get(keyframeDictionary, "easing", timingFunctionString)) {
            if (RefPtr<TimingFunction> timingFunction = AnimationInputHelpers::parseTimingFunction(timingFunctionString))
                keyframe->setEasing(timingFunction);
        }

        Vector<String> keyframeProperties;
        keyframeDictionary.getPropertyNames(keyframeProperties);
        for (const auto& property : keyframeProperties) {
            CSSPropertyID id = AnimationInputHelpers::keyframeAttributeToCSSPropertyID(property);
            if (id == CSSPropertyInvalid)
                continue;
            String value;
            DictionaryHelper::get(keyframeDictionary, property, value);
            keyframe->setPropertyValue(id, value, styleSheetContents);
        }
    }

    RefPtrWillBeRawPtr<StringKeyframeEffectModel> keyframeEffectModel = StringKeyframeEffectModel::create(keyframes);
    if (keyframeEffectModel->hasSyntheticKeyframes()) {
        exceptionState.throwDOMException(NotSupportedError, "Partial keyframes are not supported.");
        return nullptr;
    }
    if (!keyframeEffectModel->isReplaceOnly()) {
        exceptionState.throwDOMException(NotSupportedError, "Additive animations are not supported.");
        return nullptr;
    }
    keyframeEffectModel->forceConversionsToAnimatableValues(*element, element->computedStyle());

    return keyframeEffectModel;
}

PassRefPtrWillBeRawPtr<AnimationEffect> EffectInput::convert(Element* element, const AnimationEffectOrDictionarySequence& effectInput, ExceptionState& exceptionState)
{
    if (effectInput.isAnimationEffect())
        return effectInput.getAsAnimationEffect();
    if (effectInput.isDictionarySequence())
        return convert(element, effectInput.getAsDictionarySequence(), exceptionState);
    return nullptr;
}

} // namespace blink
