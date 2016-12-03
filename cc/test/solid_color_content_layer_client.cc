// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/solid_color_content_layer_client.h"

#include <stddef.h>

#include "cc/playback/display_item_list_settings.h"
#include "cc/playback/drawing_display_item.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/skia_util.h"

namespace cc {

gfx::Rect SolidColorContentLayerClient::PaintableRegion() {
  return gfx::Rect(size_);
}

scoped_refptr<DisplayItemList>
SolidColorContentLayerClient::PaintContentsToDisplayList(
    PaintingControlSetting painting_control) {
  SkPictureRecorder recorder;
  gfx::Rect clip(PaintableRegion());
  sk_sp<SkCanvas> canvas =
      sk_ref_sp(recorder.beginRecording(gfx::RectToSkRect(clip)));

  canvas->clear(SK_ColorTRANSPARENT);

  if (border_size_ != 0) {
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(border_color_);
    canvas->drawRect(
        SkRect::MakeXYWH(clip.x(), clip.y(), clip.width(), clip.height()),
        paint);
  }

  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(color_);
  canvas->drawRect(
      SkRect::MakeXYWH(clip.x() + border_size_, clip.y() + border_size_,
                       clip.width() - 2 * border_size_,
                       clip.height() - 2 * border_size_),
      paint);

  DisplayItemListSettings settings;
  settings.use_cached_picture = false;
  scoped_refptr<DisplayItemList> display_list =
      DisplayItemList::Create(settings);

  display_list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
      clip, recorder.finishRecordingAsPicture());

  display_list->Finalize();
  return display_list;
}

bool SolidColorContentLayerClient::FillsBoundsCompletely() const {
  return false;
}

size_t SolidColorContentLayerClient::GetApproximateUnsharedMemoryUsage() const {
  return 0;
}

}  // namespace cc
