/*
 * libjingle
 * Copyright 2013 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/app/webrtc/localaudiosource.h"

#include <vector>

#include "talk/app/webrtc/mediaconstraintsinterface.h"
#include "talk/media/base/mediaengine.h"

using webrtc::MediaConstraintsInterface;
using webrtc::MediaSourceInterface;

namespace webrtc {

namespace {

// Convert constraints to audio options. Return false if constraints are
// invalid.
void FromConstraints(const MediaConstraintsInterface::Constraints& constraints,
                     cricket::AudioOptions* options) {
  MediaConstraintsInterface::Constraints::const_iterator iter;

  // This design relies on the fact that all the audio constraints are actually
  // "options", i.e. boolean-valued and always satisfiable.  If the constraints
  // are extended to include non-boolean values or actual format constraints,
  // a different algorithm will be required.
  for (iter = constraints.begin(); iter != constraints.end(); ++iter) {
    bool value = false;

    if (!rtc::FromString(iter->value, &value))
      continue;

    if (iter->key == MediaConstraintsInterface::kEchoCancellation)
      options->echo_cancellation.Set(value);
    else if (iter->key ==
        MediaConstraintsInterface::kExperimentalEchoCancellation)
      options->experimental_aec.Set(value);
    else if (iter->key == MediaConstraintsInterface::kDAEchoCancellation)
      options->delay_agnostic_aec.Set(value);
    else if (iter->key == MediaConstraintsInterface::kAutoGainControl)
      options->auto_gain_control.Set(value);
    else if (iter->key ==
        MediaConstraintsInterface::kExperimentalAutoGainControl)
      options->experimental_agc.Set(value);
    else if (iter->key == MediaConstraintsInterface::kNoiseSuppression)
      options->noise_suppression.Set(value);
    else if (iter->key ==
          MediaConstraintsInterface::kExperimentalNoiseSuppression)
      options->experimental_ns.Set(value);
    else if (iter->key == MediaConstraintsInterface::kHighpassFilter)
      options->highpass_filter.Set(value);
    else if (iter->key == MediaConstraintsInterface::kTypingNoiseDetection)
      options->typing_detection.Set(value);
    else if (iter->key == MediaConstraintsInterface::kAudioMirroring)
      options->stereo_swapping.Set(value);
    else if (iter->key == MediaConstraintsInterface::kAecDump)
      options->aec_dump.Set(value);
  }
}

}  // namespace

rtc::scoped_refptr<LocalAudioSource> LocalAudioSource::Create(
    const PeerConnectionFactoryInterface::Options& options,
    const MediaConstraintsInterface* constraints) {
  rtc::scoped_refptr<LocalAudioSource> source(
      new rtc::RefCountedObject<LocalAudioSource>());
  source->Initialize(options, constraints);
  return source;
}

void LocalAudioSource::Initialize(
    const PeerConnectionFactoryInterface::Options& options,
    const MediaConstraintsInterface* constraints) {
  if (!constraints)
    return;

  // Apply optional constraints first, they will be overwritten by mandatory
  // constraints.
  FromConstraints(constraints->GetOptional(), &options_);

  cricket::AudioOptions mandatory_options;
  FromConstraints(constraints->GetMandatory(), &mandatory_options);
  options_.SetAll(mandatory_options);
  source_state_ = kLive;
}

}  // namespace webrtc
