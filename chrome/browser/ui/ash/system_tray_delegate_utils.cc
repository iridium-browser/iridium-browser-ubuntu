// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_delegate_utils.h"

#include "ash/common/system/tray/system_tray_delegate.h"
#include "base/logging.h"
#include "chrome/browser/upgrade_detector.h"

void GetUpdateInfo(const UpgradeDetector* detector, ash::UpdateInfo* info) {
  DCHECK(detector);
  DCHECK(info);
  switch (detector->upgrade_notification_stage()) {
    case UpgradeDetector::UPGRADE_ANNOYANCE_CRITICAL:
      info->severity = ash::UpdateInfo::UPDATE_CRITICAL;
      break;
    case UpgradeDetector::UPGRADE_ANNOYANCE_SEVERE:
      info->severity = ash::UpdateInfo::UPDATE_SEVERE;
      break;
    case UpgradeDetector::UPGRADE_ANNOYANCE_HIGH:
      info->severity = ash::UpdateInfo::UPDATE_HIGH;
      break;
    case UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED:
      info->severity = ash::UpdateInfo::UPDATE_ELEVATED;
      break;
    case UpgradeDetector::UPGRADE_ANNOYANCE_LOW:
      info->severity = ash::UpdateInfo::UPDATE_LOW;
      break;
    case UpgradeDetector::UPGRADE_ANNOYANCE_NONE:
      info->severity = ash::UpdateInfo::UPDATE_NONE;
      break;
  }
  info->update_required = detector->notify_upgrade();
  info->factory_reset_required = detector->is_factory_reset_required();
}
