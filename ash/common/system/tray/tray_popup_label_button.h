// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_TRAY_POPUP_LABEL_BUTTON_H_
#define ASH_COMMON_SYSTEM_TRAY_TRAY_POPUP_LABEL_BUTTON_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/views/controls/button/label_button.h"

namespace ash {

// A label button with custom alignment, border and focus border.
// TODO(estade): deprecated for MD. Use CreateTrayPopupButton instead.
// See crbug.com/614453
class TrayPopupLabelButton : public views::LabelButton {
 public:
  TrayPopupLabelButton(views::ButtonListener* listener,
                       const base::string16& text);
  ~TrayPopupLabelButton() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TrayPopupLabelButton);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_TRAY_POPUP_LABEL_BUTTON_H_
