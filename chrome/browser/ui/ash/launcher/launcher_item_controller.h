// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_ITEM_CONTROLLER_H_

#include <string>

#include "ash/shelf/shelf_item_delegate.h"
#include "ash/shelf/shelf_item_types.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_types.h"
#include "ui/events/event.h"

class ChromeLauncherController;
class ChromeLauncherAppMenuItem;

typedef ScopedVector<ChromeLauncherAppMenuItem> ChromeLauncherAppMenuItems;

namespace aura {
class Window;
}

namespace content {
class WebContents;
}

// LauncherItemController is used by ChromeLauncherController to track one
// or more windows associated with a shelf item.
class LauncherItemController : public ash::ShelfItemDelegate {
 public:
  enum Type {
    TYPE_APP,
    TYPE_APP_PANEL,
    TYPE_SHORTCUT,
    TYPE_WINDOWED_APP
  };

  LauncherItemController(Type type,
                         const std::string& app_id,
                         ChromeLauncherController* launcher_controller);
  ~LauncherItemController() override;

  Type type() const { return type_; }
  ash::ShelfID shelf_id() const { return shelf_id_; }
  void set_shelf_id(ash::ShelfID id) { shelf_id_ = id; }
  virtual const std::string& app_id() const;
  ChromeLauncherController* launcher_controller() const {
    return launcher_controller_;
  }

  // Lock this item to the launcher without being pinned (windowed v1 apps).
  void lock() { locked_++; }
  void unlock() {
    DCHECK(locked_);
    locked_--;
  }
  bool locked() { return locked_ > 0; }

  bool image_set_by_controller() const { return image_set_by_controller_; }
  void set_image_set_by_controller(bool image_set_by_controller) {
    image_set_by_controller_ = image_set_by_controller;
  }

  // Returns true if this item is open.
  virtual bool IsOpen() const = 0;

  // Returns true if this item is visible (e.g. not minimized).
  virtual bool IsVisible() const = 0;

  // Launches a new instance of the app associated with this item.
  virtual void Launch(ash::LaunchSource source, int event_flags) = 0;

  // Shows and activates the most-recently-active window associated with the
  // item, or launches the item if it is not currently open.
  // Returns the action performed by activating the item.
  virtual PerformedAction Activate(ash::LaunchSource source) = 0;

  // Called to retrieve the list of running applications.
  virtual ChromeLauncherAppMenuItems GetApplicationList(int event_flags) = 0;

  // Helper function to get the ash::ShelfItemType for the item type.
  ash::ShelfItemType GetShelfItemType() const;

 protected:
  // Helper function to return the title associated with |app_id_|.
  // Returns an empty title if no matching extension can be found.
  base::string16 GetAppTitle() const;

 private:
  const Type type_;
  // App id will be empty if there is no app associated with the window.
  const std::string app_id_;
  ash::ShelfID shelf_id_;
  ChromeLauncherController* launcher_controller_;

  // The lock counter which tells the launcher if the item can be removed from
  // the launcher (0) or not (>0). It is being used for windowed V1
  // applications.
  int locked_;

  // Set to true if the launcher item image has been set by the controller.
  bool image_set_by_controller_;

  DISALLOW_COPY_AND_ASSIGN(LauncherItemController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_ITEM_CONTROLLER_H_
