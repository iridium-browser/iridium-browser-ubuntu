// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/country_combobox_model.h"

#include <memory>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui_component.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/localization.h"

namespace autofill {

class CountryComboboxModelTest : public testing::Test {
 public:
  CountryComboboxModelTest() {
    manager_.Init(
        NULL, profile_.GetPrefs(),
        AccountTrackerServiceFactory::GetForProfile(&profile_),
        SigninManagerFactory::GetForProfile(&profile_), false);
    manager_.set_timezone_country_code("KR");
    model_.reset(new CountryComboboxModel());
    model_->SetCountries(manager_, base::Callback<bool(const std::string&)>());
  }

  TestPersonalDataManager* manager() { return &manager_; }
  CountryComboboxModel* model() { return model_.get(); }

 private:
  // NB: order is important here - |profile_| must go down after |manager_|.
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  TestPersonalDataManager manager_;
  std::unique_ptr<CountryComboboxModel> model_;
};

TEST_F(CountryComboboxModelTest, DefaultCountryCode) {
  std::string default_country = model()->GetDefaultCountryCode();
  EXPECT_EQ(manager()->GetDefaultCountryCodeForNewAddress(), default_country);

  AutofillCountry country(default_country,
                          g_browser_process->GetApplicationLocale());
  EXPECT_EQ(country.name(), model()->GetItemAt(0));
}

TEST_F(CountryComboboxModelTest, AllCountriesHaveComponents) {
  ::i18n::addressinput::Localization localization;
  std::string unused;
  for (int i = 0; i < model()->GetItemCount(); ++i) {
    if (model()->IsItemSeparatorAt(i))
      continue;

    std::string country_code = model()->countries()[i]->country_code();
    std::vector< ::i18n::addressinput::AddressUiComponent> components =
        ::i18n::addressinput::BuildComponents(
            country_code, localization, std::string(), &unused);
    EXPECT_FALSE(components.empty()) << " for country " << country_code;
  }
}

}  // namespace autofill
