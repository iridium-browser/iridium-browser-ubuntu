// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/arc_app_deferred_launcher_item_controller.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/ash/launcher/arc_app_deferred_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

ArcAppDeferredLauncherItemController::ArcAppDeferredLauncherItemController(
    const std::string& arc_app_id,
    ChromeLauncherController* controller,
    int event_flags,
    const base::WeakPtr<ArcAppDeferredLauncherController>& host)
    : LauncherItemController(arc_app_id, "", controller),
      event_flags_(event_flags),
      host_(host),
      start_time_(base::Time::Now()) {}

ArcAppDeferredLauncherItemController::~ArcAppDeferredLauncherItemController() {
  if (host_)
    host_->Remove(app_id());
}

base::TimeDelta ArcAppDeferredLauncherItemController::GetActiveTime() const {
  return base::Time::Now() - start_time_;
}

ash::ShelfItemDelegate::PerformedAction
ArcAppDeferredLauncherItemController::ItemSelected(const ui::Event& event) {
  return ash::ShelfItemDelegate::kNoAction;
}

ash::ShelfMenuModel*
ArcAppDeferredLauncherItemController::CreateApplicationMenu(int event_flags) {
  return nullptr;
}

void ArcAppDeferredLauncherItemController::Close() {
  if (host_)
    host_->Close(app_id());
}

void ArcAppDeferredLauncherItemController::Launch(ash::LaunchSource source,
                                                  int event_flags) {}

ash::ShelfItemDelegate::PerformedAction
ArcAppDeferredLauncherItemController::Activate(ash::LaunchSource source) {
  return ash::ShelfItemDelegate::kNoAction;
}

ChromeLauncherAppMenuItems
ArcAppDeferredLauncherItemController::GetApplicationList(int event_flags) {
  return ChromeLauncherAppMenuItems();
}
