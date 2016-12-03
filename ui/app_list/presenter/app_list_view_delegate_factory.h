// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_PRESENTER_APP_LIST_VIEW_DELEGATE_FACTORY_H_
#define UI_APP_LIST_PRESENTER_APP_LIST_VIEW_DELEGATE_FACTORY_H_

#include "ui/app_list/presenter/app_list_presenter_export.h"

namespace app_list {
class AppListViewDelegate;

class APP_LIST_PRESENTER_EXPORT AppListViewDelegateFactory {
 public:
  virtual ~AppListViewDelegateFactory();

  virtual app_list::AppListViewDelegate* GetDelegate() = 0;
};

}  // namespace app_list

#endif  // UI_APP_LIST_PRESENTER_APP_LIST_VIEW_DELEGATE_FACTORY_H_
