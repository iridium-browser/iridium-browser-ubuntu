// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_SHAPED_APP_WINDOW_TARGETER_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_SHAPED_APP_WINDOW_TARGETER_H_

#include "base/macros.h"
#include "ui/wm/core/masked_window_targeter.h"

class ChromeNativeAppWindowViews;

class ShapedAppWindowTargeter : public wm::MaskedWindowTargeter {
 public:
  ShapedAppWindowTargeter(aura::Window* window,
                          ChromeNativeAppWindowViews* app_window);
  ~ShapedAppWindowTargeter() override;

 private:
  // wm::MaskedWindowTargeter:
  bool GetHitTestMask(aura::Window* window, gfx::Path* mask) const override;

  ChromeNativeAppWindowViews* app_window_;

  DISALLOW_COPY_AND_ASSIGN(ShapedAppWindowTargeter);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_SHAPED_APP_WINDOW_TARGETER_H_
