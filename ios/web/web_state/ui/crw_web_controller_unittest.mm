// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_controller.h"

#import <WebKit/WebKit.h>

#include <utility>

#include "base/ios/ios_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/navigation/crw_session_entry.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/public/referrer.h"
#import "ios/web/public/test/fakes/test_native_content.h"
#import "ios/web/public/test/fakes/test_native_content_provider.h"
#import "ios/web/public/test/fakes/test_web_client.h"
#import "ios/web/public/test/fakes/test_web_state_delegate.h"
#import "ios/web/public/test/fakes/test_web_view_content_view.h"
#import "ios/web/public/web_state/crw_web_controller_observer.h"
#import "ios/web/public/web_state/ui/crw_content_view.h"
#import "ios/web/public/web_state/ui/crw_native_content.h"
#import "ios/web/public/web_state/ui/crw_native_content_provider.h"
#import "ios/web/public/web_state/ui/crw_web_view_content_view.h"
#include "ios/web/public/web_state/url_verification_constants.h"
#include "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/test/web_test_with_web_controller.h"
#import "ios/web/test/wk_web_view_crash_utils.h"
#import "ios/web/web_state/ui/crw_web_controller_container_view.h"
#import "ios/web/web_state/web_state_impl.h"
#import "ios/web/web_state/wk_web_view_security_util.h"
#import "net/base/mac/url_conversions.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "third_party/ocmock/ocmock_extensions.h"
#import "ui/base/test/ios/ui_view_test_utils.h"

using web::NavigationManagerImpl;

@interface CRWWebController (PrivateAPI)
@property(nonatomic, readwrite) web::PageDisplayState pageDisplayState;
- (GURL)URLForHistoryNavigationToItem:(web::NavigationItem*)toItem
                          previousURL:(const GURL&)previousURL;
@end

// Used to mock CRWWebDelegate methods with C++ params.
@interface MockInteractionLoader : OCMockComplexTypeHelper
// Whether or not the delegate should block popups.
@property(nonatomic, assign) BOOL blockPopups;
// A web controller that will be returned by webPageOrdered... methods.
@property(nonatomic, assign) CRWWebController* childWebController;

// Values of arguments passed to
// |webController:createWebControllerForURL:openerURL:initiatedByUser:|.
@property(nonatomic, readonly) CRWWebController* webController;
@property(nonatomic, readonly) GURL childURL;
@property(nonatomic, readonly) GURL openerURL;
@property(nonatomic, readonly) BOOL initiatedByUser;
@end

@implementation MockInteractionLoader

@synthesize blockPopups = _blockPopups;
@synthesize childWebController = _childWebController;
@synthesize webController = _webController;
@synthesize childURL = _childURL;
@synthesize openerURL = _openerURL;
@synthesize initiatedByUser = _initiatedByUser;

- (instancetype)initWithRepresentedObject:(id)representedObject {
  self = [super initWithRepresentedObject:representedObject];
  if (self) {
    _blockPopups = YES;
  }
  return self;
}

- (CRWWebController*)webController:(CRWWebController*)webController
         createWebControllerForURL:(const GURL&)childURL
                         openerURL:(const GURL&)openerURL
                   initiatedByUser:(BOOL)initiatedByUser {
  _webController = webController;
  _childURL = childURL;
  _openerURL = openerURL;
  _initiatedByUser = initiatedByUser;

  return (_blockPopups && !initiatedByUser) ? nil : _childWebController;
}

typedef BOOL (^openExternalURLBlockType)(const GURL&);

- (BOOL)openExternalURL:(const GURL&)url {
  return static_cast<openExternalURLBlockType>([self blockForSelector:_cmd])(
      url);
}

- (BOOL)webController:(CRWWebController*)webController
        shouldOpenURL:(const GURL&)URL
      mainDocumentURL:(const GURL&)mainDocumentURL
          linkClicked:(BOOL)linkClicked {
  return YES;
}
@end

@interface CountingObserver : NSObject<CRWWebControllerObserver>

@property(nonatomic, readonly) int pageLoadedCount;
@end

@implementation CountingObserver
@synthesize pageLoadedCount = _pageLoadedCount;

- (void)pageLoaded:(CRWWebController*)webController {
  ++_pageLoadedCount;
}

@end

namespace {

// Syntactically invalid URL per rfc3986.
const char kInvalidURL[] = "http://%3";

const char kTestURLString[] = "http://www.google.com/";
const char kTestAppSpecificURL[] = "testwebui://test/";

// Returns true if the current device is a large iPhone (6 or 6+).
bool IsIPhone6Or6Plus() {
  UIUserInterfaceIdiom idiom = [[UIDevice currentDevice] userInterfaceIdiom];
  return (idiom == UIUserInterfaceIdiomPhone &&
          CGRectGetHeight([[UIScreen mainScreen] nativeBounds]) >= 1334.0);
}

// Returns HTML for an optionally zoomable test page with |zoom_state|.
enum PageScalabilityType {
  PAGE_SCALABILITY_DISABLED = 0,
  PAGE_SCALABILITY_ENABLED,
};
NSString* GetHTMLForZoomState(const web::PageZoomState& zoom_state,
                              PageScalabilityType scalability_type) {
  NSString* const kHTMLFormat =
      @"<html><head><meta name='viewport' content="
       "'width=%f,minimum-scale=%f,maximum-scale=%f,initial-scale=%f,"
       "user-scalable=%@'/></head><body>Test</body></html>";
  CGFloat width = CGRectGetWidth([UIScreen mainScreen].bounds) /
      zoom_state.minimum_zoom_scale();
  BOOL scalability_enabled = scalability_type == PAGE_SCALABILITY_ENABLED;
  return [NSString
      stringWithFormat:kHTMLFormat, width, zoom_state.minimum_zoom_scale(),
                       zoom_state.maximum_zoom_scale(), zoom_state.zoom_scale(),
                       scalability_enabled ? @"yes" : @"no"];
}

// Forces |webController|'s view to render and waits until |webController|'s
// PageZoomState matches |zoom_state|.
void WaitForZoomRendering(CRWWebController* webController,
                          const web::PageZoomState& zoom_state) {
  ui::test::uiview_utils::ForceViewRendering(webController.view);
  base::test::ios::WaitUntilCondition(^bool() {
    return webController.pageDisplayState.zoom_state() == zoom_state;
  });
}

// Test fixture for testing CRWWebController. Stubs out web view and
// child CRWWebController.
class CRWWebControllerTest : public web::WebTestWithWebController {
 protected:
  void SetUp() override {
    web::WebTestWithWebController::SetUp();
    mockWebView_.reset(CreateMockWebView());
    mockScrollView_.reset([[UIScrollView alloc] init]);
    [[[mockWebView_ stub] andReturn:mockScrollView_.get()] scrollView];

    id originalMockDelegate =
        [OCMockObject niceMockForProtocol:@protocol(CRWWebDelegate)];
    mockDelegate_.reset([[MockInteractionLoader alloc]
        initWithRepresentedObject:originalMockDelegate]);
    [web_controller() setDelegate:mockDelegate_];
    base::scoped_nsobject<TestWebViewContentView> webViewContentView(
        [[TestWebViewContentView alloc] initWithMockWebView:mockWebView_
                                                 scrollView:mockScrollView_]);
    [web_controller() injectWebViewContentView:webViewContentView];

    NavigationManagerImpl& navigationManager =
        [web_controller() webStateImpl]->GetNavigationManagerImpl();
    navigationManager.InitializeSession(NO);
    navigationManager.AddPendingItem(
        GURL("http://www.google.com/?q=foo#bar"), web::Referrer(),
        ui::PAGE_TRANSITION_TYPED,
        web::NavigationInitiationType::USER_INITIATED);
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(mockDelegate_);
    EXPECT_OCMOCK_VERIFY(mockChildWebController_);
    EXPECT_OCMOCK_VERIFY(mockWebView_);
    [web_controller() resetInjectedWebViewContentView];
    [web_controller() setDelegate:nil];
    web::WebTestWithWebController::TearDown();
  }

  // The value for web view OCMock objects to expect for |-setFrame:|.
  CGRect ExpectedWebViewFrame() const {
    CGSize containerViewSize = [UIScreen mainScreen].bounds.size;
    containerViewSize.height -=
        CGRectGetHeight([UIApplication sharedApplication].statusBarFrame);
    return {CGPointZero, containerViewSize};
  }

  // Creates WebView mock.
  UIView* CreateMockWebView() {
    id result = [[OCMockObject mockForClass:[WKWebView class]] retain];

    if (base::ios::IsRunningOnIOS10OrLater()) {
      [[result stub] serverTrust];
    } else {
      [[result stub] certificateChain];
    }

    [[result stub] backForwardList];
    [[[result stub] andReturn:[NSURL URLWithString:@(kTestURLString)]] URL];
    [[result stub] setNavigationDelegate:OCMOCK_ANY];
    [[result stub] setUIDelegate:OCMOCK_ANY];
    [[result stub] setFrame:ExpectedWebViewFrame()];
    [[result stub] addObserver:web_controller()
                    forKeyPath:OCMOCK_ANY
                       options:0
                       context:nullptr];
    [[result stub] addObserver:OCMOCK_ANY
                    forKeyPath:@"scrollView.backgroundColor"
                       options:0
                       context:nullptr];

    [[result stub] removeObserver:web_controller() forKeyPath:OCMOCK_ANY];
    [[result stub] removeObserver:OCMOCK_ANY
                       forKeyPath:@"scrollView.backgroundColor"];

    return result;
  }

  base::scoped_nsobject<UIScrollView> mockScrollView_;
  base::scoped_nsobject<id> mockWebView_;
  base::scoped_nsobject<id> mockDelegate_;
  base::scoped_nsobject<id> mockChildWebController_;
};

#define MAKE_URL(url_string) GURL([url_string UTF8String])

TEST_F(CRWWebControllerTest, UrlForHistoryNavigation) {
  NSArray* urlsNoFragments = @[
    @"http://one.com",
    @"http://two.com/",
    @"http://three.com/bar",
    @"http://four.com/bar/",
    @"five",
    @"/six",
    @"/seven/",
    @""
  ];

  NSArray* fragments = @[ @"#", @"#bar" ];
  NSMutableArray* urlsWithFragments = [NSMutableArray array];
  for (NSString* url in urlsNoFragments) {
    for (NSString* fragment in fragments) {
      [urlsWithFragments addObject:[url stringByAppendingString:fragment]];
    }
  }

  GURL previous_url;
  web::NavigationItemImpl toItem;

  // No start fragment: the end url is never changed.
  for (NSString* start in urlsNoFragments) {
    for (NSString* end in urlsWithFragments) {
      previous_url = MAKE_URL(start);
      toItem.SetURL(MAKE_URL(end));
      EXPECT_EQ(MAKE_URL(end),
                [web_controller() URLForHistoryNavigationToItem:&toItem
                                                    previousURL:previous_url]);
    }
  }
  // Both contain fragments: the end url is never changed.
  for (NSString* start in urlsWithFragments) {
    for (NSString* end in urlsWithFragments) {
      previous_url = MAKE_URL(start);
      toItem.SetURL(MAKE_URL(end));
      EXPECT_EQ(MAKE_URL(end),
                [web_controller() URLForHistoryNavigationToItem:&toItem
                                                    previousURL:previous_url]);
    }
  }
  for (unsigned start_index = 0; start_index < [urlsWithFragments count];
       ++start_index) {
    NSString* start = urlsWithFragments[start_index];
    for (unsigned end_index = 0; end_index < [urlsNoFragments count];
         ++end_index) {
      NSString* end = urlsNoFragments[end_index];
      previous_url = MAKE_URL(start);
      if (start_index / 2 != end_index) {
        // The URLs have nothing in common, they are left untouched.
        toItem.SetURL(MAKE_URL(end));
        EXPECT_EQ(
            MAKE_URL(end),
            [web_controller() URLForHistoryNavigationToItem:&toItem
                                                previousURL:previous_url]);
      } else {
        // Start contains a fragment and matches end: An empty fragment is
        // added.
        toItem.SetURL(MAKE_URL(end));
        EXPECT_EQ(
            MAKE_URL([end stringByAppendingString:@"#"]),
            [web_controller() URLForHistoryNavigationToItem:&toItem
                                                previousURL:previous_url]);
      }
    }
  }
}

// Tests that AllowCertificateError is called with correct arguments if
// WKWebView fails to load a page with bad SSL cert.
TEST_F(CRWWebControllerTest, SslCertError) {
  // Last arguments passed to AllowCertificateError must be in default state.
  ASSERT_FALSE(GetWebClient()->last_cert_error_code());
  ASSERT_FALSE(GetWebClient()->last_cert_error_ssl_info().is_valid());
  ASSERT_FALSE(GetWebClient()->last_cert_error_ssl_info().cert_status);
  ASSERT_FALSE(GetWebClient()->last_cert_error_request_url().is_valid());
  ASSERT_TRUE(GetWebClient()->last_cert_error_overridable());

  scoped_refptr<net::X509Certificate> cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");

  NSArray* chain = @[ static_cast<id>(cert->os_cert_handle()) ];
  GURL url("https://chromium.test");
  NSError* error =
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorServerCertificateHasUnknownRoot
                      userInfo:@{
                        web::kNSErrorPeerCertificateChainKey : chain,
                        web::kNSErrorFailingURLKey : net::NSURLWithGURL(url),
                      }];
  CRWWebControllerContainerView* containerView =
      static_cast<CRWWebControllerContainerView*>([web_controller() view]);
  WKWebView* webView =
      static_cast<WKWebView*>(containerView.webViewContentView.webView);
  base::scoped_nsobject<NSObject> navigation([[NSObject alloc] init]);
  [static_cast<id<WKNavigationDelegate>>(web_controller())
                            webView:webView
      didStartProvisionalNavigation:static_cast<WKNavigation*>(navigation)];
  [static_cast<id<WKNavigationDelegate>>(web_controller())
                           webView:webView
      didFailProvisionalNavigation:static_cast<WKNavigation*>(navigation)
                         withError:error];

  // Verify correctness of AllowCertificateError method call.
  EXPECT_EQ(net::ERR_CERT_INVALID, GetWebClient()->last_cert_error_code());
  EXPECT_TRUE(GetWebClient()->last_cert_error_ssl_info().is_valid());
  EXPECT_EQ(net::CERT_STATUS_INVALID,
            GetWebClient()->last_cert_error_ssl_info().cert_status);
  EXPECT_EQ(url, GetWebClient()->last_cert_error_request_url());
  EXPECT_FALSE(GetWebClient()->last_cert_error_overridable());
}

// Test fixture to test |setPageDialogOpenPolicy:|.
class CRWWebControllerPageDialogOpenPolicyTest
    : public web::WebTestWithWebController {
 protected:
  CRWWebControllerPageDialogOpenPolicyTest()
      : page_url_("https://chromium.test/") {}
  void SetUp() override {
    web::WebTestWithWebController::SetUp();
    LoadHtml(@"<html><body></body></html>", page_url_);
    web_delegate_mock_.reset(
        [[OCMockObject mockForProtocol:@protocol(CRWWebDelegate)] retain]);
    [web_controller() setDelegate:web_delegate_mock_];
    web_state()->SetDelegate(&test_web_delegate_);
  }
  void TearDown() override {
    WaitForBackgroundTasks();
    EXPECT_OCMOCK_VERIFY(web_delegate_mock_);
    [web_controller() setDelegate:nil];
    web_state()->SetDelegate(nullptr);

    web::WebTestWithWebController::TearDown();
  }
  id web_delegate_mock() { return web_delegate_mock_; };
  web::TestJavaScriptDialogPresenter* js_dialog_presenter() {
    return test_web_delegate_.GetTestJavaScriptDialogPresenter();
  }
  const std::vector<web::TestJavaScriptDialog>& requested_dialogs() {
    return js_dialog_presenter()->requested_dialogs();
  }
  const GURL& page_url() { return page_url_; }

 private:
  web::TestWebStateDelegate test_web_delegate_;
  base::scoped_nsprotocol<id> web_delegate_mock_;
  GURL page_url_;
};

// Tests that window.alert dialog is suppressed for DIALOG_POLICY_SUPPRESS.
TEST_F(CRWWebControllerPageDialogOpenPolicyTest, SuppressAlert) {
  [[web_delegate_mock() expect]
      webControllerDidSuppressDialog:web_controller()];
  [web_controller() setShouldSuppressDialogs:YES];
  ExecuteJavaScript(@"alert('test')");
};

// Tests that window.alert dialog is shown for DIALOG_POLICY_ALLOW.
TEST_F(CRWWebControllerPageDialogOpenPolicyTest, AllowAlert) {
  ASSERT_TRUE(requested_dialogs().empty());

  [web_controller() setShouldSuppressDialogs:NO];
  ExecuteJavaScript(@"alert('test')");

  ASSERT_EQ(1U, requested_dialogs().size());
  web::TestJavaScriptDialog dialog = requested_dialogs()[0];
  EXPECT_EQ(web_state(), dialog.web_state);
  EXPECT_EQ(page_url(), dialog.origin_url);
  EXPECT_EQ(web::JAVASCRIPT_DIALOG_TYPE_ALERT, dialog.java_script_dialog_type);
  EXPECT_NSEQ(@"test", dialog.message_text);
  EXPECT_FALSE(dialog.default_prompt_text);
};

// Tests that window.confirm dialog is suppressed for DIALOG_POLICY_SUPPRESS.
TEST_F(CRWWebControllerPageDialogOpenPolicyTest, SuppressConfirm) {
  ASSERT_TRUE(requested_dialogs().empty());

  [[web_delegate_mock() expect]
      webControllerDidSuppressDialog:web_controller()];
  [web_controller() setShouldSuppressDialogs:YES];
  EXPECT_NSEQ(@NO, ExecuteJavaScript(@"confirm('test')"));

  ASSERT_TRUE(requested_dialogs().empty());
};

// Tests that window.confirm dialog is shown for DIALOG_POLICY_ALLOW and
// it's result is true.
TEST_F(CRWWebControllerPageDialogOpenPolicyTest, AllowConfirmWithTrue) {
  ASSERT_TRUE(requested_dialogs().empty());

  js_dialog_presenter()->set_callback_success_argument(true);

  [web_controller() setShouldSuppressDialogs:NO];
  EXPECT_NSEQ(@YES, ExecuteJavaScript(@"confirm('test')"));

  ASSERT_EQ(1U, requested_dialogs().size());
  web::TestJavaScriptDialog dialog = requested_dialogs()[0];
  EXPECT_EQ(web_state(), dialog.web_state);
  EXPECT_EQ(page_url(), dialog.origin_url);
  EXPECT_EQ(web::JAVASCRIPT_DIALOG_TYPE_CONFIRM,
            dialog.java_script_dialog_type);
  EXPECT_NSEQ(@"test", dialog.message_text);
  EXPECT_FALSE(dialog.default_prompt_text);
}

// Tests that window.confirm dialog is shown for DIALOG_POLICY_ALLOW and
// it's result is false.
TEST_F(CRWWebControllerPageDialogOpenPolicyTest, AllowConfirmWithFalse) {
  ASSERT_TRUE(requested_dialogs().empty());

  [web_controller() setShouldSuppressDialogs:NO];
  EXPECT_NSEQ(@NO, ExecuteJavaScript(@"confirm('test')"));

  ASSERT_EQ(1U, requested_dialogs().size());
  web::TestJavaScriptDialog dialog = requested_dialogs()[0];
  EXPECT_EQ(web_state(), dialog.web_state);
  EXPECT_EQ(page_url(), dialog.origin_url);
  EXPECT_EQ(web::JAVASCRIPT_DIALOG_TYPE_CONFIRM,
            dialog.java_script_dialog_type);
  EXPECT_NSEQ(@"test", dialog.message_text);
  EXPECT_FALSE(dialog.default_prompt_text);
}

// Tests that window.prompt dialog is suppressed for DIALOG_POLICY_SUPPRESS.
TEST_F(CRWWebControllerPageDialogOpenPolicyTest, SuppressPrompt) {
  ASSERT_TRUE(requested_dialogs().empty());

  [[web_delegate_mock() expect]
      webControllerDidSuppressDialog:web_controller()];
  [web_controller() setShouldSuppressDialogs:YES];
  EXPECT_EQ([NSNull null], ExecuteJavaScript(@"prompt('Yes?', 'No')"));

  ASSERT_TRUE(requested_dialogs().empty());
}

// Tests that window.prompt dialog is shown for DIALOG_POLICY_ALLOW.
TEST_F(CRWWebControllerPageDialogOpenPolicyTest, AllowPrompt) {
  ASSERT_TRUE(requested_dialogs().empty());

  js_dialog_presenter()->set_callback_user_input_argument(@"Maybe");

  [web_controller() setShouldSuppressDialogs:NO];
  EXPECT_NSEQ(@"Maybe", ExecuteJavaScript(@"prompt('Yes?', 'No')"));

  ASSERT_EQ(1U, requested_dialogs().size());
  web::TestJavaScriptDialog dialog = requested_dialogs()[0];
  EXPECT_EQ(web_state(), dialog.web_state);
  EXPECT_EQ(page_url(), dialog.origin_url);
  EXPECT_EQ(web::JAVASCRIPT_DIALOG_TYPE_PROMPT, dialog.java_script_dialog_type);
  EXPECT_NSEQ(@"Yes?", dialog.message_text);
  EXPECT_NSEQ(@"No", dialog.default_prompt_text);
}

// Tests that geolocation dialog is suppressed for DIALOG_POLICY_SUPPRESS.
TEST_F(CRWWebControllerPageDialogOpenPolicyTest, SuppressGeolocation) {
  // The geolocation APIs require HTTPS on iOS 10, which can not be simulated
  // even using |loadHTMLString:baseURL:| WKWebView API.
  if (base::ios::IsRunningOnIOS10OrLater()) {
    return;
  }

  [[web_delegate_mock() expect]
      webControllerDidSuppressDialog:web_controller()];
  [web_controller() setShouldSuppressDialogs:YES];
  ExecuteJavaScript(@"navigator.geolocation.getCurrentPosition()");
}

// Tests that window.open is suppressed for DIALOG_POLICY_SUPPRESS.
TEST_F(CRWWebControllerPageDialogOpenPolicyTest, SuppressWindowOpen) {
  [[web_delegate_mock() expect]
      webControllerDidSuppressDialog:web_controller()];
  [web_controller() setShouldSuppressDialogs:YES];
  ExecuteJavaScript(@"window.open('')");
}

// A separate test class, as none of the |CRWWebControllerTest| setup is
// needed.
class CRWWebControllerPageScrollStateTest
    : public web::WebTestWithWebController {
 protected:
  // Returns a web::PageDisplayState that will scroll a WKWebView to
  // |scrollOffset| and zoom the content by |relativeZoomScale|.
  inline web::PageDisplayState CreateTestPageDisplayState(
      CGPoint scroll_offset,
      CGFloat relative_zoom_scale,
      CGFloat original_minimum_zoom_scale,
      CGFloat original_maximum_zoom_scale,
      CGFloat original_zoom_scale) const {
    return web::PageDisplayState(
        scroll_offset.x, scroll_offset.y, original_minimum_zoom_scale,
        original_maximum_zoom_scale,
        relative_zoom_scale * original_minimum_zoom_scale);
  }
};

// TODO(crbug/493427): Flaky on the bots.
TEST_F(CRWWebControllerPageScrollStateTest,
       FLAKY_SetPageDisplayStateWithUserScalableDisabled) {
#if !TARGET_IPHONE_SIMULATOR
  // TODO(crbug.com/493427): fails flakily on device, so skip it there.
  return;
#endif
  web::PageZoomState zoom_state(1.0, 5.0, 1.0);
  LoadHtml(GetHTMLForZoomState(zoom_state, PAGE_SCALABILITY_DISABLED));
  WaitForZoomRendering(web_controller(), zoom_state);
  web::PageZoomState original_zoom_state =
      web_controller().pageDisplayState.zoom_state();

  web::NavigationManager* nagivation_manager =
      web_state()->GetNavigationManager();
  nagivation_manager->GetLastCommittedItem()->SetPageDisplayState(
      CreateTestPageDisplayState(CGPointMake(1.0, 1.0),  // scroll offset
                                 3.0,                    // relative zoom scale
                                 1.0,    // original minimum zoom scale
                                 5.0,    // original maximum zoom scale
                                 1.0));  // original zoom scale
  [web_controller() restoreStateFromHistory];

  // |-restoreStateFromHistory| is async; wait for its completion.
  base::test::ios::WaitUntilCondition(^bool() {
    return web_controller().pageDisplayState.scroll_state().offset_x() == 1.0;
  });

  ASSERT_EQ(original_zoom_state,
            web_controller().pageDisplayState.zoom_state());
};

// TODO(crbug/493427): Flaky on the bots.
TEST_F(CRWWebControllerPageScrollStateTest,
       FLAKY_SetPageDisplayStateWithUserScalableEnabled) {
#if !TARGET_IPHONE_SIMULATOR
  // TODO(crbug.com/493427): fails flakily on device, so skip it there.
  return;
#endif
  web::PageZoomState zoom_state(1.0, 5.0, 1.0);

  LoadHtml(GetHTMLForZoomState(zoom_state, PAGE_SCALABILITY_ENABLED));
  WaitForZoomRendering(web_controller(), zoom_state);

  web::NavigationManager* nagivation_manager =
      web_state()->GetNavigationManager();
  nagivation_manager->GetLastCommittedItem()->SetPageDisplayState(
      CreateTestPageDisplayState(CGPointMake(1.0, 1.0),  // scroll offset
                                 3.0,                    // relative zoom scale
                                 1.0,    // original minimum zoom scale
                                 5.0,    // original maximum zoom scale
                                 1.0));  // original zoom scale
  [web_controller() restoreStateFromHistory];

  // |-restoreStateFromHistory| is async; wait for its completion.
  base::test::ios::WaitUntilCondition(^bool() {
    return web_controller().pageDisplayState.scroll_state().offset_x() == 1.0;
  });

  web::PageZoomState final_zoom_state =
      web_controller().pageDisplayState.zoom_state();
  EXPECT_FLOAT_EQ(3, final_zoom_state.zoom_scale() /
                        final_zoom_state.minimum_zoom_scale());
};

// TODO(crbug/493427): Flaky on the bots.
TEST_F(CRWWebControllerPageScrollStateTest, DISABLED_AtTop) {
  // This test fails on iPhone 6/6+; skip until it's fixed. crbug.com/453105
  if (IsIPhone6Or6Plus())
    return;

  web::PageZoomState zoom_state = web::PageZoomState(1.0, 5.0, 1.0);
  LoadHtml(GetHTMLForZoomState(zoom_state, PAGE_SCALABILITY_ENABLED));
  WaitForZoomRendering(web_controller(), zoom_state);
  ASSERT_TRUE(web_controller().atTop);

  web::NavigationManager* nagivation_manager =
      web_state()->GetNavigationManager();
  nagivation_manager->GetLastCommittedItem()->SetPageDisplayState(
      CreateTestPageDisplayState(CGPointMake(0.0, 30.0),  // scroll offset
                                 5.0,                     // relative zoom scale
                                 1.0,    // original minimum zoom scale
                                 5.0,    // original maximum zoom scale
                                 1.0));  // original zoom scale
  [web_controller() restoreStateFromHistory];

  // |-restoreStateFromHistory| is async; wait for its completion.
  base::test::ios::WaitUntilCondition(^bool() {
    return web_controller().pageDisplayState.scroll_state().offset_y() == 30.0;
  });

  ASSERT_FALSE(web_controller().atTop);
};

// Real WKWebView is required for CRWWebControllerNavigationTest.
typedef web::WebTestWithWebController CRWWebControllerNavigationTest;

// Tests navigation between 2 URLs which differ only by fragment.
TEST_F(CRWWebControllerNavigationTest, GoToEntryWithoutDocumentChange) {
  LoadHtml(@"<html><body></body></html>", GURL("https://chromium.test"));
  LoadHtml(@"<html><body></body></html>", GURL("https://chromium.test#hash"));
  NavigationManagerImpl& nav_manager =
      web_controller().webStateImpl->GetNavigationManagerImpl();
  CRWSessionController* session_controller = nav_manager.GetSessionController();
  EXPECT_EQ(2U, session_controller.entries.count);
  EXPECT_NSEQ(session_controller.entries.lastObject,
              session_controller.currentEntry);

  [web_controller() goToItemAtIndex:0];
  EXPECT_NSEQ(session_controller.entries.firstObject,
              session_controller.currentEntry);
}

// Tests that didShowPasswordInputOnHTTP updates the SSLStatus to indicate that
// a password field has been displayed on an HTTP page.
TEST_F(CRWWebControllerNavigationTest, HTTPPassword) {
  LoadHtml(@"<html><body></body></html>", GURL("http://chromium.test"));
  NavigationManagerImpl& nav_manager =
      web_controller().webStateImpl->GetNavigationManagerImpl();
  EXPECT_FALSE(nav_manager.GetLastCommittedItem()->GetSSL().content_status &
               web::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
  [web_controller() didShowPasswordInputOnHTTP];
  EXPECT_TRUE(nav_manager.GetLastCommittedItem()->GetSSL().content_status &
              web::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
}

// Tests that didShowCreditCardInputOnHTTP updates the SSLStatus to indicate
// that a credit card field has been displayed on an HTTP page.
TEST_F(CRWWebControllerNavigationTest, HTTPCreditCard) {
  LoadHtml(@"<html><body></body></html>", GURL("http://chromium.test"));
  NavigationManagerImpl& nav_manager =
      web_controller().webStateImpl->GetNavigationManagerImpl();
  EXPECT_FALSE(nav_manager.GetLastCommittedItem()->GetSSL().content_status &
               web::SSLStatus::DISPLAYED_CREDIT_CARD_FIELD_ON_HTTP);
  [web_controller() didShowCreditCardInputOnHTTP];
  EXPECT_TRUE(nav_manager.GetLastCommittedItem()->GetSSL().content_status &
              web::SSLStatus::DISPLAYED_CREDIT_CARD_FIELD_ON_HTTP);
}

// Real WKWebView is required for CRWWebControllerInvalidUrlTest.
typedef web::WebTestWithWebState CRWWebControllerInvalidUrlTest;

// Tests that web controller navigates to about:blank if invalid URL is loaded.
TEST_F(CRWWebControllerInvalidUrlTest, LoadInvalidURL) {
  GURL url(kInvalidURL);
  ASSERT_FALSE(url.is_valid());
  LoadHtml(@"<html><body></body></html>", url);
  EXPECT_EQ(GURL(url::kAboutBlankURL), web_state()->GetLastCommittedURL());
}

// Tests that web controller does not navigate to about:blank if iframe src
// has invalid url. Web controller loads about:blank if page navigates to
// invalid url, but should do nothing if navigation is performed in iframe. This
// test prevents crbug.com/694865 regression.
TEST_F(CRWWebControllerInvalidUrlTest, IFrameWithInvalidURL) {
  GURL url("http://chromium.test");
  ASSERT_FALSE(GURL(kInvalidURL).is_valid());
  LoadHtml([NSString stringWithFormat:@"<iframe src='%s'/>", kInvalidURL], url);
  EXPECT_EQ(url, web_state()->GetLastCommittedURL());
}

// Real WKWebView is required for CRWWebControllerFormActivityTest.
typedef web::WebTestWithWebController CRWWebControllerFormActivityTest;

// Tests that keyup event correctly delivered to WebStateObserver.
TEST_F(CRWWebControllerFormActivityTest, KeyUpEvent) {
  // Observes and verifies FormActivityRegistered call.
  class FormActivityObserver : public web::WebStateObserver {
   public:
    explicit FormActivityObserver(web::WebState* web_state)
        : web::WebStateObserver(web_state) {}
    bool form_activity_registered() const { return form_activity_registered_; }
    // WebStateObserver overrides:
    void FormActivityRegistered(const std::string& form_name,
                                const std::string& field_name,
                                const std::string& type,
                                const std::string& value,
                                bool input_missing) override {
      EXPECT_EQ("keyup", type);
      EXPECT_FALSE(input_missing);
      form_activity_registered_ = true;
    }

   private:
    bool form_activity_registered_ = false;
  };
  FormActivityObserver form_activity_observer(web_state());
  FormActivityObserver& form_activity_observer_ref(form_activity_observer);

  LoadHtml(@"<p></p>");
  ExecuteJavaScript(@"document.dispatchEvent(new KeyboardEvent('keyup'));");
  base::test::ios::WaitUntilCondition(^{
    return form_activity_observer_ref.form_activity_registered();
  });
}

// Real WKWebView is required for CRWWebControllerJSExecutionTest.
typedef web::WebTestWithWebController CRWWebControllerJSExecutionTest;

// Tests that a script correctly evaluates to boolean.
TEST_F(CRWWebControllerJSExecutionTest, Execution) {
  LoadHtml(@"<p></p>");
  EXPECT_NSEQ(@YES, ExecuteJavaScript(@"true"));
  EXPECT_NSEQ(@NO, ExecuteJavaScript(@"false"));
}

// Tests that a script is not executed on windowID mismatch.
TEST_F(CRWWebControllerJSExecutionTest, WindowIdMissmatch) {
  LoadHtml(@"<p></p>");
  // Script is evaluated since windowID is matched.
  ExecuteJavaScript(@"window.test1 = '1';");
  EXPECT_NSEQ(@"1", ExecuteJavaScript(@"window.test1"));

  // Change windowID.
  ExecuteJavaScript(@"__gCrWeb['windowId'] = '';");

  // Script is not evaluated because of windowID mismatch.
  ExecuteJavaScript(@"window.test2 = '2';");
  EXPECT_FALSE(ExecuteJavaScript(@"window.test2"));
}

TEST_F(CRWWebControllerTest, WebUrlWithTrustLevel) {
  [[[mockWebView_ stub] andReturn:[NSURL URLWithString:@(kTestURLString)]] URL];
  [[[mockWebView_ stub] andReturnBool:NO] hasOnlySecureContent];
  [[[mockWebView_ stub] andReturn:@""] title];

  // Stub out the injection process.
  [[mockWebView_ stub] evaluateJavaScript:OCMOCK_ANY
                        completionHandler:OCMOCK_ANY];

  // Simulate registering load request to avoid failing page load simulation.
  [web_controller() simulateLoadRequestWithURL:GURL(kTestURLString)];
  // Simulate a page load to trigger a URL update.
  [static_cast<id<WKNavigationDelegate>>(web_controller()) webView:mockWebView_
                                               didCommitNavigation:nil];

  web::URLVerificationTrustLevel trust_level = web::kNone;
  GURL gurl = [web_controller() currentURLWithTrustLevel:&trust_level];

  EXPECT_EQ(gurl, GURL(kTestURLString));
  EXPECT_EQ(web::kAbsolute, trust_level);
}

// Test fixture for testing CRWWebController presenting native content.
class CRWWebControllerNativeContentTest : public web::WebTestWithWebController {
 protected:
  void SetUp() override {
    web::WebTestWithWebController::SetUp();
    mock_native_provider_.reset([[TestNativeContentProvider alloc] init]);
    [web_controller() setNativeProvider:mock_native_provider_];
  }

  void Load(const GURL& URL) {
    NavigationManagerImpl& navigation_manager =
        [web_controller() webStateImpl]->GetNavigationManagerImpl();
    navigation_manager.InitializeSession(NO);
    navigation_manager.AddPendingItem(
        URL, web::Referrer(), ui::PAGE_TRANSITION_TYPED,
        web::NavigationInitiationType::USER_INITIATED);
    [web_controller() loadCurrentURL];
  }

  base::scoped_nsobject<TestNativeContentProvider> mock_native_provider_;
};

// Tests WebState and NavigationManager correctly return native content URL.
TEST_F(CRWWebControllerNativeContentTest, NativeContentURL) {
  GURL url_to_load(kTestAppSpecificURL);
  base::scoped_nsobject<TestNativeContent> content(
      [[TestNativeContent alloc] initWithURL:url_to_load virtualURL:GURL()]);
  [mock_native_provider_ setController:content forURL:url_to_load];
  Load(url_to_load);
  web::URLVerificationTrustLevel trust_level = web::kNone;
  GURL gurl = [web_controller() currentURLWithTrustLevel:&trust_level];
  EXPECT_EQ(gurl, url_to_load);
  EXPECT_EQ(web::kAbsolute, trust_level);
  EXPECT_EQ([web_controller() webState]->GetVisibleURL(), url_to_load);
  NavigationManagerImpl& navigationManager =
      [web_controller() webStateImpl]->GetNavigationManagerImpl();
  EXPECT_EQ(navigationManager.GetVisibleItem()->GetURL(), url_to_load);
  EXPECT_EQ(navigationManager.GetVisibleItem()->GetVirtualURL(), url_to_load);
  EXPECT_EQ(navigationManager.GetLastCommittedItem()->GetURL(), url_to_load);
  EXPECT_EQ(navigationManager.GetLastCommittedItem()->GetVirtualURL(),
            url_to_load);
}

// Tests WebState and NavigationManager correctly return native content URL and
// VirtualURL
TEST_F(CRWWebControllerNativeContentTest, NativeContentVirtualURL) {
  GURL url_to_load(kTestAppSpecificURL);
  GURL virtual_url(kTestURLString);
  base::scoped_nsobject<TestNativeContent> content([[TestNativeContent alloc]
      initWithURL:virtual_url
       virtualURL:virtual_url]);
  [mock_native_provider_ setController:content forURL:url_to_load];
  Load(url_to_load);
  web::URLVerificationTrustLevel trust_level = web::kNone;
  GURL gurl = [web_controller() currentURLWithTrustLevel:&trust_level];
  EXPECT_EQ(gurl, virtual_url);
  EXPECT_EQ(web::kAbsolute, trust_level);
  EXPECT_EQ([web_controller() webState]->GetVisibleURL(), virtual_url);
  NavigationManagerImpl& navigationManager =
      [web_controller() webStateImpl]->GetNavigationManagerImpl();
  EXPECT_EQ(navigationManager.GetVisibleItem()->GetURL(), url_to_load);
  EXPECT_EQ(navigationManager.GetVisibleItem()->GetVirtualURL(), virtual_url);
  EXPECT_EQ(navigationManager.GetLastCommittedItem()->GetURL(), url_to_load);
  EXPECT_EQ(navigationManager.GetLastCommittedItem()->GetVirtualURL(),
            virtual_url);
}

// A separate test class, as none of the |CRWUIWebViewWebControllerTest| setup
// is needed;
typedef web::WebTestWithWebController CRWWebControllerObserversTest;

// Tests that CRWWebControllerObservers are called.
TEST_F(CRWWebControllerObserversTest, Observers) {
  base::scoped_nsobject<CountingObserver> observer(
      [[CountingObserver alloc] init]);
  EXPECT_EQ(0u, [web_controller() observerCount]);
  [web_controller() addObserver:observer];
  EXPECT_EQ(1u, [web_controller() observerCount]);

  EXPECT_EQ(0, [observer pageLoadedCount]);
  [web_controller() webStateImpl]->OnPageLoaded(GURL("http://test"), false);
  EXPECT_EQ(0, [observer pageLoadedCount]);
  [web_controller() webStateImpl]->OnPageLoaded(GURL("http://test"), true);
  EXPECT_EQ(1, [observer pageLoadedCount]);

  [web_controller() removeObserver:observer];
  EXPECT_EQ(0u, [web_controller() observerCount]);
};

// Test fixture for window.open tests.
class CRWWebControllerWindowOpenTest : public web::WebTestWithWebController {
 protected:
  void SetUp() override {
    web::WebTestWithWebController::SetUp();

    // Configure web delegate.
    delegate_.reset([[MockInteractionLoader alloc]
        initWithRepresentedObject:
            [OCMockObject niceMockForProtocol:@protocol(CRWWebDelegate)]]);
    ASSERT_TRUE([delegate_ blockPopups]);
    [web_controller() setDelegate:delegate_];

    // Configure child web state.
    child_web_state_.reset(new web::WebStateImpl(GetBrowserState()));
    child_web_state_->SetWebUsageEnabled(true);
    [delegate_ setChildWebController:child_web_state_->GetWebController()];

    // Configure child web controller's session controller mock.
    id sessionController =
        [OCMockObject niceMockForClass:[CRWSessionController class]];
    BOOL yes = YES;
    [[[sessionController stub] andReturnValue:OCMOCK_VALUE(yes)] isOpenedByDOM];
    child_web_state_->GetNavigationManagerImpl().SetSessionController(
        sessionController);

    LoadHtml(@"<html><body></body></html>", GURL("http://test"));
  }
  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(delegate_);
    [web_controller() setDelegate:nil];

    web::WebTestWithWebController::TearDown();
  }
  // Executes JavaScript that opens a new window and returns evaluation result
  // as a string.
  id OpenWindowByDOM() {
    NSString* const kOpenWindowScript =
        @"var w = window.open('javascript:void(0);', target='_blank');"
         "w ? w.toString() : null;";
    id windowJSObject = ExecuteJavaScript(kOpenWindowScript);
    WaitForBackgroundTasks();
    return windowJSObject;
  }
  // A CRWWebDelegate mock used for testing.
  base::scoped_nsobject<id> delegate_;
  // A child WebState used for testing.
  std::unique_ptr<web::WebStateImpl> child_web_state_;
};

// Tests that absence of web delegate is handled gracefully.
TEST_F(CRWWebControllerWindowOpenTest, NoDelegate) {
  [web_controller() setDelegate:nil];

  EXPECT_NSEQ([NSNull null], OpenWindowByDOM());

  EXPECT_FALSE([delegate_ webController]);
  EXPECT_FALSE([delegate_ childURL].is_valid());
  EXPECT_FALSE([delegate_ openerURL].is_valid());
  EXPECT_FALSE([delegate_ initiatedByUser]);
}

// Tests that window.open triggered by user gesture opens a new non-popup
// window.
TEST_F(CRWWebControllerWindowOpenTest, OpenWithUserGesture) {
  [web_controller() touched:YES];
  EXPECT_NSEQ(@"[object Window]", OpenWindowByDOM());

  EXPECT_EQ(web_controller(), [delegate_ webController]);
  EXPECT_EQ("javascript:void(0);", [delegate_ childURL].spec());
  EXPECT_EQ("http://test/", [delegate_ openerURL].spec());
  EXPECT_TRUE([delegate_ initiatedByUser]);
}

// Tests that window.open executed w/o user gesture does not open a new window.
// Once the blocked popup is allowed a new window is opened.
TEST_F(CRWWebControllerWindowOpenTest, AllowPopup) {
  ASSERT_FALSE([web_controller() userIsInteracting]);
  EXPECT_NSEQ([NSNull null], OpenWindowByDOM());

  EXPECT_EQ(web_controller(), [delegate_ webController]);
  EXPECT_EQ("javascript:void(0);", [delegate_ childURL].spec());
  EXPECT_EQ("http://test/", [delegate_ openerURL].spec());
  EXPECT_FALSE([delegate_ initiatedByUser]);
}

// Tests that window.open executed w/o user gesture opens a new window, assuming
// that delegate allows popups.
TEST_F(CRWWebControllerWindowOpenTest, DontBlockPopup) {
  [delegate_ setBlockPopups:NO];
  EXPECT_NSEQ(@"[object Window]", OpenWindowByDOM());

  EXPECT_EQ(web_controller(), [delegate_ webController]);
  EXPECT_EQ("javascript:void(0);", [delegate_ childURL].spec());
  EXPECT_EQ("http://test/", [delegate_ openerURL].spec());
  EXPECT_FALSE([delegate_ initiatedByUser]);
}

// Tests that window.open executed w/o user gesture does not open a new window.
TEST_F(CRWWebControllerWindowOpenTest, BlockPopup) {
  ASSERT_FALSE([web_controller() userIsInteracting]);
  EXPECT_NSEQ([NSNull null], OpenWindowByDOM());

  EXPECT_EQ(web_controller(), [delegate_ webController]);
  EXPECT_EQ("javascript:void(0);", [delegate_ childURL].spec());
  EXPECT_EQ("http://test/", [delegate_ openerURL].spec());
  EXPECT_FALSE([delegate_ initiatedByUser]);
};

// Tests page title changes.
typedef web::WebTestWithWebState CRWWebControllerTitleTest;
TEST_F(CRWWebControllerTitleTest, TitleChange) {
  // Observes and waits for TitleWasSet call.
  class TitleObserver : public web::WebStateObserver {
   public:
    explicit TitleObserver(web::WebState* web_state)
        : web::WebStateObserver(web_state) {}
    // Returns number of times |TitleWasSet| was called.
    int title_change_count() { return title_change_count_; }
    // WebStateObserver overrides:
    void TitleWasSet() override { title_change_count_++; }

   private:
    int title_change_count_ = 0;
  };

  TitleObserver observer(web_state());
  ASSERT_EQ(0, observer.title_change_count());

  // Expect TitleWasSet callback after the page is loaded.
  LoadHtml(@"<title>Title1</title>");
  EXPECT_EQ("Title1", base::UTF16ToUTF8(web_state()->GetTitle()));
  EXPECT_EQ(1, observer.title_change_count());

  // Expect at least one more TitleWasSet callback after changing title via
  // JavaScript. On iOS 10 WKWebView fires 3 callbacks after JS excucution
  // with the following title changes: "Title2", "" and "Title2".
  // TODO(crbug.com/696104): There should be only 2 calls of TitleWasSet.
  // Fix expecteation when WKWebView stops sending extra KVO calls.
  ExecuteJavaScript(@"window.document.title = 'Title2';");
  EXPECT_EQ("Title2", base::UTF16ToUTF8(web_state()->GetTitle()));
  EXPECT_GE(observer.title_change_count(), 2);
};

// Fixture class to test WKWebView crashes.
class CRWWebControllerWebProcessTest : public web::WebTestWithWebController {
 protected:
  void SetUp() override {
    web::WebTestWithWebController::SetUp();
    webView_.reset([web::BuildTerminatedWKWebView() retain]);
    base::scoped_nsobject<TestWebViewContentView> webViewContentView(
        [[TestWebViewContentView alloc]
            initWithMockWebView:webView_
                     scrollView:[webView_ scrollView]]);
    [web_controller() injectWebViewContentView:webViewContentView];

    // This test intentionally crashes the render process.
    SetIgnoreRenderProcessCrashesDuringTesting(true);
  }
  base::scoped_nsobject<WKWebView> webView_;
};

// Tests that WebStateDelegate::RenderProcessGone is called when WKWebView web
// process has crashed.
TEST_F(CRWWebControllerWebProcessTest, Crash) {
  // Observes and waits for RenderProcessGone call.
  class RenderProcessGoneObserver : public web::WebStateObserver {
   public:
    explicit RenderProcessGoneObserver(web::WebState* web_state)
        : web::WebStateObserver(web_state) {}
    void WaitForRenderProcessGone() const {
      base::test::ios::WaitUntilCondition(^{
        return render_process_gone_;
      });
    }
    // WebStateObserver overrides:
    void RenderProcessGone() override { render_process_gone_ = true; }

   private:
    bool render_process_gone_ = false;
  };

  RenderProcessGoneObserver observer(web_state());
  web::SimulateWKWebViewCrash(webView_);
  observer.WaitForRenderProcessGone();

  EXPECT_FALSE([web_controller() isViewAlive]);
};

}  // namespace
