// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_APP_LIST_BACKGROUND_H_
#define UI_APP_LIST_VIEWS_APP_LIST_BACKGROUND_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/views/background.h"

namespace views {
class View;
}

namespace app_list {

class AppListMainView;

// A class to paint bubble background.
class AppListBackground : public views::Background {
 public:
  explicit AppListBackground(int corner_radius);
  ~AppListBackground() override;

 private:
  // views::Background overrides:
  void Paint(gfx::Canvas* canvas, views::View* view) const override;

  const int corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(AppListBackground);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_APP_LIST_BACKGROUND_H_
