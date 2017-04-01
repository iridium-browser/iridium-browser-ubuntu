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

#include "modules/webaudio/ScriptProcessorNode.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/dom/TaskRunnerHelper.h"
#include "modules/webaudio/AudioBuffer.h"
#include "modules/webaudio/AudioNodeInput.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "modules/webaudio/AudioProcessingEvent.h"
#include "modules/webaudio/BaseAudioContext.h"
#include "platform/WaitableEvent.h"
#include "public/platform/Platform.h"

namespace blink {

ScriptProcessorHandler::ScriptProcessorHandler(AudioNode& node,
                                               float sampleRate,
                                               size_t bufferSize,
                                               unsigned numberOfInputChannels,
                                               unsigned numberOfOutputChannels)
    : AudioHandler(NodeTypeJavaScript, node, sampleRate),
      m_doubleBufferIndex(0),
      m_bufferSize(bufferSize),
      m_bufferReadWriteIndex(0),
      m_numberOfInputChannels(numberOfInputChannels),
      m_numberOfOutputChannels(numberOfOutputChannels),
      m_internalInputBus(AudioBus::create(numberOfInputChannels,
                                          AudioUtilities::kRenderQuantumFrames,
                                          false)) {
  // Regardless of the allowed buffer sizes, we still need to process at the
  // granularity of the AudioNode.
  if (m_bufferSize < AudioUtilities::kRenderQuantumFrames)
    m_bufferSize = AudioUtilities::kRenderQuantumFrames;

  DCHECK_LE(numberOfInputChannels, BaseAudioContext::maxNumberOfChannels());

  addInput();
  addOutput(numberOfOutputChannels);

  m_channelCount = numberOfInputChannels;
  setInternalChannelCountMode(Explicit);

  initialize();
}

PassRefPtr<ScriptProcessorHandler> ScriptProcessorHandler::create(
    AudioNode& node,
    float sampleRate,
    size_t bufferSize,
    unsigned numberOfInputChannels,
    unsigned numberOfOutputChannels) {
  return adoptRef(new ScriptProcessorHandler(node, sampleRate, bufferSize,
                                             numberOfInputChannels,
                                             numberOfOutputChannels));
}

ScriptProcessorHandler::~ScriptProcessorHandler() {
  uninitialize();
}

void ScriptProcessorHandler::initialize() {
  if (isInitialized())
    return;

  float sampleRate = context()->sampleRate();

  // Create double buffers on both the input and output sides.
  // These AudioBuffers will be directly accessed in the main thread by
  // JavaScript.
  for (unsigned i = 0; i < 2; ++i) {
    AudioBuffer* inputBuffer =
        m_numberOfInputChannels ? AudioBuffer::create(m_numberOfInputChannels,
                                                      bufferSize(), sampleRate)
                                : 0;
    AudioBuffer* outputBuffer =
        m_numberOfOutputChannels ? AudioBuffer::create(m_numberOfOutputChannels,
                                                       bufferSize(), sampleRate)
                                 : 0;

    m_inputBuffers.push_back(inputBuffer);
    m_outputBuffers.push_back(outputBuffer);
  }

  AudioHandler::initialize();
}

void ScriptProcessorHandler::process(size_t framesToProcess) {
  // Discussion about inputs and outputs:
  // As in other AudioNodes, ScriptProcessorNode uses an AudioBus for its input
  // and output (see inputBus and outputBus below).  Additionally, there is a
  // double-buffering for input and output which is exposed directly to
  // JavaScript (see inputBuffer and outputBuffer below).  This node is the
  // producer for inputBuffer and the consumer for outputBuffer.  The JavaScript
  // code is the consumer of inputBuffer and the producer for outputBuffer.

  // Get input and output busses.
  AudioBus* inputBus = input(0).bus();
  AudioBus* outputBus = output(0).bus();

  // Get input and output buffers. We double-buffer both the input and output
  // sides.
  unsigned doubleBufferIndex = this->doubleBufferIndex();
  bool isDoubleBufferIndexGood = doubleBufferIndex < 2 &&
                                 doubleBufferIndex < m_inputBuffers.size() &&
                                 doubleBufferIndex < m_outputBuffers.size();
  DCHECK(isDoubleBufferIndexGood);
  if (!isDoubleBufferIndexGood)
    return;

  AudioBuffer* inputBuffer = m_inputBuffers[doubleBufferIndex].get();
  AudioBuffer* outputBuffer = m_outputBuffers[doubleBufferIndex].get();

  // Check the consistency of input and output buffers.
  unsigned numberOfInputChannels = m_internalInputBus->numberOfChannels();
  bool buffersAreGood =
      outputBuffer && bufferSize() == outputBuffer->length() &&
      m_bufferReadWriteIndex + framesToProcess <= bufferSize();

  // If the number of input channels is zero, it's ok to have inputBuffer = 0.
  if (m_internalInputBus->numberOfChannels())
    buffersAreGood =
        buffersAreGood && inputBuffer && bufferSize() == inputBuffer->length();

  DCHECK(buffersAreGood);
  if (!buffersAreGood)
    return;

  // We assume that bufferSize() is evenly divisible by framesToProcess - should
  // always be true, but we should still check.
  bool isFramesToProcessGood = framesToProcess &&
                               bufferSize() >= framesToProcess &&
                               !(bufferSize() % framesToProcess);
  DCHECK(isFramesToProcessGood);
  if (!isFramesToProcessGood)
    return;

  unsigned numberOfOutputChannels = outputBus->numberOfChannels();

  bool channelsAreGood = (numberOfInputChannels == m_numberOfInputChannels) &&
                         (numberOfOutputChannels == m_numberOfOutputChannels);
  DCHECK(channelsAreGood);
  if (!channelsAreGood)
    return;

  for (unsigned i = 0; i < numberOfInputChannels; ++i)
    m_internalInputBus->setChannelMemory(
        i, inputBuffer->getChannelData(i)->data() + m_bufferReadWriteIndex,
        framesToProcess);

  if (numberOfInputChannels)
    m_internalInputBus->copyFrom(*inputBus);

  // Copy from the output buffer to the output.
  for (unsigned i = 0; i < numberOfOutputChannels; ++i)
    memcpy(outputBus->channel(i)->mutableData(),
           outputBuffer->getChannelData(i)->data() + m_bufferReadWriteIndex,
           sizeof(float) * framesToProcess);

  // Update the buffering index.
  m_bufferReadWriteIndex =
      (m_bufferReadWriteIndex + framesToProcess) % bufferSize();

  // m_bufferReadWriteIndex will wrap back around to 0 when the current input
  // and output buffers are full.
  // When this happens, fire an event and swap buffers.
  if (!m_bufferReadWriteIndex) {
    // Avoid building up requests on the main thread to fire process events when
    // they're not being handled.  This could be a problem if the main thread is
    // very busy doing other things and is being held up handling previous
    // requests.  The audio thread can't block on this lock, so we call
    // tryLock() instead.
    MutexTryLocker tryLocker(m_processEventLock);
    if (!tryLocker.locked()) {
      // We're late in handling the previous request. The main thread must be
      // very busy.  The best we can do is clear out the buffer ourself here.
      outputBuffer->zero();
    } else if (context()->getExecutionContext()) {
      // With the realtime context, execute the script code asynchronously
      // and do not wait.
      if (context()->hasRealtimeConstraint()) {
        // Fire the event on the main thread with the appropriate buffer
        // index.
        context()->getExecutionContext()->postTask(
            TaskType::MediaElementEvent, BLINK_FROM_HERE,
            createCrossThreadTask(&ScriptProcessorHandler::fireProcessEvent,
                                  crossThreadUnretained(this),
                                  m_doubleBufferIndex));
      } else {
        // If this node is in the offline audio context, use the
        // waitable event to synchronize to the offline rendering thread.
        std::unique_ptr<WaitableEvent> waitableEvent =
            WTF::makeUnique<WaitableEvent>();

        context()->getExecutionContext()->postTask(
            TaskType::MediaElementEvent, BLINK_FROM_HERE,
            createCrossThreadTask(
                &ScriptProcessorHandler::fireProcessEventForOfflineAudioContext,
                crossThreadUnretained(this), m_doubleBufferIndex,
                crossThreadUnretained(waitableEvent.get())));

        // Okay to block the offline audio rendering thread since it is
        // not the actual audio device thread.
        waitableEvent->wait();
      }
    }

    swapBuffers();
  }
}

void ScriptProcessorHandler::fireProcessEvent(unsigned doubleBufferIndex) {
  DCHECK(isMainThread());

  DCHECK_LT(doubleBufferIndex, 2u);
  if (doubleBufferIndex > 1)
    return;

  AudioBuffer* inputBuffer = m_inputBuffers[doubleBufferIndex].get();
  AudioBuffer* outputBuffer = m_outputBuffers[doubleBufferIndex].get();
  DCHECK(outputBuffer);
  if (!outputBuffer)
    return;

  // Avoid firing the event if the document has already gone away.
  if (node() && context() && context()->getExecutionContext()) {
    // This synchronizes with process().
    MutexLocker processLocker(m_processEventLock);

    // Calculate a playbackTime with the buffersize which needs to be processed
    // each time onaudioprocess is called.  The outputBuffer being passed to JS
    // will be played after exhuasting previous outputBuffer by
    // double-buffering.
    double playbackTime = (context()->currentSampleFrame() + m_bufferSize) /
                          static_cast<double>(context()->sampleRate());

    // Call the JavaScript event handler which will do the audio processing.
    node()->dispatchEvent(
        AudioProcessingEvent::create(inputBuffer, outputBuffer, playbackTime));
  }
}

void ScriptProcessorHandler::fireProcessEventForOfflineAudioContext(
    unsigned doubleBufferIndex,
    WaitableEvent* waitableEvent) {
  DCHECK(isMainThread());

  DCHECK_LT(doubleBufferIndex, 2u);
  if (doubleBufferIndex > 1) {
    waitableEvent->signal();
    return;
  }

  AudioBuffer* inputBuffer = m_inputBuffers[doubleBufferIndex].get();
  AudioBuffer* outputBuffer = m_outputBuffers[doubleBufferIndex].get();
  DCHECK(outputBuffer);
  if (!outputBuffer) {
    waitableEvent->signal();
    return;
  }

  if (node() && context() && context()->getExecutionContext()) {
    // We do not need a process lock here because the offline render thread
    // is locked by the waitable event.
    double playbackTime = (context()->currentSampleFrame() + m_bufferSize) /
                          static_cast<double>(context()->sampleRate());
    node()->dispatchEvent(
        AudioProcessingEvent::create(inputBuffer, outputBuffer, playbackTime));
  }

  waitableEvent->signal();
}

double ScriptProcessorHandler::tailTime() const {
  return std::numeric_limits<double>::infinity();
}

double ScriptProcessorHandler::latencyTime() const {
  return std::numeric_limits<double>::infinity();
}

void ScriptProcessorHandler::setChannelCount(unsigned long channelCount,
                                             ExceptionState& exceptionState) {
  DCHECK(isMainThread());
  BaseAudioContext::AutoLocker locker(context());

  if (channelCount != m_channelCount) {
    exceptionState.throwDOMException(
        NotSupportedError, "channelCount cannot be changed from " +
                               String::number(m_channelCount) + " to " +
                               String::number(channelCount));
  }
}

void ScriptProcessorHandler::setChannelCountMode(
    const String& mode,
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());
  BaseAudioContext::AutoLocker locker(context());

  if ((mode == "max") || (mode == "clamped-max")) {
    exceptionState.throwDOMException(
        NotSupportedError,
        "channelCountMode cannot be changed from 'explicit' to '" + mode + "'");
  }
}

// ----------------------------------------------------------------

ScriptProcessorNode::ScriptProcessorNode(BaseAudioContext& context,
                                         float sampleRate,
                                         size_t bufferSize,
                                         unsigned numberOfInputChannels,
                                         unsigned numberOfOutputChannels)
    : AudioNode(context) {
  setHandler(ScriptProcessorHandler::create(*this, sampleRate, bufferSize,
                                            numberOfInputChannels,
                                            numberOfOutputChannels));
}

static size_t chooseBufferSize(size_t callbackBufferSize) {
  // Choose a buffer size based on the audio hardware buffer size. Arbitarily
  // make it a power of two that is 4 times greater than the hardware buffer
  // size.
  // FIXME: What is the best way to choose this?
  size_t bufferSize =
      1 << static_cast<unsigned>(log2(4 * callbackBufferSize) + 0.5);

  if (bufferSize < 256)
    return 256;
  if (bufferSize > 16384)
    return 16384;

  return bufferSize;
}

ScriptProcessorNode* ScriptProcessorNode::create(
    BaseAudioContext& context,
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  // Default buffer size is 0 (let WebAudio choose) with 2 inputs and 2
  // outputs.
  return create(context, 0, 2, 2, exceptionState);
}

ScriptProcessorNode* ScriptProcessorNode::create(
    BaseAudioContext& context,
    size_t bufferSize,
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  // Default is 2 inputs and 2 outputs.
  return create(context, bufferSize, 2, 2, exceptionState);
}

ScriptProcessorNode* ScriptProcessorNode::create(
    BaseAudioContext& context,
    size_t bufferSize,
    unsigned numberOfInputChannels,
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  // Default is 2 outputs.
  return create(context, bufferSize, numberOfInputChannels, 2, exceptionState);
}

ScriptProcessorNode* ScriptProcessorNode::create(
    BaseAudioContext& context,
    size_t bufferSize,
    unsigned numberOfInputChannels,
    unsigned numberOfOutputChannels,
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  if (context.isContextClosed()) {
    context.throwExceptionForClosedState(exceptionState);
    return nullptr;
  }

  if (numberOfInputChannels == 0 && numberOfOutputChannels == 0) {
    exceptionState.throwDOMException(
        IndexSizeError,
        "number of input channels and output channels cannot both be zero.");
    return nullptr;
  }

  if (numberOfInputChannels > BaseAudioContext::maxNumberOfChannels()) {
    exceptionState.throwDOMException(
        IndexSizeError,
        "number of input channels (" + String::number(numberOfInputChannels) +
            ") exceeds maximum (" +
            String::number(BaseAudioContext::maxNumberOfChannels()) + ").");
    return nullptr;
  }

  if (numberOfOutputChannels > BaseAudioContext::maxNumberOfChannels()) {
    exceptionState.throwDOMException(
        IndexSizeError,
        "number of output channels (" + String::number(numberOfOutputChannels) +
            ") exceeds maximum (" +
            String::number(BaseAudioContext::maxNumberOfChannels()) + ").");
    return nullptr;
  }

  // Check for valid buffer size.
  switch (bufferSize) {
    case 0:
      // Choose an appropriate size.  For an AudioContext, we need to
      // choose an appropriate size based on the callback buffer size.
      // For OfflineAudioContext, there's no callback buffer size, so
      // just use the minimum valid buffer size.
      bufferSize =
          context.hasRealtimeConstraint()
              ? chooseBufferSize(context.destination()->callbackBufferSize())
              : 256;
      break;
    case 256:
    case 512:
    case 1024:
    case 2048:
    case 4096:
    case 8192:
    case 16384:
      break;
    default:
      exceptionState.throwDOMException(
          IndexSizeError,
          "buffer size (" + String::number(bufferSize) +
              ") must be 0 or a power of two between 256 and 16384.");
      return nullptr;
  }

  ScriptProcessorNode* node =
      new ScriptProcessorNode(context, context.sampleRate(), bufferSize,
                              numberOfInputChannels, numberOfOutputChannels);

  if (!node)
    return nullptr;

  // context keeps reference until we stop making javascript rendering callbacks
  context.notifySourceNodeStartedProcessing(node);

  return node;
}

size_t ScriptProcessorNode::bufferSize() const {
  return static_cast<ScriptProcessorHandler&>(handler()).bufferSize();
}

bool ScriptProcessorNode::hasPendingActivity() const {
  // To prevent the node from leaking after the context is closed.
  if (context()->isContextClosed())
    return false;

  // If |onaudioprocess| event handler is defined, the node should not be
  // GCed even if it is out of scope.
  if (hasEventListeners(EventTypeNames::audioprocess))
    return true;

  return false;
}

}  // namespace blink
