// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/skpicture_content_layer_updater.h"

#include "base/trace_event/trace_event.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/resources/layer_painter.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/resource_update_queue.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"

namespace cc {

SkPictureContentLayerUpdater::SkPictureContentLayerUpdater(
    scoped_ptr<LayerPainter> painter,
    int layer_id)
    : ContentLayerUpdater(painter.Pass(), layer_id) {
}

SkPictureContentLayerUpdater::~SkPictureContentLayerUpdater() {}

void SkPictureContentLayerUpdater::PrepareToUpdate(
    const gfx::Size& content_size,
    const gfx::Rect& paint_rect,
    const gfx::Size& tile_size,
    float contents_width_scale,
    float contents_height_scale) {
  SkPictureRecorder recorder;
  SkCanvas* canvas =
      recorder.beginRecording(paint_rect.width(), paint_rect.height(), NULL, 0);
  DCHECK_EQ(paint_rect.width(), canvas->getBaseLayerSize().width());
  DCHECK_EQ(paint_rect.height(), canvas->getBaseLayerSize().height());
  PaintContents(canvas,
                content_size,
                paint_rect,
                contents_width_scale,
                contents_height_scale);
  picture_ = skia::AdoptRef(recorder.endRecordingAsPicture());
}

void SkPictureContentLayerUpdater::DrawPicture(SkCanvas* canvas) {
  TRACE_EVENT0("cc", "SkPictureContentLayerUpdater::DrawPicture");
  if (picture_)
    canvas->drawPicture(picture_.get());
}

}  // namespace cc
