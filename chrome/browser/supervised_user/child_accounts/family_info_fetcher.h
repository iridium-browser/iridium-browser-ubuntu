// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_FAMILY_INFO_FETCHER_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_FAMILY_INFO_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace base {
class DictionaryValue;
class ListValue;
class Time;
}

namespace net {
class URLRequestContextGetter;
}

class FamilyInfoFetcher : public OAuth2TokenService::Observer,
                          public OAuth2TokenService::Consumer,
                          public net::URLFetcherDelegate {
 public:
  enum ErrorCode {
    TOKEN_ERROR,    // Failed to get OAuth2 token.
    NETWORK_ERROR,  // Network failure.
    SERVICE_ERROR,  // Service returned an error or malformed reply.
  };
  enum FamilyMemberRole {
    HEAD_OF_HOUSEHOLD = 0,
    PARENT,
    MEMBER,
    CHILD
  };
  struct FamilyProfile {
    FamilyProfile();
    FamilyProfile(const std::string& id, const std::string& name);
    ~FamilyProfile();
    std::string id;
    std::string name;
  };
  struct FamilyMember {
    FamilyMember();
    FamilyMember(const std::string& obfuscated_gaia_id,
                 FamilyMemberRole role,
                 const std::string& display_name,
                 const std::string& email,
                 const std::string& profile_url,
                 const std::string& profile_image_url);
    FamilyMember(const FamilyMember& other);
    ~FamilyMember();
    std::string obfuscated_gaia_id;
    FamilyMemberRole role;
    // All of the following may be empty.
    std::string display_name;
    std::string email;
    std::string profile_url;
    std::string profile_image_url;
  };

  class Consumer {
   public:
    virtual void OnGetFamilyProfileSuccess(const FamilyProfile& family) {}
    virtual void OnGetFamilyMembersSuccess(
        const std::vector<FamilyMember>& members) {}
    virtual void OnFailure(ErrorCode error) {}
  };

  FamilyInfoFetcher(Consumer* consumer,
                    const std::string& account_id,
                    OAuth2TokenService* token_service,
                    net::URLRequestContextGetter* request_context);
  ~FamilyInfoFetcher() override;

  // Public so tests can use them.
  static std::string RoleToString(FamilyMemberRole role);
  static bool StringToRole(const std::string& str, FamilyMemberRole* role);

  void StartGetFamilyProfile();
  void StartGetFamilyMembers();

 private:
  // OAuth2TokenService::Observer implementation:
  void OnRefreshTokenAvailable(const std::string& account_id) override;
  void OnRefreshTokensLoaded() override;

  // OAuth2TokenService::Consumer implementation:
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // net::URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  static bool ParseMembers(const base::ListValue* list,
                           std::vector<FamilyMember>* members);
  static bool ParseMember(const base::DictionaryValue* dict,
                          FamilyMember* member);
  static void ParseProfile(const base::DictionaryValue* dict,
                           FamilyMember* member);

  void StartFetching();
  void StartFetchingAccessToken();
  void FamilyProfileFetched(const std::string& response);
  void FamilyMembersFetched(const std::string& response);

  Consumer* consumer_;
  const std::string account_id_;
  OAuth2TokenService* token_service_;
  net::URLRequestContextGetter* request_context_;

  std::string request_suffix_;
  net::URLFetcher::RequestType request_type_;
  std::unique_ptr<OAuth2TokenService::Request> access_token_request_;
  std::string access_token_;
  bool access_token_expired_;
  std::unique_ptr<net::URLFetcher> url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(FamilyInfoFetcher);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_FAMILY_INFO_FETCHER_H_

