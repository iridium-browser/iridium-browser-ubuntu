// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_BRIDGE_WM_SHELF_MUS_H_
#define ASH_MUS_BRIDGE_WM_SHELF_MUS_H_

#include "ash/common/shelf/wm_shelf.h"
#include "base/macros.h"

namespace ash {

class Shelf;
class ShelfWidget;
class WmRootWindowController;

namespace mus {

// WmShelf implementation for mus.
class WmShelfMus : public WmShelf {
 public:
  WmShelfMus(WmRootWindowController* root_window_controller);
  ~WmShelfMus() override;

  // WmShelf:
  void WillDeleteShelfLayoutManager() override;

 private:
  // Legacy shelf controller. Only present after shelf is created (post-login).
  std::unique_ptr<Shelf> shelf_;

  // The shelf widget for this shelf.
  std::unique_ptr<ShelfWidget> shelf_widget_;

  DISALLOW_COPY_AND_ASSIGN(WmShelfMus);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_BRIDGE_WM_SHELF_MUS_H_
