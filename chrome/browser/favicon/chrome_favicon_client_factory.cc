// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/chrome_favicon_client_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/favicon/chrome_favicon_client.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

ChromeFaviconClientFactory::ChromeFaviconClientFactory()
    : BrowserContextKeyedServiceFactory(
          "ChromeFaviconClient",
          BrowserContextDependencyManager::GetInstance()) {
}

ChromeFaviconClientFactory::~ChromeFaviconClientFactory() {
}

// static
favicon::FaviconClient* ChromeFaviconClientFactory::GetForProfile(
    Profile* profile) {
  return static_cast<favicon::FaviconClient*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ChromeFaviconClientFactory* ChromeFaviconClientFactory::GetInstance() {
  return Singleton<ChromeFaviconClientFactory>::get();
}

KeyedService* ChromeFaviconClientFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ChromeFaviconClient(Profile::FromBrowserContext(context));
}

content::BrowserContext* ChromeFaviconClientFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}
