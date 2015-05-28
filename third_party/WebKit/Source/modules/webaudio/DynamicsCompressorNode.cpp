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

#include "config.h"
#if ENABLE(WEB_AUDIO)
#include "modules/webaudio/DynamicsCompressorNode.h"

#include "modules/webaudio/AudioContext.h"
#include "modules/webaudio/AudioNodeInput.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "platform/audio/DynamicsCompressor.h"

// Set output to stereo by default.
static const unsigned defaultNumberOfOutputChannels = 2;

namespace blink {

DynamicsCompressorHandler::DynamicsCompressorHandler(AudioNode& node, float sampleRate)
    : AudioHandler(NodeTypeDynamicsCompressor, node, sampleRate)
{
    addInput();
    addOutput(defaultNumberOfOutputChannels);

    m_threshold = AudioParam::create(context(), -24);
    m_knee = AudioParam::create(context(), 30);
    m_ratio = AudioParam::create(context(), 12);
    m_reduction = AudioParam::create(context(), 0);
    m_attack = AudioParam::create(context(), 0.003);
    m_release = AudioParam::create(context(), 0.250);

    initialize();
}

DynamicsCompressorHandler::~DynamicsCompressorHandler()
{
    ASSERT(!isInitialized());
}

void DynamicsCompressorHandler::dispose()
{
    uninitialize();
    AudioHandler::dispose();
}

void DynamicsCompressorHandler::process(size_t framesToProcess)
{
    AudioBus* outputBus = output(0)->bus();
    ASSERT(outputBus);

    float threshold = m_threshold->value();
    float knee = m_knee->value();
    float ratio = m_ratio->value();
    float attack = m_attack->value();
    float release = m_release->value();

    m_dynamicsCompressor->setParameterValue(DynamicsCompressor::ParamThreshold, threshold);
    m_dynamicsCompressor->setParameterValue(DynamicsCompressor::ParamKnee, knee);
    m_dynamicsCompressor->setParameterValue(DynamicsCompressor::ParamRatio, ratio);
    m_dynamicsCompressor->setParameterValue(DynamicsCompressor::ParamAttack, attack);
    m_dynamicsCompressor->setParameterValue(DynamicsCompressor::ParamRelease, release);

    m_dynamicsCompressor->process(input(0)->bus(), outputBus, framesToProcess);

    float reduction = m_dynamicsCompressor->parameterValue(DynamicsCompressor::ParamReduction);
    m_reduction->setValue(reduction);
}

void DynamicsCompressorHandler::initialize()
{
    if (isInitialized())
        return;

    AudioHandler::initialize();
    m_dynamicsCompressor = adoptPtr(new DynamicsCompressor(sampleRate(), defaultNumberOfOutputChannels));
}

void DynamicsCompressorHandler::uninitialize()
{
    if (!isInitialized())
        return;

    m_dynamicsCompressor.clear();
    AudioHandler::uninitialize();
}

void DynamicsCompressorHandler::clearInternalStateWhenDisabled()
{
    m_reduction->setValue(0);
}

double DynamicsCompressorHandler::tailTime() const
{
    return m_dynamicsCompressor->tailTime();
}

double DynamicsCompressorHandler::latencyTime() const
{
    return m_dynamicsCompressor->latencyTime();
}

DEFINE_TRACE(DynamicsCompressorHandler)
{
    visitor->trace(m_threshold);
    visitor->trace(m_knee);
    visitor->trace(m_ratio);
    visitor->trace(m_reduction);
    visitor->trace(m_attack);
    visitor->trace(m_release);
    AudioHandler::trace(visitor);
}

// ----------------------------------------------------------------

DynamicsCompressorNode::DynamicsCompressorNode(AudioContext& context, float sampleRate)
    : AudioNode(context)
{
    setHandler(new DynamicsCompressorHandler(*this, sampleRate));
}

DynamicsCompressorNode* DynamicsCompressorNode::create(AudioContext* context, float sampleRate)
{
    return new DynamicsCompressorNode(*context, sampleRate);
}

DynamicsCompressorHandler& DynamicsCompressorNode::dynamicsCompressorHandler() const
{
    return static_cast<DynamicsCompressorHandler&>(handler());
}

AudioParam* DynamicsCompressorNode::threshold() const
{
    return dynamicsCompressorHandler().threshold();
}

AudioParam* DynamicsCompressorNode::knee() const
{
    return dynamicsCompressorHandler().knee();
}

AudioParam* DynamicsCompressorNode::ratio() const
{
    return dynamicsCompressorHandler().ratio();
}

AudioParam* DynamicsCompressorNode::reduction() const
{
    return dynamicsCompressorHandler().reduction();
}

AudioParam* DynamicsCompressorNode::attack() const
{
    return dynamicsCompressorHandler().attack();
}

AudioParam* DynamicsCompressorNode::release() const
{
    return dynamicsCompressorHandler().release();
}

} // namespace blink

#endif // ENABLE(WEB_AUDIO)
