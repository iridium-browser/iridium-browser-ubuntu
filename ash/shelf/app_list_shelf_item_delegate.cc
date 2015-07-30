// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/app_list_shelf_item_delegate.h"

#include "ash/shelf/shelf_model.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "grit/ash_strings.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

AppListShelfItemDelegate::AppListShelfItemDelegate() {
  ShelfItem app_list;
  app_list.type = TYPE_APP_LIST;
  Shell::GetInstance()->shelf_model()->Add(app_list);
}

AppListShelfItemDelegate::~AppListShelfItemDelegate() {
  // ShelfItemDelegateManager owns and destroys this class.
}

ShelfItemDelegate::PerformedAction AppListShelfItemDelegate::ItemSelected(
    const ui::Event& event) {
  // Pass NULL here to show the app list in the currently active RootWindow.
  Shell::GetInstance()->ToggleAppList(NULL);
  return ShelfItemDelegate::kAppListMenuShown;
}

base::string16 AppListShelfItemDelegate::GetTitle() {
  ShelfModel* model = Shell::GetInstance()->shelf_model();
  DCHECK(model);
  int title_id;
  if (app_list::switches::IsExperimentalAppListEnabled()) {
    title_id = model->status() == ShelfModel::STATUS_LOADING
                   ? IDS_ASH_SHELF_APP_LIST_LAUNCHER_SYNCING_TITLE
                   : IDS_ASH_SHELF_APP_LIST_LAUNCHER_TITLE;
  } else {
    title_id = model->status() == ShelfModel::STATUS_LOADING
                   ? IDS_ASH_SHELF_APP_LIST_SYNCING_TITLE
                   : IDS_ASH_SHELF_APP_LIST_TITLE;
  }
  return l10n_util::GetStringUTF16(title_id);
}

ui::MenuModel* AppListShelfItemDelegate::CreateContextMenu(
    aura::Window* root_window) {
  return Shell::GetInstance()->delegate()->CreateContextMenu(root_window,
                                                             NULL,
                                                             NULL);
}

ShelfMenuModel* AppListShelfItemDelegate::CreateApplicationMenu(
    int event_flags) {
  // AppList does not show an application menu.
  return NULL;
}

bool AppListShelfItemDelegate::IsDraggable() {
  return false;
}

bool AppListShelfItemDelegate::ShouldShowTooltip() {
  return true;
}

void AppListShelfItemDelegate::Close() {
}

}  // namespace ash
