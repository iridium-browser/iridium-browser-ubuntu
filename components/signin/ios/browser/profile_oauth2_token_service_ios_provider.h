// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_IOS_BROWSER_PROFILE_OAUTH2_TOKEN_SERVICE_IOS_PROVIDER_H_
#define COMPONENTS_SIGNIN_IOS_BROWSER_PROFILE_OAUTH2_TOKEN_SERVICE_IOS_PROVIDER_H_

#if defined(__OBJC__)
@class NSDate;
@class NSError;
@class NSString;
#else
class NSDate;
class NSError;
class NSString;
#endif  // defined(__OBJC__)

#include <set>
#include <string>
#include <vector>

#include "base/callback.h"

enum AuthenticationErrorCategory {
  // Unknown errors.
  kAuthenticationErrorCategoryUnknownErrors,
  // Authorization errors.
  kAuthenticationErrorCategoryAuthorizationErrors,
  // Authorization errors with HTTP_FORBIDDEN (403) error code.
  kAuthenticationErrorCategoryAuthorizationForbiddenErrors,
  // Network server errors includes parsing error and should be treated as
  // transient/offline errors.
  kAuthenticationErrorCategoryNetworkServerErrors,
  // User cancellation errors should be handled by treating them as a no-op.
  kAuthenticationErrorCategoryUserCancellationErrors,
  // User identity not found errors.
  kAuthenticationErrorCategoryUnknownIdentityErrors,
};

// Interface that provides support for ProfileOAuth2TokenServiceIOS.
class ProfileOAuth2TokenServiceIOSProvider {
 public:
  // Account information.
  struct AccountInfo {
    std::string gaia;
    std::string email;
  };

  typedef base::Callback<void(NSString* token,
                              NSDate* expiration,
                              NSError* error)> AccessTokenCallback;

  ProfileOAuth2TokenServiceIOSProvider() {}
  virtual ~ProfileOAuth2TokenServiceIOSProvider() {}

  // Returns the ids of all accounts.
  virtual std::vector<AccountInfo> GetAllAccounts() const = 0;

  // Returns the account info composed of a GAIA id and email corresponding to
  // email |email|.
  virtual AccountInfo GetAccountInfoForEmail(
      const std::string& email) const = 0;

  // Returns the account info composed of a GAIA id and email corresponding to
  // GAIA id |gaia|.
  virtual AccountInfo GetAccountInfoForGaia(const std::string& gaia) const = 0;

  // Starts fetching an access token for the account with id |gaia_id| with
  // the given |scopes|. Once the token is obtained, |callback| is called.
  virtual void GetAccessToken(const std::string& gaia_id,
                              const std::string& client_id,
                              const std::string& client_secret,
                              const std::set<std::string>& scopes,
                              const AccessTokenCallback& callback) = 0;

  // Returns the authentication error category of |error|.
  virtual AuthenticationErrorCategory GetAuthenticationErrorCategory(
      NSError* error) const = 0;
};

#endif  // COMPONENTS_SIGNIN_IOS_BROWSER_PROFILE_OAUTH2_TOKEN_SERVICE_IOS_PROVIDER_H_
