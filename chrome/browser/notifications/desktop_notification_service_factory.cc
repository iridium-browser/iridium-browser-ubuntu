// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/desktop_notification_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

// static
DesktopNotificationService* DesktopNotificationServiceFactory::GetForProfile(
    Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return static_cast<DesktopNotificationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
DesktopNotificationServiceFactory* DesktopNotificationServiceFactory::
    GetInstance() {
  return Singleton<DesktopNotificationServiceFactory>::get();
}

DesktopNotificationServiceFactory::DesktopNotificationServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "DesktopNotificationService",
        BrowserContextDependencyManager::GetInstance()) {
}

DesktopNotificationServiceFactory::~DesktopNotificationServiceFactory() {
}

KeyedService* DesktopNotificationServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DesktopNotificationService* service =
      new DesktopNotificationService(static_cast<Profile*>(context));
  return service;
}

content::BrowserContext*
DesktopNotificationServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
