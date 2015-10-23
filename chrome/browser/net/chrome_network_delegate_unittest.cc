// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_network_delegate.h"

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_member.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/net/safe_search_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/event_router_forwarder.h"
#endif

TEST(ChromeNetworkDelegateTest, DisableFirstPartyOnlyCookiesIffFlagDisabled) {
  BooleanPrefMember pref_member_;
  scoped_ptr<ChromeNetworkDelegate> delegate;

#if defined(ENABLE_EXTENSIONS)
  scoped_refptr<extensions::EventRouterForwarder> forwarder =
      new extensions::EventRouterForwarder();
  delegate.reset(new ChromeNetworkDelegate(forwarder.get(), &pref_member_));
#else
  delegate.reset(new ChromeNetworkDelegate(nullptr, &pref_member_));
#endif
  EXPECT_FALSE(delegate->FirstPartyOnlyCookieExperimentEnabled());
}

TEST(ChromeNetworkDelegateTest, EnableFirstPartyOnlyCookiesIffFlagEnabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalWebPlatformFeatures);
  BooleanPrefMember pref_member_;
  scoped_ptr<ChromeNetworkDelegate> delegate;

#if defined(ENABLE_EXTENSIONS)
  scoped_refptr<extensions::EventRouterForwarder> forwarder =
      new extensions::EventRouterForwarder();
  delegate.reset(new ChromeNetworkDelegate(forwarder.get(), &pref_member_));
#else
  delegate.reset(new ChromeNetworkDelegate(nullptr, &pref_member_));
#endif
  EXPECT_TRUE(delegate->FirstPartyOnlyCookieExperimentEnabled());
}

class ChromeNetworkDelegateSafeSearchTest : public testing::Test {
 public:
  ChromeNetworkDelegateSafeSearchTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {
#if defined(ENABLE_EXTENSIONS)
    forwarder_ = new extensions::EventRouterForwarder();
#endif
  }

  void SetUp() override {
    ChromeNetworkDelegate::InitializePrefsOnUIThread(
        &enable_referrers_,
        NULL,
        &force_google_safe_search_,
        &force_youtube_safety_mode_,
        profile_.GetTestingPrefService());
  }

 protected:
  scoped_ptr<net::NetworkDelegate> CreateNetworkDelegate() {
    scoped_ptr<ChromeNetworkDelegate> network_delegate(
        new ChromeNetworkDelegate(forwarder(), &enable_referrers_));
    network_delegate->set_force_google_safe_search(&force_google_safe_search_);
    network_delegate->set_force_youtube_safety_mode(
        &force_youtube_safety_mode_);
    return network_delegate.Pass();
  }

  void SetSafeSearch(bool google_safe_search,
                     bool youtube_safety_mode) {
    force_google_safe_search_.SetValue(google_safe_search);
    force_youtube_safety_mode_.SetValue(youtube_safety_mode);
  }

  void SetDelegate(net::NetworkDelegate* delegate) {
    network_delegate_ = delegate;
    context_.set_network_delegate(network_delegate_);
  }

  // Does a request to an arbitrary URL and verifies that the SafeSearch
  // enforcement utility functions were called/not called as expected.
  void QueryURL(bool expect_google_safe_search,
                bool expect_youtube_safety_mode) {
    safe_search_util::ClearForceGoogleSafeSearchCountForTesting();
    safe_search_util::ClearForceYouTubeSafetyModeCountForTesting();

    scoped_ptr<net::URLRequest> request(context_.CreateRequest(
        GURL("http://anyurl.com"), net::DEFAULT_PRIORITY, &delegate_));

    request->Start();
    base::MessageLoop::current()->RunUntilIdle();

    EXPECT_EQ(expect_google_safe_search ? 1 : 0,
        safe_search_util::GetForceGoogleSafeSearchCountForTesting());
    EXPECT_EQ(expect_youtube_safety_mode ? 1 : 0,
        safe_search_util::GetForceYouTubeSafetyModeCountForTesting());
  }

 private:
  extensions::EventRouterForwarder* forwarder() {
#if defined(ENABLE_EXTENSIONS)
    return forwarder_.get();
#else
    return NULL;
#endif
  }

  content::TestBrowserThreadBundle thread_bundle_;
#if defined(ENABLE_EXTENSIONS)
  scoped_refptr<extensions::EventRouterForwarder> forwarder_;
#endif
  TestingProfile profile_;
  BooleanPrefMember enable_referrers_;
  BooleanPrefMember force_google_safe_search_;
  BooleanPrefMember force_youtube_safety_mode_;
  scoped_ptr<net::URLRequest> request_;
  net::TestURLRequestContext context_;
  net::NetworkDelegate* network_delegate_;
  net::TestDelegate delegate_;
};

TEST_F(ChromeNetworkDelegateSafeSearchTest, SafeSearch) {
  scoped_ptr<net::NetworkDelegate> delegate(CreateNetworkDelegate());
  SetDelegate(delegate.get());

  // Loop over all combinations of the two policies.
  for (int i = 0; i < 4; i++) {
    bool google_safe_search = i % 2;
    bool youtube_safety_mode = i / 2;
    SetSafeSearch(google_safe_search, youtube_safety_mode);

    QueryURL(google_safe_search, youtube_safety_mode);
  }
}

// Privacy Mode disables Channel Id if cookies are blocked (cr223191)
class ChromeNetworkDelegatePrivacyModeTest : public testing::Test {
 public:
  ChromeNetworkDelegatePrivacyModeTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
#if defined(ENABLE_EXTENSIONS)
        forwarder_(new extensions::EventRouterForwarder()),
#endif
        cookie_settings_(CookieSettingsFactory::GetForProfile(&profile_).get()),
        kBlockedSite("http://ads.thirdparty.com"),
        kAllowedSite("http://good.allays.com"),
        kFirstPartySite("http://cool.things.com"),
        kBlockedFirstPartySite("http://no.thirdparties.com") {
  }

  void SetUp() override {
    ChromeNetworkDelegate::InitializePrefsOnUIThread(
        &enable_referrers_, NULL, NULL, NULL,
        profile_.GetTestingPrefService());
  }

 protected:
  scoped_ptr<ChromeNetworkDelegate> CreateNetworkDelegate() {
    scoped_ptr<ChromeNetworkDelegate> network_delegate(
        new ChromeNetworkDelegate(forwarder(), &enable_referrers_));
    network_delegate->set_cookie_settings(cookie_settings_);
    return network_delegate.Pass();
  }

  void SetDelegate(net::NetworkDelegate* delegate) {
    network_delegate_ = delegate;
    context_.set_network_delegate(network_delegate_);
  }

 protected:
  extensions::EventRouterForwarder* forwarder() {
#if defined(ENABLE_EXTENSIONS)
    return forwarder_.get();
#else
    return NULL;
#endif
  }

  content::TestBrowserThreadBundle thread_bundle_;
#if defined(ENABLE_EXTENSIONS)
  scoped_refptr<extensions::EventRouterForwarder> forwarder_;
#endif
  TestingProfile profile_;
  content_settings::CookieSettings* cookie_settings_;
  BooleanPrefMember enable_referrers_;
  scoped_ptr<net::URLRequest> request_;
  net::TestURLRequestContext context_;
  net::NetworkDelegate* network_delegate_;

  const GURL kBlockedSite;
  const GURL kAllowedSite;
  const GURL kEmptyFirstPartySite;
  const GURL kFirstPartySite;
  const GURL kBlockedFirstPartySite;
};

TEST_F(ChromeNetworkDelegatePrivacyModeTest, DisablePrivacyIfCookiesAllowed) {
  scoped_ptr<ChromeNetworkDelegate> delegate(CreateNetworkDelegate());
  SetDelegate(delegate.get());

  EXPECT_FALSE(network_delegate_->CanEnablePrivacyMode(kAllowedSite,
                                                       kEmptyFirstPartySite));
}


TEST_F(ChromeNetworkDelegatePrivacyModeTest, EnablePrivacyIfCookiesBlocked) {
  scoped_ptr<ChromeNetworkDelegate> delegate(CreateNetworkDelegate());
  SetDelegate(delegate.get());

  EXPECT_FALSE(network_delegate_->CanEnablePrivacyMode(kBlockedSite,
                                                       kEmptyFirstPartySite));

  cookie_settings_->SetCookieSetting(
      ContentSettingsPattern::FromURL(kBlockedSite),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTING_BLOCK);
  EXPECT_TRUE(network_delegate_->CanEnablePrivacyMode(kBlockedSite,
                                                      kEmptyFirstPartySite));
}

TEST_F(ChromeNetworkDelegatePrivacyModeTest, EnablePrivacyIfThirdPartyBlocked) {
  scoped_ptr<ChromeNetworkDelegate> delegate(CreateNetworkDelegate());
  SetDelegate(delegate.get());

  EXPECT_FALSE(network_delegate_->CanEnablePrivacyMode(kAllowedSite,
                                                       kFirstPartySite));

  profile_.GetPrefs()->SetBoolean(prefs::kBlockThirdPartyCookies, true);
  EXPECT_TRUE(network_delegate_->CanEnablePrivacyMode(kAllowedSite,
                                                      kFirstPartySite));
  profile_.GetPrefs()->SetBoolean(prefs::kBlockThirdPartyCookies, false);
  EXPECT_FALSE(network_delegate_->CanEnablePrivacyMode(kAllowedSite,
                                                       kFirstPartySite));
}

TEST_F(ChromeNetworkDelegatePrivacyModeTest,
       DisablePrivacyIfOnlyFirstPartyBlocked) {
  scoped_ptr<ChromeNetworkDelegate> delegate(CreateNetworkDelegate());
  SetDelegate(delegate.get());

  EXPECT_FALSE(network_delegate_->CanEnablePrivacyMode(kAllowedSite,
                                                       kBlockedFirstPartySite));

  cookie_settings_->SetCookieSetting(
      ContentSettingsPattern::FromURL(kBlockedFirstPartySite),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTING_BLOCK);
  // Privacy mode is disabled as kAllowedSite is still getting cookies
  EXPECT_FALSE(network_delegate_->CanEnablePrivacyMode(kAllowedSite,
                                                       kBlockedFirstPartySite));
}
