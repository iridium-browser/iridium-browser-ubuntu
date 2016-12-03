// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_H_
#define CC_TREES_LAYER_TREE_H_

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/base/cc_export.h"
#include "cc/input/event_listener_properties.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/layer_list_iterator.h"
#include "cc/trees/mutator_host_client.h"
#include "cc/trees/property_tree.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace cc {

namespace proto {
class LayerTree;
class LayerUpdate;
}  // namespace proto

class AnimationHost;
class HeadsUpDisplayLayer;
class Layer;
class LayerTreeHost;
class LayerTreeImpl;
struct PendingPageScaleAnimation;

class CC_EXPORT LayerTree : public MutatorHostClient {
 public:
  using LayerSet = std::unordered_set<Layer*>;
  using LayerIdMap = std::unordered_map<int, Layer*>;

  LayerTree(std::unique_ptr<AnimationHost> animation_host,
            LayerTreeHost* layer_tree_host);
  virtual ~LayerTree();

  void SetRootLayer(scoped_refptr<Layer> root_layer);
  Layer* root_layer() { return inputs_.root_layer.get(); }
  const Layer* root_layer() const { return inputs_.root_layer.get(); }

  void RegisterViewportLayers(scoped_refptr<Layer> overscroll_elasticity_layer,
                              scoped_refptr<Layer> page_scale_layer,
                              scoped_refptr<Layer> inner_viewport_scroll_layer,
                              scoped_refptr<Layer> outer_viewport_scroll_layer);

  Layer* overscroll_elasticity_layer() const {
    return inputs_.overscroll_elasticity_layer.get();
  }
  Layer* page_scale_layer() const { return inputs_.page_scale_layer.get(); }
  Layer* inner_viewport_scroll_layer() const {
    return inputs_.inner_viewport_scroll_layer.get();
  }
  Layer* outer_viewport_scroll_layer() const {
    return inputs_.outer_viewport_scroll_layer.get();
  }

  void RegisterSelection(const LayerSelection& selection);

  void SetHaveScrollEventHandlers(bool have_event_handlers);
  bool have_scroll_event_handlers() const {
    return inputs_.have_scroll_event_handlers;
  }

  void SetEventListenerProperties(EventListenerClass event_class,
                                  EventListenerProperties event_properties);
  EventListenerProperties event_listener_properties(
      EventListenerClass event_class) const {
    return inputs_.event_listener_properties[static_cast<size_t>(event_class)];
  }

  void SetViewportSize(const gfx::Size& device_viewport_size);
  gfx::Size device_viewport_size() const {
    return inputs_.device_viewport_size;
  }

  void SetTopControlsHeight(float height, bool shrink);
  void SetTopControlsShownRatio(float ratio);
  void SetBottomControlsHeight(float height);

  void SetPageScaleFactorAndLimits(float page_scale_factor,
                                   float min_page_scale_factor,
                                   float max_page_scale_factor);
  float page_scale_factor() const { return inputs_.page_scale_factor; }

  void set_background_color(SkColor color) { inputs_.background_color = color; }
  SkColor background_color() const { return inputs_.background_color; }

  void set_has_transparent_background(bool transparent) {
    inputs_.has_transparent_background = transparent;
  }

  void StartPageScaleAnimation(const gfx::Vector2d& target_offset,
                               bool use_anchor,
                               float scale,
                               base::TimeDelta duration);
  bool HasPendingPageScaleAnimation() const;

  void SetDeviceScaleFactor(float device_scale_factor);
  float device_scale_factor() const { return inputs_.device_scale_factor; }

  void SetPaintedDeviceScaleFactor(float painted_device_scale_factor);

  gfx::Vector2dF elastic_overscroll() const { return elastic_overscroll_; }

  // Used externally by blink for setting the PropertyTrees when
  // |settings_.use_layer_lists| is true. This is a SPV2 setting.
  PropertyTrees* property_trees() { return &property_trees_; }

  bool in_paint_layer_contents() const { return in_paint_layer_contents_; }

  // Methods which should only be used internally in cc ------------------
  void RegisterLayer(Layer* layer);
  void UnregisterLayer(Layer* layer);
  Layer* LayerById(int id) const;
  bool UpdateLayers(const LayerList& update_layer_list,
                    bool* content_is_suitable_for_gpu);

  void AddLayerShouldPushProperties(Layer* layer);
  void RemoveLayerShouldPushProperties(Layer* layer);
  std::unordered_set<Layer*>& LayersThatShouldPushProperties();
  bool LayerNeedsPushPropertiesForTesting(Layer* layer) const;

  virtual void SetNeedsMetaInfoRecomputation(
      bool needs_meta_info_recomputation);
  bool needs_meta_info_recomputation() const {
    return needs_meta_info_recomputation_;
  }

  void SetPageScaleFromImplSide(float page_scale);
  void SetElasticOverscrollFromImplSide(gfx::Vector2dF elastic_overscroll);

  void UpdateHudLayer(bool show_hud_info);
  HeadsUpDisplayLayer* hud_layer() const { return hud_layer_.get(); }

  virtual void SetNeedsFullTreeSync();
  bool needs_full_tree_sync() const { return needs_full_tree_sync_; }

  void SetNeedsCommit();
  void SetPropertyTreesNeedRebuild();

  void PushPropertiesTo(LayerTreeImpl* tree_impl);

  void ToProtobuf(proto::LayerTree* proto);
  void FromProtobuf(const proto::LayerTree& proto);

  AnimationHost* animation_host() const { return animation_host_.get(); }

  Layer* LayerByElementId(ElementId element_id) const;
  void RegisterElement(ElementId element_id,
                       ElementListType list_type,
                       Layer* layer);
  void UnregisterElement(ElementId element_id,
                         ElementListType list_type,
                         Layer* layer);
  void SetElementIdsForTesting();

  // Layer iterators.
  LayerListIterator<Layer> begin() const;
  LayerListIterator<Layer> end() const;
  LayerListReverseIterator<Layer> rbegin();
  LayerListReverseIterator<Layer> rend();

  void SetNeedsDisplayOnAllLayers();
  // ---------------------------------------------------------------------

 private:
  friend class LayerTreeHostSerializationTest;

  // MutatorHostClient implementation.
  bool IsElementInList(ElementId element_id,
                       ElementListType list_type) const override;
  void SetMutatorsNeedCommit() override;
  void SetMutatorsNeedRebuildPropertyTrees() override;
  void SetElementFilterMutated(ElementId element_id,
                               ElementListType list_type,
                               const FilterOperations& filters) override;
  void SetElementOpacityMutated(ElementId element_id,
                                ElementListType list_type,
                                float opacity) override;
  void SetElementTransformMutated(ElementId element_id,
                                  ElementListType list_type,
                                  const gfx::Transform& transform) override;
  void SetElementScrollOffsetMutated(
      ElementId element_id,
      ElementListType list_type,
      const gfx::ScrollOffset& scroll_offset) override;
  void ElementTransformIsAnimatingChanged(ElementId element_id,
                                          ElementListType list_type,
                                          AnimationChangeType change_type,
                                          bool is_animating) override;
  void ElementOpacityIsAnimatingChanged(ElementId element_id,
                                        ElementListType list_type,
                                        AnimationChangeType change_type,
                                        bool is_animating) override;
  void ElementFilterIsAnimatingChanged(ElementId element_id,
                                       ElementListType list_type,
                                       AnimationChangeType change_type,
                                       bool is_animating) override;
  void ScrollOffsetAnimationFinished() override {}
  gfx::ScrollOffset GetScrollOffsetForAnimation(
      ElementId element_id) const override;

  // Encapsulates the data, callbacks, interfaces received from the embedder.
  struct Inputs {
    Inputs();
    ~Inputs();

    scoped_refptr<Layer> root_layer;

    scoped_refptr<Layer> overscroll_elasticity_layer;
    scoped_refptr<Layer> page_scale_layer;
    scoped_refptr<Layer> inner_viewport_scroll_layer;
    scoped_refptr<Layer> outer_viewport_scroll_layer;

    float top_controls_height;
    float top_controls_shown_ratio;
    bool top_controls_shrink_blink_size;

    float bottom_controls_height;

    float device_scale_factor;
    float painted_device_scale_factor;
    float page_scale_factor;
    float min_page_scale_factor;
    float max_page_scale_factor;

    SkColor background_color;
    bool has_transparent_background;

    LayerSelection selection;

    gfx::Size device_viewport_size;

    bool have_scroll_event_handlers;
    EventListenerProperties event_listener_properties[static_cast<size_t>(
        EventListenerClass::kNumClasses)];

    std::unique_ptr<PendingPageScaleAnimation> pending_page_scale_animation;
  };

  Inputs inputs_;

  PropertyTrees property_trees_;

  bool needs_full_tree_sync_;
  bool needs_meta_info_recomputation_;

  gfx::Vector2dF elastic_overscroll_;

  scoped_refptr<HeadsUpDisplayLayer> hud_layer_;

  // Set of layers that need to push properties.
  LayerSet layers_that_should_push_properties_;

  // Layer id to Layer map.
  LayerIdMap layer_id_map_;

  using ElementLayersMap = std::unordered_map<ElementId, Layer*, ElementIdHash>;
  ElementLayersMap element_layers_map_;

  bool in_paint_layer_contents_;

  std::unique_ptr<AnimationHost> animation_host_;
  LayerTreeHost* layer_tree_host_;

  DISALLOW_COPY_AND_ASSIGN(LayerTree);
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_H_
