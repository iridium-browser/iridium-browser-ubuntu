// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_OVERFLOW_BUBBLE_VIEW_H_
#define ASH_COMMON_SHELF_OVERFLOW_BUBBLE_VIEW_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"

namespace views {
class View;
}

namespace ash {
class WmShelf;

namespace test {
class OverflowBubbleViewTestAPI;
}

// OverflowBubbleView hosts a ShelfView to display overflown items.
// Exports to access this class from OverflowBubbleViewTestAPI.
class ASH_EXPORT OverflowBubbleView : public views::BubbleDialogDelegateView {
 public:
  explicit OverflowBubbleView(WmShelf* wm_shelf);
  ~OverflowBubbleView() override;

  // |anchor| is the overflow button on the main shelf. |shelf_view| is the
  // ShelfView containing the overflow items.
  void InitOverflowBubble(views::View* anchor, views::View* shelf_view);

  // views::BubbleDialogDelegateView overrides:
  int GetDialogButtons() const override;
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* bubble_widget) const override;
  gfx::Rect GetBubbleBounds() override;

 private:
  friend class test::OverflowBubbleViewTestAPI;

  bool IsHorizontalAlignment() const;

  const gfx::Size GetContentsSize() const;

  // Gets arrow location based on shelf alignment.
  views::BubbleBorder::Arrow GetBubbleArrow() const;

  void ScrollByXOffset(int x_offset);
  void ScrollByYOffset(int y_offset);

  // views::View overrides:
  gfx::Size GetPreferredSize() const override;
  void Layout() override;
  void ChildPreferredSizeChanged(views::View* child) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;

  // ui::EventHandler overrides:
  void OnScrollEvent(ui::ScrollEvent* event) override;

  WmShelf* wm_shelf_;
  views::View* shelf_view_;  // Owned by views hierarchy.
  gfx::Vector2d scroll_offset_;

  DISALLOW_COPY_AND_ASSIGN(OverflowBubbleView);
};

}  // namespace ash

#endif  // ASH_COMMON_SHELF_OVERFLOW_BUBBLE_VIEW_H_
