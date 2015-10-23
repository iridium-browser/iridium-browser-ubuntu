// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/overscan_calibrator.h"

#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "base/callback.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"

namespace chromeos {
namespace {

// The opacity for the arrows of the overscan calibration.
const float kArrowOpacity = 0.8;

// The height in pixel for the arrows to show the overscan calibration.
const int kCalibrationArrowHeight = 50;

// The gap between the boundary and calibration arrows.
const int kArrowGapWidth = 20;

// Draw the arrow for the overscan calibration to |canvas|.
void DrawTriangle(int x_offset,
                  int y_offset,
                  double rotation_degree,
                  gfx::Canvas* canvas) {
  // Draw triangular arrows.
  SkPaint content_paint;
  content_paint.setStyle(SkPaint::kFill_Style);
  content_paint.setColor(SkColorSetA(SK_ColorBLACK, kuint8max * kArrowOpacity));
  SkPaint border_paint;
  border_paint.setStyle(SkPaint::kStroke_Style);
  border_paint.setColor(SkColorSetA(SK_ColorWHITE, kuint8max * kArrowOpacity));

  SkPath base_path;
  base_path.moveTo(0, SkIntToScalar(-kCalibrationArrowHeight));
  base_path.lineTo(SkIntToScalar(-kCalibrationArrowHeight), 0);
  base_path.lineTo(SkIntToScalar(kCalibrationArrowHeight), 0);
  base_path.close();

  SkPath path;
  gfx::Transform rotate_transform;
  rotate_transform.Rotate(rotation_degree);
  gfx::Transform move_transform;
  move_transform.Translate(x_offset, y_offset);
  rotate_transform.ConcatTransform(move_transform);
  base_path.transform(rotate_transform.matrix(), &path);

  canvas->DrawPath(path, content_paint);
  canvas->DrawPath(path, border_paint);
}

}  // namespace

OverscanCalibrator::OverscanCalibrator(
    const gfx::Display& target_display, const gfx::Insets& initial_insets)
    : display_(target_display),
      insets_(initial_insets),
      initial_insets_(initial_insets),
      committed_(false) {
  // Undo the overscan calibration temporarily so that the user can see
  // dark boundary and current overscan region.
  ash::Shell::GetInstance()->window_tree_host_manager()->SetOverscanInsets(
      display_.id(), gfx::Insets());

  ash::DisplayInfo info =
      ash::Shell::GetInstance()->display_manager()->GetDisplayInfo(
          display_.id());

  aura::Window* root = ash::Shell::GetInstance()
                           ->window_tree_host_manager()
                           ->GetRootWindowForDisplayId(display_.id());
  ui::Layer* parent_layer =
      ash::Shell::GetContainer(root, ash::kShellWindowId_OverlayContainer)
          ->layer();

  calibration_layer_.reset(new ui::Layer());
  calibration_layer_->SetOpacity(0.5f);
  calibration_layer_->SetBounds(parent_layer->bounds());
  calibration_layer_->set_delegate(this);
  parent_layer->Add(calibration_layer_.get());
}

OverscanCalibrator::~OverscanCalibrator() {
  // Overscan calibration has finished without commit, so the display has to
  // be the original offset.
  if (!committed_) {
    ash::Shell::GetInstance()->window_tree_host_manager()->SetOverscanInsets(
        display_.id(), initial_insets_);
  }
}

void OverscanCalibrator::Commit() {
  ash::Shell::GetInstance()->window_tree_host_manager()->SetOverscanInsets(
      display_.id(), insets_);
  committed_ = true;
}

void OverscanCalibrator::Reset() {
  insets_ = initial_insets_;
  calibration_layer_->SchedulePaint(calibration_layer_->bounds());
}

void OverscanCalibrator::UpdateInsets(const gfx::Insets& insets) {
  insets_.Set(std::max(insets.top(), 0),
              std::max(insets.left(), 0),
              std::max(insets.bottom(), 0),
              std::max(insets.right(), 0));
  calibration_layer_->SchedulePaint(calibration_layer_->bounds());
}

void OverscanCalibrator::OnPaintLayer(const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, calibration_layer_->size());
  static const SkColor kTransparent = SkColorSetARGB(0, 0, 0, 0);
  gfx::Rect full_bounds = calibration_layer_->bounds();
  gfx::Rect inner_bounds = full_bounds;
  inner_bounds.Inset(insets_);
  recorder.canvas()->FillRect(full_bounds, SK_ColorBLACK);
  recorder.canvas()->FillRect(inner_bounds, kTransparent,
                              SkXfermode::kClear_Mode);

  gfx::Point center = inner_bounds.CenterPoint();
  int vertical_offset = inner_bounds.height() / 2 - kArrowGapWidth;
  int horizontal_offset = inner_bounds.width() / 2 - kArrowGapWidth;

  gfx::Canvas* canvas = recorder.canvas();
  DrawTriangle(center.x(), center.y() + vertical_offset, 0, canvas);
  DrawTriangle(center.x(), center.y() - vertical_offset, 180, canvas);
  DrawTriangle(center.x() - horizontal_offset, center.y(), 90, canvas);
  DrawTriangle(center.x() + horizontal_offset, center.y(), -90, canvas);
}

void OverscanCalibrator::OnDelegatedFrameDamage(
    const gfx::Rect& damage_rect_in_dip) {
}

void OverscanCalibrator::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  // TODO(mukai): Cancel the overscan calibration when the device
  // configuration has changed.
}

base::Closure OverscanCalibrator::PrepareForLayerBoundsChange() {
  return base::Closure();
}

}  // namespace chromeos
