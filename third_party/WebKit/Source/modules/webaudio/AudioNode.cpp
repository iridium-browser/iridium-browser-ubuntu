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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "modules/webaudio/AudioNode.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webaudio/AudioNodeInput.h"
#include "modules/webaudio/AudioNodeOptions.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "modules/webaudio/AudioParam.h"
#include "modules/webaudio/BaseAudioContext.h"
#include "platform/InstanceCounters.h"
#include "wtf/Atomics.h"

#if DEBUG_AUDIONODE_REFERENCES
#include <stdio.h>
#endif

namespace blink {

AudioHandler::AudioHandler(NodeType nodeType, AudioNode& node, float sampleRate)
    : m_isInitialized(false),
      m_nodeType(NodeTypeUnknown),
      m_node(&node),
      m_context(node.context()),
      m_sampleRate(sampleRate),
      m_lastProcessingTime(-1),
      m_lastNonSilentTime(-1),
      m_connectionRefCount(0),
      m_isDisabled(false),
      m_channelCount(2) {
  setNodeType(nodeType);
  setInternalChannelCountMode(Max);
  setInternalChannelInterpretation(AudioBus::Speakers);

#if DEBUG_AUDIONODE_REFERENCES
  if (!s_isNodeCountInitialized) {
    s_isNodeCountInitialized = true;
    atexit(AudioHandler::printNodeCounts);
  }
#endif
  InstanceCounters::incrementCounter(InstanceCounters::AudioHandlerCounter);
}

AudioHandler::~AudioHandler() {
  DCHECK(isMainThread());
  // dispose() should be called.
  DCHECK(!node());
  InstanceCounters::decrementCounter(InstanceCounters::AudioHandlerCounter);
#if DEBUG_AUDIONODE_REFERENCES
  --s_nodeCount[getNodeType()];
  fprintf(stderr, "[%16p]: %16p: %2d: AudioHandler::~AudioHandler() %d [%d]\n",
          context(), this, getNodeType(), m_connectionRefCount,
          s_nodeCount[getNodeType()]);
#endif
}

void AudioHandler::initialize() {
  DCHECK_EQ(m_newChannelCountMode, m_channelCountMode);
  DCHECK_EQ(m_newChannelInterpretation, m_channelInterpretation);

  m_isInitialized = true;
}

void AudioHandler::uninitialize() {
  m_isInitialized = false;
}

void AudioHandler::clearInternalStateWhenDisabled() {}

void AudioHandler::dispose() {
  DCHECK(isMainThread());
  ASSERT(context()->isGraphOwner());

  context()->deferredTaskHandler().removeChangedChannelCountMode(this);
  context()->deferredTaskHandler().removeChangedChannelInterpretation(this);
  context()->deferredTaskHandler().removeAutomaticPullNode(this);
  for (auto& output : m_outputs)
    output->dispose();
  m_node = nullptr;
}

AudioNode* AudioHandler::node() const {
  DCHECK(isMainThread());
  return m_node;
}

BaseAudioContext* AudioHandler::context() const {
  return m_context;
}

String AudioHandler::nodeTypeName() const {
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

void AudioHandler::setNodeType(NodeType type) {
  // Don't allow the node type to be changed to a different node type, after
  // it's already been set.  And the new type can't be unknown or end.
  DCHECK_EQ(m_nodeType, NodeTypeUnknown);
  DCHECK_NE(type, NodeTypeUnknown);
  DCHECK_NE(type, NodeTypeEnd);

  m_nodeType = type;

#if DEBUG_AUDIONODE_REFERENCES
  ++s_nodeCount[type];
  fprintf(stderr, "[%16p]: %16p: %2d: AudioHandler::AudioHandler [%3d]\n",
          context(), this, getNodeType(), s_nodeCount[getNodeType()]);
#endif
}

void AudioHandler::addInput() {
  m_inputs.push_back(AudioNodeInput::create(*this));
}

void AudioHandler::addOutput(unsigned numberOfChannels) {
  DCHECK(isMainThread());
  m_outputs.push_back(AudioNodeOutput::create(this, numberOfChannels));
  node()->didAddOutput(numberOfOutputs());
}

AudioNodeInput& AudioHandler::input(unsigned i) {
  return *m_inputs[i];
}

AudioNodeOutput& AudioHandler::output(unsigned i) {
  return *m_outputs[i];
}

unsigned long AudioHandler::channelCount() {
  return m_channelCount;
}

void AudioHandler::setInternalChannelCountMode(ChannelCountMode mode) {
  m_channelCountMode = mode;
  m_newChannelCountMode = mode;
}

void AudioHandler::setInternalChannelInterpretation(
    AudioBus::ChannelInterpretation interpretation) {
  m_channelInterpretation = interpretation;
  m_newChannelInterpretation = interpretation;
}

void AudioHandler::setChannelCount(unsigned long channelCount,
                                   ExceptionState& exceptionState) {
  DCHECK(isMainThread());
  BaseAudioContext::AutoLocker locker(context());

  if (channelCount > 0 &&
      channelCount <= BaseAudioContext::maxNumberOfChannels()) {
    if (m_channelCount != channelCount) {
      m_channelCount = channelCount;
      if (m_channelCountMode != Max)
        updateChannelsForInputs();
    }
  } else {
    exceptionState.throwDOMException(
        NotSupportedError,
        ExceptionMessages::indexOutsideRange<unsigned long>(
            "channel count", channelCount, 1, ExceptionMessages::InclusiveBound,
            BaseAudioContext::maxNumberOfChannels(),
            ExceptionMessages::InclusiveBound));
  }
}

String AudioHandler::channelCountMode() {
  // Because we delay the actual setting of the mode to the pre or post
  // rendering phase, we want to return the value that was set, not the actual
  // current mode.
  switch (m_newChannelCountMode) {
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

void AudioHandler::setChannelCountMode(const String& mode,
                                       ExceptionState& exceptionState) {
  DCHECK(isMainThread());
  BaseAudioContext::AutoLocker locker(context());

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
    context()->deferredTaskHandler().addChangedChannelCountMode(this);
}

String AudioHandler::channelInterpretation() {
  // Because we delay the actual setting of the interpreation to the pre or
  // post rendering phase, we want to return the value that was set, not the
  // actual current interpretation.
  switch (m_newChannelInterpretation) {
    case AudioBus::Speakers:
      return "speakers";
    case AudioBus::Discrete:
      return "discrete";
  }
  ASSERT_NOT_REACHED();
  return "";
}

void AudioHandler::setChannelInterpretation(const String& interpretation,
                                            ExceptionState& exceptionState) {
  DCHECK(isMainThread());
  BaseAudioContext::AutoLocker locker(context());

  AudioBus::ChannelInterpretation oldMode = m_channelInterpretation;

  if (interpretation == "speakers") {
    m_newChannelInterpretation = AudioBus::Speakers;
  } else if (interpretation == "discrete") {
    m_newChannelInterpretation = AudioBus::Discrete;
  } else {
    ASSERT_NOT_REACHED();
  }

  if (m_newChannelInterpretation != oldMode)
    context()->deferredTaskHandler().addChangedChannelInterpretation(this);
}

void AudioHandler::updateChannelsForInputs() {
  for (auto& input : m_inputs)
    input->changedOutputs();
}

void AudioHandler::processIfNecessary(size_t framesToProcess) {
  DCHECK(context()->isAudioThread());

  if (!isInitialized())
    return;

  // Ensure that we only process once per rendering quantum.
  // This handles the "fanout" problem where an output is connected to multiple
  // inputs.  The first time we're called during this time slice we process, but
  // after that we don't want to re-process, instead our output(s) will already
  // have the results cached in their bus;
  double currentTime = context()->currentTime();
  if (m_lastProcessingTime != currentTime) {
    // important to first update this time because of feedback loops in the
    // rendering graph.
    m_lastProcessingTime = currentTime;

    pullInputs(framesToProcess);

    bool silentInputs = inputsAreSilent();
    if (!silentInputs)
      m_lastNonSilentTime =
          (context()->currentSampleFrame() + framesToProcess) /
          static_cast<double>(m_sampleRate);

    if (silentInputs && propagatesSilence()) {
      silenceOutputs();
      // AudioParams still need to be processed so that the value can be updated
      // if there are automations or so that the upstream nodes get pulled if
      // any are connected to the AudioParam.
      processOnlyAudioParams(framesToProcess);
    } else {
      // Unsilence the outputs first because the processing of the node may
      // cause the outputs to go silent and we want to propagate that hint to
      // the downstream nodes.  (For example, a Gain node with a gain of 0 will
      // want to silence its output.)
      unsilenceOutputs();
      process(framesToProcess);
    }
  }
}

void AudioHandler::checkNumberOfChannelsForInput(AudioNodeInput* input) {
  DCHECK(context()->isAudioThread());
  ASSERT(context()->isGraphOwner());

  DCHECK(m_inputs.contains(input));
  if (!m_inputs.contains(input))
    return;

  input->updateInternalBus();
}

double AudioHandler::tailTime() const {
  return 0;
}

double AudioHandler::latencyTime() const {
  return 0;
}

bool AudioHandler::propagatesSilence() const {
  return m_lastNonSilentTime + latencyTime() + tailTime() <
         context()->currentTime();
}

void AudioHandler::pullInputs(size_t framesToProcess) {
  DCHECK(context()->isAudioThread());

  // Process all of the AudioNodes connected to our inputs.
  for (auto& input : m_inputs)
    input->pull(0, framesToProcess);
}

bool AudioHandler::inputsAreSilent() {
  for (auto& input : m_inputs) {
    if (!input->bus()->isSilent())
      return false;
  }
  return true;
}

void AudioHandler::silenceOutputs() {
  for (auto& output : m_outputs)
    output->bus()->zero();
}

void AudioHandler::unsilenceOutputs() {
  for (auto& output : m_outputs)
    output->bus()->clearSilentFlag();
}

void AudioHandler::enableOutputsIfNecessary() {
  if (m_isDisabled && m_connectionRefCount > 0) {
    DCHECK(isMainThread());
    BaseAudioContext::AutoLocker locker(context());

    m_isDisabled = false;
    for (auto& output : m_outputs)
      output->enable();
  }
}

void AudioHandler::disableOutputsIfNecessary() {
  // Disable outputs if appropriate. We do this if the number of connections is
  // 0 or 1. The case of 0 is from deref() where there are no connections left.
  // The case of 1 is from AudioNodeInput::disable() where we want to disable
  // outputs when there's only one connection left because we're ready to go
  // away, but can't quite yet.
  if (m_connectionRefCount <= 1 && !m_isDisabled) {
    // Still may have JavaScript references, but no more "active" connection
    // references, so put all of our outputs in a "dormant" disabled state.
    // Garbage collection may take a very long time after this time, so the
    // "dormant" disabled nodes should not bog down the rendering...

    // As far as JavaScript is concerned, our outputs must still appear to be
    // connected.  But internally our outputs should be disabled from the inputs
    // they're connected to.  disable() can recursively deref connections (and
    // call disable()) down a whole chain of connected nodes.

    // TODO(rtoy,hongchan): we need special cases the convolver, delay, biquad,
    // and IIR since they have a significant tail-time and shouldn't be
    // disconnected simply because they no longer have any input connections.
    // This needs to be handled more generally where AudioNodes have a tailTime
    // attribute. Then the AudioNode only needs to remain "active" for tailTime
    // seconds after there are no longer any active connections.
    if (getNodeType() != NodeTypeConvolver && getNodeType() != NodeTypeDelay &&
        getNodeType() != NodeTypeBiquadFilter &&
        getNodeType() != NodeTypeIIRFilter) {
      m_isDisabled = true;
      clearInternalStateWhenDisabled();
      for (auto& output : m_outputs)
        output->disable();
    }
  }
}

void AudioHandler::makeConnection() {
  atomicIncrement(&m_connectionRefCount);

#if DEBUG_AUDIONODE_REFERENCES
  fprintf(stderr, "[%16p]: %16p: %2d: AudioHandler::ref   %3d [%3d]\n",
          context(), this, getNodeType(), m_connectionRefCount,
          s_nodeCount[getNodeType()]);
#endif
  // See the disabling code in disableOutputsIfNecessary(). This handles
  // the case where a node is being re-connected after being used at least
  // once and disconnected. In this case, we need to re-enable.
  enableOutputsIfNecessary();
}

void AudioHandler::breakConnection() {
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
    DCHECK(context()->isAudioThread());
    context()->deferredTaskHandler().addDeferredBreakConnection(*this);
  }
}

void AudioHandler::breakConnectionWithLock() {
  atomicDecrement(&m_connectionRefCount);

#if DEBUG_AUDIONODE_REFERENCES
  fprintf(stderr, "[%16p]: %16p: %2d: AudioHandler::deref %3d [%3d]\n",
          context(), this, getNodeType(), m_connectionRefCount,
          s_nodeCount[getNodeType()]);
#endif

  if (!m_connectionRefCount)
    disableOutputsIfNecessary();
}

#if DEBUG_AUDIONODE_REFERENCES

bool AudioHandler::s_isNodeCountInitialized = false;
int AudioHandler::s_nodeCount[NodeTypeEnd];

void AudioHandler::printNodeCounts() {
  fprintf(stderr, "\n\n");
  fprintf(stderr, "===========================\n");
  fprintf(stderr, "AudioNode: reference counts\n");
  fprintf(stderr, "===========================\n");

  for (unsigned i = 0; i < NodeTypeEnd; ++i)
    fprintf(stderr, "%2d: %d\n", i, s_nodeCount[i]);

  fprintf(stderr, "===========================\n\n\n");
}

#endif  // DEBUG_AUDIONODE_REFERENCES

void AudioHandler::updateChannelCountMode() {
  m_channelCountMode = m_newChannelCountMode;
  updateChannelsForInputs();
}

void AudioHandler::updateChannelInterpretation() {
  m_channelInterpretation = m_newChannelInterpretation;
}

unsigned AudioHandler::numberOfOutputChannels() const {
  // This should only be called for ScriptProcessorNodes which are the only
  // nodes where you can have an output with 0 channels.  All other nodes have
  // have at least one output channel, so there's no reason other nodes should
  // ever call this function.
  DCHECK(0) << "numberOfOutputChannels() not valid for node type "
            << getNodeType();
  return 1;
}
// ----------------------------------------------------------------

AudioNode::AudioNode(BaseAudioContext& context)
    : m_context(context), m_handler(nullptr) {
}

void AudioNode::dispose() {
  DCHECK(isMainThread());
#if DEBUG_AUDIONODE_REFERENCES
  fprintf(stderr, "[%16p]: %16p: %2d: AudioNode::dispose %16p\n", context(),
          this, handler().getNodeType(), m_handler.get());
#endif
  BaseAudioContext::AutoLocker locker(context());
  handler().dispose();
  if (context()->contextState() == BaseAudioContext::Running)
    context()->deferredTaskHandler().addRenderingOrphanHandler(
        m_handler.release());
}

void AudioNode::setHandler(PassRefPtr<AudioHandler> handler) {
  DCHECK(handler);
  m_handler = handler;

#if DEBUG_AUDIONODE_REFERENCES
  fprintf(stderr, "[%16p]: %16p: %2d: AudioNode::AudioNode %16p\n", context(),
          this, m_handler->getNodeType(), m_handler.get());
#endif
}

AudioHandler& AudioNode::handler() const {
  return *m_handler;
}

DEFINE_TRACE(AudioNode) {
  visitor->trace(m_context);
  visitor->trace(m_connectedNodes);
  visitor->trace(m_connectedParams);
  EventTargetWithInlineData::trace(visitor);
}

void AudioNode::handleChannelOptions(const AudioNodeOptions& options,
                                     ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  if (options.hasChannelCount())
    setChannelCount(options.channelCount(), exceptionState);
  if (options.hasChannelCountMode())
    setChannelCountMode(options.channelCountMode(), exceptionState);
  if (options.hasChannelInterpretation())
    setChannelInterpretation(options.channelInterpretation(), exceptionState);
}

BaseAudioContext* AudioNode::context() const {
  return m_context;
}

AudioNode* AudioNode::connect(AudioNode* destination,
                              unsigned outputIndex,
                              unsigned inputIndex,
                              ExceptionState& exceptionState) {
  DCHECK(isMainThread());
  BaseAudioContext::AutoLocker locker(context());

  if (context()->isContextClosed()) {
    exceptionState.throwDOMException(
        InvalidStateError, "Cannot connect after the context has been closed.");
    return nullptr;
  }

  if (!destination) {
    exceptionState.throwDOMException(SyntaxError, "invalid destination node.");
    return nullptr;
  }

  // Sanity check input and output indices.
  if (outputIndex >= numberOfOutputs()) {
    exceptionState.throwDOMException(
        IndexSizeError, "output index (" + String::number(outputIndex) +
                            ") exceeds number of outputs (" +
                            String::number(numberOfOutputs()) + ").");
    return nullptr;
  }

  if (destination && inputIndex >= destination->numberOfInputs()) {
    exceptionState.throwDOMException(
        IndexSizeError, "input index (" + String::number(inputIndex) +
                            ") exceeds number of inputs (" +
                            String::number(destination->numberOfInputs()) +
                            ").");
    return nullptr;
  }

  if (context() != destination->context()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "cannot connect to a destination "
                                     "belonging to a different audio context.");
    return nullptr;
  }

  // ScriptProcessorNodes with 0 output channels can't be connected to any
  // destination.  If there are no output channels, what would the destination
  // receive?  Just disallow this.
  if (handler().getNodeType() == AudioHandler::NodeTypeJavaScript &&
      handler().numberOfOutputChannels() == 0) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "cannot connect a ScriptProcessorNode "
                                     "with 0 output channels to any "
                                     "destination node.");
    return nullptr;
  }

  destination->handler()
      .input(inputIndex)
      .connect(handler().output(outputIndex));
  if (!m_connectedNodes[outputIndex])
    m_connectedNodes[outputIndex] = new HeapHashSet<Member<AudioNode>>();
  m_connectedNodes[outputIndex]->add(destination);

  // Let context know that a connection has been made.
  context()->incrementConnectionCount();

  return destination;
}

void AudioNode::connect(AudioParam* param,
                        unsigned outputIndex,
                        ExceptionState& exceptionState) {
  DCHECK(isMainThread());
  BaseAudioContext::AutoLocker locker(context());

  if (context()->isContextClosed()) {
    exceptionState.throwDOMException(
        InvalidStateError, "Cannot connect after the context has been closed.");
    return;
  }

  if (!param) {
    exceptionState.throwDOMException(SyntaxError, "invalid AudioParam.");
    return;
  }

  if (outputIndex >= numberOfOutputs()) {
    exceptionState.throwDOMException(
        IndexSizeError, "output index (" + String::number(outputIndex) +
                            ") exceeds number of outputs (" +
                            String::number(numberOfOutputs()) + ").");
    return;
  }

  if (context() != param->context()) {
    exceptionState.throwDOMException(SyntaxError,
                                     "cannot connect to an AudioParam "
                                     "belonging to a different audio context.");
    return;
  }

  param->handler().connect(handler().output(outputIndex));
  if (!m_connectedParams[outputIndex])
    m_connectedParams[outputIndex] = new HeapHashSet<Member<AudioParam>>();
  m_connectedParams[outputIndex]->add(param);
}

void AudioNode::disconnectAllFromOutput(unsigned outputIndex) {
  handler().output(outputIndex).disconnectAll();
  m_connectedNodes[outputIndex] = nullptr;
  m_connectedParams[outputIndex] = nullptr;
}

bool AudioNode::disconnectFromOutputIfConnected(
    unsigned outputIndex,
    AudioNode& destination,
    unsigned inputIndexOfDestination) {
  AudioNodeOutput& output = handler().output(outputIndex);
  AudioNodeInput& input = destination.handler().input(inputIndexOfDestination);
  if (!output.isConnectedToInput(input))
    return false;
  output.disconnectInput(input);
  m_connectedNodes[outputIndex]->remove(&destination);
  return true;
}

bool AudioNode::disconnectFromOutputIfConnected(unsigned outputIndex,
                                                AudioParam& param) {
  AudioNodeOutput& output = handler().output(outputIndex);
  if (!output.isConnectedToAudioParam(param.handler()))
    return false;
  output.disconnectAudioParam(param.handler());
  m_connectedParams[outputIndex]->remove(&param);
  return true;
}

void AudioNode::disconnect() {
  DCHECK(isMainThread());
  BaseAudioContext::AutoLocker locker(context());

  // Disconnect all outgoing connections.
  for (unsigned i = 0; i < numberOfOutputs(); ++i)
    disconnectAllFromOutput(i);
}

void AudioNode::disconnect(unsigned outputIndex,
                           ExceptionState& exceptionState) {
  DCHECK(isMainThread());
  BaseAudioContext::AutoLocker locker(context());

  // Sanity check on the output index.
  if (outputIndex >= numberOfOutputs()) {
    exceptionState.throwDOMException(
        IndexSizeError,
        ExceptionMessages::indexOutsideRange(
            "output index", outputIndex, 0u, ExceptionMessages::InclusiveBound,
            numberOfOutputs() - 1, ExceptionMessages::InclusiveBound));
    return;
  }
  // Disconnect all outgoing connections from the given output.
  disconnectAllFromOutput(outputIndex);
}

void AudioNode::disconnect(AudioNode* destination,
                           ExceptionState& exceptionState) {
  DCHECK(isMainThread());
  BaseAudioContext::AutoLocker locker(context());

  unsigned numberOfDisconnections = 0;

  // FIXME: Can this be optimized? ChannelSplitter and ChannelMerger can have
  // 32 ports and that requires 1024 iterations to validate entire connections.
  for (unsigned outputIndex = 0; outputIndex < numberOfOutputs();
       ++outputIndex) {
    for (unsigned inputIndex = 0;
         inputIndex < destination->handler().numberOfInputs(); ++inputIndex) {
      if (disconnectFromOutputIfConnected(outputIndex, *destination,
                                          inputIndex))
        numberOfDisconnections++;
    }
  }

  // If there is no connection to the destination, throw an exception.
  if (numberOfDisconnections == 0) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "the given destination is not connected.");
    return;
  }
}

void AudioNode::disconnect(AudioNode* destination,
                           unsigned outputIndex,
                           ExceptionState& exceptionState) {
  DCHECK(isMainThread());
  BaseAudioContext::AutoLocker locker(context());

  if (outputIndex >= numberOfOutputs()) {
    // The output index is out of range. Throw an exception.
    exceptionState.throwDOMException(
        IndexSizeError,
        ExceptionMessages::indexOutsideRange(
            "output index", outputIndex, 0u, ExceptionMessages::InclusiveBound,
            numberOfOutputs() - 1, ExceptionMessages::InclusiveBound));
    return;
  }

  // If the output index is valid, proceed to disconnect.
  unsigned numberOfDisconnections = 0;
  // Sanity check on destination inputs and disconnect when possible.
  for (unsigned inputIndex = 0; inputIndex < destination->numberOfInputs();
       ++inputIndex) {
    if (disconnectFromOutputIfConnected(outputIndex, *destination, inputIndex))
      numberOfDisconnections++;
  }

  // If there is no connection to the destination, throw an exception.
  if (numberOfDisconnections == 0) {
    exceptionState.throwDOMException(
        InvalidAccessError, "output (" + String::number(outputIndex) +
                                ") is not connected to the given destination.");
  }
}

void AudioNode::disconnect(AudioNode* destination,
                           unsigned outputIndex,
                           unsigned inputIndex,
                           ExceptionState& exceptionState) {
  DCHECK(isMainThread());
  BaseAudioContext::AutoLocker locker(context());

  if (outputIndex >= numberOfOutputs()) {
    exceptionState.throwDOMException(
        IndexSizeError,
        ExceptionMessages::indexOutsideRange(
            "output index", outputIndex, 0u, ExceptionMessages::InclusiveBound,
            numberOfOutputs() - 1, ExceptionMessages::InclusiveBound));
    return;
  }

  if (inputIndex >= destination->handler().numberOfInputs()) {
    exceptionState.throwDOMException(
        IndexSizeError,
        ExceptionMessages::indexOutsideRange(
            "input index", inputIndex, 0u, ExceptionMessages::InclusiveBound,
            destination->numberOfInputs() - 1,
            ExceptionMessages::InclusiveBound));
    return;
  }

  // If both indices are valid, proceed to disconnect.
  if (!disconnectFromOutputIfConnected(outputIndex, *destination, inputIndex)) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "output (" + String::number(outputIndex) +
                                         ") is not connected to the input (" +
                                         String::number(inputIndex) +
                                         ") of the destination.");
    return;
  }
}

void AudioNode::disconnect(AudioParam* destinationParam,
                           ExceptionState& exceptionState) {
  DCHECK(isMainThread());
  BaseAudioContext::AutoLocker locker(context());

  // The number of disconnection made.
  unsigned numberOfDisconnections = 0;

  // Check if the node output is connected the destination AudioParam.
  // Disconnect if connected and increase |numberOfDisconnectios| by 1.
  for (unsigned outputIndex = 0; outputIndex < handler().numberOfOutputs();
       ++outputIndex) {
    if (disconnectFromOutputIfConnected(outputIndex, *destinationParam))
      numberOfDisconnections++;
  }

  // Throw an exception when there is no valid connection to the destination.
  if (numberOfDisconnections == 0) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "the given AudioParam is not connected.");
    return;
  }
}

void AudioNode::disconnect(AudioParam* destinationParam,
                           unsigned outputIndex,
                           ExceptionState& exceptionState) {
  DCHECK(isMainThread());
  BaseAudioContext::AutoLocker locker(context());

  if (outputIndex >= handler().numberOfOutputs()) {
    // The output index is out of range. Throw an exception.
    exceptionState.throwDOMException(
        IndexSizeError,
        ExceptionMessages::indexOutsideRange(
            "output index", outputIndex, 0u, ExceptionMessages::InclusiveBound,
            numberOfOutputs() - 1, ExceptionMessages::InclusiveBound));
    return;
  }

  // If the output index is valid, proceed to disconnect.
  if (!disconnectFromOutputIfConnected(outputIndex, *destinationParam)) {
    exceptionState.throwDOMException(
        InvalidAccessError,
        "specified destination AudioParam and node output (" +
            String::number(outputIndex) + ") are not connected.");
    return;
  }
}

void AudioNode::disconnectWithoutException(unsigned outputIndex) {
  DCHECK(isMainThread());
  BaseAudioContext::AutoLocker locker(context());

  // Sanity check input and output indices.
  if (outputIndex >= handler().numberOfOutputs())
    return;
  disconnectAllFromOutput(outputIndex);
}

unsigned AudioNode::numberOfInputs() const {
  return handler().numberOfInputs();
}

unsigned AudioNode::numberOfOutputs() const {
  return handler().numberOfOutputs();
}

unsigned long AudioNode::channelCount() const {
  return handler().channelCount();
}

void AudioNode::setChannelCount(unsigned long count,
                                ExceptionState& exceptionState) {
  handler().setChannelCount(count, exceptionState);
}

String AudioNode::channelCountMode() const {
  return handler().channelCountMode();
}

void AudioNode::setChannelCountMode(const String& mode,
                                    ExceptionState& exceptionState) {
  handler().setChannelCountMode(mode, exceptionState);
}

String AudioNode::channelInterpretation() const {
  return handler().channelInterpretation();
}

void AudioNode::setChannelInterpretation(const String& interpretation,
                                         ExceptionState& exceptionState) {
  handler().setChannelInterpretation(interpretation, exceptionState);
}

const AtomicString& AudioNode::interfaceName() const {
  return EventTargetNames::AudioNode;
}

ExecutionContext* AudioNode::getExecutionContext() const {
  return context()->getExecutionContext();
}

void AudioNode::didAddOutput(unsigned numberOfOutputs) {
  m_connectedNodes.push_back(nullptr);
  DCHECK_EQ(numberOfOutputs, m_connectedNodes.size());
  m_connectedParams.push_back(nullptr);
  DCHECK_EQ(numberOfOutputs, m_connectedParams.size());
}

}  // namespace blink
