// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"

#include "base/memory/singleton.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/signin/account_tracker_service_factory.h"
#include "ios/chrome/browser/web_data_service_factory.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"

// static
autofill::PersonalDataManager* PersonalDataManagerFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<autofill::PersonalDataManager*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
PersonalDataManagerFactory* PersonalDataManagerFactory::GetInstance() {
  return Singleton<PersonalDataManagerFactory>::get();
}

PersonalDataManagerFactory::PersonalDataManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "PersonalDataManager",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::AccountTrackerServiceFactory::GetInstance());
  DependsOn(ios::WebDataServiceFactory::GetInstance());
}

PersonalDataManagerFactory::~PersonalDataManagerFactory() {}

scoped_ptr<KeyedService> PersonalDataManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  scoped_ptr<autofill::PersonalDataManager> service(
      new autofill::PersonalDataManager(
          GetApplicationContext()->GetApplicationLocale()));
  service->Init(ios::WebDataServiceFactory::GetAutofillWebDataForBrowserState(
                    chrome_browser_state, ServiceAccessType::EXPLICIT_ACCESS),
                chrome_browser_state->GetPrefs(),
                ios::AccountTrackerServiceFactory::GetForBrowserState(
                    chrome_browser_state),
                chrome_browser_state->IsOffTheRecord());
  return service.Pass();
}
