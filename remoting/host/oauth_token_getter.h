// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_OAUTH_TOKEN_GETTER_H_
#define REMOTING_HOST_OAUTH_TOKEN_GETTER_H_

#include <queue>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "google_apis/gaia/gaia_oauth_client.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace remoting {

// OAuthTokenGetter caches OAuth access tokens and refreshes them as needed.
class OAuthTokenGetter :
      public base::NonThreadSafe,
      public gaia::GaiaOAuthClient::Delegate {
 public:
  // Status of the refresh token attempt.
  enum Status {
    // Success, credentials in user_email/access_token.
    SUCCESS,
    // Network failure (caller may retry).
    NETWORK_ERROR,
    // Authentication failure (permanent).
    AUTH_ERROR,
  };

  typedef base::Callback<void(Status status,
                              const std::string& user_email,
                              const std::string& access_token)> TokenCallback;

  // This structure contains information required to perform
  // authentication to OAuth2.
  struct OAuthCredentials {
    // |is_service_account| should be True if the OAuth refresh token is for a
    // service account, False for a user account, to allow the correct client-ID
    // to be used.
    OAuthCredentials(const std::string& login,
                     const std::string& refresh_token,
                     bool is_service_account);

    // The user's account name (i.e. their email address).
    std::string login;

    // Token delegating authority to us to act as the user.
    std::string refresh_token;

    // Whether these credentials belong to a service account.
    bool is_service_account;
  };

  OAuthTokenGetter(scoped_ptr<OAuthCredentials> oauth_credentials,
                   const scoped_refptr<net::URLRequestContextGetter>&
                       url_request_context_getter,
                   bool auto_refresh,
                   bool verify_email);
  ~OAuthTokenGetter() override;

  // Call |on_access_token| with an access token, or the failure status.
  void CallWithToken(const OAuthTokenGetter::TokenCallback& on_access_token);

  // gaia::GaiaOAuthClient::Delegate interface.
  void OnGetTokensResponse(const std::string& user_email,
                           const std::string& access_token,
                           int expires_seconds) override;
  void OnRefreshTokenResponse(const std::string& access_token,
                              int expires_in_seconds) override;
  void OnGetUserEmailResponse(const std::string& user_email) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

 private:
  void NotifyCallbacks(Status status,
                       const std::string& user_email,
                       const std::string& access_token);
  void RefreshOAuthToken();

  scoped_ptr<OAuthCredentials> oauth_credentials_;
  scoped_ptr<gaia::GaiaOAuthClient> gaia_oauth_client_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  const bool verify_email_;

  bool refreshing_oauth_token_ = false;
  bool email_verified_ = false;
  std::string oauth_access_token_;
  base::Time auth_token_expiry_time_;
  std::queue<OAuthTokenGetter::TokenCallback> pending_callbacks_;
  scoped_ptr<base::OneShotTimer<OAuthTokenGetter> > refresh_timer_;

  DISALLOW_COPY_AND_ASSIGN(OAuthTokenGetter);
};

}  // namespace remoting

#endif  // REMOTING_HOST_OAUTH_TOKEN_GETTER_H_
