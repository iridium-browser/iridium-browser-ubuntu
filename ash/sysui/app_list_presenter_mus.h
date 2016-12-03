// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSUI_APP_LIST_PRESENTER_MUS_H_
#define ASH_SYSUI_APP_LIST_PRESENTER_MUS_H_

#include "base/macros.h"
#include "ui/app_list/presenter/app_list_presenter.h"
#include "ui/app_list/presenter/app_list_presenter.mojom.h"

namespace shell {
class Connector;
}

namespace ash {

// Mus+ash implementation of the AppListPresenter interface for mash, which
// talks to the mojo app list service in chrome.
class AppListPresenterMus : public app_list::AppListPresenter {
 public:
  explicit AppListPresenterMus(::shell::Connector* connector);
  ~AppListPresenterMus() override;

  // app_list::AppListPresenter:
  void Show(int64_t display_id) override;
  void Dismiss() override;
  void ToggleAppList(int64_t display_id) override;
  bool IsVisible() const override;
  bool GetTargetVisibility() const override;

 private:
  // Connect to the mojo app list service in chrome if the connection hasn't
  // been established or has an error.
  bool ConnectIfNeeded();

  ::shell::Connector* connector_;
  app_list::mojom::AppListPresenterPtr presenter_;

  DISALLOW_COPY_AND_ASSIGN(AppListPresenterMus);
};

}  // namespace ash

#endif  // ASH_SYSUI_APP_LIST_PRESENTER_MUS_H_
