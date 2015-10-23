// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/geolocation/omnibox_geolocation_authorization_alert.h"

#import <UIKit/UIKit.h>

#import "base/ios/weak_nsobject.h"
#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/grit/ios_strings_resources.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/string_provider.h"
#include "ui/base/l10n/l10n_util_mac.h"

@interface OmniboxGeolocationAuthorizationAlert () <UIAlertViewDelegate> {
  base::WeakNSProtocol<id<OmniboxGeolocationAuthorizationAlertDelegate>>
      delegate_;
}

@end

@implementation OmniboxGeolocationAuthorizationAlert

- (instancetype)initWithDelegate:
        (id<OmniboxGeolocationAuthorizationAlertDelegate>)delegate {
  self = [super init];
  if (self) {
    delegate_.reset(delegate);
  }
  return self;
}

- (instancetype)init {
  return [self initWithDelegate:nil];
}

- (id<OmniboxGeolocationAuthorizationAlertDelegate>)delegate {
  return delegate_.get();
}

- (void)setDelegate:(id<OmniboxGeolocationAuthorizationAlertDelegate>)delegate {
  delegate_.reset(delegate);
}

- (void)showAuthorizationAlert {
  NSString* message =
      l10n_util::GetNSString(IDS_IOS_LOCATION_AUTHORIZATION_ALERT);
  NSString* cancel = l10n_util::GetNSString(IDS_IOS_LOCATION_USAGE_CANCEL);
  NSString* ok = base::SysUTF16ToNSString(
      ios::GetChromeBrowserProvider()->GetStringProvider()->GetOKString());

  // Use a UIAlertView to match the style of the iOS system location alert.
  base::scoped_nsobject<UIAlertView> alertView(
      [[UIAlertView alloc] initWithTitle:nil
                                 message:message
                                delegate:self
                       cancelButtonTitle:cancel
                       otherButtonTitles:ok, nil]);

  [alertView show];
}

#pragma mark - UIAlertViewDelegate

- (void)alertView:(UIAlertView*)alertView
    clickedButtonAtIndex:(NSInteger)buttonIndex {
  switch (buttonIndex) {
    case 0:
      [delegate_ authorizationAlertDidCancel:self];
      break;
    case 1:
      [delegate_ authorizationAlertDidAuthorize:self];
      break;
    default:
      NOTREACHED();
      break;
  }
}

@end
