// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/audio/audio_devices_pref_handler_impl.h"

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromeos/audio/audio_device.h"
#include "chromeos/audio/audio_devices_pref_handler.h"
#include "chromeos/chromeos_pref_names.h"
#include "chromeos/dbus/audio_node.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

const uint64_t kInternalMicId = 10003;
const uint64_t kHeadphoneId = 10002;
const uint64_t kHDMIOutputId = 10006;
const uint64_t kUSBMicId = 10004;
const uint64_t kOtherTypeOutputId = 90001;
const uint64_t kOtherTypeInputId = 90002;

const AudioDevice kInternalMic(AudioNode(true,
                                         kInternalMicId,
                                         kInternalMicId,
                                         "Fake Mic",
                                         "INTERNAL_MIC",
                                         "Internal Mic",
                                         false,
                                         0));
const AudioDevice kUSBMic(AudioNode(true,
                                    kUSBMicId,
                                    kUSBMicId,
                                    "Fake USB Mic",
                                    "USB",
                                    "USB Microphone",
                                    false,
                                    0));

const AudioDevice kHeadphone(AudioNode(false,
                                       kHeadphoneId,
                                       kHeadphoneId,
                                       "Fake Headphone",
                                       "HEADPHONE",
                                       "Headphone",
                                       false,
                                       0));

const AudioDevice kHDMIOutput(AudioNode(false,
                                        kHDMIOutputId,
                                        kHDMIOutputId,
                                        "HDMI output",
                                        "HDMI",
                                        "HDMI output",
                                        false,
                                        0));

const AudioDevice kInputDeviceWithSpecialCharacters(
    AudioNode(true,
              kOtherTypeInputId,
              kOtherTypeInputId,
              "Fake ~!@#$%^&*()_+`-=<>?,./{}|[]\\\\Mic",
              "SOME_OTHER_TYPE",
              "Other Type Input Device",
              true,
              0));

const AudioDevice kOutputDeviceWithSpecialCharacters(
    AudioNode(false,
              kOtherTypeOutputId,
              kOtherTypeOutputId,
              "Fake ~!@#$%^&*()_+`-=<>?,./{}|[]\\\\Headphone",
              "SOME_OTHER_TYPE",
              "Other Type Output Device",
              false,
              0));

class AudioDevicesPrefHandlerTest : public testing::Test {
 public:
  AudioDevicesPrefHandlerTest() {}
  ~AudioDevicesPrefHandlerTest() override {}

  void SetUp() override {
    pref_service_.reset(new TestingPrefServiceSimple());
    AudioDevicesPrefHandlerImpl::RegisterPrefs(pref_service_->registry());
    audio_pref_handler_ = new AudioDevicesPrefHandlerImpl(pref_service_.get());
  }

  void TearDown() override { audio_pref_handler_ = NULL; }

 protected:
  scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioDevicesPrefHandlerTest);
};

TEST_F(AudioDevicesPrefHandlerTest, TestDefaultValues) {
  // TODO(rkc): Once the bug with default preferences is fixed, fix this test
  // also. http://crbug.com/442489
  EXPECT_EQ(75.0, audio_pref_handler_->GetInputGainValue(&kInternalMic));
  EXPECT_EQ(75.0, audio_pref_handler_->GetOutputVolumeValue(&kHeadphone));
  EXPECT_EQ(75.0, audio_pref_handler_->GetOutputVolumeValue(&kHDMIOutput));
  bool active, activate_by_user;
  EXPECT_FALSE(audio_pref_handler_->GetDeviceActive(kInternalMic, &active,
                                                    &activate_by_user));
  EXPECT_FALSE(audio_pref_handler_->GetDeviceActive(kHeadphone, &active,
                                                    &activate_by_user));
  EXPECT_FALSE(audio_pref_handler_->GetDeviceActive(kHDMIOutput, &active,
                                                    &activate_by_user));
}

TEST_F(AudioDevicesPrefHandlerTest, PrefsRegistered) {
  // The standard audio prefs are registered.
  EXPECT_TRUE(pref_service_->FindPreference(prefs::kAudioDevicesVolumePercent));
  EXPECT_TRUE(pref_service_->FindPreference(prefs::kAudioDevicesMute));
  EXPECT_TRUE(pref_service_->FindPreference(prefs::kAudioOutputAllowed));
  EXPECT_TRUE(pref_service_->FindPreference(prefs::kAudioVolumePercent));
  EXPECT_TRUE(pref_service_->FindPreference(prefs::kAudioMute));
  EXPECT_TRUE(pref_service_->FindPreference(prefs::kAudioDevicesState));
}

TEST_F(AudioDevicesPrefHandlerTest, TestBasicInputOutputDevices) {
  audio_pref_handler_->SetVolumeGainValue(kInternalMic, 13.37);
  EXPECT_EQ(13.37, audio_pref_handler_->GetInputGainValue(&kInternalMic));
  audio_pref_handler_->SetVolumeGainValue(kHeadphone, 47.28);
  EXPECT_EQ(47.28, audio_pref_handler_->GetOutputVolumeValue(&kHeadphone));
}

TEST_F(AudioDevicesPrefHandlerTest, TestSpecialCharactersInDeviceNames) {
  audio_pref_handler_->SetVolumeGainValue(kInputDeviceWithSpecialCharacters,
                                          73.31);
  audio_pref_handler_->SetVolumeGainValue(kOutputDeviceWithSpecialCharacters,
                                          85.92);

  EXPECT_EQ(73.31, audio_pref_handler_->GetInputGainValue(
      &kInputDeviceWithSpecialCharacters));
  EXPECT_EQ(85.92, audio_pref_handler_->GetOutputVolumeValue(
      &kOutputDeviceWithSpecialCharacters));
}

TEST_F(AudioDevicesPrefHandlerTest, TestDeviceStates) {
  audio_pref_handler_->SetDeviceActive(kInternalMic, true, true);
  bool active = false;
  bool activate_by_user = false;
  EXPECT_TRUE(audio_pref_handler_->GetDeviceActive(kInternalMic, &active,
                                                   &activate_by_user));
  EXPECT_TRUE(active);
  EXPECT_TRUE(activate_by_user);

  audio_pref_handler_->SetDeviceActive(kHeadphone, true, false);
  EXPECT_TRUE(audio_pref_handler_->GetDeviceActive(kHeadphone, &active,
                                                   &activate_by_user));
  EXPECT_TRUE(active);
  EXPECT_FALSE(activate_by_user);

  audio_pref_handler_->SetDeviceActive(kHDMIOutput, false, false);
  EXPECT_TRUE(audio_pref_handler_->GetDeviceActive(kHDMIOutput, &active,
                                                   &activate_by_user));
  EXPECT_FALSE(active);

  // Device not exist in device state prefs.
  EXPECT_FALSE(audio_pref_handler_->GetDeviceActive(kUSBMic, &active,
                                                    &activate_by_user));
}

}  // namespace chromeos
