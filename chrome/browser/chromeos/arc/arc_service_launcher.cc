// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_service_launcher.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/arc/arc_auth_service.h"
#include "chrome/browser/chromeos/arc/arc_boot_error_notification.h"
#include "chrome/browser/chromeos/arc/arc_downloads_watcher_service.h"
#include "chrome/browser/chromeos/arc/arc_enterprise_reporting_service.h"
#include "chrome/browser/chromeos/arc/arc_policy_bridge.h"
#include "chrome/browser/chromeos/arc/arc_print_service.h"
#include "chrome/browser/chromeos/arc/arc_process_service.h"
#include "chrome/browser/chromeos/arc/arc_settings_service.h"
#include "chrome/browser/chromeos/arc/arc_tts_service.h"
#include "chrome/browser/chromeos/arc/arc_wallpaper_handler.h"
#include "chrome/browser/chromeos/arc/gpu_arc_video_service_host.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "content/public/browser/browser_thread.h"

namespace arc {

ArcServiceLauncher::ArcServiceLauncher() = default;

ArcServiceLauncher::~ArcServiceLauncher() = default;

void ArcServiceLauncher::Initialize() {
  // Create ARC service manager.
  arc_service_manager_ = base::MakeUnique<ArcServiceManager>(
      content::BrowserThread::GetBlockingPool());
  arc_service_manager_->AddService(base::MakeUnique<ArcAuthService>(
      arc_service_manager_->arc_bridge_service()));
  arc_service_manager_->AddService(base::MakeUnique<ArcBootErrorNotification>(
      arc_service_manager_->arc_bridge_service()));
  arc_service_manager_->AddService(base::MakeUnique<ArcDownloadsWatcherService>(
      arc_service_manager_->arc_bridge_service()));
  arc_service_manager_->AddService(
      base::MakeUnique<ArcEnterpriseReportingService>(
          arc_service_manager_->arc_bridge_service()));
  arc_service_manager_->AddService(base::MakeUnique<ArcIntentHelperBridge>(
      arc_service_manager_->arc_bridge_service(),
      arc_service_manager_->icon_loader(),
      base::MakeUnique<ArcWallpaperHandler>(),
      arc_service_manager_->activity_resolver()));
  arc_service_manager_->AddService(base::MakeUnique<ArcPolicyBridge>(
      arc_service_manager_->arc_bridge_service()));
  arc_service_manager_->AddService(base::MakeUnique<ArcPrintService>(
      arc_service_manager_->arc_bridge_service()));
  arc_service_manager_->AddService(base::MakeUnique<ArcProcessService>(
      arc_service_manager_->arc_bridge_service()));
  arc_service_manager_->AddService(base::MakeUnique<ArcSettingsService>(
      arc_service_manager_->arc_bridge_service()));
  arc_service_manager_->AddService(base::MakeUnique<ArcTtsService>(
      arc_service_manager_->arc_bridge_service()));
  arc_service_manager_->AddService(base::MakeUnique<GpuArcVideoServiceHost>(
      arc_service_manager_->arc_bridge_service()));
}

void ArcServiceLauncher::Shutdown() {
  DCHECK(arc_service_manager_);
  arc_service_manager_->Shutdown();
  arc_service_manager_->arc_bridge_service()->Shutdown();
}

}  // namespace arc
