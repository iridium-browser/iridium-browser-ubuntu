// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SAFE_MODE_CRASHING_MODULES_CONFIG_H_
#define IOS_CHROME_APP_SAFE_MODE_CRASHING_MODULES_CONFIG_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/updatable_config/updatable_dictionary.h"

// Class for updatable configuration file singleton. This singleton object
// is created when +sharedInstance is called for the first time and the default
// configuration is loaded from a plist bundled into the application.
@interface SafeModeCrashingModulesConfig : UpdatableDictionary

// Returns singleton object for this class.
+ (SafeModeCrashingModulesConfig*)sharedInstance;

// Return friendly name of module if module is a known crasher.
- (NSString*)startupCrashModuleFriendlyName:(NSString*)modulePath;

@end

#endif  // IOS_CHROME_APP_SAFE_MODE_CRASHING_MODULES_CONFIG_H_
