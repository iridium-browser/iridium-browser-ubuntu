// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_UI_SIMPLE_WEB_VIEW_CONTROLLER_H_
#define IOS_WEB_WEB_STATE_UI_CRW_UI_SIMPLE_WEB_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/web/web_state/ui/crw_simple_web_view_controller.h"

// A CRWSimpleWebViewController implentation backed by an UIWebView.
@interface CRWUISimpleWebViewController : NSObject<CRWSimpleWebViewController>

// Designated initializer. Initializes a new view controller backed by a
// UIWebView.
- (instancetype)initWithUIWebView:(UIWebView*)webView;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_UI_SIMPLE_WEB_VIEW_CONTROLLER_H_
