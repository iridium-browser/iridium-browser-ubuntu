// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_DISPLAY_ITEM_LIST_H_
#define CC_PLAYBACK_DISPLAY_ITEM_LIST_H_

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/cc_export.h"
#include "cc/base/scoped_ptr_vector.h"
#include "cc/playback/display_item.h"
#include "cc/playback/pixel_ref_map.h"
// TODO(danakj): Move ListContainer out of cc/quads/
#include "cc/quads/list_container.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/geometry/rect.h"

class SkCanvas;
class SkDrawPictureCallback;
class SkPictureRecorder;

namespace cc {

class CC_EXPORT DisplayItemList
    : public base::RefCountedThreadSafe<DisplayItemList> {
 public:
  static scoped_refptr<DisplayItemList> Create(gfx::Rect layer_rect,
                                               bool use_cached_picture);

  void Raster(SkCanvas* canvas,
              SkDrawPictureCallback* callback,
              float contents_scale) const;

  template <typename DisplayItemType>
  DisplayItemType* CreateAndAppendItem() {
#if DCHECK_IS_ON()
    needs_process_ = true;
#endif
    ProcessAppendedItemsOnTheFly();
    return items_.AllocateAndConstruct<DisplayItemType>();
  }

  void ProcessAppendedItems();
  void CreateAndCacheSkPicture();

  bool IsSuitableForGpuRasterization() const;
  int ApproximateOpCount() const;
  size_t PictureMemoryUsage() const;

  scoped_refptr<base::trace_event::ConvertableToTraceFormat> AsValue() const;

  void EmitTraceSnapshot() const;

  void GatherPixelRefs(const gfx::Size& grid_cell_size);

 private:
  DisplayItemList(gfx::Rect layer_rect,
                  bool use_cached_picture,
                  bool retain_individual_display_items);
  DisplayItemList(gfx::Rect layer_rect, bool use_cached_picture);
  ~DisplayItemList();

  // While appending new items, if they are not being retained, this can process
  // periodically to avoid retaining all the items and processing at the end.
  void ProcessAppendedItemsOnTheFly();
#if DCHECK_IS_ON()
  bool ProcessAppendedItemsCalled() const { return !needs_process_; }
  bool needs_process_;
#else
  bool ProcessAppendedItemsCalled() const { return true; }
#endif

  ListContainer<DisplayItem> items_;
  skia::RefPtr<SkPicture> picture_;

  scoped_ptr<SkPictureRecorder> recorder_;
  skia::RefPtr<SkCanvas> canvas_;
  bool use_cached_picture_;
  bool retain_individual_display_items_;

  gfx::Rect layer_rect_;
  bool is_suitable_for_gpu_rasterization_;
  int approximate_op_count_;
  size_t picture_memory_usage_;

  scoped_ptr<PixelRefMap> pixel_refs_;

  friend class base::RefCountedThreadSafe<DisplayItemList>;
  friend class PixelRefMap::Iterator;
  FRIEND_TEST_ALL_PREFIXES(DisplayItemListTest, PictureMemoryUsage);
  DISALLOW_COPY_AND_ASSIGN(DisplayItemList);
};

}  // namespace cc

#endif  // CC_PLAYBACK_DISPLAY_ITEM_LIST_H_
