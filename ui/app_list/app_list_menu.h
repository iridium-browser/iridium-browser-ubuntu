// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_MENU_H_
#define UI_APP_LIST_APP_LIST_MENU_H_

#include "base/macros.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/base/models/simple_menu_model.h"

namespace app_list {

// Menu for the app list. This is shown in the top right hand corner of the
// app list.
// TODO(benwells): We should delete this. http://crbug.com/600915.
class AppListMenu : public ui::SimpleMenuModel::Delegate {
 public:
  enum AppListMenuCommands {
    SHOW_SETTINGS,
    SHOW_HELP,
    SHOW_FEEDBACK,
    // |SELECT_PROFILE| must remain the last enum because values greater than
    // |SELECT_PROFILE| are used to indicate a profile index.
    SELECT_PROFILE,
  };

  explicit AppListMenu(AppListViewDelegate* delegate);
  ~AppListMenu() override;

  ui::SimpleMenuModel* menu_model() { return &menu_model_; }

 private:
  void InitMenu();

  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  ui::SimpleMenuModel menu_model_;
  AppListViewDelegate* delegate_;
  AppListViewDelegate::Users users_;

  DISALLOW_COPY_AND_ASSIGN(AppListMenu);
};

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_MENU_H_
