// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/chrome_browser_operations.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/user_experiment.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {

void ChromeBrowserOperations::ReadOptions(const MasterPreferences& prefs,
                                          std::set<base::string16>* options)
    const {
  DCHECK(options);

  bool pref_value;

  if (prefs.GetBool(master_preferences::kMultiInstall, &pref_value) &&
      pref_value) {
    options->insert(kOptionMultiInstall);
  }
}

void ChromeBrowserOperations::ReadOptions(
    const base::CommandLine& uninstall_command,
    std::set<base::string16>* options) const {
  DCHECK(options);

  if (uninstall_command.HasSwitch(switches::kMultiInstall))
    options->insert(kOptionMultiInstall);
}

void ChromeBrowserOperations::AddKeyFiles(
    const std::set<base::string16>& options,
    std::vector<base::FilePath>* key_files) const {
  DCHECK(key_files);
  key_files->push_back(base::FilePath(installer::kChromeDll));
}

void ChromeBrowserOperations::AddComDllList(
    const std::set<base::string16>& options,
    std::vector<base::FilePath>* com_dll_list) const {
}

void ChromeBrowserOperations::AppendProductFlags(
    const std::set<base::string16>& options,
    base::CommandLine* cmd_line) const {
  DCHECK(cmd_line);

  if (options.find(kOptionMultiInstall) != options.end()) {
    // Add --multi-install if it isn't already there.
    if (!cmd_line->HasSwitch(switches::kMultiInstall))
      cmd_line->AppendSwitch(switches::kMultiInstall);

    // --chrome is only needed in multi-install.
    cmd_line->AppendSwitch(switches::kChrome);
  }
}

void ChromeBrowserOperations::AppendRenameFlags(
    const std::set<base::string16>& options,
    base::CommandLine* cmd_line) const {
  DCHECK(cmd_line);

  // Add --multi-install if it isn't already there.
  if (options.find(kOptionMultiInstall) != options.end() &&
      !cmd_line->HasSwitch(switches::kMultiInstall)) {
    cmd_line->AppendSwitch(switches::kMultiInstall);
  }
}

bool ChromeBrowserOperations::SetChannelFlags(
    const std::set<base::string16>& options,
    bool set,
    ChannelInfo* channel_info) const {
#if defined(GOOGLE_CHROME_BUILD)
  DCHECK(channel_info);
  bool chrome_changed = channel_info->SetChrome(set);
  // Remove App Launcher's channel flags, since App Launcher does not exist as
  // an independent product, and is a part of Chrome.
  bool app_launcher_changed = channel_info->SetAppLauncher(false);
  return chrome_changed || app_launcher_changed;
#else
  return false;
#endif
}

bool ChromeBrowserOperations::ShouldCreateUninstallEntry(
    const std::set<base::string16>& options) const {
  return true;
}

// Modifies a ShortcutProperties object by adding default values to
// uninitialized members. Tries to assign:
// - target: |chrome_exe|.
// - icon: from |chrome_exe|.
// - icon_index: |dist|'s icon index
// - app_id: the browser model id for the current install.
// - description: |dist|'s description.
void ChromeBrowserOperations::AddDefaultShortcutProperties(
      BrowserDistribution* dist,
      const base::FilePath& target_exe,
      ShellUtil::ShortcutProperties* properties) const {
  if (!properties->has_target())
    properties->set_target(target_exe);

  if (!properties->has_icon())
    properties->set_icon(target_exe, dist->GetIconIndex());

  if (!properties->has_app_id()) {
    properties->set_app_id(ShellUtil::GetBrowserModelId(
        dist, InstallUtil::IsPerUserInstall(target_exe)));
  }

  if (!properties->has_description())
    properties->set_description(dist->GetAppDescription());
}

void ChromeBrowserOperations::LaunchUserExperiment(
    const base::FilePath& setup_path,
    const std::set<base::string16>& options,
    InstallStatus status,
    bool system_level) const {
  base::CommandLine base_command(setup_path);
  AppendProductFlags(options, &base_command);
  installer::LaunchBrowserUserExperiment(base_command, status, system_level);
}

}  // namespace installer
