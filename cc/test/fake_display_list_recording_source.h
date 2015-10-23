// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_DISPLAY_LIST_RECORDING_SOURCE_H_
#define CC_TEST_FAKE_DISPLAY_LIST_RECORDING_SOURCE_H_

#include "cc/base/region.h"
#include "cc/playback/display_list_recording_source.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/trees/layer_tree_settings.h"

namespace cc {

// This class provides method for test to add bitmap and draw rect to content
// layer client. This class also provides function to rerecord to generate a new
// display list.
class FakeDisplayListRecordingSource : public DisplayListRecordingSource {
 public:
  explicit FakeDisplayListRecordingSource(const gfx::Size& grid_cell_size)
      : DisplayListRecordingSource(grid_cell_size) {}
  ~FakeDisplayListRecordingSource() override {}

  static scoped_ptr<FakeDisplayListRecordingSource> CreateRecordingSource(
      const gfx::Rect& recorded_viewport,
      const gfx::Size& layer_bounds) {
    scoped_ptr<FakeDisplayListRecordingSource> recording_source(
        new FakeDisplayListRecordingSource(
            LayerTreeSettings().default_tile_grid_size));
    recording_source->SetRecordedViewport(recorded_viewport);
    recording_source->SetLayerBounds(layer_bounds);
    return recording_source;
  }

  static scoped_ptr<FakeDisplayListRecordingSource> CreateFilledRecordingSource(
      const gfx::Size& layer_bounds) {
    scoped_ptr<FakeDisplayListRecordingSource> recording_source(
        new FakeDisplayListRecordingSource(
            LayerTreeSettings().default_tile_grid_size));
    recording_source->SetRecordedViewport(gfx::Rect(layer_bounds));
    recording_source->SetLayerBounds(layer_bounds);
    return recording_source;
  }

  void SetRecordedViewport(const gfx::Rect& recorded_viewport) {
    recorded_viewport_ = recorded_viewport;
  }

  void SetLayerBounds(const gfx::Size& layer_bounds) { size_ = layer_bounds; }

  void SetGridCellSize(const gfx::Size& grid_cell_size) {
    grid_cell_size_ = grid_cell_size;
  }

  void SetClearCanvasWithDebugColor(bool clear) {
    clear_canvas_with_debug_color_ = clear;
  }

  void Rerecord() {
    Region invalidation = recorded_viewport_;
    UpdateAndExpandInvalidation(&client_, &invalidation, size_,
                                recorded_viewport_, 0, RECORD_NORMALLY);
  }

  void add_draw_rect(const gfx::RectF& rect) {
    client_.add_draw_rect(rect, default_paint_);
  }

  void add_draw_rect_with_paint(const gfx::RectF& rect, const SkPaint& paint) {
    client_.add_draw_rect(rect, paint);
  }

  void add_draw_bitmap(const SkBitmap& bitmap, const gfx::Point& point) {
    client_.add_draw_bitmap(bitmap, point, default_paint_);
  }

  void add_draw_bitmap_with_transform(const SkBitmap& bitmap,
                                      const gfx::Transform& transform) {
    client_.add_draw_bitmap_with_transform(bitmap, transform, default_paint_);
  }

  void add_draw_bitmap_with_paint(const SkBitmap& bitmap,
                                  const gfx::Point& point,
                                  const SkPaint& paint) {
    client_.add_draw_bitmap(bitmap, point, paint);
  }

  void add_draw_bitmap_with_paint_and_transform(const SkBitmap& bitmap,
                                                const gfx::Transform& transform,
                                                const SkPaint& paint) {
    client_.add_draw_bitmap_with_transform(bitmap, transform, paint);
  }

  void set_default_paint(const SkPaint& paint) { default_paint_ = paint; }

  void set_reported_memory_usage(size_t reported_memory_usage) {
    client_.set_reported_memory_usage(reported_memory_usage);
  }

 private:
  FakeContentLayerClient client_;
  SkPaint default_paint_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_DISPLAY_LIST_RECORDING_SOURCE_H_
