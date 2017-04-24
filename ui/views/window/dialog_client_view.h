// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_DIALOG_CLIENT_VIEW_H_
#define UI_VIEWS_WINDOW_DIALOG_CLIENT_VIEW_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/client_view.h"

namespace views {

class DialogDelegate;
class LabelButton;
class Widget;

// DialogClientView provides adornments for a dialog's content view, including
// custom-labeled [OK] and [Cancel] buttons with [Enter] and [Esc] accelerators.
// The view also displays the delegate's extra view alongside the buttons. The
// view appears like below. NOTE: The contents view is not inset on the top or
// side client view edges.
//   +------------------------------+
//   |        Contents View         |
//   +------------------------------+
//   | [Extra View]   [OK] [Cancel] |
//   +------------------------------+
class VIEWS_EXPORT DialogClientView : public ClientView,
                                      public ButtonListener {
 public:
  DialogClientView(Widget* widget, View* contents_view);
  ~DialogClientView() override;

  // Accept or Cancel the dialog.
  void AcceptWindow();
  void CancelWindow();

  // Accessors in case the user wishes to adjust these buttons.
  LabelButton* ok_button() const { return ok_button_; }
  LabelButton* cancel_button() const { return cancel_button_; }

  // Update the dialog buttons to match the dialog's delegate.
  void UpdateDialogButtons();

  // ClientView implementation:
  bool CanClose() override;
  DialogClientView* AsDialogClientView() override;
  const DialogClientView* AsDialogClientView() const override;

  // View implementation:
  gfx::Size GetPreferredSize() const override;
  void Layout() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;

  // ButtonListener implementation:
  void ButtonPressed(Button* sender, const ui::Event& event) override;

  void set_button_row_insets(const gfx::Insets& insets) {
    button_row_insets_ = insets;
  }

  void set_minimum_size(const gfx::Size& size) { minimum_size_ = size; }

 private:
  bool has_dialog_buttons() const { return ok_button_ || cancel_button_; }

  // Returns the DialogDelegate for the window.
  DialogDelegate* GetDialogDelegate() const;

  // View implementation.
  void ChildPreferredSizeChanged(View* child) override;
  void ChildVisibilityChanged(View* child) override;

  // Create a dialog button of the appropriate type.
  LabelButton* CreateDialogButton(ui::DialogButton type);

  // Update |button|'s text and enabled state according to the delegate's state.
  void UpdateButton(LabelButton* button, ui::DialogButton type);

  // Returns the height of the buttons.
  int GetButtonHeight() const;

  // Returns the height of the extra view.
  int GetExtraViewHeight() const;

  // Returns the height of the row containing the buttons and the extra view.
  int GetButtonsAndExtraViewRowHeight() const;

  // Returns the insets for the buttons and extra view, including the vertical
  // padding between them and the contents view.
  gfx::Insets GetButtonRowInsets() const;

  // Sets up the focus chain for the child views. This is required since the
  // delegate may choose to add/remove views at any time.
  void SetupFocusChain();

  // Returns the spacing between the extra view and the ok/cancel buttons. 0 if
  // no extra view. Otherwise uses GetExtraViewPadding() or the default padding.
  int GetExtraViewSpacing() const;

  // Creates or deletes any buttons that are required. Updates data members.
  void SetupViews();

  // How much to inset the button row.
  gfx::Insets button_row_insets_;

  // The minimum size of this dialog, regardless of the size of its content
  // view.
  gfx::Size minimum_size_;

  // The dialog buttons.
  LabelButton* ok_button_ = nullptr;
  LabelButton* cancel_button_ = nullptr;

  // The extra view shown in the row of buttons; may be NULL.
  View* extra_view_ = nullptr;

  // True if we've notified the delegate the window is closing and the delegate
  // allowed the close. In some situations it's possible to get two closes (see
  // http://crbug.com/71940). This is used to avoid notifying the delegate
  // twice, which can have bad consequences.
  bool delegate_allowed_close_ = false;

  DISALLOW_COPY_AND_ASSIGN(DialogClientView);
};

}  // namespace views

#endif  // UI_VIEWS_WINDOW_DIALOG_CLIENT_VIEW_H_
