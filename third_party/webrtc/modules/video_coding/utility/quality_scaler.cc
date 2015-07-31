/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/video_coding/utility/include/quality_scaler.h"

namespace webrtc {

static const int kMinFps = 10;
static const int kMeasureSeconds = 5;
static const int kFramedropPercentThreshold = 60;
static const int kLowQpThresholdDenominator = 3;
static const double kFramesizeFlucThreshold = 0.11;

QualityScaler::QualityScaler()
    : num_samples_(0), low_qp_threshold_(-1), downscale_shift_(0),
      min_width_(0), min_height_(0) {
}

void QualityScaler::Init(int max_qp) {
  ClearSamples();
  low_qp_threshold_ = max_qp / kLowQpThresholdDenominator;
}

void QualityScaler::SetMinResolution(int min_width, int min_height) {
  min_width_ = min_width;
  min_height_ = min_height;
}

// TODO(jackychen): target_framesize should be calculated from average bitrate
// in the measured period of time.
// Report framerate(fps) and target_bitrate(kbit/s) to estimate # of samples
// and get target_framesize_.
void QualityScaler::ReportFramerate(int framerate) {
  num_samples_ = static_cast<size_t>(
      kMeasureSeconds * (framerate < kMinFps ? kMinFps : framerate));
}

void QualityScaler::ReportNormalizedQP(int qp) {
  framedrop_percent_.AddSample(0);
  frame_quality_.AddSample(static_cast<double>(qp) / low_qp_threshold_);
}

void QualityScaler::ReportNormalizedFrameSizeFluctuation(
    double framesize_deviation) {
  framedrop_percent_.AddSample(0);
  frame_quality_.AddSample(framesize_deviation / kFramesizeFlucThreshold);
}

void QualityScaler::ReportDroppedFrame() {
  framedrop_percent_.AddSample(100);
}

QualityScaler::Resolution QualityScaler::GetScaledResolution(
    const I420VideoFrame& frame) {
  // Should be set through InitEncode -> Should be set by now.
  assert(low_qp_threshold_ >= 0);
  assert(num_samples_ > 0);

  Resolution res;
  res.width = frame.width();
  res.height = frame.height();

  // Update scale factor.
  int avg_drop;
  double avg_quality;
  if (framedrop_percent_.GetAverage(num_samples_, &avg_drop) &&
      avg_drop >= kFramedropPercentThreshold) {
    AdjustScale(false);
  } else if (frame_quality_.GetAverage(num_samples_, &avg_quality) &&
      avg_quality <= 1.0) {
    AdjustScale(true);
  }

  assert(downscale_shift_ >= 0);
  for (int shift = downscale_shift_;
       shift > 0 && res.width > 1 && res.height > 1;
       --shift) {
    res.width >>= 1;
    res.height >>= 1;
  }

  // Set this limitation for VP8 HW encoder to avoid crash.
  if (min_width_ > 0 && res.width * res.height < min_width_ * min_height_) {
    res.width = min_width_;
    res.height = min_height_;
  }

  return res;
}

const I420VideoFrame& QualityScaler::GetScaledFrame(
    const I420VideoFrame& frame) {
  Resolution res = GetScaledResolution(frame);
  if (res.width == frame.width())
    return frame;

  scaler_.Set(frame.width(),
              frame.height(),
              res.width,
              res.height,
              kI420,
              kI420,
              kScaleBox);
  if (scaler_.Scale(frame, &scaled_frame_) != 0)
    return frame;

  scaled_frame_.set_ntp_time_ms(frame.ntp_time_ms());
  scaled_frame_.set_timestamp(frame.timestamp());
  scaled_frame_.set_render_time_ms(frame.render_time_ms());

  return scaled_frame_;
}

void QualityScaler::ClearSamples() {
  framedrop_percent_.Reset();
  frame_quality_.Reset();
}

void QualityScaler::AdjustScale(bool up) {
  downscale_shift_ += up ? -1 : 1;
  if (downscale_shift_ < 0)
    downscale_shift_ = 0;
  ClearSamples();
}

}  // namespace webrtc
