// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/display_item_list.h"

#include <string>

#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/base/math_util.h"
#include "cc/debug/picture_debug_util.h"
#include "cc/debug/traced_picture.h"
#include "cc/debug/traced_value.h"
#include "cc/playback/largest_display_item.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDrawPictureCallback.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/utils/SkPictureUtils.h"
#include "ui/gfx/skia_util.h"

namespace cc {

namespace {

bool PictureTracingEnabled() {
  bool tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
    TRACE_DISABLED_BY_DEFAULT("cc.debug.picture") ","
    TRACE_DISABLED_BY_DEFAULT("devtools.timeline.picture"),
    &tracing_enabled);
  return tracing_enabled;
}

const int kDefaultNumDisplayItemsToReserve = 100;

}  // namespace

DisplayItemList::DisplayItemList(gfx::Rect layer_rect,
                                 bool use_cached_picture,
                                 bool retain_individual_display_items)
    : items_(LargestDisplayItemSize(), kDefaultNumDisplayItemsToReserve),
      use_cached_picture_(use_cached_picture),
      retain_individual_display_items_(retain_individual_display_items),
      layer_rect_(layer_rect),
      is_suitable_for_gpu_rasterization_(true),
      approximate_op_count_(0),
      picture_memory_usage_(0) {
#if DCHECK_IS_ON()
  needs_process_ = false;
#endif
  if (use_cached_picture_) {
    SkRTreeFactory factory;
    recorder_.reset(new SkPictureRecorder());
    canvas_ = skia::SharePtr(recorder_->beginRecording(
        layer_rect_.width(), layer_rect_.height(), &factory));
    canvas_->translate(-layer_rect_.x(), -layer_rect_.y());
    canvas_->clipRect(gfx::RectToSkRect(layer_rect_));
  }
}

DisplayItemList::DisplayItemList(gfx::Rect layer_rect, bool use_cached_picture)
    : DisplayItemList(layer_rect,
                      use_cached_picture,
                      !use_cached_picture || PictureTracingEnabled()) {
}

scoped_refptr<DisplayItemList> DisplayItemList::Create(
    gfx::Rect layer_rect,
    bool use_cached_picture) {
  return make_scoped_refptr(
      new DisplayItemList(layer_rect, use_cached_picture));
}

DisplayItemList::~DisplayItemList() {
}

void DisplayItemList::Raster(SkCanvas* canvas,
                             SkDrawPictureCallback* callback,
                             float contents_scale) const {
  DCHECK(ProcessAppendedItemsCalled());
  if (!use_cached_picture_) {
    canvas->save();
    canvas->scale(contents_scale, contents_scale);
    for (auto* item : items_)
      item->Raster(canvas, callback);
    canvas->restore();
  } else {
    DCHECK(picture_);

    canvas->save();
    canvas->scale(contents_scale, contents_scale);
    canvas->translate(layer_rect_.x(), layer_rect_.y());
    if (callback) {
      // If we have a callback, we need to call |draw()|, |drawPicture()|
      // doesn't take a callback.  This is used by |AnalysisCanvas| to early
      // out.
      picture_->playback(canvas, callback);
    } else {
      // Prefer to call |drawPicture()| on the canvas since it could place the
      // entire picture on the canvas instead of parsing the skia operations.
      canvas->drawPicture(picture_.get());
    }
    canvas->restore();
  }
}

void DisplayItemList::ProcessAppendedItemsOnTheFly() {
  if (retain_individual_display_items_)
    return;
  if (items_.size() >= kDefaultNumDisplayItemsToReserve) {
    ProcessAppendedItems();
    // This function exists to keep the |items_| from growing indefinitely if
    // we're not going to store them anyway. So the items better be deleted
    // after |items_| grows too large and we process it.
    DCHECK(items_.empty());
  }
}

void DisplayItemList::ProcessAppendedItems() {
#if DCHECK_IS_ON()
  needs_process_ = false;
#endif
  for (DisplayItem* item : items_) {
    is_suitable_for_gpu_rasterization_ &=
        item->is_suitable_for_gpu_rasterization();
    approximate_op_count_ += item->approximate_op_count();

    if (use_cached_picture_) {
      DCHECK(canvas_);
      item->Raster(canvas_.get(), NULL);
    }

    if (retain_individual_display_items_) {
      // Warning: this double-counts SkPicture data if use_cached_picture_ is
      // also true.
      picture_memory_usage_ += item->picture_memory_usage();
    }
  }

  if (!retain_individual_display_items_)
    items_.clear();
}

void DisplayItemList::CreateAndCacheSkPicture() {
  DCHECK(ProcessAppendedItemsCalled());
  // Convert to an SkPicture for faster rasterization.
  DCHECK(use_cached_picture_);
  DCHECK(!picture_);
  picture_ = skia::AdoptRef(recorder_->endRecordingAsPicture());
  DCHECK(picture_);
  picture_memory_usage_ += SkPictureUtils::ApproximateBytesUsed(picture_.get());
  recorder_.reset();
  canvas_.clear();
}

bool DisplayItemList::IsSuitableForGpuRasterization() const {
  DCHECK(ProcessAppendedItemsCalled());
  // This is more permissive than Picture's implementation, since none of the
  // items might individually trigger a veto even though they collectively have
  // enough "bad" operations that a corresponding Picture would get vetoed.
  return is_suitable_for_gpu_rasterization_;
}

int DisplayItemList::ApproximateOpCount() const {
  DCHECK(ProcessAppendedItemsCalled());
  return approximate_op_count_;
}

size_t DisplayItemList::PictureMemoryUsage() const {
  DCHECK(ProcessAppendedItemsCalled());
  // We double-count in this case. Produce zero to avoid being misleading.
  if (use_cached_picture_ && retain_individual_display_items_)
    return 0;

  DCHECK_IMPLIES(use_cached_picture_, picture_);
  return picture_memory_usage_;
}

scoped_refptr<base::trace_event::ConvertableToTraceFormat>
DisplayItemList::AsValue() const {
  DCHECK(ProcessAppendedItemsCalled());
  scoped_refptr<base::trace_event::TracedValue> state =
      new base::trace_event::TracedValue();

  state->SetInteger("length", items_.size());
  state->BeginArray("params.items");
  for (const DisplayItem* item : items_) {
    item->AsValueInto(state.get());
  }
  state->EndArray();
  state->SetValue("params.layer_rect", MathUtil::AsValue(layer_rect_));

  SkPictureRecorder recorder;
  SkCanvas* canvas =
      recorder.beginRecording(layer_rect_.width(), layer_rect_.height());
  canvas->translate(-layer_rect_.x(), -layer_rect_.y());
  canvas->clipRect(gfx::RectToSkRect(layer_rect_));
  Raster(canvas, NULL, 1.f);
  skia::RefPtr<SkPicture> picture =
      skia::AdoptRef(recorder.endRecordingAsPicture());

  std::string b64_picture;
  PictureDebugUtil::SerializeAsBase64(picture.get(), &b64_picture);
  state->SetString("skp64", b64_picture);

  return state;
}

void DisplayItemList::EmitTraceSnapshot() const {
  DCHECK(ProcessAppendedItemsCalled());
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.picture") ","
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.picture"),
      "cc::DisplayItemList", this, AsValue());
}

void DisplayItemList::GatherPixelRefs(const gfx::Size& grid_cell_size) {
  DCHECK(ProcessAppendedItemsCalled());
  // This should be only called once, and only after CreateAndCacheSkPicture.
  DCHECK(picture_);
  DCHECK(!pixel_refs_);
  pixel_refs_ = make_scoped_ptr(new PixelRefMap(grid_cell_size));
  if (!picture_->willPlayBackBitmaps())
    return;

  pixel_refs_->GatherPixelRefsFromPicture(picture_.get());
}
}  // namespace cc
