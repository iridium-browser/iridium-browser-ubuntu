// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CLASSIFIER_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CLASSIFIER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

template <typename T>
struct DefaultSingletonTraits;
class AutocompleteClassifier;

namespace ios {

class ChromeBrowserState;

// Singleton that owns all AutocompleteClassifiers and associates them with
// ios::ChromeBrowserState.
class AutocompleteClassifierFactory : public BrowserStateKeyedServiceFactory {
 public:
  static AutocompleteClassifier* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);
  static AutocompleteClassifierFactory* GetInstance();

  // Returns the default factory used to build AutocompleteClassifier. Can be
  // registered with SetTestingFactory to use the AutocompleteClassifier
  // instance during testing.
  static TestingFactoryFunction GetDefaultFactory();

 private:
  friend struct DefaultSingletonTraits<AutocompleteClassifierFactory>;

  AutocompleteClassifierFactory();
  ~AutocompleteClassifierFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  scoped_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteClassifierFactory);
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CLASSIFIER_FACTORY_H_
