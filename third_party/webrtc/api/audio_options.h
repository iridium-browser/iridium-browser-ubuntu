/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_OPTIONS_H_
#define API_AUDIO_OPTIONS_H_

#include <string>

#include "api/optional.h"
#include "rtc_base/stringencode.h"

namespace cricket {

// Options that can be applied to a VoiceMediaChannel or a VoiceMediaEngine.
// Used to be flags, but that makes it hard to selectively apply options.
// We are moving all of the setting of options to structs like this,
// but some things currently still use flags.
struct AudioOptions {
  void SetAll(const AudioOptions& change) {
    SetFrom(&echo_cancellation, change.echo_cancellation);
#if defined(WEBRTC_IOS)
    SetFrom(&ios_force_software_aec_HACK, change.ios_force_software_aec_HACK);
#endif
    SetFrom(&auto_gain_control, change.auto_gain_control);
    SetFrom(&noise_suppression, change.noise_suppression);
    SetFrom(&highpass_filter, change.highpass_filter);
    SetFrom(&stereo_swapping, change.stereo_swapping);
    SetFrom(&audio_jitter_buffer_max_packets,
            change.audio_jitter_buffer_max_packets);
    SetFrom(&audio_jitter_buffer_fast_accelerate,
            change.audio_jitter_buffer_fast_accelerate);
    SetFrom(&typing_detection, change.typing_detection);
    SetFrom(&aecm_generate_comfort_noise, change.aecm_generate_comfort_noise);
    SetFrom(&experimental_agc, change.experimental_agc);
    SetFrom(&extended_filter_aec, change.extended_filter_aec);
    SetFrom(&delay_agnostic_aec, change.delay_agnostic_aec);
    SetFrom(&experimental_ns, change.experimental_ns);
    SetFrom(&intelligibility_enhancer, change.intelligibility_enhancer);
    SetFrom(&level_control, change.level_control);
    SetFrom(&residual_echo_detector, change.residual_echo_detector);
    SetFrom(&tx_agc_target_dbov, change.tx_agc_target_dbov);
    SetFrom(&tx_agc_digital_compression_gain,
            change.tx_agc_digital_compression_gain);
    SetFrom(&tx_agc_limiter, change.tx_agc_limiter);
    SetFrom(&combined_audio_video_bwe, change.combined_audio_video_bwe);
    SetFrom(&audio_network_adaptor, change.audio_network_adaptor);
    SetFrom(&audio_network_adaptor_config, change.audio_network_adaptor_config);
    SetFrom(&level_control_initial_peak_level_dbfs,
            change.level_control_initial_peak_level_dbfs);
  }

  bool operator==(const AudioOptions& o) const {
    return echo_cancellation == o.echo_cancellation &&
#if defined(WEBRTC_IOS)
           ios_force_software_aec_HACK == o.ios_force_software_aec_HACK &&
#endif
           auto_gain_control == o.auto_gain_control &&
           noise_suppression == o.noise_suppression &&
           highpass_filter == o.highpass_filter &&
           stereo_swapping == o.stereo_swapping &&
           audio_jitter_buffer_max_packets ==
               o.audio_jitter_buffer_max_packets &&
           audio_jitter_buffer_fast_accelerate ==
               o.audio_jitter_buffer_fast_accelerate &&
           typing_detection == o.typing_detection &&
           aecm_generate_comfort_noise == o.aecm_generate_comfort_noise &&
           experimental_agc == o.experimental_agc &&
           extended_filter_aec == o.extended_filter_aec &&
           delay_agnostic_aec == o.delay_agnostic_aec &&
           experimental_ns == o.experimental_ns &&
           intelligibility_enhancer == o.intelligibility_enhancer &&
           level_control == o.level_control &&
           residual_echo_detector == o.residual_echo_detector &&
           tx_agc_target_dbov == o.tx_agc_target_dbov &&
           tx_agc_digital_compression_gain ==
               o.tx_agc_digital_compression_gain &&
           tx_agc_limiter == o.tx_agc_limiter &&
           combined_audio_video_bwe == o.combined_audio_video_bwe &&
           audio_network_adaptor == o.audio_network_adaptor &&
           audio_network_adaptor_config == o.audio_network_adaptor_config &&
           level_control_initial_peak_level_dbfs ==
               o.level_control_initial_peak_level_dbfs;
  }
  bool operator!=(const AudioOptions& o) const { return !(*this == o); }

  std::string ToString() const {
    std::ostringstream ost;
    ost << "AudioOptions {";
    ost << ToStringIfSet("aec", echo_cancellation);
#if defined(WEBRTC_IOS)
    ost << ToStringIfSet("ios_force_software_aec_HACK",
                         ios_force_software_aec_HACK);
#endif
    ost << ToStringIfSet("agc", auto_gain_control);
    ost << ToStringIfSet("ns", noise_suppression);
    ost << ToStringIfSet("hf", highpass_filter);
    ost << ToStringIfSet("swap", stereo_swapping);
    ost << ToStringIfSet("audio_jitter_buffer_max_packets",
                         audio_jitter_buffer_max_packets);
    ost << ToStringIfSet("audio_jitter_buffer_fast_accelerate",
                         audio_jitter_buffer_fast_accelerate);
    ost << ToStringIfSet("typing", typing_detection);
    ost << ToStringIfSet("comfort_noise", aecm_generate_comfort_noise);
    ost << ToStringIfSet("experimental_agc", experimental_agc);
    ost << ToStringIfSet("extended_filter_aec", extended_filter_aec);
    ost << ToStringIfSet("delay_agnostic_aec", delay_agnostic_aec);
    ost << ToStringIfSet("experimental_ns", experimental_ns);
    ost << ToStringIfSet("intelligibility_enhancer", intelligibility_enhancer);
    ost << ToStringIfSet("level_control", level_control);
    ost << ToStringIfSet("level_control_initial_peak_level_dbfs",
                         level_control_initial_peak_level_dbfs);
    ost << ToStringIfSet("residual_echo_detector", residual_echo_detector);
    ost << ToStringIfSet("tx_agc_target_dbov", tx_agc_target_dbov);
    ost << ToStringIfSet("tx_agc_digital_compression_gain",
        tx_agc_digital_compression_gain);
    ost << ToStringIfSet("tx_agc_limiter", tx_agc_limiter);
    ost << ToStringIfSet("combined_audio_video_bwe", combined_audio_video_bwe);
    ost << ToStringIfSet("audio_network_adaptor", audio_network_adaptor);
    // The adaptor config is a serialized proto buffer and therefore not human
    // readable. So we comment out the following line.
    // ost << ToStringIfSet("audio_network_adaptor_config",
    //     audio_network_adaptor_config);
    ost << "}";
    return ost.str();
  }

  // Audio processing that attempts to filter away the output signal from
  // later inbound pickup.
  rtc::Optional<bool> echo_cancellation;
#if defined(WEBRTC_IOS)
  // Forces software echo cancellation on iOS. This is a temporary workaround
  // (until Apple fixes the bug) for a device with non-functioning AEC. May
  // improve performance on that particular device, but will cause unpredictable
  // behavior in all other cases. See http://bugs.webrtc.org/8682.
  rtc::Optional<bool> ios_force_software_aec_HACK;
#endif
  // Audio processing to adjust the sensitivity of the local mic dynamically.
  rtc::Optional<bool> auto_gain_control;
  // Audio processing to filter out background noise.
  rtc::Optional<bool> noise_suppression;
  // Audio processing to remove background noise of lower frequencies.
  rtc::Optional<bool> highpass_filter;
  // Audio processing to swap the left and right channels.
  rtc::Optional<bool> stereo_swapping;
  // Audio receiver jitter buffer (NetEq) max capacity in number of packets.
  rtc::Optional<int> audio_jitter_buffer_max_packets;
  // Audio receiver jitter buffer (NetEq) fast accelerate mode.
  rtc::Optional<bool> audio_jitter_buffer_fast_accelerate;
  // Audio processing to detect typing.
  rtc::Optional<bool> typing_detection;
  rtc::Optional<bool> aecm_generate_comfort_noise;
  rtc::Optional<bool> experimental_agc;
  rtc::Optional<bool> extended_filter_aec;
  rtc::Optional<bool> delay_agnostic_aec;
  rtc::Optional<bool> experimental_ns;
  rtc::Optional<bool> intelligibility_enhancer;
  rtc::Optional<bool> level_control;
  // Specifies an optional initialization value for the level controller.
  rtc::Optional<float> level_control_initial_peak_level_dbfs;
  // Note that tx_agc_* only applies to non-experimental AGC.
  rtc::Optional<bool> residual_echo_detector;
  rtc::Optional<uint16_t> tx_agc_target_dbov;
  rtc::Optional<uint16_t> tx_agc_digital_compression_gain;
  rtc::Optional<bool> tx_agc_limiter;
  // Enable combined audio+bandwidth BWE.
  // TODO(pthatcher): This flag is set from the
  // "googCombinedAudioVideoBwe", but not used anywhere. So delete it,
  // and check if any other AudioOptions members are unused.
  rtc::Optional<bool> combined_audio_video_bwe;
  // Enable audio network adaptor.
  rtc::Optional<bool> audio_network_adaptor;
  // Config string for audio network adaptor.
  rtc::Optional<std::string> audio_network_adaptor_config;

 private:
  template <class T>
  static std::string ToStringIfSet(const char* key,
                                   const rtc::Optional<T>& val) {
    std::string str;
    if (val) {
      str = key;
      str += ": ";
      str += val ? rtc::ToString(*val) : "";
      str += ", ";
    }
    return str;
  }

  template <typename T>
  static void SetFrom(rtc::Optional<T>* s, const rtc::Optional<T>& o) {
    if (o) {
      *s = o;
    }
  }
};

}  // namespace cricket

#endif  // API_AUDIO_OPTIONS_H_
