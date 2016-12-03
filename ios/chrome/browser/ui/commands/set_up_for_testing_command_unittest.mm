// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/commands/set_up_for_testing_command.h"

#include "base/mac/scoped_nsobject.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

namespace {

typedef PlatformTest SetUpForTestingCommandTest;

TEST_F(SetUpForTestingCommandTest, InitNoArguments) {
  GURL url("chrome://setupfortesting");
  base::scoped_nsobject<SetUpForTestingCommand> command(
      [[SetUpForTestingCommand alloc] initWithURL:url]);
  EXPECT_FALSE([command clearBrowsingData]);
  EXPECT_FALSE([command closeTabs]);
  EXPECT_EQ(0, [command numberOfNewTabs]);
}

TEST_F(SetUpForTestingCommandTest, InitClearBrowsingData) {
  GURL url("chrome://setupfortesting?clearBrowsingData");
  base::scoped_nsobject<SetUpForTestingCommand> command(
      [[SetUpForTestingCommand alloc] initWithURL:url]);
  EXPECT_TRUE([command clearBrowsingData]);
  EXPECT_FALSE([command closeTabs]);
  EXPECT_EQ(0, [command numberOfNewTabs]);
}

TEST_F(SetUpForTestingCommandTest, InitCloseTabs) {
  GURL url("chrome://setupfortesting?closeTabs");
  base::scoped_nsobject<SetUpForTestingCommand> command(
      [[SetUpForTestingCommand alloc] initWithURL:url]);
  EXPECT_FALSE([command clearBrowsingData]);
  EXPECT_TRUE([command closeTabs]);
  EXPECT_EQ(0, [command numberOfNewTabs]);
}

TEST_F(SetUpForTestingCommandTest, InitNumberOfNewTabs) {
  GURL url("chrome://setupfortesting?numberOfNewTabs=3");
  base::scoped_nsobject<SetUpForTestingCommand> command(
      [[SetUpForTestingCommand alloc] initWithURL:url]);
  EXPECT_FALSE([command clearBrowsingData]);
  EXPECT_FALSE([command closeTabs]);
  EXPECT_EQ(3, [command numberOfNewTabs]);
}

TEST_F(SetUpForTestingCommandTest, InitWithBadNumberOfNewTabs) {
  GURL url("chrome://setupfortesting?numberOfNewTabs=a");
  base::scoped_nsobject<SetUpForTestingCommand> command(
      [[SetUpForTestingCommand alloc] initWithURL:url]);
  EXPECT_FALSE([command clearBrowsingData]);
  EXPECT_FALSE([command closeTabs]);
  EXPECT_EQ(0, [command numberOfNewTabs]);
}

TEST_F(SetUpForTestingCommandTest, InitWithNegativeNumberOfNewTabs) {
  GURL url("chrome://setupfortesting?numberOfNewTabs=-3");
  base::scoped_nsobject<SetUpForTestingCommand> command(
      [[SetUpForTestingCommand alloc] initWithURL:url]);
  EXPECT_FALSE([command clearBrowsingData]);
  EXPECT_FALSE([command closeTabs]);
  EXPECT_EQ(0, [command numberOfNewTabs]);
}

TEST_F(SetUpForTestingCommandTest, InitWithArguments) {
  GURL url(
      "chrome://setupfortesting?clearBrowsingData&closeTabs&numberOfNewTabs=5");
  base::scoped_nsobject<SetUpForTestingCommand> command(
      [[SetUpForTestingCommand alloc] initWithURL:url]);
  EXPECT_TRUE([command clearBrowsingData]);
  EXPECT_TRUE([command closeTabs]);
  EXPECT_EQ(5, [command numberOfNewTabs]);
}

TEST_F(SetUpForTestingCommandTest, InitWithBadArguments) {
  GURL url("chrome://setupfortesting?badArg");
  base::scoped_nsobject<SetUpForTestingCommand> command(
      [[SetUpForTestingCommand alloc] initWithURL:url]);
  EXPECT_FALSE([command clearBrowsingData]);
  EXPECT_FALSE([command closeTabs]);
  EXPECT_EQ(0, [command numberOfNewTabs]);
}

}  // namespace
