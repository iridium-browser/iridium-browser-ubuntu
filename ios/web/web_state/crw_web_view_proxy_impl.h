// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_CRW_WEB_VIEW_PROXY_IMPL_H_
#define IOS_WEB_WEB_STATE_CRW_WEB_VIEW_PROXY_IMPL_H_

#import <UIKit/UIKit.h>

#include "ios/web/public/web_state/crw_web_view_proxy.h"
#include "ios/web/public/web_state/ui/crw_content_view.h"

@class CRWWebController;

// TODO(kkhorimoto): Rename class to CRWContentViewProxyImpl.
@interface CRWWebViewProxyImpl : NSObject<CRWWebViewProxy>

// Used by CRWWebController to set the content view being managed.
// |contentView|'s scroll view property will be managed by the
// WebViewScrollViewProxy.
@property(nonatomic, weak) CRWContentView* contentView;

// TODO(justincohen): It would be better if we could use something like a
// ScrollPositionController instead of passing in all of web controller.
// crbug.com/227744
// Init with a weak reference of web controller, used for passing thru calls.
- (instancetype)initWithWebController:(CRWWebController*)webController;

@end

#endif  // IOS_WEB_WEB_STATE_CRW_WEB_VIEW_PROXY_IMPL_H_
