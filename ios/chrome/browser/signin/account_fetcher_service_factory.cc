// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/account_fetcher_service_factory.h"

#include "base/memory/singleton.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/signin/core/browser/account_fetcher_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "ios/chrome/browser/signin/account_tracker_service_factory.h"
#include "ios/chrome/browser/signin/oauth2_token_service_factory.h"
#include "ios/chrome/browser/signin/signin_client_factory.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/public/provider/chrome/browser/keyed_service_provider.h"

namespace ios {

AccountFetcherServiceFactory::AccountFetcherServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "AccountFetcherService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(AccountTrackerServiceFactory::GetInstance());
  DependsOn(OAuth2TokenServiceFactory::GetInstance());
  DependsOn(SigninClientFactory::GetInstance());
  DependsOn(GetKeyedServiceProvider()->GetProfileInvalidationProviderFactory());
}

AccountFetcherServiceFactory::~AccountFetcherServiceFactory() {}

// static
AccountFetcherService* AccountFetcherServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<AccountFetcherService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
AccountFetcherServiceFactory* AccountFetcherServiceFactory::GetInstance() {
  return Singleton<AccountFetcherServiceFactory>::get();
}

void AccountFetcherServiceFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  AccountFetcherService::RegisterPrefs(registry);
}

scoped_ptr<KeyedService> AccountFetcherServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  scoped_ptr<AccountFetcherService> service(new AccountFetcherService());
  invalidation::ProfileInvalidationProvider* invalidation_provider =
      ios::GetKeyedServiceProvider()
          ->GetProfileInvalidationProviderForBrowserState(browser_state);
  invalidation::InvalidationService* invalidation_service =
      invalidation_provider->GetInvalidationService();
  service->Initialize(
      SigninClientFactory::GetForBrowserState(browser_state),
      OAuth2TokenServiceFactory::GetForBrowserState(browser_state),
      ios::AccountTrackerServiceFactory::GetForBrowserState(browser_state),
      invalidation_service);
  return service.Pass();
}

}  // namespace ios
