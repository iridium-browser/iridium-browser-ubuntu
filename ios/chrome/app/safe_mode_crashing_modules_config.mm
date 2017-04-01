// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/safe_mode_crashing_modules_config.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"

namespace {

NSString* const kStartupCrashModulesKey = @"StartupCrashModules";
NSString* const kModuleFriendlyNameKey = @"ModuleFriendlyName";

}  // namespace

@interface SafeModeCrashingModulesConfig () {
  base::scoped_nsobject<NSDictionary> _configuration;
}
@end

@implementation SafeModeCrashingModulesConfig

+ (SafeModeCrashingModulesConfig*)sharedInstance {
  static SafeModeCrashingModulesConfig* instance =
      [[SafeModeCrashingModulesConfig alloc] init];
  return instance;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    NSString* configPath =
        [[NSBundle mainBundle] pathForResource:@"SafeModeCrashingModules"
                                        ofType:@"plist"];
    _configuration.reset(
        [[NSDictionary alloc] initWithContentsOfFile:configPath]);
  }
  return self;
}

- (NSString*)startupCrashModuleFriendlyName:(NSString*)modulePath {
  NSDictionary* modules = base::mac::ObjCCastStrict<NSDictionary>(
      [_configuration objectForKey:kStartupCrashModulesKey]);
  NSDictionary* module =
      base::mac::ObjCCastStrict<NSDictionary>(modules[modulePath]);
  return base::mac::ObjCCast<NSString>(module[kModuleFriendlyNameKey]);
}

@end
