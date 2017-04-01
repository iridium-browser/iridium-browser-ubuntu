// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/user/rounded_image_view.h"

#include "ash/common/material_design/material_design_controller.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skia_util.h"

namespace ash {
namespace tray {

RoundedImageView::RoundedImageView(int corner_radius, bool active_user)
    : active_user_(active_user) {
  for (int i = 0; i < 4; ++i)
    corner_radius_[i] = corner_radius;
}

RoundedImageView::~RoundedImageView() {}

void RoundedImageView::SetImage(const gfx::ImageSkia& img,
                                const gfx::Size& size) {
  image_ = img;
  image_size_ = size;

  // Try to get the best image quality for the avatar.
  resized_ = gfx::ImageSkiaOperations::CreateResizedImage(
      image_, skia::ImageOperations::RESIZE_BEST, size);
  if (GetWidget() && visible()) {
    PreferredSizeChanged();
    SchedulePaint();
  }
}

void RoundedImageView::SetCornerRadii(int top_left,
                                      int top_right,
                                      int bottom_right,
                                      int bottom_left) {
  corner_radius_[0] = top_left;
  corner_radius_[1] = top_right;
  corner_radius_[2] = bottom_right;
  corner_radius_[3] = bottom_left;
}

gfx::Size RoundedImageView::GetPreferredSize() const {
  return gfx::Size(image_size_.width() + GetInsets().width(),
                   image_size_.height() + GetInsets().height());
}

void RoundedImageView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  gfx::Rect image_bounds(size());
  image_bounds.ClampToCenteredSize(GetPreferredSize());
  image_bounds.Inset(GetInsets());
  const SkScalar kRadius[8] = {
      SkIntToScalar(corner_radius_[0]), SkIntToScalar(corner_radius_[0]),
      SkIntToScalar(corner_radius_[1]), SkIntToScalar(corner_radius_[1]),
      SkIntToScalar(corner_radius_[2]), SkIntToScalar(corner_radius_[2]),
      SkIntToScalar(corner_radius_[3]), SkIntToScalar(corner_radius_[3])};
  SkPath path;
  path.addRoundRect(gfx::RectToSkRect(image_bounds), kRadius);
  SkPaint paint;
  paint.setAntiAlias(true);
  const bool grayscale =
      !active_user_ && !MaterialDesignController::IsSystemTrayMenuMaterial();
  paint.setBlendMode(grayscale ? SkBlendMode::kLuminosity
                               : SkBlendMode::kSrcOver);
  canvas->DrawImageInPath(resized_, image_bounds.x(), image_bounds.y(), path,
                          paint);
}

}  // namespace tray
}  // namespace ash
