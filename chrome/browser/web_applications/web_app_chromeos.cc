// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app.h"

namespace web_app {

void UpdateShortcutsForAllApps(Profile* profile,
                               const base::Closure& callback) {
  callback.Run();
}

namespace internals {

bool CreatePlatformShortcuts(
    const base::FilePath& web_app_path,
    scoped_ptr<ShortcutInfo> shortcut_info,
    const extensions::FileHandlersInfo& file_handlers_info,
    const ShortcutLocations& creation_locations,
    ShortcutCreationReason creation_reason) {
  return true;
}

void DeletePlatformShortcuts(const base::FilePath& web_app_path,
                             scoped_ptr<ShortcutInfo> shortcut_info) {}

void UpdatePlatformShortcuts(
    const base::FilePath& web_app_path,
    const base::string16& old_app_title,
    scoped_ptr<ShortcutInfo> shortcut_info,
    const extensions::FileHandlersInfo& file_handlers_info) {}

void DeleteAllShortcutsForProfile(const base::FilePath& profile_path) {}

}  // namespace internals
}  // namespace web_app
