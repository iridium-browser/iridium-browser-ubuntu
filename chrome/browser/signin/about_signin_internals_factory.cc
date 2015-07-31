// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/about_signin_internals_factory.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "components/signin/core/browser/signin_internals_util.h"
#include "components/signin/core/browser/signin_manager.h"
#include "google_apis/gaia/gaia_constants.h"

using namespace signin_internals_util;

AboutSigninInternalsFactory::AboutSigninInternalsFactory()
    : BrowserContextKeyedServiceFactory(
        "AboutSigninInternals",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(AccountTrackerServiceFactory::GetInstance());
  DependsOn(ChromeSigninClientFactory::GetInstance());
  DependsOn(GaiaCookieManagerServiceFactory::GetInstance());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  DependsOn(SigninErrorControllerFactory::GetInstance());
  DependsOn(SigninManagerFactory::GetInstance());
}

AboutSigninInternalsFactory::~AboutSigninInternalsFactory() {}

// static
AboutSigninInternals* AboutSigninInternalsFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AboutSigninInternals*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AboutSigninInternalsFactory* AboutSigninInternalsFactory::GetInstance() {
  return Singleton<AboutSigninInternalsFactory>::get();
}

void AboutSigninInternalsFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  // SigninManager information for about:signin-internals.

  // TODO(rogerta): leaving untimed fields here for now because legacy
  // profiles still have these prefs.  In three or four version from M43
  // we can probably remove them.
  for (int i = UNTIMED_FIELDS_BEGIN; i < UNTIMED_FIELDS_END; ++i) {
    const std::string pref_path = SigninStatusFieldToString(
        static_cast<UntimedSigninStatusField>(i));
    user_prefs->RegisterStringPref(pref_path.c_str(), std::string());
  }

  for (int i = TIMED_FIELDS_BEGIN; i < TIMED_FIELDS_END; ++i) {
    const std::string value = SigninStatusFieldToString(
        static_cast<TimedSigninStatusField>(i)) + ".value";
    const std::string time = SigninStatusFieldToString(
        static_cast<TimedSigninStatusField>(i)) + ".time";
    user_prefs->RegisterStringPref(value.c_str(), std::string());
    user_prefs->RegisterStringPref(time.c_str(), std::string());
  }
}

KeyedService* AboutSigninInternalsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  AboutSigninInternals* service = new AboutSigninInternals(
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      AccountTrackerServiceFactory::GetForProfile(profile),
      SigninManagerFactory::GetForProfile(profile),
      SigninErrorControllerFactory::GetForProfile(profile),
      GaiaCookieManagerServiceFactory::GetForProfile(profile));
  service->Initialize(ChromeSigninClientFactory::GetForProfile(profile));
  return service;
}
