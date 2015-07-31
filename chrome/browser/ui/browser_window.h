// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_H_

#include "base/callback_forward.h"
#include "chrome/browser/lifetime/browser_close_manager.h"
#include "chrome/browser/signin/signin_header_helper.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_type.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/translate/core/common/translate_errors.h"
#include "ui/base/base_window.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class DownloadShelf;
class ExclusiveAccessContext;
class FindBar;
class GlobalErrorBubbleViewBase;
class GURL;
class LocationBar;
class Profile;
class ProfileResetGlobalError;
class StatusBubble;
class TemplateURL;

struct WebApplicationInfo;

namespace content {
class WebContents;
struct NativeWebKeyboardEvent;
struct SSLStatus;
}

namespace extensions {
class Command;
class Extension;
}

namespace gfx {
class Rect;
class Size;
}

namespace web_modal {
class WebContentsModalDialogHost;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserWindow interface
//  An interface implemented by the "view" of the Browser window.
//  This interface includes ui::BaseWindow methods as well as Browser window
//  specific methods.
//
// NOTE: All getters may return NULL.
//
class BrowserWindow : public ui::BaseWindow {
 public:
  virtual ~BrowserWindow() {}

  //////////////////////////////////////////////////////////////////////////////
  // ui::BaseWindow interface notes:

  // Closes the window as soon as possible. If the window is not in a drag
  // session, it will close immediately; otherwise, it will move offscreen (so
  // events are still fired) until the drag ends, then close. This assumes
  // that the Browser is not immediately destroyed, but will be eventually
  // destroyed by other means (eg, the tab strip going to zero elements).
  // Bad things happen if the Browser dtor is called directly as a result of
  // invoking this method.
  // virtual void Close() = 0;

  // Browser::OnWindowDidShow should be called after showing the window.
  // virtual void Show() = 0;

  //////////////////////////////////////////////////////////////////////////////
  // Browser specific methods:

  // Return the status bubble associated with the frame
  virtual StatusBubble* GetStatusBubble() = 0;

  // Inform the frame that the selected tab favicon or title has changed. Some
  // frames may need to refresh their title bar.
  virtual void UpdateTitleBar() = 0;

  // Invoked when the state of the bookmark bar changes. This is only invoked if
  // the state changes for the current tab, it is not sent when switching tabs.
  virtual void BookmarkBarStateChanged(
      BookmarkBar::AnimateChangeType change_type) = 0;

  // Inform the frame that the dev tools window for the selected tab has
  // changed.
  virtual void UpdateDevTools() = 0;

  // Update any loading animations running in the window. |should_animate| is
  // true if there are tabs loading and the animations should continue, false
  // if there are no active loads and the animations should end.
  virtual void UpdateLoadingAnimations(bool should_animate) = 0;

  // Sets the starred state for the current tab.
  virtual void SetStarredState(bool is_starred) = 0;

  // Sets whether the translate icon is lit for the current tab.
  virtual void SetTranslateIconToggled(bool is_lit) = 0;

  // Called when the active tab changes.  Subclasses which implement
  // TabStripModelObserver should implement this instead of ActiveTabChanged();
  // the Browser will call this method while processing that one.
  virtual void OnActiveTabChanged(content::WebContents* old_contents,
                                  content::WebContents* new_contents,
                                  int index,
                                  int reason) = 0;

  // Called to force the zoom state to for the active tab to be recalculated.
  // |can_show_bubble| is true when a user presses the zoom up or down keyboard
  // shortcuts and will be false in other cases (e.g. switching tabs, "clicking"
  // + or - in the wrench menu to change zoom).
  virtual void ZoomChangedForActiveTab(bool can_show_bubble) = 0;

  // Methods that change fullscreen state.
  // On Mac, the tab strip and toolbar will be shown if |with_toolbar| is true,
  // |with_toolbar| is ignored on other platforms.
  virtual void EnterFullscreen(const GURL& url,
                               ExclusiveAccessBubbleType bubble_type,
                               bool with_toolbar) = 0;
  virtual void ExitFullscreen() = 0;
  virtual void UpdateExclusiveAccessExitBubbleContent(
      const GURL& url,
      ExclusiveAccessBubbleType bubble_type) = 0;

  // Windows and GTK remove the top controls in fullscreen, but Mac and Ash
  // keep the controls in a slide-down panel.
  virtual bool ShouldHideUIForFullscreen() const = 0;

  // Returns true if the fullscreen bubble is visible.
  virtual bool IsFullscreenBubbleVisible() const = 0;

  // Show or hide the tab strip, toolbar and bookmark bar when in browser
  // fullscreen.
  // Currently only supported on Mac.
  virtual bool SupportsFullscreenWithToolbar() const = 0;
  virtual void UpdateFullscreenWithToolbar(bool with_toolbar) = 0;
  virtual bool IsFullscreenWithToolbar() const = 0;

#if defined(OS_WIN)
  // Sets state for entering or exiting Win8 Metro snap mode.
  virtual void SetMetroSnapMode(bool enable) = 0;

  // Returns whether the window is currently in Win8 Metro snap mode.
  virtual bool IsInMetroSnapMode() const = 0;
#endif

  // Returns the location bar.
  virtual LocationBar* GetLocationBar() const = 0;

  // Tries to focus the location bar.  Clears the window focus (to avoid
  // inconsistent state) if this fails.
  virtual void SetFocusToLocationBar(bool select_all) = 0;

  // Informs the view whether or not a load is in progress for the current tab.
  // The view can use this notification to update the reload/stop button.
  virtual void UpdateReloadStopState(bool is_loading, bool force) = 0;

  // Updates the toolbar with the state for the specified |contents|.
  virtual void UpdateToolbar(content::WebContents* contents) = 0;

  // Resets the toolbar's tab state for |contents|.
  virtual void ResetToolbarTabState(content::WebContents* contents) = 0;

  // Focuses the toolbar (for accessibility).
  virtual void FocusToolbar() = 0;

  // Called from toolbar subviews during their show/hide animations.
  virtual void ToolbarSizeChanged(bool is_animating) = 0;

  // Focuses the app menu like it was a menu bar.
  //
  // Not used on the Mac, which has a "normal" menu bar.
  virtual void FocusAppMenu() = 0;

  // Focuses the bookmarks toolbar (for accessibility).
  virtual void FocusBookmarksToolbar() = 0;

  // Focuses an infobar, if shown (for accessibility).
  virtual void FocusInfobars() = 0;

  // Moves keyboard focus to the next pane.
  virtual void RotatePaneFocus(bool forwards) = 0;

  // Returns whether the bookmark bar is visible or not.
  virtual bool IsBookmarkBarVisible() const = 0;

  // Returns whether the bookmark bar is animating or not.
  virtual bool IsBookmarkBarAnimating() const = 0;

  // Returns whether the tab strip is editable (for extensions).
  virtual bool IsTabStripEditable() const = 0;

  // Returns whether the tool bar is visible or not.
  virtual bool IsToolbarVisible() const = 0;

  // Returns the rect where the resize corner should be drawn by the render
  // widget host view (on top of what the renderer returns). We return an empty
  // rect to identify that there shouldn't be a resize corner (in the cases
  // where we take care of it ourselves at the browser level).
  virtual gfx::Rect GetRootWindowResizerRect() const = 0;

  // Shows a confirmation dialog box for adding a search engine described by
  // |template_url|. Takes ownership of |template_url|.
  virtual void ConfirmAddSearchProvider(TemplateURL* template_url,
                                        Profile* profile) = 0;

  // Shows the Update Recommended dialog box.
  virtual void ShowUpdateChromeDialog() = 0;

  // Shows the Bookmark bubble. |url| is the URL being bookmarked,
  // |already_bookmarked| is true if the url is already bookmarked.
  virtual void ShowBookmarkBubble(const GURL& url, bool already_bookmarked) = 0;

  // Callback type used with the ShowBookmarkAppBubble() method. The boolean
  // parameter is true when the user accepts the dialog. The WebApplicationInfo
  // parameter contains the WebApplicationInfo as edited by the user.
  typedef base::Callback<void(bool, const WebApplicationInfo&)>
      ShowBookmarkAppBubbleCallback;

  // Shows the Bookmark App bubble.
  // See Extension::InitFromValueFlags::FROM_BOOKMARK for a description of
  // bookmark apps.
  //
  // |web_app_info| is the WebApplicationInfo being converted into an app.
  virtual void ShowBookmarkAppBubble(
      const WebApplicationInfo& web_app_info,
      const ShowBookmarkAppBubbleCallback& callback) = 0;

  // Shows the translate bubble.
  //
  // |is_user_gesture| is true when the bubble is shown on the user's deliberate
  // action.
  virtual void ShowTranslateBubble(
      content::WebContents* contents,
      translate::TranslateStep step,
      translate::TranslateErrors::Type error_type,
      bool is_user_gesture) = 0;

  // Create a session recovery bubble if the last session crashed. It also
  // offers the option to enable metrics reporting if it's not already enabled.
  // Returns true if a bubble is created, returns false if nothing is created.
  virtual bool ShowSessionCrashedBubble() = 0;

  // Shows the profile reset bubble on the platforms that support it.
  virtual bool IsProfileResetBubbleSupported() const = 0;
  virtual GlobalErrorBubbleViewBase* ShowProfileResetBubble(
      const base::WeakPtr<ProfileResetGlobalError>& global_error) = 0;

#if defined(ENABLE_ONE_CLICK_SIGNIN)
  enum OneClickSigninBubbleType {
    ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE,
    ONE_CLICK_SIGNIN_BUBBLE_TYPE_MODAL_DIALOG,
    ONE_CLICK_SIGNIN_BUBBLE_TYPE_SAML_MODAL_DIALOG
  };

  // Callback type used with the ShowOneClickSigninBubble() method.  If the
  // user chooses to accept the sign in, the callback is called to start the
  // sync process.
  typedef base::Callback<void(OneClickSigninSyncStarter::StartSyncMode)>
      StartSyncCallback;

  // Shows the one-click sign in bubble.  |email| holds the full email address
  // of the account that has signed in.
  virtual void ShowOneClickSigninBubble(
      OneClickSigninBubbleType type,
      const base::string16& email,
      const base::string16& error_message,
      const StartSyncCallback& start_sync_callback) = 0;
#endif

  // Whether or not the shelf view is visible.
  virtual bool IsDownloadShelfVisible() const = 0;

  // Returns the DownloadShelf.
  virtual DownloadShelf* GetDownloadShelf() = 0;

  // Shows the confirmation dialog box warning that the browser is closing with
  // in-progress downloads.
  // This method should call |callback| with the user's response.
  virtual void ConfirmBrowserCloseWithPendingDownloads(
      int download_count,
      Browser::DownloadClosePreventionType dialog_type,
      bool app_modal,
      const base::Callback<void(bool)>& callback) = 0;

  // ThemeService calls this when a user has changed his or her theme,
  // indicating that it's time to redraw everything.
  virtual void UserChangedTheme() = 0;

  // Shows the website settings using the specified information. |url| is the
  // url of the page/frame the info applies to, |ssl| is the SSL information for
  // that page/frame.  If |show_history| is true, a section showing how many
  // times that URL has been visited is added to the page info.
  virtual void ShowWebsiteSettings(Profile* profile,
                                   content::WebContents* web_contents,
                                   const GURL& url,
                                   const content::SSLStatus& ssl) = 0;

  // Shows the app menu (for accessibility).
  virtual void ShowAppMenu() = 0;

  // Allows the BrowserWindow object to handle the specified keyboard event
  // before sending it to the renderer.
  // Returns true if the |event| was handled. Otherwise, if the |event| would
  // be handled in HandleKeyboardEvent() method as a normal keyboard shortcut,
  // |*is_keyboard_shortcut| should be set to true.
  virtual bool PreHandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) = 0;

  // Allows the BrowserWindow object to handle the specified keyboard event,
  // if the renderer did not process it.
  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) = 0;

  // Clipboard commands applied to the whole browser window.
  virtual void CutCopyPaste(int command_id) = 0;

  // Return the correct disposition for a popup window based on |bounds|.
  virtual WindowOpenDisposition GetDispositionForPopupBounds(
      const gfx::Rect& bounds) = 0;

  // Construct a FindBar implementation for the |browser|.
  virtual FindBar* CreateFindBar() = 0;

  // Return the WebContentsModalDialogHost for use in positioning web contents
  // modal dialogs within the browser window. This can sometimes be NULL (for
  // instance during tab drag on Views/Win32).
  virtual web_modal::WebContentsModalDialogHost*
      GetWebContentsModalDialogHost() = 0;

  // Invoked when the preferred size of the contents in current tab has been
  // changed. We might choose to update the window size to accomodate this
  // change.
  // Note that this won't be fired if we change tabs.
  virtual void UpdatePreferredSize(content::WebContents* web_contents,
                                   const gfx::Size& pref_size) {}

  // Invoked when the contents auto-resized and the container should match it.
  virtual void ResizeDueToAutoResize(content::WebContents* web_contents,
                                     const gfx::Size& new_size) {}

  // Construct a BrowserWindow implementation for the specified |browser|.
  static BrowserWindow* CreateBrowserWindow(Browser* browser);

  // Returns a HostDesktopType that is compatible with the current Chrome window
  // configuration. On Windows with Ash, this is always HOST_DESKTOP_TYPE_ASH
  // while Chrome is running in Metro mode. Otherwise returns |desktop_type|.
  static chrome::HostDesktopType AdjustHostDesktopType(
      chrome::HostDesktopType desktop_type);

  // Shows the avatar bubble on the window frame off of the avatar button with
  // the given mode. The Service Type specified by GAIA is provided as well.
  enum AvatarBubbleMode {
    AVATAR_BUBBLE_MODE_DEFAULT,
    AVATAR_BUBBLE_MODE_ACCOUNT_MANAGEMENT,
    AVATAR_BUBBLE_MODE_SIGNIN,
    AVATAR_BUBBLE_MODE_ADD_ACCOUNT,
    AVATAR_BUBBLE_MODE_REAUTH,
    AVATAR_BUBBLE_MODE_CONFIRM_SIGNIN,
    AVATAR_BUBBLE_MODE_SHOW_ERROR,
    AVATAR_BUBBLE_MODE_FAST_USER_SWITCH,
  };
  virtual void ShowAvatarBubbleFromAvatarButton(AvatarBubbleMode mode,
      const signin::ManageAccountsParams& manage_accounts_params) = 0;

  // Returns the height inset for RenderView when detached bookmark bar is
  // shown.  Invoked when a new RenderHostView is created for a non-NTP
  // navigation entry and the bookmark bar is detached.
  virtual int GetRenderViewHeightInsetWithDetachedBookmarkBar() = 0;

  // Executes |command| registered by |extension|.
  virtual void ExecuteExtensionCommand(const extensions::Extension* extension,
                                       const extensions::Command& command) = 0;

  // Returns object implementing ExclusiveAccessContext interface.
  virtual ExclusiveAccessContext* GetExclusiveAccessContext() = 0;

 protected:
  friend class BrowserCloseManager;
  friend class BrowserView;
  virtual void DestroyBrowser() = 0;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_H_
