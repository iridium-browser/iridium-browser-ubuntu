// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_APP_LIST_BUTTON_H_
#define ASH_COMMON_SHELF_APP_LIST_BUTTON_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {
class InkDropButtonListener;
class ShelfView;
class WmShelf;

// Button used for the AppList icon on the shelf.
class ASH_EXPORT AppListButton : public views::ImageButton {
 public:
  AppListButton(InkDropButtonListener* listener,
                ShelfView* shelf_view,
                WmShelf* wm_shelf);
  ~AppListButton() override;

  void OnAppListShown();
  void OnAppListDismissed();

  bool draw_background_as_active() { return draw_background_as_active_; }

  // Sets alpha value of the background and schedules a paint.
  void SetBackgroundAlpha(int alpha);

 protected:
  // views::ImageButton overrides:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void GetAccessibleState(ui::AXViewState* state) override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  void NotifyClick(const ui::Event& event) override;
  bool ShouldEnterPushedState(const ui::Event& event) override;
  bool ShouldShowInkDropHighlight() const override;

  // ui::EventHandler overrides:
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  // Toggles the active state for painting the background and schedules a paint.
  void SetDrawBackgroundAsActive(bool draw_background_as_active);

  // Helper functions to paint the background and foreground of the AppList
  // button in Chrome OS MD.
  void PaintBackgroundMD(gfx::Canvas* canvas);
  void PaintForegroundMD(gfx::Canvas* canvas,
                         const gfx::ImageSkia& foreground_image);

  // Helper function to paint the AppList button in Chrome OS non-MD.
  void PaintAppListButton(gfx::Canvas* canvas,
                          const gfx::ImageSkia& foreground_image);

  // True if the background should render as active, regardless of the state of
  // the application list.
  bool draw_background_as_active_;

  // Alpha value used to paint the background.
  int background_alpha_;

  InkDropButtonListener* listener_;
  ShelfView* shelf_view_;
  WmShelf* wm_shelf_;

  DISALLOW_COPY_AND_ASSIGN(AppListButton);
};

}  // namespace ash

#endif  // ASH_COMMON_SHELF_APP_LIST_BUTTON_H_
