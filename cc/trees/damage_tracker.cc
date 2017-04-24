// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/damage_tracker.h"

#include <stddef.h>

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "cc/base/math_util.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/output/filter_operations.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_impl.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

std::unique_ptr<DamageTracker> DamageTracker::Create() {
  return base::WrapUnique(new DamageTracker());
}

DamageTracker::DamageTracker()
  : mailboxId_(0) {}

DamageTracker::~DamageTracker() {}

void DamageTracker::UpdateDamageTrackingState(
    const LayerImplList& layer_list,
    const RenderSurfaceImpl* target_surface,
    bool target_surface_property_changed_only_from_descendant,
    const gfx::Rect& target_surface_content_rect,
    LayerImpl* target_surface_mask_layer,
    const FilterOperations& filters) {
  //
  // This function computes the "damage rect" of a target surface, and updates
  // the state that is used to correctly track damage across frames. The damage
  // rect is the region of the surface that may have changed and needs to be
  // redrawn. This can be used to scissor what is actually drawn, to save GPU
  // computation and bandwidth.
  //
  // The surface's damage rect is computed as the union of all possible changes
  // that have happened to the surface since the last frame was drawn. This
  // includes:
  //   - any changes for existing layers/surfaces that contribute to the target
  //     surface
  //   - layers/surfaces that existed in the previous frame, but no longer exist
  //
  // The basic algorithm for computing the damage region is as follows:
  //
  //   1. compute damage caused by changes in active/new layers
  //       for each layer in the layer_list:
  //           if the layer is actually a render_surface:
  //               add the surface's damage to our target surface.
  //           else
  //               add the layer's damage to the target surface.
  //
  //   2. compute damage caused by the target surface's mask, if it exists.
  //
  //   3. compute damage caused by old layers/surfaces that no longer exist
  //       for each leftover layer:
  //           add the old layer/surface bounds to the target surface damage.
  //
  //   4. combine all partial damage rects to get the full damage rect.
  //
  // Additional important points:
  //
  // - This algorithm is implicitly recursive; it assumes that descendant
  //   surfaces have already computed their damage.
  //
  // - Changes to layers/surfaces indicate "damage" to the target surface; If a
  //   layer is not changed, it does NOT mean that the layer can skip drawing.
  //   All layers that overlap the damaged region still need to be drawn. For
  //   example, if a layer changed its opacity, then layers underneath must be
  //   re-drawn as well, even if they did not change.
  //
  // - If a layer/surface property changed, the old bounds and new bounds may
  //   overlap... i.e. some of the exposed region may not actually be exposing
  //   anything. But this does not artificially inflate the damage rect. If the
  //   layer changed, its entire old bounds would always need to be redrawn,
  //   regardless of how much it overlaps with the layer's new bounds, which
  //   also need to be entirely redrawn.
  //
  // - See comments in the rest of the code to see what exactly is considered a
  //   "change" in a layer/surface.
  //
  // - To correctly manage exposed rects, SortedRectMap is maintained:
  //
  //      1. All existing rects from the previous frame are marked as
  //         not updated.
  //      2. The map contains all the layer bounds that contributed to
  //         the previous frame (even outside the previous damaged area). If a
  //         layer changes or does not exist anymore, those regions are then
  //         exposed and damage the target surface. As the algorithm progresses,
  //         entries are updated in the map until only leftover layers
  //         that no longer exist stay marked not updated.
  //
  //      3. After the damage rect is computed, the leftover not marked regions
  //         in a map are used to compute are damaged by deleted layers and
  //         erased from map.
  //

  PrepareRectHistoryForUpdate();
  // These functions cannot be bypassed with early-exits, even if we know what
  // the damage will be for this frame, because we need to update the damage
  // tracker state to correctly track the next frame.
  DamageAccumulator damage_from_active_layers =
      TrackDamageFromActiveLayers(layer_list, target_surface);
  DamageAccumulator damage_from_surface_mask =
      TrackDamageFromSurfaceMask(target_surface_mask_layer);
  DamageAccumulator damage_from_leftover_rects = TrackDamageFromLeftoverRects();

  DamageAccumulator damage_for_this_update;

  if (target_surface_property_changed_only_from_descendant) {
    damage_for_this_update.Union(target_surface_content_rect);
  } else {
    // TODO(shawnsingh): can we clamp this damage to the surface's content rect?
    // (affects performance, but not correctness)
    damage_for_this_update.Union(damage_from_active_layers);
    damage_for_this_update.Union(damage_from_surface_mask);
    damage_for_this_update.Union(damage_from_leftover_rects);

    gfx::Rect damage_rect;
    bool is_rect_valid = damage_for_this_update.GetAsRect(&damage_rect);
    if (is_rect_valid) {
      damage_rect = filters.MapRect(
          damage_rect, target_surface->FiltersTransform().matrix());
      damage_for_this_update = DamageAccumulator();
      damage_for_this_update.Union(damage_rect);
    }
  }

  // Damage accumulates until we are notified that we actually did draw on that
  // frame.
  current_damage_.Union(damage_for_this_update);
}

bool DamageTracker::GetDamageRectIfValid(gfx::Rect* rect) {
  return current_damage_.GetAsRect(rect);
}

DamageTracker::LayerRectMapData& DamageTracker::RectDataForLayer(
    int layer_id,
    bool* layer_is_new) {
  LayerRectMapData data(layer_id);

  SortedRectMapForLayers::iterator it = std::lower_bound(
      rect_history_for_layers_.begin(), rect_history_for_layers_.end(), data);

  if (it == rect_history_for_layers_.end() || it->layer_id_ != layer_id) {
    *layer_is_new = true;
    it = rect_history_for_layers_.insert(it, data);
  }

  return *it;
}

DamageTracker::SurfaceRectMapData& DamageTracker::RectDataForSurface(
    int surface_id,
    bool* surface_is_new) {
  SurfaceRectMapData data(surface_id);

  SortedRectMapForSurfaces::iterator it =
      std::lower_bound(rect_history_for_surfaces_.begin(),
                       rect_history_for_surfaces_.end(), data);

  if (it == rect_history_for_surfaces_.end() || it->surface_id_ != surface_id) {
    *surface_is_new = true;
    it = rect_history_for_surfaces_.insert(it, data);
  }

  return *it;
}

DamageTracker::DamageAccumulator DamageTracker::TrackDamageFromActiveLayers(
    const LayerImplList& layer_list,
    const RenderSurfaceImpl* target_surface) {
  DamageAccumulator damage;

  for (size_t layer_index = 0; layer_index < layer_list.size(); ++layer_index) {
    // Visit layers in back-to-front order.
    LayerImpl* layer = layer_list[layer_index];

    // We skip damage from the HUD layer because (a) the HUD layer damages the
    // whole frame and (b) we don't want HUD layer damage to be shown by the
    // HUD damage rect visualization.
    if (layer == layer->layer_tree_impl()->hud_layer())
      continue;

    RenderSurfaceImpl* render_surface = layer->GetRenderSurface();
    if (render_surface && render_surface != target_surface)
      ExtendDamageForRenderSurface(render_surface, &damage);
    else
      ExtendDamageForLayer(layer, &damage);
  }

  return damage;
}

DamageTracker::DamageAccumulator DamageTracker::TrackDamageFromSurfaceMask(
    LayerImpl* target_surface_mask_layer) {
  DamageAccumulator damage;

  if (!target_surface_mask_layer)
    return damage;

  // Currently, if there is any change to the mask, we choose to damage the
  // entire surface. This could potentially be optimized later, but it is not
  // expected to be a common case.
  if (target_surface_mask_layer->LayerPropertyChanged() ||
      !target_surface_mask_layer->update_rect().IsEmpty()) {
    damage.Union(gfx::Rect(target_surface_mask_layer->bounds()));
  }

  return damage;
}

void DamageTracker::PrepareRectHistoryForUpdate() {
  mailboxId_++;
}

DamageTracker::DamageAccumulator DamageTracker::TrackDamageFromLeftoverRects() {
  // After computing damage for all active layers, any leftover items in the
  // current rect history correspond to layers/surfaces that no longer exist.
  // So, these regions are now exposed on the target surface.

  DamageAccumulator damage;
  SortedRectMapForLayers::iterator layer_cur_pos =
      rect_history_for_layers_.begin();
  SortedRectMapForLayers::iterator layer_copy_pos = layer_cur_pos;
  SortedRectMapForSurfaces::iterator surface_cur_pos =
      rect_history_for_surfaces_.begin();
  SortedRectMapForSurfaces::iterator surface_copy_pos = surface_cur_pos;

  // Loop below basically implements std::remove_if loop with and extra
  // processing (adding deleted rect to damage) for deleted items.
  // cur_pos iterator runs through all elements of the vector, but copy_pos
  // always points to the element after the last not deleted element. If new
  // not deleted element found then it is copied to the *copy_pos and copy_pos
  // moved to the next position.
  // If there are no deleted elements then copy_pos iterator is in sync with
  // cur_pos and no copy happens.
  while (layer_cur_pos < rect_history_for_layers_.end()) {
    if (layer_cur_pos->mailboxId_ == mailboxId_) {
      if (layer_cur_pos != layer_copy_pos)
        *layer_copy_pos = *layer_cur_pos;

      ++layer_copy_pos;
    } else {
      damage.Union(layer_cur_pos->rect_);
    }

    ++layer_cur_pos;
  }

  while (surface_cur_pos < rect_history_for_surfaces_.end()) {
    if (surface_cur_pos->mailboxId_ == mailboxId_) {
      if (surface_cur_pos != surface_copy_pos)
        *surface_copy_pos = *surface_cur_pos;

      ++surface_copy_pos;
    } else {
      damage.Union(surface_cur_pos->rect_);
    }

    ++surface_cur_pos;
  }

  if (layer_copy_pos != rect_history_for_layers_.end())
    rect_history_for_layers_.erase(layer_copy_pos,
                                   rect_history_for_layers_.end());
  if (surface_copy_pos != rect_history_for_surfaces_.end())
    rect_history_for_surfaces_.erase(surface_copy_pos,
                                     rect_history_for_surfaces_.end());

  // If the vector has excessive storage, shrink it
  if (rect_history_for_layers_.capacity() > rect_history_for_layers_.size() * 4)
    SortedRectMapForLayers(rect_history_for_layers_)
        .swap(rect_history_for_layers_);
  if (rect_history_for_surfaces_.capacity() >
      rect_history_for_surfaces_.size() * 4)
    SortedRectMapForSurfaces(rect_history_for_surfaces_)
        .swap(rect_history_for_surfaces_);

  return damage;
}

void DamageTracker::ExpandDamageInsideRectWithFilters(
    const gfx::Rect& pre_filter_rect,
    const FilterOperations& filters,
    DamageAccumulator* damage) {
  gfx::Rect damage_rect;
  bool is_valid_rect = damage->GetAsRect(&damage_rect);
  // If the input isn't a valid rect, then there is no point in trying to make
  // it bigger.
  if (!is_valid_rect)
    return;

  // Compute the pixels in the background of the surface that could be affected
  // by the damage in the content below.
  gfx::Rect expanded_damage_rect = filters.MapRect(damage_rect, SkMatrix::I());

  // Restrict it to the rectangle in which the background filter is shown.
  expanded_damage_rect.Intersect(pre_filter_rect);

  damage->Union(expanded_damage_rect);
}

void DamageTracker::ExtendDamageForLayer(LayerImpl* layer,
                                         DamageAccumulator* target_damage) {
  // There are two ways that a layer can damage a region of the target surface:
  //   1. Property change (e.g. opacity, position, transforms):
  //        - the entire region of the layer itself damages the surface.
  //        - the old layer region also damages the surface, because this region
  //          is now exposed.
  //        - note that in many cases the old and new layer rects may overlap,
  //          which is fine.
  //
  //   2. Repaint/update: If a region of the layer that was repainted/updated,
  //      that region damages the surface.
  //
  // Property changes take priority over update rects.
  //
  // This method is called when we want to consider how a layer contributes to
  // its target RenderSurface, even if that layer owns the target RenderSurface
  // itself. To consider how a layer's target surface contributes to the
  // ancestor surface, ExtendDamageForRenderSurface() must be called instead.

  bool layer_is_new = false;
  LayerRectMapData& data = RectDataForLayer(layer->id(), &layer_is_new);
  gfx::Rect old_rect_in_target_space = data.rect_;

  gfx::Rect rect_in_target_space = layer->GetEnclosingRectInTargetSpace();
  data.Update(rect_in_target_space, mailboxId_);

  if (layer_is_new || layer->LayerPropertyChanged()) {
    // If a layer is new or has changed, then its entire layer rect affects the
    // target surface.
    target_damage->Union(rect_in_target_space);

    // The layer's old region is now exposed on the target surface, too.
    // Note old_rect_in_target_space is already in target space.
    target_damage->Union(old_rect_in_target_space);
    return;
  }

  // If the layer properties haven't changed, then the the target surface is
  // only affected by the layer's damaged area, which could be empty.
  gfx::Rect damage_rect =
      gfx::UnionRects(layer->update_rect(), layer->damage_rect());
  damage_rect.Intersect(gfx::Rect(layer->bounds()));
  if (!damage_rect.IsEmpty()) {
    gfx::Rect damage_rect_in_target_space =
        MathUtil::MapEnclosingClippedRect(layer->DrawTransform(), damage_rect);
    target_damage->Union(damage_rect_in_target_space);
  }
}

void DamageTracker::ExtendDamageForRenderSurface(
    RenderSurfaceImpl* render_surface,
    DamageAccumulator* target_damage) {
  // There are two ways a "descendant surface" can damage regions of the "target
  // surface":
  //   1. Property change:
  //        - a surface's geometry can change because of
  //            - changes to descendants (i.e. the subtree) that affect the
  //              surface's content rect
  //            - changes to ancestor layers that propagate their property
  //              changes to their entire subtree.
  //        - just like layers, both the old surface rect and new surface rect
  //          will damage the target surface in this case.
  //
  //   2. Damage rect: This surface may have been damaged by its own layer_list
  //      as well, and that damage should propagate to the target surface.
  //

  bool surface_is_new = false;
  SurfaceRectMapData& data =
      RectDataForSurface(render_surface->id(), &surface_is_new);
  gfx::Rect old_surface_rect = data.rect_;

  gfx::Rect surface_rect_in_target_space =
      gfx::ToEnclosingRect(render_surface->DrawableContentRect());
  data.Update(surface_rect_in_target_space, mailboxId_);

  if (surface_is_new || render_surface->SurfacePropertyChanged()) {
    // The entire surface contributes damage.
    target_damage->Union(surface_rect_in_target_space);

    // The surface's old region is now exposed on the target surface, too.
    target_damage->Union(old_surface_rect);
  } else {
    // Only the surface's damage_rect will damage the target surface.
    gfx::Rect damage_rect_in_local_space;
    bool is_valid_rect = render_surface->damage_tracker()->GetDamageRectIfValid(
        &damage_rect_in_local_space);
    if (is_valid_rect && !damage_rect_in_local_space.IsEmpty()) {
      // If there was damage, transform it to target space, and possibly
      // contribute its reflection if needed.
      const gfx::Transform& draw_transform = render_surface->draw_transform();
      gfx::Rect damage_rect_in_target_space = MathUtil::MapEnclosingClippedRect(
          draw_transform, damage_rect_in_local_space);
      target_damage->Union(damage_rect_in_target_space);
    } else if (!is_valid_rect) {
      target_damage->Union(surface_rect_in_target_space);
    }
  }

  // If the layer has a background filter, this may cause pixels in our surface
  // to be expanded, so we will need to expand any damage at or below this
  // layer. We expand the damage from this layer too, as we need to readback
  // those pixels from the surface with only the contents of layers below this
  // one in them. This means we need to redraw any pixels in the surface being
  // used for the blur in this layer this frame.
  const FilterOperations& background_filters =
      render_surface->BackgroundFilters();
  if (background_filters.HasFilterThatMovesPixels()) {
    ExpandDamageInsideRectWithFilters(surface_rect_in_target_space,
                                      background_filters, target_damage);
  }
}

bool DamageTracker::DamageAccumulator::GetAsRect(gfx::Rect* rect) {
  if (!is_valid_rect_)
    return false;

  base::CheckedNumeric<int> width = right_;
  width -= x_;
  base::CheckedNumeric<int> height = bottom_;
  height -= y_;
  if (!width.IsValid() || !height.IsValid()) {
    is_valid_rect_ = false;
    return false;
  }

  rect->set_x(x_);
  rect->set_y(y_);
  rect->set_width(width.ValueOrDie());
  rect->set_height(height.ValueOrDie());
  return true;
}

}  // namespace cc
