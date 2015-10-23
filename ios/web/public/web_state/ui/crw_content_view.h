// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_CONTENT_VIEW_H_
#define IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_CONTENT_VIEW_H_

#import <UIKit/UIKit.h>

// UIViews conforming to CRWScrollableContent (i.e. CRWContentViews) are used
// to display content within a WebState.
@protocol CRWScrollableContent<NSObject>

// The scroll view used to display the content.  If |scrollView| is non-nil,
// it will be used to back the CRWContentViewScrollViewProxy and is expected to
// be a subview of the CRWContentView.
@property(nonatomic, retain, readonly) UIScrollView* scrollView;

// Returns YES if content is being displayed in the scroll view.
// TODO(stuartmorgan): See if this can be removed from the public interface.
- (BOOL)isViewAlive;

@end

// Convenience type for content views.
typedef UIView<CRWScrollableContent> CRWContentView;

#endif  // IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_CONTENT_VIEW_H_
