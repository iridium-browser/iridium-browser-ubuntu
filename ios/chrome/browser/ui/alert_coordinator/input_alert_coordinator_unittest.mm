// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/alert_coordinator/input_alert_coordinator.h"

#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

TEST(InputAlertCoordinatorTest, AddTextField) {
  // Setup.
  UIViewController* viewController =
      [[[UIViewController alloc] init] autorelease];
  InputAlertCoordinator* alertCoordinator = [[[InputAlertCoordinator alloc]
      initWithBaseViewController:viewController
                           title:@"Test"
                         message:nil] autorelease];

  void (^emptyHandler)(UITextField* textField) = ^(UITextField* textField) {
  };
  id alert =
      [OCMockObject partialMockForObject:alertCoordinator.alertController];
  [[alert expect] addTextFieldWithConfigurationHandler:emptyHandler];

  // Action.
  [alertCoordinator addTextFieldWithConfigurationHandler:emptyHandler];

  // Test.
  EXPECT_OCMOCK_VERIFY(alert);
}

TEST(InputAlertCoordinatorTest, GetTextFields) {
  // Setup.
  UIViewController* viewController =
      [[[UIViewController alloc] init] autorelease];
  InputAlertCoordinator* alertCoordinator = [[[InputAlertCoordinator alloc]
      initWithBaseViewController:viewController
                           title:@"Test"
                         message:nil] autorelease];

  NSArray<UITextField*>* array = [NSArray array];
  id alert =
      [OCMockObject partialMockForObject:alertCoordinator.alertController];
  [[[alert expect] andReturn:array] textFields];

  // Action.
  NSArray<UITextField*>* resultArray = alertCoordinator.textFields;

  // Test.
  EXPECT_EQ(array, resultArray);
  EXPECT_OCMOCK_VERIFY(alert);
}
