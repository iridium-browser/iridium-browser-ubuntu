/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/videoprocessor_integrationtest.h"

#include <vector>

#include "test/field_trial.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

namespace {
const int kForemanNumFrames = 300;
}  // namespace

class VideoProcessorIntegrationTestVideoToolbox
    : public VideoProcessorIntegrationTest {
 protected:
  VideoProcessorIntegrationTestVideoToolbox() {
    config_.filename = "foreman_cif";
    config_.filepath = ResourcePath(config_.filename, "yuv");
    config_.num_frames = kForemanNumFrames;
    config_.hw_encoder = true;
    config_.hw_decoder = true;
    config_.encoded_frame_checker = &h264_keyframe_checker_;
  }
};

// TODO(webrtc:9099): Disabled until the issue is fixed.
// HW codecs don't work on simulators. Only run these tests on device.
// #if TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR
//  #define MAYBE_TEST_F TEST_F
// #else
#define MAYBE_TEST_F(s, name) TEST_F(s, DISABLED_##name)
// #endif

// TODO(kthelgason): Use RC Thresholds when the internal bitrateAdjuster is no
// longer in use.
MAYBE_TEST_F(VideoProcessorIntegrationTestVideoToolbox,
       ForemanCif500kbpsH264CBP) {
  config_.SetCodecSettings(kVideoCodecH264, 1, 1, 1, false, false, false, false,
                           352, 288);

  std::vector<RateProfile> rate_profiles = {{500, 30, kForemanNumFrames}};

  std::vector<QualityThresholds> quality_thresholds = {{33, 29, 0.9, 0.82}};

  ProcessFramesAndMaybeVerify(rate_profiles, nullptr,
                              &quality_thresholds, nullptr, nullptr);
}

MAYBE_TEST_F(VideoProcessorIntegrationTestVideoToolbox,
       ForemanCif500kbpsH264CHP) {
  ScopedFieldTrials override_field_trials("WebRTC-H264HighProfile/Enabled/");

  config_.h264_codec_settings.profile = H264::kProfileConstrainedHigh;
  config_.SetCodecSettings(kVideoCodecH264, 1, 1, 1, false, false, false, false,
                           352, 288);

  std::vector<RateProfile> rate_profiles = {{500, 30, kForemanNumFrames}};

  std::vector<QualityThresholds> quality_thresholds = {{33, 30, 0.91, 0.83}};

  ProcessFramesAndMaybeVerify(rate_profiles, nullptr,
                              &quality_thresholds, nullptr, nullptr);
}

}  // namespace test
}  // namespace webrtc
