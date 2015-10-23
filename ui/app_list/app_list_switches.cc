// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_switches.h"

#include "base/command_line.h"

namespace app_list {
namespace switches {

// Specifies the chrome-extension:// URL for the contents of an additional page
// added to the experimental app launcher.
const char kCustomLauncherPage[] = "custom-launcher-page";

// If set, the app list will not be dismissed when it loses focus. This is
// useful when testing the app list or a custom launcher page. It can still be
// dismissed via the other methods (like the Esc key).
const char kDisableAppListDismissOnBlur[] = "disable-app-list-dismiss-on-blur";

// If set, Drive apps will not be shown side-by-side with Chrome apps.
const char kDisableDriveAppsInAppList[] = "disable-drive-apps-in-app-list";

// If set, the app list will be enabled as if enabled from CWS.
const char kEnableAppList[] = "enable-app-list";

// If set, the app list will be centered and wide instead of tall.
const char kEnableCenteredAppList[] = "enable-centered-app-list";

// Enable/disable the experimental app list. If enabled, implies
// --enable-centered-app-list.
const char kEnableExperimentalAppList[] = "enable-experimental-app-list";
const char kDisableExperimentalAppList[] = "disable-experimental-app-list";

// Enable/disable syncing of the app list independent of extensions.
const char kEnableSyncAppList[] = "enable-sync-app-list";
const char kDisableSyncAppList[] = "disable-sync-app-list";

// Enable/disable drive search in chrome launcher.
const char kEnableDriveSearchInChromeLauncher[] =
    "enable-drive-search-in-app-launcher";
const char kDisableDriveSearchInChromeLauncher[] =
    "disable-drive-search-in-app-launcher";

// Enable/disable the new "blended" algorithm in app_list::Mixer. This is just
// forcing the AppListMixer/Blended field trial.
const char kEnableNewAppListMixer[] = "enable-new-app-list-mixer";
const char kDisableNewAppListMixer[] = "disable-new-app-list-mixer";

// If set, the app list will forget it has been installed on startup. Note this
// doesn't prevent the app list from running, it just makes Chrome think the app
// list hasn't been enabled (as in kEnableAppList) yet.
const char kResetAppListInstallState[] = "reset-app-list-install-state";

#if defined(OS_MACOSX)
// Enables use of the toolkit-views app list on Mac.
const char kEnableMacViewsAppList[] = "enable-mac-views-app-list";
#endif

bool IsAppListSyncEnabled() {
#if defined(OS_MACOSX)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kEnableSyncAppList))
    return true;

  if (!IsMacViewsAppListEnabled())
    return false;
#endif
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableSyncAppList);
}

bool IsFolderUIEnabled() {
  // Folder UI is available only when AppList sync is enabled, and should
  // not be disabled separately.
  return IsAppListSyncEnabled();
}

bool IsVoiceSearchEnabled() {
  // Speech recognition in AppList is only for ChromeOS right now.
#if defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

bool IsExperimentalAppListEnabled() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kEnableExperimentalAppList))
    return true;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kDisableExperimentalAppList))
    return false;

#if defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

bool IsCenteredAppListEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             kEnableCenteredAppList) ||
         IsExperimentalAppListEnabled();
}

bool ShouldNotDismissOnBlur() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableAppListDismissOnBlur);
}

bool IsDriveAppsInAppListEnabled() {
#if defined(OS_CHROMEOS)
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableDriveAppsInAppList);
#else
  return false;
#endif
}

bool IsDriveSearchInChromeLauncherEnabled() {
#if defined(OS_CHROMEOS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kEnableDriveSearchInChromeLauncher))
    return true;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kDisableDriveSearchInChromeLauncher))
    return false;

  return true;
#else
  return false;
#endif
}

#if defined(OS_MACOSX)
bool IsMacViewsAppListEnabled() {
#if defined(TOOLKIT_VIEWS)
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kEnableMacViewsAppList);
#endif
  return false;
}
#endif  // defined(OS_MACOSX)

}  // namespace switches
}  // namespace app_list
