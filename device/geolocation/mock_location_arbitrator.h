// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GEOLOCATION_MOCK_LOCATION_ARBITRATOR_H_
#define DEVICE_GEOLOCATION_MOCK_LOCATION_ARBITRATOR_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "device/geolocation/location_arbitrator.h"

namespace device {

struct Geoposition;

class MockLocationArbitrator : public LocationArbitrator {
 public:
  MockLocationArbitrator();

  bool providers_started() const { return providers_started_; }

  // LocationArbitrator:
  void StartProviders(bool enable_high_accuracy) override;
  void StopProviders() override;
  void OnPermissionGranted() override;
  bool HasPermissionBeenGranted() const override;

 private:
  bool permission_granted_;
  bool providers_started_;

  DISALLOW_COPY_AND_ASSIGN(MockLocationArbitrator);
};

}  // namespace device

#endif  // DEVICE_GEOLOCATION_MOCK_LOCATION_ARBITRATOR_H_
