// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/profiles/avatar_base_controller.h"

#include "base/mac/foundation_util.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/signin/signin_header_helper.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/profiles/avatar_menu_bubble_controller.h"
#import "chrome/browser/ui/cocoa/profiles/profile_chooser_controller.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/common/profile_management_switches.h"

// Space between the avatar icon and the avatar menu bubble.
const CGFloat kMenuYOffsetAdjust = 1.0;
// Offset needed to align the edge of the avatar bubble with the edge of the
// avatar button.
const CGFloat kMenuXOffsetAdjust = 2.0;

@interface AvatarBaseController (Private)
// Shows the avatar bubble.
- (IBAction)buttonClicked:(id)sender;
- (IBAction)buttonRightClicked:(id)sender;

- (void)bubbleWillClose:(NSNotification*)notif;

// Updates the profile name displayed by the avatar button. If |layoutParent| is
// yes, then the BrowserWindowController is notified to relayout the subviews,
// as the button needs to be repositioned.
- (void)updateAvatarButtonAndLayoutParent:(BOOL)layoutParent;

// Displays an error icon if any accounts associated with this profile have an
// auth error.
- (void)updateErrorStatus:(BOOL)hasError;
@end

class ProfileInfoUpdateObserver : public ProfileInfoCacheObserver,
                                  public SigninErrorController::Observer {
 public:
  ProfileInfoUpdateObserver(Profile* profile,
                            AvatarBaseController* avatarController)
      : profile_(profile),
        avatarController_(avatarController) {
    g_browser_process->profile_manager()->
        GetProfileInfoCache().AddObserver(this);

    // Subscribe to authentication error changes so that the avatar button
    // can update itself.
    SigninErrorController* errorController =
        profiles::GetSigninErrorController(profile_);
    if (errorController)
      errorController->AddObserver(this);
  }

  ~ProfileInfoUpdateObserver() override {
    g_browser_process->profile_manager()->
        GetProfileInfoCache().RemoveObserver(this);
    SigninErrorController* errorController =
        profiles::GetSigninErrorController(profile_);
    if (errorController)
      errorController->RemoveObserver(this);
  }

  // ProfileInfoCacheObserver:
  void OnProfileAdded(const base::FilePath& profile_path) override {
    [avatarController_ updateAvatarButtonAndLayoutParent:YES];
  }

  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const base::string16& profile_name) override {
    // If deleting the active profile, don't bother updating the avatar
    // button, as the browser window is being closed anyway.
    if (profile_->GetPath() != profile_path)
      [avatarController_ updateAvatarButtonAndLayoutParent:YES];
  }

  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const base::string16& old_profile_name) override {
    if (profile_->GetPath() == profile_path)
      [avatarController_ updateAvatarButtonAndLayoutParent:YES];
  }

  void OnProfileAvatarChanged(const base::FilePath& profile_path) override {
    if (!switches::IsNewAvatarMenu() && profile_->GetPath() == profile_path)
      [avatarController_ updateAvatarButtonAndLayoutParent:YES];
  }

  void OnProfileSupervisedUserIdChanged(
      const base::FilePath& profile_path) override {
    if (profile_->GetPath() == profile_path)
      [avatarController_ updateAvatarButtonAndLayoutParent:YES];
  }

  // SigninErrorController::Observer:
  void OnErrorChanged() override {
    SigninErrorController* errorController =
        profiles::GetSigninErrorController(profile_);
    if (errorController)
      [avatarController_ updateErrorStatus:errorController->HasError()];
  }

 private:
  Profile* profile_;
  AvatarBaseController* avatarController_;  // Weak; owns this.

  DISALLOW_COPY_AND_ASSIGN(ProfileInfoUpdateObserver);
};

@implementation AvatarBaseController

- (id)initWithBrowser:(Browser*)browser {
  if ((self = [super init])) {
    browser_ = browser;
    profileInfoObserver_.reset(
        new ProfileInfoUpdateObserver(browser_->profile(), self));
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:NSWindowWillCloseNotification
              object:[menuController_ window]];
  [super dealloc];
}

- (NSButton*)buttonView {
  CHECK(button_.get());  // Subclasses must set this.
  return button_.get();
}

- (void)showAvatarBubbleAnchoredAt:(NSView*)anchor
                          withMode:(BrowserWindow::AvatarBubbleMode)mode
                   withServiceType:(signin::GAIAServiceType)serviceType {
  if (menuController_) {
    if (switches::IsNewAvatarMenu()) {
      profiles::BubbleViewMode viewMode;
      profiles::TutorialMode tutorialMode;
      profiles::BubbleViewModeFromAvatarBubbleMode(
          mode, &viewMode, &tutorialMode);
      if (tutorialMode != profiles::TUTORIAL_MODE_NONE) {
        ProfileChooserController* profileChooserController =
            base::mac::ObjCCastStrict<ProfileChooserController>(
                menuController_);
        [profileChooserController setTutorialMode:tutorialMode];
        [profileChooserController initMenuContentsWithView:viewMode];
      }
    }
    return;
  }

  DCHECK(chrome::IsCommandEnabled(browser_, IDC_SHOW_AVATAR_MENU));

  NSWindowController* wc =
      [browser_->window()->GetNativeWindow() windowController];
  if ([wc isKindOfClass:[BrowserWindowController class]]) {
    [static_cast<BrowserWindowController*>(wc)
        lockBarVisibilityForOwner:self withAnimation:NO delay:NO];
  }

  // The new avatar bubble does not have an arrow, and it should be anchored
  // to the edge of the avatar button.
  int anchorX = switches::IsNewAvatarMenu() ?
      NSMaxX([anchor bounds]) - kMenuXOffsetAdjust :
      NSMidX([anchor bounds]);
  NSPoint point = NSMakePoint(anchorX,
                              NSMaxY([anchor bounds]) + kMenuYOffsetAdjust);
  point = [anchor convertPoint:point toView:nil];
  point = [[anchor window] convertBaseToScreen:point];

  // |menuController_| will automatically release itself on close.
  if (switches::IsNewAvatarMenu()) {
    profiles::BubbleViewMode viewMode;
    profiles::TutorialMode tutorialMode;
    profiles::BubbleViewModeFromAvatarBubbleMode(
        mode, &viewMode, &tutorialMode);
    // Don't start creating the view if it would be an empty fast user switcher.
    // It has to happen here to prevent the view system from creating an empty
    // container.
    if (viewMode == profiles::BUBBLE_VIEW_MODE_FAST_PROFILE_CHOOSER &&
        !profiles::HasProfileSwitchTargets(browser_->profile())) {
      return;
    }

    menuController_ =
        [[ProfileChooserController alloc] initWithBrowser:browser_
                                               anchoredAt:point
                                                 viewMode:viewMode
                                             tutorialMode:tutorialMode
                                              serviceType:serviceType];
  } else {
    menuController_ =
      [[AvatarMenuBubbleController alloc] initWithBrowser:browser_
                                               anchoredAt:point];
  }

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(bubbleWillClose:)
             name:NSWindowWillCloseNotification
           object:[menuController_ window]];
  [menuController_ showWindow:self];

  ProfileMetrics::LogProfileOpenMethod(ProfileMetrics::ICON_AVATAR_BUBBLE);
}

- (IBAction)buttonClicked:(id)sender {
  BrowserWindow::AvatarBubbleMode mode =
      BrowserWindow::AVATAR_BUBBLE_MODE_DEFAULT;

  [self showAvatarBubbleAnchoredAt:button_
                          withMode:mode
                   withServiceType:signin::GAIA_SERVICE_TYPE_NONE];
}

- (IBAction)buttonRightClicked:(id)sender {
  BrowserWindow::AvatarBubbleMode mode =
      BrowserWindow::AVATAR_BUBBLE_MODE_FAST_USER_SWITCH;

  [self showAvatarBubbleAnchoredAt:button_
                          withMode:mode
                   withServiceType:signin::GAIA_SERVICE_TYPE_NONE];
}

- (void)bubbleWillClose:(NSNotification*)notif {
  NSWindowController* wc =
      [browser_->window()->GetNativeWindow() windowController];
  if ([wc isKindOfClass:[BrowserWindowController class]]) {
    [static_cast<BrowserWindowController*>(wc)
        releaseBarVisibilityForOwner:self withAnimation:YES delay:NO];
  }
  menuController_ = nil;
}

- (void)updateAvatarButtonAndLayoutParent:(BOOL)layoutParent {
  NOTREACHED();
}

- (void)updateErrorStatus:(BOOL)hasError {
}

- (BaseBubbleController*)menuController {
  return menuController_;
}

@end
