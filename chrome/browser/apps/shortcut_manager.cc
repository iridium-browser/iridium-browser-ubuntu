// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/shortcut_manager.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/one_shot_event.h"

using extensions::Extension;

namespace {

// This version number is stored in local prefs to check whether app shortcuts
// need to be recreated. This might happen when we change various aspects of app
// shortcuts like command-line flags or associated icons, binaries, etc.
#if defined(OS_MACOSX)
// This was changed to 3 at r316520, but reverted again. Next time we need to
// trigger a recreate, increment this to 4.
const int kCurrentAppShortcutsVersion = 2;
#else
const int kCurrentAppShortcutsVersion = 0;
#endif

// Delay in seconds before running UpdateShortcutsForAllApps.
const int kUpdateShortcutsForAllAppsDelay = 10;

void CreateShortcutsForApp(Profile* profile, const Extension* app) {
  web_app::ShortcutLocations creation_locations;

  if (extensions::util::IsEphemeralApp(app->id(), profile)) {
    // Ephemeral apps should not have visible shortcuts, but may still require
    // platform-specific handling.
    creation_locations.applications_menu_location =
        web_app::APP_MENU_LOCATION_HIDDEN;
  } else {
    // Creates a shortcut for an app in the Chrome Apps subdir of the
    // applications menu, if there is not already one present.
    creation_locations.applications_menu_location =
        web_app::APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;
  }

  web_app::CreateShortcuts(
      web_app::SHORTCUT_CREATION_AUTOMATED, creation_locations, profile, app);
}

void SetCurrentAppShortcutsVersion(PrefService* prefs) {
  prefs->SetInteger(prefs::kAppShortcutsVersion, kCurrentAppShortcutsVersion);
}

}  // namespace

// static
void AppShortcutManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // Indicates whether app shortcuts have been created.
  registry->RegisterIntegerPref(prefs::kAppShortcutsVersion, 0);
}

AppShortcutManager::AppShortcutManager(Profile* profile)
    : profile_(profile),
      is_profile_info_cache_observer_(false),
      prefs_(profile->GetPrefs()),
      extension_registry_observer_(this),
      weak_ptr_factory_(this) {
  // Use of g_browser_process requires that we are either on the UI thread, or
  // there are no threads initialized (such as in unit tests).
  DCHECK(!content::BrowserThread::IsThreadInitialized(
             content::BrowserThread::UI) ||
         content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  extension_registry_observer_.Add(
      extensions::ExtensionRegistry::Get(profile_));
  // Wait for extensions to be ready before running
  // UpdateShortcutsForAllAppsIfNeeded.
  extensions::ExtensionSystem::Get(profile)->ready().Post(
      FROM_HERE,
      base::Bind(&AppShortcutManager::UpdateShortcutsForAllAppsIfNeeded,
                 weak_ptr_factory_.GetWeakPtr()));

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // profile_manager might be NULL in testing environments.
  if (profile_manager) {
    profile_manager->GetProfileInfoCache().AddObserver(this);
    is_profile_info_cache_observer_ = true;
  }
}

AppShortcutManager::~AppShortcutManager() {
  if (g_browser_process && is_profile_info_cache_observer_) {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    // profile_manager might be NULL in testing environments or during shutdown.
    if (profile_manager)
      profile_manager->GetProfileInfoCache().RemoveObserver(this);
  }
}

void AppShortcutManager::OnExtensionWillBeInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update,
    bool from_ephemeral,
    const std::string& old_name) {
  if (!extension->is_app())
    return;

  // If the app is being updated, update any existing shortcuts but do not
  // create new ones. If it is being installed, automatically create a
  // shortcut in the applications menu (e.g., Start Menu).
  if (is_update && !from_ephemeral) {
    web_app::UpdateAllShortcuts(
        base::UTF8ToUTF16(old_name), profile_, extension);
  } else {
    CreateShortcutsForApp(profile_, extension);
  }
}

void AppShortcutManager::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  web_app::DeleteAllShortcuts(profile_, extension);
}

void AppShortcutManager::OnProfileWillBeRemoved(
    const base::FilePath& profile_path) {
  if (profile_path != profile_->GetPath())
    return;
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&web_app::internals::DeleteAllShortcutsForProfile,
                 profile_path));
}

void AppShortcutManager::UpdateShortcutsForAllAppsIfNeeded() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType))
    return;

  int last_version = prefs_->GetInteger(prefs::kAppShortcutsVersion);
  if (last_version >= kCurrentAppShortcutsVersion)
    return;

  content::BrowserThread::PostDelayedTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&web_app::UpdateShortcutsForAllApps,
                 profile_,
                 base::Bind(&SetCurrentAppShortcutsVersion, prefs_)),
      base::TimeDelta::FromSeconds(kUpdateShortcutsForAllAppsDelay));
}
