// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#import <WebKit/WebKit.h>

#include "base/ios/ios_util.h"
#import "base/ios/weak_nsobject.h"
#include "ios/web/public/test/test_browser_state.h"
#include "ios/web/public/test/web_test_util.h"
#include "ios/web/public/web_client.h"
#import "ios/web/web_state/js/page_script_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace web {
namespace {

class WKWebViewConfigurationProviderTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    SetWebClient(&web_client_);
  }
  void TearDown() override {
    SetWebClient(nullptr);
    PlatformTest::TearDown();
  }
  // Returns WKWebViewConfigurationProvider associated with |browser_state_|.
  WKWebViewConfigurationProvider& GetProvider() {
    return GetProvider(&browser_state_);
  }
  // Returns WKWebViewConfigurationProvider for given |browser_state|.
  WKWebViewConfigurationProvider& GetProvider(
      BrowserState* browser_state) const {
    return WKWebViewConfigurationProvider::FromBrowserState(browser_state);
  }
  // BrowserState required for WKWebViewConfigurationProvider creation.
  TestBrowserState browser_state_;

 private:
  // WebClient required for getting early page script.
  WebClient web_client_;
};

// Tests that each WKWebViewConfigurationProvider has own, non-nil
// configuration and configurations returned by the same provider will always
// have the same process pool.
TEST_F(WKWebViewConfigurationProviderTest, ConfigurationOwnerhip) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  // Configuration is not nil.
  WKWebViewConfigurationProvider& provider = GetProvider(&browser_state_);
  ASSERT_TRUE(provider.GetWebViewConfiguration());

  // Same non-nil WKProcessPool for the same provider.
  ASSERT_TRUE(provider.GetWebViewConfiguration().processPool);
  EXPECT_EQ(provider.GetWebViewConfiguration().processPool,
            provider.GetWebViewConfiguration().processPool);

  // Different WKProcessPools for different providers.
  TestBrowserState other_browser_state;
  WKWebViewConfigurationProvider& other_provider =
      GetProvider(&other_browser_state);
  EXPECT_NE(provider.GetWebViewConfiguration().processPool,
            other_provider.GetWebViewConfiguration().processPool);
}

// TODO(eugenebut): Cleanup this macro, once all bots switched to iOS9 SDK
// (crbug.com/523365).
#if defined(__IPHONE_9_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_9_0

// Tests Non-OffTheRecord configuration.
TEST_F(WKWebViewConfigurationProviderTest, NoneOffTheRecordConfiguration) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  if (!base::ios::IsRunningOnIOS9OrLater())
    return;

  browser_state_.SetOffTheRecord(false);
  WKWebViewConfigurationProvider& provider = GetProvider(&browser_state_);
  EXPECT_TRUE(provider.GetWebViewConfiguration().websiteDataStore.persistent);
}

// Tests OffTheRecord configuration.
TEST_F(WKWebViewConfigurationProviderTest, OffTheRecordConfiguration) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  if (!base::ios::IsRunningOnIOS9OrLater())
    return;

  browser_state_.SetOffTheRecord(true);
  WKWebViewConfigurationProvider& provider = GetProvider(&browser_state_);
  WKWebViewConfiguration* config = provider.GetWebViewConfiguration();
  ASSERT_TRUE(config);
  EXPECT_FALSE(config.websiteDataStore.persistent);
}

#endif  // defined(__IPHONE_9_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >=
        // __IPHONE_9_0

// Tests that internal configuration object can not be changed by clients.
TEST_F(WKWebViewConfigurationProviderTest, ConfigurationProtection) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  WKWebViewConfigurationProvider& provider = GetProvider(&browser_state_);
  WKWebViewConfiguration* config = provider.GetWebViewConfiguration();
  base::scoped_nsobject<WKProcessPool> pool([[config processPool] retain]);
  base::scoped_nsobject<WKPreferences> prefs([[config preferences] retain]);
  base::scoped_nsobject<WKUserContentController> userContentController(
      [[config userContentController] retain]);

  // Change the properties of returned configuration object.
  TestBrowserState other_browser_state;
  WKWebViewConfiguration* other_wk_web_view_configuration =
      GetProvider(&other_browser_state).GetWebViewConfiguration();
  ASSERT_TRUE(other_wk_web_view_configuration);
  config.processPool = other_wk_web_view_configuration.processPool;
  config.preferences = other_wk_web_view_configuration.preferences;
  config.userContentController =
      other_wk_web_view_configuration.userContentController;

  // Make sure that the properties of internal configuration were not changed.
  EXPECT_TRUE(provider.GetWebViewConfiguration().processPool);
  EXPECT_EQ(pool.get(), provider.GetWebViewConfiguration().processPool);
  EXPECT_TRUE(provider.GetWebViewConfiguration().preferences);
  EXPECT_EQ(prefs.get(), provider.GetWebViewConfiguration().preferences);
  EXPECT_TRUE(provider.GetWebViewConfiguration().userContentController);
  EXPECT_EQ(userContentController.get(),
            provider.GetWebViewConfiguration().userContentController);
}

// Tests that configuration is deallocated after |Purge| call.
TEST_F(WKWebViewConfigurationProviderTest, Purge) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  base::WeakNSObject<id> config;
  @autoreleasepool {  // Make sure that resulting copy is deallocated.
    config.reset(GetProvider().GetWebViewConfiguration());
    ASSERT_TRUE(config);
  }

  // No configuration after |Purge| call.
  GetProvider().Purge();
  EXPECT_FALSE(config);
}

// Tests that configuration's userContentController has only one script with the
// same content as web::GetEarlyPageScript(WK_WEB_VIEW_TYPE) returns.
TEST_F(WKWebViewConfigurationProviderTest, UserScript) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  WKWebViewConfiguration* config = GetProvider().GetWebViewConfiguration();
  NSArray* scripts = config.userContentController.userScripts;
  EXPECT_EQ(1U, scripts.count);
  NSString* early_script = GetEarlyPageScript(WK_WEB_VIEW_TYPE);
  // |earlyScript| is a substring of |userScripts|. The latter wraps the
  // former with "if (!injected)" check to avoid double injections.
  EXPECT_LT(0U, [[scripts[0] source] rangeOfString:early_script].length);
}

}  // namespace
}  // namespace web
