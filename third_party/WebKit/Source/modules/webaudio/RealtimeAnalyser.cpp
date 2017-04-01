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

#include "modules/webaudio/RealtimeAnalyser.h"
#include "platform/audio/AudioBus.h"
#include "platform/audio/AudioUtilities.h"
#include "platform/audio/VectorMath.h"
#include "wtf/MathExtras.h"
#include "wtf/PtrUtil.h"
#include <algorithm>
#include <complex>
#include <limits.h>

namespace blink {

const double RealtimeAnalyser::DefaultSmoothingTimeConstant = 0.8;
const double RealtimeAnalyser::DefaultMinDecibels = -100;
const double RealtimeAnalyser::DefaultMaxDecibels = -30;

const unsigned RealtimeAnalyser::DefaultFFTSize = 2048;
// All FFT implementations are expected to handle power-of-two sizes
// MinFFTSize <= size <= MaxFFTSize.
const unsigned RealtimeAnalyser::MinFFTSize = 32;
const unsigned RealtimeAnalyser::MaxFFTSize = 32768;
const unsigned RealtimeAnalyser::InputBufferSize =
    RealtimeAnalyser::MaxFFTSize * 2;

RealtimeAnalyser::RealtimeAnalyser()
    : m_inputBuffer(InputBufferSize),
      m_writeIndex(0),
      m_downMixBus(AudioBus::create(1, AudioUtilities::kRenderQuantumFrames)),
      m_fftSize(DefaultFFTSize),
      m_magnitudeBuffer(DefaultFFTSize / 2),
      m_smoothingTimeConstant(DefaultSmoothingTimeConstant),
      m_minDecibels(DefaultMinDecibels),
      m_maxDecibels(DefaultMaxDecibels),
      m_lastAnalysisTime(-1) {
  m_analysisFrame = WTF::makeUnique<FFTFrame>(DefaultFFTSize);
}

bool RealtimeAnalyser::setFftSize(size_t size) {
  DCHECK(isMainThread());

  // Only allow powers of two within the allowed range.
  if (size > MaxFFTSize || size < MinFFTSize ||
      !AudioUtilities::isPowerOfTwo(size))
    return false;

  if (m_fftSize != size) {
    m_analysisFrame = WTF::makeUnique<FFTFrame>(size);
    // m_magnitudeBuffer has size = fftSize / 2 because it contains floats
    // reduced from complex values in m_analysisFrame.
    m_magnitudeBuffer.allocate(size / 2);
    m_fftSize = size;
  }

  return true;
}

void RealtimeAnalyser::writeInput(AudioBus* bus, size_t framesToProcess) {
  bool isBusGood = bus && bus->numberOfChannels() > 0 &&
                   bus->channel(0)->length() >= framesToProcess;
  DCHECK(isBusGood);
  if (!isBusGood)
    return;

  // FIXME : allow to work with non-FFTSize divisible chunking
  bool isDestinationGood =
      m_writeIndex < m_inputBuffer.size() &&
      m_writeIndex + framesToProcess <= m_inputBuffer.size();
  DCHECK(isDestinationGood);
  if (!isDestinationGood)
    return;

  // Perform real-time analysis
  float* dest = m_inputBuffer.data() + m_writeIndex;

  // Clear the bus and downmix the input according to the down mixing rules.
  // Then save the result in the m_inputBuffer at the appropriate place.
  m_downMixBus->zero();
  m_downMixBus->sumFrom(*bus);
  memcpy(dest, m_downMixBus->channel(0)->data(),
         framesToProcess * sizeof(*dest));

  m_writeIndex += framesToProcess;
  if (m_writeIndex >= InputBufferSize)
    m_writeIndex = 0;
}

namespace {

void applyWindow(float* p, size_t n) {
  DCHECK(isMainThread());

  // Blackman window
  double alpha = 0.16;
  double a0 = 0.5 * (1 - alpha);
  double a1 = 0.5;
  double a2 = 0.5 * alpha;

  for (unsigned i = 0; i < n; ++i) {
    double x = static_cast<double>(i) / static_cast<double>(n);
    double window =
        a0 - a1 * cos(twoPiDouble * x) + a2 * cos(twoPiDouble * 2.0 * x);
    p[i] *= float(window);
  }
}

}  // namespace

void RealtimeAnalyser::doFFTAnalysis() {
  DCHECK(isMainThread());

  // Unroll the input buffer into a temporary buffer, where we'll apply an
  // analysis window followed by an FFT.
  size_t fftSize = this->fftSize();

  AudioFloatArray temporaryBuffer(fftSize);
  float* inputBuffer = m_inputBuffer.data();
  float* tempP = temporaryBuffer.data();

  // Take the previous fftSize values from the input buffer and copy into the
  // temporary buffer.
  unsigned writeIndex = m_writeIndex;
  if (writeIndex < fftSize) {
    memcpy(tempP, inputBuffer + writeIndex - fftSize + InputBufferSize,
           sizeof(*tempP) * (fftSize - writeIndex));
    memcpy(tempP + fftSize - writeIndex, inputBuffer,
           sizeof(*tempP) * writeIndex);
  } else {
    memcpy(tempP, inputBuffer + writeIndex - fftSize, sizeof(*tempP) * fftSize);
  }

  // Window the input samples.
  applyWindow(tempP, fftSize);

  // Do the analysis.
  m_analysisFrame->doFFT(tempP);

  float* realP = m_analysisFrame->realData();
  float* imagP = m_analysisFrame->imagData();

  // Blow away the packed nyquist component.
  imagP[0] = 0;

  // Normalize so than an input sine wave at 0dBfs registers as 0dBfs (undo FFT
  // scaling factor).
  const double magnitudeScale = 1.0 / fftSize;

  // A value of 0 does no averaging with the previous result.  Larger values
  // produce slower, but smoother changes.
  double k = m_smoothingTimeConstant;
  k = std::max(0.0, k);
  k = std::min(1.0, k);

  // Convert the analysis data from complex to magnitude and average with the
  // previous result.
  float* destination = magnitudeBuffer().data();
  size_t n = magnitudeBuffer().size();
  for (size_t i = 0; i < n; ++i) {
    std::complex<double> c(realP[i], imagP[i]);
    double scalarMagnitude = abs(c) * magnitudeScale;
    destination[i] = float(k * destination[i] + (1 - k) * scalarMagnitude);
  }
}

void RealtimeAnalyser::convertFloatToDb(DOMFloat32Array* destinationArray) {
  // Convert from linear magnitude to floating-point decibels.
  unsigned sourceLength = magnitudeBuffer().size();
  size_t len = std::min(sourceLength, destinationArray->length());
  if (len > 0) {
    const float* source = magnitudeBuffer().data();
    float* destination = destinationArray->data();

    for (unsigned i = 0; i < len; ++i) {
      float linearValue = source[i];
      double dbMag = AudioUtilities::linearToDecibels(linearValue);
      destination[i] = float(dbMag);
    }
  }
}

void RealtimeAnalyser::getFloatFrequencyData(DOMFloat32Array* destinationArray,
                                             double currentTime) {
  DCHECK(isMainThread());
  DCHECK(destinationArray);

  if (currentTime <= m_lastAnalysisTime) {
    convertFloatToDb(destinationArray);
    return;
  }

  // Time has advanced since the last call; update the FFT data.
  m_lastAnalysisTime = currentTime;
  doFFTAnalysis();

  convertFloatToDb(destinationArray);
}

void RealtimeAnalyser::convertToByteData(DOMUint8Array* destinationArray) {
  // Convert from linear magnitude to unsigned-byte decibels.
  unsigned sourceLength = magnitudeBuffer().size();
  size_t len = std::min(sourceLength, destinationArray->length());
  if (len > 0) {
    const double rangeScaleFactor = m_maxDecibels == m_minDecibels
                                        ? 1
                                        : 1 / (m_maxDecibels - m_minDecibels);
    const double minDecibels = m_minDecibels;

    const float* source = magnitudeBuffer().data();
    unsigned char* destination = destinationArray->data();

    for (unsigned i = 0; i < len; ++i) {
      float linearValue = source[i];
      double dbMag = AudioUtilities::linearToDecibels(linearValue);

      // The range m_minDecibels to m_maxDecibels will be scaled to byte values
      // from 0 to UCHAR_MAX.
      double scaledValue = UCHAR_MAX * (dbMag - minDecibels) * rangeScaleFactor;

      // Clip to valid range.
      if (scaledValue < 0)
        scaledValue = 0;
      if (scaledValue > UCHAR_MAX)
        scaledValue = UCHAR_MAX;

      destination[i] = static_cast<unsigned char>(scaledValue);
    }
  }
}

void RealtimeAnalyser::getByteFrequencyData(DOMUint8Array* destinationArray,
                                            double currentTime) {
  DCHECK(isMainThread());
  DCHECK(destinationArray);

  if (currentTime <= m_lastAnalysisTime) {
    // FIXME: Is it worth caching the data so we don't have to do the conversion
    // every time?  Perhaps not, since we expect many calls in the same
    // rendering quantum.
    convertToByteData(destinationArray);
    return;
  }

  // Time has advanced since the last call; update the FFT data.
  m_lastAnalysisTime = currentTime;
  doFFTAnalysis();

  convertToByteData(destinationArray);
}

void RealtimeAnalyser::getFloatTimeDomainData(
    DOMFloat32Array* destinationArray) {
  DCHECK(isMainThread());
  DCHECK(destinationArray);

  unsigned fftSize = this->fftSize();
  size_t len = std::min(fftSize, destinationArray->length());
  if (len > 0) {
    bool isInputBufferGood = m_inputBuffer.size() == InputBufferSize &&
                             m_inputBuffer.size() > fftSize;
    DCHECK(isInputBufferGood);
    if (!isInputBufferGood)
      return;

    float* inputBuffer = m_inputBuffer.data();
    float* destination = destinationArray->data();

    unsigned writeIndex = m_writeIndex;

    for (unsigned i = 0; i < len; ++i) {
      // Buffer access is protected due to modulo operation.
      float value = inputBuffer[(i + writeIndex - fftSize + InputBufferSize) %
                                InputBufferSize];

      destination[i] = value;
    }
  }
}

void RealtimeAnalyser::getByteTimeDomainData(DOMUint8Array* destinationArray) {
  DCHECK(isMainThread());
  DCHECK(destinationArray);

  unsigned fftSize = this->fftSize();
  size_t len = std::min(fftSize, destinationArray->length());
  if (len > 0) {
    bool isInputBufferGood = m_inputBuffer.size() == InputBufferSize &&
                             m_inputBuffer.size() > fftSize;
    DCHECK(isInputBufferGood);
    if (!isInputBufferGood)
      return;

    float* inputBuffer = m_inputBuffer.data();
    unsigned char* destination = destinationArray->data();

    unsigned writeIndex = m_writeIndex;

    for (unsigned i = 0; i < len; ++i) {
      // Buffer access is protected due to modulo operation.
      float value = inputBuffer[(i + writeIndex - fftSize + InputBufferSize) %
                                InputBufferSize];

      // Scale from nominal -1 -> +1 to unsigned byte.
      double scaledValue = 128 * (value + 1);

      // Clip to valid range.
      if (scaledValue < 0)
        scaledValue = 0;
      if (scaledValue > UCHAR_MAX)
        scaledValue = UCHAR_MAX;

      destination[i] = static_cast<unsigned char>(scaledValue);
    }
  }
}

}  // namespace blink
