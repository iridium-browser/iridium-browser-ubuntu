// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/sync_confirmation_handler.h"

#include <vector>

#include "base/bind.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/signin_view_controller_delegate.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

const int kProfileImageSize = 128;

SyncConfirmationHandler::SyncConfirmationHandler()
    : did_user_explicitly_interact(false) {}

SyncConfirmationHandler::~SyncConfirmationHandler() {
  Profile* profile = Profile::FromWebUI(web_ui());
  AccountTrackerServiceFactory::GetForProfile(profile)->RemoveObserver(this);

  // Abort signin and prevent sync from starting if none of the actions on the
  // sync confirmation dialog are taken by the user.
  if (!did_user_explicitly_interact) {
    HandleUndo(nullptr);
    content::RecordAction(base::UserMetricsAction("Signin_Abort_Signin"));
  }
}

void SyncConfirmationHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("confirm",
      base::Bind(&SyncConfirmationHandler::HandleConfirm,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("undo",
      base::Bind(&SyncConfirmationHandler::HandleUndo, base::Unretained(this)));
  web_ui()->RegisterMessageCallback("goToSettings",
      base::Bind(&SyncConfirmationHandler::HandleGoToSettings,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("initializedWithSize",
      base::Bind(&SyncConfirmationHandler::HandleInitializedWithSize,
                 base::Unretained(this)));
}

void SyncConfirmationHandler::HandleConfirm(const base::ListValue* args) {
  did_user_explicitly_interact = true;
  CloseModalSigninWindow(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
}

void SyncConfirmationHandler::HandleGoToSettings(const base::ListValue* args) {
  did_user_explicitly_interact = true;
  CloseModalSigninWindow(LoginUIService::CONFIGURE_SYNC_FIRST);
}

void SyncConfirmationHandler::HandleUndo(const base::ListValue* args) {
  did_user_explicitly_interact = true;
  content::RecordAction(base::UserMetricsAction("Signin_Undo_Signin"));
  Browser* browser = signin::GetDesktopBrowser(web_ui());
  if (browser) {
    LoginUIServiceFactory::GetForProfile(browser->profile())->
        SyncConfirmationUIClosed(LoginUIService::ABORT_SIGNIN);
    SigninManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()))->SignOut(
        signin_metrics::ABORT_SIGNIN,
        signin_metrics::SignoutDelete::IGNORE_METRIC);
    browser->CloseModalSigninWindow();
  }
}

void SyncConfirmationHandler::SetUserImageURL(const std::string& picture_url) {
  GURL url;
  if (profiles::GetImageURLWithThumbnailSize(GURL(picture_url),
                                             kProfileImageSize,
                                             &url)) {
    base::StringValue picture_url_value(url.spec());
    web_ui()->CallJavascriptFunctionUnsafe("sync.confirmation.setUserImageURL",
                                           picture_url_value);
  }
}

void SyncConfirmationHandler::OnAccountUpdated(const AccountInfo& info) {
  DCHECK(info.IsValid());
  Profile* profile = Profile::FromWebUI(web_ui());
  AccountTrackerServiceFactory::GetForProfile(profile)->RemoveObserver(this);

  SetUserImageURL(info.picture_url);
}

void SyncConfirmationHandler::CloseModalSigninWindow(
    LoginUIService::SyncConfirmationUIClosedResult result) {
  Browser* browser = signin::GetDesktopBrowser(web_ui());
  if (browser) {
    LoginUIServiceFactory::GetForProfile(browser->profile())->
        SyncConfirmationUIClosed(result);
    browser->CloseModalSigninWindow();
  }
}

void SyncConfirmationHandler::HandleInitializedWithSize(
    const base::ListValue* args) {
  Browser* browser = signin::GetDesktopBrowser(web_ui());
  if (!browser)
    return;

  Profile* profile = browser->profile();
  std::vector<AccountInfo> accounts =
      AccountTrackerServiceFactory::GetForProfile(profile)->GetAccounts();

  if (accounts.empty())
    return;

  AccountInfo primary_account_info = accounts[0];

  if (!primary_account_info.IsValid())
    AccountTrackerServiceFactory::GetForProfile(profile)->AddObserver(this);
  else
    SetUserImageURL(primary_account_info.picture_url);

  signin::SetInitializedModalHeight(web_ui(), args);

  // After the dialog is shown, some platforms might have an element focused.
  // To be consistent, clear the focused element on all platforms.
  // TODO(anthonyvd): Figure out why this is needed on Mac and not other
  // platforms and if there's a way to start unfocused while avoiding this
  // workaround.
  web_ui()->CallJavascriptFunctionUnsafe("sync.confirmation.clearFocus");
}
