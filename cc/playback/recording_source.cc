// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/recording_source.h"

#include <stdint.h>

#include <algorithm>

#include "base/numerics/safe_math.h"
#include "cc/base/region.h"
#include "cc/layers/content_layer_client.h"
#include "cc/playback/display_item_list.h"
#include "cc/playback/raster_source.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/proto/recording_source.pb.h"
#include "skia/ext/analysis_canvas.h"

namespace {

#ifdef NDEBUG
const bool kDefaultClearCanvasSetting = false;
#else
const bool kDefaultClearCanvasSetting = true;
#endif

}  // namespace

namespace cc {

RecordingSource::RecordingSource()
    : slow_down_raster_scale_factor_for_debug_(0),
      generate_discardable_images_metadata_(false),
      requires_clear_(false),
      is_solid_color_(false),
      clear_canvas_with_debug_color_(kDefaultClearCanvasSetting),
      solid_color_(SK_ColorTRANSPARENT),
      background_color_(SK_ColorTRANSPARENT) {}

RecordingSource::~RecordingSource() {}

void RecordingSource::ToProtobuf(proto::RecordingSource* proto) const {
  SizeToProto(size_, proto->mutable_size());
  proto->set_slow_down_raster_scale_factor_for_debug(
      slow_down_raster_scale_factor_for_debug_);
  proto->set_generate_discardable_images_metadata(
      generate_discardable_images_metadata_);
  proto->set_requires_clear(requires_clear_);
  proto->set_is_solid_color(is_solid_color_);
  proto->set_clear_canvas_with_debug_color(clear_canvas_with_debug_color_);
  proto->set_solid_color(static_cast<uint64_t>(solid_color_));
  proto->set_background_color(static_cast<uint64_t>(background_color_));
}

void RecordingSource::FromProtobuf(
    const proto::RecordingSource& proto,
    const scoped_refptr<DisplayItemList>& display_list,
    const gfx::Rect& recorded_viewport) {
  size_ = ProtoToSize(proto.size());
  slow_down_raster_scale_factor_for_debug_ =
      proto.slow_down_raster_scale_factor_for_debug();
  generate_discardable_images_metadata_ =
      proto.generate_discardable_images_metadata();
  requires_clear_ = proto.requires_clear();
  is_solid_color_ = proto.is_solid_color();
  clear_canvas_with_debug_color_ = proto.clear_canvas_with_debug_color();
  solid_color_ = static_cast<SkColor>(proto.solid_color());
  background_color_ = static_cast<SkColor>(proto.background_color());

  display_list_ = display_list;
  recorded_viewport_ = recorded_viewport;
  if (display_list_)
    FinishDisplayItemListUpdate();
}

void RecordingSource::UpdateInvalidationForNewViewport(
    const gfx::Rect& old_recorded_viewport,
    const gfx::Rect& new_recorded_viewport,
    Region* invalidation) {
  // Invalidate newly-exposed and no-longer-exposed areas.
  Region newly_exposed_region(new_recorded_viewport);
  newly_exposed_region.Subtract(old_recorded_viewport);
  invalidation->Union(newly_exposed_region);

  Region no_longer_exposed_region(old_recorded_viewport);
  no_longer_exposed_region.Subtract(new_recorded_viewport);
  invalidation->Union(no_longer_exposed_region);
}

void RecordingSource::FinishDisplayItemListUpdate() {
  TRACE_EVENT0("cc", "RecordingSource::FinishDisplayItemListUpdate");
  DetermineIfSolidColor();
  display_list_->EmitTraceSnapshot();
  if (generate_discardable_images_metadata_)
    display_list_->GenerateDiscardableImagesMetadata();
}

void RecordingSource::SetNeedsDisplayRect(const gfx::Rect& layer_rect) {
  if (!layer_rect.IsEmpty()) {
    // Clamp invalidation to the layer bounds.
    invalidation_.Union(gfx::IntersectRects(layer_rect, gfx::Rect(size_)));
  }
}

bool RecordingSource::UpdateAndExpandInvalidation(
    Region* invalidation,
    const gfx::Size& layer_size,
    const gfx::Rect& new_recorded_viewport) {
  bool updated = false;

  if (size_ != layer_size)
    size_ = layer_size;

  invalidation_.Swap(invalidation);
  invalidation_.Clear();

  if (new_recorded_viewport != recorded_viewport_) {
    UpdateInvalidationForNewViewport(recorded_viewport_, new_recorded_viewport,
                                     invalidation);
    recorded_viewport_ = new_recorded_viewport;
    updated = true;
  }

  if (!updated && !invalidation->Intersects(recorded_viewport_))
    return false;

  if (invalidation->IsEmpty())
    return false;

  return true;
}

void RecordingSource::UpdateDisplayItemList(
    const scoped_refptr<DisplayItemList>& display_list,
    const size_t& painter_reported_memory_usage) {
  display_list_ = display_list;
  painter_reported_memory_usage_ = painter_reported_memory_usage;

  FinishDisplayItemListUpdate();
}

gfx::Size RecordingSource::GetSize() const {
  return size_;
}

void RecordingSource::SetEmptyBounds() {
  size_ = gfx::Size();
  is_solid_color_ = false;

  recorded_viewport_ = gfx::Rect();
  display_list_ = nullptr;
  painter_reported_memory_usage_ = 0;
}

void RecordingSource::SetSlowdownRasterScaleFactor(int factor) {
  slow_down_raster_scale_factor_for_debug_ = factor;
}

void RecordingSource::SetGenerateDiscardableImagesMetadata(
    bool generate_metadata) {
  generate_discardable_images_metadata_ = generate_metadata;
}

void RecordingSource::SetBackgroundColor(SkColor background_color) {
  background_color_ = background_color;
}

void RecordingSource::SetRequiresClear(bool requires_clear) {
  requires_clear_ = requires_clear;
}

const DisplayItemList* RecordingSource::GetDisplayItemList() {
  return display_list_.get();
}

scoped_refptr<RasterSource> RecordingSource::CreateRasterSource(
    bool can_use_lcd_text) const {
  return scoped_refptr<RasterSource>(
      RasterSource::CreateFromRecordingSource(this, can_use_lcd_text));
}

void RecordingSource::DetermineIfSolidColor() {
  DCHECK(display_list_);
  is_solid_color_ = false;
  solid_color_ = SK_ColorTRANSPARENT;

  if (!display_list_->ShouldBeAnalyzedForSolidColor())
    return;

  TRACE_EVENT1("cc", "RecordingSource::DetermineIfSolidColor", "opcount",
               display_list_->ApproximateOpCount());
  gfx::Size layer_size = GetSize();
  skia::AnalysisCanvas canvas(layer_size.width(), layer_size.height());
  display_list_->Raster(&canvas, nullptr, gfx::Rect(layer_size), 1.f);
  is_solid_color_ = canvas.GetColorIfSolid(&solid_color_);
}

}  // namespace cc
