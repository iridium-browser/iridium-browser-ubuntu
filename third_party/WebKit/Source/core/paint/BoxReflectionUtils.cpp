// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BoxReflectionUtils.h"

#include "core/layout/LayoutBox.h"
#include "core/paint/NinePieceImagePainter.h"
#include "core/paint/PaintLayer.h"
#include "platform/LengthFunctions.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/BoxReflection.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/SkPictureBuilder.h"

namespace blink {

BoxReflection boxReflectionForPaintLayer(const PaintLayer& layer,
                                         const ComputedStyle& style) {
  const StyleReflection* reflectStyle = style.boxReflect();

  LayoutRect frameLayoutRect = toLayoutBox(layer.layoutObject())->frameRect();
  FloatRect frameRect(frameLayoutRect);
  BoxReflection::ReflectionDirection direction =
      BoxReflection::VerticalReflection;
  float offset = 0;
  switch (reflectStyle->direction()) {
    case ReflectionAbove:
      direction = BoxReflection::VerticalReflection;
      offset = -floatValueForLength(reflectStyle->offset(), frameRect.height());
      break;
    case ReflectionBelow:
      direction = BoxReflection::VerticalReflection;
      offset = 2 * frameRect.height() +
               floatValueForLength(reflectStyle->offset(), frameRect.height());
      break;
    case ReflectionLeft:
      direction = BoxReflection::HorizontalReflection;
      offset = -floatValueForLength(reflectStyle->offset(), frameRect.width());
      break;
    case ReflectionRight:
      direction = BoxReflection::HorizontalReflection;
      offset = 2 * frameRect.width() +
               floatValueForLength(reflectStyle->offset(), frameRect.width());
      break;
  }

  sk_sp<SkPicture> mask;
  const NinePieceImage& maskNinePiece = reflectStyle->mask();
  if (maskNinePiece.hasImage()) {
    LayoutRect maskRect(LayoutPoint(), frameLayoutRect.size());
    LayoutRect maskBoundingRect(maskRect);
    maskBoundingRect.expand(style.imageOutsets(maskNinePiece));
    FloatRect maskBoundingFloatRect(maskBoundingRect);

    // TODO(jbroman): SkPictureBuilder + DrawingRecorder seems excessive.
    // If NinePieceImagePainter operated on SkCanvas, we'd only need an
    // SkPictureRecorder here.
    SkPictureBuilder recorder(maskBoundingFloatRect);
    {
      GraphicsContext& context = recorder.context();
      DrawingRecorder drawingRecorder(context, *layer.layoutObject(),
                                      DisplayItem::kReflectionMask,
                                      maskBoundingFloatRect);
      NinePieceImagePainter(*layer.layoutObject())
          .paint(recorder.context(), maskRect, style, maskNinePiece,
                 SkBlendMode::kSrcOver);
    }
    mask = recorder.endRecording();
  }

  return BoxReflection(direction, offset, std::move(mask));
}

}  // namespace blink
