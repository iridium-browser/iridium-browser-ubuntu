// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/elements/activity_overlay_coordinator.h"

#include "base/ios/weak_nsobject.h"
#include "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/elements/activity_overlay_view_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

// Tests that invoking start and stop on the coordinator presents and dismisses
// the activity overlay view, respectively.
TEST(ActivityOverlayCoordinatorTest, StartAndStop) {
  base::WeakNSObject<UIView> overlay_view;
  @autoreleasepool {
    UIViewController* base_view_controller =
        [[[UIViewController alloc] init] autorelease];
    ActivityOverlayCoordinator* coordinator =
        [[[ActivityOverlayCoordinator alloc]
            initWithBaseViewController:base_view_controller] autorelease];

    EXPECT_EQ(0u, [base_view_controller.childViewControllers count]);

    [coordinator start];
    EXPECT_EQ(1u, [base_view_controller.childViewControllers count]);
    overlay_view.reset(
        [base_view_controller.childViewControllers firstObject].view);
    EXPECT_TRUE(
        [[base_view_controller.view subviews] containsObject:overlay_view]);

    [coordinator stop];
    EXPECT_EQ(0u, [base_view_controller.childViewControllers count]);
  }
  EXPECT_FALSE(overlay_view.get());
}
