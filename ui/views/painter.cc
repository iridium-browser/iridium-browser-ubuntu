// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/painter.h"

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "cc/paint/paint_shader.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/nine_image_painter.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/view.h"

namespace views {

namespace {

// SolidRoundRectPainter -------------------------------------------------------

// Creates a round rect painter with a 1 pixel border. The border paints on top
// of the background.
class SolidRoundRectPainter : public Painter {
 public:
  SolidRoundRectPainter(SkColor bg_color, SkColor stroke_color, float radius);
  ~SolidRoundRectPainter() override;

  // Painter:
  gfx::Size GetMinimumSize() const override;
  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override;

 private:
  const SkColor bg_color_;
  const SkColor stroke_color_;
  const float radius_;

  DISALLOW_COPY_AND_ASSIGN(SolidRoundRectPainter);
};

SolidRoundRectPainter::SolidRoundRectPainter(SkColor bg_color,
                                             SkColor stroke_color,
                                             float radius)
    : bg_color_(bg_color), stroke_color_(stroke_color), radius_(radius) {}

SolidRoundRectPainter::~SolidRoundRectPainter() {}

gfx::Size SolidRoundRectPainter::GetMinimumSize() const {
  return gfx::Size();
}

void SolidRoundRectPainter::Paint(gfx::Canvas* canvas, const gfx::Size& size) {
  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();

  gfx::RectF border_rect_f(gfx::ScaleToEnclosingRect(gfx::Rect(size), scale));
  const SkScalar scaled_corner_radius = SkFloatToScalar(radius_ * scale);

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(bg_color_);
  canvas->DrawRoundRect(border_rect_f, scaled_corner_radius, flags);

  border_rect_f.Inset(gfx::InsetsF(0.5f));
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(1);
  flags.setColor(stroke_color_);
  canvas->DrawRoundRect(border_rect_f, scaled_corner_radius, flags);
}

// DashedFocusPainter ----------------------------------------------------------

class DashedFocusPainter : public Painter {
 public:
  explicit DashedFocusPainter(const gfx::Insets& insets);
  ~DashedFocusPainter() override;

  // Painter:
  gfx::Size GetMinimumSize() const override;
  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override;

 private:
  const gfx::Insets insets_;

  DISALLOW_COPY_AND_ASSIGN(DashedFocusPainter);
};

DashedFocusPainter::DashedFocusPainter(const gfx::Insets& insets)
    : insets_(insets) {
}

DashedFocusPainter::~DashedFocusPainter() {
}

gfx::Size DashedFocusPainter::GetMinimumSize() const {
  return gfx::Size();
}

void DashedFocusPainter::Paint(gfx::Canvas* canvas, const gfx::Size& size) {
  gfx::Rect rect(size);
  rect.Inset(insets_);
  canvas->DrawFocusRect(rect);
}

// SolidFocusPainter -----------------------------------------------------------

class SolidFocusPainter : public Painter {
 public:
  SolidFocusPainter(SkColor color, int thickness, const gfx::InsetsF& insets);
  ~SolidFocusPainter() override;

  // Painter:
  gfx::Size GetMinimumSize() const override;
  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override;

 private:
  const SkColor color_;
  const int thickness_;
  const gfx::InsetsF insets_;

  DISALLOW_COPY_AND_ASSIGN(SolidFocusPainter);
};

SolidFocusPainter::SolidFocusPainter(SkColor color,
                                     int thickness,
                                     const gfx::InsetsF& insets)
    : color_(color), thickness_(thickness), insets_(insets) {}

SolidFocusPainter::~SolidFocusPainter() {
}

gfx::Size SolidFocusPainter::GetMinimumSize() const {
  return gfx::Size();
}

void SolidFocusPainter::Paint(gfx::Canvas* canvas, const gfx::Size& size) {
  gfx::RectF rect((gfx::Rect(size)));
  rect.Inset(insets_);
  canvas->DrawSolidFocusRect(rect, color_, thickness_);
}

// GradientPainter ------------------------------------------------------------

class GradientPainter : public Painter {
 public:
  GradientPainter(bool horizontal,
                  SkColor* colors,
                  SkScalar* pos,
                  size_t count);
  ~GradientPainter() override;

  // Painter:
  gfx::Size GetMinimumSize() const override;
  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override;

 private:
  // If |horizontal_| is true then the gradient is painted horizontally.
  bool horizontal_;
  // The gradient colors.
  std::unique_ptr<SkColor[]> colors_;
  // The relative positions of the corresponding gradient colors.
  std::unique_ptr<SkScalar[]> pos_;
  // The number of elements in |colors_| and |pos_|.
  size_t count_;

  DISALLOW_COPY_AND_ASSIGN(GradientPainter);
};

GradientPainter::GradientPainter(bool horizontal,
                                 SkColor* colors,
                                 SkScalar* pos,
                                 size_t count)
    : horizontal_(horizontal),
      colors_(new SkColor[count]),
      pos_(new SkScalar[count]),
      count_(count) {
  for (size_t i = 0; i < count_; ++i) {
    pos_[i] = pos[i];
    colors_[i] = colors[i];
  }
}

GradientPainter::~GradientPainter() {
}

gfx::Size GradientPainter::GetMinimumSize() const {
  return gfx::Size();
}

void GradientPainter::Paint(gfx::Canvas* canvas, const gfx::Size& size) {
  cc::PaintFlags flags;
  SkPoint p[2];
  p[0].iset(0, 0);
  if (horizontal_)
    p[1].iset(size.width(), 0);
  else
    p[1].iset(0, size.height());

  flags.setShader(cc::WrapSkShader(SkGradientShader::MakeLinear(
      p, colors_.get(), pos_.get(), count_, SkShader::kClamp_TileMode)));
  flags.setStyle(cc::PaintFlags::kFill_Style);

  canvas->sk_canvas()->drawRect(SkRect::MakeIWH(size.width(), size.height()),
                                flags);
}

// ImagePainter ---------------------------------------------------------------

// ImagePainter stores and paints nine images as a scalable grid.
class ImagePainter : public Painter {
 public:
  // Constructs an ImagePainter with the specified image resource ids.
  // See CreateImageGridPainter()'s comment regarding image ID count and order.
  explicit ImagePainter(const int image_ids[]);

  // Constructs an ImagePainter with the specified image and insets.
  ImagePainter(const gfx::ImageSkia& image, const gfx::Insets& insets);

  ~ImagePainter() override;

  // Painter:
  gfx::Size GetMinimumSize() const override;
  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override;

 private:
  std::unique_ptr<gfx::NineImagePainter> nine_painter_;

  DISALLOW_COPY_AND_ASSIGN(ImagePainter);
};

ImagePainter::ImagePainter(const int image_ids[])
    : nine_painter_(ui::CreateNineImagePainter(image_ids)) {
}

ImagePainter::ImagePainter(const gfx::ImageSkia& image,
                           const gfx::Insets& insets)
    : nine_painter_(new gfx::NineImagePainter(image, insets)) {
}

ImagePainter::~ImagePainter() {
}

gfx::Size ImagePainter::GetMinimumSize() const {
  return nine_painter_->GetMinimumSize();
}

void ImagePainter::Paint(gfx::Canvas* canvas, const gfx::Size& size) {
  nine_painter_->Paint(canvas, gfx::Rect(size));
}

}  // namespace


// Painter --------------------------------------------------------------------

Painter::Painter() {
}

Painter::~Painter() {
}

// static
void Painter::PaintPainterAt(gfx::Canvas* canvas,
                             Painter* painter,
                             const gfx::Rect& rect) {
  DCHECK(canvas);
  DCHECK(painter);
  canvas->Save();
  canvas->Translate(rect.OffsetFromOrigin());
  painter->Paint(canvas, rect.size());
  canvas->Restore();
}

// static
void Painter::PaintFocusPainter(View* view,
                                gfx::Canvas* canvas,
                                Painter* focus_painter) {
  if (focus_painter && view->HasFocus())
    PaintPainterAt(canvas, focus_painter, view->GetLocalBounds());
}

// static
std::unique_ptr<Painter> Painter::CreateSolidRoundRectPainter(SkColor color,
                                                              float radius) {
  return base::MakeUnique<SolidRoundRectPainter>(color, SK_ColorTRANSPARENT,
                                                 radius);
}

// static
std::unique_ptr<Painter> Painter::CreateRoundRectWith1PxBorderPainter(
    SkColor bg_color,
    SkColor stroke_color,
    float radius) {
  return base::MakeUnique<SolidRoundRectPainter>(bg_color, stroke_color,
                                                 radius);
}

// static
std::unique_ptr<Painter> Painter::CreateVerticalGradient(SkColor c1,
                                                         SkColor c2) {
  SkColor colors[2];
  colors[0] = c1;
  colors[1] = c2;
  SkScalar pos[] = {0, 1};
  return base::MakeUnique<GradientPainter>(false, colors, pos, 2);
}

// static
std::unique_ptr<Painter> Painter::CreateImagePainter(
    const gfx::ImageSkia& image,
    const gfx::Insets& insets) {
  return base::MakeUnique<ImagePainter>(image, insets);
}

// static
std::unique_ptr<Painter> Painter::CreateImageGridPainter(
    const int image_ids[]) {
  return base::MakeUnique<ImagePainter>(image_ids);
}

// static
std::unique_ptr<Painter> Painter::CreateDashedFocusPainter() {
  return base::MakeUnique<DashedFocusPainter>(gfx::Insets());
}

// static
std::unique_ptr<Painter> Painter::CreateDashedFocusPainterWithInsets(
    const gfx::Insets& insets) {
  return base::MakeUnique<DashedFocusPainter>(insets);
}

// static
std::unique_ptr<Painter> Painter::CreateSolidFocusPainter(
    SkColor color,
    const gfx::Insets& insets) {
  // Before Canvas::DrawSolidFocusRect correctly inset the rect's bounds based
  // on the thickness, callers had to add 1 to the bottom and right insets.
  // Subtract that here so it works the same way with the new
  // Canvas::DrawSolidFocusRect.
  const gfx::Insets corrected_insets = insets - gfx::Insets(0, 0, 1, 1);
  return base::MakeUnique<SolidFocusPainter>(color, 1, corrected_insets);
}

// static
std::unique_ptr<Painter> Painter::CreateSolidFocusPainter(
    SkColor color,
    int thickness,
    const gfx::InsetsF& insets) {
  return base::MakeUnique<SolidFocusPainter>(color, thickness, insets);
}

// HorizontalPainter ----------------------------------------------------------

HorizontalPainter::HorizontalPainter(const int image_resource_names[]) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  for (int i = 0; i < 3; ++i)
    images_[i] = rb.GetImageNamed(image_resource_names[i]).ToImageSkia();
  DCHECK_EQ(images_[LEFT]->height(), images_[CENTER]->height());
  DCHECK_EQ(images_[LEFT]->height(), images_[RIGHT]->height());
}

HorizontalPainter::~HorizontalPainter() {
}

gfx::Size HorizontalPainter::GetMinimumSize() const {
  return gfx::Size(
      images_[LEFT]->width() + images_[CENTER]->width() +
          images_[RIGHT]->width(), images_[LEFT]->height());
}

void HorizontalPainter::Paint(gfx::Canvas* canvas, const gfx::Size& size) {
  if (size.width() < GetMinimumSize().width())
    return;  // No room to paint.

  canvas->DrawImageInt(*images_[LEFT], 0, 0);
  canvas->DrawImageInt(*images_[RIGHT], size.width() - images_[RIGHT]->width(),
                       0);
  canvas->TileImageInt(
      *images_[CENTER], images_[LEFT]->width(), 0,
      size.width() - images_[LEFT]->width() - images_[RIGHT]->width(),
      images_[LEFT]->height());
}

}  // namespace views
