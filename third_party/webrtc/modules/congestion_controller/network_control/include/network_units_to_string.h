/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_INCLUDE_NETWORK_UNITS_TO_STRING_H_
#define MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_INCLUDE_NETWORK_UNITS_TO_STRING_H_

#include <string>
#include "modules/congestion_controller/network_control/include/network_units.h"

namespace webrtc {
std::string ToString(const DataRate& datarate);
std::string ToString(const DataSize& datarate);
std::string ToString(const Timestamp& datarate);
std::string ToString(const TimeDelta& datarate);
}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_INCLUDE_NETWORK_UNITS_TO_STRING_H_
