// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabs/tab_controller.h"

#include <algorithm>
#include <cmath>

#include "base/i18n/rtl.h"
#include "base/mac/bundle_locations.h"
#import "chrome/browser/themes/theme_properties.h"
#import "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/sprite_view.h"
#import "chrome/browser/ui/cocoa/tabs/media_indicator_button_cocoa.h"
#import "chrome/browser/ui/cocoa/tabs/tab_controller_target.h"
#import "chrome/browser/ui/cocoa/tabs/tab_view.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "content/public/browser/user_metrics.h"
#import "extensions/common/extension.h"
#import "ui/base/cocoa/menu_controller.h"

@implementation TabController

@synthesize action = action_;
@synthesize loadingState = loadingState_;
@synthesize pinned = pinned_;
@synthesize target = target_;
@synthesize url = url_;

namespace TabControllerInternal {

// A C++ delegate that handles enabling/disabling menu items and handling when
// a menu command is chosen. Also fixes up the menu item label for "pin/unpin
// tab".
class MenuDelegate : public ui::SimpleMenuModel::Delegate {
 public:
  explicit MenuDelegate(id<TabControllerTarget> target, TabController* owner)
      : target_(target),
        owner_(owner) {}

  // Overridden from ui::SimpleMenuModel::Delegate
  bool IsCommandIdChecked(int command_id) const override { return false; }
  bool IsCommandIdEnabled(int command_id) const override {
    TabStripModel::ContextMenuCommand command =
        static_cast<TabStripModel::ContextMenuCommand>(command_id);
    return [target_ isCommandEnabled:command forController:owner_];
  }
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) override {
    return false;
  }
  void ExecuteCommand(int command_id, int event_flags) override {
    TabStripModel::ContextMenuCommand command =
        static_cast<TabStripModel::ContextMenuCommand>(command_id);
    [target_ commandDispatch:command forController:owner_];
  }

 private:
  id<TabControllerTarget> target_;  // weak
  TabController* owner_;  // weak, owns me
};

}  // TabControllerInternal namespace

+ (CGFloat)defaultTabHeight { return 26; }

// The min widths is the smallest number at which the right edge of the right
// tab border image is not visibly clipped.  It is a bit smaller than the sum
// of the two tab edge bitmaps because these bitmaps have a few transparent
// pixels on the side.  The selected tab width includes the close button width.
+ (CGFloat)minTabWidth { return 36; }
+ (CGFloat)minActiveTabWidth { return 52; }
+ (CGFloat)maxTabWidth { return 214; }
+ (CGFloat)pinnedTabWidth { return 58; }

- (TabView*)tabView {
  DCHECK([[self view] isKindOfClass:[TabView class]]);
  return static_cast<TabView*>([self view]);
}

- (id)init {
  if ((self = [super init])) {
    // Icon.
    // Remember the icon's frame, so that if the icon is ever removed, a new
    // one can later replace it in the proper location.
    originalIconFrame_ = NSMakeRect(19, 5, 16, 16);
    iconView_.reset([[SpriteView alloc] initWithFrame:originalIconFrame_]);
    [iconView_ setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];

    // When the icon is removed, the title expands to the left to fill the
    // space left by the icon.  When the close button is removed, the title
    // expands to the right to fill its space.  These are the amounts to expand
    // and contract the title frame under those conditions. We don't have to
    // explicilty save the offset between the title and the close button since
    // we can just get that value for the close button's frame.
    NSRect titleFrame = NSMakeRect(35, 6, 92, 14);

    // Close button.
    closeButton_.reset([[HoverCloseButton alloc] initWithFrame:
        NSMakeRect(127, 4, 18, 18)]);
    [closeButton_ setAutoresizingMask:NSViewMinXMargin];
    [closeButton_ setTarget:self];
    [closeButton_ setAction:@selector(closeTab:)];

    base::scoped_nsobject<TabView> view([[TabView alloc]
        initWithFrame:NSMakeRect(0, 0, 160, [TabController defaultTabHeight])
           controller:self
          closeButton:closeButton_]);
    [view setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
    [view setPostsFrameChangedNotifications:NO];
    [view setPostsBoundsChangedNotifications:NO];
    [view addSubview:iconView_];
    [view addSubview:closeButton_];
    [view setTitleFrame:titleFrame];
    [super setView:view];

    isIconShowing_ = YES;
    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserver:self
                      selector:@selector(themeChangedNotification:)
                          name:kBrowserThemeDidChangeNotification
                        object:nil];

    [self internalSetSelected:selected_];
  }
  return self;
}

- (void)dealloc {
  [mediaIndicatorButton_ setAnimationDoneTarget:nil withAction:nil];
  [mediaIndicatorButton_ setClickTarget:nil withAction:nil];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [[self tabView] setController:nil];
  [super dealloc];
}

// The internals of |-setSelected:| and |-setActive:| but doesn't set the
// backing variables. This updates the drawing state and marks self as needing
// a re-draw.
- (void)internalSetSelected:(BOOL)selected {
  TabView* tabView = [self tabView];
  if ([self active]) {
    [tabView setState:NSOnState];
    [tabView cancelAlert];
  } else {
    [tabView setState:selected ? NSMixedState : NSOffState];
  }
  [self updateVisibility];
  [self updateTitleColor];
}

// Called when Cocoa wants to display the context menu. Lazily instantiate
// the menu based off of the cross-platform model. Re-create the menu and
// model every time to get the correct labels and enabling.
- (NSMenu*)menu {
  contextMenuDelegate_.reset(
      new TabControllerInternal::MenuDelegate(target_, self));
  contextMenuModel_.reset(
      [target_ contextMenuModelForController:self
                                menuDelegate:contextMenuDelegate_.get()]);
  contextMenuController_.reset(
      [[MenuController alloc] initWithModel:contextMenuModel_.get()
                     useWithPopUpButtonCell:NO]);
  return [contextMenuController_ menu];
}

- (void)toggleMute:(id)sender {
  if ([[self target] respondsToSelector:@selector(toggleMute:)]) {
    [[self target] performSelector:@selector(toggleMute:)
                        withObject:[self view]];
  }
}

- (void)closeTab:(id)sender {
  using base::UserMetricsAction;

  if (mediaIndicatorButton_ && ![mediaIndicatorButton_ isHidden]) {
    if ([mediaIndicatorButton_ isEnabled]) {
      content::RecordAction(UserMetricsAction("CloseTab_MuteToggleAvailable"));
    } else if ([mediaIndicatorButton_ showingMediaState] ==
                   TAB_MEDIA_STATE_AUDIO_PLAYING) {
      content::RecordAction(UserMetricsAction("CloseTab_AudioIndicator"));
    } else {
      content::RecordAction(UserMetricsAction("CloseTab_RecordingIndicator"));
    }
  } else {
    content::RecordAction(UserMetricsAction("CloseTab_NoMediaIndicator"));
  }

  if ([[self target] respondsToSelector:@selector(closeTab:)]) {
    [[self target] performSelector:@selector(closeTab:)
                        withObject:[self view]];
  }
}

- (void)selectTab:(id)sender {
  if ([[self tabView] isClosing])
    return;
  if ([[self target] respondsToSelector:[self action]]) {
    [[self target] performSelector:[self action]
                        withObject:[self view]];
  }
}

- (void)setTitle:(NSString*)title {
  if ([[self title] isEqualToString:title])
    return;

  TabView* tabView = [self tabView];
  [tabView setTitle:title];

  if ([self pinned] && ![self active]) {
    [tabView startAlert];
  }
  [super setTitle:title];
}

- (void)setActive:(BOOL)active {
  if (active != active_) {
    active_ = active;
    [self internalSetSelected:[self selected]];
  }
}

- (BOOL)active {
  return active_;
}

- (void)setSelected:(BOOL)selected {
  if (selected_ != selected) {
    selected_ = selected;
    [self internalSetSelected:[self selected]];
  }
}

- (BOOL)selected {
  return selected_ || active_;
}

- (SpriteView*)iconView {
  return iconView_;
}

- (void)setIconView:(SpriteView*)iconView {
  [iconView_ removeFromSuperview];
  iconView_.reset([iconView retain]);

  if (iconView_)
    [[self view] addSubview:iconView_];
}

- (MediaIndicatorButton*)mediaIndicatorButton {
  return mediaIndicatorButton_;
}

- (void)setMediaState:(TabMediaState)mediaState {
  if (!mediaIndicatorButton_ && mediaState != TAB_MEDIA_STATE_NONE) {
    mediaIndicatorButton_.reset([[MediaIndicatorButton alloc] init]);
    [self updateVisibility];  // Do layout and visibility before adding subview.
    [[self view] addSubview:mediaIndicatorButton_];
    [mediaIndicatorButton_ setAnimationDoneTarget:self
                                       withAction:@selector(updateVisibility)];
    [mediaIndicatorButton_ setClickTarget:self
                               withAction:@selector(toggleMute:)];
  }
  [mediaIndicatorButton_ transitionToMediaState:mediaState];
}

- (HoverCloseButton*)closeButton {
  return closeButton_;
}

- (NSString*)toolTip {
  return [[self tabView] toolTipText];
}

- (void)setToolTip:(NSString*)toolTip {
  [[self tabView] setToolTipText:toolTip];
}

// Return a rough approximation of the number of icons we could fit in the
// tab. We never actually do this, but it's a helpful guide for determining
// how much space we have available.
- (int)iconCapacity {
  const CGFloat availableWidth = std::max<CGFloat>(
      0, NSMaxX([closeButton_ frame]) - NSMinX(originalIconFrame_));
  const CGFloat widthPerIcon = NSWidth(originalIconFrame_);
  const int kPaddingBetweenIcons = 2;
  if (availableWidth >= widthPerIcon &&
      availableWidth < (widthPerIcon + kPaddingBetweenIcons)) {
    return 1;
  }
  return availableWidth / (widthPerIcon + kPaddingBetweenIcons);
}

- (BOOL)shouldShowIcon {
  return chrome::ShouldTabShowFavicon(
      [self iconCapacity], [self pinned], [self active], iconView_ != nil,
      !mediaIndicatorButton_ ? TAB_MEDIA_STATE_NONE :
          [mediaIndicatorButton_ showingMediaState]);
}

- (BOOL)shouldShowMediaIndicator {
  return chrome::ShouldTabShowMediaIndicator(
      [self iconCapacity], [self pinned], [self active], iconView_ != nil,
      !mediaIndicatorButton_ ? TAB_MEDIA_STATE_NONE :
          [mediaIndicatorButton_ showingMediaState]);
}

- (BOOL)shouldShowCloseButton {
  return chrome::ShouldTabShowCloseButton(
      [self iconCapacity], [self pinned], [self active]);
}

- (void)setIconImage:(NSImage*)image {
  [self setIconImage:image withToastAnimation:NO];
}

- (void)setIconImage:(NSImage*)image withToastAnimation:(BOOL)animate {
  if (image == nil) {
    [self setIconView:nil];
  } else {
    if (iconView_.get() == nil) {
      base::scoped_nsobject<SpriteView> iconView([[SpriteView alloc] init]);
      [iconView setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
      [self setIconView:iconView];
    }

    [iconView_ setImage:image withToastAnimation:animate];

    if ([self pinned]) {
      NSRect appIconFrame = [iconView_ frame];
      appIconFrame.origin = originalIconFrame_.origin;

      const CGFloat tabWidth = [TabController pinnedTabWidth];

      // Center the icon.
      appIconFrame.origin.x =
          std::floor((tabWidth - NSWidth(appIconFrame)) / 2.0);
      [iconView_ setFrame:appIconFrame];
    } else {
      [iconView_ setFrame:originalIconFrame_];
    }
  }
}

- (void)updateVisibility {
  // iconView_ may have been replaced or it may be nil, so [iconView_ isHidden]
  // won't work.  Instead, the state of the icon is tracked separately in
  // isIconShowing_.
  BOOL newShowIcon = [self shouldShowIcon];

  [iconView_ setHidden:!newShowIcon];
  isIconShowing_ = newShowIcon;

  // If the tab is a pinned-tab, hide the title.
  TabView* tabView = [self tabView];
  [tabView setTitleHidden:[self pinned]];

  BOOL newShowCloseButton = [self shouldShowCloseButton];

  [closeButton_ setHidden:!newShowCloseButton];

  BOOL newShowMediaIndicator = [self shouldShowMediaIndicator];

  [mediaIndicatorButton_ setHidden:!newShowMediaIndicator];

  if (newShowMediaIndicator) {
    NSRect newFrame = [mediaIndicatorButton_ frame];
    newFrame.size = [[mediaIndicatorButton_ image] size];
    if ([self pinned]) {
      // Tab is pinned: Position the media indicator in the center.
      const CGFloat tabWidth = [TabController pinnedTabWidth];
      newFrame.origin.x = std::floor((tabWidth - NSWidth(newFrame)) / 2);
      newFrame.origin.y = NSMinY(originalIconFrame_) -
          std::floor((NSHeight(newFrame) - NSHeight(originalIconFrame_)) / 2);
    } else {
      // The Frame for the mediaIndicatorButton_ depends on whether iconView_
      // and/or closeButton_ are visible, and where they have been positioned.
      const NSRect closeButtonFrame = [closeButton_ frame];
      newFrame.origin.x = NSMinX(closeButtonFrame);
      // Position to the left of the close button when it is showing.
      if (newShowCloseButton)
        newFrame.origin.x -= NSWidth(newFrame);
      // Media indicator is centered vertically, with respect to closeButton_.
      newFrame.origin.y = NSMinY(closeButtonFrame) -
          std::floor((NSHeight(newFrame) - NSHeight(closeButtonFrame)) / 2);
    }
    [mediaIndicatorButton_ setFrame:newFrame];
    [mediaIndicatorButton_ updateEnabledForMuteToggle];
  }

  // Adjust the title view based on changes to the icon's and close button's
  // visibility.
  NSRect oldTitleFrame = [tabView titleFrame];
  NSRect newTitleFrame;
  newTitleFrame.size.height = oldTitleFrame.size.height;
  newTitleFrame.origin.y = oldTitleFrame.origin.y;

  if (newShowIcon) {
    newTitleFrame.origin.x = NSMaxX([iconView_ frame]);
  } else {
    newTitleFrame.origin.x = originalIconFrame_.origin.x;
  }

  if (newShowMediaIndicator) {
    newTitleFrame.size.width = NSMinX([mediaIndicatorButton_ frame]) -
                               newTitleFrame.origin.x;
  } else if (newShowCloseButton) {
    newTitleFrame.size.width = NSMinX([closeButton_ frame]) -
                               newTitleFrame.origin.x;
  } else {
    newTitleFrame.size.width = NSMaxX([closeButton_ frame]) -
                               newTitleFrame.origin.x;
  }

  [tabView setTitleFrame:newTitleFrame];
}

- (void)updateTitleColor {
  NSColor* titleColor = nil;
  ui::ThemeProvider* theme = [[[self view] window] themeProvider];
  if (theme && ![self selected])
    titleColor = theme->GetNSColor(ThemeProperties::COLOR_BACKGROUND_TAB_TEXT);
  // Default to the selected text color unless told otherwise.
  if (theme && !titleColor)
    titleColor = theme->GetNSColor(ThemeProperties::COLOR_TAB_TEXT);
  [[self tabView] setTitleColor:titleColor ? titleColor : [NSColor textColor]];
}

- (void)themeChangedNotification:(NSNotification*)notification {
  [self updateTitleColor];
}

// Called by the tabs to determine whether we are in rapid (tab) closure mode.
- (BOOL)inRapidClosureMode {
  if ([[self target] respondsToSelector:@selector(inRapidClosureMode)]) {
    return [[self target] performSelector:@selector(inRapidClosureMode)] ?
        YES : NO;
  }
  return NO;
}

- (void)maybeStartDrag:(NSEvent*)event forTab:(TabController*)tab {
  [[target_ dragController] maybeStartDrag:event forTab:tab];
}

@end
