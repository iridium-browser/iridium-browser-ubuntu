// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/fake_profile_oauth2_token_service_ios_provider.h"

#include <Foundation/Foundation.h>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"

FakeProfileOAuth2TokenServiceIOSProvider::
    FakeProfileOAuth2TokenServiceIOSProvider() {
}

FakeProfileOAuth2TokenServiceIOSProvider::
    ~FakeProfileOAuth2TokenServiceIOSProvider() {
}

void FakeProfileOAuth2TokenServiceIOSProvider::GetAccessToken(
    const std::string& account_id,
    const std::string& client_id,
    const std::string& client_secret,
    const std::set<std::string>& scopes,
    const AccessTokenCallback& callback) {
  requests_.push_back(AccessTokenRequest(account_id, callback));
}

std::vector<ProfileOAuth2TokenServiceIOSProvider::AccountInfo>
FakeProfileOAuth2TokenServiceIOSProvider::GetAllAccounts() const {
  return accounts_;
}

ProfileOAuth2TokenServiceIOSProvider::AccountInfo
FakeProfileOAuth2TokenServiceIOSProvider::GetAccountInfoForEmail(
    const std::string& email) const {
  for (const auto& account : accounts_) {
    if (account.email == email)
      return account;
  }
  return ProfileOAuth2TokenServiceIOSProvider::AccountInfo();
}

ProfileOAuth2TokenServiceIOSProvider::AccountInfo
FakeProfileOAuth2TokenServiceIOSProvider::GetAccountInfoForGaia(
    const std::string& gaia) const {
  for (const auto& account : accounts_) {
    if (account.gaia == gaia)
      return account;
  }
  return ProfileOAuth2TokenServiceIOSProvider::AccountInfo();
}

ProfileOAuth2TokenServiceIOSProvider::AccountInfo
FakeProfileOAuth2TokenServiceIOSProvider::AddAccount(const std::string& gaia,
                                                     const std::string& email) {
  ProfileOAuth2TokenServiceIOSProvider::AccountInfo account;
  account.gaia = gaia;
  account.email = email;
  accounts_.push_back(account);
  return account;
}

void FakeProfileOAuth2TokenServiceIOSProvider::ClearAccounts() {
  accounts_.clear();
}

void FakeProfileOAuth2TokenServiceIOSProvider::
    IssueAccessTokenForAllRequests() {
  for (auto i = requests_.begin(); i != requests_.end(); ++i) {
    std::string account_id = i->first;
    AccessTokenCallback callback = i->second;
    NSString* access_token = [NSString
        stringWithFormat:@"fake_access_token [account=%s]", account_id.c_str()];
    NSDate* one_hour_from_now = [NSDate dateWithTimeIntervalSinceNow:3600];
    callback.Run(access_token, one_hour_from_now, nil);
  }
  requests_.clear();
}

void FakeProfileOAuth2TokenServiceIOSProvider::
    IssueAccessTokenErrorForAllRequests() {
  for (auto i = requests_.begin(); i != requests_.end(); ++i) {
    std::string account_id = i->first;
    AccessTokenCallback callback = i->second;
    NSError* error = [[[NSError alloc] initWithDomain:@"fake_access_token_error"
                                                 code:-1
                                             userInfo:nil] autorelease];
    callback.Run(nil, nil, error);
  }
  requests_.clear();
}

AuthenticationErrorCategory
FakeProfileOAuth2TokenServiceIOSProvider::GetAuthenticationErrorCategory(
    NSError* error) const {
  DCHECK(error);
  return kAuthenticationErrorCategoryAuthorizationErrors;
}
