// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/commands/show_signin_command.h"

#include "base/logging.h"
#include "base/mac/scoped_block.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"

@implementation ShowSigninCommand {
  base::mac::ScopedBlock<ShowSigninCommandCompletionCallback> _callback;
}

@synthesize operation = _operation;
@synthesize signInAccessPoint = _signInAccessPoint;

- (instancetype)initWithTag:(NSInteger)tag {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithOperation:(AuthenticationOperation)operation
                signInAccessPoint:(signin_metrics::AccessPoint)signInAccessPoint
                         callback:
                             (ShowSigninCommandCompletionCallback)callback {
  if ((self = [super initWithTag:IDC_SHOW_SIGNIN_IOS])) {
    _operation = operation;
    _signInAccessPoint = signInAccessPoint;
    _callback.reset(callback, base::scoped_policy::RETAIN);
  }
  return self;
}

- (instancetype)initWithOperation:(AuthenticationOperation)operation
                signInAccessPoint:
                    (signin_metrics::AccessPoint)signInAccessPoint {
  return [self initWithOperation:operation
               signInAccessPoint:signInAccessPoint
                        callback:nil];
}

- (ShowSigninCommandCompletionCallback)callback {
  return _callback.get();
}

@end
