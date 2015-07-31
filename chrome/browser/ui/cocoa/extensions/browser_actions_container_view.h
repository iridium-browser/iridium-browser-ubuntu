// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_BROWSER_ACTIONS_CONTAINER_VIEW_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_BROWSER_ACTIONS_CONTAINER_VIEW_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "ui/base/cocoa/tracking_area.h"

// Sent when a user-initiated drag to resize the container is initiated.
extern NSString* const kBrowserActionGrippyDragStartedNotification;

// Sent when a user-initiated drag is resizing the container.
extern NSString* const kBrowserActionGrippyDraggingNotification;

// Sent when a user-initiated drag to resize the container has finished.
extern NSString* const kBrowserActionGrippyDragFinishedNotification;

// Sent when the Browser Actions container view is about to animate.
extern NSString* const kBrowserActionsContainerWillAnimate;

// Sent when the mouse enters the browser actions container (if tracking is
// enabled).
extern NSString* const kBrowserActionsContainerMouseEntered;

// Sent when a running animation has ended.
extern NSString* const kBrowserActionsContainerAnimationEnded;

// Key which is used to notify the translation with delta.
extern NSString* const kTranslationWithDelta;

class BrowserActionsContainerViewSizeDelegate {
 public:
  virtual CGFloat GetMaxAllowedWidth() = 0;
  virtual ~BrowserActionsContainerViewSizeDelegate() {}
};

// The view that encompasses the Browser Action buttons in the toolbar and
// provides mechanisms for resizing.
@interface BrowserActionsContainerView : NSView<NSAnimationDelegate> {
 @private
  // The frame encompasing the grippy used for resizing the container.
  NSRect grippyRect_;

  // Used to cache the original position within the container that initiated the
  // drag.
  NSPoint initialDragPoint_;

  // The maximum width the container could want; i.e., the width required to
  // display all the icons.
  CGFloat maxDesiredWidth_;

  // Whether the container is currently being resized by the user.
  BOOL userIsResizing_;

  // Whether the user can resize this at all. Resizing is disabled in incognito
  // mode since any changes done in incognito mode are not saved anyway, and
  // also to avoid a crash. http://crbug.com/42848
  BOOL resizable_;

  // Whether the user is allowed to drag the grippy to the left. NO if all
  // extensions are shown or the location bar has hit its minimum width (handled
  // within toolbar_controller.mm).
  BOOL canDragLeft_;

  // Whether the user is allowed to drag the grippy to the right. NO if all
  // extensions are hidden.
  BOOL canDragRight_;

  // When the left grippy is pinned, resizing the window has no effect on its
  // position. This prevents it from overlapping with other elements as well
  // as letting the container expand when the window is going from super small
  // to large.
  BOOL grippyPinned_;

  // Whether the toolbar is currently highlighting its actions (in which case it
  // is drawn with an orange background).
  BOOL isHighlighting_;

  // A tracking area to receive mouseEntered events, if tracking is enabled.
  ui::ScopedCrTrackingArea trackingArea_;

  // The size delegate, if any.
  // Weak; delegate is responsible for adding/removing itself.
  BrowserActionsContainerViewSizeDelegate* sizeDelegate_;

  base::scoped_nsobject<NSViewAnimation> resizeAnimation_;
}

// Sets whether or not tracking (for mouseEntered events) is enabled.
- (void)setTrackingEnabled:(BOOL)enabled;

// Sets whether or not the container is highlighting.
- (void)setIsHighlighting:(BOOL)isHighlighting;

// Resizes the container to the given ideal width, optionally animating.
- (void)resizeToWidth:(CGFloat)width animate:(BOOL)animate;

// Returns the frame of the container after the running animation has finished.
// If no animation is running, returns the container's current frame.
- (NSRect)animationEndFrame;

// Returns true if the view is animating.
- (BOOL)isAnimating;

// Stops any animation in progress.
- (void)stopAnimation;

@property(nonatomic) BOOL canDragLeft;
@property(nonatomic) BOOL canDragRight;
@property(nonatomic) BOOL grippyPinned;
@property(nonatomic,getter=isResizable) BOOL resizable;
@property(nonatomic) CGFloat maxDesiredWidth;
@property(readonly, nonatomic) BOOL userIsResizing;
@property(nonatomic) BrowserActionsContainerViewSizeDelegate* delegate;

@end

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_BROWSER_ACTIONS_CONTAINER_VIEW_
