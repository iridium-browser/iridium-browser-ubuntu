// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/webui/crw_web_ui_manager.h"

#include "base/mac/bind_objc_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_vector.h"
#import "base/strings/sys_string_conversions.h"
#include "base/values.h"
#import "ios/web/net/request_group_util.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/web_client.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#include "ios/web/web_state/web_state_impl.h"
#import "ios/web/webui/crw_web_ui_page_builder.h"
#include "ios/web/webui/url_fetcher_block_adapter.h"
#import "net/base/mac/url_conversions.h"

namespace {
// Prefix for history.requestFavicon JavaScript message.
const char kScriptCommandPrefix[] = "webui";
}

@interface CRWWebUIManager () <CRWWebUIPageBuilderDelegate>

// Current web state.
@property(nonatomic, readonly) web::WebStateImpl* webState;

// Composes WebUI page for webUIURL and invokes completionHandler with the
// result.
- (void)loadWebUIPageForURL:(const GURL&)webUIURL
          completionHandler:(void (^)(NSString*))completionHandler;

// Retrieves resource for URL and invokes completionHandler with the result.
- (void)fetchResourceWithURL:(const GURL&)URL
           completionHandler:(void (^)(NSData*))completionHandler;

// Handles JavaScript message from the WebUI page.
- (BOOL)handleWebUIJSMessage:(const base::DictionaryValue&)message;

// Removes favicon callback from web state.
- (void)resetWebState;

// Removes fetcher from vector of active fetchers.
- (void)removeFetcher:(web::URLFetcherBlockAdapter*)fetcher;

@end

@implementation CRWWebUIManager {
  // Set of live WebUI fetchers for retrieving data.
  ScopedVector<web::URLFetcherBlockAdapter> _fetchers;
  // Bridge to observe the web state from Objective-C.
  scoped_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  // Weak WebStateImpl this CRWWebUIManager is associated with.
  web::WebStateImpl* _webState;
}

- (instancetype)init {
  NOTREACHED();
  return self;
}

- (instancetype)initWithWebState:(web::WebStateImpl*)webState {
  if (self = [super init]) {
    _webState = webState;
    _webStateObserverBridge.reset(
        new web::WebStateObserverBridge(webState, self));
    base::WeakNSObject<CRWWebUIManager> weakSelf(self);
    _webState->AddScriptCommandCallback(
        base::BindBlock(
            ^bool(const base::DictionaryValue& message, const GURL&, bool) {
              return [weakSelf handleWebUIJSMessage:message];
            }),
        kScriptCommandPrefix);
  }
  return self;
}

- (void)dealloc {
  [self resetWebState];
  [super dealloc];
}

#pragma mark - CRWWebStateObserver Methods

- (void)webState:(web::WebState*)webState
    didStartProvisionalNavigationForURL:(const GURL&)URL {
  DCHECK(webState == _webState);
  GURL navigationURL(URL);
  // Add request group ID to the URL, if not present. Request group ID may
  // already be added if restoring state to a WebUI page.
  GURL requestURL =
      web::ExtractRequestGroupIDFromURL(net::NSURLWithGURL(URL))
          ? URL
          : net::GURLWithNSURL(web::AddRequestGroupIDToURL(
                net::NSURLWithGURL(URL), _webState->GetRequestGroupID()));
  base::WeakNSObject<CRWWebUIManager> weakSelf(self);
  [self loadWebUIPageForURL:requestURL
          completionHandler:^(NSString* HTML) {
            web::WebStateImpl* webState = [weakSelf webState];
            if (webState) {
              webState->LoadWebUIHtml(base::SysNSStringToUTF16(HTML),
                                      navigationURL);
            }
          }];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  [self resetWebState];
}

#pragma mark - CRWWebUIPageBuilderDelegate Methods

- (void)webUIPageBuilder:(CRWWebUIPageBuilder*)webUIPageBuilder
    fetchResourceWithURL:(const GURL&)resourceURL
       completionHandler:(web::WebUIDelegateCompletion)completionHandler {
  GURL URL(resourceURL);
  [self fetchResourceWithURL:URL
           completionHandler:^(NSData* data) {
             base::scoped_nsobject<NSString> resource(
                 [[NSString alloc] initWithData:data
                                       encoding:NSUTF8StringEncoding]);
             completionHandler(resource, URL);
           }];
}

#pragma mark - Private Methods

- (void)loadWebUIPageForURL:(const GURL&)webUIURL
          completionHandler:(void (^)(NSString*))handler {
  base::scoped_nsobject<CRWWebUIPageBuilder> pageBuilder(
      [[CRWWebUIPageBuilder alloc] initWithDelegate:self]);
  [pageBuilder buildWebUIPageForURL:webUIURL completionHandler:handler];
}

- (void)fetchResourceWithURL:(const GURL&)URL
           completionHandler:(void (^)(NSData*))completionHandler {
  base::WeakNSObject<CRWWebUIManager> weakSelf(self);
  web::URLFetcherBlockAdapterCompletion fetcherCompletion =
      ^(NSData* data, web::URLFetcherBlockAdapter* fetcher) {
        completionHandler(data);
        [weakSelf removeFetcher:fetcher];
      };

  _fetchers.push_back(
      [self fetcherForURL:URL completionHandler:fetcherCompletion].Pass());
  _fetchers.back()->Start();
}

- (BOOL)handleWebUIJSMessage:(const base::DictionaryValue&)message {
  std::string command;
  if (!message.GetString("message", &command) ||
      command != "webui.requestFavicon") {
    DLOG(WARNING) << "Unexpected message received" << command;
    return NO;
  }
  const base::ListValue* arguments = nullptr;
  if (!message.GetList("arguments", &arguments)) {
    DLOG(WARNING) << "JS message parameter not found: arguments";
    return NO;
  }
  std::string favicon;
  if (!arguments->GetString(0, &favicon)) {
    DLOG(WARNING) << "JS message parameter not found: Favicon URL";
    return NO;
  }
  GURL faviconURL(favicon);
  DCHECK(faviconURL.is_valid());
  // Retrieve favicon resource and set favicon background image via JavaScript.
  base::WeakNSObject<CRWWebUIManager> weakSelf(self);
  void (^faviconHandler)(NSData*) = ^void(NSData* data) {
    NSString* base64EncodedResource = [data base64EncodedStringWithOptions:0];
    NSString* dataURLString = [NSString
        stringWithFormat:@"data:image/png;base64,%@", base64EncodedResource];
    NSString* faviconURLString = base::SysUTF8ToNSString(faviconURL.spec());
    NSString* script =
        [NSString stringWithFormat:@"chrome.setFaviconBackground('%@', '%@');",
                                   faviconURLString, dataURLString];
    [weakSelf webState]->ExecuteJavaScriptAsync(
        base::SysNSStringToUTF16(script));
  };
  [self fetchResourceWithURL:faviconURL completionHandler:faviconHandler];
  return YES;
}

- (void)resetWebState {
  if (_webState) {
    _webState->RemoveScriptCommandCallback(kScriptCommandPrefix);
  }
  _webState = nullptr;
}

- (web::WebStateImpl*)webState {
  return _webState;
}

- (void)removeFetcher:(web::URLFetcherBlockAdapter*)fetcher {
  _fetchers.erase(std::find(_fetchers.begin(), _fetchers.end(), fetcher));
}

#pragma mark - Testing-Only Methods

- (scoped_ptr<web::URLFetcherBlockAdapter>)
        fetcherForURL:(const GURL&)URL
    completionHandler:(web::URLFetcherBlockAdapterCompletion)handler {
  return scoped_ptr<web::URLFetcherBlockAdapter>(
      new web::URLFetcherBlockAdapter(
          URL, _webState->GetBrowserState()->GetRequestContext(), handler));
}

@end
