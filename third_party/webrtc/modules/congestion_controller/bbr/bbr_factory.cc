/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/bbr/bbr_factory.h"
#include <memory>

#include "modules/congestion_controller/bbr/bbr_network_controller.h"
#include "rtc_base/ptr_util.h"

namespace webrtc {

BbrNetworkControllerFactory::BbrNetworkControllerFactory() {}

std::unique_ptr<NetworkControllerInterface> BbrNetworkControllerFactory::Create(
    NetworkControllerObserver* observer,
    NetworkControllerConfig config) {
  return rtc::MakeUnique<bbr::BbrNetworkController>(observer, config);
}

TimeDelta BbrNetworkControllerFactory::GetProcessInterval() const {
  return TimeDelta::PlusInfinity();
}

}  // namespace webrtc
