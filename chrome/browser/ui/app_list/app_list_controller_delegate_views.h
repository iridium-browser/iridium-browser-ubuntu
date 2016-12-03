// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_CONTROLLER_DELEGATE_VIEWS_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_CONTROLLER_DELEGATE_VIEWS_H_

#include "base/macros.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate_impl.h"

class AppListServiceViews;

// Conveys messages from a views-backed app list to the AppListService that
// created it.
class AppListControllerDelegateViews : public AppListControllerDelegateImpl {
 public:
  explicit AppListControllerDelegateViews(AppListServiceViews* service);
  ~AppListControllerDelegateViews() override;

  // AppListControllerDelegate overrides:
  gfx::Rect GetAppListBounds() override;
  void ViewClosing() override;
  void OnShowChildDialog() override;
  void OnCloseChildDialog() override;
  bool CanDoCreateShortcutsFlow() override;

 private:
  AppListServiceViews* service_;  // Weak. Owns us.

  DISALLOW_COPY_AND_ASSIGN(AppListControllerDelegateViews);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_CONTROLLER_DELEGATE_VIEWS_H_
