// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/geolocation/location_manager.h"

#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/geolocation/CLLocation+OmniboxGeolocation.h"
#import "ios/chrome/browser/geolocation/location_manager+Testing.h"
#import "ios/public/provider/chrome/browser/geolocation_updater_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class LocationManagerTest : public PlatformTest {
 public:
  LocationManagerTest() {}

 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    mock_geolocation_updater_.reset(
        [[OCMockObject mockForProtocol:@protocol(GeolocationUpdater)] retain]);

    // Set up LocationManager with a mock GeolocationUpdater.
    location_manager_.reset([[LocationManager alloc] init]);
    [location_manager_ setGeolocationUpdater:mock_geolocation_updater_.get()];
  }

  void TearDown() override {
    [location_manager_ setGeolocationUpdater:nil];

    PlatformTest::TearDown();
  }

  base::scoped_nsobject<id> mock_geolocation_updater_;
  base::scoped_nsobject<LocationManager> location_manager_;
};

// Verifies that -[LocationManager startUpdatingLocation] calls
// -[GeolocationUpdater setEnabled:] when GeolocationUpdater does not yet have
// a current location.
TEST_F(LocationManagerTest, StartUpdatingLocationNilCurrentLocation) {
  [[[mock_geolocation_updater_ expect] andReturn:nil] currentLocation];

  // Also expect the call to -[GeolocationUpdater isEnabled];
  BOOL no = NO;
  [[[mock_geolocation_updater_ expect]
      andReturnValue:OCMOCK_VALUE(no)] isEnabled];
  [[mock_geolocation_updater_ expect] requestWhenInUseAuthorization];
  [[mock_geolocation_updater_ expect] setEnabled:YES];

  [location_manager_ startUpdatingLocation];
  EXPECT_OCMOCK_VERIFY(mock_geolocation_updater_.get());
}

// Verifies that -[LocationManager startUpdatingLocation] calls
// -[GeolocationUpdater setEnabled:] when GeolocationUpdater
// |currentLocation| is stale.
TEST_F(LocationManagerTest, StartUpdatingLocationStaleCurrentLocation) {
  // Set up to return a stale mock CLLocation from -[GeolocationUpdater
  // currentLocation].
  base::scoped_nsobject<id> mock_location(
      [[OCMockObject mockForClass:[CLLocation class]] retain]);
  BOOL yes = YES;
  [[[mock_location expect] andReturnValue:OCMOCK_VALUE(yes)] cr_shouldRefresh];

  [[[mock_geolocation_updater_ expect]
      andReturn:mock_location.get()] currentLocation];

  // Also expect the call to -[GeolocationUpdater isEnabled];
  BOOL no = NO;
  [[[mock_geolocation_updater_ expect]
      andReturnValue:OCMOCK_VALUE(no)] isEnabled];
  [[mock_geolocation_updater_ expect] requestWhenInUseAuthorization];
  [[mock_geolocation_updater_ expect] setEnabled:YES];

  [location_manager_ startUpdatingLocation];
  EXPECT_OCMOCK_VERIFY(mock_geolocation_updater_.get());
  EXPECT_OCMOCK_VERIFY(mock_location.get());
}

// Verifies that -[LocationManager startUpdatingLocation] does not call
// -[GeolocationUpdater setEnabled:] when GeolocationUpdater's
// |currentLocation| is fresh.
TEST_F(LocationManagerTest, StartUpdatingLocationFreshCurrentLocation) {
  // Set up to return a fresh mock CLLocation from -[GeolocationUpdater
  // currentLocation].
  base::scoped_nsobject<id> mock_location(
      [[OCMockObject mockForClass:[CLLocation class]] retain]);
  BOOL no = NO;
  [[[mock_location expect] andReturnValue:OCMOCK_VALUE(no)] cr_shouldRefresh];

  [[[mock_geolocation_updater_ expect]
      andReturn:mock_location.get()] currentLocation];

  [location_manager_ startUpdatingLocation];
  EXPECT_OCMOCK_VERIFY(mock_geolocation_updater_.get());
  EXPECT_OCMOCK_VERIFY(mock_location.get());
}

}  // namespace
