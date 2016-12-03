// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree.h"

#include "base/auto_reset.h"
#include "base/time/time.h"
#include "cc/animation/animation_host.h"
#include "cc/input/page_scale_animation.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_proto_converter.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/proto/layer_tree.pb.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

namespace {

Layer* UpdateAndGetLayer(Layer* current_layer,
                         int layer_id,
                         LayerTree* layer_tree) {
  if (layer_id == Layer::INVALID_ID) {
    if (current_layer)
      current_layer->SetLayerTreeHost(nullptr);

    return nullptr;
  }
  Layer* layer = layer_tree->LayerById(layer_id);
  DCHECK(layer);
  if (current_layer && current_layer != layer)
    current_layer->SetLayerTreeHost(nullptr);

  return layer;
}

}  // namespace

LayerTree::Inputs::Inputs()
    : top_controls_height(0.f),
      top_controls_shown_ratio(0.f),
      top_controls_shrink_blink_size(false),
      bottom_controls_height(0.f),
      device_scale_factor(1.f),
      painted_device_scale_factor(1.f),
      page_scale_factor(1.f),
      min_page_scale_factor(1.f),
      max_page_scale_factor(1.f),
      background_color(SK_ColorWHITE),
      has_transparent_background(false),
      have_scroll_event_handlers(false),
      event_listener_properties() {}

LayerTree::Inputs::~Inputs() = default;

LayerTree::LayerTree(std::unique_ptr<AnimationHost> animation_host,
                     LayerTreeHost* layer_tree_host)
    : needs_full_tree_sync_(true),
      needs_meta_info_recomputation_(true),
      in_paint_layer_contents_(false),
      animation_host_(std::move(animation_host)),
      layer_tree_host_(layer_tree_host) {
  DCHECK(animation_host_);
  DCHECK(layer_tree_host_);
  animation_host_->SetMutatorHostClient(this);
}

LayerTree::~LayerTree() {
  animation_host_->SetMutatorHostClient(nullptr);

  // We must clear any pointers into the layer tree prior to destroying it.
  RegisterViewportLayers(nullptr, nullptr, nullptr, nullptr);

  if (inputs_.root_layer) {
    inputs_.root_layer->SetLayerTreeHost(nullptr);

    // The root layer must be destroyed before the layer tree. We've made a
    // contract with our animation controllers that the animation_host will
    // outlive them, and we must make good.
    inputs_.root_layer = nullptr;
  }
}

void LayerTree::SetRootLayer(scoped_refptr<Layer> root_layer) {
  if (inputs_.root_layer.get() == root_layer.get())
    return;

  if (inputs_.root_layer.get())
    inputs_.root_layer->SetLayerTreeHost(nullptr);
  inputs_.root_layer = root_layer;
  if (inputs_.root_layer.get()) {
    DCHECK(!inputs_.root_layer->parent());
    inputs_.root_layer->SetLayerTreeHost(layer_tree_host_);
  }

  if (hud_layer_.get())
    hud_layer_->RemoveFromParent();

  // Reset gpu rasterization tracking.
  // This flag is sticky until a new tree comes along.
  layer_tree_host_->ResetGpuRasterizationTracking();

  SetNeedsFullTreeSync();
}

void LayerTree::RegisterViewportLayers(
    scoped_refptr<Layer> overscroll_elasticity_layer,
    scoped_refptr<Layer> page_scale_layer,
    scoped_refptr<Layer> inner_viewport_scroll_layer,
    scoped_refptr<Layer> outer_viewport_scroll_layer) {
  DCHECK(!inner_viewport_scroll_layer ||
         inner_viewport_scroll_layer != outer_viewport_scroll_layer);
  inputs_.overscroll_elasticity_layer = overscroll_elasticity_layer;
  inputs_.page_scale_layer = page_scale_layer;
  inputs_.inner_viewport_scroll_layer = inner_viewport_scroll_layer;
  inputs_.outer_viewport_scroll_layer = outer_viewport_scroll_layer;
}

void LayerTree::RegisterSelection(const LayerSelection& selection) {
  if (inputs_.selection == selection)
    return;

  inputs_.selection = selection;
  SetNeedsCommit();
}

void LayerTree::SetHaveScrollEventHandlers(bool have_event_handlers) {
  if (inputs_.have_scroll_event_handlers == have_event_handlers)
    return;

  inputs_.have_scroll_event_handlers = have_event_handlers;
  SetNeedsCommit();
}

void LayerTree::SetEventListenerProperties(EventListenerClass event_class,
                                           EventListenerProperties properties) {
  const size_t index = static_cast<size_t>(event_class);
  if (inputs_.event_listener_properties[index] == properties)
    return;

  inputs_.event_listener_properties[index] = properties;
  SetNeedsCommit();
}

void LayerTree::SetViewportSize(const gfx::Size& device_viewport_size) {
  if (inputs_.device_viewport_size == device_viewport_size)
    return;

  inputs_.device_viewport_size = device_viewport_size;

  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void LayerTree::SetTopControlsHeight(float height, bool shrink) {
  if (inputs_.top_controls_height == height &&
      inputs_.top_controls_shrink_blink_size == shrink)
    return;

  inputs_.top_controls_height = height;
  inputs_.top_controls_shrink_blink_size = shrink;
  SetNeedsCommit();
}

void LayerTree::SetTopControlsShownRatio(float ratio) {
  if (inputs_.top_controls_shown_ratio == ratio)
    return;

  inputs_.top_controls_shown_ratio = ratio;
  SetNeedsCommit();
}

void LayerTree::SetBottomControlsHeight(float height) {
  if (inputs_.bottom_controls_height == height)
    return;

  inputs_.bottom_controls_height = height;
  SetNeedsCommit();
}

void LayerTree::SetPageScaleFactorAndLimits(float page_scale_factor,
                                            float min_page_scale_factor,
                                            float max_page_scale_factor) {
  if (inputs_.page_scale_factor == page_scale_factor &&
      inputs_.min_page_scale_factor == min_page_scale_factor &&
      inputs_.max_page_scale_factor == max_page_scale_factor)
    return;

  inputs_.page_scale_factor = page_scale_factor;
  inputs_.min_page_scale_factor = min_page_scale_factor;
  inputs_.max_page_scale_factor = max_page_scale_factor;
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void LayerTree::StartPageScaleAnimation(const gfx::Vector2d& target_offset,
                                        bool use_anchor,
                                        float scale,
                                        base::TimeDelta duration) {
  inputs_.pending_page_scale_animation.reset(new PendingPageScaleAnimation(
      target_offset, use_anchor, scale, duration));

  SetNeedsCommit();
}

bool LayerTree::HasPendingPageScaleAnimation() const {
  return !!inputs_.pending_page_scale_animation.get();
}

void LayerTree::SetDeviceScaleFactor(float device_scale_factor) {
  if (inputs_.device_scale_factor == device_scale_factor)
    return;
  inputs_.device_scale_factor = device_scale_factor;

  property_trees_.needs_rebuild = true;
  SetNeedsCommit();
}

void LayerTree::SetPaintedDeviceScaleFactor(float painted_device_scale_factor) {
  if (inputs_.painted_device_scale_factor == painted_device_scale_factor)
    return;
  inputs_.painted_device_scale_factor = painted_device_scale_factor;

  SetNeedsCommit();
}

void LayerTree::RegisterLayer(Layer* layer) {
  DCHECK(!LayerById(layer->id()));
  DCHECK(!in_paint_layer_contents_);
  layer_id_map_[layer->id()] = layer;
  if (layer->element_id()) {
    animation_host_->RegisterElement(layer->element_id(),
                                     ElementListType::ACTIVE);
  }
}

void LayerTree::UnregisterLayer(Layer* layer) {
  DCHECK(LayerById(layer->id()));
  DCHECK(!in_paint_layer_contents_);
  if (layer->element_id()) {
    animation_host_->UnregisterElement(layer->element_id(),
                                       ElementListType::ACTIVE);
  }
  RemoveLayerShouldPushProperties(layer);
  layer_id_map_.erase(layer->id());
}

Layer* LayerTree::LayerById(int id) const {
  LayerIdMap::const_iterator iter = layer_id_map_.find(id);
  return iter != layer_id_map_.end() ? iter->second : nullptr;
}

bool LayerTree::UpdateLayers(const LayerList& update_layer_list,
                             bool* content_is_suitable_for_gpu) {
  base::AutoReset<bool> painting(&in_paint_layer_contents_, true);
  bool did_paint_content = false;
  for (const auto& layer : update_layer_list) {
    did_paint_content |= layer->Update();
    *content_is_suitable_for_gpu &= layer->IsSuitableForGpuRasterization();
  }
  return did_paint_content;
}

void LayerTree::AddLayerShouldPushProperties(Layer* layer) {
  layers_that_should_push_properties_.insert(layer);
}

void LayerTree::RemoveLayerShouldPushProperties(Layer* layer) {
  layers_that_should_push_properties_.erase(layer);
}

std::unordered_set<Layer*>& LayerTree::LayersThatShouldPushProperties() {
  return layers_that_should_push_properties_;
}

bool LayerTree::LayerNeedsPushPropertiesForTesting(Layer* layer) const {
  return layers_that_should_push_properties_.find(layer) !=
         layers_that_should_push_properties_.end();
}

void LayerTree::SetNeedsMetaInfoRecomputation(bool needs_recomputation) {
  needs_meta_info_recomputation_ = needs_recomputation;
}

void LayerTree::SetPageScaleFromImplSide(float page_scale) {
  DCHECK(layer_tree_host_->CommitRequested());
  inputs_.page_scale_factor = page_scale;
  SetPropertyTreesNeedRebuild();
}

void LayerTree::SetElasticOverscrollFromImplSide(
    gfx::Vector2dF elastic_overscroll) {
  DCHECK(layer_tree_host_->CommitRequested());
  elastic_overscroll_ = elastic_overscroll;
}

void LayerTree::UpdateHudLayer(bool show_hud_info) {
  if (show_hud_info) {
    if (!hud_layer_.get()) {
      hud_layer_ = HeadsUpDisplayLayer::Create();
    }

    if (inputs_.root_layer.get() && !hud_layer_->parent())
      inputs_.root_layer->AddChild(hud_layer_);
  } else if (hud_layer_.get()) {
    hud_layer_->RemoveFromParent();
    hud_layer_ = nullptr;
  }
}

void LayerTree::SetNeedsFullTreeSync() {
  needs_full_tree_sync_ = true;
  needs_meta_info_recomputation_ = true;

  property_trees_.needs_rebuild = true;
  SetNeedsCommit();
}

void LayerTree::SetNeedsCommit() {
  layer_tree_host_->SetNeedsCommit();
}

void LayerTree::SetPropertyTreesNeedRebuild() {
  property_trees_.needs_rebuild = true;
  layer_tree_host_->SetNeedsUpdateLayers();
}

void LayerTree::PushPropertiesTo(LayerTreeImpl* tree_impl) {
  tree_impl->set_needs_full_tree_sync(needs_full_tree_sync_);
  needs_full_tree_sync_ = false;

  if (hud_layer_.get()) {
    LayerImpl* hud_impl = tree_impl->LayerById(hud_layer_->id());
    tree_impl->set_hud_layer(static_cast<HeadsUpDisplayLayerImpl*>(hud_impl));
  } else {
    tree_impl->set_hud_layer(nullptr);
  }

  tree_impl->set_background_color(inputs_.background_color);
  tree_impl->set_has_transparent_background(inputs_.has_transparent_background);
  tree_impl->set_have_scroll_event_handlers(inputs_.have_scroll_event_handlers);
  tree_impl->set_event_listener_properties(
      EventListenerClass::kTouchStartOrMove,
      event_listener_properties(EventListenerClass::kTouchStartOrMove));
  tree_impl->set_event_listener_properties(
      EventListenerClass::kMouseWheel,
      event_listener_properties(EventListenerClass::kMouseWheel));
  tree_impl->set_event_listener_properties(
      EventListenerClass::kTouchEndOrCancel,
      event_listener_properties(EventListenerClass::kTouchEndOrCancel));

  if (inputs_.page_scale_layer && inputs_.inner_viewport_scroll_layer) {
    tree_impl->SetViewportLayersFromIds(
        inputs_.overscroll_elasticity_layer
            ? inputs_.overscroll_elasticity_layer->id()
            : Layer::INVALID_ID,
        inputs_.page_scale_layer->id(),
        inputs_.inner_viewport_scroll_layer->id(),
        inputs_.outer_viewport_scroll_layer
            ? inputs_.outer_viewport_scroll_layer->id()
            : Layer::INVALID_ID);
    DCHECK(inputs_.inner_viewport_scroll_layer
               ->IsContainerForFixedPositionLayers());
  } else {
    tree_impl->ClearViewportLayers();
  }

  tree_impl->RegisterSelection(inputs_.selection);

  bool property_trees_changed_on_active_tree =
      tree_impl->IsActiveTree() && tree_impl->property_trees()->changed;
  // Property trees may store damage status. We preserve the sync tree damage
  // status by pushing the damage status from sync tree property trees to main
  // thread property trees or by moving it onto the layers.
  if (inputs_.root_layer && property_trees_changed_on_active_tree) {
    if (property_trees_.sequence_number ==
        tree_impl->property_trees()->sequence_number)
      tree_impl->property_trees()->PushChangeTrackingTo(&property_trees_);
    else
      tree_impl->MoveChangeTrackingToLayers();
  }
  // Setting property trees must happen before pushing the page scale.
  tree_impl->SetPropertyTrees(&property_trees_);

  tree_impl->PushPageScaleFromMainThread(inputs_.page_scale_factor,
                                         inputs_.min_page_scale_factor,
                                         inputs_.max_page_scale_factor);

  tree_impl->set_top_controls_shrink_blink_size(
      inputs_.top_controls_shrink_blink_size);
  tree_impl->set_top_controls_height(inputs_.top_controls_height);
  tree_impl->set_bottom_controls_height(inputs_.bottom_controls_height);
  tree_impl->PushTopControlsFromMainThread(inputs_.top_controls_shown_ratio);
  tree_impl->elastic_overscroll()->PushFromMainThread(elastic_overscroll_);
  if (tree_impl->IsActiveTree())
    tree_impl->elastic_overscroll()->PushPendingToActive();

  tree_impl->set_painted_device_scale_factor(
      inputs_.painted_device_scale_factor);

  if (inputs_.pending_page_scale_animation) {
    tree_impl->SetPendingPageScaleAnimation(
        std::move(inputs_.pending_page_scale_animation));
  }

  DCHECK(!tree_impl->ViewportSizeInvalid());

  tree_impl->set_has_ever_been_drawn(false);
}

void LayerTree::ToProtobuf(proto::LayerTree* proto) {
  LayerProtoConverter::SerializeLayerHierarchy(inputs_.root_layer,
                                               proto->mutable_root_layer());

  for (auto* layer : layers_that_should_push_properties_) {
    proto->add_layers_that_should_push_properties(layer->id());
  }
  proto->set_in_paint_layer_contents(in_paint_layer_contents());

  proto->set_needs_full_tree_sync(needs_full_tree_sync_);
  proto->set_needs_meta_info_recomputation(needs_meta_info_recomputation_);
  proto->set_hud_layer_id(hud_layer_ ? hud_layer_->id() : Layer::INVALID_ID);

  // Viewport layers.
  proto->set_overscroll_elasticity_layer_id(
      inputs_.overscroll_elasticity_layer
          ? inputs_.overscroll_elasticity_layer->id()
          : Layer::INVALID_ID);
  proto->set_page_scale_layer_id(inputs_.page_scale_layer
                                     ? inputs_.page_scale_layer->id()
                                     : Layer::INVALID_ID);
  proto->set_inner_viewport_scroll_layer_id(
      inputs_.inner_viewport_scroll_layer
          ? inputs_.inner_viewport_scroll_layer->id()
          : Layer::INVALID_ID);
  proto->set_outer_viewport_scroll_layer_id(
      inputs_.outer_viewport_scroll_layer
          ? inputs_.outer_viewport_scroll_layer->id()
          : Layer::INVALID_ID);

  SizeToProto(inputs_.device_viewport_size,
              proto->mutable_device_viewport_size());
  proto->set_top_controls_shrink_blink_size(
      inputs_.top_controls_shrink_blink_size);
  proto->set_top_controls_height(inputs_.top_controls_height);
  proto->set_top_controls_shown_ratio(inputs_.top_controls_shown_ratio);
  proto->set_device_scale_factor(inputs_.device_scale_factor);
  proto->set_painted_device_scale_factor(inputs_.painted_device_scale_factor);
  proto->set_page_scale_factor(inputs_.page_scale_factor);
  proto->set_min_page_scale_factor(inputs_.min_page_scale_factor);
  proto->set_max_page_scale_factor(inputs_.max_page_scale_factor);

  proto->set_background_color(inputs_.background_color);
  proto->set_has_transparent_background(inputs_.has_transparent_background);
  proto->set_have_scroll_event_handlers(inputs_.have_scroll_event_handlers);
  proto->set_wheel_event_listener_properties(static_cast<uint32_t>(
      event_listener_properties(EventListenerClass::kMouseWheel)));
  proto->set_touch_start_or_move_event_listener_properties(
      static_cast<uint32_t>(
          event_listener_properties(EventListenerClass::kTouchStartOrMove)));
  proto->set_touch_end_or_cancel_event_listener_properties(
      static_cast<uint32_t>(
          event_listener_properties(EventListenerClass::kTouchEndOrCancel)));

  LayerSelectionToProtobuf(inputs_.selection, proto->mutable_selection());
  property_trees_.ToProtobuf(proto->mutable_property_trees());
  Vector2dFToProto(elastic_overscroll_, proto->mutable_elastic_overscroll());
}

void LayerTree::FromProtobuf(const proto::LayerTree& proto) {
  // Layer hierarchy.
  scoped_refptr<Layer> new_root_layer =
      LayerProtoConverter::DeserializeLayerHierarchy(
          inputs_.root_layer, proto.root_layer(), layer_tree_host_);
  if (inputs_.root_layer != new_root_layer) {
    inputs_.root_layer = new_root_layer;
  }

  for (auto layer_id : proto.layers_that_should_push_properties()) {
    AddLayerShouldPushProperties(layer_id_map_[layer_id]);
  }
  in_paint_layer_contents_ = proto.in_paint_layer_contents();

  needs_full_tree_sync_ = proto.needs_full_tree_sync();
  needs_meta_info_recomputation_ = proto.needs_meta_info_recomputation();

  inputs_.overscroll_elasticity_layer =
      UpdateAndGetLayer(inputs_.overscroll_elasticity_layer.get(),
                        proto.overscroll_elasticity_layer_id(), this);
  inputs_.page_scale_layer = UpdateAndGetLayer(
      inputs_.page_scale_layer.get(), proto.page_scale_layer_id(), this);
  inputs_.inner_viewport_scroll_layer =
      UpdateAndGetLayer(inputs_.inner_viewport_scroll_layer.get(),
                        proto.inner_viewport_scroll_layer_id(), this);
  inputs_.outer_viewport_scroll_layer =
      UpdateAndGetLayer(inputs_.outer_viewport_scroll_layer.get(),
                        proto.outer_viewport_scroll_layer_id(), this);

  inputs_.device_viewport_size = ProtoToSize(proto.device_viewport_size());
  inputs_.top_controls_shrink_blink_size =
      proto.top_controls_shrink_blink_size();
  inputs_.top_controls_height = proto.top_controls_height();
  inputs_.top_controls_shown_ratio = proto.top_controls_shown_ratio();
  inputs_.device_scale_factor = proto.device_scale_factor();
  inputs_.painted_device_scale_factor = proto.painted_device_scale_factor();
  inputs_.page_scale_factor = proto.page_scale_factor();
  inputs_.min_page_scale_factor = proto.min_page_scale_factor();
  inputs_.max_page_scale_factor = proto.max_page_scale_factor();
  inputs_.background_color = proto.background_color();
  inputs_.has_transparent_background = proto.has_transparent_background();
  inputs_.have_scroll_event_handlers = proto.have_scroll_event_handlers();
  inputs_.event_listener_properties[static_cast<size_t>(
      EventListenerClass::kMouseWheel)] =
      static_cast<EventListenerProperties>(
          proto.wheel_event_listener_properties());
  inputs_.event_listener_properties[static_cast<size_t>(
      EventListenerClass::kTouchStartOrMove)] =
      static_cast<EventListenerProperties>(
          proto.touch_start_or_move_event_listener_properties());
  inputs_.event_listener_properties[static_cast<size_t>(
      EventListenerClass::kTouchEndOrCancel)] =
      static_cast<EventListenerProperties>(
          proto.touch_end_or_cancel_event_listener_properties());

  hud_layer_ = static_cast<HeadsUpDisplayLayer*>(
      UpdateAndGetLayer(hud_layer_.get(), proto.hud_layer_id(), this));

  LayerSelectionFromProtobuf(&inputs_.selection, proto.selection());
  elastic_overscroll_ = ProtoToVector2dF(proto.elastic_overscroll());

  // It is required to create new PropertyTrees before deserializing it.
  property_trees_ = PropertyTrees();
  property_trees_.FromProtobuf(proto.property_trees());

  // Forcefully override the sequence number of all layers in the tree to have
  // a valid sequence number. Changing the sequence number for a layer does not
  // need a commit, so the value will become out of date for layers that are not
  // updated for other reasons. All layers that at this point are part of the
  // layer tree are valid, so it is OK that they have a valid sequence number.
  int seq_num = property_trees_.sequence_number;
  LayerTreeHostCommon::CallFunctionForEveryLayer(this, [seq_num](Layer* layer) {
    layer->set_property_tree_sequence_number(seq_num);
  });
}

Layer* LayerTree::LayerByElementId(ElementId element_id) const {
  ElementLayersMap::const_iterator iter = element_layers_map_.find(element_id);
  return iter != element_layers_map_.end() ? iter->second : nullptr;
}

void LayerTree::RegisterElement(ElementId element_id,
                                ElementListType list_type,
                                Layer* layer) {
  if (layer->element_id()) {
    element_layers_map_[layer->element_id()] = layer;
  }

  animation_host_->RegisterElement(element_id, list_type);
}

void LayerTree::UnregisterElement(ElementId element_id,
                                  ElementListType list_type,
                                  Layer* layer) {
  animation_host_->UnregisterElement(element_id, list_type);

  if (layer->element_id()) {
    element_layers_map_.erase(layer->element_id());
  }
}

static void SetElementIdForTesting(Layer* layer) {
  layer->SetElementId(LayerIdToElementIdForTesting(layer->id()));
}

void LayerTree::SetElementIdsForTesting() {
  LayerTreeHostCommon::CallFunctionForEveryLayer(this, SetElementIdForTesting);
}

bool LayerTree::IsElementInList(ElementId element_id,
                                ElementListType list_type) const {
  return list_type == ElementListType::ACTIVE && LayerByElementId(element_id);
}

void LayerTree::SetMutatorsNeedCommit() {
  layer_tree_host_->SetNeedsCommit();
}

void LayerTree::SetMutatorsNeedRebuildPropertyTrees() {
  property_trees_.needs_rebuild = true;
}

void LayerTree::SetElementFilterMutated(ElementId element_id,
                                        ElementListType list_type,
                                        const FilterOperations& filters) {
  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnFilterAnimated(filters);
}

void LayerTree::SetElementOpacityMutated(ElementId element_id,
                                         ElementListType list_type,
                                         float opacity) {
  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnOpacityAnimated(opacity);
}

void LayerTree::SetElementTransformMutated(ElementId element_id,
                                           ElementListType list_type,
                                           const gfx::Transform& transform) {
  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnTransformAnimated(transform);
}

void LayerTree::SetElementScrollOffsetMutated(
    ElementId element_id,
    ElementListType list_type,
    const gfx::ScrollOffset& scroll_offset) {
  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnScrollOffsetAnimated(scroll_offset);
}

void LayerTree::ElementTransformIsAnimatingChanged(
    ElementId element_id,
    ElementListType list_type,
    AnimationChangeType change_type,
    bool is_animating) {
  Layer* layer = LayerByElementId(element_id);
  if (layer) {
    switch (change_type) {
      case AnimationChangeType::POTENTIAL:
        layer->OnTransformIsPotentiallyAnimatingChanged(is_animating);
        break;
      case AnimationChangeType::RUNNING:
        layer->OnTransformIsCurrentlyAnimatingChanged(is_animating);
        break;
      case AnimationChangeType::BOTH:
        layer->OnTransformIsPotentiallyAnimatingChanged(is_animating);
        layer->OnTransformIsCurrentlyAnimatingChanged(is_animating);
        break;
    }
  }
}

void LayerTree::ElementOpacityIsAnimatingChanged(
    ElementId element_id,
    ElementListType list_type,
    AnimationChangeType change_type,
    bool is_animating) {
  Layer* layer = LayerByElementId(element_id);
  if (layer) {
    switch (change_type) {
      case AnimationChangeType::POTENTIAL:
        layer->OnOpacityIsPotentiallyAnimatingChanged(is_animating);
        break;
      case AnimationChangeType::RUNNING:
        layer->OnOpacityIsCurrentlyAnimatingChanged(is_animating);
        break;
      case AnimationChangeType::BOTH:
        layer->OnOpacityIsPotentiallyAnimatingChanged(is_animating);
        layer->OnOpacityIsCurrentlyAnimatingChanged(is_animating);
        break;
    }
  }
}

void LayerTree::ElementFilterIsAnimatingChanged(ElementId element_id,
                                                ElementListType list_type,
                                                AnimationChangeType change_type,
                                                bool is_animating) {
  Layer* layer = LayerByElementId(element_id);
  if (layer) {
    switch (change_type) {
      case AnimationChangeType::POTENTIAL:
        layer->OnFilterIsPotentiallyAnimatingChanged(is_animating);
        break;
      case AnimationChangeType::RUNNING:
        layer->OnFilterIsCurrentlyAnimatingChanged(is_animating);
        break;
      case AnimationChangeType::BOTH:
        layer->OnFilterIsPotentiallyAnimatingChanged(is_animating);
        layer->OnFilterIsCurrentlyAnimatingChanged(is_animating);
        break;
    }
  }
}

gfx::ScrollOffset LayerTree::GetScrollOffsetForAnimation(
    ElementId element_id) const {
  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  return layer->ScrollOffsetForAnimation();
}

LayerListIterator<Layer> LayerTree::begin() const {
  return LayerListIterator<Layer>(inputs_.root_layer.get());
}

LayerListIterator<Layer> LayerTree::end() const {
  return LayerListIterator<Layer>(nullptr);
}

LayerListReverseIterator<Layer> LayerTree::rbegin() {
  return LayerListReverseIterator<Layer>(inputs_.root_layer.get());
}

LayerListReverseIterator<Layer> LayerTree::rend() {
  return LayerListReverseIterator<Layer>(nullptr);
}

void LayerTree::SetNeedsDisplayOnAllLayers() {
  for (auto* layer : *this)
    layer->SetNeedsDisplay();
}

}  // namespace cc
