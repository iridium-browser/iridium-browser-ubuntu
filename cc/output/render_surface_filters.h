// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CC_OUTPUT_RENDER_SURFACE_FILTERS_H_
#define CC_OUTPUT_RENDER_SURFACE_FILTERS_H_

#include "base/macros.h"
#include "cc/base/cc_export.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class GrContext;
class SkBitmap;
class SkImageFilter;

namespace gfx {
class SizeF;
}

namespace cc {

class FilterOperations;

class CC_EXPORT RenderSurfaceFilters {
 public:
  static SkBitmap Apply(const FilterOperations& filters,
                        unsigned texture_id,
                        const gfx::SizeF& size,
                        GrContext* gr_context);
  static FilterOperations Optimize(const FilterOperations& filters);

  static sk_sp<SkImageFilter> BuildImageFilter(const FilterOperations& filters,
                                               const gfx::SizeF& size);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(RenderSurfaceFilters);
};

}  // namespace cc

#endif  // CC_OUTPUT_RENDER_SURFACE_FILTERS_H_
