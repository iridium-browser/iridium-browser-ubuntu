// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/platform_style.h"

#include "ui/base/resource/material_design/material_design_controller.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"

namespace views {

#if defined(OS_CHROMEOS)
// static
scoped_ptr<LabelButtonBorder> PlatformStyle::CreateLabelButtonBorder(
    Button::ButtonStyle style) {
  if (!ui::MaterialDesignController::IsModeMaterial() ||
      style != Button::STYLE_TEXTBUTTON) {
    return make_scoped_ptr(new LabelButtonAssetBorder(style));
  }

  // The material design spec for Chrome OS includes no visual effects for
  // button states, so a non-asset border with insets is used.
  scoped_ptr<LabelButtonBorder> border(new views::LabelButtonBorder());
  border->set_insets(views::LabelButtonAssetBorder::GetDefaultInsetsForStyle(
      Button::STYLE_TEXTBUTTON));
  return border.Pass();
}
#elif !defined(OS_MACOSX)
// static
scoped_ptr<LabelButtonBorder> PlatformStyle::CreateLabelButtonBorder(
    Button::ButtonStyle style) {
  scoped_ptr<LabelButtonAssetBorder> border(new LabelButtonAssetBorder(style));
  // The material design spec does not include a visual effect for the
  // STATE_HOVERED button state so we have to remove the default one added by
  // LabelButtonAssetBorder.
  if (ui::MaterialDesignController::IsModeMaterial())
    border->SetPainter(false, Button::STATE_HOVERED, nullptr);
  return border.Pass();
}
#endif

#if !defined(OS_LINUX) || defined(OS_CHROMEOS)
// static
scoped_ptr<Border> PlatformStyle::CreateThemedLabelButtonBorder(
    LabelButton* button) {
  return button->CreateDefaultBorder();
}
#endif

}  // namespace views
