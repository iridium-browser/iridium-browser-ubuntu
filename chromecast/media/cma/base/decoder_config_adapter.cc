// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/decoder_config_adapter.h"

#include "base/logging.h"
#include "media/base/channel_layout.h"

namespace chromecast {
namespace media {

namespace {

// Converts ::media::AudioCodec to chromecast::media::AudioCodec. Any unknown or
// unsupported codec will be converted to chromecast::media::kCodecUnknown.
AudioCodec ToAudioCodec(const ::media::AudioCodec audio_codec) {
  switch (audio_codec) {
    case ::media::kCodecAAC:
      return kCodecAAC;
    case ::media::kCodecMP3:
      return kCodecMP3;
    case ::media::kCodecPCM:
      return kCodecPCM;
    case ::media::kCodecPCM_S16BE:
      return kCodecPCM_S16BE;
    case ::media::kCodecVorbis:
      return kCodecVorbis;
    default:
      LOG(ERROR) << "Unsupported audio codec " << audio_codec;
  }
  return kAudioCodecUnknown;
}

// Converts ::media::VideoCodec to chromecast::media::VideoCodec. Any unknown or
// unsupported codec will be converted to chromecast::media::kCodecUnknown.
VideoCodec ToVideoCodec(const ::media::VideoCodec video_codec) {
  switch (video_codec) {
    case ::media::kCodecH264:
      return kCodecH264;
    case ::media::kCodecVP8:
      return kCodecVP8;
    case ::media::kCodecVP9:
      return kCodecVP9;
    default:
      LOG(ERROR) << "Unsupported video codec " << video_codec;
  }
  return kVideoCodecUnknown;
}

// Converts ::media::VideoCodecProfile to chromecast::media::VideoProfile.
VideoProfile ToVideoProfile(const ::media::VideoCodecProfile codec_profile) {
  switch(codec_profile) {
    case ::media::H264PROFILE_BASELINE:
      return kH264Baseline;
    case ::media::H264PROFILE_MAIN:
      return kH264Main;
    case ::media::H264PROFILE_EXTENDED:
      return kH264Extended;
    case ::media::H264PROFILE_HIGH:
      return kH264High;
    case ::media::H264PROFILE_HIGH10PROFILE:
      return kH264High10;
    case ::media::H264PROFILE_HIGH422PROFILE:
      return kH264High422;
    case ::media::H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return kH264High444Predictive;
    case ::media::H264PROFILE_SCALABLEBASELINE:
      return kH264ScalableBaseline;
    case ::media::H264PROFILE_SCALABLEHIGH:
      return kH264ScalableHigh;
    case ::media::H264PROFILE_STEREOHIGH:
      return kH264Stereohigh;
    case ::media::H264PROFILE_MULTIVIEWHIGH:
      return kH264MultiviewHigh;
    case ::media::VP8PROFILE_ANY:
      return kVP8ProfileAny;
    case ::media::VP9PROFILE_ANY:
      return kVP9ProfileAny;
    default:
      LOG(INFO) << "Unsupported video codec profile " << codec_profile;
  }
  return kVideoProfileUnknown;
}

}  // namespace

// static
AudioConfig DecoderConfigAdapter::ToCastAudioConfig(
    const ::media::AudioDecoderConfig& config) {
  AudioConfig audio_config;
  if (!config.IsValidConfig()) {
    return audio_config;
  }

  audio_config.codec = ToAudioCodec(config.codec());
  audio_config.bytes_per_channel = config.bytes_per_channel();
  audio_config.channel_number =
      ::media::ChannelLayoutToChannelCount(config.channel_layout()),
       audio_config.samples_per_second = config.samples_per_second();
  audio_config.extra_data = (config.extra_data_size() > 0) ?
      config.extra_data() : nullptr;
  audio_config.extra_data_size = config.extra_data_size();
  audio_config.is_encrypted = config.is_encrypted();
  return audio_config;
}

// static
VideoConfig DecoderConfigAdapter::ToCastVideoConfig(
    const ::media::VideoDecoderConfig& config) {
  VideoConfig video_config;
  if (!config.IsValidConfig()) {
    return video_config;
  }

  video_config.codec = ToVideoCodec(config.codec());
  video_config.profile = ToVideoProfile(config.profile());
  video_config.extra_data = (config.extra_data_size() > 0) ?
      config.extra_data() : nullptr;
  video_config.extra_data_size = config.extra_data_size();
  video_config.is_encrypted = config.is_encrypted();
  return video_config;
}

}  // namespace media
}  // namespace chromecast
