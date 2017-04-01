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

#include "modules/webaudio/DelayDSPKernel.h"
#include "platform/audio/AudioUtilities.h"
#include "wtf/MathExtras.h"
#include <algorithm>

namespace blink {

const float SmoothingTimeConstant = 0.020f;  // 20ms

DelayDSPKernel::DelayDSPKernel(DelayProcessor* processor)
    : AudioDelayDSPKernel(processor, AudioUtilities::kRenderQuantumFrames) {
  DCHECK(processor);
  DCHECK_GT(processor->sampleRate(), 0);
  if (!(processor && processor->sampleRate() > 0))
    return;

  m_maxDelayTime = processor->maxDelayTime();
  DCHECK_GE(m_maxDelayTime, 0);
  DCHECK(!std::isnan(m_maxDelayTime));
  if (m_maxDelayTime < 0 || std::isnan(m_maxDelayTime))
    return;

  m_buffer.allocate(
      bufferLengthForDelay(m_maxDelayTime, processor->sampleRate()));
  m_buffer.zero();

  m_smoothingRate = AudioUtilities::discreteTimeConstantForSampleRate(
      SmoothingTimeConstant, processor->sampleRate());
}

bool DelayDSPKernel::hasSampleAccurateValues() {
  return getDelayProcessor()->delayTime().hasSampleAccurateValues();
}

void DelayDSPKernel::calculateSampleAccurateValues(float* delayTimes,
                                                   size_t framesToProcess) {
  getDelayProcessor()->delayTime().calculateSampleAccurateValues(
      delayTimes, framesToProcess);
}

double DelayDSPKernel::delayTime(float) {
  return getDelayProcessor()->delayTime().finalValue();
}

void DelayDSPKernel::processOnlyAudioParams(size_t framesToProcess) {
  DCHECK_LE(framesToProcess, AudioUtilities::kRenderQuantumFrames);

  float values[AudioUtilities::kRenderQuantumFrames];

  getDelayProcessor()->delayTime().calculateSampleAccurateValues(
      values, framesToProcess);
}

}  // namespace blink
