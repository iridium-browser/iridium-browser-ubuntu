/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_EXPERIMENTS_RATE_CONTROL_SETTINGS_H_
#define RTC_BASE_EXPERIMENTS_RATE_CONTROL_SETTINGS_H_

#include "absl/types/optional.h"
#include "api/transport/webrtc_key_value_config.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/experiments/field_trial_units.h"

namespace webrtc {

class RateControlSettings final {
 public:
  ~RateControlSettings();
  RateControlSettings(RateControlSettings&&);

  static RateControlSettings ParseFromFieldTrials();
  static RateControlSettings ParseFromKeyValueConfig(
      const WebRtcKeyValueConfig* const key_value_config);

  // When CongestionWindowPushback is enabled, the pacer is oblivious to
  // the congestion window. The relation between outstanding data and
  // the congestion window affects encoder allocations directly.
  bool UseCongestionWindow() const;
  int64_t GetCongestionWindowAdditionalTimeMs() const;
  bool UseCongestionWindowPushback() const;
  uint32_t CongestionWindowMinPushbackTargetBitrateBps() const;

  absl::optional<double> GetPacingFactor() const;
  bool UseAlrProbing() const;

  bool LibvpxVp8TrustedRateController() const;
  bool LibvpxVp9TrustedRateController() const;

  double GetSimulcastVideoHysteresisFactor() const;
  double GetSimulcastScreenshareHysteresisFactor() const;

  bool UseEncoderBitrateAdjuster() const;

 private:
  explicit RateControlSettings(
      const WebRtcKeyValueConfig* const key_value_config);

  FieldTrialOptional<int> congestion_window_;
  FieldTrialOptional<int> congestion_window_pushback_;
  FieldTrialOptional<double> pacing_factor_;
  FieldTrialParameter<bool> alr_probing_;
  FieldTrialParameter<bool> trust_vp8_;
  FieldTrialParameter<bool> trust_vp9_;
  FieldTrialParameter<double> video_hysteresis_;
  FieldTrialParameter<double> screenshare_hysteresis_;
  FieldTrialParameter<bool> bitrate_adjuster_;
};

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_RATE_CONTROL_SETTINGS_H_
