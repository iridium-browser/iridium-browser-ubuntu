// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_within_tab_helper.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_contents_sizer.h"
#include "chrome/common/chrome_switches.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"

#if !defined(OS_MACOSX)
#include "base/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#endif

using base::UserMetricsAction;
using content::RenderViewHost;
using content::WebContents;

FullscreenController::FullscreenController(ExclusiveAccessManager* manager)
    : ExclusiveAccessControllerBase(manager),
      state_prior_to_tab_fullscreen_(STATE_INVALID),
      tab_fullscreen_accepted_(false),
      toggled_into_fullscreen_(false),
      reentrant_window_state_change_call_check_(false),
      is_privileged_fullscreen_for_testing_(false),
      ptr_factory_(this) {
}

FullscreenController::~FullscreenController() {
}

bool FullscreenController::IsFullscreenForBrowser() const {
  return exclusive_access_manager()->context()->IsFullscreen() &&
         !IsFullscreenCausedByTab();
}

void FullscreenController::ToggleBrowserFullscreenMode() {
  extension_caused_fullscreen_ = GURL();
  ToggleFullscreenModeInternal(BROWSER);
}

void FullscreenController::ToggleBrowserFullscreenWithToolbar() {
  ToggleFullscreenModeInternal(BROWSER_WITH_TOOLBAR);
}

void FullscreenController::ToggleBrowserFullscreenModeWithExtension(
    const GURL& extension_url) {
  // |extension_caused_fullscreen_| will be reset if this causes fullscreen to
  // exit.
  extension_caused_fullscreen_ = extension_url;
  ToggleFullscreenModeInternal(BROWSER);
}

bool FullscreenController::IsWindowFullscreenForTabOrPending() const {
  return exclusive_access_tab() != nullptr;
}

bool FullscreenController::IsExtensionFullscreenOrPending() const {
  return !extension_caused_fullscreen_.is_empty();
}

bool FullscreenController::IsControllerInitiatedFullscreen() const {
  return toggled_into_fullscreen_;
}

bool FullscreenController::IsUserAcceptedFullscreen() const {
  return tab_fullscreen_accepted_;
}

bool FullscreenController::IsFullscreenForTabOrPending(
    const WebContents* web_contents) const {
  if (web_contents == exclusive_access_tab()) {
    DCHECK(web_contents ==
           exclusive_access_manager()->context()->GetActiveWebContents());
    DCHECK(web_contents->GetCapturerCount() == 0);
    return true;
  }
  return IsFullscreenForCapturedTab(web_contents);
}

bool FullscreenController::IsFullscreenCausedByTab() const {
  return state_prior_to_tab_fullscreen_ == STATE_NORMAL;
}

void FullscreenController::EnterFullscreenModeForTab(WebContents* web_contents,
                                                     const GURL& origin) {
  DCHECK(web_contents);

  if (MaybeToggleFullscreenForCapturedTab(web_contents, true)) {
    // During tab capture of fullscreen-within-tab views, the browser window
    // fullscreen state is unchanged, so return now.
    return;
  }

  if (web_contents !=
          exclusive_access_manager()->context()->GetActiveWebContents() ||
      IsWindowFullscreenForTabOrPending()) {
      return;
  }

#if defined(OS_WIN)
  // For now, avoid breaking when initiating full screen tab mode while in
  // a metro snap.
  // TODO(robertshield): Find a way to reconcile tab-initiated fullscreen
  //                     modes with metro snap.
  if (IsInMetroSnapMode())
    return;
#endif

  SetTabWithExclusiveAccess(web_contents);
  fullscreened_origin_ = origin;

  ExclusiveAccessContext* exclusive_access_context =
      exclusive_access_manager()->context();

  if (!exclusive_access_context->IsFullscreen()) {
    // Normal -> Tab Fullscreen.
    state_prior_to_tab_fullscreen_ = STATE_NORMAL;
    ToggleFullscreenModeInternal(TAB);
    return;
  }

  if (exclusive_access_context->IsFullscreenWithToolbar()) {
    // Browser Fullscreen with Toolbar -> Tab Fullscreen (no toolbar).
    exclusive_access_context->UpdateFullscreenWithToolbar(false);
    state_prior_to_tab_fullscreen_ = STATE_BROWSER_FULLSCREEN_WITH_TOOLBAR;
  } else {
    // Browser Fullscreen without Toolbar -> Tab Fullscreen.
    state_prior_to_tab_fullscreen_ = STATE_BROWSER_FULLSCREEN_NO_TOOLBAR;
  }

  // We need to update the fullscreen exit bubble, e.g., going from browser
  // fullscreen to tab fullscreen will need to show different content.
  if (!tab_fullscreen_accepted_) {
    tab_fullscreen_accepted_ = GetFullscreenSetting() == CONTENT_SETTING_ALLOW;
  }
  exclusive_access_manager()->UpdateExclusiveAccessExitBubbleContent();

  // This is only a change between Browser and Tab fullscreen. We generate
  // a fullscreen notification now because there is no window change.
  PostFullscreenChangeNotification(true);
}

void FullscreenController::ExitFullscreenModeForTab(WebContents* web_contents) {
  if (MaybeToggleFullscreenForCapturedTab(web_contents, false)) {
    // During tab capture of fullscreen-within-tab views, the browser window
    // fullscreen state is unchanged, so return now.
    return;
  }

  if (!IsWindowFullscreenForTabOrPending() ||
      web_contents != exclusive_access_tab()) {
    return;
  }

#if defined(OS_WIN)
  // For now, avoid breaking when initiating full screen tab mode while in
  // a metro snap.
  // TODO(robertshield): Find a way to reconcile tab-initiated fullscreen
  //                     modes with metro snap.
  if (IsInMetroSnapMode())
    return;
#endif

  ExclusiveAccessContext* exclusive_access_context =
      exclusive_access_manager()->context();

  if (!exclusive_access_context->IsFullscreen())
    return;

  if (IsFullscreenCausedByTab()) {
    // Tab Fullscreen -> Normal.
    ToggleFullscreenModeInternal(TAB);
    return;
  }

  // Tab Fullscreen -> Browser Fullscreen (with or without toolbar).
  if (state_prior_to_tab_fullscreen_ == STATE_BROWSER_FULLSCREEN_WITH_TOOLBAR) {
    // Tab Fullscreen (no toolbar) -> Browser Fullscreen with Toolbar.
    exclusive_access_context->UpdateFullscreenWithToolbar(true);
  }

#if defined(OS_MACOSX)
  // Clear the bubble URL, which forces the Mac UI to redraw.
  exclusive_access_manager()->UpdateExclusiveAccessExitBubbleContent();
#endif  // defined(OS_MACOSX)

  // If currently there is a tab in "tab fullscreen" mode and fullscreen
  // was not caused by it (i.e., previously it was in "browser fullscreen"
  // mode), we need to switch back to "browser fullscreen" mode. In this
  // case, all we have to do is notifying the tab that it has exited "tab
  // fullscreen" mode.
  NotifyTabExclusiveAccessLost();

  // This is only a change between Browser and Tab fullscreen. We generate
  // a fullscreen notification now because there is no window change.
  PostFullscreenChangeNotification(true);
}

#if defined(OS_WIN)
bool FullscreenController::IsInMetroSnapMode() {
  return exclusive_access_manager()->context()->IsInMetroSnapMode();
}

void FullscreenController::SetMetroSnapMode(bool enable) {
  reentrant_window_state_change_call_check_ = false;

  toggled_into_fullscreen_ = false;
  exclusive_access_manager()->context()->SetMetroSnapMode(enable);

  // FullscreenController unit tests for metro snap assume that on Windows calls
  // to WindowFullscreenStateChanged are reentrant. If that assumption is
  // invalidated, the tests must be updated to maintain coverage.
  CHECK(reentrant_window_state_change_call_check_);
}
#endif  // defined(OS_WIN)

void FullscreenController::OnTabDetachedFromView(WebContents* old_contents) {
  if (!IsFullscreenForCapturedTab(old_contents))
    return;

  // A fullscreen-within-tab view undergoing screen capture has been detached
  // and is no longer visible to the user. Set it to exactly the WebContents'
  // preferred size. See 'FullscreenWithinTab Note'.
  //
  // When the user later selects the tab to show |old_contents| again, UI code
  // elsewhere (e.g., views::WebView) will resize the view to fit within the
  // browser window once again.

  // If the view has been detached from the browser window (e.g., to drag a tab
  // off into a new browser window), return immediately to avoid an unnecessary
  // resize.
  if (!old_contents->GetDelegate())
    return;

  // Do nothing if tab capture ended after toggling fullscreen, or a preferred
  // size was never specified by the capturer.
  if (old_contents->GetCapturerCount() == 0 ||
      old_contents->GetPreferredSize().IsEmpty()) {
    return;
  }

  content::RenderWidgetHostView* const current_fs_view =
      old_contents->GetFullscreenRenderWidgetHostView();
  if (current_fs_view)
    current_fs_view->SetSize(old_contents->GetPreferredSize());
  ResizeWebContents(old_contents, old_contents->GetPreferredSize());
}

void FullscreenController::OnTabClosing(WebContents* web_contents) {
  if (IsFullscreenForCapturedTab(web_contents)) {
    web_contents->ExitFullscreen();
  } else {
    ExclusiveAccessControllerBase::OnTabClosing(web_contents);
  }
}

void FullscreenController::WindowFullscreenStateChanged() {
  reentrant_window_state_change_call_check_ = true;
  ExclusiveAccessContext* const exclusive_access_context =
      exclusive_access_manager()->context();
  bool exiting_fullscreen = !exclusive_access_context->IsFullscreen();

  PostFullscreenChangeNotification(!exiting_fullscreen);
  if (exiting_fullscreen) {
    toggled_into_fullscreen_ = false;
    extension_caused_fullscreen_ = GURL();
    NotifyTabExclusiveAccessLost();
    exclusive_access_context->UnhideDownloadShelf();
  } else {
    exclusive_access_context->HideDownloadShelf();
  }
}

bool FullscreenController::HandleUserPressedEscape() {
  WebContents* const active_web_contents =
      exclusive_access_manager()->context()->GetActiveWebContents();
  if (IsFullscreenForCapturedTab(active_web_contents)) {
    active_web_contents->ExitFullscreen();
    return true;
  } else if (IsWindowFullscreenForTabOrPending()) {
    ExitExclusiveAccessIfNecessary();
    return true;
  }

  return false;
}

void FullscreenController::ExitExclusiveAccessToPreviousState() {
  if (IsWindowFullscreenForTabOrPending())
    ExitFullscreenModeForTab(exclusive_access_tab());
  else if (IsFullscreenForBrowser())
    ExitFullscreenModeInternal();
}

bool FullscreenController::OnAcceptExclusiveAccessPermission() {
  ExclusiveAccessBubbleType bubble_type =
      exclusive_access_manager()->GetExclusiveAccessExitBubbleType();
  bool fullscreen = false;
  exclusive_access_bubble::PermissionRequestedByType(bubble_type, &fullscreen,
                                                     nullptr);
  DCHECK(!(fullscreen && tab_fullscreen_accepted_));

  if (fullscreen && !tab_fullscreen_accepted_) {
    DCHECK(exclusive_access_tab());
    // Origins can enter fullscreen even when embedded in other origins.
    // Permission is tracked based on the combinations of requester and
    // embedder. Thus, even if a requesting origin has been previously approved
    // for embedder A, it will not be approved when embedded in a different
    // origin B.
    //
    // However, an exception is made when a requester and an embedder are the
    // same origin. In other words, if the requester is the top-level frame. If
    // that combination is ALLOWED, then future requests from that origin will
    // succeed no matter what the embedder is. For example, if youtube.com
    // is visited and user selects ALLOW. Later user visits example.com which
    // embeds youtube.com in an iframe, which is then ALLOWED to go fullscreen.
    GURL requester = GetRequestingOrigin();
    GURL embedder = GetEmbeddingOrigin();
    ContentSettingsPattern primary_pattern =
        ContentSettingsPattern::FromURLNoWildcard(requester);
    ContentSettingsPattern secondary_pattern =
        ContentSettingsPattern::FromURLNoWildcard(embedder);

    // ContentSettings requires valid patterns and the patterns might be invalid
    // in some edge cases like if the current frame is about:blank.
    //
    // Do not store preference on file:// URLs, they don't have a clean
    // origin policy.
    // TODO(estark): Revisit this when crbug.com/455882 is fixed.
    if (!requester.SchemeIsFile() && !embedder.SchemeIsFile() &&
        primary_pattern.IsValid() && secondary_pattern.IsValid()) {
      HostContentSettingsMap* settings_map = exclusive_access_manager()
                                                 ->context()
                                                 ->GetProfile()
                                                 ->GetHostContentSettingsMap();
      settings_map->SetContentSetting(
          primary_pattern, secondary_pattern, CONTENT_SETTINGS_TYPE_FULLSCREEN,
          std::string(), CONTENT_SETTING_ALLOW);
    }
    tab_fullscreen_accepted_ = true;
    return true;
  }

  return false;
}

bool FullscreenController::OnDenyExclusiveAccessPermission() {
  if (IsWindowFullscreenForTabOrPending()) {
    ExitExclusiveAccessIfNecessary();
    return true;
  }

  return false;
}

GURL FullscreenController::GetURLForExclusiveAccessBubble() const {
  if (exclusive_access_tab())
    return GetRequestingOrigin();
  return extension_caused_fullscreen_;
}

void FullscreenController::ExitExclusiveAccessIfNecessary() {
  if (IsWindowFullscreenForTabOrPending())
    ExitFullscreenModeForTab(exclusive_access_tab());
  else
    NotifyTabExclusiveAccessLost();
}

void FullscreenController::PostFullscreenChangeNotification(
    bool is_fullscreen) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&FullscreenController::NotifyFullscreenChange,
                 ptr_factory_.GetWeakPtr(),
                 is_fullscreen));
}

void FullscreenController::NotifyFullscreenChange(bool is_fullscreen) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_FULLSCREEN_CHANGED,
      content::Source<FullscreenController>(this),
      content::Details<bool>(&is_fullscreen));
}

void FullscreenController::NotifyTabExclusiveAccessLost() {
  if (exclusive_access_tab()) {
    WebContents* web_contents = exclusive_access_tab();
    SetTabWithExclusiveAccess(nullptr);
    fullscreened_origin_ = GURL();
    state_prior_to_tab_fullscreen_ = STATE_INVALID;
    tab_fullscreen_accepted_ = false;
    web_contents->ExitFullscreen();
    exclusive_access_manager()->UpdateExclusiveAccessExitBubbleContent();
  }
}

void FullscreenController::ToggleFullscreenModeInternal(
    FullscreenInternalOption option) {
#if defined(OS_WIN)
  // When in Metro snap mode, toggling in and out of fullscreen is prevented.
  if (IsInMetroSnapMode())
    return;
#endif

  ExclusiveAccessContext* const exclusive_access_context =
      exclusive_access_manager()->context();
  bool enter_fullscreen = !exclusive_access_context->IsFullscreen();

  // When a Mac user requests a toggle they may be toggling between
  // FullscreenWithoutChrome and FullscreenWithToolbar.
  if (exclusive_access_context->IsFullscreen() &&
      !IsWindowFullscreenForTabOrPending() &&
      exclusive_access_context->SupportsFullscreenWithToolbar()) {
    if (option == BROWSER_WITH_TOOLBAR) {
      enter_fullscreen = enter_fullscreen ||
                         !exclusive_access_context->IsFullscreenWithToolbar();
    } else {
      enter_fullscreen = enter_fullscreen ||
                         exclusive_access_context->IsFullscreenWithToolbar();
    }
  }

  // In kiosk mode, we always want to be fullscreen. When the browser first
  // starts we're not yet fullscreen, so let the initial toggle go through.
  if (chrome::IsRunningInAppMode() && exclusive_access_context->IsFullscreen())
    return;

#if !defined(OS_MACOSX)
  // Do not enter fullscreen mode if disallowed by pref. This prevents the user
  // from manually entering fullscreen mode and also disables kiosk mode on
  // desktop platforms.
  if (enter_fullscreen &&
      !exclusive_access_context->GetProfile()->GetPrefs()->GetBoolean(
          prefs::kFullscreenAllowed)) {
    return;
  }
#endif

  if (enter_fullscreen)
    EnterFullscreenModeInternal(option);
  else
    ExitFullscreenModeInternal();
}

void FullscreenController::EnterFullscreenModeInternal(
    FullscreenInternalOption option) {
  toggled_into_fullscreen_ = true;
  GURL url;
  if (option == TAB) {
    url = GetRequestingOrigin();
    tab_fullscreen_accepted_ = GetFullscreenSetting() == CONTENT_SETTING_ALLOW;
  } else {
    if (!extension_caused_fullscreen_.is_empty())
      url = extension_caused_fullscreen_;
  }

  if (option == BROWSER)
    content::RecordAction(UserMetricsAction("ToggleFullscreen"));
  // TODO(scheib): Record metrics for WITH_TOOLBAR, without counting transitions
  // from tab fullscreen out to browser with toolbar.

  exclusive_access_manager()->context()->EnterFullscreen(
      url, exclusive_access_manager()->GetExclusiveAccessExitBubbleType(),
      option == BROWSER_WITH_TOOLBAR);

  exclusive_access_manager()->UpdateExclusiveAccessExitBubbleContent();

  // Once the window has become fullscreen it'll call back to
  // WindowFullscreenStateChanged(). We don't do this immediately as
  // BrowserWindow::EnterFullscreen() asks for bookmark_bar_state_, so we let
  // the BrowserWindow invoke WindowFullscreenStateChanged when appropriate.
}

void FullscreenController::ExitFullscreenModeInternal() {
  toggled_into_fullscreen_ = false;
#if defined(OS_MACOSX)
  // Mac windows report a state change instantly, and so we must also clear
  // state_prior_to_tab_fullscreen_ to match them else other logic using
  // state_prior_to_tab_fullscreen_ will be incorrect.
  NotifyTabExclusiveAccessLost();
#endif
  exclusive_access_manager()->context()->ExitFullscreen();
  extension_caused_fullscreen_ = GURL();

  exclusive_access_manager()->UpdateExclusiveAccessExitBubbleContent();
}

ContentSetting FullscreenController::GetFullscreenSetting() const {
  DCHECK(exclusive_access_tab());

  GURL url = GetRequestingOrigin();

  // Always ask on file:// URLs, since we can't meaningfully make the
  // decision stick for a particular origin.
  // TODO(estark): Revisit this when crbug.com/455882 is fixed.
  if (url.SchemeIsFile())
    return CONTENT_SETTING_ASK;

  if (IsPrivilegedFullscreenForTab())
    return CONTENT_SETTING_ALLOW;

  // If the permission was granted to the website with no embedder, it should
  // always be allowed, even if embedded.
  if (exclusive_access_manager()
          ->context()
          ->GetProfile()
          ->GetHostContentSettingsMap()
          ->GetContentSetting(url, url, CONTENT_SETTINGS_TYPE_FULLSCREEN,
                              std::string()) == CONTENT_SETTING_ALLOW) {
    return CONTENT_SETTING_ALLOW;
  }

  // See the comment above the call to |SetContentSetting()| for how the
  // requesting and embedding origins interact with each other wrt permissions.
  return exclusive_access_manager()
      ->context()
      ->GetProfile()
      ->GetHostContentSettingsMap()
      ->GetContentSetting(url, GetEmbeddingOrigin(),
                          CONTENT_SETTINGS_TYPE_FULLSCREEN, std::string());
}

bool FullscreenController::IsPrivilegedFullscreenForTab() const {
  const bool embedded_widget_present =
      exclusive_access_tab() &&
      exclusive_access_tab()->GetFullscreenRenderWidgetHostView();
  return embedded_widget_present || is_privileged_fullscreen_for_testing_;
}

void FullscreenController::SetPrivilegedFullscreenForTesting(
    bool is_privileged) {
  is_privileged_fullscreen_for_testing_ = is_privileged;
}

bool FullscreenController::MaybeToggleFullscreenForCapturedTab(
    WebContents* web_contents, bool enter_fullscreen) {
  if (enter_fullscreen) {
    if (web_contents->GetCapturerCount() > 0) {
      FullscreenWithinTabHelper::CreateForWebContents(web_contents);
      FullscreenWithinTabHelper::FromWebContents(web_contents)->
          SetIsFullscreenForCapturedTab(true);
      return true;
    }
  } else {
    if (IsFullscreenForCapturedTab(web_contents)) {
      FullscreenWithinTabHelper::RemoveForWebContents(web_contents);
      return true;
    }
  }

  return false;
}

bool FullscreenController::IsFullscreenForCapturedTab(
    const WebContents* web_contents) const {
  // Note: On Mac, some of the OnTabXXX() methods get called with a nullptr
  // value
  // for web_contents. Check for that here.
  const FullscreenWithinTabHelper* const helper =
      web_contents ? FullscreenWithinTabHelper::FromWebContents(web_contents)
                   : nullptr;
  if (helper && helper->is_fullscreen_for_captured_tab()) {
    DCHECK_NE(exclusive_access_tab(), web_contents);
    return true;
  }
  return false;
}

GURL FullscreenController::GetRequestingOrigin() const {
  DCHECK(exclusive_access_tab());

  if (!fullscreened_origin_.is_empty())
    return fullscreened_origin_;

  return exclusive_access_tab()->GetLastCommittedURL();
}

GURL FullscreenController::GetEmbeddingOrigin() const {
  DCHECK(exclusive_access_tab());

  return exclusive_access_tab()->GetLastCommittedURL();
}
