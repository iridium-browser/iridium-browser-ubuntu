// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_delegate.h"

#include "ash/content_support/gpu_support_impl.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/ash/app_list/app_list_service_ash.h"
#include "chrome/browser/ui/ash/ash_keyboard_controller_proxy.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/grit/chromium_strings.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "base/prefs/pref_service.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/display/display_configuration_observer.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

// static
ChromeShellDelegate* ChromeShellDelegate::instance_ = NULL;

ChromeShellDelegate::ChromeShellDelegate()
    : shelf_delegate_(NULL) {
  instance_ = this;
  PlatformInit();
}

ChromeShellDelegate::~ChromeShellDelegate() {
  if (instance_ == this)
    instance_ = NULL;
}

bool ChromeShellDelegate::IsMultiProfilesEnabled() const {
  if (!profiles::IsMultipleProfilesEnabled())
    return false;
#if defined(OS_CHROMEOS)
  // If there is a user manager, we need to see that we can at least have 2
  // simultaneous users to allow this feature.
  if (!user_manager::UserManager::IsInitialized())
    return false;
  size_t admitted_users_to_be_added =
      user_manager::UserManager::Get()->GetUsersAllowedForMultiProfile().size();
  size_t logged_in_users =
      user_manager::UserManager::Get()->GetLoggedInUsers().size();
  if (!logged_in_users) {
    // The shelf gets created on the login screen and as such we have to create
    // all multi profile items of the the system tray menu before the user logs
    // in. For special cases like Kiosk mode and / or guest mode this isn't a
    // problem since either the browser gets restarted and / or the flag is not
    // allowed, but for an "ephermal" user (see crbug.com/312324) it is not
    // decided yet if he could add other users to his session or not.
    // TODO(skuhne): As soon as the issue above needs to be resolved, this logic
    // should change.
    logged_in_users = 1;
  }
  if (admitted_users_to_be_added + logged_in_users <= 1)
    return false;
#endif
  return true;
}

bool ChromeShellDelegate::IsIncognitoAllowed() const {
#if defined(OS_CHROMEOS)
  return chromeos::AccessibilityManager::Get()->IsIncognitoAllowed();
#endif
  return true;
}

bool ChromeShellDelegate::IsRunningInForcedAppMode() const {
  return chrome::IsRunningInForcedAppMode();
}

bool ChromeShellDelegate::IsMultiAccountEnabled() const {
#if defined(OS_CHROMEOS)
  return switches::IsEnableAccountConsistency();
#endif
  return false;
}

bool ChromeShellDelegate::IsForceMaximizeOnFirstRun() const {
#if defined(OS_CHROMEOS)
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  if (user) {
    return chromeos::ProfileHelper::Get()
        ->GetProfileByUser(user)
        ->GetPrefs()
        ->GetBoolean(prefs::kForceMaximizeOnFirstRun);
  }
#endif
  return false;
}

void ChromeShellDelegate::Exit() {
  chrome::AttemptUserExit();
}

content::BrowserContext* ChromeShellDelegate::GetActiveBrowserContext() {
#if defined(OS_CHROMEOS)
  DCHECK(user_manager::UserManager::Get()->GetLoggedInUsers().size());
#endif
  return ProfileManager::GetActiveUserProfile();
}

app_list::AppListViewDelegate* ChromeShellDelegate::GetAppListViewDelegate() {
  DCHECK(ash::Shell::HasInstance());
  return AppListServiceAsh::GetInstance()->GetViewDelegate(
      Profile::FromBrowserContext(GetActiveBrowserContext()));
}

ash::ShelfDelegate* ChromeShellDelegate::CreateShelfDelegate(
    ash::ShelfModel* model) {
  if (!shelf_delegate_) {
    shelf_delegate_ = ChromeLauncherController::CreateInstance(NULL, model);
    shelf_delegate_->Init();
  }
  return shelf_delegate_;
}

ui::MenuModel* ChromeShellDelegate::CreateContextMenu(
    aura::Window* root,
    ash::ShelfItemDelegate* item_delegate,
    ash::ShelfItem* item) {
  DCHECK(shelf_delegate_);
  // Don't show context menu for exclusive app runtime mode.
  if (chrome::IsRunningInAppMode())
    return NULL;

  if (item_delegate && item)
    return new LauncherContextMenu(shelf_delegate_, item_delegate, item, root);

  return new LauncherContextMenu(shelf_delegate_, root);
}

ash::GPUSupport* ChromeShellDelegate::CreateGPUSupport() {
  // Chrome uses real GPU support.
  return new ash::GPUSupportImpl;
}

base::string16 ChromeShellDelegate::GetProductName() const {
  return l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
}

keyboard::KeyboardControllerProxy*
    ChromeShellDelegate::CreateKeyboardControllerProxy() {
  return new AshKeyboardControllerProxy(
      ProfileManager::GetActiveUserProfile());
}

void ChromeShellDelegate::VirtualKeyboardActivated(bool activated) {
  FOR_EACH_OBSERVER(ash::VirtualKeyboardStateObserver,
                    keyboard_state_observer_list_,
                    OnVirtualKeyboardStateChanged(activated));
}

void ChromeShellDelegate::AddVirtualKeyboardStateObserver(
    ash::VirtualKeyboardStateObserver* observer) {
  keyboard_state_observer_list_.AddObserver(observer);
}

void ChromeShellDelegate::RemoveVirtualKeyboardStateObserver(
    ash::VirtualKeyboardStateObserver* observer) {
  keyboard_state_observer_list_.RemoveObserver(observer);
}
