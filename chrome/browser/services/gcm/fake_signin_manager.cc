// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/fake_signin_manager.h"

#include "base/observer_list.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "content/public/browser/browser_context.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#endif

namespace gcm {

FakeSigninManager::FakeSigninManager(Profile* profile)
#if defined(OS_CHROMEOS)
    : SigninManagerBase(
        ChromeSigninClientFactory::GetInstance()->GetForProfile(profile),
        AccountTrackerServiceFactory::GetForProfile(profile)),
#else
    : SigninManager(
        ChromeSigninClientFactory::GetInstance()->GetForProfile(profile),
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
        AccountTrackerServiceFactory::GetForProfile(profile),
        GaiaCookieManagerServiceFactory::GetForProfile(profile)),
#endif
      profile_(profile) {
  Initialize(NULL);
}

FakeSigninManager::~FakeSigninManager() {
}

void FakeSigninManager::SignIn(const std::string& account_id) {
  SetAuthenticatedAccountId(account_id);
  FOR_EACH_OBSERVER(SigninManagerBase::Observer,
                    observer_list_,
                    GoogleSigninSucceeded(account_id, account_id,
                                          std::string()));
}

void FakeSigninManager::SignOut(
    signin_metrics::ProfileSignout signout_source_metric) {
  const std::string account_id = GetAuthenticatedAccountId();
  const std::string username = GetAuthenticatedUsername();
  clear_authenticated_user();
  profile_->GetPrefs()->ClearPref(prefs::kGoogleServicesAccountId);
  FOR_EACH_OBSERVER(SigninManagerBase::Observer,
                    observer_list_,
                    GoogleSignedOut(account_id, username));
}

// static
KeyedService* FakeSigninManager::Build(content::BrowserContext* context) {
  return new FakeSigninManager(Profile::FromBrowserContext(context));
}

}  // namespace gcm
