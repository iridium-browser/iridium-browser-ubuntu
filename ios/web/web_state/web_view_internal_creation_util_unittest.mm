// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_view_internal_creation_util.h"

#import <CoreGraphics/CoreGraphics.h>
#import <WebKit/WebKit.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#include "ios/web/public/test/test_browser_state.h"
#import "ios/web/public/test/test_web_client.h"
#include "ios/web/public/test/web_test.h"
#import "ios/web/public/web_view_creation_util.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest_mac.h"

namespace {

// An arbitrary sized frame for testing web view creation.
const CGRect kTestFrame = CGRectMake(5.0f, 10.0f, 15.0f, 20.0f);

// A WebClient that stubs PreWebViewCreation call for testing purposes.
class CreationUtilsWebClient : public web::TestWebClient {
 public:
  MOCK_CONST_METHOD0(PreWebViewCreation, void());
};
}  // namespace

namespace web {

// Test fixture for testing web view creation.
class WebViewCreationUtilsTest : public WebTest {
 public:
  WebViewCreationUtilsTest()
      : web_client_(base::WrapUnique(new CreationUtilsWebClient)) {}

 protected:
  CreationUtilsWebClient* creation_utils_web_client() {
    return static_cast<CreationUtilsWebClient*>(web_client_.Get());
  }

 private:
  // WebClient that stubs PreWebViewCreation.
  web::ScopedTestingWebClient web_client_;
};

// Tests web::CreateWKWebView function that it correctly returns a WKWebView
// with the correct frame, WKProcessPool and calls WebClient::PreWebViewCreation
// method.
TEST_F(WebViewCreationUtilsTest, WKWebViewCreationWithBrowserState) {
  EXPECT_CALL(*creation_utils_web_client(), PreWebViewCreation()).Times(1);

  base::scoped_nsobject<WKWebView> web_view(
      CreateWKWebView(kTestFrame, GetBrowserState()));

  EXPECT_TRUE([web_view isKindOfClass:[WKWebView class]]);
  EXPECT_TRUE(CGRectEqualToRect(kTestFrame, [web_view frame]));

  // Make sure that web view's configuration shares the same process pool with
  // browser state's configuration. Otherwise cookie will not be immediately
  // shared between different web views.
  WKWebViewConfigurationProvider& config_provider =
      WKWebViewConfigurationProvider::FromBrowserState(GetBrowserState());
  EXPECT_EQ(config_provider.GetWebViewConfiguration().processPool,
            [[web_view configuration] processPool]);
}

// Tests that web::CreateWKWebView always returns a web view with the same
// processPool.
TEST_F(WebViewCreationUtilsTest, WKWebViewsShareProcessPool) {
  base::scoped_nsobject<WKWebView> web_view(
      CreateWKWebView(kTestFrame, GetBrowserState()));
  ASSERT_TRUE(web_view);
  base::scoped_nsobject<WKWebView> web_view2(
      CreateWKWebView(kTestFrame, GetBrowserState()));
  ASSERT_TRUE(web_view2);

  // Make sure that web views share the same non-nil process pool. Otherwise
  // cookie will not be immediately shared between different web views.
  EXPECT_TRUE([[web_view configuration] processPool]);
  EXPECT_EQ([[web_view configuration] processPool],
            [[web_view2 configuration] processPool]);
}

}  // namespace web
