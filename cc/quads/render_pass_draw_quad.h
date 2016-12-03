// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_QUADS_RENDER_PASS_DRAW_QUAD_H_
#define CC_QUADS_RENDER_PASS_DRAW_QUAD_H_

#include <stddef.h>

#include <memory>

#include "cc/base/cc_export.h"
#include "cc/output/filter_operations.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/render_pass_id.h"

#include "ui/gfx/geometry/point_f.h"

namespace cc {

class CC_EXPORT RenderPassDrawQuad : public DrawQuad {
 public:
  static const size_t kMaskResourceIdIndex = 0;

  RenderPassDrawQuad();
  RenderPassDrawQuad(const RenderPassDrawQuad& other);
  ~RenderPassDrawQuad() override;

  void SetNew(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              RenderPassId render_pass_id,
              ResourceId mask_resource_id,
              const gfx::Vector2dF& mask_uv_scale,
              const gfx::Size& mask_texture_size,
              const FilterOperations& filters,
              const gfx::Vector2dF& filters_scale,
              const gfx::PointF& filters_origin,
              const FilterOperations& background_filters);

  void SetAll(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& opaque_rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              RenderPassId render_pass_id,
              ResourceId mask_resource_id,
              const gfx::Vector2dF& mask_uv_scale,
              const gfx::Size& mask_texture_size,
              const FilterOperations& filters,
              const gfx::Vector2dF& filters_scale,
              const gfx::PointF& filters_origin,
              const FilterOperations& background_filters);

  RenderPassId render_pass_id;
  gfx::Vector2dF mask_uv_scale;
  gfx::Size mask_texture_size;

  // Post-processing filters, applied to the pixels in the render pass' texture.
  FilterOperations filters;

  // The scale from layer space of the root layer of the render pass to
  // the render pass physical pixels. This scale is applied to the filter
  // parameters for pixel-moving filters. This scale should include
  // content-to-target-space scale, and device pixel ratio.
  gfx::Vector2dF filters_scale;

  // The origin for post-processing filters which will be used to offset
  // crop rects, lights, etc.
  gfx::PointF filters_origin;

  // Post-processing filters, applied to the pixels showing through the
  // background of the render pass, from behind it.
  FilterOperations background_filters;

  // Helper function to generate the normalized uv rect.
  gfx::RectF MaskUVRect() const;

  ResourceId mask_resource_id() const {
    return resources.ids[kMaskResourceIdIndex];
  }

  static const RenderPassDrawQuad* MaterialCast(const DrawQuad*);

 private:
  void ExtendValue(base::trace_event::TracedValue* value) const override;
};

}  // namespace cc

#endif  // CC_QUADS_RENDER_PASS_DRAW_QUAD_H_
