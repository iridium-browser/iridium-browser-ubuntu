// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONFIRM_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_CONFIRM_BUBBLE_VIEWS_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/window/dialog_delegate.h"

class ConfirmBubbleModel;

// A dialog (with the standard Title/(x)/[OK]/[Cancel] UI elements), as well as
// a message Label and optional Link. The dialog ultimately appears like this:
//   +------------------------+
//   | Title              (x) |
//   | Label                  |
//   | Link     [OK] [Cancel] |
//   +------------------------+
//
// TODO(msw): Remove this class or merge it with DialogDelegateView.
class ConfirmBubbleViews : public views::DialogDelegateView,
                           public views::LinkListener {
 public:
  explicit ConfirmBubbleViews(std::unique_ptr<ConfirmBubbleModel> model);

 protected:
  ~ConfirmBubbleViews() override;

  // views::DialogDelegate implementation.
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  views::View* CreateExtraView() override;
  bool Cancel() override;
  bool Accept() override;

  // views::WidgetDelegate implementation.
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;

  // views::LinkListener implementation.
  void LinkClicked(views::Link* source, int event_flags) override;

 private:
  // The model to customize this bubble view.
  std::unique_ptr<ConfirmBubbleModel> model_;

  views::Link* link_;

  DISALLOW_COPY_AND_ASSIGN(ConfirmBubbleViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONFIRM_BUBBLE_VIEWS_H_
