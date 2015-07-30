// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/js/page_script_util.h"

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/js_test_util.h"
#include "ios/web/public/test/test_browser_state.h"
#import "ios/web/public/test/test_web_client.h"
#include "ios/web/public/test/web_test_util.h"
#import "ios/web/web_state/web_view_creation_utils.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace web {
namespace {

class PageScriptUtilTest : public PlatformTest {
 protected:
  void SetUp() override { SetWebClient(&web_client_); }

  void TearDown() override { SetWebClient(nullptr); }

  // WebClient and BrowserState required for web view creation.
  TestWebClient web_client_;
  TestBrowserState browser_state_;
};

// Tests that UIWebView early page script is a valid script that injects global
// __gCrWeb object.
TEST_F(PageScriptUtilTest, UIWebViewEarlyPageScript) {
  base::scoped_nsobject<UIWebView> web_view([[UIWebView alloc] init]);
  EvaluateJavaScriptAsString(web_view, GetEarlyPageScript(UI_WEB_VIEW_TYPE));
  EXPECT_NSEQ(@"object",
              EvaluateJavaScriptAsString(web_view, @"typeof __gCrWeb"));
}

// Tests that WKWebView early page script is a valid script that injects global
// __gCrWeb object.
TEST_F(PageScriptUtilTest, WKWebViewEarlyPageScript) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  base::scoped_nsobject<WKWebView> web_view(
      CreateWKWebView(CGRectZero, &browser_state_));
  EvaluateJavaScript(web_view, GetEarlyPageScript(WK_WEB_VIEW_TYPE));
  EXPECT_NSEQ(@"object", EvaluateJavaScript(web_view, @"typeof __gCrWeb"));
}

// Tests that embedder's UIWebView script is included into early script.
TEST_F(PageScriptUtilTest, UIEmbedderScript) {
  web_client_.SetEarlyPageScript(@"__gCrEmbedder = {};", UI_WEB_VIEW_TYPE);
  base::scoped_nsobject<UIWebView> web_view([[UIWebView alloc] init]);
  EvaluateJavaScriptAsString(web_view, GetEarlyPageScript(UI_WEB_VIEW_TYPE));
  EXPECT_NSEQ(@"object",
              EvaluateJavaScriptAsString(web_view, @"typeof __gCrEmbedder"));
}

// Tests that embedder's WKWebView script is included into early script.
TEST_F(PageScriptUtilTest, WKEmbedderScript) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  web_client_.SetEarlyPageScript(@"__gCrEmbedder = {};", WK_WEB_VIEW_TYPE);
  base::scoped_nsobject<WKWebView> web_view(
      CreateWKWebView(CGRectZero, &browser_state_));
  EvaluateJavaScript(web_view, GetEarlyPageScript(WK_WEB_VIEW_TYPE));
  EXPECT_NSEQ(@"object", EvaluateJavaScript(web_view, @"typeof __gCrEmbedder"));
}

}  // namespace
}  // namespace web
