// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_SHELF_WINDOW_WATCHER_ITEM_DELEGATE_H_
#define ASH_COMMON_SHELF_SHELF_WINDOW_WATCHER_ITEM_DELEGATE_H_

#include "ash/common/shelf/shelf_item_delegate.h"
#include "ash/common/shelf/shelf_item_types.h"
#include "base/macros.h"

namespace ash {

class WmWindow;

// ShelfItemDelegate for the items created by ShelfWindowWatcher.
class ShelfWindowWatcherItemDelegate : public ShelfItemDelegate {
 public:
  ShelfWindowWatcherItemDelegate(ShelfID id, WmWindow* window);
  ~ShelfWindowWatcherItemDelegate() override;

 private:
  // ShelfItemDelegate overrides:
  ShelfItemDelegate::PerformedAction ItemSelected(
      const ui::Event& event) override;
  ShelfMenuModel* CreateApplicationMenu(int event_flags) override;
  void Close() override;

  ShelfID id_;
  // The window associated with this item. Not owned.
  WmWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(ShelfWindowWatcherItemDelegate);
};

}  // namespace ash

#endif  // ASH_COMMON_SHELF_SHELF_WINDOW_WATCHER_ITEM_DELEGATE_H_
