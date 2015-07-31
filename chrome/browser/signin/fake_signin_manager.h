// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_FAKE_SIGNIN_MANAGER_H_
#define CHROME_BROWSER_SIGNIN_FAKE_SIGNIN_MANAGER_H_

#include <string>

#include "base/compiler_specific.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"

namespace content {
class BrowserContext;
}

class Profile;

// SigninManager to use for testing. Tests should use the type
// SigninManagerForTesting to ensure that the right type for their platform is
// used.

// Overrides InitTokenService to do-nothing in tests.
class FakeSigninManagerBase : public SigninManagerBase {
 public:
  explicit FakeSigninManagerBase(Profile* profile);
  ~FakeSigninManagerBase() override;

  // Helper function to be used with
  // KeyedService::SetTestingFactory(). In order to match
  // the API of SigninManagerFactory::GetForProfile(), returns a
  // FakeSigninManagerBase* on ChromeOS, and a FakeSigninManager* on all other
  // platforms. The returned instance is initialized.
  static KeyedService* Build(content::BrowserContext* context);
};

#if !defined(OS_CHROMEOS)

// A signin manager that bypasses actual authentication routines with servers
// and accepts the credentials provided to StartSignIn.
class FakeSigninManager : public SigninManager {
 public:
  explicit FakeSigninManager(Profile* profile);
  ~FakeSigninManager() override;

  void set_auth_in_progress(const std::string& account_id) {
    possibly_invalid_account_id_ = account_id;
  }

  void set_password(const std::string& password) { password_ = password; }

  void SignIn(const std::string& account_id,
              const std::string& username,
              const std::string& password);

  void FailSignin(const GoogleServiceAuthError& error);

  void StartSignInWithRefreshToken(
      const std::string& refresh_token,
      const std::string& gaia_id,
      const std::string& username,
      const std::string& password,
      const OAuthTokenFetchedCallback& oauth_fetched_callback) override;

  void SignOut(signin_metrics::ProfileSignout signout_source_metric) override;

  void CompletePendingSignin() override;

  // Username specified in StartSignInWithRefreshToken() call.
  std::string username_;
};

#endif  // !defined (OS_CHROMEOS)

#if defined(OS_CHROMEOS)
typedef FakeSigninManagerBase FakeSigninManagerForTesting;
#else
typedef FakeSigninManager FakeSigninManagerForTesting;
#endif

#endif  // CHROME_BROWSER_SIGNIN_FAKE_SIGNIN_MANAGER_H_
