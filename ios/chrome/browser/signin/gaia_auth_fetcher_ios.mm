// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/gaia_auth_fetcher_ios.h"

#import <WebKit/WebKit.h>

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/mac/scoped_block.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/signin/gaia_auth_fetcher_ios_private.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/web_view_creation_util.h"
#include "net/base/load_flags.h"
#import "net/base/mac/url_conversions.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request_status.h"

namespace {

// Whether the iOS specialization of the GaiaAuthFetcher should be used.
// TODO(bzanotti): Having this be false is a temporary solution to
// crbug.com/521521.
bool g_should_use_gaia_auth_fetcher_ios = false;

// Creates an NSURLRequest to |url| that can be loaded by a WebView from |body|
// and |headers|.
// The request is a GET if |body| is empty and a POST otherwise.
NSURLRequest* GetRequest(const std::string& body,
                         const std::string& headers,
                         const GURL& url) {
  base::scoped_nsobject<NSMutableURLRequest> request(
      [[NSMutableURLRequest alloc] initWithURL:net::NSURLWithGURL(url)]);
  net::HttpRequestHeaders request_headers;
  request_headers.AddHeadersFromString(headers);
  for (net::HttpRequestHeaders::Iterator it(request_headers); it.GetNext();) {
    [request setValue:base::SysUTF8ToNSString(it.value())
        forHTTPHeaderField:base::SysUTF8ToNSString(it.name())];
  }
  if (!body.empty()) {
    // TODO(bzanotti): HTTPBody is currently ignored in POST Request on
    // WKWebView. Add a workaround here.
    NSData* post_data =
        [base::SysUTF8ToNSString(body) dataUsingEncoding:NSUTF8StringEncoding];
    [request setHTTPBody:post_data];
    [request setHTTPMethod:@"POST"];
    DCHECK(![[request allHTTPHeaderFields] objectForKey:@"Content-Type"]);
    [request setValue:@"application/x-www-form-urlencoded"
        forHTTPHeaderField:@"Content-Type"];
  }
  return request.autorelease();
}
}

#pragma mark - GaiaAuthFetcherNavigationDelegate

@implementation GaiaAuthFetcherNavigationDelegate {
  GaiaAuthFetcherIOSBridge* bridge_;  // weak
}

- (instancetype)initWithBridge:(GaiaAuthFetcherIOSBridge*)bridge {
  self = [super init];
  if (self) {
    bridge_ = bridge;
  }
  return self;
}

#pragma mark WKNavigationDelegate

- (void)webView:(WKWebView*)webView
    didFailNavigation:(WKNavigation*)navigation
            withError:(NSError*)error {
  DVLOG(1) << "Gaia fetcher navigation failed: "
           << base::SysNSStringToUTF8(error.localizedDescription);
  bridge_->URLFetchFailure(false /* is_cancelled */);
}

- (void)webView:(WKWebView*)webView
    didFailProvisionalNavigation:(WKNavigation*)navigation
                       withError:(NSError*)error {
  DVLOG(1) << "Gaia fetcher provisional navigation failed: "
           << base::SysNSStringToUTF8(error.localizedDescription);
  bridge_->URLFetchFailure(false /* is_cancelled */);
}

- (void)webView:(WKWebView*)webView
    didFinishNavigation:(WKNavigation*)navigation {
  // A WKNavigation is an opaque object. The only way to access the body of the
  // response is via Javascript.
  [webView evaluateJavaScript:@"document.body.innerText"
            completionHandler:^(id result, NSError* error) {
              NSString* data = base::mac::ObjCCast<NSString>(result);
              if (error || !data) {
                DVLOG(1) << "Gaia fetcher extract body failed:"
                         << base::SysNSStringToUTF8(error.localizedDescription);
                bridge_->URLFetchFailure(false /* is_cancelled */);
              } else {
                bridge_->URLFetchSuccess(base::SysNSStringToUTF8(data));
              }
            }];
}

@end

#pragma mark - GaiaAuthFetcherIOSBridge

GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridge(
    GaiaAuthFetcherIOS* fetcher,
    web::BrowserState* browser_state)
    : fetch_pending_(false),
      fetcher_(fetcher),
      url_(),
      navigation_delegate_(
          [[GaiaAuthFetcherNavigationDelegate alloc] initWithBridge:this]),
      web_view_(web::CreateWKWebView(CGRectZero, browser_state)) {
  [web_view_ setNavigationDelegate:navigation_delegate_];
}

GaiaAuthFetcherIOSBridge::~GaiaAuthFetcherIOSBridge() {
  [web_view_ setNavigationDelegate:nil];
  [web_view_ stopLoading];
  web_view_.reset();
  navigation_delegate_.reset();
}

void GaiaAuthFetcherIOSBridge::Fetch(const GURL& url,
                                     const std::string& headers,
                                     const std::string& body) {
  fetch_pending_ = true;
  url_ = url;
  [web_view_ loadRequest:GetRequest(body, headers, url)];
}

void GaiaAuthFetcherIOSBridge::Cancel() {
  if (!fetch_pending_) {
    return;
  }
  [web_view_ stopLoading];
  URLFetchFailure(true /* is_cancelled */);
}

void GaiaAuthFetcherIOSBridge::URLFetchSuccess(const std::string& data) {
  if (!fetch_pending_) {
    return;
  }
  fetch_pending_ = false;
  // WKWebViewNavigationDelegate API doesn't give any way to get the HTTP
  // response code of a navigation. Default to 200 for success.
  fetcher_->FetchComplete(url_, data, net::ResponseCookies(),
                          net::URLRequestStatus(), 200);
}

void GaiaAuthFetcherIOSBridge::URLFetchFailure(bool is_cancelled) {
  if (!fetch_pending_) {
    return;
  }
  fetch_pending_ = false;
  // WKWebViewNavigationDelegate API doesn't give any way to get the HTTP
  // response code of a navigation. Default to 500 for error.
  int error = is_cancelled ? net::ERR_ABORTED : net::ERR_FAILED;
  fetcher_->FetchComplete(url_, std::string(), net::ResponseCookies(),
                          net::URLRequestStatus::FromError(error), 500);
}

#pragma mark - GaiaAuthFetcherIOS definition

GaiaAuthFetcherIOS::GaiaAuthFetcherIOS(GaiaAuthConsumer* consumer,
                                       const std::string& source,
                                       net::URLRequestContextGetter* getter,
                                       web::BrowserState* browser_state)
    : GaiaAuthFetcher(consumer, source, getter),
      bridge_(),
      browser_state_(browser_state) {
}

GaiaAuthFetcherIOS::~GaiaAuthFetcherIOS() {
}

void GaiaAuthFetcherIOS::CreateAndStartGaiaFetcher(const std::string& body,
                                                   const std::string& headers,
                                                   const GURL& gaia_gurl,
                                                   int load_flags) {
  DCHECK(!HasPendingFetch()) << "Tried to fetch two things at once!";

  bool cookies_required = !(load_flags & (net::LOAD_DO_NOT_SEND_COOKIES |
                                          net::LOAD_DO_NOT_SAVE_COOKIES));
  if (!ShouldUseGaiaAuthFetcherIOS() || !cookies_required) {
    GaiaAuthFetcher::CreateAndStartGaiaFetcher(body, headers, gaia_gurl,
                                               load_flags);
    return;
  }

  DVLOG(2) << "Gaia fetcher URL: " << gaia_gurl.spec();
  DVLOG(2) << "Gaia fetcher headers: " << headers;
  DVLOG(2) << "Gaia fetcher body: " << body;

  // The fetch requires cookies and WKWebView is being used. The only way to do
  // a network request with cookies sent and saved is by making it through a
  // WKWebView.
  SetPendingFetch(true);
  if (!bridge_) {
    bridge_.reset(new GaiaAuthFetcherIOSBridge(this, browser_state_));
  }
  bridge_->Fetch(gaia_gurl, headers, body);
}

void GaiaAuthFetcherIOS::CancelRequest() {
  if (!HasPendingFetch()) {
    return;
  }
  if (bridge_) {
    bridge_->Cancel();
  }
  GaiaAuthFetcher::CancelRequest();
}

void GaiaAuthFetcherIOS::FetchComplete(const GURL& url,
                                       const std::string& data,
                                       const net::ResponseCookies& cookies,
                                       const net::URLRequestStatus& status,
                                       int response_code) {
  DVLOG(2) << "Response " << url.spec() << ", code = " << response_code << "\n";
  DVLOG(2) << "data: " << data << "\n";
  SetPendingFetch(false);
  DispatchFetchedRequest(url, data, cookies, status, response_code);
}

void GaiaAuthFetcherIOS::SetShouldUseGaiaAuthFetcherIOSForTesting(
    bool use_gaia_fetcher_ios) {
  // TODO(bzanotti): Commenting the below line is a temporary solution to
  // crbug.com/521521.
  // g_should_use_gaia_auth_fetcher_ios = use_gaia_fetcher_ios;
}

bool GaiaAuthFetcherIOS::ShouldUseGaiaAuthFetcherIOS() {
  return experimental_flags::IsWKWebViewEnabled() &&
         g_should_use_gaia_auth_fetcher_ios;
}
