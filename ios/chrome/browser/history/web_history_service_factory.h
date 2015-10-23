// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_WEB_HISTORY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_HISTORY_WEB_HISTORY_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

template <typename T>
struct DefaultSingletonTraits;

namespace history {
class WebHistoryService;
}

namespace ios {

class ChromeBrowserState;

// Singleton that owns all WebHistoryServices and associates them with
// ios::ChromeBrowserState.
class WebHistoryServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static history::WebHistoryService* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);
  static WebHistoryServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<WebHistoryServiceFactory>;

  WebHistoryServiceFactory();
  ~WebHistoryServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  scoped_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(WebHistoryServiceFactory);
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_HISTORY_WEB_HISTORY_SERVICE_FACTORY_H_
