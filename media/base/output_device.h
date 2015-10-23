// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_OUTPUT_DEVICE_H_
#define MEDIA_BASE_OUTPUT_DEVICE_H_

#include <string>

#include "base/callback.h"
#include "media/audio/audio_parameters.h"
#include "media/base/media_export.h"
#include "url/gurl.h"

namespace media {

// Result of an audio output device switch operation
enum SwitchOutputDeviceResult {
  SWITCH_OUTPUT_DEVICE_RESULT_SUCCESS = 0,
  SWITCH_OUTPUT_DEVICE_RESULT_ERROR_NOT_FOUND,
  SWITCH_OUTPUT_DEVICE_RESULT_ERROR_NOT_AUTHORIZED,
  SWITCH_OUTPUT_DEVICE_RESULT_ERROR_OBSOLETE,
  SWITCH_OUTPUT_DEVICE_RESULT_ERROR_NOT_SUPPORTED,
  SWITCH_OUTPUT_DEVICE_RESULT_LAST =
      SWITCH_OUTPUT_DEVICE_RESULT_ERROR_NOT_SUPPORTED,
};

typedef base::Callback<void(SwitchOutputDeviceResult)> SwitchOutputDeviceCB;

// OutputDevice is an interface that allows performing operations related
// audio output devices.

class OutputDevice {
 public:
  // Attempts to switch the audio output device.
  // Once the attempt is finished, |callback| is invoked with the
  // result of the operation passed as a parameter. The result is a value from
  // the  media::SwitchOutputDeviceResult enum.
  // There is no guarantee about the thread where |callback| will
  // be invoked, so users are advised to use media::BindToCurrentLoop() to
  // ensure that |callback| runs on the correct thread.
  // Note also that copy constructors and destructors for arguments bound to
  // |callback| may run on arbitrary threads as |callback| is moved across
  // threads. It is advisable to bind arguments such that they are released by
  // |callback| when it runs in order to avoid surprises.
  virtual void SwitchOutputDevice(const std::string& device_id,
                                  const GURL& security_origin,
                                  const SwitchOutputDeviceCB& callback) = 0;

 protected:
  virtual ~OutputDevice() {}
};

}  // namespace media

#endif  // MEDIA_BASE_OUTPUT_DEVICE_H_
