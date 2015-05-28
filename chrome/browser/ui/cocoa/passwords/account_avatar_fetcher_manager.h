// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_ACCOUNT_AVATAR_FETCHER_MANAGER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_ACCOUNT_AVATAR_FETCHER_MANAGER_H_

#include <Cocoa/Cocoa.h>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "net/url_request/url_request_context_getter.h"

class AccountAvatarFetcherBridge;
@class CredentialItemView;
class GURL;

// Handles retrieving avatar images for credential items.
@interface AccountAvatarFetcherManager : NSObject {
  ScopedVector<AccountAvatarFetcherBridge> bridges_;
  scoped_refptr<net::URLRequestContextGetter> requestContext_;
}

// Initializes a manager with the specified request context.
- (id)initWithRequestContext:
        (scoped_refptr<net::URLRequestContextGetter>)requestContext;

// Retrieves the image located at |avatarURL| and updates |view| if successful.
- (void)fetchAvatar:(const GURL&)avatarURL forView:(CredentialItemView*)view;

@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_ACCOUNT_AVATAR_FETCHER_MANAGER_H_
