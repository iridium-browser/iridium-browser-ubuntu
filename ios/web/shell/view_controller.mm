// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/view_controller.h"

#include "base/mac/objc_property_releaser.h"
#import "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/net/cookies/cookie_store_ios.h"
#import "ios/net/crn_http_protocol_handler.h"
#import "ios/net/empty_nsurlcache.h"
#import "ios/web/navigation/crw_session_controller.h"
#include "ios/web/navigation/web_load_params.h"
#import "ios/web/net/crw_url_verifying_protocol_handler.h"
#include "ios/web/net/request_tracker_factory_impl.h"
#import "ios/web/net/web_http_protocol_handler_delegate.h"
#include "ios/web/public/referrer.h"
#import "ios/web/public/web_controller_factory.h"
#include "ios/web/public/web_state/web_state.h"
#include "ios/web/public/web_view_creation_util.h"
#include "ios/web/shell/shell_browser_state.h"
#include "ios/web/web_state/ui/crw_web_controller.h"
#include "ios/web/web_state/web_state_impl.h"
#include "ui/base/page_transition_types.h"

namespace {
// Returns true if WKWebView should be used instead of UIWebView.
// TODO(stuartmorgan): Decide on a better way to control this.
bool UseWKWebView() {
#if defined(FORCE_ENABLE_WKWEBVIEW)
  return web::IsWKWebViewSupported();
#else
  return false;
#endif
}
}

@interface ViewController () {
  web::BrowserState* _browserState;
  base::scoped_nsobject<CRWWebController> _webController;
  scoped_ptr<web::RequestTrackerFactoryImpl> _requestTrackerFactory;
  scoped_ptr<web::WebHTTPProtocolHandlerDelegate> _httpProtocolDelegate;

  base::mac::ObjCPropertyReleaser _propertyReleaser_ViewController;
}
@property(nonatomic, readwrite, retain) UITextField* field;
@end

@implementation ViewController

@synthesize field = _field;
@synthesize containerView = _containerView;
@synthesize toolbarView = _toolbarView;

- (instancetype)initWithBrowserState:(web::BrowserState*)browserState {
  self = [super initWithNibName:@"MainView" bundle:nil];
  if (self) {
    _propertyReleaser_ViewController.Init(self, [ViewController class]);
    _browserState = browserState;
  }
  return self;
}

- (void)dealloc {
  net::HTTPProtocolHandlerDelegate::SetInstance(nullptr);
  net::RequestTracker::SetRequestTrackerFactory(nullptr);
  [super dealloc];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Set up the toolbar buttons.
  UIButton* back = [UIButton buttonWithType:UIButtonTypeCustom];
  [back setImage:[UIImage imageNamed:@"toolbar_back"]
        forState:UIControlStateNormal];
  [back setFrame:CGRectMake(0, 0, 44, 44)];
  [back setImageEdgeInsets:UIEdgeInsetsMake(5, 5, 4, 4)];
  [back setAutoresizingMask:UIViewAutoresizingFlexibleRightMargin];
  [back addTarget:self
                action:@selector(back)
      forControlEvents:UIControlEventTouchUpInside];

  UIButton* forward = [UIButton buttonWithType:UIButtonTypeCustom];
  [forward setImage:[UIImage imageNamed:@"toolbar_forward"]
           forState:UIControlStateNormal];
  [forward setFrame:CGRectMake(44, 0, 44, 44)];
  [forward setImageEdgeInsets:UIEdgeInsetsMake(5, 5, 4, 4)];
  [forward setAutoresizingMask:UIViewAutoresizingFlexibleRightMargin];
  [forward addTarget:self
                action:@selector(forward)
      forControlEvents:UIControlEventTouchUpInside];

  base::scoped_nsobject<UITextField> field([[UITextField alloc]
      initWithFrame:CGRectMake(88, 6, CGRectGetWidth([_toolbarView frame]) - 98,
                               31)]);
  [field setDelegate:self];
  [field setBackground:[[UIImage imageNamed:@"textfield_background"]
                           resizableImageWithCapInsets:UIEdgeInsetsMake(
                                                           12, 12, 12, 12)]];
  [field setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
  [field setKeyboardType:UIKeyboardTypeWebSearch];
  [field setAutocorrectionType:UITextAutocorrectionTypeNo];
  [field setClearButtonMode:UITextFieldViewModeWhileEditing];
  self.field = field;

  [_toolbarView addSubview:back];
  [_toolbarView addSubview:forward];
  [_toolbarView addSubview:field];

  // Set up the network stack before creating the WebState.
  [self setUpNetworkStack];

  scoped_ptr<web::WebStateImpl> webState(new web::WebStateImpl(_browserState));
  webState->GetNavigationManagerImpl().InitializeSession(nil, nil, NO, 0);
  web::WebViewType webViewType =
      UseWKWebView() ? web::WK_WEB_VIEW_TYPE : web::UI_WEB_VIEW_TYPE;
  _webController.reset(web::CreateWebController(webViewType, webState.Pass()));
  [_webController setDelegate:self];
  [_webController setWebUsageEnabled:YES];

  [[_webController view] setFrame:[_containerView bounds]];
  [_containerView addSubview:[_webController view]];

  web::WebLoadParams params(GURL("https://dev.chromium.org/"));
  params.transition_type = ui::PAGE_TRANSITION_TYPED;
  [_webController loadWithParams:params];
}

- (void)setUpNetworkStack {
  // Disable the default cache.
  [NSURLCache setSharedURLCache:[EmptyNSURLCache emptyNSURLCache]];

  _httpProtocolDelegate.reset(new web::WebHTTPProtocolHandlerDelegate(
      _browserState->GetRequestContext()));
  net::HTTPProtocolHandlerDelegate::SetInstance(_httpProtocolDelegate.get());
  BOOL success = [NSURLProtocol registerClass:[CRNHTTPProtocolHandler class]];
  DCHECK(success);
  // The CRWURLVerifyingProtocolHandler is used to verify URL in the
  // CRWWebController. It must be registered after the HttpProtocolHandler
  // because handlers are called in the reverse order of declaration.
  success =
      [NSURLProtocol registerClass:[CRWURLVerifyingProtocolHandler class]];
  DCHECK(success);
  _requestTrackerFactory.reset(
      new web::RequestTrackerFactoryImpl(std::string()));
  net::RequestTracker::SetRequestTrackerFactory(_requestTrackerFactory.get());
  net::CookieStoreIOS::SetCookiePolicy(net::CookieStoreIOS::ALLOW);
}

- (void)didReceiveMemoryWarning {
  [super didReceiveMemoryWarning];
}

- (UIBarPosition)positionForBar:(id<UIBarPositioning>)bar {
  if (bar == _toolbarView) {
    return UIBarPositionTopAttached;
  }
  return UIBarPositionAny;
}

- (void)back {
  if ([_webController canGoBack]) {
    [_webController goBack];
  }
}

- (void)forward {
  if ([_webController canGoForward]) {
    [_webController goForward];
  }
}

- (BOOL)textFieldShouldReturn:(UITextField*)field {
  GURL url = GURL(base::SysNSStringToUTF8([field text]));

  // Do not try to load invalid URLs.
  if (url.is_valid()) {
    web::WebLoadParams params(url);
    params.transition_type = ui::PAGE_TRANSITION_TYPED;
    [_webController loadWithParams:params];
  }

  [field resignFirstResponder];
  [self updateToolbar];
  return YES;
}

- (void)updateToolbar {
  // Do not update the URL if the text field is currently being edited.
  if ([_field isFirstResponder]) {
    return;
  }

  const GURL& url = [_webController webStateImpl]->GetVisibleURL();
  [_field setText:base::SysUTF8ToNSString(url.spec())];
}

// -----------------------------------------------------------------------
#pragma mark Bikeshedding Implementation

// Overridden to allow this view controller to receive motion events by being
// first responder when no other views are.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (void)motionEnded:(UIEventSubtype)motion withEvent:(UIEvent*)event {
  if (event.subtype == UIEventSubtypeMotionShake) {
    [self updateToolbarColor];
  }
}

- (void)updateToolbarColor {
  // Cycle through the following set of colors:
  NSArray* colors = @[
    // Vanilla Blue.
    [UIColor colorWithRed:0.337 green:0.467 blue:0.988 alpha:1.0],
    // Vanilla Red.
    [UIColor colorWithRed:0.898 green:0.110 blue:0.137 alpha:1.0],
    // Blue Grey.
    [UIColor colorWithRed:0.376 green:0.490 blue:0.545 alpha:1.0],
    // Brown.
    [UIColor colorWithRed:0.475 green:0.333 blue:0.282 alpha:1.0],
    // Purple.
    [UIColor colorWithRed:0.612 green:0.153 blue:0.690 alpha:1.0],
    // Teal.
    [UIColor colorWithRed:0.000 green:0.737 blue:0.831 alpha:1.0],
    // Deep Orange.
    [UIColor colorWithRed:1.000 green:0.341 blue:0.133 alpha:1.0],
    // Indigo.
    [UIColor colorWithRed:0.247 green:0.318 blue:0.710 alpha:1.0],
    // Vanilla Green.
    [UIColor colorWithRed:0.145 green:0.608 blue:0.141 alpha:1.0],
    // Pinkerton.
    [UIColor colorWithRed:0.914 green:0.118 blue:0.388 alpha:1.0],
  ];

  NSUInteger currentIndex = [colors indexOfObject:_toolbarView.barTintColor];
  if (currentIndex == NSNotFound) {
    currentIndex = 0;
  }
  NSUInteger newIndex = currentIndex + 1;
  if (newIndex >= [colors count]) {
    // TODO(rohitrao): Out of colors!  Consider prompting the user to pick their
    // own color here.  Also consider allowing the user to choose the entire set
    // of colors or allowing the user to choose color randomization.
    newIndex = 0;
  }
  _toolbarView.barTintColor = [colors objectAtIndex:newIndex];
}

// -----------------------------------------------------------------------
// WebDelegate implementation.

- (void)webWillAddPendingURL:(const GURL&)url
                  transition:(ui::PageTransition)transition {
}
- (void)webDidAddPendingURL {
  [self updateToolbar];
}
- (void)webCancelStartLoadingRequest {
}
- (void)webDidStartLoadingURL:(const GURL&)currentUrl
          shouldUpdateHistory:(BOOL)updateHistory {
  [self updateToolbar];
}
- (void)webDidFinishWithURL:(const GURL&)url loadSuccess:(BOOL)loadSuccess {
  [self updateToolbar];
}

- (CRWWebController*)webPageOrderedOpen:(const GURL&)url
                               referrer:(const web::Referrer&)referrer
                             windowName:(NSString*)windowName
                           inBackground:(BOOL)inBackground {
  return nil;
}

- (CRWWebController*)webPageOrderedOpenBlankWithReferrer:
                         (const web::Referrer&)referrer
                                            inBackground:(BOOL)inBackground {
  return nil;
}

- (void)webPageOrderedClose {
}
- (void)goDelta:(int)delta {
}
- (void)openURLWithParams:(const web::WebState::OpenURLParams&)params {
}
- (BOOL)openExternalURL:(const GURL&)url {
  return NO;
}
- (void)presentSSLError:(const net::SSLInfo&)info
           forSSLStatus:(const web::SSLStatus&)status
            recoverable:(BOOL)recoverable
               callback:(SSLErrorCallback)shouldContinue {
}
- (void)presentSpoofingError {
}
- (void)webLoadCancelled:(const GURL&)url {
}
- (void)webDidUpdateHistoryStateWithPageURL:(const GURL&)pageUrl {
}
- (void)webController:(CRWWebController*)webController
    retrievePlaceholderOverlayImage:(void (^)(UIImage*))block {
}
- (void)webController:(CRWWebController*)webController
    onFormResubmissionForRequest:(NSURLRequest*)request
                   continueBlock:(ProceduralBlock)continueBlock
                     cancelBlock:(ProceduralBlock)cancelBlock {
}
- (void)webWillReload {
}
- (void)webWillInitiateLoadWithParams:(web::WebLoadParams&)params {
}
- (void)webDidUpdateSessionForLoadWithParams:(const web::WebLoadParams&)params
                        wasInitialNavigation:(BOOL)initialNavigation {
}
- (void)webWillFinishHistoryNavigationFromEntry:(CRWSessionEntry*)fromEntry {
}
- (void)webWillGoDelta:(int)delta {
}
- (void)webDidPrepareForGoBack {
}
- (int)downloadImageAtUrl:(const GURL&)url
            maxBitmapSize:(uint32_t)maxBitmapSize
                 callback:
                     (const web::WebState::ImageDownloadCallback&)callback {
  return -1;
}

@end
