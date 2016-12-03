/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/level_controller/level_controller.h"

#include <math.h>
#include <algorithm>
#include <numeric>

#include "webrtc/base/array_view.h"
#include "webrtc/base/arraysize.h"
#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_processing/audio_buffer.h"
#include "webrtc/modules/audio_processing/level_controller/gain_applier.h"
#include "webrtc/modules/audio_processing/level_controller/gain_selector.h"
#include "webrtc/modules/audio_processing/level_controller/noise_level_estimator.h"
#include "webrtc/modules/audio_processing/level_controller/peak_level_estimator.h"
#include "webrtc/modules/audio_processing/level_controller/saturating_gain_estimator.h"
#include "webrtc/modules/audio_processing/level_controller/signal_classifier.h"
#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"
#include "webrtc/system_wrappers/include/metrics.h"

namespace webrtc {
namespace {

void UpdateAndRemoveDcLevel(float forgetting_factor,
                            float* dc_level,
                            rtc::ArrayView<float> x) {
  RTC_DCHECK(!x.empty());
  float mean =
      std::accumulate(x.begin(), x.end(), 0) / static_cast<float>(x.size());
  *dc_level += forgetting_factor * (mean - *dc_level);

  for (float& v : x) {
    v -= *dc_level;
  }
}

float FrameEnergy(const AudioBuffer& audio) {
  float energy = 0.f;
  for (size_t k = 0; k < audio.num_channels(); ++k) {
    float channel_energy =
        std::accumulate(audio.channels_const_f()[k],
                        audio.channels_const_f()[k] + audio.num_frames(), 0,
                        [](float a, float b) -> float { return a + b * b; });
    energy = std::max(channel_energy, energy);
  }
  return energy;
}

float PeakLevel(const AudioBuffer& audio) {
  float peak_level = 0.f;
  for (size_t k = 0; k < audio.num_channels(); ++k) {
    auto channel_peak_level = std::max_element(
        audio.channels_const_f()[k],
        audio.channels_const_f()[k] + audio.num_frames(),
        [](float a, float b) { return std::abs(a) < std::abs(b); });
    peak_level = std::max(*channel_peak_level, peak_level);
  }
  return peak_level;
}

const int kMetricsFrameInterval = 1000;

}  // namespace

int LevelController::instance_count_ = 0;

void LevelController::Metrics::Initialize(int sample_rate_hz) {
  RTC_DCHECK(sample_rate_hz == AudioProcessing::kSampleRate8kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate16kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate32kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate48kHz);

  Reset();
  frame_length_ = rtc::CheckedDivExact(sample_rate_hz, 100);
}

void LevelController::Metrics::Reset() {
  metrics_frame_counter_ = 0;
  gain_sum_ = 0.f;
  peak_level_sum_ = 0.f;
  noise_energy_sum_ = 0.f;
  max_gain_ = 0.f;
  max_peak_level_ = 0.f;
  max_noise_energy_ = 0.f;
}

void LevelController::Metrics::Update(float peak_level,
                                      float noise_energy,
                                      float gain) {
  const float kdBFSOffset = 90.3090f;
  gain_sum_ += gain;
  peak_level_sum_ += peak_level;
  noise_energy_sum_ += noise_energy;
  max_gain_ = std::max(max_gain_, gain);
  max_peak_level_ = std::max(max_peak_level_, peak_level);
  max_noise_energy_ = std::max(max_noise_energy_, noise_energy);

  ++metrics_frame_counter_;
  if (metrics_frame_counter_ == kMetricsFrameInterval) {
    RTC_HISTOGRAM_COUNTS(
        "WebRTC.Audio.LevelControl.MaxNoisePower",
        static_cast<int>(10 * log10(max_noise_energy_ / frame_length_ + 1e-10f)
                         - kdBFSOffset),
        -90, 0, 50);
    RTC_HISTOGRAM_COUNTS(
        "WebRTC.Audio.LevelControl.AverageNoisePower",
        static_cast<int>(10 * log10(noise_energy_sum_ /
                                    (frame_length_ * kMetricsFrameInterval) +
                                    1e-10f) - kdBFSOffset),
        -90, 0, 50);

    RTC_HISTOGRAM_COUNTS(
        "WebRTC.Audio.LevelControl.MaxPeakLevel",
        static_cast<int>(10 * log10(max_peak_level_ * max_peak_level_ + 1e-10f)
                         - kdBFSOffset),
        -90, 0, 50);
    RTC_HISTOGRAM_COUNTS(
        "WebRTC.Audio.LevelControl.AveragePeakLevel",
        static_cast<int>(10 * log10(peak_level_sum_ * peak_level_sum_ /
                                    (kMetricsFrameInterval *
                                     kMetricsFrameInterval) +
                                    1e-10f) - kdBFSOffset),
        -90, 0, 50);

    RTC_DCHECK_LE(1.f, max_gain_);
    RTC_DCHECK_LE(1.f, gain_sum_ / kMetricsFrameInterval);
    RTC_HISTOGRAM_COUNTS("WebRTC.Audio.LevelControl.MaxGain",
                         static_cast<int>(10 * log10(max_gain_ * max_gain_)),
                         0, 33, 30);
    RTC_HISTOGRAM_COUNTS("WebRTC.Audio.LevelControl.AverageGain",
                         static_cast<int>(10 * log10(gain_sum_ * gain_sum_ /
                                                     (kMetricsFrameInterval *
                                                      kMetricsFrameInterval))),
                         0, 33, 30);
    Reset();
  }
}

LevelController::LevelController()
    : data_dumper_(new ApmDataDumper(instance_count_)),
      gain_applier_(data_dumper_.get()),
      signal_classifier_(data_dumper_.get()) {
  Initialize(AudioProcessing::kSampleRate48kHz);
  ++instance_count_;
}

LevelController::~LevelController() {}

void LevelController::Initialize(int sample_rate_hz) {
  RTC_DCHECK(sample_rate_hz == AudioProcessing::kSampleRate8kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate16kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate32kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate48kHz);
  data_dumper_->InitiateNewSetOfRecordings();
  gain_selector_.Initialize(sample_rate_hz);
  gain_applier_.Initialize(sample_rate_hz);
  signal_classifier_.Initialize(sample_rate_hz);
  noise_level_estimator_.Initialize(sample_rate_hz);
  peak_level_estimator_.Initialize();
  saturating_gain_estimator_.Initialize();
  metrics_.Initialize(sample_rate_hz);

  last_gain_ = 1.0f;
  sample_rate_hz_ = rtc::Optional<int>(sample_rate_hz);
  dc_forgetting_factor_ = 0.01f * sample_rate_hz / 48000.f;
  std::fill(dc_level_, dc_level_ + arraysize(dc_level_), 0.f);
}

void LevelController::Process(AudioBuffer* audio) {
  RTC_DCHECK_LT(0u, audio->num_channels());
  RTC_DCHECK_GE(2u, audio->num_channels());
  RTC_DCHECK_NE(0.f, dc_forgetting_factor_);
  RTC_DCHECK(sample_rate_hz_);
  data_dumper_->DumpWav("lc_input", audio->num_frames(),
                        audio->channels_const_f()[0], *sample_rate_hz_, 1);

  // Remove DC level.
  for (size_t k = 0; k < audio->num_channels(); ++k) {
    UpdateAndRemoveDcLevel(
        dc_forgetting_factor_, &dc_level_[k],
        rtc::ArrayView<float>(audio->channels_f()[k], audio->num_frames()));
  }

  SignalClassifier::SignalType signal_type;
  signal_classifier_.Analyze(*audio, &signal_type);
  int tmp = static_cast<int>(signal_type);
  data_dumper_->DumpRaw("lc_signal_type", 1, &tmp);

  // Estimate the noise energy.
  float noise_energy =
      noise_level_estimator_.Analyze(signal_type, FrameEnergy(*audio));

  // Estimate the overall signal peak level.
  float peak_level =
      peak_level_estimator_.Analyze(signal_type, PeakLevel(*audio));

  float saturating_gain = saturating_gain_estimator_.GetGain();

  // Compute the new gain to apply.
  last_gain_ = gain_selector_.GetNewGain(peak_level, noise_energy,
                                         saturating_gain, signal_type);

  // Apply the gain to the signal.
  int num_saturations = gain_applier_.Process(last_gain_, audio);

  // Estimate the gain that saturates the overall signal.
  saturating_gain_estimator_.Update(last_gain_, num_saturations);

  // Update the metrics.
  metrics_.Update(peak_level, noise_energy, last_gain_);

  data_dumper_->DumpRaw("lc_selected_gain", 1, &last_gain_);
  data_dumper_->DumpRaw("lc_noise_energy", 1, &noise_energy);
  data_dumper_->DumpRaw("lc_peak_level", 1, &peak_level);
  data_dumper_->DumpRaw("lc_saturating_gain", 1, &saturating_gain);

  data_dumper_->DumpWav("lc_output", audio->num_frames(),
                        audio->channels_f()[0], *sample_rate_hz_, 1);
}

}  // namespace webrtc
