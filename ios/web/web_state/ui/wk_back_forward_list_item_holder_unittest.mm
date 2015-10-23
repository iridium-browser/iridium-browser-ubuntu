// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/wk_back_forward_list_item_holder.h"

#import <WebKit/WebKit.h>

#include "ios/web/navigation/navigation_item_impl.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/test/web_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace web {

// Test fixture for WKBackForwardListItemHolder class.
typedef PlatformTest WKBackForwardListItemHolderTest;

// Tests that FromNavigationItem returns the same holder for the same
// NavigationItem.
TEST_F(WKBackForwardListItemHolderTest, GetHolderFromNavigationItem) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  scoped_ptr<web::NavigationItem> item(NavigationItem::Create());
  WKBackForwardListItemHolder* holder1 =
      WKBackForwardListItemHolder::FromNavigationItem(item.get());
  WKBackForwardListItemHolder* holder2 =
      WKBackForwardListItemHolder::FromNavigationItem(item.get());
  EXPECT_EQ(holder1, holder2);
}

// Tests that FromNavigationItem returns different holders for different
// NavigationItem objects.
TEST_F(WKBackForwardListItemHolderTest, GetHolderFromDifferentNavigationItem) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  // Create two NavigationItem objects.
  scoped_ptr<web::NavigationItem> item1(NavigationItem::Create());
  scoped_ptr<web::NavigationItem> item2(NavigationItem::Create());
  EXPECT_NE(item1.get(), item2.get());

  // Verify that the two objects have different holders.
  WKBackForwardListItemHolder* holder1 =
      WKBackForwardListItemHolder::FromNavigationItem(item1.get());
  WKBackForwardListItemHolder* holder2 =
      WKBackForwardListItemHolder::FromNavigationItem(item2.get());
  EXPECT_NE(holder1, holder2);
}

// Tests that acessors for the WKBackForwardListItem object work as
// expected. The test bellow uses NSObject instead of WKBackForwardListItem
// because WKBackForwardListItem alloc/release is not designed to be called
// directly and will crash.
TEST_F(WKBackForwardListItemHolderTest, GetBackForwardListItemFromHolder) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  scoped_ptr<web::NavigationItem> item(NavigationItem::Create());
  base::scoped_nsobject<NSObject> input([[NSObject alloc] init]);
  WKBackForwardListItemHolder* holder =
      WKBackForwardListItemHolder::FromNavigationItem(item.get());
  holder->set_back_forward_list_item(
      static_cast<WKBackForwardListItem*>(input.get()));
  NSObject* result = holder->back_forward_list_item();
  EXPECT_EQ(input, result);
}

// Tests that acessors for navigation type work as expected.
TEST_F(WKBackForwardListItemHolderTest, GetNavigationTypeFromHolder) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  scoped_ptr<web::NavigationItem> item(NavigationItem::Create());
  WKBackForwardListItemHolder* holder =
      WKBackForwardListItemHolder::FromNavigationItem(item.get());

  // Verify that setting 'WKNavigationTypeOther' means
  // |navigation_type| returns WKNavigationTypeBackForward
  WKNavigationType type = WKNavigationTypeOther;
  holder->set_navigation_type(type);
  EXPECT_EQ(type, holder->navigation_type());

  // Verify that setting 'WKNavigationTypeBackForward' means
  // |navigation_type| returns 'WKNavigationTypeBackForward'
  type = WKNavigationTypeBackForward;
  holder->set_navigation_type(type);
  EXPECT_EQ(type, holder->navigation_type());

  // Verify that setting 'WKNavigationTypeFormSubmitted' means
  // |navigation_type| returns 'WKNavigationTypeFormSubmitted'
  type = WKNavigationTypeFormSubmitted;
  holder->set_navigation_type(type);
  EXPECT_EQ(type, holder->navigation_type());

  // Verify that setting 'WKNavigationTypeFormResubmitted' means
  // |navigation_type| returns 'WKNavigationTypeFormResubmitted'
  type = WKNavigationTypeFormResubmitted;
  holder->set_navigation_type(type);
  EXPECT_EQ(type, holder->navigation_type());

  // Verify that setting 'WKNavigationTypeReload' means
  // |navigation_type| returns 'WKNavigationTypeReload'
  type = WKNavigationTypeReload;
  holder->set_navigation_type(type);
  EXPECT_EQ(type, holder->navigation_type());

  // Verify that setting 'WKNavigationTypeLinkActivated' means
  // |navigation_type| returns 'WKNavigationTypeLinkActivated'
  type = WKNavigationTypeLinkActivated;
  holder->set_navigation_type(type);
  EXPECT_EQ(type, holder->navigation_type());
}

// Tests that |back_forward_list_item| returns nil if the internal
// WKBackForwardListItem was deallocated. The test bellow uses NSObject
// instead of WKBackForwardListItem because WKBackForwardListItem alloc/
// release is not designed to be called directly and will crash.
TEST_F(WKBackForwardListItemHolderTest, GetNilBackForwardListItemFromHolder) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  scoped_ptr<web::NavigationItem> item(NavigationItem::Create());
  WKBackForwardListItemHolder* holder =
      WKBackForwardListItemHolder::FromNavigationItem(item.get());

  // Add a WKBackForwardListItem and verify that |back_forward_list_item|
  // returns does not return nil.
  base::scoped_nsobject<NSObject> input([[NSObject alloc] init]);
  holder->set_back_forward_list_item(
      static_cast<WKBackForwardListItem*>(input.get()));
  NSObject* result = holder->back_forward_list_item();
  EXPECT_NE(nil, result);

  // Deallocate the WKBackForwardListItem and verify that
  // |back_forward_list_item| returns nil.
  input.reset();
  result = holder->back_forward_list_item();
  EXPECT_EQ(nil, result);
}

}  // namespace web
