// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/audio_encoder_verbatim.h"

#include "base/logging.h"
#include "remoting/proto/audio.pb.h"

namespace remoting {

AudioEncoderVerbatim::AudioEncoderVerbatim() {}

AudioEncoderVerbatim::~AudioEncoderVerbatim() {}

std::unique_ptr<AudioPacket> AudioEncoderVerbatim::Encode(
    std::unique_ptr<AudioPacket> packet) {
  DCHECK_EQ(AudioPacket::ENCODING_RAW, packet->encoding());
  DCHECK_EQ(1, packet->data_size());
  DCHECK_NE(AudioPacket::SAMPLING_RATE_INVALID, packet->sampling_rate());
  DCHECK_NE(AudioPacket::BYTES_PER_SAMPLE_INVALID, packet->bytes_per_sample());
  DCHECK_NE(AudioPacket::CHANNELS_INVALID, packet->channels());
  return packet;
}

int AudioEncoderVerbatim::GetBitrate() {
  return AudioPacket::SAMPLING_RATE_48000 * AudioPacket::BYTES_PER_SAMPLE_2 *
         AudioPacket::CHANNELS_STEREO * 8;
}

}  // namespace remoting
