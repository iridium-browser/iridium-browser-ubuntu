// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_H_

// A class acting as the Objective-C controller for the Browser
// object. Handles interactions between Cocoa and the cross-platform
// code. Each window has a single toolbar and, by virtue of being a
// TabWindowController, a tab strip along the top.

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bubble_controller.h"
#import "chrome/browser/ui/cocoa/browser_command_executor.h"
#import "chrome/browser/ui/cocoa/exclusive_access_bubble_window_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_window_controller.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/url_drop_target.h"
#import "chrome/browser/ui/cocoa/view_resizer.h"
#include "components/translate/core/common/translate_errors.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/gfx/geometry/rect.h"

@class AvatarBaseController;
class Browser;
class BrowserWindow;
class BrowserWindowCocoa;
@class BrowserWindowEnterFullscreenTransition;
@class DevToolsController;
@class DownloadShelfController;
class ExtensionKeybindingRegistryCocoa;
@class FindBarCocoaController;
@class FullscreenModeController;
@class FullscreenWindow;
@class InfoBarContainerController;
class LocationBarViewMac;
@class OverlayableContentsController;
class PermissionBubbleCocoa;
@class PresentationModeController;
class StatusBubbleMac;
@class TabStripController;
@class TabStripView;
@class ToolbarController;
@class TranslateBubbleController;

namespace content {
class WebContents;
}

namespace extensions {
class Command;
}

@interface BrowserWindowController :
  TabWindowController<NSUserInterfaceValidations,
                      BookmarkBarControllerDelegate,
                      BrowserCommandExecutor,
                      ViewResizer,
                      TabStripControllerDelegate> {
 @private
  // The ordering of these members is important as it determines the order in
  // which they are destroyed. |browser_| needs to be destroyed last as most of
  // the other objects hold weak references to it or things it owns
  // (tab/toolbar/bookmark models, profiles, etc).
  scoped_ptr<Browser> browser_;
  NSWindow* savedRegularWindow_;
  scoped_ptr<BrowserWindowCocoa> windowShim_;
  base::scoped_nsobject<ToolbarController> toolbarController_;
  base::scoped_nsobject<TabStripController> tabStripController_;
  base::scoped_nsobject<FindBarCocoaController> findBarCocoaController_;
  base::scoped_nsobject<InfoBarContainerController> infoBarContainerController_;
  base::scoped_nsobject<DownloadShelfController> downloadShelfController_;
  base::scoped_nsobject<BookmarkBarController> bookmarkBarController_;
  base::scoped_nsobject<DevToolsController> devToolsController_;
  base::scoped_nsobject<OverlayableContentsController>
      overlayableContentsController_;
  base::scoped_nsobject<PresentationModeController> presentationModeController_;
  base::scoped_nsobject<ExclusiveAccessBubbleWindowController>
      exclusiveAccessBubbleWindowController_;
  base::scoped_nsobject<BrowserWindowEnterFullscreenTransition>
      enterFullscreenTransition_;

  // Strong. StatusBubble is a special case of a strong reference that
  // we don't wrap in a scoped_ptr because it is acting the same
  // as an NSWindowController in that it wraps a window that must
  // be shut down before our destructors are called.
  StatusBubbleMac* statusBubble_;

  BookmarkBubbleController* bookmarkBubbleController_;  // Weak.
  BOOL initializing_;  // YES while we are currently in initWithBrowser:
  BOOL ownsBrowser_;  // Only ever NO when testing

  TranslateBubbleController* translateBubbleController_;  // Weak.

  // The total amount by which we've grown the window up or down (to display a
  // bookmark bar and/or download shelf), respectively; reset to 0 when moved
  // away from the bottom/top or resized (or zoomed).
  CGFloat windowTopGrowth_;
  CGFloat windowBottomGrowth_;

  // YES only if we're shrinking the window from an apparent zoomed state (which
  // we'll only do if we grew it to the zoomed state); needed since we'll then
  // restrict the amount of shrinking by the amounts specified above. Reset to
  // NO on growth.
  BOOL isShrinkingFromZoomed_;

  // The view controller that manages the incognito badge or the multi-profile
  // avatar button. Depending on whether the --new-profile-management flag is
  // used, the multi-profile button can either be the avatar's icon badge or a
  // button with the profile's name. If the flag is used, the button is always
  // shown, otherwise the view will always be in the view hierarchy but will
  // be hidden unless it's appropriate to show it (i.e. if there's more than
  // one profile).
  base::scoped_nsobject<AvatarBaseController> avatarButtonController_;

  // Lazily created view which draws the background for the floating set of bars
  // in presentation mode (for window types having a floating bar; it remains
  // nil for those which don't).
  base::scoped_nsobject<NSView> floatingBarBackingView_;

  // The borderless window used in fullscreen mode when Cocoa's System
  // Fullscreen API is not being used (or not available, before OS 10.7).
  base::scoped_nsobject<NSWindow> fullscreenWindow_;

  // The Cocoa implementation of the PermissionBubbleView.
  scoped_ptr<PermissionBubbleCocoa> permissionBubbleCocoa_;

  // True between |-windowWillEnterFullScreen:| and |-windowDidEnterFullScreen:|
  // to indicate that the window is in the process of transitioning into
  // AppKit fullscreen mode.
  BOOL enteringAppKitFullscreen_;

  // True between |enterImmersiveFullscreen| and |-windowDidEnterFullScreen:|
  // to indicate that the window is in the process of transitioning into
  // AppKit fullscreen mode.
  BOOL enteringImmersiveFullscreen_;

  // True between |-setPresentationMode:url:bubbleType:| and
  // |-windowDidEnterFullScreen:| to indicate that the window is in the process
  // of transitioning into fullscreen presentation mode.
  BOOL enteringPresentationMode_;

  // When the window is in the process of entering AppKit Fullscreen, this
  // property indicates whether the window is being fullscreened on the
  // primary screen.
  BOOL enteringAppKitFullscreenOnPrimaryScreen_;

  // The size of the original (non-fullscreen) window.  This is saved just
  // before entering fullscreen mode and is only valid when |-isFullscreen|
  // returns YES.
  NSRect savedRegularWindowFrame_;

  // The proportion of the floating bar which is shown (in presentation mode).
  CGFloat floatingBarShownFraction_;

  // Various UI elements/events may want to ensure that the floating bar is
  // visible (in presentation mode), e.g., because of where the mouse is or
  // where keyboard focus is. Whenever an object requires bar visibility, it has
  // itself added to |barVisibilityLocks_|. When it no longer requires bar
  // visibility, it has itself removed.
  base::scoped_nsobject<NSMutableSet> barVisibilityLocks_;

  // Bar visibility locks and releases only result (when appropriate) in changes
  // in visible state when the following is |YES|.
  BOOL barVisibilityUpdatesEnabled_;

  // When going fullscreen for a tab, we need to store the URL and the
  // fullscreen type, since we can't show the bubble until
  // -windowDidEnterFullScreen: gets called.
  GURL fullscreenUrl_;
  ExclusiveAccessBubbleType exclusiveAccessBubbleType_;

  // The Extension Command Registry used to determine which keyboard events to
  // handle.
  scoped_ptr<ExtensionKeybindingRegistryCocoa> extension_keybinding_registry_;

  // Whether the root view of the window is layer backed.
  BOOL windowViewWantsLayer_;
}

// A convenience class method which gets the |BrowserWindowController| for a
// given window. This method returns nil if no window in the chain has a BWC.
+ (BrowserWindowController*)browserWindowControllerForWindow:(NSWindow*)window;

// A convenience class method which gets the |BrowserWindowController| for a
// given view.  This is the controller for the window containing |view|, if it
// is a BWC, or the first controller in the parent-window chain that is a
// BWC. This method returns nil if no window in the chain has a BWC.
+ (BrowserWindowController*)browserWindowControllerForView:(NSView*)view;

// Load the browser window nib and do any Cocoa-specific initialization.
// Takes ownership of |browser|.
- (id)initWithBrowser:(Browser*)browser;

// Call to make the browser go away from other places in the cross-platform
// code.
- (void)destroyBrowser;

// Ensure bounds for the window abide by the minimum window size.
- (gfx::Rect)enforceMinWindowSize:(gfx::Rect)bounds;

// Access the C++ bridge between the NSWindow and the rest of Chromium.
- (BrowserWindow*)browserWindow;

// Return a weak pointer to the toolbar controller.
- (ToolbarController*)toolbarController;

// Return a weak pointer to the tab strip controller.
- (TabStripController*)tabStripController;

// Return a weak pointer to the find bar controller.
- (FindBarCocoaController*)findBarCocoaController;

// Access the ObjC controller that contains the infobars.
- (InfoBarContainerController*)infoBarContainerController;

// Access the C++ bridge object representing the status bubble for the window.
- (StatusBubbleMac*)statusBubble;

// Access the C++ bridge object representing the location bar.
- (LocationBarViewMac*)locationBarBridge;

// Returns a weak pointer to the floating bar backing view;
- (NSView*)floatingBarBackingView;

// Returns a weak pointer to the overlayable contents controller.
- (OverlayableContentsController*)overlayableContentsController;

// Access the Profile object that backs this Browser.
- (Profile*)profile;

// Access the avatar button controller.
- (AvatarBaseController*)avatarButtonController;

// Forces the toolbar (and transitively the location bar) to update its current
// state.  If |tab| is non-NULL, we're switching (back?) to this tab and should
// restore any previous location bar state (such as user editing) as well.
- (void)updateToolbarWithContents:(content::WebContents*)tab;

// Resets the toolbar's tab state for |tab|.
- (void)resetTabState:(content::WebContents*)tab;

// Sets whether or not the current page in the frontmost tab is bookmarked.
- (void)setStarredState:(BOOL)isStarred;

// Sets whether or not the current page is translated.
- (void)setCurrentPageIsTranslated:(BOOL)on;

// Invoked via BrowserWindowCocoa::OnActiveTabChanged, happens whenever a
// new tab becomes active.
- (void)onActiveTabChanged:(content::WebContents*)oldContents
                        to:(content::WebContents*)newContents;

// Happens when the zoom level is changed in the active tab, the active tab is
// changed, or a new browser window or tab is created. |canShowBubble| denotes
// whether it would be appropriate to show a zoom bubble or not.
- (void)zoomChangedForActiveTab:(BOOL)canShowBubble;

// Return the rect, in WebKit coordinates (flipped), of the window's grow box
// in the coordinate system of the content area of the currently selected tab.
- (NSRect)selectedTabGrowBoxRect;

// Called to tell the selected tab to update its loading state.
// |force| is set if the update is due to changing tabs, as opposed to
// the page-load finishing.  See comment in reload_button_cocoa.h.
- (void)setIsLoading:(BOOL)isLoading force:(BOOL)force;

// Brings this controller's window to the front.
- (void)activate;

// Make the location bar the first responder, if possible.
- (void)focusLocationBar:(BOOL)selectAll;

// Make the (currently-selected) tab contents the first responder, if possible.
- (void)focusTabContents;

// Returns the frame of the regular (non-fullscreened) window (even if the
// window is currently in fullscreen mode).  The frame is returned in Cocoa
// coordinates (origin in bottom-left).
- (NSRect)regularWindowFrame;

// Whether or not to show the avatar, which is either the incognito guy or the
// user's profile avatar.
- (BOOL)shouldShowAvatar;

// Whether or not to show the new avatar button used by --new-profile-maagement.
- (BOOL)shouldUseNewAvatarButton;

- (BOOL)isBookmarkBarVisible;

// Returns YES if the bookmark bar is currently animating.
- (BOOL)isBookmarkBarAnimating;

- (BookmarkBarController*)bookmarkBarController;

- (DevToolsController*)devToolsController;

- (BOOL)isDownloadShelfVisible;

// Lazily creates the download shelf in visible state if it doesn't exist yet.
- (void)createAndAddDownloadShelf;

// Returns the download shelf controller, if it exists.
- (DownloadShelfController*)downloadShelf;

// Retains the given FindBarCocoaController and adds its view to this
// browser window.  Must only be called once per
// BrowserWindowController.
- (void)addFindBar:(FindBarCocoaController*)findBarCocoaController;

// The user changed the theme.
- (void)userChangedTheme;

// Executes the command in the context of the current browser.
// |command| is an integer value containing one of the constants defined in the
// "chrome/app/chrome_command_ids.h" file.
- (void)executeCommand:(int)command;

// Consults the Command Registry to see if this |event| needs to be handled as
// an extension command and returns YES if so (NO otherwise).
// Only extensions with the given |priority| are considered.
- (BOOL)handledByExtensionCommand:(NSEvent*)event
    priority:(ui::AcceleratorManager::HandlerPriority)priority;

// Delegate method for the status bubble to query its base frame.
- (NSRect)statusBubbleBaseFrame;

// Show the bookmark bubble (e.g. user just clicked on the STAR)
- (void)showBookmarkBubbleForURL:(const GURL&)url
               alreadyBookmarked:(BOOL)alreadyBookmarked;

// Show the translate bubble.
- (void)showTranslateBubbleForWebContents:(content::WebContents*)contents
                                     step:(translate::TranslateStep)step
                                errorType:
                                    (translate::TranslateErrors::Type)errorType;

// Shows or hides the docked web inspector depending on |contents|'s state.
- (void)updateDevToolsForContents:(content::WebContents*)contents;

// Gets the current theme provider.
- (ui::ThemeProvider*)themeProvider;

// Gets the window style.
- (ThemedWindowStyle)themedWindowStyle;

// Returns the position in window coordinates that the top left of a theme
// image with |alignment| should be painted at. If the window does not have a
// tab strip, the offset for THEME_IMAGE_ALIGN_WITH_FRAME is always returned.
// The result of this method can be used in conjunction with
// [NSGraphicsContext cr_setPatternPhase:] to set the offset of pattern colors.
- (NSPoint)themeImagePositionForAlignment:(ThemeImageAlignment)alignment;

// Return the point to which a bubble window's arrow should point, in window
// coordinates.
- (NSPoint)bookmarkBubblePoint;

// Called when the Add Search Engine dialog is closed.
- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(NSInteger)code
            context:(void*)context;

// Executes the command registered by the extension that has the given id.
- (void)executeExtensionCommand:(const std::string&)extension_id
                        command:(const extensions::Command&)command;

@end  // @interface BrowserWindowController


// Methods having to do with the window type (normal/popup/app, and whether the
// window has various features; fullscreen and presentation mode methods are
// separate).
@interface BrowserWindowController(WindowType)

// Determines whether this controller's window supports a given feature (i.e.,
// whether a given feature is or can be shown in the window).
// TODO(viettrungluu): |feature| is really should be |Browser::Feature|, but I
// don't want to include browser.h (and you can't forward declare enums).
- (BOOL)supportsWindowFeature:(int)feature;

// Called to check whether or not this window has a normal title bar (YES if it
// does, NO otherwise). (E.g., normal browser windows do not, pop-ups do.)
- (BOOL)hasTitleBar;

// Called to check whether or not this window has a toolbar (YES if it does, NO
// otherwise). (E.g., normal browser windows do, pop-ups do not.)
- (BOOL)hasToolbar;

// Called to check whether or not this window has a location bar (YES if it
// does, NO otherwise). (E.g., normal browser windows do, pop-ups may or may
// not.)
- (BOOL)hasLocationBar;

// Called to check whether or not this window can have bookmark bar (YES if it
// does, NO otherwise). (E.g., normal browser windows may, pop-ups may not.)
- (BOOL)supportsBookmarkBar;

// Called to check if this controller's window is a tabbed window (e.g., not a
// pop-up window). Returns YES if it is, NO otherwise.
// Note: The |-has...| methods are usually preferred, so this method is largely
// deprecated.
- (BOOL)isTabbedWindow;

@end  // @interface BrowserWindowController(WindowType)

// Fullscreen terminology:
//
// ----------------------------------------------------------------------------
// There are 2 APIs that cause the window to get resized, and possibly move
// spaces.
//
// + AppKitFullscreen API: AppKit touts a feature known as "fullscreen". This
// involves moving the current window to a different space, and resizing the
// window to take up the entire size of the screen.
//
// + Immersive fullscreen: An alternative to AppKitFullscreen API. Uses on 10.6
// (before AppKitFullscreen API was available), and on certain HTML/Flash
// content. This is a method defined by Chrome.
//
// The Immersive fullscreen API can be called after the AppKitFullscreen API.
// Calling the AppKitFullscreen API while immersive fullscreen API has been
// invoked causes all fullscreen modes to exit.
//
// ----------------------------------------------------------------------------
// There are 2 "styles" of omnibox sliding.
// + OMNIBOX_TABS_PRESENT: Both the omnibox and the tabstrip are present.
// Moving the cursor to the top causes the menubar to appear, and everything
// else to slide down.
// + OMNIBOX_TABS_HIDDEN: Both tabstrip and omnibox are hidden. Moving cursor
// to top shows tabstrip, omnibox, and menu bar.
//
// The omnibox sliding styles are used in conjunction with the fullscreen APIs.
// There is exactly 1 sliding style active at a time. The sliding is mangaged
// by the presentationModeController_. (poorly named).
//
// ----------------------------------------------------------------------------
// There are several "fullscreen modes" bantered around. Technically, any
// fullscreen API can be combined with any sliding style.
//
// + System fullscreen***deprecated***: This term is confusing. Don't use it.
// It either refers to the AppKitFullscreen API, or the behavior that users
// expect to see when they click the fullscreen button, or some Chrome specific
// implementation that uses the AppKitFullscreen API.
//
// + Canonical Fullscreen: When a user clicks on the fullscreen button, they
// expect a fullscreen behavior similar to other AppKit apps.
//  - AppKitFullscreen API + OMNIBOX_TABS_PRESENT.
//  - The button click directly invokes the AppKitFullscreen API. This class
//  get a callback, and calls adjustUIForOmniboxFullscreen.
//  - There is a menu item that is intended to invoke the same behavior. When
//  the user clicks the menu item, or use its hotkey, this class invokes the
//  AppKitFullscreen API.
//
// + Presentation Mode:
//  - OMNIBOX_TABS_HIDDEN, typically with AppKitFullscreen API, but can
//  also be with Immersive fullscreen API.
//  - This class sets a flag, indicating that it wants Presentation Mode
//  instead of Canonical Fullscreen. Then it invokes the AppKitFullscreen API.
//
// + HTML5 fullscreen. <-- Currently uses AppKitFullscreen API. This should
// eventually migrate to the Immersive Fullscreen API.
//
// There are more fullscreen styles on OSX than other OSes. However, all OSes
// share the same cross-platform code for entering fullscreen
// (FullscreenController). It is important for OSX fullscreen logic to track
// how the user triggered fullscreen mode.
// There are currently 5 possible mechanisms:
//   - User clicks the AppKit Fullscreen button.
//     -- This invokes -[BrowserWindowController windowWillEnterFullscreen:]
//   - User selects the menu item "Enter Full Screen".
//     -- This invokes FullscreenController::ToggleFullscreenModeInternal(
//        BROWSER_WITH_CHROME)
//   - User selects the menu item "Enter Presentation Mode".
//     -- This invokes FullscreenController::ToggleFullscreenModeInternal(
//        BROWSER)
//     -- The corresponding URL will be empty.
//   - User requests fullscreen via an extension.
//     -- This invokes FullscreenController::ToggleFullscreenModeInternal(
//        BROWSER)
//     -- The corresponding URL will be the url of the extension.
//   - User requests fullscreen via Flash or JavaScript apis.
//     -- This invokes FullscreenController::ToggleFullscreenModeInternal(
//        BROWSER)
//     -- browser_->fullscreen_controller()->
//        IsWindowFullscreenForTabOrPending() returns true.
//     -- The corresponding URL will be the url of the web page.

// Methods having to do with fullscreen and presentation mode.
@interface BrowserWindowController(Fullscreen)

// Toggles fullscreen mode.  Meant to be called by Lion windows when they enter
// or exit Lion fullscreen mode.  Must not be called on Snow Leopard or earlier.
- (void)handleLionToggleFullscreen;

// Enters Browser/Appkit Fullscreen.
// If |withToolbar| is NO, the tab strip and toolbar are hidden
// (aka Presentation Mode).
- (void)enterBrowserFullscreenWithToolbar:(BOOL)withToolbar;

// Adds or removes the tab strip and toolbar from the current window. The
// window must be in immersive or AppKit Fullscreen.
- (void)updateFullscreenWithToolbar:(BOOL)withToolbar;

// Updates the contents of the fullscreen exit bubble with |url| and
// |bubbleType|.
- (void)updateFullscreenExitBubbleURL:(const GURL&)url
                           bubbleType:(ExclusiveAccessBubbleType)bubbleType;

// Returns YES if the browser window is in or entering any fullscreen mode.
- (BOOL)isInAnyFullscreenMode;

// Returns YES if the browser window is currently in or entering fullscreen via
// the built-in immersive mechanism.
- (BOOL)isInImmersiveFullscreen;

// Returns YES if the browser window is currently in or entering fullscreen via
// the AppKit Fullscreen API.
- (BOOL)isInAppKitFullscreen;

// Enter fullscreen for an extension.
- (void)enterExtensionFullscreenForURL:(const GURL&)url
                            bubbleType:(ExclusiveAccessBubbleType)bubbleType;

// Enters Immersive Fullscreen for the given URL.
- (void)enterWebContentFullscreenForURL:(const GURL&)url
                             bubbleType:(ExclusiveAccessBubbleType)bubbleType;

// Exits the current fullscreen mode.
- (void)exitAnyFullscreen;

// Whether the system is in the very specific fullscreen mode: Presentation
// Mode.
- (BOOL)inPresentationMode;

// Resizes the fullscreen window to fit the screen it's currently on.  Called by
// the PresentationModeController when there is a change in monitor placement or
// resolution.
- (void)resizeFullscreenWindow;

// Query/lock/release the requirement that the tab strip/toolbar/attached
// bookmark bar bar cluster is visible (e.g., when one of its elements has
// focus). This is required for the floating bar in presentation mode, but
// should also be called when not in presentation mode; see the comments for
// |barVisibilityLocks_| for more details. Double locks/releases by the same
// owner are ignored. If |animate:| is YES, then an animation may be performed,
// possibly after a small delay if |delay:| is YES. If |animate:| is NO,
// |delay:| will be ignored. In the case of multiple calls, later calls have
// precedence with the rule that |animate:NO| has precedence over |animate:YES|,
// and |delay:NO| has precedence over |delay:YES|.
- (BOOL)isBarVisibilityLockedForOwner:(id)owner;
- (void)lockBarVisibilityForOwner:(id)owner
                    withAnimation:(BOOL)animate
                            delay:(BOOL)delay;
- (void)releaseBarVisibilityForOwner:(id)owner
                       withAnimation:(BOOL)animate
                               delay:(BOOL)delay;

// Returns YES if any of the views in the floating bar currently has focus.
- (BOOL)floatingBarHasFocus;

@end  // @interface BrowserWindowController(Fullscreen)


// Methods which are either only for testing, or only public for testing.
@interface BrowserWindowController (TestingAPI)

// Put the incognito badge or multi-profile avatar on the browser and adjust the
// tab strip accordingly.
- (void)installAvatar;

// Allows us to initWithBrowser withOUT taking ownership of the browser.
- (id)initWithBrowser:(Browser*)browser takeOwnership:(BOOL)ownIt;

// Adjusts the window height by the given amount.  If the window spans from the
// top of the current workspace to the bottom of the current workspace, the
// height is not adjusted.  If growing the window by the requested amount would
// size the window to be taller than the current workspace, the window height is
// capped to be equal to the height of the current workspace.  If the window is
// partially offscreen, its height is not adjusted at all.  This function
// prefers to grow the window down, but will grow up if needed.  Calls to this
// function should be followed by a call to |layoutSubviews|.
// Returns if the window height was changed.
- (BOOL)adjustWindowHeightBy:(CGFloat)deltaH;

// Return an autoreleased NSWindow suitable for fullscreen use.
- (NSWindow*)createFullscreenWindow;

// Resets any saved state about window growth (due to showing the bookmark bar
// or the download shelf), so that future shrinking will occur from the bottom.
- (void)resetWindowGrowthState;

// Computes by how far in each direction, horizontal and vertical, the
// |source| rect doesn't fit into |target|.
- (NSSize)overflowFrom:(NSRect)source
                    to:(NSRect)target;

// The fullscreen exit bubble controller, or nil if the bubble isn't showing.
- (ExclusiveAccessBubbleWindowController*)exclusiveAccessBubbleWindowController;

// Gets the rect, in window base coordinates, that the omnibox popup should be
// positioned relative to.
- (NSRect)omniboxPopupAnchorRect;

@end  // @interface BrowserWindowController (TestingAPI)


#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_H_
