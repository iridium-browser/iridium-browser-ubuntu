// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_controller.h"

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/histogram_tester.h"
#import "base/test/ios/wait_util.h"
#include "base/values.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#include "ios/web/navigation/crw_session_controller.h"
#include "ios/web/navigation/crw_session_entry.h"
#include "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/public/referrer.h"
#include "ios/web/public/test/web_test_util.h"
#import "ios/web/public/web_state/crw_web_controller_observer.h"
#include "ios/web/public/web_state/url_verification_constants.h"
#include "ios/web/test/web_test.h"
#import "ios/web/test/wk_web_view_crash_utils.h"
#include "ios/web/web_state/blocked_popup_info.h"
#import "ios/web/web_state/js/crw_js_invoke_parameter_queue.h"
#import "ios/web/web_state/ui/crw_ui_web_view_web_controller.h"
#import "ios/web/web_state/ui/crw_web_controller+protected.h"
#import "ios/web/web_state/web_state_impl.h"
#import "net/base/mac/url_conversions.h"
#include "net/ssl/ssl_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "third_party/ocmock/ocmock_extensions.h"
#import "ui/base/test/ios/keyboard_appearance_listener.h"
#include "ui/base/test/ios/ui_view_test_utils.h"

using web::NavigationManagerImpl;

@interface TestWebController (PrivateTesting)
- (void)reloadInternal;
@end

@interface CRWWebController (PrivateAPI)
- (void)setPageScrollState:(const web::PageScrollState&)scrollState;
- (void)setJsMessageQueueThrottled:(BOOL)throttle;
- (void)removeDocumentLoadCommandsFromQueue;
- (GURL)updateURLForHistoryNavigationFromURL:(const GURL&)startURL
                                       toURL:(const GURL&)endURL;
- (BOOL)checkForUnexpectedURLChange;
- (void)injectEarlyInjectionScripts;
- (void)stopExpectingURLChangeIfNecessary;
@end

@implementation TestWebController (PrivateTesting)

- (void)reloadInternal {
  // Empty implementation to prevent the need to mock out a huge number of
  // calls.
}

@end

// Used to mock CRWWebDelegate methods with C++ params.
@interface MockInteractionLoader : OCMockComplexTypeHelper
// popupURL passed to webController:shouldBlockPopupWithURL:sourceURL:
// Used for testing.
@property(nonatomic, assign) GURL popupURL;
// sourceURL passed to webController:shouldBlockPopupWithURL:sourceURL:
// Used for testing.
@property(nonatomic, assign) GURL sourceURL;
// Whether or not the delegate should block popups.
@property(nonatomic, assign) BOOL blockPopups;
// A web controller that will be returned by webPageOrdered... methods.
@property(nonatomic, assign) CRWWebController* childWebController;
// Blocked popup info received in |webController:didBlockPopup:| call.
// nullptr if that delegate method was not called.
@property(nonatomic, readonly) web::BlockedPopupInfo* blockedPopupInfo;
// SSL info received in |presentSSLError:forSSLStatus:recoverable:callback:|
// call.
@property(nonatomic, readonly) net::SSLInfo SSLInfo;
// SSL status received in |presentSSLError:forSSLStatus:recoverable:callback:|
// call.
@property(nonatomic, readonly) web::SSLStatus SSLStatus;
// Recoverable flag received in
// |presentSSLError:forSSLStatus:recoverable:callback:| call.
@property(nonatomic, readonly) BOOL recoverable;
// Callback received in |presentSSLError:forSSLStatus:recoverable:callback:|
// call.
@property(nonatomic, readonly) SSLErrorCallback shouldContinueCallback;
@end

@implementation MockInteractionLoader {
  // Backs up the property with the same name.
  scoped_ptr<web::BlockedPopupInfo> _blockedPopupInfo;
}
@synthesize popupURL = _popupURL;
@synthesize sourceURL = _sourceURL;
@synthesize blockPopups = _blockPopups;
@synthesize childWebController = _childWebController;
@synthesize SSLInfo = _SSLInfo;
@synthesize SSLStatus = _SSLStatus;
@synthesize recoverable = _recoverable;
@synthesize shouldContinueCallback = _shouldContinueCallback;

typedef void (^webPageOrderedOpenBlankBlockType)(const web::Referrer&, BOOL);
typedef void (^webPageOrderedOpenBlockType)(const GURL&,
                                            const web::Referrer&,
                                            NSString*,
                                            BOOL);

- (instancetype)initWithRepresentedObject:(id)representedObject {
  self = [super initWithRepresentedObject:representedObject];
  if (self) {
    _blockPopups = YES;
  }
  return self;
}

- (CRWWebController*)webPageOrderedOpenBlankWithReferrer:
    (const web::Referrer&)referrer
                                            inBackground:(BOOL)inBackground {
  static_cast<webPageOrderedOpenBlankBlockType>([self blockForSelector:_cmd])(
      referrer, inBackground);
  return _childWebController;
}

- (CRWWebController*)webPageOrderedOpen:(const GURL&)url
                               referrer:(const web::Referrer&)referrer
                             windowName:(NSString*)windowName
                           inBackground:(BOOL)inBackground {
  static_cast<webPageOrderedOpenBlockType>([self blockForSelector:_cmd])(
      url, referrer, windowName, inBackground);
  return _childWebController;
}

typedef BOOL (^openExternalURLBlockType)(const GURL&);

- (BOOL)openExternalURL:(const GURL&)url {
  return static_cast<openExternalURLBlockType>([self blockForSelector:_cmd])(
      url);
}

- (BOOL)webController:(CRWWebController*)webController
    shouldBlockPopupWithURL:(const GURL&)popupURL
                  sourceURL:(const GURL&)sourceURL {
  self.popupURL = popupURL;
  self.sourceURL = sourceURL;
  return _blockPopups;
}

- (void)webController:(CRWWebController*)webController
        didBlockPopup:(const web::BlockedPopupInfo&)blockedPopupInfo {
  _blockedPopupInfo.reset(new web::BlockedPopupInfo(blockedPopupInfo));
}

- (web::BlockedPopupInfo*)blockedPopupInfo {
  return _blockedPopupInfo.get();
}

- (void)presentSSLError:(const net::SSLInfo&)info
           forSSLStatus:(const web::SSLStatus&)status
            recoverable:(BOOL)recoverable
               callback:(SSLErrorCallback)shouldContinue {
  _SSLInfo = info;
  _SSLStatus = status;
  _recoverable = recoverable;
  _shouldContinueCallback = shouldContinue;
}

@end

@interface CountingObserver : NSObject<CRWWebControllerObserver>

@property(nonatomic, readonly) int pageLoadedCount;
@property(nonatomic, readonly) int messageCount;
@end

@implementation CountingObserver
@synthesize pageLoadedCount = _pageLoadedCount;
@synthesize messageCount = _messageCount;

- (void)pageLoaded:(CRWWebController*)webController {
  ++_pageLoadedCount;
}

- (BOOL)handleCommand:(const base::DictionaryValue&)command
        webController:(CRWWebController*)webController
    userIsInteracting:(BOOL)userIsInteracting
            originURL:(const GURL&)originURL {
  ++_messageCount;
  return YES;
}

- (NSString*)commandPrefix {
  return @"wctest";
}

@end

namespace {

NSString* const kGetMessageQueueJavaScript =
    @"window.__gCrWeb === undefined ? '' : __gCrWeb.message.getMessageQueue()";

NSString* kCheckURLJavaScript =
    @"try{"
     "window.__gCrWeb_Verifying = true;"
     "if(!window.__gCrWeb_CachedRequest||"
     "!(window.__gCrWeb_CachedRequestDocument===window.document)){"
     "window.__gCrWeb_CachedRequest = new XMLHttpRequest();"
     "window.__gCrWeb_CachedRequestDocument = window.document;"
     "}"
     "window.__gCrWeb_CachedRequest.open('POST',"
     "'https://localhost:0/crwebiossecurity',false);"
     "window.__gCrWeb_CachedRequest.send();"
     "}catch(e){"
     "try{"
     "window.__gCrWeb_CachedRequest.open('POST',"
     "'/crwebiossecurity/b86b97a1-2ce0-44fd-a074-e2158790c98d',false);"
     "window.__gCrWeb_CachedRequest.send();"
     "}catch(e2){}"
     "}"
     "delete window.__gCrWeb_Verifying;"
     "window.location.href";

NSString* kTestURLString = @"http://www.google.com/";

NSMutableURLRequest* requestForCrWebInvokeCommandAndKey(NSString* command,
                                                        NSString* key) {
  NSString* fullCommand =
      [NSString stringWithFormat:@"[{\"command\":\"%@\"}]", command];
  NSString* escapedCommand = [fullCommand
      stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
  NSString* urlString =
      [NSString stringWithFormat:@"crwebinvoke://%@/#%@", key, escapedCommand];
  return [NSMutableURLRequest requestWithURL:[NSURL URLWithString:urlString]];
}

// Returns true if the current device is a large iPhone (6 or 6+).
bool IsIPhone6Or6Plus() {
  UIUserInterfaceIdiom idiom = [[UIDevice currentDevice] userInterfaceIdiom];
  return (idiom == UIUserInterfaceIdiomPhone &&
          CGRectGetHeight([[UIScreen mainScreen] nativeBounds]) >= 1334.0);
}

// A mixin class for testing CRWWKWebViewWebController or
// CRWUIWebViewWebController. Stubs out WebView and child CRWWebController.
template <typename WebTestT>
class WebControllerTest : public WebTestT {
 protected:
  virtual void SetUp() override {
    WebTestT::SetUp();
    mockWebView_.reset(CreateMockWebView());
    mockScrollView_.reset([[UIScrollView alloc] init]);
    [[[mockWebView_ stub] andReturn:mockScrollView_.get()] scrollView];

    id originalMockDelegate =
        [OCMockObject niceMockForProtocol:@protocol(CRWWebDelegate)];
    mockDelegate_.reset([[MockInteractionLoader alloc]
        initWithRepresentedObject:originalMockDelegate]);
    [WebTestT::webController_ setDelegate:mockDelegate_];
    [WebTestT::webController_ injectWebView:(UIView*)mockWebView_];

    NavigationManagerImpl& navigationManager =
        [WebTestT::webController_ webStateImpl]->GetNavigationManagerImpl();
    navigationManager.InitializeSession(@"name", nil, NO, 0);
    [navigationManager.GetSessionController()
          addPendingEntry:GURL("http://www.google.com/?q=foo#bar")
                 referrer:web::Referrer()
               transition:ui::PAGE_TRANSITION_TYPED
        rendererInitiated:NO];
    // Set up child CRWWebController.
    mockChildWebController_.reset([[OCMockObject
        mockForProtocol:@protocol(CRWWebControllerScripting)] retain]);
    [[[mockDelegate_ stub] andReturn:mockChildWebController_.get()]
                           webController:WebTestT::webController_
        scriptingInterfaceForWindowNamed:@"http://www.google.com/#newtab"];
  }

  virtual void TearDown() override {
    EXPECT_OCMOCK_VERIFY(mockDelegate_);
    EXPECT_OCMOCK_VERIFY(mockChildWebController_.get());
    EXPECT_OCMOCK_VERIFY(mockWebView_);
    [WebTestT::webController_ resetInjectedWebView];
    [WebTestT::webController_ setDelegate:nil];
    WebTestT::TearDown();
  }

  // Creates WebView mock.
  virtual UIView* CreateMockWebView() const = 0;

  // Simulates a load request delegate call from the web view.
  virtual void SimulateLoadRequest(NSURLRequest* request) const = 0;

  base::scoped_nsobject<UIScrollView> mockScrollView_;
  base::scoped_nsobject<id> mockWebView_;
  base::scoped_nsobject<id> mockDelegate_;
  base::scoped_nsobject<id> mockChildWebController_;
};

class CRWUIWebViewWebControllerTest
    : public WebControllerTest<web::UIWebViewWebTest> {
 protected:
  UIView* CreateMockWebView() const override {
    id result = [[OCMockObject mockForClass:[UIWebView class]] retain];
    [[[result stub] andReturn:nil] request];
    [[result stub] setDelegate:OCMOCK_ANY];  // Called by resetInjectedWebView
    // Stub out the injection process.
    [[[result stub] andReturn:@"object"]
        stringByEvaluatingJavaScriptFromString:
            @"try { typeof __gCrWeb; } catch (e) { 'undefined'; }"];
    [[[result stub] andReturn:@"object"]
        stringByEvaluatingJavaScriptFromString:@"try { typeof "
                                               @"__gCrWeb.windowIdObject; } "
                                               @"catch (e) { 'undefined'; }"];
    return result;
  }
  void SimulateLoadRequest(NSURLRequest* request) const override {
    id<UIWebViewDelegate> delegate =
        static_cast<id<UIWebViewDelegate>>(webController_.get());
    [delegate webView:(UIWebView*)mockWebView_
        shouldStartLoadWithRequest:request
                    navigationType:UIWebViewNavigationTypeLinkClicked];
  }
};

class CRWWKWebViewWebControllerTest
    : public WebControllerTest<web::WKWebViewWebTest> {
 protected:
  void SetUp() override {
    CR_TEST_REQUIRES_WK_WEB_VIEW();
    WebControllerTest<web::WKWebViewWebTest>::SetUp();
  }
  UIView* CreateMockWebView() const override {
    id result = [[OCMockObject mockForClass:[WKWebView class]] retain];

    // Called by resetInjectedWebView
    [[result stub] configuration];
    [[result stub] setNavigationDelegate:OCMOCK_ANY];
    [[result stub] setUIDelegate:OCMOCK_ANY];
    [[result stub] addObserver:webController_
                    forKeyPath:OCMOCK_ANY
                       options:0
                       context:nullptr];
    [[result stub] addObserver:OCMOCK_ANY
                    forKeyPath:@"scrollView.backgroundColor"
                       options:0
                       context:nullptr];

    [[result stub] removeObserver:webController_ forKeyPath:OCMOCK_ANY];
    [[result stub] removeObserver:OCMOCK_ANY
                       forKeyPath:@"scrollView.backgroundColor"];

    return result;
  }
  void SimulateLoadRequest(NSURLRequest* request) const override {
    // TODO(eugenebut): implement this method.
  }
};

TEST_F(CRWUIWebViewWebControllerTest, CrWebInvokeWithIncorrectKey) {
  NSURLRequest* request = [NSURLRequest
      requestWithURL:[NSURL URLWithString:@"crwebinvoke:invalidkey#commands"]];

  SimulateLoadRequest(request);
}

TEST_F(CRWUIWebViewWebControllerTest, CrWebInvokeWithLargeQueue) {
  // Pre-define test window id.
  [webController_ setWindowId:@"12345678901234567890123456789012"];
  NSString* valid_key = [webController_ windowId];
  [[[mockWebView_ stub] andReturn:kTestURLString]
      stringByEvaluatingJavaScriptFromString:kCheckURLJavaScript];

  // Install an observer to handle custom command messages.
  base::scoped_nsobject<CountingObserver> observer(
      [[CountingObserver alloc] init]);
  [webController_ addObserver:observer];

  // Queue a lot of messages.
  [webController_ setJsMessageQueueThrottled:YES];
  const int kNumQueuedMessages = 1000;
  for (int i = 0; i < kNumQueuedMessages; ++i) {
    NSMutableURLRequest* request =
        requestForCrWebInvokeCommandAndKey(@"wctest.largequeue", valid_key);
    [request setMainDocumentURL:[NSURL URLWithString:kTestURLString]];
    SimulateLoadRequest(request);
  }

  // Verify the queue still works and all messages are delivered.
  [webController_ setJsMessageQueueThrottled:NO];
  EXPECT_EQ(kNumQueuedMessages, [observer messageCount]);

  [webController_ removeObserver:observer];
}

TEST_F(CRWUIWebViewWebControllerTest,
       CrWebInvokeWithAllMessagesFromCurrentWindow) {
  // Pre-define test window id.
  [webController_ setWindowId:@"12345678901234567890123456789012"];
  NSString* valid_key = [webController_ windowId];
  [[[mockWebView_ stub] andReturn:kTestURLString]
      stringByEvaluatingJavaScriptFromString:kCheckURLJavaScript];

  // Install an observer to handle custom command messages.
  base::scoped_nsobject<CountingObserver> observer(
      [[CountingObserver alloc] init]);
  [webController_ addObserver:observer];

  // Queue messages.
  [webController_ setJsMessageQueueThrottled:YES];
  NSMutableURLRequest* request =
      requestForCrWebInvokeCommandAndKey(@"wctest.currentwindow1", valid_key);
  [request setMainDocumentURL:[NSURL URLWithString:kTestURLString]];
  SimulateLoadRequest(request);
  request =
      requestForCrWebInvokeCommandAndKey(@"wctest.currentwindow2", valid_key);
  [request setMainDocumentURL:[NSURL URLWithString:kTestURLString]];
  SimulateLoadRequest(request);

  // Verify the behavior.
  [webController_ setJsMessageQueueThrottled:NO];
  EXPECT_EQ(2, [observer messageCount]);

  [webController_ removeObserver:observer];
}

TEST_F(CRWUIWebViewWebControllerTest,
       CrWebInvokeWithMessagesFromDifferentWindows) {
  // Pre-define test window id.
  [webController_ setWindowId:@"DEADBEEFDEADBEEFDEADBEEFDEADBEEF"];
  NSString* valid_key = [webController_ windowId];
  [[[mockWebView_ stub] andReturn:kTestURLString]
      stringByEvaluatingJavaScriptFromString:kCheckURLJavaScript];

  // Install an observer to handle custom command messages.
  base::scoped_nsobject<CountingObserver> observer(
      [[CountingObserver alloc] init]);
  [webController_ addObserver:observer];

  // Queue messages.
  [webController_ setJsMessageQueueThrottled:YES];
  NSMutableURLRequest* request =
      requestForCrWebInvokeCommandAndKey(@"wctest.diffwindow1", valid_key);
  [request setMainDocumentURL:[NSURL URLWithString:kTestURLString]];
  SimulateLoadRequest(request);

  // Second queued message will come with a new window id.
  [webController_ setWindowId:@"12345678901234567890123456789012"];
  valid_key = [webController_ windowId];
  request =
      requestForCrWebInvokeCommandAndKey(@"wctest.diffwindow2", valid_key);
  [request setMainDocumentURL:[NSURL URLWithString:kTestURLString]];
  SimulateLoadRequest(request);

  [webController_ setJsMessageQueueThrottled:NO];
  EXPECT_EQ(1, [observer messageCount]);

  [webController_ removeObserver:observer];
}

TEST_F(CRWUIWebViewWebControllerTest, CrWebInvokeWithSameOrigin) {
  // Pre-define test window id.
  [webController_ setWindowId:@"12345678901234567890123456789012"];
  NSString* valid_key = [webController_ windowId];
  [[[mockWebView_ stub] andReturn:@"http://www.google.com/foo"]
      stringByEvaluatingJavaScriptFromString:kCheckURLJavaScript];

  // Install an observer to handle custom command messages.
  base::scoped_nsobject<CountingObserver> observer(
      [[CountingObserver alloc] init]);
  [webController_ addObserver:observer];

  // Queue message.
  [webController_ setJsMessageQueueThrottled:YES];
  NSMutableURLRequest* request =
      requestForCrWebInvokeCommandAndKey(@"wctest.sameorigin", valid_key);
  // Make originURL different from currentURL but keep the origin the same.
  [request
      setMainDocumentURL:[NSURL URLWithString:@"http://www.google.com/bar"]];
  SimulateLoadRequest(request);
  // Verify the behavior.
  [webController_ setJsMessageQueueThrottled:NO];
  EXPECT_EQ(1, [observer messageCount]);

  [webController_ removeObserver:observer];
}

TEST_F(CRWUIWebViewWebControllerTest, CrWebInvokeWithDifferentOrigin) {
  // Pre-define test window id.
  [webController_ setWindowId:@"12345678901234567890123456789012"];
  NSString* valid_key = [webController_ windowId];
  [[[mockWebView_ stub] andReturn:@"http://www.google.com/"]
      stringByEvaluatingJavaScriptFromString:kCheckURLJavaScript];

  // Install an observer to handle custom command messages.
  base::scoped_nsobject<CountingObserver> observer(
      [[CountingObserver alloc] init]);
  [webController_ addObserver:observer];

  // Queue message.
  [webController_ setJsMessageQueueThrottled:YES];
  NSMutableURLRequest* request =
      requestForCrWebInvokeCommandAndKey(@"wctest.difforigin", valid_key);
  // Make originURL have a different scheme from currentURL so that the origin
  // is different.
  [request setMainDocumentURL:[NSURL URLWithString:@"https://www.google.com/"]];
  SimulateLoadRequest(request);
  // Verify the behavior.
  [webController_ setJsMessageQueueThrottled:NO];
  EXPECT_EQ(0, [observer messageCount]);

  [webController_ removeObserver:observer];
}

TEST_F(CRWUIWebViewWebControllerTest, EmptyMessageQueue) {
  [[[mockWebView_ stub] andReturn:@"[]"]
      stringByEvaluatingJavaScriptFromString:kGetMessageQueueJavaScript];

  NSURLRequest* request =
      [NSURLRequest requestWithURL:[NSURL URLWithString:@"chrome:"]];

  SimulateLoadRequest(request);
};

TEST_F(CRWUIWebViewWebControllerTest, WindowOpenBlankURL) {
  NSString* messageQueueJSON = @"[{"
                                "\"command\" : \"window.open\", "
                                "\"target\" : \"newtab\", "
                                "\"url\" : \"\", "
                                "\"referrerPolicy\" : \"default\" }]";

  SEL selector =
      @selector(webPageOrderedOpen:referrer:windowName:inBackground:);
  [mockDelegate_ onSelector:selector
       callBlockExpectation:^(const GURL& url, const web::Referrer& referrer,
                              NSString* windowName, BOOL inBackground) {
         EXPECT_EQ(url, GURL("about:blank"));
         EXPECT_EQ(referrer.url, GURL("http://www.google.com?q=foo#bar"));
         EXPECT_NSEQ(windowName, @"http://www.google.com/#newtab");
         EXPECT_FALSE(inBackground);
       }];
  [[[mockWebView_ stub] andReturn:messageQueueJSON]
      stringByEvaluatingJavaScriptFromString:kGetMessageQueueJavaScript];
  [[[mockWebView_ stub] andReturn:kTestURLString]
      stringByEvaluatingJavaScriptFromString:kCheckURLJavaScript];

  NSMutableURLRequest* request =
      [NSMutableURLRequest requestWithURL:[NSURL URLWithString:@"chrome:"]];
  [request setMainDocumentURL:[NSURL URLWithString:kTestURLString]];
  [webController_ touched:YES];
  SimulateLoadRequest(request);

  LoadCommands(messageQueueJSON, net::GURLWithNSURL(request.mainDocumentURL),
               YES);
}

TEST_F(CRWUIWebViewWebControllerTest, WindowOpenWithInteraction) {
  NSString* messageQueueJSON = @"[{"
                                "\"command\" : \"window.open\", "
                                "\"target\" : \"newtab\", "
                                "\"url\" : \"http://chromium.org\", "
                                "\"referrerPolicy\" : \"default\" }]";

  SEL selector =
      @selector(webPageOrderedOpen:referrer:windowName:inBackground:);
  [mockDelegate_ onSelector:selector
       callBlockExpectation:^(const GURL& url, const web::Referrer& referrer,
                              NSString* windowName, BOOL inBackground) {
         EXPECT_EQ(url, GURL("http://chromium.org"));
         EXPECT_EQ(referrer.url, GURL("http://www.google.com?q=foo#bar"));
         EXPECT_NSEQ(windowName, @"http://www.google.com/#newtab");
         EXPECT_FALSE(inBackground);
       }];
  [[[mockWebView_ stub] andReturn:messageQueueJSON]
      stringByEvaluatingJavaScriptFromString:kGetMessageQueueJavaScript];
  [[[mockWebView_ stub] andReturn:kTestURLString]
      stringByEvaluatingJavaScriptFromString:kCheckURLJavaScript];

  NSMutableURLRequest* request =
      [NSMutableURLRequest requestWithURL:[NSURL URLWithString:@"chrome:"]];
  [request setMainDocumentURL:[NSURL URLWithString:kTestURLString]];
  [webController_ touched:YES];
  SimulateLoadRequest(request);

  LoadCommands(messageQueueJSON, net::GURLWithNSURL(request.mainDocumentURL),
               YES);
};

TEST_F(CRWUIWebViewWebControllerTest, WindowOpenWithFinishingInteraction) {
  NSString* messageQueueJSON = @"[{"
                                "\"command\" : \"window.open\", "
                                "\"target\" : \"newtab\", "
                                "\"url\" : \"http://chromium.org\", "
                                "\"referrerPolicy\" : \"default\" }]";

  SEL selector =
      @selector(webPageOrderedOpen:referrer:windowName:inBackground:);
  [mockDelegate_ onSelector:selector
       callBlockExpectation:^(const GURL& url, const web::Referrer& referrer,
                              NSString* windowName, BOOL inBackground) {
         EXPECT_EQ(url, GURL("http://chromium.org"));
         EXPECT_EQ(referrer.url, GURL("http://www.google.com?q=foo#bar"));
         EXPECT_NSEQ(windowName, @"http://www.google.com/#newtab");
         EXPECT_FALSE(inBackground);
       }];
  [[[mockWebView_ stub] andReturn:kTestURLString]
      stringByEvaluatingJavaScriptFromString:kCheckURLJavaScript];

  NSMutableURLRequest* request =
      [NSMutableURLRequest requestWithURL:[NSURL URLWithString:@"chrome:"]];
  [request setMainDocumentURL:[NSURL URLWithString:kTestURLString]];
  [webController_ touched:YES];
  [webController_ touched:NO];
  SimulateLoadRequest(request);

  LoadCommands(messageQueueJSON, net::GURLWithNSURL(request.mainDocumentURL),
               YES);
};

TEST_F(CRWUIWebViewWebControllerTest, WindowOpenWithoutInteraction) {
  NSString* messageQueueJSON = @"[{"
                                "\"command\" : \"window.open\", "
                                "\"target\" : \"newtab\", "
                                "\"url\" : \"http://chromium.org\", "
                                "\"referrerPolicy\" : \"default\" }]";
  [[[mockWebView_ stub] andReturn:kTestURLString]
      stringByEvaluatingJavaScriptFromString:kCheckURLJavaScript];

  NSMutableURLRequest* request =
      [NSMutableURLRequest requestWithURL:[NSURL URLWithString:@"chrome:"]];
  [request setMainDocumentURL:[NSURL URLWithString:kTestURLString]];
  SimulateLoadRequest(request);

  LoadCommands(messageQueueJSON, net::GURLWithNSURL(request.mainDocumentURL),
               NO);

  MockInteractionLoader* delegate = (MockInteractionLoader*)mockDelegate_;
  EXPECT_EQ("http://www.google.com/?q=foo#bar", [delegate sourceURL].spec());
  EXPECT_EQ("http://chromium.org/", [delegate popupURL].spec());

  EXPECT_TRUE([delegate blockedPopupInfo]);
};

TEST_F(CRWUIWebViewWebControllerTest, WindowClose) {
  NSString* messageQueueJSON = @"[{"
                                "\"command\" : \"window.close\", "
                                "\"target\" : \"newtab\" }]";

  [[mockChildWebController_ expect] orderClose];

  NSMutableURLRequest* request =
      [NSMutableURLRequest requestWithURL:[NSURL URLWithString:@"chrome:"]];
  [request setMainDocumentURL:[NSURL URLWithString:kTestURLString]];
  [[[mockWebView_ stub] andReturn:kTestURLString]
      stringByEvaluatingJavaScriptFromString:kCheckURLJavaScript];

  SimulateLoadRequest(request);

  LoadCommands(messageQueueJSON, net::GURLWithNSURL(request.mainDocumentURL),
               YES);
};

TEST_F(CRWUIWebViewWebControllerTest, WindowStop) {
  NSString* messageQueueJSON = @"[{"
                                "\"command\" : \"window.stop\", "
                                "\"target\" : \"newtab\" }]";

  [[mockChildWebController_ expect] stopLoading];

  NSMutableURLRequest* request =
      [NSMutableURLRequest requestWithURL:[NSURL URLWithString:@"chrome:"]];
  [request setMainDocumentURL:[NSURL URLWithString:kTestURLString]];
  [[[mockWebView_ stub] andReturn:kTestURLString]
      stringByEvaluatingJavaScriptFromString:kCheckURLJavaScript];

  SimulateLoadRequest(request);

  LoadCommands(messageQueueJSON, net::GURLWithNSURL(request.mainDocumentURL),
               YES);
};

TEST_F(CRWUIWebViewWebControllerTest, DocumentWrite) {
  NSString* messageQueueJSON = @"[{"
                                "\"command\" : \"window.document.write\", "
                                "\"target\" : \"newtab\", "
                                "\"html\" : \"<html></html>\" }]";

  [[mockChildWebController_ expect] loadHTML:@"<html></html>"];
  [[[mockWebView_ stub] andReturn:kTestURLString]
      stringByEvaluatingJavaScriptFromString:kCheckURLJavaScript];

  NSMutableURLRequest* request =
      [NSMutableURLRequest requestWithURL:[NSURL URLWithString:@"chrome:"]];
  [request setMainDocumentURL:[NSURL URLWithString:kTestURLString]];
  SimulateLoadRequest(request);

  LoadCommands(messageQueueJSON, net::GURLWithNSURL(request.mainDocumentURL),
               YES);
};

TEST_F(CRWUIWebViewWebControllerTest, UnicodeEncoding) {
  base::scoped_nsobject<UIWebView> testWebView(
      [[UIWebView alloc] initWithFrame:CGRectZero]);
  NSArray* unicodeArray = @[
    @"ascii",
    @"unicode £´∑∂∆˚√˜ß∂",
    @"Â˜Ç¯˜Â‚·´ÎÔ´„ÅÒ",
    @"adª™£nÎÍlansdn",
    @"אבדלמצש",
    @"صسخبئغفىي",
    @"ऒतॲहड़६ॼ",
  ];
  for (NSString* unicodeString in unicodeArray) {
    NSString* encodeJS =
        [NSString stringWithFormat:@"encodeURIComponent('%@');", unicodeString];
    NSString* encodedString =
        [testWebView stringByEvaluatingJavaScriptFromString:encodeJS];
    NSString* decodedString = [encodedString
        stringByReplacingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
    EXPECT_NSEQ(unicodeString, decodedString);
  }
};

// Tests the removal of document.loaded and document.present commands in the
// CRWJSInvokeParameterQueue. Only applicable for UIWebView.
TEST_F(CRWUIWebViewWebControllerTest, PageLoadCommandRemoval) {
  [[[mockWebView_ stub] andReturn:@"[]"]
      stringByEvaluatingJavaScriptFromString:kGetMessageQueueJavaScript];
  [[[mockWebView_ stub] andReturn:kTestURLString]
      stringByEvaluatingJavaScriptFromString:kCheckURLJavaScript];

  // Pre-define test window id.
  [webController_ setWindowId:@"12345678901234567890123456789012"];
  NSString* valid_key = [webController_ windowId];

  // Add some commands to the queue.
  [webController_ setJsMessageQueueThrottled:YES];
  NSURLRequest* request =
      requestForCrWebInvokeCommandAndKey(@"document.present", valid_key);
  SimulateLoadRequest(request);
  request = requestForCrWebInvokeCommandAndKey(@"document.loaded", valid_key);
  SimulateLoadRequest(request);
  request =
      requestForCrWebInvokeCommandAndKey(@"window.history.forward", valid_key);
  SimulateLoadRequest(request);

  // Check the queue size before and after removing document load commands.
  NSUInteger expectedBeforeCount = 3;
  NSUInteger expectedAfterCount = 1;
  ASSERT_EQ(expectedBeforeCount,
            [[static_cast<CRWUIWebViewWebController*>(webController_)
                    jsInvokeParameterQueue] queueLength]);
  [webController_ removeDocumentLoadCommandsFromQueue];
  ASSERT_EQ(expectedAfterCount,
            [[static_cast<CRWUIWebViewWebController*>(webController_)
                    jsInvokeParameterQueue] queueLength]);
  [webController_ setJsMessageQueueThrottled:NO];
};

#define MAKE_URL(url_string) GURL([url_string UTF8String])

WEB_TEST_F(CRWUIWebViewWebControllerTest,
           CRWWKWebViewWebControllerTest,
           URLForHistoryNavigation) {
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

  // No start fragment: the end url is never changed.
  for (NSString* start in urlsNoFragments) {
    for (NSString* end in urlsWithFragments) {
      EXPECT_EQ(MAKE_URL(end),
                [this->webController_
                    updateURLForHistoryNavigationFromURL:MAKE_URL(start)
                                                   toURL:MAKE_URL(end)]);
    }
  }
  // Both contain fragments: the end url is never changed.
  for (NSString* start in urlsWithFragments) {
    for (NSString* end in urlsWithFragments) {
      EXPECT_EQ(MAKE_URL(end),
                [this->webController_
                    updateURLForHistoryNavigationFromURL:MAKE_URL(start)
                                                   toURL:MAKE_URL(end)]);
    }
  }
  for (unsigned start_index = 0; start_index < [urlsWithFragments count];
       ++start_index) {
    NSString* start = urlsWithFragments[start_index];
    for (unsigned end_index = 0; end_index < [urlsNoFragments count];
         ++end_index) {
      NSString* end = urlsNoFragments[end_index];
      if (start_index / 2 != end_index) {
        // The URLs have nothing in common, they are left untouched.
        EXPECT_EQ(MAKE_URL(end),
                  [this->webController_
                      updateURLForHistoryNavigationFromURL:MAKE_URL(start)
                                                     toURL:MAKE_URL(end)]);
      } else {
        // Start contains a fragment and matches end: An empty fragment is
        // added.
        EXPECT_EQ(MAKE_URL([end stringByAppendingString:@"#"]),
                  [this->webController_
                      updateURLForHistoryNavigationFromURL:MAKE_URL(start)
                                                     toURL:MAKE_URL(end)]);
      }
    }
  }
}

// This test requires iOS net stack.
#if !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)
// Tests that presentSSLError:forSSLStatus:recoverable:callback: is called with
// correct arguments if WKWebView fails to load a page with bad SSL cert.
TEST_F(CRWWKWebViewWebControllerTest, SSLError) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  ASSERT_FALSE([mockDelegate_ SSLInfo].is_valid());

  NSError* error =
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorServerCertificateHasUnknownRoot
                      userInfo:nil];
  [static_cast<id<WKNavigationDelegate>>(webController_.get()) webView:nil
                                          didFailProvisionalNavigation:nil
                                                             withError:error];

  // Verify correctness of delegate's method arguments.
  EXPECT_TRUE([mockDelegate_ SSLInfo].is_valid());
  EXPECT_EQ(net::CERT_STATUS_INVALID, [mockDelegate_ SSLInfo].cert_status);
  EXPECT_EQ(net::CERT_STATUS_INVALID, [mockDelegate_ SSLStatus].cert_status);
  EXPECT_EQ(web::SECURITY_STYLE_AUTHENTICATION_BROKEN,
            [mockDelegate_ SSLStatus].security_style);
  EXPECT_FALSE([mockDelegate_ recoverable]);
  EXPECT_FALSE([mockDelegate_ shouldContinueCallback]);
}
#endif  // !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)

// None of the |CRWUIWebViewWebControllerTest| setup is needed;
typedef web::UIWebViewWebTest CRWUIWebControllerPageDialogsOpenPolicyTest;

// None of the |CRWWKWebViewWebControllerTest| setup is needed;
typedef web::WKWebViewWebTest CRWWKWebControllerPageDialogsOpenPolicyTest;

WEB_TEST_F(CRWUIWebControllerPageDialogsOpenPolicyTest,
           CRWWKWebControllerPageDialogsOpenPolicyTest,
           SuppressPolicy) {
  this->LoadHtml(@"<html><body></body></html>");
  id delegate = [OCMockObject niceMockForProtocol:@protocol(CRWWebDelegate)];
  [[delegate expect] webControllerDidSuppressDialog:this->webController_];

  [this->webController_ setDelegate:delegate];
  [this->webController_ setPageDialogOpenPolicy:web::DIALOG_POLICY_SUPPRESS];
  this->RunJavaScript(@"alert('')");

  this->WaitForBackgroundTasks();
  EXPECT_OCMOCK_VERIFY(delegate);
  [this->webController_ setDelegate:nil];
};

// A separate test class, as none of the |CRWUIWebViewWebControllerTest| setup
// is needed;
class CRWUIWebControllerPageScrollStateTest : public web::UIWebViewWebTest {
 protected:
  // Returns a web::PageScrollState that will scroll a UIWebView to
  // |scrollOffset| and zoom the content by |relativeZoomScale|.
  inline web::PageScrollState CreateTestScrollState(
      CGPoint scroll_offset,
      CGFloat relative_zoom_scale,
      CGFloat original_minimum_zoom_scale,
      CGFloat original_maximum_zoom_scale,
      CGFloat original_zoom_scale) const {
    return web::PageScrollState(
        scroll_offset.x, scroll_offset.y,
        original_minimum_zoom_scale / relative_zoom_scale,
        original_maximum_zoom_scale / relative_zoom_scale, 1.0);
  }
};

// A separate test class, as none of the |CRWUIWebViewWebControllerTest| setup
// is needed;
class CRWWKWebControllerPageScrollStateTest : public web::WKWebViewWebTest {
 protected:
  // Returns a web::PageScrollState that will scroll a WKWebView to
  // |scrollOffset| and zoom the content by |relativeZoomScale|.
  inline web::PageScrollState CreateTestScrollState(
      CGPoint scroll_offset,
      CGFloat relative_zoom_scale,
      CGFloat original_minimum_zoom_scale,
      CGFloat original_maximum_zoom_scale,
      CGFloat original_zoom_scale) const {
    return web::PageScrollState(
        scroll_offset.x, scroll_offset.y, original_minimum_zoom_scale,
        original_maximum_zoom_scale,
        relative_zoom_scale * original_minimum_zoom_scale);
  }
};

WEB_TEST_F(CRWUIWebControllerPageScrollStateTest,
           CRWWKWebControllerPageScrollStateTest,
           SetPageStateWithUserScalableDisabled) {
#if !TARGET_IPHONE_SIMULATOR
  // This test fails flakily on device with WKWebView, so skip it there.
  // crbug.com/453530
  if ([this->webController_ webViewType] == web::WK_WEB_VIEW_TYPE)
    return;
#endif
  this->LoadHtml(@"<html><head>"
                  "<meta name='viewport' content="
                  "'width=device-width,maximum-scale=5,initial-scale=1.0,"
                  "user-scalable=no'"
                  " /></head><body></body></html>");
  UIScrollView* scrollView =
      [[[this->webController_ view] subviews][0] scrollView];
  float originZoomScale = scrollView.zoomScale;
  float originMinimumZoomScale = scrollView.minimumZoomScale;
  float originMaximumZoomScale = scrollView.maximumZoomScale;

  web::PageScrollState scrollState =
      this->CreateTestScrollState(CGPointMake(1.0, 1.0),  // scroll offset
                                  3.0,                    // relative zoom scale
                                  1.0,   // original minimum zoom scale
                                  5.0,   // original maximum zoom scale
                                  1.0);  // original zoom scale
  [this->webController_ setPageScrollState:scrollState];

  // setPageState: is async; wait for its completion.
  scrollView = [[[this->webController_ view] subviews][0] scrollView];
  base::test::ios::WaitUntilCondition(^bool() {
    return [scrollView contentOffset].x == 1.0f;
  });

  ASSERT_EQ(originZoomScale, scrollView.zoomScale);
  ASSERT_EQ(originMinimumZoomScale, scrollView.minimumZoomScale);
  ASSERT_EQ(originMaximumZoomScale, scrollView.maximumZoomScale);
};

WEB_TEST_F(CRWUIWebControllerPageScrollStateTest,
           CRWWKWebControllerPageScrollStateTest,
           SetPageStateWithUserScalableEnabled) {
  this->LoadHtml(@"<html><head>"
                  "<meta name='viewport' content="
                  "'width=device-width,maximum-scale=10,initial-scale=1.0'"
                  " /></head><body>Test</body></html>");

  ui::test::uiview_utils::ForceViewRendering([this->webController_ view]);
  web::PageScrollState scrollState =
      this->CreateTestScrollState(CGPointMake(1.0, 1.0),  // scroll offset
                                  3.0,                    // relative zoom scale
                                  1.0,   // original minimum zoom scale
                                  10.0,  // original maximum zoom scale
                                  1.0);  // original zoom scale
  [this->webController_ setPageScrollState:scrollState];

  // setPageState: is async; wait for its completion.
  id webView = [[this->webController_ view] subviews][0];
  UIScrollView* scrollView = [webView scrollView];
  base::test::ios::WaitUntilCondition(^bool() {
    return [scrollView contentOffset].x == 1.0f;
  });

  EXPECT_FLOAT_EQ(3, scrollView.zoomScale / scrollView.minimumZoomScale);
};

WEB_TEST_F(CRWUIWebControllerPageScrollStateTest,
           CRWWKWebControllerPageScrollStateTest,
           AtTop) {
  // This test fails on iPhone 6/6+ with WKWebView; skip until it's fixed.
  // crbug.com/453105
  if ([this->webController_ webViewType] == web::WK_WEB_VIEW_TYPE &&
      IsIPhone6Or6Plus())
    return;

  this->LoadHtml(@"<html><head>"
                  "<meta name='viewport' content="
                  "'width=device-width,maximum-scale=5.0,initial-scale=1.0'"
                  " /></head><body>Test</body></html>");
  ASSERT_TRUE(this->webController_.get().atTop);

  web::PageScrollState scrollState =
      this->CreateTestScrollState(CGPointMake(0.0, 30.0),  // scroll offset
                                  5.0,   // relative zoom scale
                                  1.0,   // original minimum zoom scale
                                  5.0,   // original maximum zoom scale
                                  1.0);  // original zoom scale
  [this->webController_ setPageScrollState:scrollState];

  // setPageState: is async; wait for its completion.
  id webView = [[this->webController_ view] subviews][0];
  base::test::ios::WaitUntilCondition(^bool() {
    return [[webView scrollView] contentOffset].y == 30.0f;
  });

  ASSERT_FALSE([this->webController_ atTop]);
};

// Tests that evaluateJavaScript:completionHandler: properly forwards the
// call to UIWebView.
TEST_F(CRWUIWebViewWebControllerTest, JavaScriptEvaluation) {
  NSString* kTestScript = @"script";
  NSString* kTestResult = @"result";
  NSString* scriptWithIDCheck =
      [webController_ scriptByAddingWindowIDCheckForScript:kTestScript];
  [[[mockWebView_ stub] andReturn:kTestResult]
      stringByEvaluatingJavaScriptFromString:scriptWithIDCheck];

  __block BOOL completionBlockWasCalled = NO;
  [webController_ evaluateJavaScript:kTestScript
                 stringResultHandler:^(NSString* string, NSError* error) {
                   completionBlockWasCalled = YES;
                   EXPECT_NSEQ(kTestResult, string);
                   EXPECT_EQ(nil, error);
                 }];

  // Wait until JavaScript is evaluated.
  base::test::ios::WaitUntilCondition(^bool() {
    return completionBlockWasCalled;
  });
  EXPECT_TRUE(completionBlockWasCalled);
};

TEST_F(CRWUIWebViewWebControllerTest, POSTRequestCache) {
  GURL url("http://www.google.fr/");
  NSString* mixedCaseCookieHeaderName = @"cOoKiE";
  NSString* otherHeaderName = @"Myheader";
  NSString* otherHeaderValue = @"A";
  NSString* otherHeaderIncorrectValue = @"C";

  scoped_ptr<web::NavigationItemImpl> item(new web::NavigationItemImpl());
  item->SetURL(url);
  item->SetTransitionType(ui::PAGE_TRANSITION_FORM_SUBMIT);
  item->set_is_renderer_initiated(true);
  base::scoped_nsobject<CRWSessionEntry> currentEntry(
      [[CRWSessionEntry alloc] initWithNavigationItem:item.Pass()]);
  base::scoped_nsobject<NSMutableURLRequest> request(
      [[NSMutableURLRequest alloc] initWithURL:net::NSURLWithGURL(url)]);
  [request setHTTPMethod:@"POST"];
  [request setValue:otherHeaderValue forHTTPHeaderField:otherHeaderName];
  [request setValue:@"B" forHTTPHeaderField:mixedCaseCookieHeaderName];
  // No data is cached initially.
  EXPECT_EQ(nil, [currentEntry navigationItemImpl]->GetPostData());
  EXPECT_EQ(nil, [currentEntry navigationItem]->GetHttpRequestHeaders());
  // Streams are not cached.
  [request setHTTPBodyStream:[NSInputStream inputStreamWithData:[NSData data]]];
  [webController_ cachePOSTDataForRequest:request inSessionEntry:currentEntry];
  EXPECT_EQ(nil, [currentEntry navigationItemImpl]->GetPostData());
  EXPECT_EQ(nil, [currentEntry navigationItem]->GetHttpRequestHeaders());
  [request setHTTPBodyStream:nil];
  // POST Data is cached. Cookie headers are stripped, no matter their case.
  base::scoped_nsobject<NSData> body([[NSData alloc] init]);
  [request setHTTPBody:body];
  [webController_ cachePOSTDataForRequest:request inSessionEntry:currentEntry];
  EXPECT_EQ(body.get(), [currentEntry navigationItemImpl]->GetPostData());
  EXPECT_NSEQ([currentEntry navigationItem]->GetHttpRequestHeaders(),
              @{otherHeaderName : otherHeaderValue});
  // A new request will not change the cached version.
  base::scoped_nsobject<NSData> body2([[NSData alloc] init]);
  [request setValue:otherHeaderIncorrectValue
      forHTTPHeaderField:otherHeaderName];
  [request setHTTPBody:body2];
  EXPECT_EQ(body.get(), [currentEntry navigationItemImpl]->GetPostData());
  EXPECT_NSEQ([currentEntry navigationItem]->GetHttpRequestHeaders(),
              @{otherHeaderName : otherHeaderValue});
}

class WebControllerJSEvaluationTest : public CRWUIWebViewWebControllerTest {
 protected:
  void SetUp() override {
    WebControllerTest::SetUp();

    NSString* kTestResult = @"result";
    completionBlockWasCalled_ = NO;
    NSString* scriptWithIDCheck =
        [webController_ scriptByAddingWindowIDCheckForScript:GetTestScript()];
    [[[mockWebView_ stub] andReturn:kTestResult]
        stringByEvaluatingJavaScriptFromString:scriptWithIDCheck];
    completionHandler_.reset([^(NSString* string, NSError* error) {
      completionBlockWasCalled_ = YES;
      EXPECT_NSEQ(kTestResult, string);
      EXPECT_EQ(nil, error);
    } copy]);
  }
  NSString* GetTestScript() const { return @"script"; }
  base::scoped_nsprotocol<web::JavaScriptCompletion> completionHandler_;
  BOOL completionBlockWasCalled_;
};

// Tests that -evaluateJavaScript:stringResultHandler: properly forwards
// the call to the UIWebView.
TEST_F(WebControllerJSEvaluationTest, JavaScriptEvaluation) {
  [webController_ evaluateJavaScript:GetTestScript()
                 stringResultHandler:completionHandler_];
  // Wait until JavaScript is evaluated.
  base::test::ios::WaitUntilCondition(^bool() {
    return this->completionBlockWasCalled_;
  });
  EXPECT_TRUE(completionBlockWasCalled_);
}

// Tests that -evaluateUserJavaScript:stringResultHandler: properly
// forwards the call to the UIWebView.
TEST_F(WebControllerJSEvaluationTest, UserJavaScriptEvaluation) {
  __block BOOL method_called = NO;
  [[[mockWebView_ stub] andDo:^(NSInvocation*) {
    method_called = YES;
  }]
      performSelectorOnMainThread:@selector(
                                      stringByEvaluatingJavaScriptFromString:)
                       withObject:GetTestScript()
                    waitUntilDone:NO];
  [webController_ evaluateUserJavaScript:GetTestScript()];
  EXPECT_TRUE(method_called);
}

// Tests that -evaluateJavaScript:stringResultHandler: does not crash
// on a nil completionHandler.
TEST_F(WebControllerJSEvaluationTest, JavaScriptEvaluationNilHandler) {
  [webController_ evaluateJavaScript:GetTestScript() stringResultHandler:nil];
}

// Real UIWebView is required for JSEvaluationTest.
typedef web::UIWebViewWebTest CRWUIWebControllerJSEvaluationTest;

// Real WKWebView is required for JSEvaluationTest.
typedef web::WKWebViewWebTest CRWWKWebControllerJSEvaluationTest;

// Tests that a script correctly evaluates to string.
WEB_TEST_F(CRWUIWebControllerJSEvaluationTest,
           CRWWKWebControllerJSEvaluationTest,
           Evaluation) {
  this->LoadHtml(@"<p></p>");
  EXPECT_NSEQ(@"true", this->EvaluateJavaScriptAsString(@"true"));
  EXPECT_NSEQ(@"false", this->EvaluateJavaScriptAsString(@"false"));
}

// Tests that a script is not evaluated on windowID mismatch.
WEB_TEST_F(CRWUIWebControllerJSEvaluationTest,
           CRWWKWebControllerJSEvaluationTest,
           WindowIDMissmatch) {
  this->LoadHtml(@"<p></p>");
  // Script is evaluated since windowID is matched.
  this->EvaluateJavaScriptAsString(@"window.test1 = '1';");
  EXPECT_NSEQ(@"1", this->EvaluateJavaScriptAsString(@"window.test1"));

  // Change windowID.
  this->EvaluateJavaScriptAsString(@"__gCrWeb['windowId'] = '';");

  // Script is not evaluated because of windowID mismatch.
  this->EvaluateJavaScriptAsString(@"window.test2 = '2';");
  EXPECT_NSEQ(@"", this->EvaluateJavaScriptAsString(@"window.test2"));
}

// A separate test class is used for testing the keyboard dismissal, as none of
// the setup in |CRWUIWebViewWebControllerTest| is needed; keyboard appearance
// is tracked by the KeyboardAppearanceListener.
class WebControllerKeyboardTest : public web::UIWebViewWebTest {
 protected:
  void SetUp() override {
    UIWebViewWebTest::SetUp();
    // Close any outstanding alert boxes.
    ui::test::uiview_utils::CancelAlerts();

    // Sets up the listener for keyboard activation/deactivation notifications.
    keyboardListener_.reset([[KeyboardAppearanceListener alloc] init]);
  }

  base::scoped_nsobject<KeyboardAppearanceListener> keyboardListener_;
};

TEST_F(WebControllerKeyboardTest, DismissKeyboard) {
  LoadHtml(@"<html><head></head><body><form><input id=\"textField\" /></form>"
           @"</body></html>");

  // Get the webview.
  UIWebView* webView =
      (UIWebView*)[[[webController_ view] subviews] objectAtIndex:0];
  EXPECT_TRUE(webView);

  // Create the window and add the webview.
  base::scoped_nsobject<UIWindow> window(
      [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]]);
  [window makeKeyAndVisible];
  [window addSubview:webView];

  // Set the flag to allow focus() in code in UIWebView.
  EXPECT_TRUE([webView keyboardDisplayRequiresUserAction]);
  [webView setKeyboardDisplayRequiresUserAction:NO];
  EXPECT_FALSE([webView keyboardDisplayRequiresUserAction]);

  // Set the focus on the textField.
  EXPECT_FALSE([keyboardListener_ isKeyboardVisible]);
  [webView stringByEvaluatingJavaScriptFromString:
               @"document.getElementById(\"textField\").focus();"];
  base::test::ios::WaitUntilCondition(^bool() {
    return [keyboardListener_ isKeyboardVisible];
  });
  EXPECT_TRUE([keyboardListener_ isKeyboardVisible]);

  // Method to test.
  [webController_ dismissKeyboard];

  base::test::ios::WaitUntilCondition(^bool() {
    return ![keyboardListener_ isKeyboardVisible];
  });
  EXPECT_FALSE([keyboardListener_ isKeyboardVisible]);
}

TEST_F(CRWWKWebViewWebControllerTest, WebURLWithTrustLevel) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  [[[mockWebView_ stub] andReturn:[NSURL URLWithString:kTestURLString]] URL];
#if !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)
  [[[mockWebView_ stub] andReturnBool:NO] hasOnlySecureContent];
#endif

  // Stub out the injection process.
  [[mockWebView_ stub] evaluateJavaScript:OCMOCK_ANY
                        completionHandler:OCMOCK_ANY];

  // Simulate registering load request to avoid failing page load simulation.
  [webController_ simulateLoadRequestWithURL:GURL([kTestURLString UTF8String])];
  // Simulate a page load to trigger a URL update.
  [static_cast<id<WKNavigationDelegate>>(webController_.get())
                  webView:mockWebView_
      didCommitNavigation:nil];

  web::URLVerificationTrustLevel trust_level = web::kNone;
  GURL gurl = [webController_ currentURLWithTrustLevel:&trust_level];

  EXPECT_EQ(gurl, GURL(base::SysNSStringToUTF8(kTestURLString)));
  EXPECT_EQ(web::kAbsolute, trust_level);
}

// A separate test class, as none of the |CRWUIWebViewWebControllerTest| setup
// is needed;
typedef web::UIWebViewWebTest CRWUIWebControllerObserversTest;
typedef web::WKWebViewWebTest CRWWKWebControllerObserversTest;

// Tests that CRWWebControllerObservers are called.
WEB_TEST_F(CRWUIWebControllerObserversTest,
           CRWWKWebControllerObserversTest,
           Observers) {
  base::scoped_nsobject<CountingObserver> observer(
      [[CountingObserver alloc] init]);
  CRWWebController* web_controller = this->webController_;
  EXPECT_EQ(0u, [web_controller observerCount]);
  [web_controller addObserver:observer];
  EXPECT_EQ(1u, [web_controller observerCount]);

  EXPECT_EQ(0, [observer pageLoadedCount]);
  [web_controller webStateImpl]->OnPageLoaded(GURL("http://test"), false);
  EXPECT_EQ(0, [observer pageLoadedCount]);
  [web_controller webStateImpl]->OnPageLoaded(GURL("http://test"), true);
  EXPECT_EQ(1, [observer pageLoadedCount]);

  EXPECT_EQ(0, [observer messageCount]);
  // Non-matching prefix.
  EXPECT_FALSE([web_controller webStateImpl]->OnScriptCommandReceived(
      "a", base::DictionaryValue(), GURL("http://test"), true));
  EXPECT_EQ(0, [observer messageCount]);
  // Matching prefix.
  EXPECT_TRUE([web_controller webStateImpl]->OnScriptCommandReceived(
      base::SysNSStringToUTF8([observer commandPrefix]) + ".foo",
      base::DictionaryValue(), GURL("http://test"), true));
  EXPECT_EQ(1, [observer messageCount]);

  [web_controller removeObserver:observer];
  EXPECT_EQ(0u, [web_controller observerCount]);
};

// Test fixture for window.open tests.
class CRWWKWebControllerWindowOpenTest : public web::WKWebViewWebTest {
 protected:
  void SetUp() override {
    CR_TEST_REQUIRES_WK_WEB_VIEW();
    WKWebViewWebTest::SetUp();

    // Configure web delegate.
    delegate_.reset([[MockInteractionLoader alloc]
        initWithRepresentedObject:
            [OCMockObject niceMockForProtocol:@protocol(CRWWebDelegate)]]);
    ASSERT_TRUE([delegate_ blockPopups]);
    [webController_ setDelegate:delegate_];

    // Configure child web controller.
    child_.reset(CreateWebController());
    [child_ setWebUsageEnabled:YES];
    [delegate_ setChildWebController:child_];

    // Configure child web controller's session controller mock.
    id sessionController =
        [OCMockObject niceMockForClass:[CRWSessionController class]];
    BOOL yes = YES;
    [[[sessionController stub] andReturnValue:OCMOCK_VALUE(yes)] isOpenedByDOM];
    [child_ webStateImpl]->GetNavigationManagerImpl().SetSessionController(
        sessionController);

    LoadHtml(@"<html><body></body></html>");
  }
  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(delegate_);
    [webController_ setDelegate:nil];
    [child_ close];

    WKWebViewWebTest::TearDown();
  }
  // Executes JavaScript that opens a new window and returns evaluation result
  // as a string.
  NSString* OpenWindowByDOM() {
    NSString* const kOpenWindowScript =
        @"var w = window.open('javascript:void(0);', target='_blank');"
         "w.toString();";
    NSString* windowJSObject = EvaluateJavaScriptAsString(kOpenWindowScript);
    WaitForBackgroundTasks();
    return windowJSObject;
  }
  // A CRWWebDelegate mock used for testing.
  base::scoped_nsobject<id> delegate_;
  // A child CRWWebController used for testing.
  base::scoped_nsobject<CRWWebController> child_;
};

// Tests that absence of web delegate is handled gracefully.
TEST_F(CRWWKWebControllerWindowOpenTest, NoDelegate) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  [webController_ setDelegate:nil];

  EXPECT_NSEQ(@"", OpenWindowByDOM());

  EXPECT_FALSE([delegate_ blockedPopupInfo]);
}

// Tests that window.open triggered by user gesture opens a new non-popup
// window.
TEST_F(CRWWKWebControllerWindowOpenTest, OpenWithUserGesture) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  SEL selector = @selector(webPageOrderedOpenBlankWithReferrer:inBackground:);
  [delegate_ onSelector:selector
      callBlockExpectation:^(const web::Referrer& referrer,
                             BOOL in_background) {
        EXPECT_EQ("", referrer.url.spec());
        EXPECT_FALSE(in_background);
      }];

  [webController_ touched:YES];
  EXPECT_NSEQ(@"[object Window]", OpenWindowByDOM());
  EXPECT_FALSE([delegate_ blockedPopupInfo]);
}

// Tests that window.open executed w/o user gesture does not open a new window.
// Once the blocked popup is allowed a new window is opened.
TEST_F(CRWWKWebControllerWindowOpenTest, AllowPopup) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  SEL selector =
      @selector(webPageOrderedOpen:referrer:windowName:inBackground:);
  [delegate_ onSelector:selector
      callBlockExpectation:^(const GURL& new_window_url,
                             const web::Referrer& referrer,
                             NSString* obsoleted_window_name,
                             BOOL in_background) {
        EXPECT_EQ("javascript:void(0);", new_window_url.spec());
        EXPECT_EQ("", referrer.url.spec());
        EXPECT_FALSE(in_background);
      }];

  ASSERT_FALSE([webController_ userIsInteracting]);
  EXPECT_NSEQ(@"", OpenWindowByDOM());
  base::test::ios::WaitUntilCondition(^bool() {
    return [delegate_ blockedPopupInfo];
  });

  if ([delegate_ blockedPopupInfo])
    [delegate_ blockedPopupInfo]->ShowPopup();

  EXPECT_EQ("", [delegate_ sourceURL].spec());
  EXPECT_EQ("javascript:void(0);", [delegate_ popupURL].spec());
}

// Tests that window.open executed w/o user gesture opens a new window, assuming
// that delegate allows popups.
TEST_F(CRWWKWebControllerWindowOpenTest, DontBlockPopup) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  [delegate_ setBlockPopups:NO];
  SEL selector = @selector(webPageOrderedOpenBlankWithReferrer:inBackground:);
  [delegate_ onSelector:selector
      callBlockExpectation:^(const web::Referrer& referrer,
                             BOOL in_background) {
        EXPECT_EQ("", referrer.url.spec());
        EXPECT_FALSE(in_background);
      }];

  EXPECT_NSEQ(@"[object Window]", OpenWindowByDOM());
  EXPECT_FALSE([delegate_ blockedPopupInfo]);

  EXPECT_EQ("", [delegate_ sourceURL].spec());
  EXPECT_EQ("javascript:void(0);", [delegate_ popupURL].spec());
}

// Tests that window.open executed w/o user gesture does not open a new window.
TEST_F(CRWWKWebControllerWindowOpenTest, BlockPopup) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  ASSERT_FALSE([webController_ userIsInteracting]);
  EXPECT_NSEQ(@"", OpenWindowByDOM());
  base::test::ios::WaitUntilCondition(^bool() {
    return [delegate_ blockedPopupInfo];
  });

  EXPECT_EQ("", [delegate_ sourceURL].spec());
  EXPECT_EQ("javascript:void(0);", [delegate_ popupURL].spec());
};

// Fixture class to test WKWebView crashes.
class CRWWKWebControllerWebProcessTest : public web::WKWebViewWebTest {
 protected:
  void SetUp() override {
    CR_TEST_REQUIRES_WK_WEB_VIEW();
    WKWebViewWebTest::SetUp();
    webView_.reset(web::CreateTerminatedWKWebView());
    [webController_ injectWebView:webView_];
  }
  base::scoped_nsobject<WKWebView> webView_;
};

// Tests that -[CRWWebDelegate webControllerWebProcessDidCrash:] is called
// when WKWebView web process has crashed.
TEST_F(CRWWKWebControllerWebProcessTest, Crash) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  id delegate = [OCMockObject niceMockForProtocol:@protocol(CRWWebDelegate)];
  [[delegate expect] webControllerWebProcessDidCrash:this->webController_];

  [this->webController_ setDelegate:delegate];
  web::SimulateWKWebViewCrash(webView_);

  EXPECT_OCMOCK_VERIFY(delegate);
  [this->webController_ setDelegate:nil];
};

// Tests that WKWebView does not crash if deallocation happens during JavaScript
// evaluation.
typedef web::WKWebViewWebTest DeallocationDuringJSEvaluation;
TEST_F(DeallocationDuringJSEvaluation, NoCrash) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  this->LoadHtml(@"<html><body></body></html>");
  [webController_ evaluateJavaScript:@"null"
                 stringResultHandler:^(NSString* value, NSError*) {
                   // Block can not be empty in order to reproduce the crash:
                   // https://bugs.webkit.org/show_bug.cgi?id=140203
                   VLOG(1) << "Script has been flushed.";
                 }];
  // -evaluateJavaScript:stringResultHandler: is asynchronous so JavaScript
  // evaluation will not happen until TearDown, which deallocates
  // CRWWebController, which in its turn will deallocate WKWebView to create a
  // crashy condition.
};

}  // namespace
