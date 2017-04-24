// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_RECOMMENDATION_RESTORER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_RECOMMENDATION_RESTORER_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace policy {

class RecommendationRestorer;

class RecommendationRestorerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static RecommendationRestorerFactory* GetInstance();

  static RecommendationRestorer* GetForProfile(Profile* profile);

 protected:
  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;

 private:
  friend struct base::DefaultSingletonTraits<RecommendationRestorerFactory>;

  RecommendationRestorerFactory();
  ~RecommendationRestorerFactory() override;

  DISALLOW_COPY_AND_ASSIGN(RecommendationRestorerFactory);
};

}  //  namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_RECOMMENDATION_RESTORER_FACTORY_H_
