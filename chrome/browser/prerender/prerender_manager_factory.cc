// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_manager_factory.h"

#include "base/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

#if defined(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#endif

namespace prerender {

// static
PrerenderManager* PrerenderManagerFactory::GetForProfile(
    Profile* profile) {
  TRACE_EVENT0("browser", "PrerenderManagerFactory::GetForProfile")
  if (!PrerenderManager::IsPrerenderingPossible())
    return NULL;
  return static_cast<PrerenderManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PrerenderManagerFactory* PrerenderManagerFactory::GetInstance() {
  return Singleton<PrerenderManagerFactory>::get();
}

PrerenderManagerFactory::PrerenderManagerFactory()
    : BrowserContextKeyedServiceFactory(
        "PrerenderManager",
        BrowserContextDependencyManager::GetInstance()) {
#if defined(ENABLE_EXTENSIONS)
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
#endif
  // PrerenderLocalPredictor observers the history visit DB.
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(predictors::PredictorDatabaseFactory::GetInstance());
  DependsOn(ProfileSyncServiceFactory::GetInstance());
}

PrerenderManagerFactory::~PrerenderManagerFactory() {
}

KeyedService* PrerenderManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (base::SysInfo::IsLowEndDevice())
    return NULL;

  return new PrerenderManager(profile);
}

content::BrowserContext* PrerenderManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace prerender
