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

#include "modules/webaudio/AnalyserNode.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webaudio/AnalyserOptions.h"
#include "modules/webaudio/AudioNodeInput.h"
#include "modules/webaudio/AudioNodeOutput.h"

namespace blink {

AnalyserHandler::AnalyserHandler(AudioNode& node, float sampleRate)
    : AudioBasicInspectorHandler(NodeTypeAnalyser, node, sampleRate, 2) {
  initialize();
}

PassRefPtr<AnalyserHandler> AnalyserHandler::create(AudioNode& node,
                                                    float sampleRate) {
  return adoptRef(new AnalyserHandler(node, sampleRate));
}

AnalyserHandler::~AnalyserHandler() {
  uninitialize();
}

void AnalyserHandler::process(size_t framesToProcess) {
  AudioBus* outputBus = output(0).bus();

  if (!isInitialized()) {
    outputBus->zero();
    return;
  }

  AudioBus* inputBus = input(0).bus();

  // Give the analyser the audio which is passing through this
  // AudioNode.  This must always be done so that the state of the
  // Analyser reflects the current input.
  m_analyser.writeInput(inputBus, framesToProcess);

  if (!input(0).isConnected()) {
    // No inputs, so clear the output, and propagate the silence hint.
    outputBus->zero();
    return;
  }

  // For in-place processing, our override of pullInputs() will just pass the
  // audio data through unchanged if the channel count matches from input to
  // output (resulting in inputBus == outputBus). Otherwise, do an up-mix to
  // stereo.
  if (inputBus != outputBus)
    outputBus->copyFrom(*inputBus);
}

void AnalyserHandler::setFftSize(unsigned size,
                                 ExceptionState& exceptionState) {
  if (!m_analyser.setFftSize(size)) {
    exceptionState.throwDOMException(
        IndexSizeError,
        (size < RealtimeAnalyser::MinFFTSize ||
         size > RealtimeAnalyser::MaxFFTSize)
            ? ExceptionMessages::indexOutsideRange(
                  "FFT size", size, RealtimeAnalyser::MinFFTSize,
                  ExceptionMessages::InclusiveBound,
                  RealtimeAnalyser::MaxFFTSize,
                  ExceptionMessages::InclusiveBound)
            : ("The value provided (" + String::number(size) +
               ") is not a power of two."));
  }
}

void AnalyserHandler::setMinDecibels(double k, ExceptionState& exceptionState) {
  if (k < maxDecibels()) {
    m_analyser.setMinDecibels(k);
  } else {
    exceptionState.throwDOMException(
        IndexSizeError, ExceptionMessages::indexExceedsMaximumBound(
                            "minDecibels", k, maxDecibels()));
  }
}

void AnalyserHandler::setMaxDecibels(double k, ExceptionState& exceptionState) {
  if (k > minDecibels()) {
    m_analyser.setMaxDecibels(k);
  } else {
    exceptionState.throwDOMException(
        IndexSizeError, ExceptionMessages::indexExceedsMinimumBound(
                            "maxDecibels", k, minDecibels()));
  }
}

void AnalyserHandler::setMinMaxDecibels(double minDecibels,
                                        double maxDecibels,
                                        ExceptionState& exceptionState) {
  if (minDecibels >= maxDecibels) {
    exceptionState.throwDOMException(
        IndexSizeError, "maxDecibels (" + String::number(maxDecibels) +
                            ") must be greater than or equal to minDecibels " +
                            "( " + String::number(minDecibels) + ").");
    return;
  }
  m_analyser.setMinDecibels(minDecibels);
  m_analyser.setMaxDecibels(maxDecibels);
}

void AnalyserHandler::setSmoothingTimeConstant(double k,
                                               ExceptionState& exceptionState) {
  if (k >= 0 && k <= 1) {
    m_analyser.setSmoothingTimeConstant(k);
  } else {
    exceptionState.throwDOMException(
        IndexSizeError,
        ExceptionMessages::indexOutsideRange(
            "smoothing value", k, 0.0, ExceptionMessages::InclusiveBound, 1.0,
            ExceptionMessages::InclusiveBound));
  }
}

void AnalyserHandler::updatePullStatus() {
#if DCHECK_IS_ON()
  DCHECK(context()->isGraphOwner());
#endif

  if (output(0).isConnected()) {
    // When an AudioBasicInspectorNode is connected to a downstream node, it
    // will get pulled by the downstream node, thus remove it from the context's
    // automatic pull list.
    if (m_needAutomaticPull) {
      context()->deferredTaskHandler().removeAutomaticPullNode(this);
      m_needAutomaticPull = false;
    }
  } else {
    unsigned numberOfInputConnections = input(0).numberOfRenderingConnections();
    // When an AnalyserNode is not connected to any downstream node
    // while still connected from upstream node(s), add it to the context's
    // automatic pull list.
    //
    // But don't remove the AnalyserNode if there are no inputs
    // connected to the node.  The node needs to be pulled so that the
    // internal state is updated with the correct input signal (of
    // zeroes).
    if (numberOfInputConnections && !m_needAutomaticPull) {
      context()->deferredTaskHandler().addAutomaticPullNode(this);
      m_needAutomaticPull = true;
    }
  }
}
// ----------------------------------------------------------------

AnalyserNode::AnalyserNode(BaseAudioContext& context)
    : AudioBasicInspectorNode(context) {
  setHandler(AnalyserHandler::create(*this, context.sampleRate()));
}

AnalyserNode* AnalyserNode::create(BaseAudioContext& context,
                                   ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  if (context.isContextClosed()) {
    context.throwExceptionForClosedState(exceptionState);
    return nullptr;
  }

  return new AnalyserNode(context);
}

AnalyserNode* AnalyserNode::create(BaseAudioContext* context,
                                   const AnalyserOptions& options,
                                   ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  AnalyserNode* node = create(*context, exceptionState);

  if (!node)
    return nullptr;

  node->handleChannelOptions(options, exceptionState);

  if (options.hasFftSize())
    node->setFftSize(options.fftSize(), exceptionState);

  if (options.hasSmoothingTimeConstant())
    node->setSmoothingTimeConstant(options.smoothingTimeConstant(),
                                   exceptionState);

  // minDecibels and maxDecibels have default values.  Set both of the values
  // at once.
  node->setMinMaxDecibels(options.minDecibels(), options.maxDecibels(),
                          exceptionState);

  return node;
}

AnalyserHandler& AnalyserNode::analyserHandler() const {
  return static_cast<AnalyserHandler&>(handler());
}

unsigned AnalyserNode::fftSize() const {
  return analyserHandler().fftSize();
}

void AnalyserNode::setFftSize(unsigned size, ExceptionState& exceptionState) {
  return analyserHandler().setFftSize(size, exceptionState);
}

unsigned AnalyserNode::frequencyBinCount() const {
  return analyserHandler().frequencyBinCount();
}

void AnalyserNode::setMinDecibels(double min, ExceptionState& exceptionState) {
  analyserHandler().setMinDecibels(min, exceptionState);
}

double AnalyserNode::minDecibels() const {
  return analyserHandler().minDecibels();
}

void AnalyserNode::setMaxDecibels(double max, ExceptionState& exceptionState) {
  analyserHandler().setMaxDecibels(max, exceptionState);
}

void AnalyserNode::setMinMaxDecibels(double min,
                                     double max,
                                     ExceptionState& exceptionState) {
  analyserHandler().setMinMaxDecibels(min, max, exceptionState);
}

double AnalyserNode::maxDecibels() const {
  return analyserHandler().maxDecibels();
}

void AnalyserNode::setSmoothingTimeConstant(double smoothingTime,
                                            ExceptionState& exceptionState) {
  analyserHandler().setSmoothingTimeConstant(smoothingTime, exceptionState);
}

double AnalyserNode::smoothingTimeConstant() const {
  return analyserHandler().smoothingTimeConstant();
}

void AnalyserNode::getFloatFrequencyData(DOMFloat32Array* array) {
  analyserHandler().getFloatFrequencyData(array, context()->currentTime());
}

void AnalyserNode::getByteFrequencyData(DOMUint8Array* array) {
  analyserHandler().getByteFrequencyData(array, context()->currentTime());
}

void AnalyserNode::getFloatTimeDomainData(DOMFloat32Array* array) {
  analyserHandler().getFloatTimeDomainData(array);
}

void AnalyserNode::getByteTimeDomainData(DOMUint8Array* array) {
  analyserHandler().getByteTimeDomainData(array);
}

}  // namespace blink
