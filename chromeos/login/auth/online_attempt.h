// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_ONLINE_ATTEMPT_H_
#define CHROMEOS_LOGIN_AUTH_ONLINE_ATTEMPT_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/google_service_auth_error.h"

class GaiaAuthFetcher;

namespace base {
// TODO(skyostil): Migrate to SingleThreadTaskRunner (crbug.com/465354).
class MessageLoopProxy;
}

namespace net {
class URLRequestContextGetter;
}

namespace chromeos {
class AuthAttemptState;
class AuthAttemptStateResolver;

class CHROMEOS_EXPORT OnlineAttempt : public GaiaAuthConsumer {
 public:
  OnlineAttempt(AuthAttemptState* current_attempt,
                AuthAttemptStateResolver* callback);
  ~OnlineAttempt() override;

  // Initiate the online login attempt either through client or auth login.
  // Status will be recorded in |current_attempt|, and resolver_->Resolve() will
  // be called on the IO thread when useful state is available.
  // Must be called on the UI thread.
  void Initiate(net::URLRequestContextGetter* request_context);

  // GaiaAuthConsumer overrides. Callbacks from GaiaAuthFetcher
  void OnClientLoginFailure(const GoogleServiceAuthError& error) override;
  void OnClientLoginSuccess(
      const GaiaAuthConsumer::ClientLoginResult& credentials) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(OnlineAttemptTest, LoginSuccess);
  FRIEND_TEST_ALL_PREFIXES(OnlineAttemptTest, TwoFactorSuccess);

  // Milliseconds until we timeout our attempt to hit ClientLogin.
  static const int kClientLoginTimeoutMs;

  void TryClientLogin();
  void CancelClientLogin();

  void TriggerResolve(const AuthFailure& outcome);

  bool HasPendingFetch();
  void CancelRequest();

  scoped_refptr<base::MessageLoopProxy> message_loop_;

  AuthAttemptState* const attempt_;
  AuthAttemptStateResolver* const resolver_;

  // Handles ClientLogin communications with Gaia.
  scoped_ptr<GaiaAuthFetcher> client_fetcher_;

  // Whether we're willing to re-try the ClientLogin attempt.
  bool try_again_;

  // Used to cancel the CancelClientLogin closure.
  base::WeakPtrFactory<OnlineAttempt> weak_factory_;

  friend class OnlineAttemptTest;
  DISALLOW_COPY_AND_ASSIGN(OnlineAttempt);
};

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_AUTH_ONLINE_ATTEMPT_H_
