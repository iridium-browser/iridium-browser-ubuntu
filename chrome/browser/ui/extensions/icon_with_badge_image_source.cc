// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/icon_with_badge_image_source.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_action.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/resources/grit/ui_resources.h"

namespace {

// Different platforms need slightly different constants to look good.
// TODO(devlin): Comb through these and see if they are all still needed/
// appropriate.
#if defined(OS_WIN)
const float kTextSize = 10;
// The padding between the top of the badge and the top of the text.
const int kTopTextPadding = -1;
#elif defined(OS_MACOSX)
const float kTextSize = 9.0;
const int kTopTextPadding = 0;
#elif defined(OS_CHROMEOS)
const float kTextSize = 8.0;
const int kTopTextPadding = 1;
#elif defined(OS_POSIX)
const float kTextSize = 9.0;
const int kTopTextPadding = 0;
#endif

const int kPadding = 2;
const int kBadgeHeight = 11;
const int kMaxTextWidth = 23;

// The minimum width for center-aligning the badge.
const int kCenterAlignThreshold = 20;

// Helper routine that returns a singleton SkPaint object configured for
// rendering badge overlay text (correct font, typeface, etc).
SkPaint* GetBadgeTextPaintSingleton() {
#if defined(OS_MACOSX)
  const char kPreferredTypeface[] = "Helvetica Bold";
#else
  const char kPreferredTypeface[] = "Arial";
#endif

  static SkPaint* text_paint = NULL;
  if (!text_paint) {
    text_paint = new SkPaint;
    text_paint->setAntiAlias(true);
    text_paint->setTextAlign(SkPaint::kLeft_Align);

    sk_sp<SkTypeface> typeface(
        SkTypeface::MakeFromName(kPreferredTypeface,
                                 SkFontStyle::FromOldStyle(SkTypeface::kBold)));
    // Skia doesn't do any font fallback---if the user is missing the font then
    // typeface will be NULL. If we don't do manual fallback then we'll crash.
    if (typeface) {
      text_paint->setFakeBoldText(true);
    } else {
      // Fall back to the system font. We don't bold it because we aren't sure
      // how it will look.
      // For the most part this code path will only be hit on Linux systems
      // that don't have Arial.
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      const gfx::Font& base_font = rb.GetFont(ResourceBundle::BaseFont);
      typeface = SkTypeface::MakeFromName(base_font.GetFontName().c_str(),
                                          SkFontStyle());
      DCHECK(typeface);
    }

    text_paint->setTypeface(std::move(typeface));
  }
  return text_paint;
}

gfx::ImageSkiaRep ScaleImageSkiaRep(const gfx::ImageSkiaRep& rep,
                                    int target_width_dp,
                                    float target_scale) {
  int width_px = target_width_dp * target_scale;
  return gfx::ImageSkiaRep(
      skia::ImageOperations::Resize(rep.sk_bitmap(),
                                    skia::ImageOperations::RESIZE_BEST,
                                    width_px, width_px),
      target_scale);
}

}  // namespace

IconWithBadgeImageSource::Badge::Badge(const std::string& text,
                                       SkColor text_color,
                                       SkColor background_color)
    : text(text), text_color(text_color), background_color(background_color) {}

IconWithBadgeImageSource::Badge::~Badge() {}

IconWithBadgeImageSource::IconWithBadgeImageSource(const gfx::Size& size)
    : gfx::CanvasImageSource(size, false),
      grayscale_(false),
      paint_page_action_decoration_(false),
      paint_blocked_actions_decoration_(false) {}

IconWithBadgeImageSource::~IconWithBadgeImageSource() {}

void IconWithBadgeImageSource::SetIcon(const gfx::Image& icon) {
  icon_ = icon;
}

void IconWithBadgeImageSource::SetBadge(std::unique_ptr<Badge> badge) {
  badge_ = std::move(badge);
}

void IconWithBadgeImageSource::Draw(gfx::Canvas* canvas) {
  if (icon_.IsEmpty())
    return;

  gfx::ImageSkia skia = icon_.AsImageSkia();
  gfx::ImageSkiaRep rep = skia.GetRepresentation(canvas->image_scale());
  if (rep.scale() != canvas->image_scale()) {
    skia.AddRepresentation(ScaleImageSkiaRep(
        rep, ExtensionAction::ActionIconSize(), canvas->image_scale()));
  }
  if (grayscale_)
    skia = gfx::ImageSkiaOperations::CreateHSLShiftedImage(skia, {-1, 0, 0.75});

  int x_offset =
      std::floor((size().width() - ExtensionAction::ActionIconSize()) / 2.0);
  int y_offset =
      std::floor((size().height() - ExtensionAction::ActionIconSize()) / 2.0);
  canvas->DrawImageInt(skia, x_offset, y_offset);

  // Draw a badge on the provided browser action icon's canvas.
  PaintBadge(canvas);

  if (paint_page_action_decoration_)
    PaintPageActionDecoration(canvas);

  if (paint_blocked_actions_decoration_)
    PaintBlockedActionDecoration(canvas);
}

// Paints badge with specified parameters to |canvas|.
void IconWithBadgeImageSource::PaintBadge(gfx::Canvas* canvas) {
  if (!badge_ || badge_->text.empty())
    return;

  SkColor text_color = SkColorGetA(badge_->text_color) == SK_AlphaTRANSPARENT
                           ? SK_ColorWHITE
                           : badge_->text_color;

  SkColor background_color = ui::MaterialDesignController::IsModeMaterial()
                                 ? gfx::kGoogleBlue500
                                 : SkColorSetRGB(218, 0, 24);
  if (SkColorGetA(badge_->background_color) != SK_AlphaTRANSPARENT)
    background_color = badge_->background_color;
  // Make sure the background color is opaque. See http://crbug.com/619499
  if (ui::MaterialDesignController::IsModeMaterial())
    background_color = SkColorSetA(background_color, SK_AlphaOPAQUE);

  canvas->Save();

  SkPaint* text_paint = nullptr;
  int text_width = 0;
  ResourceBundle* rb = &ResourceBundle::GetSharedInstance();
  gfx::FontList base_font = rb->GetFontList(ResourceBundle::BaseFont)
                                .DeriveWithHeightUpperBound(kBadgeHeight);
  base::string16 utf16_text = base::UTF8ToUTF16(badge_->text);

  // See if we can squeeze a slightly larger font into the badge given the
  // actual string that is to be displayed.
  const int kMaxIncrementAttempts = 5;
  for (size_t i = 0; i < kMaxIncrementAttempts; ++i) {
    int w = 0;
    int h = 0;
    gfx::FontList bigger_font =
        base_font.Derive(1, 0, gfx::Font::Weight::NORMAL);
    gfx::Canvas::SizeStringInt(utf16_text, bigger_font, &w, &h, 0,
                               gfx::Canvas::NO_ELLIPSIS);
    if (h > kBadgeHeight)
      break;
    base_font = bigger_font;
  }

  if (ui::MaterialDesignController::IsModeMaterial()) {
    text_width =
        std::min(kMaxTextWidth, canvas->GetStringWidth(utf16_text, base_font));
  } else {
    text_paint = GetBadgeTextPaintSingleton();
    text_paint->setColor(text_color);
    float scale = canvas->image_scale();

    // Calculate text width. Font width may not be linear with respect to the
    // scale factor (e.g. when hinting is applied), so we need to use the font
    // size that canvas actually uses when drawing a text.
    text_paint->setTextSize(SkFloatToScalar(kTextSize) * scale);
    SkScalar sk_text_width_in_pixel =
        text_paint->measureText(badge_->text.c_str(), badge_->text.size());
    text_paint->setTextSize(SkFloatToScalar(kTextSize));

    // We clamp the width to a max size. SkPaint::measureText returns the width
    // in pixel (as a result of scale multiplier), so convert
    // sk_text_width_in_pixel back to DIP (density independent pixel) first.
    text_width = std::min(
        kMaxTextWidth, static_cast<int>(std::ceil(
                           SkScalarToFloat(sk_text_width_in_pixel) / scale)));
  }

  // Calculate badge size. It is clamped to a min width just because it looks
  // silly if it is too skinny.
  int badge_width = text_width + kPadding * 2;
  // Force the pixel width of badge to be either odd (if the icon width is odd)
  // or even otherwise. If there is a mismatch you get http://crbug.com/26400.
  if (size().width() != 0 && (badge_width % 2 != size().width() % 2))
    badge_width += 1;
  badge_width = std::max(kBadgeHeight, badge_width);

  // Calculate the badge background rect. It is usually right-aligned, but it
  // can also be center-aligned if it is large.
  gfx::Rect rect(badge_width >= kCenterAlignThreshold
                     ? (size().width() - badge_width) / 2
                     : size().width() - badge_width,
                 size().height() - kBadgeHeight, badge_width, kBadgeHeight);
  SkPaint rect_paint;
  rect_paint.setStyle(SkPaint::kFill_Style);
  rect_paint.setAntiAlias(true);
  rect_paint.setColor(background_color);

  if (ui::MaterialDesignController::IsModeMaterial()) {
    // Clear part of the background icon.
    gfx::Rect cutout_rect(rect);
    cutout_rect.Inset(-1, -1);
    SkPaint cutout_paint = rect_paint;
    cutout_paint.setXfermodeMode(SkXfermode::kClear_Mode);
    canvas->DrawRoundRect(cutout_rect, 2, cutout_paint);

    // Paint the backdrop.
    canvas->DrawRoundRect(rect, 1, rect_paint);

    // Paint the text.
    rect.Inset(std::max(kPadding, (rect.width() - text_width) / 2),
               kBadgeHeight - base_font.GetHeight(), kPadding, 0);
    canvas->DrawStringRect(utf16_text, base_font, text_color, rect);
  } else {
    // Paint the backdrop.
    canvas->DrawRoundRect(rect, 2, rect_paint);

    // Overlay the gradient. It is stretchy, so we do this in three parts.
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    gfx::ImageSkia* gradient_left =
        rb.GetImageSkiaNamed(IDR_BROWSER_ACTION_BADGE_LEFT);
    gfx::ImageSkia* gradient_right =
        rb.GetImageSkiaNamed(IDR_BROWSER_ACTION_BADGE_RIGHT);
    gfx::ImageSkia* gradient_center =
        rb.GetImageSkiaNamed(IDR_BROWSER_ACTION_BADGE_CENTER);

    canvas->DrawImageInt(*gradient_left, rect.x(), rect.y());
    canvas->TileImageInt(
        *gradient_center, rect.x() + gradient_left->width(), rect.y(),
        rect.width() - gradient_left->width() - gradient_right->width(),
        rect.height());
    canvas->DrawImageInt(*gradient_right,
                         rect.right() - gradient_right->width(), rect.y());

    // Finally, draw the text centered within the badge. We set a clip in case
    // the text was too large.
    rect.Inset(kPadding, 0);
    canvas->ClipRect(rect);
    canvas->sk_canvas()->drawText(
        badge_->text.c_str(), badge_->text.size(),
        SkFloatToScalar(rect.x() +
                        static_cast<float>(rect.width() - text_width) / 2),
        SkFloatToScalar(rect.y() + kTextSize + kTopTextPadding), *text_paint);
  }
  canvas->Restore();
}

void IconWithBadgeImageSource::PaintPageActionDecoration(gfx::Canvas* canvas) {
  static const SkColor decoration_color = SkColorSetARGB(255, 70, 142, 226);

  int major_radius = std::ceil(size().width() / 5.0);
  int minor_radius = std::ceil(major_radius / 2.0);
  gfx::Point center_point(major_radius + 1, size().height() - (major_radius)-1);
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(SK_ColorTRANSPARENT);
  paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  canvas->DrawCircle(center_point, major_radius, paint);
  paint.setColor(decoration_color);
  canvas->DrawCircle(center_point, minor_radius, paint);
}

void IconWithBadgeImageSource::PaintBlockedActionDecoration(
    gfx::Canvas* canvas) {
  canvas->Save();
  gfx::ImageSkia img = *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_BLOCKED_EXTENSION_SCRIPT);
  canvas->DrawImageInt(img, size().width() - img.width(), 0);
  canvas->Restore();
}
