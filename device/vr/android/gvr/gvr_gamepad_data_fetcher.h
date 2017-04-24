// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_GAMEPAD_DATA_FETCHER_H_
#define DEVICE_VR_ANDROID_GVR_GAMEPAD_DATA_FETCHER_H_

#include <string>

#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/vr/vr_export.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_controller.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

namespace device {

class DEVICE_VR_EXPORT GvrGamepadDataFetcher : public GamepadDataFetcher {
 public:
  class Factory : public GamepadDataFetcherFactory {
   public:
    Factory(gvr_context* context, unsigned int display_id);
    ~Factory() override;
    std::unique_ptr<GamepadDataFetcher> CreateDataFetcher() override;
    GamepadSource source() override;

   private:
    gvr_context* context_;
    unsigned int display_id_;
  };

  GvrGamepadDataFetcher(gvr_context* context, unsigned int display_id);
  ~GvrGamepadDataFetcher() override;

  GamepadSource source() override;

  void GetGamepadData(bool devices_changed_hint) override;
  void PauseHint(bool paused) override;
  void OnAddedToProvider() override;

 private:
  std::unique_ptr<gvr::ControllerApi> controller_api_;
  gvr::ControllerState controller_state_;
  gvr::ControllerHandedness handedness_;
  unsigned int display_id_;

  DISALLOW_COPY_AND_ASSIGN(GvrGamepadDataFetcher);
};

}  // namespace device
#endif  // DEVICE_VR_ANDROID_GVR_GAMEPAD_DATA_FETCHER_H_
