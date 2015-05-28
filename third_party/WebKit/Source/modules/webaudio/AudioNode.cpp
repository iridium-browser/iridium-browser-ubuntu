/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
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
#include "modules/webaudio/AudioNode.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webaudio/AudioContext.h"
#include "modules/webaudio/AudioNodeInput.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "modules/webaudio/AudioParam.h"
#include "wtf/Atomics.h"
#include "wtf/MainThread.h"

#if DEBUG_AUDIONODE_REFERENCES
#include <stdio.h>
#endif

namespace blink {

unsigned AudioHandler::s_instanceCount = 0;

AudioHandler::AudioHandler(NodeType nodeType, AudioNode& node, float sampleRate)
    : m_isInitialized(false)
    , m_nodeType(NodeTypeUnknown)
    , m_node(node)
    , m_context(node.context())
    , m_sampleRate(sampleRate)
    , m_lastProcessingTime(-1)
    , m_lastNonSilentTime(-1)
    , m_connectionRefCount(0)
    , m_isDisabled(false)
    , m_channelCount(2)
    , m_channelCountMode(Max)
    , m_channelInterpretation(AudioBus::Speakers)
    , m_newChannelCountMode(Max)
{
    setNodeType(nodeType);

#if DEBUG_AUDIONODE_REFERENCES
    if (!s_isNodeCountInitialized) {
        s_isNodeCountInitialized = true;
        atexit(AudioHandler::printNodeCounts);
    }
#endif
    ++s_instanceCount;
}

AudioHandler::~AudioHandler()
{
    --s_instanceCount;
#if DEBUG_AUDIONODE_REFERENCES
    --s_nodeCount[nodeType()];
    fprintf(stderr, "%p: %2d: AudioNode::~AudioNode() %d [%d]\n",
        this, nodeType(), m_connectionRefCount, s_nodeCount[nodeType()]);
#endif
}

void AudioHandler::initialize()
{
    m_isInitialized = true;
}

void AudioHandler::uninitialize()
{
    m_isInitialized = false;
}

void AudioHandler::clearInternalStateWhenDisabled()
{
}

void AudioHandler::dispose()
{
    ASSERT(isMainThread());
    ASSERT(context()->isGraphOwner());

    context()->handler().removeChangedChannelCountMode(this);
    context()->handler().removeAutomaticPullNode(this);
    context()->handler().disposeOutputs(*this);
}

String AudioHandler::nodeTypeName() const
{
    switch (m_nodeType) {
    case NodeTypeDestination:
        return "AudioDestinationNode";
    case NodeTypeOscillator:
        return "OscillatorNode";
    case NodeTypeAudioBufferSource:
        return "AudioBufferSourceNode";
    case NodeTypeMediaElementAudioSource:
        return "MediaElementAudioSourceNode";
    case NodeTypeMediaStreamAudioDestination:
        return "MediaStreamAudioDestinationNode";
    case NodeTypeMediaStreamAudioSource:
        return "MediaStreamAudioSourceNode";
    case NodeTypeJavaScript:
        return "ScriptProcessorNode";
    case NodeTypeBiquadFilter:
        return "BiquadFilterNode";
    case NodeTypePanner:
        return "PannerNode";
    case NodeTypeStereoPanner:
        return "StereoPannerNode";
    case NodeTypeConvolver:
        return "ConvolverNode";
    case NodeTypeDelay:
        return "DelayNode";
    case NodeTypeGain:
        return "GainNode";
    case NodeTypeChannelSplitter:
        return "ChannelSplitterNode";
    case NodeTypeChannelMerger:
        return "ChannelMergerNode";
    case NodeTypeAnalyser:
        return "AnalyserNode";
    case NodeTypeDynamicsCompressor:
        return "DynamicsCompressorNode";
    case NodeTypeWaveShaper:
        return "WaveShaperNode";
    case NodeTypeUnknown:
    case NodeTypeEnd:
    default:
        ASSERT_NOT_REACHED();
        return "UnknownNode";
    }
}

void AudioHandler::setNodeType(NodeType type)
{
    // Don't allow the node type to be changed to a different node type, after it's already been
    // set!  And the new type can't be unknown or end!
    ASSERT(m_nodeType == NodeTypeUnknown);
    ASSERT(type != NodeTypeUnknown);
    ASSERT(type != NodeTypeEnd);

    m_nodeType = type;

#if DEBUG_AUDIONODE_REFERENCES
    ++s_nodeCount[type];
    fprintf(stderr, "%p: %2d: AudioNode::AudioNode [%3d]\n", this, nodeType(), s_nodeCount[nodeType()]);
#endif
}

void AudioHandler::addInput()
{
    m_inputs.append(AudioNodeInput::create(*this));
}

void AudioHandler::addOutput(unsigned numberOfChannels)
{
    m_outputs.append(AudioNodeOutput::create(this, numberOfChannels));
    m_connectedNodes.append(nullptr);
    ASSERT(numberOfOutputs() == m_connectedNodes.size());
    m_connectedParams.append(nullptr);
    ASSERT(numberOfOutputs() == m_connectedParams.size());
}

AudioNodeInput* AudioHandler::input(unsigned i)
{
    if (i < m_inputs.size())
        return m_inputs[i].get();
    return nullptr;
}

AudioNodeOutput* AudioHandler::output(unsigned i)
{
    if (i < m_outputs.size())
        return m_outputs[i].get();
    return nullptr;
}

void AudioHandler::connect(AudioHandler* destination, unsigned outputIndex, unsigned inputIndex, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    AudioContext::AutoLocker locker(context());

    if (context()->isContextClosed()) {
        exceptionState.throwDOMException(
            InvalidStateError,
            "Cannot connect after the context has been closed.");
        return;
    }

    if (!destination) {
        exceptionState.throwDOMException(
            SyntaxError,
            "invalid destination node.");
        return;
    }

    // Sanity check input and output indices.
    if (outputIndex >= numberOfOutputs()) {
        exceptionState.throwDOMException(
            IndexSizeError,
            "output index (" + String::number(outputIndex) + ") exceeds number of outputs (" + String::number(numberOfOutputs()) + ").");
        return;
    }

    if (destination && inputIndex >= destination->numberOfInputs()) {
        exceptionState.throwDOMException(
            IndexSizeError,
            "input index (" + String::number(inputIndex) + ") exceeds number of inputs (" + String::number(destination->numberOfInputs()) + ").");
        return;
    }

    if (context() != destination->context()) {
        exceptionState.throwDOMException(
            SyntaxError,
            "cannot connect to a destination belonging to a different audio context.");
        return;
    }

    destination->input(inputIndex)->connect(*output(outputIndex));
    if (!m_connectedNodes[outputIndex])
        m_connectedNodes[outputIndex] = new HeapHashSet<Member<AudioHandler>>();
    m_connectedNodes[outputIndex]->add(destination);

    // Let context know that a connection has been made.
    context()->incrementConnectionCount();
}

void AudioHandler::connect(AudioParam* param, unsigned outputIndex, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    AudioContext::AutoLocker locker(context());

    if (context()->isContextClosed()) {
        exceptionState.throwDOMException(
            InvalidStateError,
            "Cannot connect after the context has been closed.");
        return;
    }

    if (!param) {
        exceptionState.throwDOMException(
            SyntaxError,
            "invalid AudioParam.");
        return;
    }

    if (outputIndex >= numberOfOutputs()) {
        exceptionState.throwDOMException(
            IndexSizeError,
            "output index (" + String::number(outputIndex) + ") exceeds number of outputs (" + String::number(numberOfOutputs()) + ").");
        return;
    }

    if (context() != param->context()) {
        exceptionState.throwDOMException(
            SyntaxError,
            "cannot connect to an AudioParam belonging to a different audio context.");
        return;
    }

    param->handler().connect(*output(outputIndex));
    if (!m_connectedParams[outputIndex])
        m_connectedParams[outputIndex] = new HeapHashSet<Member<AudioParam>>();
    m_connectedParams[outputIndex]->add(param);
}

void AudioHandler::disconnect()
{
    ASSERT(isMainThread());
    AudioContext::AutoLocker locker(context());

    // Disconnect all outgoing connections.
    for (unsigned i = 0; i < numberOfOutputs(); ++i) {
        this->output(i)->disconnectAll();
        m_connectedNodes[i] = nullptr;
        m_connectedParams[i] = nullptr;
    }
}

void AudioHandler::disconnect(unsigned outputIndex, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    AudioContext::AutoLocker locker(context());

    // Sanity check on the output index.
    if (outputIndex >= numberOfOutputs()) {
        exceptionState.throwDOMException(
            IndexSizeError,
            ExceptionMessages::indexOutsideRange(
                "output index",
                outputIndex,
                0u,
                ExceptionMessages::InclusiveBound,
                numberOfOutputs(),
                ExceptionMessages::InclusiveBound));
        return;
    }

    // Disconnect all outgoing connections from the given output.
    output(outputIndex)->disconnectAll();
    m_connectedNodes[outputIndex] = nullptr;
    m_connectedParams[outputIndex] = nullptr;
}

void AudioHandler::disconnect(AudioHandler* destination, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    AudioContext::AutoLocker locker(context());

    unsigned numberOfDisconnections = 0;

    // FIXME: Can this be optimized? ChannelSplitter and ChannelMerger can have
    // 32 ports and that requires 1024 iterations to validate entire connections.
    for (unsigned i = 0; i < numberOfOutputs(); ++i) {
        AudioNodeOutput* output = this->output(i);
        for (unsigned j = 0; j < destination->numberOfInputs(); ++j) {
            AudioNodeInput* input = destination->input(j);
            if (output->isConnectedToInput(*input)) {
                output->disconnectInput(*input);
                m_connectedNodes[i]->remove(destination);
                numberOfDisconnections++;
            }
        }
    }

    // If there is no connection to the destination, throw an exception.
    if (numberOfDisconnections == 0) {
        exceptionState.throwDOMException(
            InvalidAccessError,
            "the given destination is not connected.");
        return;
    }
}

void AudioHandler::disconnect(AudioHandler* destination, unsigned outputIndex, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    AudioContext::AutoLocker locker(context());

    bool isOutputIndexInRange = outputIndex < numberOfOutputs();

    // If the output index is valid, proceed to disconnect.
    if (isOutputIndexInRange) {
        unsigned numberOfDisconnections = 0;
        AudioNodeOutput* output = this->output(outputIndex);

        // Sanity check on destination inputs and disconnect when possible.
        for (unsigned i = 0; i < destination->numberOfInputs(); ++i) {
            AudioNodeInput* input = destination->input(i);
            if (output->isConnectedToInput(*input)) {
                output->disconnectInput(*input);
                m_connectedNodes[outputIndex]->remove(destination);
                numberOfDisconnections++;
            }
        }

        // If there is no connection to the destination, throw an exception.
        if (numberOfDisconnections == 0) {
            exceptionState.throwDOMException(
                InvalidAccessError,
                "output (" + String::number(outputIndex) + ") is not connected to the given destination.");
            return;
        }

    } else {

        // The output index is out of range. Throw an exception.
        exceptionState.throwDOMException(
            IndexSizeError,
            ExceptionMessages::indexOutsideRange(
                "output index",
                outputIndex,
                0u,
                ExceptionMessages::InclusiveBound,
                numberOfOutputs(),
                ExceptionMessages::InclusiveBound));
        return;
    }
}

void AudioHandler::disconnect(AudioHandler* destination, unsigned outputIndex, unsigned inputIndex, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    AudioContext::AutoLocker locker(context());

    bool isOutputIndexInRange = outputIndex < numberOfOutputs();
    bool isInputIndexInRange = inputIndex < destination->numberOfInputs();

    // If both indices are valid, proceed to disconnect.
    if (isOutputIndexInRange && isInputIndexInRange) {
        AudioNodeOutput* output = this->output(outputIndex);
        AudioNodeInput* input = destination->input(inputIndex);

        // Sanity check on the connection between the output and the destination input.
        if (!output->isConnectedToInput(*input)) {
            exceptionState.throwDOMException(
                InvalidAccessError,
                "output (" + String::number(outputIndex) + ") is not connected to the input (" + String::number(inputIndex) + ") of the destination.");
            return;
        }

        output->disconnectInput(*input);
        m_connectedNodes[outputIndex]->remove(destination);
    }

    // Sanity check input and output indices.
    if (!isOutputIndexInRange) {
        exceptionState.throwDOMException(
            IndexSizeError,
            ExceptionMessages::indexOutsideRange(
                "output index",
                outputIndex,
                0u,
                ExceptionMessages::InclusiveBound,
                numberOfOutputs(),
                ExceptionMessages::InclusiveBound));
        return;
    }

    if (!isInputIndexInRange) {
        exceptionState.throwDOMException(
            IndexSizeError,
            ExceptionMessages::indexOutsideRange(
                "input index",
                inputIndex,
                0u,
                ExceptionMessages::InclusiveBound,
                destination->numberOfInputs(),
                ExceptionMessages::InclusiveBound));
        return;
    }
}

void AudioHandler::disconnect(AudioParam* destinationParam, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    AudioContext::AutoLocker locker(context());

    // The number of disconnection made.
    unsigned numberOfDisconnections = 0;

    // Check if the node output is connected the destination AudioParam.
    // Disconnect if connected and increase |numberOfDisconnectios| by 1.
    for (unsigned i = 0; i < numberOfOutputs(); ++i) {
        AudioNodeOutput* output = this->output(i);
        if (output->isConnectedToAudioParam(destinationParam->handler())) {
            output->disconnectAudioParam(destinationParam->handler());
            m_connectedParams[i]->remove(destinationParam);
            numberOfDisconnections++;
        }
    }

    // Throw an exception when there is no valid connection to the destination.
    if (numberOfDisconnections == 0) {
        exceptionState.throwDOMException(
            InvalidAccessError,
            "the given AudioParam is not connected.");
        return;
    }
}

void AudioHandler::disconnect(AudioParam* destinationParam, unsigned outputIndex, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    AudioContext::AutoLocker locker(context());

    bool isOutputIndexInRange = outputIndex < numberOfOutputs();

    // If the output index is valid, proceed to disconnect.
    if (isOutputIndexInRange) {
        AudioNodeOutput* output = this->output(outputIndex);

        // Sanity check on the connection between the output and the destination.
        if (!output->isConnectedToAudioParam(destinationParam->handler())) {
            exceptionState.throwDOMException(
                InvalidAccessError,
                "specified destination AudioParam and node output (" + String::number(outputIndex) + ") are not connected.");
            return;
        }

        output->disconnectAudioParam(destinationParam->handler());
        m_connectedParams[outputIndex]->remove(destinationParam);
    } else {

        // The output index is out of range. Throw an exception.
        exceptionState.throwDOMException(
            IndexSizeError,
            ExceptionMessages::indexOutsideRange(
                "output index",
                outputIndex,
                0u,
                ExceptionMessages::InclusiveBound,
                numberOfOutputs(),
                ExceptionMessages::InclusiveBound));
        return;
    }
}

void AudioHandler::disconnectWithoutException(unsigned outputIndex)
{
    ASSERT(isMainThread());
    AudioContext::AutoLocker locker(context());

    // Sanity check input and output indices.
    if (outputIndex >= numberOfOutputs())
        return;
    output(outputIndex)->disconnectAll();
    m_connectedNodes[outputIndex] = nullptr;
    m_connectedParams[outputIndex] = nullptr;
}

unsigned long AudioHandler::channelCount()
{
    return m_channelCount;
}

void AudioHandler::setChannelCount(unsigned long channelCount, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    AudioContext::AutoLocker locker(context());

    if (channelCount > 0 && channelCount <= AudioContext::maxNumberOfChannels()) {
        if (m_channelCount != channelCount) {
            m_channelCount = channelCount;
            if (m_channelCountMode != Max)
                updateChannelsForInputs();
        }
    } else {
        exceptionState.throwDOMException(
            NotSupportedError,
            "channel count (" + String::number(channelCount) + ") must be between 1 and " + String::number(AudioContext::maxNumberOfChannels()) + ".");
    }
}

String AudioHandler::channelCountMode()
{
    switch (m_channelCountMode) {
    case Max:
        return "max";
    case ClampedMax:
        return "clamped-max";
    case Explicit:
        return "explicit";
    }
    ASSERT_NOT_REACHED();
    return "";
}

void AudioHandler::setChannelCountMode(const String& mode, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    AudioContext::AutoLocker locker(context());

    ChannelCountMode oldMode = m_channelCountMode;

    if (mode == "max") {
        m_newChannelCountMode = Max;
    } else if (mode == "clamped-max") {
        m_newChannelCountMode = ClampedMax;
    } else if (mode == "explicit") {
        m_newChannelCountMode = Explicit;
    } else {
        ASSERT_NOT_REACHED();
    }

    if (m_newChannelCountMode != oldMode)
        context()->handler().addChangedChannelCountMode(this);
}

String AudioHandler::channelInterpretation()
{
    switch (m_channelInterpretation) {
    case AudioBus::Speakers:
        return "speakers";
    case AudioBus::Discrete:
        return "discrete";
    }
    ASSERT_NOT_REACHED();
    return "";
}

void AudioHandler::setChannelInterpretation(const String& interpretation, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    AudioContext::AutoLocker locker(context());

    if (interpretation == "speakers") {
        m_channelInterpretation = AudioBus::Speakers;
    } else if (interpretation == "discrete") {
        m_channelInterpretation = AudioBus::Discrete;
    } else {
        ASSERT_NOT_REACHED();
    }
}

void AudioHandler::updateChannelsForInputs()
{
    for (unsigned i = 0; i < m_inputs.size(); ++i)
        input(i)->changedOutputs();
}

void AudioHandler::processIfNecessary(size_t framesToProcess)
{
    ASSERT(context()->isAudioThread());

    if (!isInitialized())
        return;

    // Ensure that we only process once per rendering quantum.
    // This handles the "fanout" problem where an output is connected to multiple inputs.
    // The first time we're called during this time slice we process, but after that we don't want to re-process,
    // instead our output(s) will already have the results cached in their bus;
    double currentTime = context()->currentTime();
    if (m_lastProcessingTime != currentTime) {
        m_lastProcessingTime = currentTime; // important to first update this time because of feedback loops in the rendering graph

        pullInputs(framesToProcess);

        bool silentInputs = inputsAreSilent();
        if (!silentInputs)
            m_lastNonSilentTime = (context()->currentSampleFrame() + framesToProcess) / static_cast<double>(m_sampleRate);

        if (silentInputs && propagatesSilence()) {
            silenceOutputs();
        } else {
            process(framesToProcess);
            unsilenceOutputs();
        }
    }
}

void AudioHandler::checkNumberOfChannelsForInput(AudioNodeInput* input)
{
    ASSERT(context()->isAudioThread());
    ASSERT(context()->isGraphOwner());

    ASSERT(m_inputs.contains(input));
    if (!m_inputs.contains(input))
        return;

    input->updateInternalBus();
}

double AudioHandler::tailTime() const
{
    return 0;
}

double AudioHandler::latencyTime() const
{
    return 0;
}

bool AudioHandler::propagatesSilence() const
{
    return m_lastNonSilentTime + latencyTime() + tailTime() < context()->currentTime();
}

void AudioHandler::pullInputs(size_t framesToProcess)
{
    ASSERT(context()->isAudioThread());

    // Process all of the AudioNodes connected to our inputs.
    for (unsigned i = 0; i < m_inputs.size(); ++i)
        input(i)->pull(0, framesToProcess);
}

bool AudioHandler::inputsAreSilent()
{
    for (unsigned i = 0; i < m_inputs.size(); ++i) {
        if (!input(i)->bus()->isSilent())
            return false;
    }
    return true;
}

void AudioHandler::silenceOutputs()
{
    for (unsigned i = 0; i < m_outputs.size(); ++i)
        output(i)->bus()->zero();
}

void AudioHandler::unsilenceOutputs()
{
    for (unsigned i = 0; i < m_outputs.size(); ++i)
        output(i)->bus()->clearSilentFlag();
}

void AudioHandler::enableOutputsIfNecessary()
{
    if (m_isDisabled && m_connectionRefCount > 0) {
        ASSERT(isMainThread());
        AudioContext::AutoLocker locker(context());

        m_isDisabled = false;
        for (unsigned i = 0; i < m_outputs.size(); ++i)
            output(i)->enable();
    }
}

void AudioHandler::disableOutputsIfNecessary()
{
    // Disable outputs if appropriate. We do this if the number of connections is 0 or 1. The case
    // of 0 is from deref() where there are no connections left. The case of 1 is from
    // AudioNodeInput::disable() where we want to disable outputs when there's only one connection
    // left because we're ready to go away, but can't quite yet.
    if (m_connectionRefCount <= 1 && !m_isDisabled) {
        // Still may have JavaScript references, but no more "active" connection references, so put all of our outputs in a "dormant" disabled state.
        // Garbage collection may take a very long time after this time, so the "dormant" disabled nodes should not bog down the rendering...

        // As far as JavaScript is concerned, our outputs must still appear to be connected.
        // But internally our outputs should be disabled from the inputs they're connected to.
        // disable() can recursively deref connections (and call disable()) down a whole chain of connected nodes.

        // FIXME: we special case the convolver and delay since they have a significant tail-time and shouldn't be disconnected simply
        // because they no longer have any input connections. This needs to be handled more generally where AudioNodes have
        // a tailTime attribute. Then the AudioNode only needs to remain "active" for tailTime seconds after there are no
        // longer any active connections.
        if (nodeType() != NodeTypeConvolver && nodeType() != NodeTypeDelay) {
            m_isDisabled = true;
            clearInternalStateWhenDisabled();
            for (unsigned i = 0; i < m_outputs.size(); ++i)
                output(i)->disable();
        }
    }
}

void AudioHandler::makeConnection()
{
    atomicIncrement(&m_connectionRefCount);

#if DEBUG_AUDIONODE_REFERENCES
    fprintf(stderr, "%p: %2d: AudioNode::ref   %3d [%3d]\n",
        this, nodeType(), m_connectionRefCount, s_nodeCount[nodeType()]);
#endif
    // See the disabling code in disableOutputsIfNecessary(). This handles
    // the case where a node is being re-connected after being used at least
    // once and disconnected. In this case, we need to re-enable.
    enableOutputsIfNecessary();
}

void AudioHandler::breakConnection()
{
    // The actual work for deref happens completely within the audio context's
    // graph lock. In the case of the audio thread, we must use a tryLock to
    // avoid glitches.
    bool hasLock = false;
    if (context()->isAudioThread()) {
        // Real-time audio thread must not contend lock (to avoid glitches).
        hasLock = context()->tryLock();
    } else {
        context()->lock();
        hasLock = true;
    }

    if (hasLock) {
        breakConnectionWithLock();
        context()->unlock();
    } else {
        // We were unable to get the lock, so put this in a list to finish up
        // later.
        ASSERT(context()->isAudioThread());
        context()->handler().addDeferredBreakConnection(*this);
    }
}

void AudioHandler::breakConnectionWithLock()
{
    atomicDecrement(&m_connectionRefCount);

#if DEBUG_AUDIONODE_REFERENCES
    fprintf(stderr, "%p: %2d: AudioNode::deref %3d [%3d]\n",
        this, nodeType(), m_connectionRefCount, s_nodeCount[nodeType()]);
#endif

    if (!m_connectionRefCount)
        disableOutputsIfNecessary();
}

#if DEBUG_AUDIONODE_REFERENCES

bool AudioHandler::s_isNodeCountInitialized = false;
int AudioHandler::s_nodeCount[NodeTypeEnd];

void AudioHandler::printNodeCounts()
{
    fprintf(stderr, "\n\n");
    fprintf(stderr, "===========================\n");
    fprintf(stderr, "AudioNode: reference counts\n");
    fprintf(stderr, "===========================\n");

    for (unsigned i = 0; i < NodeTypeEnd; ++i)
        fprintf(stderr, "%2d: %d\n", i, s_nodeCount[i]);

    fprintf(stderr, "===========================\n\n\n");
}

#endif // DEBUG_AUDIONODE_REFERENCES

DEFINE_TRACE(AudioHandler)
{
    visitor->trace(m_node);
    visitor->trace(m_context);
    // TODO(tkent): Oilpan: renderingOutputs should not be strong references.
    // This is a short-term workaround to avoid crashes, and causes AudioNode
    // leaks.
    {
        AudioContext::AutoLocker locker(context()->handler());
        for (const OwnPtr<AudioNodeInput>& input : m_inputs) {
            for (unsigned i = 0; i < input->numberOfRenderingConnections(); ++i)
                visitor->trace(input->renderingOutput(i)->node());
        }
    }
    visitor->trace(m_connectedNodes);
    visitor->trace(m_connectedParams);
}

void AudioHandler::updateChannelCountMode()
{
    m_channelCountMode = m_newChannelCountMode;
    updateChannelsForInputs();
}

// ----------------------------------------------------------------

AudioNode::AudioNode(AudioContext& context)
    : m_context(context)
    , m_handler(nullptr)
{
    m_context->registerLiveNode(*this);
    ThreadState::current()->registerPreFinalizer(*this);
}

void AudioNode::dispose()
{
    ASSERT(isMainThread());
    context()->unregisterLiveNode(*this);
    AudioContext::AutoLocker locker(context());
    handler().dispose();
}

void AudioNode::setHandler(AudioHandler* handler)
{
    ASSERT(handler);
    m_handler = handler;
}

AudioHandler& AudioNode::handler() const
{
    return *m_handler;
}

DEFINE_TRACE(AudioNode)
{
    visitor->trace(m_context);
    visitor->trace(m_handler);
    RefCountedGarbageCollectedEventTargetWithInlineData<AudioNode>::trace(visitor);
}

void AudioNode::connect(AudioNode* node, unsigned outputIndex, unsigned inputIndex, ExceptionState& exceptionState)
{
    handler().connect(&node->handler(), outputIndex, inputIndex, exceptionState);
}

void AudioNode::connect(AudioParam* param, unsigned outputIndex, ExceptionState& exceptionState)
{
    handler().connect(param, outputIndex, exceptionState);
}

void AudioNode::disconnect()
{
    handler().disconnect();
}

void AudioNode::disconnect(unsigned outputIndex, ExceptionState& exceptionState)
{
    handler().disconnect(outputIndex, exceptionState);
}

void AudioNode::disconnect(AudioNode* node, ExceptionState& exceptionState)
{
    handler().disconnect(&node->handler(), exceptionState);
}

void AudioNode::disconnect(AudioNode* node, unsigned outputIndex, ExceptionState& exceptionState)
{
    handler().disconnect(&node->handler(), outputIndex, exceptionState);
}

void AudioNode::disconnect(AudioNode* node, unsigned outputIndex, unsigned inputIndex, ExceptionState& exceptionState)
{
    handler().disconnect(&node->handler(), outputIndex, inputIndex, exceptionState);
}

void AudioNode::disconnect(AudioParam* param, ExceptionState& exceptionState)
{
    handler().disconnect(param, exceptionState);
}

void AudioNode::disconnect(AudioParam* param, unsigned outputIndex, ExceptionState& exceptionState)
{
    handler().disconnect(param, outputIndex, exceptionState);
}

AudioContext* AudioNode::context() const
{
    return m_context;
}

unsigned AudioNode::numberOfInputs() const
{
    return handler().numberOfInputs();
}

unsigned AudioNode::numberOfOutputs() const
{
    return handler().numberOfOutputs();
}

unsigned long AudioNode::channelCount() const
{
    return handler().channelCount();
}

void AudioNode::setChannelCount(unsigned long count, ExceptionState& exceptionState)
{
    handler().setChannelCount(count, exceptionState);
}

String AudioNode::channelCountMode() const
{
    return handler().channelCountMode();
}

void AudioNode::setChannelCountMode(const String& mode, ExceptionState& exceptionState)
{
    handler().setChannelCountMode(mode, exceptionState);
}

String AudioNode::channelInterpretation() const
{
    return handler().channelInterpretation();
}

void AudioNode::setChannelInterpretation(const String& interpretation, ExceptionState& exceptionState)
{
    handler().setChannelInterpretation(interpretation, exceptionState);
}

const AtomicString& AudioNode::interfaceName() const
{
    return EventTargetNames::AudioNode;
}

ExecutionContext* AudioNode::executionContext() const
{
    return context()->executionContext();
}

} // namespace blink

#endif // ENABLE(WEB_AUDIO)
