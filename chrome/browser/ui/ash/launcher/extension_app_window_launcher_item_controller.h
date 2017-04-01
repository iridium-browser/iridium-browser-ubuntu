// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_EXTENSION_APP_WINDOW_LAUNCHER_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_EXTENSION_APP_WINDOW_LAUNCHER_ITEM_CONTROLLER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"

namespace extensions {
class AppWindow;
}

class ChromeLauncherController;

class ExtensionAppWindowLauncherItemController
    : public AppWindowLauncherItemController {
 public:
  ExtensionAppWindowLauncherItemController(
      const std::string& app_id,
      const std::string& launch_id,
      ChromeLauncherController* controller);

  ~ExtensionAppWindowLauncherItemController() override;

  void AddAppWindow(extensions::AppWindow* app_window);

  // LauncherItemController overrides:
  ChromeLauncherAppMenuItems GetApplicationList(int event_flags) override;

 protected:
  // AppWindowLauncherItemController:
  void OnWindowRemoved(ui::BaseWindow* window) override;

 private:
  using WindowToAppWindow =
      std::map<const ui::BaseWindow*, extensions::AppWindow*>;

  WindowToAppWindow window_to_app_window_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAppWindowLauncherItemController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_EXTENSION_APP_WINDOW_LAUNCHER_ITEM_CONTROLLER_H_
