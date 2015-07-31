// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_BUBBLE_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_BUBBLE_ICON_VIEW_H_

#include "ui/views/controls/image_view.h"

class CommandUpdater;

namespace views {
class BubbleDelegateView;
}

// Represents an icon on the omnibox that shows a bubble when clicked.
class BubbleIconView : public views::ImageView {
 protected:
  enum ExecuteSource {
    EXECUTE_SOURCE_MOUSE,
    EXECUTE_SOURCE_KEYBOARD,
    EXECUTE_SOURCE_GESTURE,
  };

  BubbleIconView(CommandUpdater* command_updater, int command_id);
  ~BubbleIconView() override;

  // Returns true if a related bubble is showing.
  bool IsBubbleShowing() const;

  // Invoked prior to executing the command.
  virtual void OnExecuting(ExecuteSource execute_source) = 0;

  // views::ImageView:
  void GetAccessibleState(ui::AXViewState* state) override;
  bool GetTooltipText(const gfx::Point& p, base::string16* tooltip) const
      override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

 protected:
  // Calls OnExecuting and runs |command_id_| with a valid |command_updater_|.
  virtual void ExecuteCommand(ExecuteSource source);

  // Returns the bubble instance for the icon.
  virtual views::BubbleDelegateView* GetBubble() const = 0;

  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  // The CommandUpdater for the Browser object that owns the location bar.
  CommandUpdater* command_updater_;

  // The command ID executed when the user clicks this icon.
  const int command_id_;

  // This is used to check if the bookmark bubble was showing during the mouse
  // pressed event. If this is true then the mouse released event is ignored to
  // prevent the bubble from reshowing.
  bool suppress_mouse_released_action_;

  DISALLOW_COPY_AND_ASSIGN(BubbleIconView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_BUBBLE_ICON_VIEW_H_
