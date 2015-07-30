// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PERMISSION_BUBBLE_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_PERMISSION_BUBBLE_COCOA_H_

#import <Foundation/Foundation.h>

#include "base/gtest_prod_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/cocoa/info_bubble_view.h"
#include "chrome/browser/ui/website_settings/permission_bubble_view.h"
#include "content/public/browser/web_contents.h"

@class PermissionBubbleController;

class PermissionBubbleCocoa : public PermissionBubbleView {
 public:
  explicit PermissionBubbleCocoa(NSWindow* parent_window);
  ~PermissionBubbleCocoa() override;

  // PermissionBubbleView interface.
  void Show(const std::vector<PermissionBubbleRequest*>& requests,
            const std::vector<bool>& accept_state) override;
  void Hide() override;
  bool IsVisible() override;
  void SetDelegate(Delegate* delegate) override;
  bool CanAcceptRequestUpdate() override;

  // Called when |bubbleController_| is closing.
  void OnBubbleClosing();

  // Returns the point, in screen coordinates, to which the bubble's arrow
  // should point.
  NSPoint GetAnchorPoint();

  // Returns the NSWindow containing the bubble.
  NSWindow* window();

  // Change the parent window to be used the next time the bubble is shown.
  void SwitchParentWindow(NSWindow* parent);

  info_bubble::BubbleArrowLocation GetArrowLocation();

 private:
  FRIEND_TEST_ALL_PREFIXES(PermissionBubbleBrowserTest,
                           HasLocationBarByDefault);
  FRIEND_TEST_ALL_PREFIXES(PermissionBubbleBrowserTest,
                           FullscreenHasLocationBar);
  FRIEND_TEST_ALL_PREFIXES(PermissionBubbleAppBrowserTest, AppHasNoLocationBar);
  FRIEND_TEST_ALL_PREFIXES(PermissionBubbleKioskBrowserTest,
                           KioskHasNoLocationBar);

  NSWindow* parent_window_;  // Weak.
  Delegate* delegate_;  // Weak.

  // Cocoa-side UI controller for the bubble.  Weak, as it will close itself.
  PermissionBubbleController* bubbleController_;
  virtual bool HasLocationBar();

  DISALLOW_COPY_AND_ASSIGN(PermissionBubbleCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_PERMISSION_BUBBLE_COCOA_H_
