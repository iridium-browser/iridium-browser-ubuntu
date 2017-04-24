// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/base/math_util.h"
#include "cc/base/simple_enclosed_region.h"
#include "cc/debug/debug_colors.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "cc/debug/micro_benchmark_impl.h"
#include "cc/debug/traced_value.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/input/scroll_state.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_utils.h"
#include "cc/output/copy_output_request.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/mutable_properties.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/proxy.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {
LayerImpl::LayerImpl(LayerTreeImpl* tree_impl, int id)
    : layer_id_(id),
      layer_tree_impl_(tree_impl),
      test_properties_(nullptr),
      scroll_clip_layer_id_(Layer::INVALID_ID),
      main_thread_scrolling_reasons_(
          MainThreadScrollingReason::kNotScrollingOnMain),
      user_scrollable_horizontal_(true),
      user_scrollable_vertical_(true),
      should_flatten_transform_from_property_tree_(false),
      layer_property_changed_(false),
      may_contain_video_(false),
      masks_to_bounds_(false),
      contents_opaque_(false),
      use_parent_backface_visibility_(false),
      use_local_transform_for_backface_visibility_(false),
      should_check_backface_visibility_(false),
      draws_content_(false),
      is_drawn_render_surface_layer_list_member_(false),
      was_ever_ready_since_last_transform_animation_(true),
      background_color_(0),
      safe_opaque_background_color_(0),
      draw_blend_mode_(SkBlendMode::kSrcOver),
      transform_tree_index_(TransformTree::kInvalidNodeId),
      effect_tree_index_(EffectTree::kInvalidNodeId),
      clip_tree_index_(ClipTree::kInvalidNodeId),
      scroll_tree_index_(ScrollTree::kInvalidNodeId),
      current_draw_mode_(DRAW_MODE_NONE),
      mutable_properties_(MutableProperty::kNone),
      debug_info_(nullptr),
      has_preferred_raster_bounds_(false),
      has_will_change_transform_hint_(false),
      needs_push_properties_(false),
      scrollbars_hidden_(false) {
  DCHECK_GT(layer_id_, 0);

  DCHECK(layer_tree_impl_);
  layer_tree_impl_->RegisterLayer(this);
  layer_tree_impl_->AddToElementMap(this);

  SetNeedsPushProperties();
}

LayerImpl::~LayerImpl() {
  DCHECK_EQ(DRAW_MODE_NONE, current_draw_mode_);

  layer_tree_impl_->UnregisterScrollLayer(this);
  layer_tree_impl_->UnregisterLayer(this);

  layer_tree_impl_->RemoveFromElementMap(this);

  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug"), "cc::LayerImpl", this);
}

void LayerImpl::SetHasWillChangeTransformHint(bool has_will_change) {
  has_will_change_transform_hint_ = has_will_change;
}

void LayerImpl::SetPreferredRasterBounds(
    const gfx::Size& preferred_raster_bounds) {
  has_preferred_raster_bounds_ = true;
  preferred_raster_bounds_ = preferred_raster_bounds;
}

void LayerImpl::ClearPreferredRasterBounds() {
  has_preferred_raster_bounds_ = false;
  preferred_raster_bounds_ = gfx::Size();
}

MutatorHost* LayerImpl::GetMutatorHost() const {
  return layer_tree_impl_ ? layer_tree_impl_->mutator_host() : nullptr;
}

ElementListType LayerImpl::GetElementTypeForAnimation() const {
  return IsActive() ? ElementListType::ACTIVE : ElementListType::PENDING;
}

void LayerImpl::SetDebugInfo(
    std::unique_ptr<base::trace_event::ConvertableToTraceFormat> debug_info) {
  owned_debug_info_ = std::move(debug_info);
  debug_info_ = owned_debug_info_.get();
  SetNeedsPushProperties();
}

void LayerImpl::SetTransformTreeIndex(int index) {
  transform_tree_index_ = index;
}

void LayerImpl::SetClipTreeIndex(int index) {
  clip_tree_index_ = index;
}

void LayerImpl::SetEffectTreeIndex(int index) {
  effect_tree_index_ = index;
}

int LayerImpl::render_target_effect_tree_index() const {
  EffectNode* effect_node = GetEffectTree().Node(effect_tree_index_);

  return GetEffectTree().GetRenderSurface(effect_tree_index_)
             ? effect_node->id
             : effect_node->target_id;
}

void LayerImpl::SetScrollTreeIndex(int index) {
  scroll_tree_index_ = index;
}

void LayerImpl::PopulateSharedQuadState(SharedQuadState* state) const {
  state->SetAll(draw_properties_.target_space_transform, bounds(),
                draw_properties_.visible_layer_rect, draw_properties_.clip_rect,
                draw_properties_.is_clipped, draw_properties_.opacity,
                draw_blend_mode_, GetSortingContextId());
}

void LayerImpl::PopulateScaledSharedQuadState(
    SharedQuadState* state,
    float layer_to_content_scale_x,
    float layer_to_content_scale_y) const {
  gfx::Transform scaled_draw_transform =
      draw_properties_.target_space_transform;
  scaled_draw_transform.Scale(SK_MScalar1 / layer_to_content_scale_x,
                              SK_MScalar1 / layer_to_content_scale_y);
  gfx::Size scaled_bounds = gfx::ScaleToCeiledSize(
      bounds(), layer_to_content_scale_x, layer_to_content_scale_y);
  gfx::Rect scaled_visible_layer_rect = gfx::ScaleToEnclosingRect(
      visible_layer_rect(), layer_to_content_scale_x, layer_to_content_scale_y);
  scaled_visible_layer_rect.Intersect(gfx::Rect(scaled_bounds));

  state->SetAll(scaled_draw_transform, scaled_bounds, scaled_visible_layer_rect,
                draw_properties().clip_rect, draw_properties().is_clipped,
                draw_properties().opacity, draw_blend_mode_,
                GetSortingContextId());
}

bool LayerImpl::WillDraw(DrawMode draw_mode,
                         ResourceProvider* resource_provider) {
  // WillDraw/DidDraw must be matched.
  DCHECK_NE(DRAW_MODE_NONE, draw_mode);
  DCHECK_EQ(DRAW_MODE_NONE, current_draw_mode_);
  current_draw_mode_ = draw_mode;
  return true;
}

void LayerImpl::DidDraw(ResourceProvider* resource_provider) {
  DCHECK_NE(DRAW_MODE_NONE, current_draw_mode_);
  current_draw_mode_ = DRAW_MODE_NONE;
}

bool LayerImpl::ShowDebugBorders() const {
  return layer_tree_impl()->debug_state().show_debug_borders;
}

void LayerImpl::GetDebugBorderProperties(SkColor* color, float* width) const {
  if (draws_content_) {
    *color = DebugColors::ContentLayerBorderColor();
    *width = DebugColors::ContentLayerBorderWidth(layer_tree_impl());
    return;
  }

  if (masks_to_bounds_) {
    *color = DebugColors::MaskingLayerBorderColor();
    *width = DebugColors::MaskingLayerBorderWidth(layer_tree_impl());
    return;
  }

  *color = DebugColors::ContainerLayerBorderColor();
  *width = DebugColors::ContainerLayerBorderWidth(layer_tree_impl());
}

void LayerImpl::AppendDebugBorderQuad(
    RenderPass* render_pass,
    const gfx::Size& bounds,
    const SharedQuadState* shared_quad_state,
    AppendQuadsData* append_quads_data) const {
  SkColor color;
  float width;
  GetDebugBorderProperties(&color, &width);
  AppendDebugBorderQuad(render_pass, bounds, shared_quad_state,
                        append_quads_data, color, width);
}

void LayerImpl::AppendDebugBorderQuad(RenderPass* render_pass,
                                      const gfx::Size& bounds,
                                      const SharedQuadState* shared_quad_state,
                                      AppendQuadsData* append_quads_data,
                                      SkColor color,
                                      float width) const {
  if (!ShowDebugBorders())
    return;

  gfx::Rect quad_rect(bounds);
  gfx::Rect visible_quad_rect(quad_rect);
  DebugBorderDrawQuad* debug_border_quad =
      render_pass->CreateAndAppendDrawQuad<DebugBorderDrawQuad>();
  debug_border_quad->SetNew(
      shared_quad_state, quad_rect, visible_quad_rect, color, width);
  if (contents_opaque()) {
    // When opaque, draw a second inner border that is thicker than the outer
    // border, but more transparent.
    static const float kFillOpacity = 0.3f;
    SkColor fill_color = SkColorSetA(
        color, static_cast<uint8_t>(SkColorGetA(color) * kFillOpacity));
    float fill_width = width * 3;
    gfx::Rect fill_rect = quad_rect;
    fill_rect.Inset(fill_width / 2.f, fill_width / 2.f);
    if (fill_rect.IsEmpty())
      return;
    gfx::Rect visible_fill_rect =
        gfx::IntersectRects(visible_quad_rect, fill_rect);
    DebugBorderDrawQuad* fill_quad =
        render_pass->CreateAndAppendDrawQuad<DebugBorderDrawQuad>();
    fill_quad->SetNew(shared_quad_state, fill_rect, visible_fill_rect,
                      fill_color, fill_width);
  }
}

void LayerImpl::GetContentsResourceId(ResourceId* resource_id,
                                      gfx::Size* resource_size) const {
  NOTREACHED();
  *resource_id = 0;
}

gfx::Vector2dF LayerImpl::ScrollBy(const gfx::Vector2dF& scroll) {
  ScrollTree& scroll_tree = GetScrollTree();
  ScrollNode* scroll_node = scroll_tree.Node(scroll_tree_index());
  return scroll_tree.ScrollBy(scroll_node, scroll, layer_tree_impl());
}

void LayerImpl::SetScrollClipLayer(int scroll_clip_layer_id) {
  if (scroll_clip_layer_id_ == scroll_clip_layer_id)
    return;

  layer_tree_impl()->UnregisterScrollLayer(this);
  scroll_clip_layer_id_ = scroll_clip_layer_id;
  layer_tree_impl()->RegisterScrollLayer(this);
}

LayerImpl* LayerImpl::scroll_clip_layer() const {
  return layer_tree_impl()->LayerById(scroll_clip_layer_id_);
}

bool LayerImpl::scrollable() const {
  return scroll_clip_layer_id_ != Layer::INVALID_ID;
}

void LayerImpl::set_user_scrollable_horizontal(bool scrollable) {
  user_scrollable_horizontal_ = scrollable;
}

void LayerImpl::set_user_scrollable_vertical(bool scrollable) {
  user_scrollable_vertical_ = scrollable;
}

bool LayerImpl::user_scrollable(ScrollbarOrientation orientation) const {
  return (orientation == HORIZONTAL) ? user_scrollable_horizontal_
                                     : user_scrollable_vertical_;
}

std::unique_ptr<LayerImpl> LayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return LayerImpl::Create(tree_impl, layer_id_);
}

bool LayerImpl::IsSnapped() {
  return scrollable();
}

void LayerImpl::PushPropertiesTo(LayerImpl* layer) {
  DCHECK(layer->IsActive());

  layer->offset_to_transform_parent_ = offset_to_transform_parent_;
  layer->main_thread_scrolling_reasons_ = main_thread_scrolling_reasons_;
  layer->user_scrollable_horizontal_ = user_scrollable_horizontal_;
  layer->user_scrollable_vertical_ = user_scrollable_vertical_;
  layer->should_flatten_transform_from_property_tree_ =
      should_flatten_transform_from_property_tree_;
  layer->masks_to_bounds_ = masks_to_bounds_;
  layer->contents_opaque_ = contents_opaque_;
  layer->may_contain_video_ = may_contain_video_;
  layer->use_parent_backface_visibility_ = use_parent_backface_visibility_;
  layer->use_local_transform_for_backface_visibility_ =
      use_local_transform_for_backface_visibility_;
  layer->should_check_backface_visibility_ = should_check_backface_visibility_;
  layer->draws_content_ = draws_content_;
  layer->non_fast_scrollable_region_ = non_fast_scrollable_region_;
  layer->touch_event_handler_region_ = touch_event_handler_region_;
  layer->background_color_ = background_color_;
  layer->safe_opaque_background_color_ = safe_opaque_background_color_;
  layer->draw_blend_mode_ = draw_blend_mode_;
  layer->position_ = position_;
  layer->transform_tree_index_ = transform_tree_index_;
  layer->effect_tree_index_ = effect_tree_index_;
  layer->clip_tree_index_ = clip_tree_index_;
  layer->scroll_tree_index_ = scroll_tree_index_;
  layer->has_will_change_transform_hint_ = has_will_change_transform_hint_;
  layer->scrollbars_hidden_ = scrollbars_hidden_;

  if (layer_property_changed_) {
    layer->layer_tree_impl()->set_needs_update_draw_properties();
    layer->layer_property_changed_ = true;
  }

  layer->SetBounds(bounds_);
  layer->SetScrollClipLayer(scroll_clip_layer_id_);
  layer->SetElementId(element_id_);
  layer->SetMutableProperties(mutable_properties_);

  // If the main thread commits multiple times before the impl thread actually
  // draws, then damage tracking will become incorrect if we simply clobber the
  // update_rect here. The LayerImpl's update_rect needs to accumulate (i.e.
  // union) any update changes that have occurred on the main thread.
  update_rect_.Union(layer->update_rect());
  layer->SetUpdateRect(update_rect_);

  if (owned_debug_info_)
    layer->SetDebugInfo(std::move(owned_debug_info_));

  // Reset any state that should be cleared for the next update.
  layer_property_changed_ = false;
  needs_push_properties_ = false;
  update_rect_ = gfx::Rect();
  layer_tree_impl()->RemoveLayerShouldPushProperties(this);
}

bool LayerImpl::IsAffectedByPageScale() const {
  TransformTree& transform_tree = GetTransformTree();
  return transform_tree.Node(transform_tree_index())
      ->in_subtree_of_page_scale_layer;
}

gfx::Vector2dF LayerImpl::FixedContainerSizeDelta() const {
  LayerImpl* scroll_clip_layer =
      layer_tree_impl()->LayerById(scroll_clip_layer_id_);
  if (!scroll_clip_layer)
    return gfx::Vector2dF();

  return scroll_clip_layer->bounds_delta();
}

std::unique_ptr<base::DictionaryValue> LayerImpl::LayerTreeAsJson() {
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue);
  result->SetInteger("LayerId", id());
  result->SetString("LayerType", LayerTypeAsString());

  base::ListValue* list = new base::ListValue;
  list->AppendInteger(bounds().width());
  list->AppendInteger(bounds().height());
  result->Set("Bounds", list);

  list = new base::ListValue;
  list->AppendDouble(position_.x());
  list->AppendDouble(position_.y());
  result->Set("Position", list);

  const gfx::Transform& gfx_transform = test_properties()->transform;
  double transform[16];
  gfx_transform.matrix().asColMajord(transform);
  list = new base::ListValue;
  for (int i = 0; i < 16; ++i)
    list->AppendDouble(transform[i]);
  result->Set("Transform", list);

  result->SetBoolean("DrawsContent", draws_content_);
  result->SetBoolean("Is3dSorted", Is3dSorted());
  result->SetDouble("OPACITY", Opacity());
  result->SetBoolean("ContentsOpaque", contents_opaque_);

  if (scrollable())
    result->SetBoolean("Scrollable", true);

  if (!touch_event_handler_region_.IsEmpty()) {
    std::unique_ptr<base::Value> region = touch_event_handler_region_.AsValue();
    result->Set("TouchRegion", region.release());
  }

  list = new base::ListValue;
  for (size_t i = 0; i < test_properties()->children.size(); ++i)
    list->Append(test_properties()->children[i]->LayerTreeAsJson());
  result->Set("Children", list);

  return result;
}

bool LayerImpl::LayerPropertyChanged() const {
  if (layer_property_changed_ || GetPropertyTrees()->full_tree_damaged)
    return true;
  if (transform_tree_index() == TransformTree::kInvalidNodeId)
    return false;
  TransformNode* transform_node =
      GetTransformTree().Node(transform_tree_index());
  if (transform_node && transform_node->transform_changed)
    return true;
  if (effect_tree_index() == EffectTree::kInvalidNodeId)
    return false;
  EffectNode* effect_node = GetEffectTree().Node(effect_tree_index());
  if (effect_node && effect_node->effect_changed)
    return true;
  return false;
}

void LayerImpl::NoteLayerPropertyChanged() {
  layer_property_changed_ = true;
  layer_tree_impl()->set_needs_update_draw_properties();
  SetNeedsPushProperties();
}

void LayerImpl::ValidateQuadResourcesInternal(DrawQuad* quad) const {
#if DCHECK_IS_ON()
  const ResourceProvider* resource_provider =
      layer_tree_impl_->resource_provider();
  for (ResourceId resource_id : quad->resources)
    resource_provider->ValidateResource(resource_id);
#endif
}

const char* LayerImpl::LayerTypeAsString() const {
  return "cc::LayerImpl";
}

void LayerImpl::ResetChangeTracking() {
  layer_property_changed_ = false;
  needs_push_properties_ = false;

  update_rect_.SetRect(0, 0, 0, 0);
  damage_rect_.SetRect(0, 0, 0, 0);
}

int LayerImpl::num_copy_requests_in_target_subtree() {
  return GetEffectTree()
      .Node(effect_tree_index())
      ->num_copy_requests_in_subtree;
}

void LayerImpl::UpdatePropertyTreeTransformIsAnimated(bool is_animated) {
  PropertyTrees* property_trees = GetPropertyTrees();
  if (property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::TRANSFORM,
                                       id())) {
    TransformTree& transform_tree = GetTransformTree();
    TransformNode* node = transform_tree.Node(
        property_trees->layer_id_to_transform_node_index[id()]);
    // A LayerImpl's own current state is insufficient for determining whether
    // it owns a TransformNode, since this depends on the state of the
    // corresponding Layer at the time of the last commit. For example, if
    // |is_animated| is false, this might mean a transform animation just ticked
    // past its finish point (so the LayerImpl still owns a TransformNode) or it
    // might mean that a transform animation was removed during commit or
    // activation (and, in that case, the LayerImpl will no longer own a
    // TransformNode, unless it has non-animation-related reasons for owning a
    // node).
    if (node->has_potential_animation != is_animated) {
      node->has_potential_animation = is_animated;
      if (is_animated) {
        node->has_only_translation_animations = HasOnlyTranslationTransforms();
      } else {
        node->has_only_translation_animations = true;
      }

      transform_tree.set_needs_update(true);
      layer_tree_impl()->set_needs_update_draw_properties();
    }
  }
}

void LayerImpl::UpdatePropertyTreeForScrollingAndAnimationIfNeeded() {
  if (scrollable())
    UpdatePropertyTreeScrollOffset();

  if (HasAnyAnimationTargetingProperty(TargetProperty::TRANSFORM)) {
    UpdatePropertyTreeTransformIsAnimated(
        HasPotentiallyRunningTransformAnimation());
  }
}

gfx::ScrollOffset LayerImpl::ScrollOffsetForAnimation() const {
  return CurrentScrollOffset();
}

void LayerImpl::OnIsAnimatingChanged(const PropertyAnimationState& mask,
                                     const PropertyAnimationState& state) {
  DCHECK(layer_tree_impl_);
  PropertyTrees* property_trees = GetPropertyTrees();

  TransformNode* transform_node = nullptr;
  if (property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::TRANSFORM,
                                       id())) {
    transform_node = GetTransformTree().Node(
        property_trees->layer_id_to_transform_node_index[id()]);
  }

  EffectNode* effect_node = nullptr;
  if (property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::EFFECT, id())) {
    effect_node = GetEffectTree().Node(
        property_trees->layer_id_to_effect_node_index[id()]);
  }

  for (int property = TargetProperty::FIRST_TARGET_PROPERTY;
       property <= TargetProperty::LAST_TARGET_PROPERTY; ++property) {
    switch (property) {
      case TargetProperty::TRANSFORM:
        if (transform_node) {
          if (mask.currently_running[property])
            transform_node->is_currently_animating =
                state.currently_running[property];
          if (mask.potentially_animating[property]) {
            UpdatePropertyTreeTransformIsAnimated(
                state.potentially_animating[property]);
            was_ever_ready_since_last_transform_animation_ = false;
          }
        }
        break;
      case TargetProperty::OPACITY:
        if (effect_node) {
          if (mask.currently_running[property])
            effect_node->is_currently_animating_opacity =
                state.currently_running[property];
          if (mask.potentially_animating[property]) {
            effect_node->has_potential_opacity_animation =
                state.potentially_animating[property];
            GetEffectTree().set_needs_update(true);
          }
        }
        break;
      case TargetProperty::FILTER:
        if (effect_node) {
          if (mask.currently_running[property])
            effect_node->is_currently_animating_filter =
                state.currently_running[property];
          if (mask.potentially_animating[property])
            effect_node->has_potential_filter_animation =
                state.potentially_animating[property];
        }
        break;
      default:
        break;
    }
  }
}

bool LayerImpl::IsActive() const {
  return layer_tree_impl_->IsActiveTree();
}

gfx::Size LayerImpl::bounds() const {
  gfx::Vector2d delta = gfx::ToCeiledVector2d(bounds_delta_);
  return gfx::Size(bounds_.width() + delta.x(),
                   bounds_.height() + delta.y());
}

gfx::SizeF LayerImpl::BoundsForScrolling() const {
  return gfx::SizeF(bounds_.width() + bounds_delta_.x(),
                    bounds_.height() + bounds_delta_.y());
}

void LayerImpl::SetBounds(const gfx::Size& bounds) {
  if (bounds_ == bounds)
    return;

  bounds_ = bounds;

  layer_tree_impl()->DidUpdateScrollState(id());

  NoteLayerPropertyChanged();
}

void LayerImpl::SetBoundsDelta(const gfx::Vector2dF& bounds_delta) {
  DCHECK(IsActive());
  if (bounds_delta_ == bounds_delta)
    return;

  bounds_delta_ = bounds_delta;

  PropertyTrees* property_trees = GetPropertyTrees();
  if (this == layer_tree_impl()->InnerViewportContainerLayer())
    property_trees->SetInnerViewportContainerBoundsDelta(bounds_delta);
  else if (this == layer_tree_impl()->OuterViewportContainerLayer())
    property_trees->SetOuterViewportContainerBoundsDelta(bounds_delta);
  else if (this == layer_tree_impl()->InnerViewportScrollLayer())
    property_trees->SetInnerViewportScrollBoundsDelta(bounds_delta);

  layer_tree_impl()->DidUpdateScrollState(id());

  if (masks_to_bounds()) {
    // If layer is clipping, then update the clip node using the new bounds.
    ClipNode* clip_node = property_trees->clip_tree.Node(clip_tree_index());
    if (clip_node) {
      DCHECK(property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::CLIP,
                                              id()));
      clip_node->clip = gfx::RectF(gfx::PointF() + offset_to_transform_parent(),
                                   gfx::SizeF(bounds()));
      property_trees->clip_tree.set_needs_update(true);
    }
    property_trees->full_tree_damaged = true;
    layer_tree_impl()->set_needs_update_draw_properties();
  } else {
    NoteLayerPropertyChanged();
  }
}

ScrollbarLayerImplBase* LayerImpl::ToScrollbarLayer() {
  return nullptr;
}

void LayerImpl::SetDrawsContent(bool draws_content) {
  if (draws_content_ == draws_content)
    return;

  draws_content_ = draws_content;
  NoteLayerPropertyChanged();
}

void LayerImpl::SetBackgroundColor(SkColor background_color) {
  if (background_color_ == background_color)
    return;

  background_color_ = background_color;
  NoteLayerPropertyChanged();
}

void LayerImpl::SetSafeOpaqueBackgroundColor(SkColor background_color) {
  safe_opaque_background_color_ = background_color;
}

SkColor LayerImpl::SafeOpaqueBackgroundColor() const {
  if (contents_opaque())
    return safe_opaque_background_color_;
  SkColor color = background_color();
  if (SkColorGetA(color) == 255)
    color = SK_ColorTRANSPARENT;
  return color;
}

bool LayerImpl::FilterIsAnimating() const {
  return GetMutatorHost()->IsAnimatingFilterProperty(
      element_id(), GetElementTypeForAnimation());
}

bool LayerImpl::HasPotentiallyRunningFilterAnimation() const {
  return GetMutatorHost()->HasPotentiallyRunningFilterAnimation(
      element_id(), GetElementTypeForAnimation());
}

void LayerImpl::SetMasksToBounds(bool masks_to_bounds) {
  masks_to_bounds_ = masks_to_bounds;
}

void LayerImpl::SetContentsOpaque(bool opaque) {
  contents_opaque_ = opaque;
}

float LayerImpl::Opacity() const {
  PropertyTrees* property_trees = GetPropertyTrees();
  if (!property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::EFFECT, id()))
    return 1.f;
  EffectNode* node =
      GetEffectTree().Node(property_trees->layer_id_to_effect_node_index[id()]);
  return node->opacity;
}

const gfx::Transform& LayerImpl::Transform() const {
  PropertyTrees* property_trees = GetPropertyTrees();
  DCHECK(property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::TRANSFORM,
                                          id()));
  TransformNode* node = GetTransformTree().Node(
      property_trees->layer_id_to_transform_node_index[id()]);
  return node->local;
}

void LayerImpl::SetElementId(ElementId element_id) {
  if (element_id == element_id_)
    return;

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("compositor-worker"),
               "LayerImpl::SetElementId", "element",
               element_id.AsValue().release());

  layer_tree_impl_->RemoveFromElementMap(this);
  element_id_ = element_id;
  layer_tree_impl_->AddToElementMap(this);

  SetNeedsPushProperties();
}

void LayerImpl::SetMutableProperties(uint32_t properties) {
  if (mutable_properties_ == properties)
    return;

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("compositor-worker"),
               "LayerImpl::SetMutableProperties", "properties", properties);

  mutable_properties_ = properties;
  // If this layer is already in the element map, update its properties.
  layer_tree_impl_->AddToElementMap(this);
}

void LayerImpl::SetPosition(const gfx::PointF& position) {
  position_ = position;
}

bool LayerImpl::TransformIsAnimating() const {
  return GetMutatorHost()->IsAnimatingTransformProperty(
      element_id(), GetElementTypeForAnimation());
}

bool LayerImpl::HasPotentiallyRunningTransformAnimation() const {
  return GetMutatorHost()->HasPotentiallyRunningTransformAnimation(
      element_id(), GetElementTypeForAnimation());
}

bool LayerImpl::HasOnlyTranslationTransforms() const {
  return GetMutatorHost()->HasOnlyTranslationTransforms(
      element_id(), GetElementTypeForAnimation());
}

bool LayerImpl::HasAnyAnimationTargetingProperty(
    TargetProperty::Type property) const {
  return GetMutatorHost()->HasAnyAnimationTargetingProperty(element_id(),
                                                            property);
}

bool LayerImpl::HasFilterAnimationThatInflatesBounds() const {
  return GetMutatorHost()->HasFilterAnimationThatInflatesBounds(element_id());
}

bool LayerImpl::HasTransformAnimationThatInflatesBounds() const {
  return GetMutatorHost()->HasTransformAnimationThatInflatesBounds(
      element_id());
}

bool LayerImpl::HasAnimationThatInflatesBounds() const {
  return GetMutatorHost()->HasAnimationThatInflatesBounds(element_id());
}

bool LayerImpl::FilterAnimationBoundsForBox(const gfx::BoxF& box,
                                            gfx::BoxF* bounds) const {
  return GetMutatorHost()->FilterAnimationBoundsForBox(element_id(), box,
                                                       bounds);
}

bool LayerImpl::TransformAnimationBoundsForBox(const gfx::BoxF& box,
                                               gfx::BoxF* bounds) const {
  return GetMutatorHost()->TransformAnimationBoundsForBox(element_id(), box,
                                                          bounds);
}

void LayerImpl::SetUpdateRect(const gfx::Rect& update_rect) {
  update_rect_ = update_rect;
  SetNeedsPushProperties();
}

void LayerImpl::AddDamageRect(const gfx::Rect& damage_rect) {
  damage_rect_.Union(damage_rect);
}

void LayerImpl::SetCurrentScrollOffset(const gfx::ScrollOffset& scroll_offset) {
  DCHECK(IsActive());
  if (GetScrollTree().SetScrollOffset(id(), scroll_offset))
    layer_tree_impl()->DidUpdateScrollOffset(id());
}

gfx::ScrollOffset LayerImpl::CurrentScrollOffset() const {
  return GetScrollTree().current_scroll_offset(id());
}

void LayerImpl::UpdatePropertyTreeScrollOffset() {
  // TODO(enne): in the future, scrolling should update the scroll tree
  // directly instead of going through layers.
  TransformTree& transform_tree = GetTransformTree();
  TransformNode* node = transform_tree.Node(transform_tree_index_);
  gfx::ScrollOffset current_offset = CurrentScrollOffset();
  if (node->scroll_offset != current_offset) {
    node->scroll_offset = current_offset;
    node->needs_local_transform_update = true;
    transform_tree.set_needs_update(true);
  }
}

SimpleEnclosedRegion LayerImpl::VisibleOpaqueRegion() const {
  if (contents_opaque())
    return SimpleEnclosedRegion(visible_layer_rect());
  return SimpleEnclosedRegion();
}

void LayerImpl::DidBeginTracing() {}

void LayerImpl::ReleaseResources() {}

void LayerImpl::ReleaseTileResources() {}

void LayerImpl::RecreateTileResources() {}

gfx::ScrollOffset LayerImpl::MaxScrollOffset() const {
  return GetScrollTree().MaxScrollOffset(scroll_tree_index());
}

gfx::ScrollOffset LayerImpl::ClampScrollOffsetToLimits(
    gfx::ScrollOffset offset) const {
  offset.SetToMin(MaxScrollOffset());
  offset.SetToMax(gfx::ScrollOffset());
  return offset;
}

gfx::Vector2dF LayerImpl::ClampScrollToMaxScrollOffset() {
  gfx::ScrollOffset old_offset = CurrentScrollOffset();
  gfx::ScrollOffset clamped_offset = ClampScrollOffsetToLimits(old_offset);
  gfx::Vector2dF delta = clamped_offset.DeltaFrom(old_offset);
  if (!delta.IsZero())
    ScrollBy(delta);
  return delta;
}

void LayerImpl::SetNeedsPushProperties() {
  if (layer_tree_impl_ && !needs_push_properties_) {
    needs_push_properties_ = true;
    layer_tree_impl()->AddLayerShouldPushProperties(this);
  }
}

void LayerImpl::GetAllPrioritizedTilesForTracing(
    std::vector<PrioritizedTile>* prioritized_tiles) const {
}

void LayerImpl::AsValueInto(base::trace_event::TracedValue* state) const {
  TracedValue::MakeDictIntoImplicitSnapshotWithCategory(
      TRACE_DISABLED_BY_DEFAULT("cc.debug"),
      state,
      "cc::LayerImpl",
      LayerTypeAsString(),
      this);
  state->SetInteger("layer_id", id());
  MathUtil::AddToTracedValue("bounds", bounds_, state);

  state->SetDouble("opacity", Opacity());

  MathUtil::AddToTracedValue("position", position_, state);

  state->SetInteger("draws_content", DrawsContent());
  state->SetInteger("gpu_memory_usage",
                    base::saturated_cast<int>(GPUMemoryUsageInBytes()));

  if (element_id_)
    element_id_.AddToTracedValue(state);

  if (mutable_properties_ != MutableProperty::kNone)
    state->SetInteger("mutable_properties", mutable_properties_);

  MathUtil::AddToTracedValue("scroll_offset", CurrentScrollOffset(), state);

  if (!ScreenSpaceTransform().IsIdentity())
    MathUtil::AddToTracedValue("screen_space_transform", ScreenSpaceTransform(),
                               state);

  bool clipped;
  gfx::QuadF layer_quad =
      MathUtil::MapQuad(ScreenSpaceTransform(),
                        gfx::QuadF(gfx::RectF(gfx::Rect(bounds()))), &clipped);
  MathUtil::AddToTracedValue("layer_quad", layer_quad, state);
  if (!touch_event_handler_region_.IsEmpty()) {
    state->BeginArray("touch_event_handler_region");
    touch_event_handler_region_.AsValueInto(state);
    state->EndArray();
  }
  if (!non_fast_scrollable_region_.IsEmpty()) {
    state->BeginArray("non_fast_scrollable_region");
    non_fast_scrollable_region_.AsValueInto(state);
    state->EndArray();
  }

  state->SetBoolean("can_use_lcd_text", CanUseLCDText());
  state->SetBoolean("contents_opaque", contents_opaque());

  state->SetBoolean("has_animation_bounds", HasAnimationThatInflatesBounds());

  state->SetBoolean("has_will_change_transform_hint",
                    has_will_change_transform_hint());

  gfx::BoxF box;
  if (LayerUtils::GetAnimationBounds(*this, &box))
    MathUtil::AddToTracedValue("animation_bounds", box, state);

  if (debug_info_) {
    std::string str;
    debug_info_->AppendAsTraceFormat(&str);
    base::JSONReader json_reader;
    std::unique_ptr<base::Value> debug_info_value(json_reader.ReadToValue(str));

    if (debug_info_value->IsType(base::Value::Type::DICTIONARY)) {
      base::DictionaryValue* dictionary_value = nullptr;
      bool converted_to_dictionary =
          debug_info_value->GetAsDictionary(&dictionary_value);
      DCHECK(converted_to_dictionary);
      for (base::DictionaryValue::Iterator it(*dictionary_value); !it.IsAtEnd();
           it.Advance()) {
        state->SetValue(it.key().data(), it.value().CreateDeepCopy());
      }
    } else {
      NOTREACHED();
    }
  }
}

size_t LayerImpl::GPUMemoryUsageInBytes() const { return 0; }

void LayerImpl::RunMicroBenchmark(MicroBenchmarkImpl* benchmark) {
  benchmark->RunOnLayer(this);
}

gfx::Transform LayerImpl::DrawTransform() const {
  // Only drawn layers have up-to-date draw properties.
  if (!is_drawn_render_surface_layer_list_member()) {
    if (GetPropertyTrees()->non_root_surfaces_enabled) {
      return draw_property_utils::DrawTransform(this, GetTransformTree(),
                                                GetEffectTree());
    } else {
      return draw_property_utils::ScreenSpaceTransform(this,
                                                       GetTransformTree());
    }
  }

  return draw_properties().target_space_transform;
}

gfx::Transform LayerImpl::ScreenSpaceTransform() const {
  // Only drawn layers have up-to-date draw properties.
  if (!is_drawn_render_surface_layer_list_member()) {
    return draw_property_utils::ScreenSpaceTransform(this, GetTransformTree());
  }

  return draw_properties().screen_space_transform;
}

bool LayerImpl::CanUseLCDText() const {
  if (layer_tree_impl()->settings().layers_always_allowed_lcd_text)
    return true;
  if (!layer_tree_impl()->settings().can_use_lcd_text)
    return false;
  if (!contents_opaque())
    return false;

  if (GetEffectTree().Node(effect_tree_index())->screen_space_opacity != 1.f)
    return false;
  if (!GetTransformTree()
           .Node(transform_tree_index())
           ->node_and_ancestors_have_only_integer_translation)
    return false;
  if (static_cast<int>(offset_to_transform_parent().x()) !=
      offset_to_transform_parent().x())
    return false;
  if (static_cast<int>(offset_to_transform_parent().y()) !=
      offset_to_transform_parent().y())
    return false;
  return true;
}

int LayerImpl::GetSortingContextId() const {
  return GetTransformTree().Node(transform_tree_index())->sorting_context_id;
}

Region LayerImpl::GetInvalidationRegionForDebugging() {
  return Region(update_rect_);
}

gfx::Rect LayerImpl::GetEnclosingRectInTargetSpace() const {
  return MathUtil::MapEnclosingClippedRect(DrawTransform(),
                                           gfx::Rect(bounds()));
}

gfx::Rect LayerImpl::GetScaledEnclosingRectInTargetSpace(float scale) const {
  gfx::Transform scaled_draw_transform = DrawTransform();
  scaled_draw_transform.Scale(SK_MScalar1 / scale, SK_MScalar1 / scale);
  gfx::Size scaled_bounds = gfx::ScaleToCeiledSize(bounds(), scale);
  return MathUtil::MapEnclosingClippedRect(scaled_draw_transform,
                                           gfx::Rect(scaled_bounds));
}

RenderSurfaceImpl* LayerImpl::GetRenderSurface() const {
  EffectNode* effect_node = GetEffectTree().Node(effect_tree_index_);
  if (effect_node->owning_layer_id == id())
    return GetEffectTree().GetRenderSurface(effect_tree_index_);
  return nullptr;
}

RenderSurfaceImpl* LayerImpl::render_target() {
  return GetEffectTree().GetRenderSurface(render_target_effect_tree_index());
}

const RenderSurfaceImpl* LayerImpl::render_target() const {
  return GetEffectTree().GetRenderSurface(render_target_effect_tree_index());
}

bool LayerImpl::IsHidden() const {
  EffectNode* node = GetEffectTree().Node(effect_tree_index_);
  return node->screen_space_opacity == 0.f;
}

float LayerImpl::GetIdealContentsScale() const {
  float page_scale = IsAffectedByPageScale()
                         ? layer_tree_impl()->current_page_scale_factor()
                         : 1.f;
  float device_scale = layer_tree_impl()->device_scale_factor();

  float default_scale = page_scale * device_scale;
  if (!layer_tree_impl()
           ->settings()
           .layer_transforms_should_scale_layer_contents) {
    return default_scale;
  }

  gfx::Vector2dF transform_scales = MathUtil::ComputeTransform2dScaleComponents(
      ScreenSpaceTransform(), default_scale);
  return std::max(transform_scales.x(), transform_scales.y());
}

PropertyTrees* LayerImpl::GetPropertyTrees() const {
  return layer_tree_impl_->property_trees();
}

EffectTree& LayerImpl::GetEffectTree() const {
  return GetPropertyTrees()->effect_tree;
}

ScrollTree& LayerImpl::GetScrollTree() const {
  return GetPropertyTrees()->scroll_tree;
}

TransformTree& LayerImpl::GetTransformTree() const {
  return GetPropertyTrees()->transform_tree;
}

}  // namespace cc
