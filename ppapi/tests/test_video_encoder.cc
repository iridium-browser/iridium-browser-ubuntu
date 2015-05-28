// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_video_encoder.h"

#include "ppapi/c/pp_codecs.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/video_encoder.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(VideoEncoder);

bool TestVideoEncoder::Init() {
  video_encoder_interface_ = static_cast<const PPB_VideoEncoder_0_1*>(
      pp::Module::Get()->GetBrowserInterface(PPB_VIDEOENCODER_INTERFACE_0_1));
  return video_encoder_interface_ && CheckTestingInterface();
}

void TestVideoEncoder::RunTests(const std::string& filter) {
  RUN_CALLBACK_TEST(TestVideoEncoder, Create, filter);
}

std::string TestVideoEncoder::TestCreate() {
  // Test that we get results for supported formats.
  {
    pp::VideoEncoder video_encoder(instance_);
    ASSERT_FALSE(video_encoder.is_null());

    TestCompletionCallbackWithOutput<std::vector<PP_VideoProfileDescription> >
        callback(instance_->pp_instance(), false);
    callback.WaitForResult(
        video_encoder.GetSupportedProfiles(callback.GetCallback()));

    ASSERT_EQ(PP_OK, callback.result());

    const std::vector<PP_VideoProfileDescription> video_profiles =
        callback.output();
    ASSERT_GE(video_profiles.size(), 1U);

    bool found_vp8 = false;
    for (uint32_t i = 0; i < video_profiles.size(); ++i) {
      const PP_VideoProfileDescription& description = video_profiles[i];
      if (description.profile == PP_VIDEOPROFILE_VP8_ANY)
        found_vp8 = true;
    }
    ASSERT_TRUE(found_vp8);
  }
  // Test that initializing the encoder with incorrect size fails.
  {
    pp::VideoEncoder video_encoder(instance_);
    ASSERT_FALSE(video_encoder.is_null());
    pp::Size video_size(0, 0);

    TestCompletionCallback callback(instance_->pp_instance(), false);
    callback.WaitForResult(
        video_encoder.Initialize(PP_VIDEOFRAME_FORMAT_I420,
                                 video_size,
                                 PP_VIDEOPROFILE_VP8_ANY,
                                 1000000,
                                 PP_HARDWAREACCELERATION_WITHFALLBACK,
                                 callback.GetCallback()));

    ASSERT_EQ(PP_ERROR_BADARGUMENT, callback.result());
  }
  // Test that initializing the encoder with software VP8 succeeds.
  {
    pp::VideoEncoder video_encoder(instance_);
    ASSERT_FALSE(video_encoder.is_null());
    pp::Size video_size(640, 480);

    TestCompletionCallback callback(instance_->pp_instance(), false);
    callback.WaitForResult(
        video_encoder.Initialize(PP_VIDEOFRAME_FORMAT_I420,
                                 video_size,
                                 PP_VIDEOPROFILE_VP8_ANY,
                                 1000000,
                                 PP_HARDWAREACCELERATION_WITHFALLBACK,
                                 callback.GetCallback()));

    ASSERT_EQ(PP_OK, callback.result());

    pp::Size coded_size;
    ASSERT_EQ(PP_OK, video_encoder.GetFrameCodedSize(&coded_size));
    ASSERT_GE(coded_size.GetArea(), video_size.GetArea());
    ASSERT_GE(video_encoder.GetFramesRequired(), 1);
  }

  PASS();
}
