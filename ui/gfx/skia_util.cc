// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/skia_util.h"

#include <stddef.h>
#include <stdint.h>

#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkUnPreMultiply.h"
#include "third_party/skia/include/effects/SkBlurMaskFilter.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "third_party/skia/include/effects/SkLayerDrawLooper.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/transform.h"

namespace gfx {

SkPoint PointToSkPoint(const Point& point) {
  return SkPoint::Make(SkIntToScalar(point.x()), SkIntToScalar(point.y()));
}

SkIPoint PointToSkIPoint(const Point& point) {
  return SkIPoint::Make(point.x(), point.y());
}

SkPoint PointFToSkPoint(const PointF& point) {
  return SkPoint::Make(SkFloatToScalar(point.x()), SkFloatToScalar(point.y()));
}

SkRect RectToSkRect(const Rect& rect) {
  return SkRect::MakeXYWH(
      SkIntToScalar(rect.x()), SkIntToScalar(rect.y()),
      SkIntToScalar(rect.width()), SkIntToScalar(rect.height()));
}

SkIRect RectToSkIRect(const Rect& rect) {
  return SkIRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height());
}

// Produces a non-negative integer for the difference between min and max,
// yielding 0 if it would be negative and INT_MAX if it would overflow.
// This yields a length such that min+length is in range as well.
static int ClampLengthFromRange(int min, int max) {
  if (min > max)
    return 0;
  return (base::CheckedNumeric<int>(max) - min)
      .ValueOrDefault(std::numeric_limits<int>::max());
}

Rect SkIRectToRect(const SkIRect& rect) {
  return Rect(rect.x(), rect.y(),
              ClampLengthFromRange(rect.left(), rect.right()),
              ClampLengthFromRange(rect.top(), rect.bottom()));
}

SkRect RectFToSkRect(const RectF& rect) {
  return SkRect::MakeXYWH(SkFloatToScalar(rect.x()),
                          SkFloatToScalar(rect.y()),
                          SkFloatToScalar(rect.width()),
                          SkFloatToScalar(rect.height()));
}

RectF SkRectToRectF(const SkRect& rect) {
  return RectF(SkScalarToFloat(rect.x()),
               SkScalarToFloat(rect.y()),
               SkScalarToFloat(rect.width()),
               SkScalarToFloat(rect.height()));
}

SkSize SizeFToSkSize(const SizeF& size) {
  return SkSize::Make(SkFloatToScalar(size.width()),
                      SkFloatToScalar(size.height()));
}

SizeF SkSizeToSizeF(const SkSize& size) {
  return SizeF(SkScalarToFloat(size.width()), SkScalarToFloat(size.height()));
}

Size SkISizeToSize(const SkISize& size) {
  return Size(size.width(), size.height());
}

void TransformToFlattenedSkMatrix(const gfx::Transform& transform,
                                  SkMatrix* flattened) {
  // Convert from 4x4 to 3x3 by dropping the third row and column.
  flattened->set(0, SkMScalarToScalar(transform.matrix().get(0, 0)));
  flattened->set(1, SkMScalarToScalar(transform.matrix().get(0, 1)));
  flattened->set(2, SkMScalarToScalar(transform.matrix().get(0, 3)));
  flattened->set(3, SkMScalarToScalar(transform.matrix().get(1, 0)));
  flattened->set(4, SkMScalarToScalar(transform.matrix().get(1, 1)));
  flattened->set(5, SkMScalarToScalar(transform.matrix().get(1, 3)));
  flattened->set(6, SkMScalarToScalar(transform.matrix().get(3, 0)));
  flattened->set(7, SkMScalarToScalar(transform.matrix().get(3, 1)));
  flattened->set(8, SkMScalarToScalar(transform.matrix().get(3, 3)));
}

sk_sp<SkShader> CreateImageRepShader(const gfx::ImageSkiaRep& image_rep,
                                            SkShader::TileMode tile_mode,
                                            const SkMatrix& local_matrix) {
  return CreateImageRepShaderForScale(image_rep, tile_mode, local_matrix,
                                      image_rep.scale());
}

sk_sp<SkShader> CreateImageRepShaderForScale(
    const gfx::ImageSkiaRep& image_rep,
    SkShader::TileMode tile_mode,
    const SkMatrix& local_matrix,
    SkScalar scale) {
  // Unscale matrix by |scale| such that the bitmap is drawn at the
  // correct density.
  // Convert skew and translation to pixel coordinates.
  // Thus, for |bitmap_scale| = 2:
  //   x scale = 2, x translation = 1 DIP,
  // should be converted to
  //   x scale = 1, x translation = 2 pixels.
  SkMatrix shader_scale = local_matrix;
  shader_scale.preScale(scale, scale);
  shader_scale.setScaleX(local_matrix.getScaleX() / scale);
  shader_scale.setScaleY(local_matrix.getScaleY() / scale);

  return SkShader::MakeBitmapShader(
      image_rep.sk_bitmap(), tile_mode, tile_mode, &shader_scale);
}

sk_sp<SkShader> CreateGradientShader(int start_point,
                                            int end_point,
                                            SkColor start_color,
                                            SkColor end_color) {
  SkColor grad_colors[2] = { start_color, end_color};
  SkPoint grad_points[2];
  grad_points[0].iset(0, start_point);
  grad_points[1].iset(0, end_point);

  return SkGradientShader::MakeLinear(
      grad_points, grad_colors, NULL, 2, SkShader::kClamp_TileMode);
}

// TODO(estade): remove. Only exists to support legacy CreateShadowDrawLooper.
static SkScalar DeprecatedRadiusToSigma(double radius) {
  // This captures historically what skia did under the hood. Now skia accepts
  // sigma, not radius, so we perform the conversion.
  return radius > 0 ? SkDoubleToScalar(0.57735f * radius + 0.5) : 0;
}

// This is copied from
// third_party/WebKit/Source/platform/graphics/skia/SkiaUtils.h
static SkScalar RadiusToSigma(double radius) {
  return radius > 0 ? SkDoubleToScalar(0.288675f * radius + 0.5f) : 0;
}

sk_sp<SkDrawLooper> CreateShadowDrawLooper(
    const std::vector<ShadowValue>& shadows) {
  if (shadows.empty())
    return nullptr;

  SkLayerDrawLooper::Builder looper_builder;

  looper_builder.addLayer();  // top layer of the original.

  SkLayerDrawLooper::LayerInfo layer_info;
  layer_info.fPaintBits |= SkLayerDrawLooper::kMaskFilter_Bit;
  layer_info.fPaintBits |= SkLayerDrawLooper::kColorFilter_Bit;
  layer_info.fColorMode = SkBlendMode::kSrc;

  for (size_t i = 0; i < shadows.size(); ++i) {
    const ShadowValue& shadow = shadows[i];

    layer_info.fOffset.set(SkIntToScalar(shadow.x()),
                           SkIntToScalar(shadow.y()));

    SkPaint* paint = looper_builder.addLayer(layer_info);
    // SkBlurMaskFilter's blur radius defines the range to extend the blur from
    // original mask, which is half of blur amount as defined in ShadowValue.
    // Note that because this function uses DeprecatedRadiusToSigma, it actually
    // creates a draw looper with roughly twice the desired blur.
    paint->setMaskFilter(SkBlurMaskFilter::Make(
        kNormal_SkBlurStyle, DeprecatedRadiusToSigma(shadow.blur() / 2),
        SkBlurMaskFilter::kHighQuality_BlurFlag));
    paint->setColorFilter(
        SkColorFilter::MakeModeFilter(shadow.color(), SkBlendMode::kSrcIn));
  }

  return looper_builder.detach();
}

sk_sp<SkDrawLooper> CreateShadowDrawLooperCorrectBlur(
    const std::vector<ShadowValue>& shadows) {
  if (shadows.empty())
    return nullptr;

  SkLayerDrawLooper::Builder looper_builder;

  looper_builder.addLayer();  // top layer of the original.

  SkLayerDrawLooper::LayerInfo layer_info;
  layer_info.fPaintBits |= SkLayerDrawLooper::kMaskFilter_Bit;
  layer_info.fPaintBits |= SkLayerDrawLooper::kColorFilter_Bit;
  layer_info.fColorMode = SkBlendMode::kSrc;

  for (size_t i = 0; i < shadows.size(); ++i) {
    const ShadowValue& shadow = shadows[i];

    layer_info.fOffset.set(SkIntToScalar(shadow.x()),
                           SkIntToScalar(shadow.y()));

    SkPaint* paint = looper_builder.addLayer(layer_info);
    // SkBlurMaskFilter's blur radius defines the range to extend the blur from
    // original mask, which is half of blur amount as defined in ShadowValue.
    paint->setMaskFilter(SkBlurMaskFilter::Make(
        kNormal_SkBlurStyle, RadiusToSigma(shadow.blur() / 2),
        SkBlurMaskFilter::kHighQuality_BlurFlag));
    paint->setColorFilter(
        SkColorFilter::MakeModeFilter(shadow.color(), SkBlendMode::kSrcIn));
  }

  return looper_builder.detach();
}

bool BitmapsAreEqual(const SkBitmap& bitmap1, const SkBitmap& bitmap2) {
  void* addr1 = NULL;
  void* addr2 = NULL;
  size_t size1 = 0;
  size_t size2 = 0;

  bitmap1.lockPixels();
  addr1 = bitmap1.getAddr32(0, 0);
  size1 = bitmap1.getSize();
  bitmap1.unlockPixels();

  bitmap2.lockPixels();
  addr2 = bitmap2.getAddr32(0, 0);
  size2 = bitmap2.getSize();
  bitmap2.unlockPixels();

  return (size1 == size2) && (0 == memcmp(addr1, addr2, bitmap1.getSize()));
}

void ConvertSkiaToRGBA(const unsigned char* skia,
                       int pixel_width,
                       unsigned char* rgba) {
  int total_length = pixel_width * 4;
  for (int i = 0; i < total_length; i += 4) {
    const uint32_t pixel_in = *reinterpret_cast<const uint32_t*>(&skia[i]);

    // Pack the components here.
    SkAlpha alpha = SkGetPackedA32(pixel_in);
    if (alpha != 0 && alpha != 255) {
      SkColor unmultiplied = SkUnPreMultiply::PMColorToColor(pixel_in);
      rgba[i + 0] = SkColorGetR(unmultiplied);
      rgba[i + 1] = SkColorGetG(unmultiplied);
      rgba[i + 2] = SkColorGetB(unmultiplied);
      rgba[i + 3] = alpha;
    } else {
      rgba[i + 0] = SkGetPackedR32(pixel_in);
      rgba[i + 1] = SkGetPackedG32(pixel_in);
      rgba[i + 2] = SkGetPackedB32(pixel_in);
      rgba[i + 3] = alpha;
    }
  }
}

void QuadFToSkPoints(const gfx::QuadF& quad, SkPoint points[4]) {
  points[0] = PointFToSkPoint(quad.p1());
  points[1] = PointFToSkPoint(quad.p2());
  points[2] = PointFToSkPoint(quad.p3());
  points[3] = PointFToSkPoint(quad.p4());
}

// We treat HarfBuzz ints as 16.16 fixed-point.
static const int kHbUnit1 = 1 << 16;

int SkiaScalarToHarfBuzzUnits(SkScalar value) {
  return base::saturated_cast<int>(value * kHbUnit1);
}

SkScalar HarfBuzzUnitsToSkiaScalar(int value) {
  static const SkScalar kSkToHbRatio = SK_Scalar1 / kHbUnit1;
  return kSkToHbRatio * value;
}

float HarfBuzzUnitsToFloat(int value) {
  static const float kFloatToHbRatio = 1.0f / kHbUnit1;
  return kFloatToHbRatio * value;
}

}  // namespace gfx
