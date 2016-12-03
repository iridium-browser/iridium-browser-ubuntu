// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_FACTORY_H_
#define CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace predictors {

class ResourcePrefetchPredictor;

class ResourcePrefetchPredictorFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static ResourcePrefetchPredictor* GetForProfile(
      content::BrowserContext* context);
  static ResourcePrefetchPredictorFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ResourcePrefetchPredictorFactory>;

  ResourcePrefetchPredictorFactory();
  ~ResourcePrefetchPredictorFactory() override;

  // RefcountedBrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(ResourcePrefetchPredictorFactory);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_FACTORY_H_
