// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_APPS_GRID_VIEW_DELEGATE_H_
#define UI_APP_LIST_VIEWS_APPS_GRID_VIEW_DELEGATE_H_

#include <string>

#include "base/callback_forward.h"
#include "ui/app_list/app_list_export.h"

namespace base {
class FilePath;
}

namespace app_list {

class AppListItem;

class APP_LIST_EXPORT AppsGridViewDelegate {
 public:
  // Invoked when an item is activated on the grid view. |event_flags| contains
  // the flags of the keyboard/mouse event that triggers the activation request.
  virtual void ActivateApp(AppListItem* item, int event_flags) = 0;

  // Called by the root grid view to cancel a drag that started inside a folder.
  // This can occur when the root grid is visible for a reparent and its model
  // changes, necessitating a cancel of the drag operation.
  virtual void CancelDragInActiveFolder() = 0;

 protected:
  virtual ~AppsGridViewDelegate() {}
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_APPS_GRID_VIEW_DELEGATE_H_
