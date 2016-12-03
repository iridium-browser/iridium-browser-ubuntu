// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_shelf_item_delegate.h"

#include "ash/wm/window_util.h"
#include "ui/aura/window.h"

namespace ash {
namespace test {

TestShelfItemDelegate::TestShelfItemDelegate(aura::Window* window)
    : window_(window), is_draggable_(true) {}

TestShelfItemDelegate::~TestShelfItemDelegate() {}

ShelfItemDelegate::PerformedAction TestShelfItemDelegate::ItemSelected(
    const ui::Event& event) {
  if (window_) {
    if (window_->type() == ui::wm::WINDOW_TYPE_PANEL)
      wm::MoveWindowToEventRoot(window_, event);
    window_->Show();
    wm::ActivateWindow(window_);
    return kExistingWindowActivated;
  }
  return kNoAction;
}

base::string16 TestShelfItemDelegate::GetTitle() {
  return window_ ? window_->title() : base::string16();
}

ShelfMenuModel* TestShelfItemDelegate::CreateApplicationMenu(int event_flags) {
  return nullptr;
}

bool TestShelfItemDelegate::IsDraggable() {
  return is_draggable_;
}

bool TestShelfItemDelegate::CanPin() const {
  return true;
}

bool TestShelfItemDelegate::ShouldShowTooltip() {
  return true;
}

void TestShelfItemDelegate::Close() {}

}  // namespace test
}  // namespace ash
