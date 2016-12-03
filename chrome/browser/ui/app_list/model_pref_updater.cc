// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/model_pref_updater.h"

#include "build/build_config.h"
#include "chrome/browser/ui/app_list/app_list_prefs.h"
#include "chrome/browser/ui/app_list/extension_app_item.h"
#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/app_list/arc/arc_app_item.h"
#endif

namespace app_list {

ModelPrefUpdater::ModelPrefUpdater(AppListPrefs* app_list_prefs,
                                   AppListModel* model)
    : app_list_prefs_(app_list_prefs), model_(model) {
  model_->AddObserver(this);
}

ModelPrefUpdater::~ModelPrefUpdater() {
  model_->RemoveObserver(this);
}

void ModelPrefUpdater::OnAppListItemAdded(AppListItem* item) {
  UpdatePrefsFromAppListItem(item);
}

void ModelPrefUpdater::OnAppListItemWillBeDeleted(AppListItem* item) {
  app_list_prefs_->DeleteAppListInfo(item->id());
}

void ModelPrefUpdater::OnAppListItemUpdated(AppListItem* item) {
  UpdatePrefsFromAppListItem(item);
}

void ModelPrefUpdater::UpdatePrefsFromAppListItem(AppListItem* item) {
  // Write synced data to local pref.
  AppListPrefs::AppListInfo info;
  if (item->GetItemType() == AppListFolderItem::kItemType)
    info.item_type = AppListPrefs::AppListInfo::FOLDER_ITEM;
  else if (item->GetItemType() == ExtensionAppItem::kItemType)
    info.item_type = AppListPrefs::AppListInfo::APP_ITEM;
#if defined(OS_CHROMEOS)
  else if (item->GetItemType() == ArcAppItem::kItemType)
    info.item_type = AppListPrefs::AppListInfo::APP_ITEM;
#endif
  else
    NOTREACHED();

  info.parent_id = item->folder_id();
  info.position = item->position();
  info.name = item->name();

  app_list_prefs_->SetAppListInfo(item->id(), info);
}

}  // namespace app_list
