/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/audio_codecs/audio_encoder_factory_template.h"
#include "webrtc/api/audio_codecs/g722/audio_encoder_g722.h"
#include "webrtc/api/audio_codecs/ilbc/audio_encoder_ilbc.h"
#include "webrtc/api/audio_codecs/opus/audio_encoder_opus.h"
#include "webrtc/rtc_base/ptr_util.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/mock_audio_encoder.h"

namespace webrtc {

namespace {

struct BogusParams {
  static SdpAudioFormat AudioFormat() { return {"bogus", 8000, 1}; }
  static AudioCodecInfo CodecInfo() { return {8000, 1, 12345}; }
};

struct ShamParams {
  static SdpAudioFormat AudioFormat() {
    return {"sham", 16000, 2, {{"param", "value"}}};
  }
  static AudioCodecInfo CodecInfo() { return {16000, 2, 23456}; }
};

struct MyLittleConfig {
  SdpAudioFormat audio_format;
};

template <typename Params>
struct AudioEncoderFakeApi {
  static rtc::Optional<MyLittleConfig> SdpToConfig(
      const SdpAudioFormat& audio_format) {
    if (Params::AudioFormat() == audio_format) {
      MyLittleConfig config = {audio_format};
      return rtc::Optional<MyLittleConfig>(config);
    } else {
      return rtc::Optional<MyLittleConfig>();
    }
  }

  static void AppendSupportedEncoders(std::vector<AudioCodecSpec>* specs) {
    specs->push_back({Params::AudioFormat(), Params::CodecInfo()});
  }

  static AudioCodecInfo QueryAudioEncoder(const MyLittleConfig&) {
    return Params::CodecInfo();
  }

  static std::unique_ptr<AudioEncoder> MakeAudioEncoder(const MyLittleConfig&,
                                                        int payload_type) {
    auto enc = rtc::MakeUnique<testing::StrictMock<MockAudioEncoder>>();
    EXPECT_CALL(*enc, SampleRateHz())
        .WillOnce(testing::Return(Params::CodecInfo().sample_rate_hz));
    EXPECT_CALL(*enc, Die());
    return std::move(enc);
  }
};

}  // namespace

TEST(AudioEncoderFactoryTemplateTest, NoEncoderTypes) {
  rtc::scoped_refptr<AudioEncoderFactory> factory(
      new rtc::RefCountedObject<
          audio_encoder_factory_template_impl::AudioEncoderFactoryT<>>());
  EXPECT_THAT(factory->GetSupportedEncoders(), testing::IsEmpty());
  EXPECT_EQ(rtc::Optional<AudioCodecInfo>(),
            factory->QueryAudioEncoder({"foo", 8000, 1}));
  EXPECT_EQ(nullptr, factory->MakeAudioEncoder(17, {"bar", 16000, 1}));
}

TEST(AudioEncoderFactoryTemplateTest, OneEncoderType) {
  auto factory = CreateAudioEncoderFactory<AudioEncoderFakeApi<BogusParams>>();
  EXPECT_THAT(factory->GetSupportedEncoders(),
              testing::ElementsAre(
                  AudioCodecSpec{{"bogus", 8000, 1}, {8000, 1, 12345}}));
  EXPECT_EQ(rtc::Optional<AudioCodecInfo>(),
            factory->QueryAudioEncoder({"foo", 8000, 1}));
  EXPECT_EQ(rtc::Optional<AudioCodecInfo>({8000, 1, 12345}),
            factory->QueryAudioEncoder({"bogus", 8000, 1}));
  EXPECT_EQ(nullptr, factory->MakeAudioEncoder(17, {"bar", 16000, 1}));
  auto enc = factory->MakeAudioEncoder(17, {"bogus", 8000, 1});
  ASSERT_NE(nullptr, enc);
  EXPECT_EQ(8000, enc->SampleRateHz());
}

TEST(AudioEncoderFactoryTemplateTest, TwoEncoderTypes) {
  auto factory = CreateAudioEncoderFactory<AudioEncoderFakeApi<BogusParams>,
                                           AudioEncoderFakeApi<ShamParams>>();
  EXPECT_THAT(factory->GetSupportedEncoders(),
              testing::ElementsAre(
                  AudioCodecSpec{{"bogus", 8000, 1}, {8000, 1, 12345}},
                  AudioCodecSpec{{"sham", 16000, 2, {{"param", "value"}}},
                                 {16000, 2, 23456}}));
  EXPECT_EQ(rtc::Optional<AudioCodecInfo>(),
            factory->QueryAudioEncoder({"foo", 8000, 1}));
  EXPECT_EQ(rtc::Optional<AudioCodecInfo>({8000, 1, 12345}),
            factory->QueryAudioEncoder({"bogus", 8000, 1}));
  EXPECT_EQ(
      rtc::Optional<AudioCodecInfo>({16000, 2, 23456}),
      factory->QueryAudioEncoder({"sham", 16000, 2, {{"param", "value"}}}));
  EXPECT_EQ(nullptr, factory->MakeAudioEncoder(17, {"bar", 16000, 1}));
  auto enc1 = factory->MakeAudioEncoder(17, {"bogus", 8000, 1});
  ASSERT_NE(nullptr, enc1);
  EXPECT_EQ(8000, enc1->SampleRateHz());
  EXPECT_EQ(nullptr, factory->MakeAudioEncoder(17, {"sham", 16000, 2}));
  auto enc2 =
      factory->MakeAudioEncoder(17, {"sham", 16000, 2, {{"param", "value"}}});
  ASSERT_NE(nullptr, enc2);
  EXPECT_EQ(16000, enc2->SampleRateHz());
}

TEST(AudioEncoderFactoryTemplateTest, G722) {
  auto factory = CreateAudioEncoderFactory<AudioEncoderG722>();
  EXPECT_THAT(factory->GetSupportedEncoders(),
              testing::ElementsAre(
                  AudioCodecSpec{{"g722", 8000, 1}, {16000, 1, 64000}}));
  EXPECT_EQ(rtc::Optional<AudioCodecInfo>(),
            factory->QueryAudioEncoder({"foo", 8000, 1}));
  EXPECT_EQ(rtc::Optional<AudioCodecInfo>({16000, 1, 64000}),
            factory->QueryAudioEncoder({"g722", 8000, 1}));
  EXPECT_EQ(nullptr, factory->MakeAudioEncoder(17, {"bar", 16000, 1}));
  auto enc = factory->MakeAudioEncoder(17, {"g722", 8000, 1});
  ASSERT_NE(nullptr, enc);
  EXPECT_EQ(16000, enc->SampleRateHz());
}

TEST(AudioEncoderFactoryTemplateTest, Ilbc) {
  auto factory = CreateAudioEncoderFactory<AudioEncoderIlbc>();
  EXPECT_THAT(factory->GetSupportedEncoders(),
              testing::ElementsAre(
                  AudioCodecSpec{{"ILBC", 8000, 1}, {8000, 1, 13333}}));
  EXPECT_EQ(rtc::Optional<AudioCodecInfo>(),
            factory->QueryAudioEncoder({"foo", 8000, 1}));
  EXPECT_EQ(rtc::Optional<AudioCodecInfo>({8000, 1, 13333}),
            factory->QueryAudioEncoder({"ilbc", 8000, 1}));
  EXPECT_EQ(nullptr, factory->MakeAudioEncoder(17, {"bar", 8000, 1}));
  auto enc = factory->MakeAudioEncoder(17, {"ilbc", 8000, 1});
  ASSERT_NE(nullptr, enc);
  EXPECT_EQ(8000, enc->SampleRateHz());
}

TEST(AudioEncoderFactoryTemplateTest, Opus) {
  auto factory = CreateAudioEncoderFactory<AudioEncoderOpus>();
  AudioCodecInfo info = {48000, 1, 32000, 6000, 510000};
  info.allow_comfort_noise = false;
  info.supports_network_adaption = true;
  EXPECT_THAT(
      factory->GetSupportedEncoders(),
      testing::ElementsAre(AudioCodecSpec{
          {"opus", 48000, 2, {{"minptime", "10"}, {"useinbandfec", "1"}}},
          info}));
  EXPECT_EQ(rtc::Optional<AudioCodecInfo>(),
            factory->QueryAudioEncoder({"foo", 8000, 1}));
  EXPECT_EQ(
      rtc::Optional<AudioCodecInfo>(info),
      factory->QueryAudioEncoder(
          {"opus", 48000, 2, {{"minptime", "10"}, {"useinbandfec", "1"}}}));
  EXPECT_EQ(nullptr, factory->MakeAudioEncoder(17, {"bar", 16000, 1}));
  auto enc = factory->MakeAudioEncoder(17, {"opus", 48000, 2});
  ASSERT_NE(nullptr, enc);
  EXPECT_EQ(48000, enc->SampleRateHz());
}

}  // namespace webrtc
