// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blink/web_layer_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/trace_event_impl.h"
#include "cc/base/region.h"
#include "cc/base/switches.h"
#include "cc/blink/web_blend_mode.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_position_constraint.h"
#include "cc/trees/element_id.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebFloatRect.h"
#include "third_party/WebKit/public/platform/WebLayerPositionConstraint.h"
#include "third_party/WebKit/public/platform/WebLayerScrollClient.h"
#include "third_party/WebKit/public/platform/WebLayerStickyPositionConstraint.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

using cc::Layer;
using blink::WebLayer;
using blink::WebFloatPoint;
using blink::WebVector;
using blink::WebRect;
using blink::WebSize;
using blink::WebColor;

namespace cc_blink {

WebLayerImpl::WebLayerImpl()
    : layer_(Layer::Create()), contents_opaque_is_fixed_(false) {}

WebLayerImpl::WebLayerImpl(scoped_refptr<Layer> layer)
    : layer_(layer), contents_opaque_is_fixed_(false) {
}

WebLayerImpl::~WebLayerImpl() {
  layer_->SetLayerClient(nullptr);
}

int WebLayerImpl::id() const {
  return layer_->id();
}

DISABLE_CFI_PERF
void WebLayerImpl::invalidateRect(const blink::WebRect& rect) {
  layer_->SetNeedsDisplayRect(rect);
}

void WebLayerImpl::invalidate() {
  layer_->SetNeedsDisplay();
}

void WebLayerImpl::addChild(WebLayer* child) {
  layer_->AddChild(static_cast<WebLayerImpl*>(child)->layer());
}

void WebLayerImpl::insertChild(WebLayer* child, size_t index) {
  layer_->InsertChild(static_cast<WebLayerImpl*>(child)->layer(), index);
}

void WebLayerImpl::replaceChild(WebLayer* reference, WebLayer* new_layer) {
  layer_->ReplaceChild(static_cast<WebLayerImpl*>(reference)->layer(),
                       static_cast<WebLayerImpl*>(new_layer)->layer());
}

void WebLayerImpl::removeFromParent() {
  layer_->RemoveFromParent();
}

void WebLayerImpl::removeAllChildren() {
  layer_->RemoveAllChildren();
}

void WebLayerImpl::setBounds(const WebSize& size) {
  layer_->SetBounds(size);
}

WebSize WebLayerImpl::bounds() const {
  return layer_->bounds();
}

void WebLayerImpl::setMasksToBounds(bool masks_to_bounds) {
  layer_->SetMasksToBounds(masks_to_bounds);
}

bool WebLayerImpl::masksToBounds() const {
  return layer_->masks_to_bounds();
}

void WebLayerImpl::setMaskLayer(WebLayer* maskLayer) {
  layer_->SetMaskLayer(
      maskLayer ? static_cast<WebLayerImpl*>(maskLayer)->layer() : 0);
}

void WebLayerImpl::setOpacity(float opacity) {
  layer_->SetOpacity(opacity);
}

float WebLayerImpl::opacity() const {
  return layer_->opacity();
}

void WebLayerImpl::setBlendMode(blink::WebBlendMode blend_mode) {
  layer_->SetBlendMode(BlendModeToSkia(blend_mode));
}

blink::WebBlendMode WebLayerImpl::blendMode() const {
  return BlendModeFromSkia(layer_->blend_mode());
}

void WebLayerImpl::setIsRootForIsolatedGroup(bool isolate) {
  layer_->SetIsRootForIsolatedGroup(isolate);
}

bool WebLayerImpl::isRootForIsolatedGroup() {
  return layer_->is_root_for_isolated_group();
}

void WebLayerImpl::setOpaque(bool opaque) {
  if (contents_opaque_is_fixed_)
    return;
  layer_->SetContentsOpaque(opaque);
}

bool WebLayerImpl::opaque() const {
  return layer_->contents_opaque();
}

void WebLayerImpl::setPosition(const WebFloatPoint& position) {
  layer_->SetPosition(position);
}

WebFloatPoint WebLayerImpl::position() const {
  return layer_->position();
}

void WebLayerImpl::setTransform(const SkMatrix44& matrix) {
  gfx::Transform transform;
  transform.matrix() = matrix;
  layer_->SetTransform(transform);
}

void WebLayerImpl::setTransformOrigin(const blink::WebFloatPoint3D& point) {
  gfx::Point3F gfx_point = point;
  layer_->SetTransformOrigin(gfx_point);
}

blink::WebFloatPoint3D WebLayerImpl::transformOrigin() const {
  return layer_->transform_origin();
}

SkMatrix44 WebLayerImpl::transform() const {
  return layer_->transform().matrix();
}

void WebLayerImpl::setDrawsContent(bool draws_content) {
  layer_->SetIsDrawable(draws_content);
}

bool WebLayerImpl::drawsContent() const {
  return layer_->DrawsContent();
}

void WebLayerImpl::setDoubleSided(bool double_sided) {
  layer_->SetDoubleSided(double_sided);
}

void WebLayerImpl::setShouldFlattenTransform(bool flatten) {
  layer_->SetShouldFlattenTransform(flatten);
}

void WebLayerImpl::setRenderingContext(int context) {
  layer_->Set3dSortingContextId(context);
}

void WebLayerImpl::setUseParentBackfaceVisibility(
    bool use_parent_backface_visibility) {
  layer_->SetUseParentBackfaceVisibility(use_parent_backface_visibility);
}

void WebLayerImpl::setBackgroundColor(WebColor color) {
  layer_->SetBackgroundColor(color);
}

WebColor WebLayerImpl::backgroundColor() const {
  return layer_->background_color();
}

void WebLayerImpl::setFilters(const cc::FilterOperations& filters) {
  layer_->SetFilters(filters);
}

void WebLayerImpl::setFiltersOrigin(const blink::WebFloatPoint& origin) {
  layer_->SetFiltersOrigin(origin);
}

void WebLayerImpl::setBackgroundFilters(const cc::FilterOperations& filters) {
  layer_->SetBackgroundFilters(filters);
}

bool WebLayerImpl::hasTickingAnimationForTesting() {
  return layer_->HasTickingAnimationForTesting();
}

void WebLayerImpl::setScrollPosition(blink::WebFloatPoint position) {
  layer_->SetScrollOffset(gfx::ScrollOffset(position.x, position.y));
}

blink::WebFloatPoint WebLayerImpl::scrollPosition() const {
  return blink::WebFloatPoint(layer_->scroll_offset().x(),
                              layer_->scroll_offset().y());
}

void WebLayerImpl::setScrollClipLayer(WebLayer* clip_layer) {
  if (!clip_layer) {
    layer_->SetScrollClipLayerId(Layer::INVALID_ID);
    return;
  }
  layer_->SetScrollClipLayerId(clip_layer->id());
}

bool WebLayerImpl::scrollable() const {
  return layer_->scrollable();
}

void WebLayerImpl::setUserScrollable(bool horizontal, bool vertical) {
  layer_->SetUserScrollable(horizontal, vertical);
}

bool WebLayerImpl::userScrollableHorizontal() const {
  return layer_->user_scrollable_horizontal();
}

bool WebLayerImpl::userScrollableVertical() const {
  return layer_->user_scrollable_vertical();
}

void WebLayerImpl::addMainThreadScrollingReasons(
    uint32_t main_thread_scrolling_reasons) {
  // WebLayerImpl should only know about non-transient scrolling
  // reasons. Transient scrolling reasons are computed per hit test.
  DCHECK(main_thread_scrolling_reasons);
  DCHECK(cc::MainThreadScrollingReason::MainThreadCanSetScrollReasons(
      main_thread_scrolling_reasons));
  layer_->AddMainThreadScrollingReasons(main_thread_scrolling_reasons);
}

void WebLayerImpl::clearMainThreadScrollingReasons(
    uint32_t main_thread_scrolling_reasons_to_clear) {
  layer_->ClearMainThreadScrollingReasons(
      main_thread_scrolling_reasons_to_clear);
}

uint32_t WebLayerImpl::mainThreadScrollingReasons() {
  return layer_->main_thread_scrolling_reasons();
}

bool WebLayerImpl::shouldScrollOnMainThread() const {
  return layer_->should_scroll_on_main_thread();
}

void WebLayerImpl::setNonFastScrollableRegion(const WebVector<WebRect>& rects) {
  cc::Region region;
  for (size_t i = 0; i < rects.size(); ++i)
    region.Union(rects[i]);
  layer_->SetNonFastScrollableRegion(region);
}

WebVector<WebRect> WebLayerImpl::nonFastScrollableRegion() const {
  size_t num_rects = 0;
  for (cc::Region::Iterator region_rects(layer_->non_fast_scrollable_region());
       region_rects.has_rect();
       region_rects.next())
    ++num_rects;

  WebVector<WebRect> result(num_rects);
  size_t i = 0;
  for (cc::Region::Iterator region_rects(layer_->non_fast_scrollable_region());
       region_rects.has_rect();
       region_rects.next()) {
    result[i] = region_rects.rect();
    ++i;
  }
  return result;
}

void WebLayerImpl::setTouchEventHandlerRegion(const WebVector<WebRect>& rects) {
  cc::Region region;
  for (size_t i = 0; i < rects.size(); ++i)
    region.Union(rects[i]);
  layer_->SetTouchEventHandlerRegion(region);
}

WebVector<WebRect> WebLayerImpl::touchEventHandlerRegion() const {
  size_t num_rects = 0;
  for (cc::Region::Iterator region_rects(layer_->touch_event_handler_region());
       region_rects.has_rect();
       region_rects.next())
    ++num_rects;

  WebVector<WebRect> result(num_rects);
  size_t i = 0;
  for (cc::Region::Iterator region_rects(layer_->touch_event_handler_region());
       region_rects.has_rect();
       region_rects.next()) {
    result[i] = region_rects.rect();
    ++i;
  }
  return result;
}

void WebLayerImpl::setIsContainerForFixedPositionLayers(bool enable) {
  layer_->SetIsContainerForFixedPositionLayers(enable);
}

bool WebLayerImpl::isContainerForFixedPositionLayers() const {
  return layer_->IsContainerForFixedPositionLayers();
}

static blink::WebLayerPositionConstraint ToWebLayerPositionConstraint(
    const cc::LayerPositionConstraint& constraint) {
  blink::WebLayerPositionConstraint web_constraint;
  web_constraint.isFixedPosition = constraint.is_fixed_position();
  web_constraint.isFixedToRightEdge = constraint.is_fixed_to_right_edge();
  web_constraint.isFixedToBottomEdge = constraint.is_fixed_to_bottom_edge();
  return web_constraint;
}

static cc::LayerPositionConstraint ToLayerPositionConstraint(
    const blink::WebLayerPositionConstraint& web_constraint) {
  cc::LayerPositionConstraint constraint;
  constraint.set_is_fixed_position(web_constraint.isFixedPosition);
  constraint.set_is_fixed_to_right_edge(web_constraint.isFixedToRightEdge);
  constraint.set_is_fixed_to_bottom_edge(web_constraint.isFixedToBottomEdge);
  return constraint;
}

void WebLayerImpl::setPositionConstraint(
    const blink::WebLayerPositionConstraint& constraint) {
  layer_->SetPositionConstraint(ToLayerPositionConstraint(constraint));
}

blink::WebLayerPositionConstraint WebLayerImpl::positionConstraint() const {
  return ToWebLayerPositionConstraint(layer_->position_constraint());
}

static blink::WebLayerStickyPositionConstraint
ToWebLayerStickyPositionConstraint(
    const cc::LayerStickyPositionConstraint& constraint) {
  blink::WebLayerStickyPositionConstraint web_constraint;
  web_constraint.isSticky = constraint.is_sticky;
  web_constraint.isAnchoredLeft = constraint.is_anchored_left;
  web_constraint.isAnchoredRight = constraint.is_anchored_right;
  web_constraint.isAnchoredTop = constraint.is_anchored_top;
  web_constraint.isAnchoredBottom = constraint.is_anchored_bottom;
  web_constraint.leftOffset = constraint.left_offset;
  web_constraint.rightOffset = constraint.right_offset;
  web_constraint.topOffset = constraint.top_offset;
  web_constraint.bottomOffset = constraint.bottom_offset;
  web_constraint.parentRelativeStickyBoxOffset =
      constraint.parent_relative_sticky_box_offset;
  web_constraint.scrollContainerRelativeStickyBoxRect =
      constraint.scroll_container_relative_sticky_box_rect;
  web_constraint.scrollContainerRelativeContainingBlockRect =
      constraint.scroll_container_relative_containing_block_rect;
  return web_constraint;
}
static cc::LayerStickyPositionConstraint ToStickyPositionConstraint(
    const blink::WebLayerStickyPositionConstraint& web_constraint) {
  cc::LayerStickyPositionConstraint constraint;
  constraint.is_sticky = web_constraint.isSticky;
  constraint.is_anchored_left = web_constraint.isAnchoredLeft;
  constraint.is_anchored_right = web_constraint.isAnchoredRight;
  constraint.is_anchored_top = web_constraint.isAnchoredTop;
  constraint.is_anchored_bottom = web_constraint.isAnchoredBottom;
  constraint.left_offset = web_constraint.leftOffset;
  constraint.right_offset = web_constraint.rightOffset;
  constraint.top_offset = web_constraint.topOffset;
  constraint.bottom_offset = web_constraint.bottomOffset;
  constraint.parent_relative_sticky_box_offset =
      web_constraint.parentRelativeStickyBoxOffset;
  constraint.scroll_container_relative_sticky_box_rect =
      web_constraint.scrollContainerRelativeStickyBoxRect;
  constraint.scroll_container_relative_containing_block_rect =
      web_constraint.scrollContainerRelativeContainingBlockRect;
  return constraint;
}
void WebLayerImpl::setStickyPositionConstraint(
    const blink::WebLayerStickyPositionConstraint& constraint) {
  layer_->SetStickyPositionConstraint(ToStickyPositionConstraint(constraint));
}
blink::WebLayerStickyPositionConstraint WebLayerImpl::stickyPositionConstraint()
    const {
  return ToWebLayerStickyPositionConstraint(
      layer_->sticky_position_constraint());
}

void WebLayerImpl::setScrollClient(blink::WebLayerScrollClient* scroll_client) {
  if (scroll_client) {
    layer_->set_did_scroll_callback(
        base::Bind(&blink::WebLayerScrollClient::didScroll,
                   base::Unretained(scroll_client)));
  } else {
    layer_->set_did_scroll_callback(
        base::Callback<void(const gfx::ScrollOffset&)>());
  }
}

void WebLayerImpl::setLayerClient(cc::LayerClient* client) {
  layer_->SetLayerClient(client);
}

const cc::Layer* WebLayerImpl::ccLayer() const {
  return layer_.get();
}

cc::Layer* WebLayerImpl::ccLayer() {
  return layer_.get();
}

void WebLayerImpl::setElementId(const cc::ElementId& id) {
  layer_->SetElementId(id);
}

cc::ElementId WebLayerImpl::elementId() const {
  return layer_->element_id();
}

void WebLayerImpl::setCompositorMutableProperties(uint32_t properties) {
  layer_->SetMutableProperties(properties);
}

uint32_t WebLayerImpl::compositorMutableProperties() const {
  return layer_->mutable_properties();
}

void WebLayerImpl::setScrollParent(blink::WebLayer* parent) {
  cc::Layer* scroll_parent = nullptr;
  if (parent)
    scroll_parent = static_cast<WebLayerImpl*>(parent)->layer();
  layer_->SetScrollParent(scroll_parent);
}

void WebLayerImpl::setClipParent(blink::WebLayer* parent) {
  cc::Layer* clip_parent = nullptr;
  if (parent)
    clip_parent = static_cast<WebLayerImpl*>(parent)->layer();
  layer_->SetClipParent(clip_parent);
}

Layer* WebLayerImpl::layer() const {
  return layer_.get();
}

void WebLayerImpl::SetContentsOpaqueIsFixed(bool fixed) {
  contents_opaque_is_fixed_ = fixed;
}

void WebLayerImpl::setHasWillChangeTransformHint(bool has_will_change) {
  layer_->SetHasWillChangeTransformHint(has_will_change);
}

void WebLayerImpl::setPreferredRasterBounds(const WebSize& bounds) {
  layer_->SetPreferredRasterBounds(bounds);
}

void WebLayerImpl::clearPreferredRasterBounds() {
  layer_->ClearPreferredRasterBounds();
}

}  // namespace cc_blink
