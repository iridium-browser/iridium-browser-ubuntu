// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#import <WebKit/WebKit.h>

#import "base/ios/weak_nsobject.h"
#include "ios/web/public/test/test_browser_state.h"
#include "ios/web/public/test/web_test_util.h"
#include "ios/web/public/web_client.h"
#import "ios/web/web_state/js/page_script_util.h"
#include "testing/gtest_mac.h"
#include "testing/gtest/include/gtest/gtest.h"
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

// Tests that internal configuration object can not be changed by clients.
TEST_F(WKWebViewConfigurationProviderTest, ConfigurationProtection) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  WKWebViewConfigurationProvider& provider = GetProvider(&browser_state_);
  WKWebViewConfiguration* config = provider.GetWebViewConfiguration();
  base::scoped_nsobject<WKProcessPool> pool([[config processPool] retain]);
  base::scoped_nsobject<WKPreferences> prefs([[config preferences] retain]);
  base::scoped_nsobject<WKUserContentController> userContentController(
      [[config userContentController] retain]);

  // nil-out the properties of returned configuration object.
  config.processPool = nil;
  config.preferences = nil;
  config.userContentController = nil;

  // Make sure that the properties of internal configuration were not changed.
  EXPECT_TRUE(provider.GetWebViewConfiguration().processPool);
  EXPECT_EQ(pool.get(), provider.GetWebViewConfiguration().processPool);
  EXPECT_TRUE(provider.GetWebViewConfiguration().preferences);
  EXPECT_EQ(prefs.get(), provider.GetWebViewConfiguration().preferences);
  EXPECT_TRUE(provider.GetWebViewConfiguration().userContentController);
  EXPECT_EQ(userContentController.get(),
            provider.GetWebViewConfiguration().userContentController);
}

// Tests that |HasWebViewConfiguration| returns false by default.
TEST_F(WKWebViewConfigurationProviderTest, NoConfigurationByDefault) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  EXPECT_FALSE(GetProvider().HasWebViewConfiguration());
}

// Tests that |HasWebViewConfiguration| returns true after
// |GetWebViewConfiguration| call and false after |Purge| call.
TEST_F(WKWebViewConfigurationProviderTest, HasWebViewConfiguration) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  // Valid configuration after |GetWebViewConfiguration| call.
  @autoreleasepool {  // Make sure that resulting copy is deallocated.
    GetProvider().GetWebViewConfiguration();
  }
  EXPECT_TRUE(GetProvider().HasWebViewConfiguration());

  // No configuration after |Purge| call.
  GetProvider().Purge();
  EXPECT_FALSE(GetProvider().HasWebViewConfiguration());

  // Valid configuration after |GetWebViewConfiguration| call.
  GetProvider().GetWebViewConfiguration();
  EXPECT_TRUE(GetProvider().HasWebViewConfiguration());
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
