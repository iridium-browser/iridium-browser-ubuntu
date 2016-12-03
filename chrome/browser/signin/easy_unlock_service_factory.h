// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_FACTORY_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace base {
template <typename T> struct DefaultSingletonTraits;
}

class EasyUnlockService;
class Profile;

// Singleton factory that builds and owns all EasyUnlockService.
class EasyUnlockServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static EasyUnlockServiceFactory* GetInstance();

  static EasyUnlockService* GetForBrowserContext(
      content::BrowserContext* browser_context);

  void set_app_path_for_testing(const base::FilePath& app_path) {
    app_path_for_testing_ = app_path;
  }

 private:
  friend struct base::DefaultSingletonTraits<EasyUnlockServiceFactory>;

  EasyUnlockServiceFactory();
  ~EasyUnlockServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

  base::FilePath app_path_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockServiceFactory);
};

#endif  // CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_FACTORY_H_
