// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/focusable_border.h"

#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/textfield/textfield.h"

namespace {

const int kInsetSize = 1;

}  // namespace

namespace views {

FocusableBorder::FocusableBorder()
    : insets_(kInsetSize, kInsetSize, kInsetSize, kInsetSize),
      override_color_(gfx::kPlaceholderColor),
      use_default_color_(true) {
}

FocusableBorder::~FocusableBorder() {
}

void FocusableBorder::SetColor(SkColor color) {
  override_color_ = color;
  use_default_color_ = false;
}

void FocusableBorder::UseDefaultColor() {
  use_default_color_ = true;
}

void FocusableBorder::Paint(const View& view, gfx::Canvas* canvas) {
  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setColor(GetCurrentColor(view));

  if (ui::MaterialDesignController::IsSecondaryUiMaterial()) {
    gfx::ScopedCanvas scoped(canvas);
    float dsf = canvas->UndoDeviceScaleFactor();
    gfx::RectF rect((gfx::Rect(view.GetLocalBounds())));
    rect = ScaleRect(rect, dsf, dsf);
    rect.Inset(gfx::InsetsF(0.5f));
    SkPath path;
    float corner_radius_px = kCornerRadiusDp * dsf;
    path.addRoundRect(gfx::RectFToSkRect(rect), corner_radius_px,
                      corner_radius_px);
    const int kStrokeWidthPx = 1;
    paint.setStrokeWidth(SkIntToScalar(kStrokeWidthPx));
    paint.setAntiAlias(true);
    canvas->DrawPath(path, paint);
  } else {
    SkPath path;
    path.addRect(gfx::RectToSkRect(view.GetLocalBounds()),
                 SkPath::kCW_Direction);
    paint.setStrokeWidth(SkIntToScalar(2));
    canvas->DrawPath(path, paint);
  }
}

gfx::Insets FocusableBorder::GetInsets() const {
  return insets_;
}

gfx::Size FocusableBorder::GetMinimumSize() const {
  return gfx::Size();
}

void FocusableBorder::SetInsets(int top, int left, int bottom, int right) {
  insets_.Set(top, left, bottom, right);
}

SkColor FocusableBorder::GetCurrentColor(const View& view) const {
  if (!use_default_color_)
    return override_color_;
  return view.GetNativeTheme()->GetSystemColor(
      view.HasFocus() ? ui::NativeTheme::kColorId_FocusedBorderColor :
                        ui::NativeTheme::kColorId_UnfocusedBorderColor);
}

}  // namespace views
