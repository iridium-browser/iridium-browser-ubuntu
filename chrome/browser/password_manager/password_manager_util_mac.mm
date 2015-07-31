// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_util.h"

#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#include <Security/Authorization.h>

#include "base/basictypes.h"
#include "base/mac/authorization_util.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_authorizationref.h"
#include "chrome/grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace password_manager_util {

bool AuthenticateUser(gfx::NativeWindow window) {
  NSString* identifier = [base::mac::MainBundle() bundleIdentifier];
  AuthorizationString name =
      [[identifier stringByAppendingString:@".show-passwords"] UTF8String];
  AuthorizationItem right_items[] = {
    {name, 0, NULL, 0}
  };
  AuthorizationRights rights = {arraysize(right_items), right_items};

  NSString* prompt =
      l10n_util::GetNSString(IDS_PASSWORDS_PAGE_AUTHENTICATION_PROMPT);

  // Pass kAuthorizationFlagDestroyRights to prevent the OS from saving the
  // authorization and not prompting the user when future requests are made.
  base::mac::ScopedAuthorizationRef authorization(
      base::mac::GetAuthorizationRightsWithPrompt(
          &rights, base::mac::NSToCFCast(prompt),
          kAuthorizationFlagDestroyRights));
  return authorization.get() != NULL;
}

// TODO(dubroy): Implement on Mac.
void GetOsPasswordStatus(const base::Callback<void(OsPasswordStatus)>& reply) {
  reply.Run(PASSWORD_STATUS_UNSUPPORTED);
}

}  // namespace password_manager_util
