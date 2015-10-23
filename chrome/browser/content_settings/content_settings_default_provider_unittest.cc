// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/prefs/testing_pref_service.h"
#include "chrome/browser/content_settings/content_settings_mock_observer.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_default_provider.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;

class DefaultProviderTest : public testing::Test {
 public:
  DefaultProviderTest()
      : provider_(profile_.GetPrefs(), false) {
  }
  ~DefaultProviderTest() override { provider_.ShutdownOnUIThread(); }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  content_settings::DefaultProvider provider_;
};

TEST_F(DefaultProviderTest, DefaultValues) {
  // Check setting defaults.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(&provider_,
                              GURL(),
                              GURL(),
                              CONTENT_SETTINGS_TYPE_COOKIES,
                              std::string(),
                              false));
  provider_.SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_COOKIES,
      std::string(),
      new base::FundamentalValue(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(&provider_,
                              GURL(),
                              GURL(),
                              CONTENT_SETTINGS_TYPE_COOKIES,
                              std::string(),
                              false));

  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetContentSetting(&provider_,
                              GURL(),
                              GURL(),
                              CONTENT_SETTINGS_TYPE_GEOLOCATION,
                              std::string(),
                              false));
  provider_.SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(),
      new base::FundamentalValue(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(&provider_,
                              GURL(),
                              GURL(),
                              CONTENT_SETTINGS_TYPE_GEOLOCATION,
                              std::string(),
                              false));

  scoped_ptr<base::Value> value(
      GetContentSettingValue(&provider_,
                             GURL("http://example.com/"),
                             GURL("http://example.com/"),
                             CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
                             std::string(),
                             false));
  EXPECT_FALSE(value.get());
}

TEST_F(DefaultProviderTest, IgnoreNonDefaultSettings) {
  GURL primary_url("http://www.google.com");
  GURL secondary_url("http://www.google.com");

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(&provider_,
                              primary_url,
                              secondary_url,
                              CONTENT_SETTINGS_TYPE_COOKIES,
                              std::string(),
                              false));
  scoped_ptr<base::Value> value(
      new base::FundamentalValue(CONTENT_SETTING_BLOCK));
  bool owned = provider_.SetWebsiteSetting(
      ContentSettingsPattern::FromURL(primary_url),
      ContentSettingsPattern::FromURL(secondary_url),
      CONTENT_SETTINGS_TYPE_COOKIES,
      std::string(),
      value.get());
  EXPECT_FALSE(owned);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(&provider_,
                              primary_url,
                              secondary_url,
                              CONTENT_SETTINGS_TYPE_COOKIES,
                              std::string(),
                              false));
}

TEST_F(DefaultProviderTest, Observer) {
  content_settings::MockObserver mock_observer;
  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(
                  _, _, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  provider_.AddObserver(&mock_observer);
  provider_.SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_IMAGES,
      std::string(),
      new base::FundamentalValue(CONTENT_SETTING_BLOCK));

  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(
                  _, _, CONTENT_SETTINGS_TYPE_GEOLOCATION, ""));
  provider_.SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(),
      new base::FundamentalValue(CONTENT_SETTING_BLOCK));
}


TEST_F(DefaultProviderTest, ObservePref) {
  PrefService* prefs = profile_.GetPrefs();

  provider_.SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_COOKIES,
      std::string(),
      new base::FundamentalValue(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(&provider_,
                              GURL(),
                              GURL(),
                              CONTENT_SETTINGS_TYPE_COOKIES,
                              std::string(),
                              false));
  const content_settings::WebsiteSettingsInfo* info =
      content_settings::WebsiteSettingsRegistry::GetInstance()->Get(
          CONTENT_SETTINGS_TYPE_COOKIES);
  // Clearing the backing pref should also clear the internal cache.
  prefs->ClearPref(info->default_value_pref_name());
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(&provider_,
                              GURL(),
                              GURL(),
                              CONTENT_SETTINGS_TYPE_COOKIES,
                              std::string(),
                              false));
  // Reseting the pref to its previous value should update the cache.
  prefs->SetInteger(info->default_value_pref_name(), CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(&provider_,
                              GURL(),
                              GURL(),
                              CONTENT_SETTINGS_TYPE_COOKIES,
                              std::string(),
                              false));
}

TEST_F(DefaultProviderTest, OffTheRecord) {
  content_settings::DefaultProvider otr_provider(profile_.GetPrefs(), true);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(&provider_,
                              GURL(),
                              GURL(),
                              CONTENT_SETTINGS_TYPE_COOKIES,
                              std::string(),
                              false));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(&otr_provider,
                              GURL(),
                              GURL(),
                              CONTENT_SETTINGS_TYPE_COOKIES,
                              std::string(),
                              true));

  // Changing content settings on the main provider should also affect the
  // incognito map.
  provider_.SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_COOKIES,
      std::string(),
      new base::FundamentalValue(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(&provider_,
                              GURL(),
                              GURL(),
                              CONTENT_SETTINGS_TYPE_COOKIES,
                              std::string(),
                              false));

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(&otr_provider,
                              GURL(),
                              GURL(),
                              CONTENT_SETTINGS_TYPE_COOKIES,
                              std::string(),
                              true));

  // Changing content settings on the incognito provider should be ignored.
  scoped_ptr<base::Value> value(
      new base::FundamentalValue(CONTENT_SETTING_ALLOW));
  bool owned = otr_provider.SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_COOKIES,
      std::string(),
      value.get());
  EXPECT_FALSE(owned);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(&provider_,
                              GURL(),
                              GURL(),
                              CONTENT_SETTINGS_TYPE_COOKIES,
                              std::string(),
                              false));

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(&otr_provider,
                              GURL(),
                              GURL(),
                              CONTENT_SETTINGS_TYPE_COOKIES,
                              std::string(),
                              true));
  otr_provider.ShutdownOnUIThread();
}
