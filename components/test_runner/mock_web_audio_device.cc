// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/mock_web_audio_device.h"

namespace test_runner {

MockWebAudioDevice::MockWebAudioDevice(double sample_rate)
    : sample_rate_(sample_rate) {}

MockWebAudioDevice::~MockWebAudioDevice() {}

void MockWebAudioDevice::start() {}

void MockWebAudioDevice::stop() {}

double MockWebAudioDevice::sampleRate() {
  return sample_rate_;
}

}  // namespace test_runner
