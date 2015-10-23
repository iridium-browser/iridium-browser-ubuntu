// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/previous_session_info.h"

#include "base/strings/sys_string_conversions.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

namespace {

// Key in the UserDefaults for a boolean value keeping track of memory warnings.
NSString* const kDidSeeMemoryWarningShortlyBeforeTerminating =
    @"DidSeeMemoryWarning";

// Key in the NSUserDefaults for a string value that stores the version of the
// last session.
NSString* const kLastRanVersion = @"LastRanVersion";

TEST(PreviousSessionInfoTest, InitializationWithEmptyDefaults) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];

  // Instantiate the PreviousSessionInfo sharedInstance.
  PreviousSessionInfo* sharedInstance = [PreviousSessionInfo sharedInstance];

  // Checks the default values.
  EXPECT_FALSE([sharedInstance didSeeMemoryWarningShortlyBeforeTerminating]);
  EXPECT_TRUE([sharedInstance isFirstSessionAfterUpgrade]);
}

TEST(PreviousSessionInfoTest, InitializationWithSameVersionNoMemoryWarning) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];

  // Set the current version as the last ran version.
  NSString* currentVersion =
      base::SysUTF8ToNSString(version_info::GetVersionNumber());
  [defaults setObject:currentVersion forKey:kLastRanVersion];

  // Instantiate the PreviousSessionInfo sharedInstance.
  PreviousSessionInfo* sharedInstance = [PreviousSessionInfo sharedInstance];

  // Checks the values.
  EXPECT_FALSE([sharedInstance didSeeMemoryWarningShortlyBeforeTerminating]);
  EXPECT_FALSE([sharedInstance isFirstSessionAfterUpgrade]);
}

TEST(PreviousSessionInfoTest, InitializationWithSameVersionMemoryWarning) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];

  // Set the current version as the last ran version.
  NSString* currentVersion =
      base::SysUTF8ToNSString(version_info::GetVersionNumber());
  [defaults setObject:currentVersion forKey:kLastRanVersion];

  // Set the memory warning flag as a previous session would have.
  [defaults setBool:YES forKey:kDidSeeMemoryWarningShortlyBeforeTerminating];

  // Instantiate the PreviousSessionInfo sharedInstance.
  PreviousSessionInfo* sharedInstance = [PreviousSessionInfo sharedInstance];

  // Checks the values.
  EXPECT_TRUE([sharedInstance didSeeMemoryWarningShortlyBeforeTerminating]);
  EXPECT_FALSE([sharedInstance isFirstSessionAfterUpgrade]);
}

TEST(PreviousSessionInfoTest, InitializationDifferentVersionNoMemoryWarning) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];

  // Set the current version as the last ran version.
  [defaults setObject:@"Fake Version" forKey:kLastRanVersion];

  // Instantiate the PreviousSessionInfo sharedInstance.
  PreviousSessionInfo* sharedInstance = [PreviousSessionInfo sharedInstance];

  // Checks the values.
  EXPECT_FALSE([sharedInstance didSeeMemoryWarningShortlyBeforeTerminating]);
  EXPECT_TRUE([sharedInstance isFirstSessionAfterUpgrade]);
}

TEST(PreviousSessionInfoTest, InitializationDifferentVersionMemoryWarning) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];

  // Set the current version as the last ran version.
  [defaults setObject:@"Fake Version" forKey:kLastRanVersion];

  // Set the memory warning flag as a previous session would have.
  [defaults setBool:YES forKey:kDidSeeMemoryWarningShortlyBeforeTerminating];

  // Instantiate the PreviousSessionInfo sharedInstance.
  PreviousSessionInfo* sharedInstance = [PreviousSessionInfo sharedInstance];

  // Checks the values.
  EXPECT_TRUE([sharedInstance didSeeMemoryWarningShortlyBeforeTerminating]);
  EXPECT_TRUE([sharedInstance isFirstSessionAfterUpgrade]);
}

TEST(PreviousSessionInfoTest, BeginRecordingCurrentSession) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];

  // Set the memory warning flag as a previous session would have.
  [defaults setBool:YES forKey:kDidSeeMemoryWarningShortlyBeforeTerminating];

  [[PreviousSessionInfo sharedInstance] beginRecordingCurrentSession];

  // Check that the version has been updated.
  EXPECT_NSEQ(base::SysUTF8ToNSString(version_info::GetVersionNumber()),
              [defaults stringForKey:kLastRanVersion]);

  // Check that the memory warning flag has been reset.
  EXPECT_FALSE(
      [defaults boolForKey:kDidSeeMemoryWarningShortlyBeforeTerminating]);
}

TEST(PreviousSessionInfoTest, SetMemoryWarningFlagNoOpUntilRecordingBegins) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];

  // Call the flag setter.
  [[PreviousSessionInfo sharedInstance] setMemoryWarningFlag];

  EXPECT_FALSE(
      [defaults boolForKey:kDidSeeMemoryWarningShortlyBeforeTerminating]);
}

TEST(PreviousSessionInfoTest, ResetMemoryWarningFlagNoOpUntilRecordingBegins) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];

  // Set the memory warning flag as a previous session would have.
  [defaults setBool:YES forKey:kDidSeeMemoryWarningShortlyBeforeTerminating];

  // Call the memory warning flag resetter.
  [[PreviousSessionInfo sharedInstance] resetMemoryWarningFlag];

  EXPECT_TRUE(
      [defaults boolForKey:kDidSeeMemoryWarningShortlyBeforeTerminating]);
}

TEST(PreviousSessionInfoTest, MemoryWarningFlagMethodsAfterRecordingBegins) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];

  // Launch the recording of the session.
  [[PreviousSessionInfo sharedInstance] beginRecordingCurrentSession];

  EXPECT_FALSE(
      [defaults boolForKey:kDidSeeMemoryWarningShortlyBeforeTerminating]);

  // Call the memory warning flag setter.
  [[PreviousSessionInfo sharedInstance] setMemoryWarningFlag];

  EXPECT_TRUE(
      [defaults boolForKey:kDidSeeMemoryWarningShortlyBeforeTerminating]);

  // Call the memory warning flag resetter.
  [[PreviousSessionInfo sharedInstance] resetMemoryWarningFlag];

  EXPECT_FALSE(
      [defaults boolForKey:kDidSeeMemoryWarningShortlyBeforeTerminating]);
}

}  // namespace
