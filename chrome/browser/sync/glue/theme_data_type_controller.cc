// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/theme_data_type_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"

using content::BrowserThread;

namespace browser_sync {

ThemeDataTypeController::ThemeDataTypeController(
    const base::Closure& error_callback,
    sync_driver::SyncClient* sync_client,
    Profile* profile)
    : UIDataTypeController(
          BrowserThread::GetTaskRunnerForThread(BrowserThread::UI),
          error_callback,
          syncer::THEMES,
          sync_client),
      profile_(profile) {}

ThemeDataTypeController::~ThemeDataTypeController() {}

bool ThemeDataTypeController::StartModels() {
  extensions::ExtensionSystem::Get(profile_)->InitForRegularProfile(true);
  return true;
}

}  // namespace browser_sync
