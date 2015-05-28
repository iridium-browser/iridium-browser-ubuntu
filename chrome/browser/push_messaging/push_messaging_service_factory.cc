// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_service_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/push_messaging/push_messaging_service_impl.h"
#include "chrome/browser/services/gcm/fake_gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
PushMessagingServiceImpl* PushMessagingServiceFactory::GetForProfile(
    content::BrowserContext* profile) {
  // The Push API is not currently supported in incognito mode.
  // See https://crbug.com/401439.
  if (profile->IsOffTheRecord())
    return NULL;

  return static_cast<PushMessagingServiceImpl*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PushMessagingServiceFactory* PushMessagingServiceFactory::GetInstance() {
  return Singleton<PushMessagingServiceFactory>::get();
}

PushMessagingServiceFactory::PushMessagingServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PushMessagingProfileService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(gcm::GCMProfileServiceFactory::GetInstance());
}

PushMessagingServiceFactory::~PushMessagingServiceFactory() {
}

KeyedService* PushMessagingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  CHECK(!profile->IsOffTheRecord());
  return new PushMessagingServiceImpl(profile);
}

content::BrowserContext* PushMessagingServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
