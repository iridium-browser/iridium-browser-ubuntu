// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_STRIP_TAB_STRIP_CONTAINER_COORDINATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_STRIP_TAB_STRIP_CONTAINER_COORDINATOR_H_

#import <UIKit/UIKit.h>
#import "ios/clean/chrome/browser/browser_coordinator.h"

namespace web {
class WebState;
}

// Coordinator that runs a tab strip container: A composed UI consisting of a
// tab strip and a tab.
@interface TabStripContainerCoordinator : BrowserCoordinator

// The webState representing the web page that will be displayed in this tab.
// Calling code should assign this before starting this coordinator.
// This coordinator doesn't assume ownersip of |webState|.
@property(nonatomic, assign) web::WebState* webState;

// An opaque key provided by this coordinator's parent which can be passed in
// to a transition animation to synchronize the presentation with the presenting
// view controller, if any.
@property(nonatomic, copy) NSObject* presentationKey;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_STRIP_TAB_STRIP_CONTAINER_COORDINATOR_H_
