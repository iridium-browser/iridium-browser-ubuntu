// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_AUTO_SIGNIN_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_AUTO_SIGNIN_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/passwords/base_passwords_content_view_controller.h"

// Manages the view that informs the user they're being automatically signed in.
@interface AutoSigninViewController : BasePasswordsContentViewController
- (instancetype)initWithDelegate:(id<BasePasswordsContentViewDelegate>)delegate;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_AUTO_SIGNIN_VIEW_CONTROLLER_H_
