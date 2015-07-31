// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_url_tracker_factory.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/google/chrome_google_url_tracker_client.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/google/core/browser/google_pref_names.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace {

const char kLastPromptedGoogleURL[] = "browser.last_prompted_google_url";

}  // namespace


// static
GoogleURLTracker* GoogleURLTrackerFactory::GetForProfile(Profile* profile) {
  return static_cast<GoogleURLTracker*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
GoogleURLTrackerFactory* GoogleURLTrackerFactory::GetInstance() {
  return Singleton<GoogleURLTrackerFactory>::get();
}

GoogleURLTrackerFactory::GoogleURLTrackerFactory()
    : BrowserContextKeyedServiceFactory(
        "GoogleURLTracker",
        BrowserContextDependencyManager::GetInstance()) {
}

GoogleURLTrackerFactory::~GoogleURLTrackerFactory() {
}

KeyedService* GoogleURLTrackerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // Delete this now-unused pref.
  // At some point in the future, this code can be removed entirely.
  static_cast<Profile*>(context)->GetOriginalProfile()->GetPrefs()->ClearPref(
      kLastPromptedGoogleURL);

  scoped_ptr<GoogleURLTrackerClient> client(
      new ChromeGoogleURLTrackerClient(Profile::FromBrowserContext(context)));
  return new GoogleURLTracker(client.Pass(), GoogleURLTracker::NORMAL_MODE);
}

void GoogleURLTrackerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterStringPref(prefs::kLastKnownGoogleURL,
                                 GoogleURLTracker::kDefaultGoogleHomepage);
  user_prefs->RegisterStringPref(kLastPromptedGoogleURL, std::string());
}

content::BrowserContext* GoogleURLTrackerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool GoogleURLTrackerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool GoogleURLTrackerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
