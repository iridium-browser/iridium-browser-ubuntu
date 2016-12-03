// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace sync_driver {
class SyncService;
}

class Profile;
class ProfileSyncService;

class ProfileSyncServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ProfileSyncService* GetForProfile(Profile* profile);
  static bool HasProfileSyncService(Profile* profile);

  // Convenience method that returns the ProfileSyncService as a
  // sync_driver::SyncService.
  static sync_driver::SyncService* GetSyncServiceForBrowserContext(
      content::BrowserContext* context);

  static ProfileSyncServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ProfileSyncServiceFactory>;

  ProfileSyncServiceFactory();
  ~ProfileSyncServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceFactory);
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_FACTORY_H_
