// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_WEB_DELEGATE_H_
#define IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_WEB_DELEGATE_H_

#include <stdint.h>
#import <UIKit/UIKit.h>
#include <vector>

#import "base/ios/block_types.h"
#include "ios/web/public/favicon_url.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/ssl_status.h"
#import "ios/web/public/web_state/ui/crw_native_content.h"
#import "ios/web/public/web_state/web_state.h"
#include "ui/base/page_transition_types.h"

class GURL;
@class CRWSessionEntry;
@class CRWWebController;

// Methods implemented by the delegate of the CRWWebController.
// DEPRECATED, do not conform to this protocol and do not add any methods to it.
// Use web::WebStateDelegate instead.
// TODO(crbug.com/674991): Remove this protocol.
@protocol CRWWebDelegate<NSObject>

// Called when the page calls window.close() on itself. Begin the shut-down
// sequence for this controller.
- (void)webPageOrderedClose;
// Called when an external app needs to be opened, it also passes |linkClicked|
// to track if this call was a result of user action or not. Returns YES iff
// |URL| is launched in an external app.
- (BOOL)openExternalURL:(const GURL&)URL linkClicked:(BOOL)linkClicked;

// This method is invoked whenever the system believes the URL is about to
// change, or immediately after any unexpected change of the URL, prior to
// updating the navigation manager's pending entry.
// Phase will be LOAD_REQUESTED.
- (void)webWillAddPendingURL:(const GURL&)url
                  transition:(ui::PageTransition)transition;
// Called when webWillStartLoadingURL was called, but something went wrong, and
// webDidStartLoadingURL will now never be called.
- (void)webCancelStartLoadingRequest;
// Called when the page URL has changed. Phase will be PAGE_LOADING. Can be
// followed by webDidFinishWithURL or webWillStartLoadingURL.
// |updateHistory| is YES if the URL should be added to the history DB.
// TODO(crbug.com/692331): Remove this method and use |DidFinishNavigation|.
- (void)webDidStartLoadingURL:(const GURL&)url
          shouldUpdateHistory:(BOOL)updateHistory;
// Called when the page load was cancelled by page activity (before a success /
// failure state is known). Phase will be PAGE_LOADED.
- (void)webLoadCancelled:(const GURL&)url;
// Called when a placeholder image should be displayed instead of the WebView.
- (void)webController:(CRWWebController*)webController
    retrievePlaceholderOverlayImage:(void (^)(UIImage*))block;

// ---------------------------------------------------------------------
// TODO(rohitrao): Eliminate as many of the following delegate methods as
// possible.  They only exist because the Tab and CRWWebController logic was
// very intertwined. We should streamline the logic to jump between classes
// less, then remove any delegate method that becomes unneccessary as a result.

// Called when the page is reloaded.
- (void)webWillReload;
// Called when a page is loaded using loadWithParams.  In
// |webWillInitiateLoadWithParams|, the |params| argument is non-const so that
// the delegate can make changes if necessary.
// TODO(rohitrao): This is not a great API.  Clean it up.
- (void)webWillInitiateLoadWithParams:
    (web::NavigationManager::WebLoadParams&)params;
- (void)webDidUpdateSessionForLoadWithParams:
            (const web::NavigationManager::WebLoadParams&)params
                        wasInitialNavigation:(BOOL)initialNavigation;
// Called from finishHistoryNavigationFromEntry.
// TODO(crbug.com/692331): Remove this method and use |DidFinishNavigation|.
- (void)webWillFinishHistoryNavigation;
// ---------------------------------------------------------------------

// Called when |webController| wants to open a new window. |URL| is the URL of
// the new window; |openerURL| is the URL of the page which requested a window
// to be open; |initiatedByUser| is YES if action was caused by the user.
// |webController| will not open a window if this method returns nil. This
// method can not return |webController|.
- (CRWWebController*)webController:(CRWWebController*)webController
         createWebControllerForURL:(const GURL&)URL
                         openerURL:(const GURL&)openerURL
                   initiatedByUser:(BOOL)initiatedByUser;

@optional

// Called to ask CRWWebDelegate if |CRWWebController| should open the given URL.
// CRWWebDelegate can intercept the request by returning NO and processing URL
// in own way.
- (BOOL)webController:(CRWWebController*)webController
        shouldOpenURL:(const GURL&)url
      mainDocumentURL:(const GURL&)mainDocumentURL
          linkClicked:(BOOL)linkClicked;

// Called to ask if external URL should be opened. External URL is one that
// cannot be presented by CRWWebController.
- (BOOL)webController:(CRWWebController*)webController
    shouldOpenExternalURL:(const GURL&)URL;

// Called when |URL| is deemed suitable to be opened in a matching native app.
// Needs to return whether |URL| was opened in a matching native app.
// Also triggering user action |linkClicked| is passed to use it when needed.
// The return value indicates if the native app was launched, not if a native
// app was found.
// TODO(shreyasv): Instead of having the CRWWebDelegate handle an external URL,
// provide a hook/API to steal a URL navigation. That way the logic to determine
// a URL as triggering a native app launch can also be moved.
- (BOOL)urlTriggersNativeAppLaunch:(const GURL&)URL
                         sourceURL:(const GURL&)sourceURL
                       linkClicked:(BOOL)linkClicked;

// Called to ask the delegate for a controller to display the given url,
// which contained content that the UIWebView couldn't display. Returns
// the native controller to display if the delegate can handle the url,
// or nil otherwise.
- (id<CRWNativeContent>)controllerForUnhandledContentAtURL:(const GURL&)url;

// Called when CRWWebController did suppress a dialog (JavaScript, HTTP
// authentication or window.open).
// NOTE: Called only if CRWWebController.shouldSuppressDialogs is set to YES.
- (void)webControllerDidSuppressDialog:(CRWWebController*)webController;

// Called to retrieve the height of any header that is overlaying on top of the
// web view. This can be used to implement, for e.g. a toolbar that changes
// height dynamically. Returning a non-zero height affects the visible frame
// shown by the CRWWebController. 0.0 is assumed if not implemented.
- (CGFloat)headerHeightForWebController:(CRWWebController*)webController;

// Called when CRWWebController updated the SSL status for the current
// NagivationItem.
- (void)webControllerDidUpdateSSLStatusForCurrentNavigationItem:
    (CRWWebController*)webController;

// Called when a PassKit file is downloaded. |data| should be the data from a
// PassKit file, but this is not guaranteed, and the delegate is responsible for
// error handling non PassKit data using -[PKPass initWithData:error:]. If the
// download does not successfully complete, |data| will be nil.
- (void)webController:(CRWWebController*)webController
    didLoadPassKitObject:(NSData*)data;

@end

#endif  // IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_WEB_DELEGATE_H_
