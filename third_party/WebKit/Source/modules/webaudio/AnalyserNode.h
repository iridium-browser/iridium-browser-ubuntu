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

#ifndef AnalyserNode_h
#define AnalyserNode_h

#include "core/dom/DOMTypedArray.h"
#include "modules/webaudio/AudioBasicInspectorNode.h"
#include "modules/webaudio/RealtimeAnalyser.h"

namespace blink {

class BaseAudioContext;
class AnalyserOptions;
class ExceptionState;

class AnalyserHandler final : public AudioBasicInspectorHandler {
 public:
  static PassRefPtr<AnalyserHandler> create(AudioNode&, float sampleRate);
  ~AnalyserHandler() override;

  // AudioHandler
  void process(size_t framesToProcess) override;

  unsigned fftSize() const { return m_analyser.fftSize(); }
  void setFftSize(unsigned size, ExceptionState&);

  unsigned frequencyBinCount() const { return m_analyser.frequencyBinCount(); }

  void setMinDecibels(double k, ExceptionState&);
  double minDecibels() const { return m_analyser.minDecibels(); }

  void setMaxDecibels(double k, ExceptionState&);
  double maxDecibels() const { return m_analyser.maxDecibels(); }

  void setMinMaxDecibels(double min, double max, ExceptionState&);

  void setSmoothingTimeConstant(double k, ExceptionState&);
  double smoothingTimeConstant() const {
    return m_analyser.smoothingTimeConstant();
  }

  void getFloatFrequencyData(DOMFloat32Array* array, double currentTime) {
    m_analyser.getFloatFrequencyData(array, currentTime);
  }
  void getByteFrequencyData(DOMUint8Array* array, double currentTime) {
    m_analyser.getByteFrequencyData(array, currentTime);
  }
  void getFloatTimeDomainData(DOMFloat32Array* array) {
    m_analyser.getFloatTimeDomainData(array);
  }
  void getByteTimeDomainData(DOMUint8Array* array) {
    m_analyser.getByteTimeDomainData(array);
  }

  // AnalyserNode needs special handling when updating the pull status
  // because the node must get pulled even if there are no inputs or
  // outputs so that the internal state is properly updated with the
  // correct time data.
  void updatePullStatus() override;

 private:
  AnalyserHandler(AudioNode&, float sampleRate);
  bool propagatesSilence() const {
    // An AnalyserNode does actually propogate silence, but to get the
    // time and FFT data updated correctly, process() needs to be
    // called even if all the inputs are silent.
    return false;
  }

  RealtimeAnalyser m_analyser;
};

class AnalyserNode final : public AudioBasicInspectorNode {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AnalyserNode* create(BaseAudioContext&, ExceptionState&);
  static AnalyserNode* create(BaseAudioContext*,
                              const AnalyserOptions&,
                              ExceptionState&);

  unsigned fftSize() const;
  void setFftSize(unsigned size, ExceptionState&);
  unsigned frequencyBinCount() const;
  void setMinDecibels(double, ExceptionState&);
  double minDecibels() const;
  void setMaxDecibels(double, ExceptionState&);
  double maxDecibels() const;
  void setSmoothingTimeConstant(double, ExceptionState&);
  double smoothingTimeConstant() const;
  void getFloatFrequencyData(DOMFloat32Array*);
  void getByteFrequencyData(DOMUint8Array*);
  void getFloatTimeDomainData(DOMFloat32Array*);
  void getByteTimeDomainData(DOMUint8Array*);

 private:
  AnalyserNode(BaseAudioContext&);
  AnalyserHandler& analyserHandler() const;

  void setMinMaxDecibels(double min, double max, ExceptionState&);
};

}  // namespace blink

#endif  // AnalyserNode_h
