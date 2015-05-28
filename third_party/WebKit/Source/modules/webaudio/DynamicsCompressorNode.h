/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DynamicsCompressorNode_h
#define DynamicsCompressorNode_h

#include "modules/webaudio/AudioNode.h"
#include "modules/webaudio/AudioParam.h"
#include "wtf/OwnPtr.h"

namespace blink {

class DynamicsCompressor;

class DynamicsCompressorHandler final : public AudioHandler {
public:
    DynamicsCompressorHandler(AudioNode&, float sampleRate);
    virtual ~DynamicsCompressorHandler();

    // AudioHandler
    virtual void dispose() override;
    virtual void process(size_t framesToProcess) override;
    virtual void initialize() override;
    virtual void uninitialize() override;
    virtual void clearInternalStateWhenDisabled() override;

    // Static compression curve parameters.
    AudioParam* threshold() { return m_threshold.get(); }
    AudioParam* knee() { return m_knee.get(); }
    AudioParam* ratio() { return m_ratio.get(); }
    AudioParam* attack() { return m_attack.get(); }
    AudioParam* release() { return m_release.get(); }

    // Amount by which the compressor is currently compressing the signal in decibels.
    AudioParam* reduction() { return m_reduction.get(); }

    DECLARE_VIRTUAL_TRACE();

private:
    virtual double tailTime() const override;
    virtual double latencyTime() const override;

    OwnPtr<DynamicsCompressor> m_dynamicsCompressor;
    Member<AudioParam> m_threshold;
    Member<AudioParam> m_knee;
    Member<AudioParam> m_ratio;
    Member<AudioParam> m_reduction;
    Member<AudioParam> m_attack;
    Member<AudioParam> m_release;
};

class DynamicsCompressorNode final : public AudioNode {
    DEFINE_WRAPPERTYPEINFO();
public:
    static DynamicsCompressorNode* create(AudioContext*, float sampleRate);

    AudioParam* threshold() const;
    AudioParam* knee() const;
    AudioParam* ratio() const;
    AudioParam* reduction() const;
    AudioParam* attack() const;
    AudioParam* release() const;

private:
    DynamicsCompressorNode(AudioContext&, float sampleRate);
    DynamicsCompressorHandler& dynamicsCompressorHandler() const;
};

} // namespace blink

#endif // DynamicsCompressorNode_h
