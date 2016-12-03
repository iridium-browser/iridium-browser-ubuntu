// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/overflow_bubble_view.h"

#include <algorithm>

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Max bubble size to screen size ratio.
const float kMaxBubbleSizeToScreenRatio = 0.5f;

// Inner padding in pixels for shelf view inside bubble.
const int kPadding = 2;

// Padding space in pixels between ShelfView's left/top edge to its contents.
const int kShelfViewLeadingInset = 8;

}  // namespace

OverflowBubbleView::OverflowBubbleView(WmShelf* wm_shelf)
    : wm_shelf_(wm_shelf), shelf_view_(nullptr) {}

OverflowBubbleView::~OverflowBubbleView() {}

void OverflowBubbleView::InitOverflowBubble(views::View* anchor,
                                            views::View* shelf_view) {
  shelf_view_ = shelf_view;

  SetAnchorView(anchor);
  set_arrow(GetBubbleArrow());
  set_mirror_arrow_in_rtl(false);
  set_background(NULL);
  SkColor color = MaterialDesignController::IsShelfMaterial()
                      ? kShelfBaseColor
                      : SkColorSetA(kShelfBaseColor,
                                    GetShelfConstant(SHELF_BACKGROUND_ALPHA));
  set_color(color);
  set_margins(gfx::Insets(kPadding, kPadding, kPadding, kPadding));
  // Overflow bubble should not get focus. If it get focus when it is shown,
  // active state item is changed to running state.
  set_can_activate(false);

  // Makes bubble view has a layer and clip its children layers.
  SetPaintToLayer(true);
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);

  // Calls into OnBeforeBubbleWidgetInit to set the window parent container.
  views::BubbleDialogDelegateView::CreateBubble(this);
  AddChildView(shelf_view_);
}

bool OverflowBubbleView::IsHorizontalAlignment() const {
  return ::ash::IsHorizontalAlignment(wm_shelf_->GetAlignment());
}

const gfx::Size OverflowBubbleView::GetContentsSize() const {
  return shelf_view_->GetPreferredSize();
}

// Gets arrow location based on shelf alignment.
views::BubbleBorder::Arrow OverflowBubbleView::GetBubbleArrow() const {
  switch (wm_shelf_->GetAlignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
      return views::BubbleBorder::BOTTOM_LEFT;
    case SHELF_ALIGNMENT_LEFT:
      return views::BubbleBorder::LEFT_TOP;
    case SHELF_ALIGNMENT_RIGHT:
      return views::BubbleBorder::RIGHT_TOP;
  }
  NOTREACHED();
  return views::BubbleBorder::NONE;
}

void OverflowBubbleView::ScrollByXOffset(int x_offset) {
  const gfx::Rect visible_bounds(GetContentsBounds());
  const gfx::Size contents_size(GetContentsSize());

  DCHECK_GE(contents_size.width(), visible_bounds.width());
  int x = std::min(contents_size.width() - visible_bounds.width(),
                   std::max(0, scroll_offset_.x() + x_offset));
  scroll_offset_.set_x(x);
}

void OverflowBubbleView::ScrollByYOffset(int y_offset) {
  const gfx::Rect visible_bounds(GetContentsBounds());
  const gfx::Size contents_size(GetContentsSize());

  DCHECK_GE(contents_size.width(), visible_bounds.width());
  int y = std::min(contents_size.height() - visible_bounds.height(),
                   std::max(0, scroll_offset_.y() + y_offset));
  scroll_offset_.set_y(y);
}

gfx::Size OverflowBubbleView::GetPreferredSize() const {
  gfx::Size preferred_size = GetContentsSize();

  const gfx::Rect monitor_rect =
      display::Screen::GetScreen()
          ->GetDisplayNearestPoint(GetAnchorRect().CenterPoint())
          .work_area();
  if (!monitor_rect.IsEmpty()) {
    if (IsHorizontalAlignment()) {
      preferred_size.set_width(
          std::min(preferred_size.width(),
                   static_cast<int>(monitor_rect.width() *
                                    kMaxBubbleSizeToScreenRatio)));
    } else {
      preferred_size.set_height(
          std::min(preferred_size.height(),
                   static_cast<int>(monitor_rect.height() *
                                    kMaxBubbleSizeToScreenRatio)));
    }
  }

  return preferred_size;
}

void OverflowBubbleView::Layout() {
  shelf_view_->SetBoundsRect(gfx::Rect(
      gfx::PointAtOffsetFromOrigin(-scroll_offset_), GetContentsSize()));
}

void OverflowBubbleView::ChildPreferredSizeChanged(views::View* child) {
  // When contents size is changed, ContentsBounds should be updated before
  // calculating scroll offset.
  SizeToContents();

  // Ensures |shelf_view_| is still visible.
  if (IsHorizontalAlignment())
    ScrollByXOffset(0);
  else
    ScrollByYOffset(0);
  Layout();
}

bool OverflowBubbleView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  // The MouseWheelEvent was changed to support both X and Y offsets
  // recently, but the behavior of this function was retained to continue
  // using Y offsets only. Might be good to simply scroll in both
  // directions as in OverflowBubbleView::OnScrollEvent.
  if (IsHorizontalAlignment())
    ScrollByXOffset(-event.y_offset());
  else
    ScrollByYOffset(-event.y_offset());
  Layout();

  return true;
}

void OverflowBubbleView::OnScrollEvent(ui::ScrollEvent* event) {
  ScrollByXOffset(-event->x_offset());
  ScrollByYOffset(-event->y_offset());
  Layout();
  event->SetHandled();
}

int OverflowBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void OverflowBubbleView::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* bubble_widget) const {
  // Place the bubble in the same root window as the anchor.
  WmLookup::Get()
      ->GetWindowForWidget(anchor_widget())
      ->GetRootWindowController()
      ->ConfigureWidgetInitParamsForContainer(
          bubble_widget, kShellWindowId_ShelfBubbleContainer, params);
}

gfx::Rect OverflowBubbleView::GetBubbleBounds() {
  views::BubbleBorder* border = GetBubbleFrameView()->bubble_border();
  gfx::Insets bubble_insets = border->GetInsets();

  const int border_size = views::BubbleBorder::is_arrow_on_horizontal(arrow())
                              ? bubble_insets.left()
                              : bubble_insets.top();
  const int arrow_offset = border_size + kPadding + kShelfViewLeadingInset +
                           GetShelfConstant(SHELF_SIZE) / 2;

  const gfx::Size content_size = GetPreferredSize();
  border->set_arrow_offset(arrow_offset);

  const gfx::Rect anchor_rect = GetAnchorRect();
  gfx::Rect bubble_rect = GetBubbleFrameView()->GetUpdatedWindowBounds(
      anchor_rect, content_size, false);

  gfx::Rect monitor_rect =
      display::Screen::GetScreen()
          ->GetDisplayNearestPoint(anchor_rect.CenterPoint())
          .work_area();

  int offset = 0;
  if (views::BubbleBorder::is_arrow_on_horizontal(arrow())) {
    if (bubble_rect.x() < monitor_rect.x())
      offset = monitor_rect.x() - bubble_rect.x();
    else if (bubble_rect.right() > monitor_rect.right())
      offset = monitor_rect.right() - bubble_rect.right();

    bubble_rect.Offset(offset, 0);
    border->set_arrow_offset(anchor_rect.CenterPoint().x() - bubble_rect.x());
  } else {
    if (bubble_rect.y() < monitor_rect.y())
      offset = monitor_rect.y() - bubble_rect.y();
    else if (bubble_rect.bottom() > monitor_rect.bottom())
      offset = monitor_rect.bottom() - bubble_rect.bottom();

    bubble_rect.Offset(0, offset);
    border->set_arrow_offset(anchor_rect.CenterPoint().y() - bubble_rect.y());
  }

  GetBubbleFrameView()->SchedulePaint();
  return bubble_rect;
}

}  // namespace ash
