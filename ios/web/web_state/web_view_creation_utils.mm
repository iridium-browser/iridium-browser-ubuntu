// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_view_creation_utils.h"

#import <objc/runtime.h>
#import <WebKit/WebKit.h>

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#import "ios/web/alloc_with_zone_interceptor.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/web_client.h"
#include "ios/web/ui_web_view_util.h"
#import "ios/web/weak_nsobject_counter.h"
#import "ios/web/web_state/ui/crw_static_file_web_view.h"
#import "ios/web/web_state/ui/crw_ui_simple_web_view_controller.h"
#import "ios/web/web_state/ui/crw_wk_simple_web_view_controller.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#if !defined(NDEBUG)
#import "ios/web/web_state/ui/crw_debug_web_view.h"
#endif

#if !defined(NDEBUG)

namespace {
// Returns the counter of all the active WKWebViews.
web::WeakNSObjectCounter& GetActiveWKWebViewCounter() {
  static web::WeakNSObjectCounter active_wk_web_view_counter;
  return active_wk_web_view_counter;
}
// Decides if WKWebView can be created.
BOOL gAllowWKWebViewCreation = NO;
}  // namespace

@interface WKWebView(CRWAdditions)
@end

@implementation WKWebView(CRWAdditions)

+ (void)load {
  id (^allocator)(Class klass, NSZone* zone) = ^id(Class klass, NSZone* zone) {
    if (gAllowWKWebViewCreation) {
      return NSAllocateObject(klass, 0, zone);
    }
    // You have hit this because you are trying to create a WKWebView directly.
    // Please use one of the web::CreateWKWKWebView methods that vend a
    // WKWebView instead.
    NOTREACHED();
    return nil;
  };
  web::AddAllocWithZoneMethod([WKWebView class], allocator);
}

@end

#endif  // !defined(NDEBUG)

namespace {
// Returns a new WKWebView for displaying regular web content.
// Note: Callers are responsible for releasing the returned WKWebView.
WKWebView* CreateWKWebViewWithConfiguration(
    CGRect frame,
    WKWebViewConfiguration* configuration) {
  DCHECK(configuration);
  DCHECK(web::GetWebClient());
  web::GetWebClient()->PreWebViewCreation();
#if !defined(NDEBUG)
  gAllowWKWebViewCreation = YES;
#endif

  WKWebView* result =
      [[WKWebView alloc] initWithFrame:frame configuration:configuration];
#if !defined(NDEBUG)
  GetActiveWKWebViewCounter().Insert(result);
  gAllowWKWebViewCreation = NO;
#endif

  // TODO(stuartmorgan): Figure out how to make this work; two different client
  // methods for the two web view types?
  // web::GetWebClient()->PostWebViewCreation(result);

  return result;
}
}

namespace web {

UIWebView* CreateWebView(CGRect frame,
                         NSString* request_group_id,
                         BOOL use_desktop_user_agent) {
  web::BuildAndRegisterUserAgentForUIWebView(request_group_id,
                                             use_desktop_user_agent);
  return web::CreateWebView(frame);
}

UIWebView* CreateWebView(CGRect frame) {
  DCHECK(web::GetWebClient());
  web::GetWebClient()->PreWebViewCreation();

  UIWebView* result = nil;
#if defined(NDEBUG)
  result = [[UIWebView alloc] initWithFrame:frame];
#else
  // TODO(eugenebut): create constant for @"LogJavascript" (crbug.com/391807).
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"LogJavascript"])
    result = [[CRWDebugWebView alloc] initWithFrame:frame];
  else
    result = [[UIWebView alloc] initWithFrame:frame];
#endif  // defined(NDEBUG)

  // Disable data detector types. Safari does the same.
  [result setDataDetectorTypes:UIDataDetectorTypeNone];
  [result setScalesPageToFit:YES];

  // By default UIWebView uses a very sluggish scroll speed. Set it to a more
  // reasonable value.
  result.scrollView.decelerationRate = UIScrollViewDecelerationRateNormal;

  web::GetWebClient()->PostWebViewCreation(result);

  return result;
}

WKWebView* CreateWKWebView(CGRect frame,
                           WKWebViewConfiguration* configuration,
                           NSString* request_group_id,
                           BOOL use_desktop_user_agent) {
  web::BuildAndRegisterUserAgentForUIWebView(request_group_id,
                                             use_desktop_user_agent);
  return CreateWKWebViewWithConfiguration(frame, configuration);
}

WKWebView* CreateWKWebView(CGRect frame, BrowserState* browser_state) {
  DCHECK(browser_state);
  WKWebViewConfigurationProvider& config_provider =
      WKWebViewConfigurationProvider::FromBrowserState(browser_state);
  return CreateWKWebViewWithConfiguration(
      frame, config_provider.GetWebViewConfiguration());
}

NSUInteger GetActiveWKWebViewsCount() {
#if defined(NDEBUG)
  // This should not be used in release builds.
  CHECK(0);
  return 0;
#else
  return GetActiveWKWebViewCounter().Size();
#endif
}

id<CRWSimpleWebViewController> CreateSimpleWebViewController(
    CGRect frame,
    BrowserState* browser_state,
    WebViewType web_view_type) {
  // Transparently return the correct subclass.
  if (web_view_type == WK_WEB_VIEW_TYPE) {
    base::scoped_nsobject<WKWebView> web_view(
        web::CreateWKWebView(frame, browser_state));
    return [[CRWWKSimpleWebViewController alloc] initWithWKWebView:web_view];
  }
  base::scoped_nsobject<UIWebView> web_view(web::CreateWebView(frame));
  return [[CRWUISimpleWebViewController alloc] initWithUIWebView:web_view];
}

id<CRWSimpleWebViewController> CreateStaticFileSimpleWebViewController(
    CGRect frame,
    BrowserState* browser_state,
    WebViewType web_view_type) {
  // Transparently return the correct subclass.
  if (web_view_type == WK_WEB_VIEW_TYPE) {
    // TOOD(shreyasv): Create a new util function vending a WKWebView, wrap that
    // now return the UIWebView version. crbug.com/403634.
  }
  base::scoped_nsobject<UIWebView> staticFileWebView(
      CreateStaticFileWebView(frame, browser_state));
  return [[CRWUISimpleWebViewController alloc]
      initWithUIWebView:staticFileWebView];
}

UIWebView* CreateStaticFileWebView(CGRect frame, BrowserState* browser_state) {
  DCHECK(web::GetWebClient());
  web::GetWebClient()->PreWebViewCreation();

  UIWebView* result =
      [[CRWStaticFileWebView alloc] initWithFrame:frame
                                     browserState:browser_state];

  web::GetWebClient()->PostWebViewCreation(result);
  return result;
}

UIWebView* CreateStaticFileWebView() {
  return CreateStaticFileWebView(CGRectZero, nullptr);
}

}  // namespace web
