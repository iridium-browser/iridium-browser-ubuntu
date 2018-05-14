/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_AEC_STATE_H_
#define MODULES_AUDIO_PROCESSING_AEC3_AEC_STATE_H_

#include <algorithm>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "api/audio/echo_canceller3_config.h"
#include "api/optional.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/delay_estimate.h"
#include "modules/audio_processing/aec3/echo_path_variability.h"
#include "modules/audio_processing/aec3/erl_estimator.h"
#include "modules/audio_processing/aec3/erle_estimator.h"
#include "modules/audio_processing/aec3/filter_analyzer.h"
#include "modules/audio_processing/aec3/render_buffer.h"
#include "modules/audio_processing/aec3/suppression_gain_limiter.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class ApmDataDumper;

// Handles the state and the conditions for the echo removal functionality.
class AecState {
 public:
  explicit AecState(const EchoCanceller3Config& config);
  ~AecState();

  // Returns whether the echo subtractor can be used to determine the residual
  // echo.
  bool UsableLinearEstimate() const { return usable_linear_estimate_; }

  // Returns whether the echo subtractor output should be used as output.
  bool UseLinearFilterOutput() const { return use_linear_filter_output_; }

  // Returns the estimated echo path gain.
  bool EchoPathGain() const { return filter_analyzer_.Gain(); }

  // Returns whether the render signal is currently active.
  bool ActiveRender() const { return blocks_with_active_render_ > 200; }

  // Returns the ERLE.
  const std::array<float, kFftLengthBy2Plus1>& Erle() const {
    return erle_estimator_.Erle();
  }

  // Returns the time-domain ERLE.
  float ErleTimeDomain() const { return erle_estimator_.ErleTimeDomain(); }

  // Returns the ERL.
  const std::array<float, kFftLengthBy2Plus1>& Erl() const {
    return erl_estimator_.Erl();
  }

  // Returns the time-domain ERL.
  float ErlTimeDomain() const { return erl_estimator_.ErlTimeDomain(); }

  // Returns the delay estimate based on the linear filter.
  int FilterDelayBlocks() const { return filter_delay_blocks_; }

  // Returns the internal delay estimate based on the linear filter.
  rtc::Optional<int> InternalDelay() const { return internal_delay_; }

  // Returns whether the capture signal is saturated.
  bool SaturatedCapture() const { return capture_signal_saturation_; }

  // Returns whether the echo signal is saturated.
  bool SaturatedEcho() const { return echo_saturation_; }

  // Returns whether the echo path can saturate.
  bool SaturatingEchoPath() const { return saturating_echo_path_; }

  // Updates the capture signal saturation.
  void UpdateCaptureSaturation(bool capture_signal_saturation) {
    capture_signal_saturation_ = capture_signal_saturation;
  }

  // Returns whether the transparent mode is active
  bool TransparentMode() const { return transparent_mode_; }

  // Takes appropriate action at an echo path change.
  void HandleEchoPathChange(const EchoPathVariability& echo_path_variability);

  // Returns the decay factor for the echo reverberation.
  float ReverbDecay() const { return reverb_decay_; }

  // Returns the upper limit for the echo suppression gain.
  float SuppressionGainLimit() const {
    return suppression_gain_limiter_.Limit();
  }

  // Returns whether the linear filter should have been able to properly adapt.
  bool FilterHasHadTimeToConverge() const {
    return filter_has_had_time_to_converge_;
  }

  // Returns whether the filter adaptation is still in the initial state.
  bool InitialState() const { return initial_state_; }

  // Updates the aec state.
  void Update(const rtc::Optional<DelayEstimate>& external_delay,
              const std::vector<std::array<float, kFftLengthBy2Plus1>>&
                  adaptive_filter_frequency_response,
              const std::vector<float>& adaptive_filter_impulse_response,
              bool converged_filter,
              bool diverged_filter,
              const RenderBuffer& render_buffer,
              const std::array<float, kFftLengthBy2Plus1>& E2_main,
              const std::array<float, kFftLengthBy2Plus1>& Y2,
              const std::array<float, kBlockSize>& s);

 private:
  void UpdateReverb(const std::vector<float>& impulse_response);
  bool DetectActiveRender(rtc::ArrayView<const float> x) const;
  void UpdateSuppressorGainLimit(bool render_activity);
  bool DetectEchoSaturation(rtc::ArrayView<const float> x);

  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  ErlEstimator erl_estimator_;
  ErleEstimator erle_estimator_;
  size_t capture_block_counter_ = 0;
  size_t blocks_since_reset_ = 0;
  size_t blocks_with_proper_filter_adaptation_ = 0;
  size_t blocks_with_active_render_ = 0;
  bool usable_linear_estimate_ = false;
  bool capture_signal_saturation_ = false;
  bool echo_saturation_ = false;
  bool transparent_mode_ = false;
  float previous_max_sample_ = 0.f;
  bool render_received_ = false;
  int filter_delay_blocks_ = 0;
  size_t blocks_since_last_saturation_ = 1000;
  float reverb_decay_to_test_ = 0.9f;
  float reverb_decay_candidate_ = 0.f;
  float reverb_decay_candidate_residual_ = -1.f;
  const EchoCanceller3Config config_;
  std::vector<float> max_render_;
  float reverb_decay_;
  bool saturating_echo_path_ = false;
  bool filter_has_had_time_to_converge_ = false;
  bool initial_state_ = true;
  const float gain_rampup_increase_;
  SuppressionGainUpperLimiter suppression_gain_limiter_;
  FilterAnalyzer filter_analyzer_;
  bool use_linear_filter_output_ = false;
  rtc::Optional<int> internal_delay_;
  size_t diverged_blocks_ = 0;
  bool filter_should_have_converged_ = false;
  size_t blocks_since_converged_filter_;
  size_t active_blocks_since_consistent_filter_estimate_;
  bool converged_filter_seen_ = false;
  bool consistent_filter_seen_ = false;
  bool external_delay_seen_ = false;
  size_t converged_filter_count_ = 0;
  bool finite_erl_ = false;
  size_t active_blocks_since_converged_filter_ = 0;

  RTC_DISALLOW_COPY_AND_ASSIGN(AecState);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_AEC_STATE_H_
