// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_SHELF_ITEM_DELEGATE_H_
#define ASH_TEST_TEST_SHELF_ITEM_DELEGATE_H_

#include "ash/common/shelf/shelf_item_delegate.h"
#include "base/compiler_specific.h"
#include "base/macros.h"

namespace aura {
class Window;
}

namespace ash {
namespace test {

// Test implementation of ShelfItemDelegate.
class TestShelfItemDelegate : public ShelfItemDelegate {
 public:
  explicit TestShelfItemDelegate(aura::Window* window);
  ~TestShelfItemDelegate() override;

  void set_is_draggable(bool is_draggable) { is_draggable_ = is_draggable; }

  // ShelfItemDelegate:
  ShelfItemDelegate::PerformedAction ItemSelected(
      const ui::Event& event) override;
  base::string16 GetTitle() override;
  ShelfMenuModel* CreateApplicationMenu(int event_flags) override;
  bool IsDraggable() override;
  bool CanPin() const override;
  bool ShouldShowTooltip() override;
  void Close() override;

 private:
  aura::Window* window_;

  bool is_draggable_;

  DISALLOW_COPY_AND_ASSIGN(TestShelfItemDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_SHELF_ITEM_DELEGATE_H_
