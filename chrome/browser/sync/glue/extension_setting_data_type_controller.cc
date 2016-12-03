// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_setting_data_type_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sync/api/syncable_service.h"
#include "components/sync/driver/generic_change_processor.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"

using content::BrowserThread;

namespace browser_sync {

ExtensionSettingDataTypeController::ExtensionSettingDataTypeController(
    syncer::ModelType type,
    const base::Closure& error_callback,
    sync_driver::SyncClient* sync_client,
    Profile* profile)
    : NonUIDataTypeController(
          BrowserThread::GetTaskRunnerForThread(BrowserThread::UI),
          error_callback,
          sync_client),
      type_(type),
      profile_(profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(type == syncer::EXTENSION_SETTINGS || type == syncer::APP_SETTINGS);
}

syncer::ModelType ExtensionSettingDataTypeController::type() const {
  return type_;
}

syncer::ModelSafeGroup
ExtensionSettingDataTypeController::model_safe_group() const {
  return syncer::GROUP_FILE;
}

ExtensionSettingDataTypeController::~ExtensionSettingDataTypeController() {}

bool ExtensionSettingDataTypeController::PostTaskOnBackendThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return BrowserThread::PostTask(BrowserThread::FILE, from_here, task);
}

bool ExtensionSettingDataTypeController::StartModels() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  extensions::ExtensionSystem::Get(profile_)->InitForRegularProfile(true);
  return true;
}

}  // namespace browser_sync
