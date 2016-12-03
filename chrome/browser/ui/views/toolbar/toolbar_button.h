// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_BUTTON_H_

#include <memory>

#include "base/macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"

class Profile;

namespace test {
class ToolbarButtonTestApi;
}

namespace ui {
class MenuModel;
}

namespace views {
class MenuModelAdapter;
class MenuRunner;
}

// This class provides basic drawing and mouse-over behavior for buttons
// appearing in the toolbar.
class ToolbarButton : public views::LabelButton,
                      public views::ContextMenuController {
 public:
  // Takes ownership of the |model|, which can be null if no menu
  // is to be shown.
  ToolbarButton(Profile* profile,
                views::ButtonListener* listener,
                ui::MenuModel* model);
  ~ToolbarButton() override;

  // Set up basic mouseover border behavior.
  // Should be called before first paint.
  void Init();

  // Methods for handling ButtonDropDown-style menus.
  void ClearPendingMenu();
  bool IsMenuShowing() const;

  // views::LabelButton:
  gfx::Size GetPreferredSize() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  // Showing the drop down results in a MouseCaptureLost, we need to ignore it.
  void OnMouseCaptureLost() override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void GetAccessibleState(ui::AXViewState* state) override;
  std::unique_ptr<views::LabelButtonBorder> CreateDefaultBorder()
      const override;

  // views::ContextMenuController:
  void ShowContextMenuForView(View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type) override;

 protected:
  // Returns if menu should be shown. Override this to change default behavior.
  virtual bool ShouldShowMenu();

  // Function to show the dropdown menu.
  virtual void ShowDropDownMenu(ui::MenuSourceType source_type);

 private:
  friend test::ToolbarButtonTestApi;

  // Callback for MenuModelAdapter.
  void OnMenuClosed();

  // views::LabelButton:
  const char* GetClassName() const override;

  // The associated profile. The browser theme affects rendering.
  Profile* profile_;

  // The model that populates the attached menu.
  std::unique_ptr<ui::MenuModel> model_;

  // Indicates if menu is currently showing.
  bool menu_showing_;

  // Y position of mouse when left mouse button is pressed.
  int y_position_on_lbuttondown_;

  // The model adapter for the drop down menu.
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;

  // Menu runner to display drop down menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // A factory for tasks that show the dropdown context menu for the button.
  base::WeakPtrFactory<ToolbarButton> show_menu_factory_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_BUTTON_H_
