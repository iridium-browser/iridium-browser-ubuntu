// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

template <typename T>
struct DefaultSingletonTraits;
class TemplateURLService;

namespace ios {

class ChromeBrowserState;

// Singleton that owns all TemplateURLServices and associates them with
// ios::ChromeBrowserState.
class TemplateURLServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static TemplateURLService* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);
  static TemplateURLServiceFactory* GetInstance();

  // Returns the default factory used to build FaviconService. Can be registered
  // with SetTestingFactory to use the FaviconService instance during testing.
  static TestingFactoryFunction GetDefaultFactory();

 private:
  friend struct DefaultSingletonTraits<TemplateURLServiceFactory>;

  TemplateURLServiceFactory();
  ~TemplateURLServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  scoped_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLServiceFactory);
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_FACTORY_H_
