// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_FACTORY_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

template <typename T>
struct DefaultSingletonTraits;

namespace offline_pages {

class OfflinePageModel;

// A factory to create one unique OfflinePageModel.
class OfflinePageModelFactory : public BrowserContextKeyedServiceFactory {
 public:
  static OfflinePageModelFactory* GetInstance();
  static OfflinePageModel* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend struct DefaultSingletonTraits<OfflinePageModelFactory>;

  OfflinePageModelFactory();
  ~OfflinePageModelFactory() override {}

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageModelFactory);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_FACTORY_H_
