// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <set>
#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/animation/animation_host.h"
#include "cc/layers/layer_impl.h"
#include "cc/output/copy_output_request.h"
#include "cc/proto/property_tree.pb.h"
#include "cc/proto/synced_property_conversions.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {

template <typename T>
PropertyTree<T>::PropertyTree()
    : needs_update_(false) {
  nodes_.push_back(T());
  back()->id = kRootNodeId;
  back()->parent_id = kInvalidNodeId;
}

// Equivalent to
// PropertyTree<T>::~PropertyTree() = default;
// but due to a gcc bug the generated destructor will have wrong symbol
// visibility in component build.
template <typename T>
PropertyTree<T>::~PropertyTree() {}

template <typename T>
PropertyTree<T>& PropertyTree<T>::operator=(const PropertyTree<T>&) = default;

TransformTree::TransformTree()
    : source_to_parent_updates_allowed_(true),
      page_scale_factor_(1.f),
      device_scale_factor_(1.f),
      device_transform_scale_factor_(1.f) {
  cached_data_.push_back(TransformCachedNodeData());
}

TransformTree::~TransformTree() = default;
TransformTree& TransformTree::operator=(const TransformTree&) = default;

template <typename T>
int PropertyTree<T>::Insert(const T& tree_node, int parent_id) {
  DCHECK_GT(nodes_.size(), 0u);
  nodes_.push_back(tree_node);
  T& node = nodes_.back();
  node.parent_id = parent_id;
  node.id = static_cast<int>(nodes_.size()) - 1;
  return node.id;
}

template <typename T>
void PropertyTree<T>::clear() {
  needs_update_ = false;
  nodes_.clear();
  nodes_.push_back(T());
  back()->id = kRootNodeId;
  back()->parent_id = kInvalidNodeId;

#if DCHECK_IS_ON()
  PropertyTree<T> tree;
  DCHECK(tree == *this);
#endif
}

template <typename T>
bool PropertyTree<T>::operator==(const PropertyTree<T>& other) const {
  return nodes_ == other.nodes() && needs_update_ == other.needs_update();
}

template <typename T>
void PropertyTree<T>::ToProtobuf(proto::PropertyTree* proto) const {
  DCHECK_EQ(0, proto->nodes_size());
  for (const auto& node : nodes_)
    node.ToProtobuf(proto->add_nodes());
  proto->set_needs_update(needs_update_);
}

template <typename T>
void PropertyTree<T>::FromProtobuf(
    const proto::PropertyTree& proto,
    std::unordered_map<int, int>* node_id_to_index_map) {
  // Verify that the property tree is empty.
  DCHECK_EQ(static_cast<int>(nodes_.size()), 1);
  DCHECK_EQ(back()->id, kRootNodeId);
  DCHECK_EQ(back()->parent_id, kInvalidNodeId);

  // Add the first node.
  DCHECK_GT(proto.nodes_size(), 0);
  nodes_.back().FromProtobuf(proto.nodes(0));

  DCHECK(!node_id_to_index_map || (*node_id_to_index_map).empty());
  for (int i = 1; i < proto.nodes_size(); ++i) {
    nodes_.push_back(T());
    nodes_.back().FromProtobuf(proto.nodes(i));
    (*node_id_to_index_map)[nodes_.back().owner_id] = nodes_.back().id;
  }

  needs_update_ = proto.needs_update();
}

template <typename T>
void PropertyTree<T>::AsValueInto(base::trace_event::TracedValue* value) const {
  value->BeginArray("nodes");
  for (const auto& node : nodes_) {
    value->BeginDictionary();
    node.AsValueInto(value);
    value->EndDictionary();
  }
  value->EndArray();
}

template class PropertyTree<TransformNode>;
template class PropertyTree<ClipNode>;
template class PropertyTree<EffectNode>;
template class PropertyTree<ScrollNode>;

int TransformTree::Insert(const TransformNode& tree_node, int parent_id) {
  int node_id = PropertyTree<TransformNode>::Insert(tree_node, parent_id);
  DCHECK_EQ(node_id, static_cast<int>(cached_data_.size()));

  cached_data_.push_back(TransformCachedNodeData());
  return node_id;
}

void TransformTree::clear() {
  PropertyTree<TransformNode>::clear();

  page_scale_factor_ = 1.f;
  device_scale_factor_ = 1.f;
  device_transform_scale_factor_ = 1.f;
  nodes_affected_by_inner_viewport_bounds_delta_.clear();
  nodes_affected_by_outer_viewport_bounds_delta_.clear();
  cached_data_.clear();
  cached_data_.push_back(TransformCachedNodeData());

#if DCHECK_IS_ON()
  TransformTree tree;
  // TODO(jaydasika) : Move tests that expect source_to_parent_updates_allowed
  // to be true on impl thread to main thread and set it to is_main_thread here.
  tree.source_to_parent_updates_allowed_ = source_to_parent_updates_allowed_;
  DCHECK(tree == *this);
#endif
}

bool TransformTree::ComputeTransform(int source_id,
                                     int dest_id,
                                     gfx::Transform* transform) const {
  transform->MakeIdentity();

  if (source_id == dest_id)
    return true;

  if (source_id > dest_id) {
    CombineTransformsBetween(source_id, dest_id, transform);
    return true;
  }

  return CombineInversesBetween(source_id, dest_id, transform);
}

bool TransformTree::ComputeTranslation(int source_id,
                                       int dest_id,
                                       gfx::Transform* transform) const {
  transform->MakeIdentity();
  if (source_id == dest_id)
    return true;

  const TransformNode* dest = Node(dest_id);
  if (!dest->ancestors_are_invertible)
    return false;
  if (source_id != kInvalidNodeId)
    transform->ConcatTransform(ToScreen(source_id));
  if (dest_id != kInvalidNodeId) {
    if (dest->local.IsFlat() && (dest->node_and_ancestors_are_flat ||
                                 dest->flattens_inherited_transform)) {
      // In this case, flattenning will not affect the result, so we can use the
      // FromScreen transform of the dest node.
      transform->ConcatTransform(FromScreen(dest_id));
    } else {
      // In this case, some node between source and destination flattens
      // inherited transform. Consider the tree R->A->B->C->D, where D is the
      // source, A is the destination and C flattens inherited transform. The
      // expected result is D * C * flattened(B). D's ToScreen will be D * C *
      // flattened(B * A * R), but as the source to destination transform is
      // at most translation, C and B cannot be non-flat and so flattened(B * A
      // * R) = B * flattened(A * R). So, to get the expected result we have to
      // multiply D's ToScreen transform with flattened(A * R)^{-1}, which is
      // the inverse of flattened ToScreen of destination.
      gfx::Transform to_screen = ToScreen(dest_id);
      to_screen.FlattenTo2d();
      gfx::Transform from_screen;
      bool success = to_screen.GetInverse(&from_screen);
      if (!success)
        return false;
      transform->ConcatTransform(from_screen);
    }
  }

  return true;
}

void TransformTree::OnTransformAnimated(const gfx::Transform& transform,
                                        int id,
                                        LayerTreeImpl* layer_tree_impl) {
  TransformNode* node = Node(id);
  layer_tree_impl->AddToTransformAnimationsMap(node->owner_id, transform);
  if (node->local == transform) {
    return;
  }
  node->local = transform;
  node->needs_local_transform_update = true;
  node->transform_changed = true;
  property_trees()->changed = true;
  set_needs_update(true);
  layer_tree_impl->set_needs_update_draw_properties();
}

bool TransformTree::NeedsSourceToParentUpdate(TransformNode* node) {
  return (source_to_parent_updates_allowed() &&
          node->parent_id != node->source_node_id);
}

void TransformTree::ResetChangeTracking() {
  for (int id = 1; id < static_cast<int>(size()); ++id) {
    TransformNode* node = Node(id);
    node->transform_changed = false;
  }
}

void TransformTree::UpdateTransforms(int id) {
  TransformNode* node = Node(id);
  TransformNode* parent_node = parent(node);
  TransformNode* target_node = Node(TargetId(id));
  TransformNode* source_node = Node(node->source_node_id);
  property_trees()->UpdateCachedNumber();
  if (node->needs_local_transform_update || NeedsSourceToParentUpdate(node))
    UpdateLocalTransform(node);
  else
    UndoSnapping(node);
  UpdateScreenSpaceTransform(node, parent_node, target_node);
  UpdateSurfaceContentsScale(node);
  UpdateAnimationProperties(node, parent_node);
  UpdateSnapping(node);
  UpdateTargetSpaceTransform(node, target_node);
  UpdateNodeAndAncestorsHaveIntegerTranslations(node, parent_node);
  UpdateTransformChanged(node, parent_node, source_node);
  UpdateNodeAndAncestorsAreAnimatedOrInvertible(node, parent_node);
}

bool TransformTree::IsDescendant(int desc_id, int source_id) const {
  while (desc_id != source_id) {
    if (desc_id == kInvalidNodeId)
      return false;
    desc_id = Node(desc_id)->parent_id;
  }
  return true;
}

void TransformTree::CombineTransformsBetween(int source_id,
                                             int dest_id,
                                             gfx::Transform* transform) const {
  DCHECK(source_id > dest_id);
  const TransformNode* current = Node(source_id);
  const TransformNode* dest = Node(dest_id);
  // Combine transforms to and from the screen when possible. Since flattening
  // is a non-linear operation, we cannot use this approach when there is
  // non-trivial flattening between the source and destination nodes. For
  // example, consider the tree R->A->B->C, where B flattens its inherited
  // transform, and A has a non-flat transform. Suppose C is the source and A is
  // the destination. The expected result is C * B. But C's to_screen
  // transform is C * B * flattened(A * R), and A's from_screen transform is
  // R^{-1} * A^{-1}. If at least one of A and R isn't flat, the inverse of
  // flattened(A * R) won't be R^{-1} * A{-1}, so multiplying C's to_screen and
  // A's from_screen will not produce the correct result.
  if (!dest ||
      (dest->ancestors_are_invertible && dest->node_and_ancestors_are_flat)) {
    transform->ConcatTransform(ToScreen(current->id));
    if (dest)
      transform->ConcatTransform(FromScreen(dest->id));
    return;
  }

  // Flattening is defined in a way that requires it to be applied while
  // traversing downward in the tree. We first identify nodes that are on the
  // path from the source to the destination (this is traversing upward), and
  // then we visit these nodes in reverse order, flattening as needed. We
  // early-out if we get to a node whose target node is the destination, since
  // we can then re-use the target space transform stored at that node. However,
  // we cannot re-use a stored target space transform if the destination has a
  // zero surface contents scale, since stored target space transforms have
  // surface contents scale baked in, but we need to compute an unscaled
  // transform.
  std::vector<int> source_to_destination;
  source_to_destination.push_back(current->id);
  current = parent(current);
  bool destination_has_non_zero_surface_contents_scale =
      dest->surface_contents_scale.x() != 0.f &&
      dest->surface_contents_scale.y() != 0.f;
  DCHECK(destination_has_non_zero_surface_contents_scale ||
         !dest->ancestors_are_invertible);
  for (; current && current->id > dest_id; current = parent(current)) {
    if (destination_has_non_zero_surface_contents_scale &&
        TargetId(current->id) == dest_id &&
        ContentTargetId(current->id) == dest_id)
      break;
    source_to_destination.push_back(current->id);
  }

  gfx::Transform combined_transform;
  if (current->id > dest_id) {
    // TODO(sunxd): Instead of using target space transform, only use to_parent
    // here when we fully implement computing draw transforms on demand.
    combined_transform = ToTarget(current->id, kInvalidNodeId);
    // The stored target space transform has surface contents scale baked in,
    // but we need the unscaled transform.
    combined_transform.matrix().postScale(
        1.0f / dest->surface_contents_scale.x(),
        1.0f / dest->surface_contents_scale.y(), 1.0f);
  } else if (current->id < dest_id) {
    // We have reached the lowest common ancestor of the source and destination
    // nodes. This case can occur when we are transforming between a node
    // corresponding to a fixed-position layer (or its descendant) and the node
    // corresponding to the layer's render target. For example, consider the
    // layer tree R->T->S->F where F is fixed-position, S owns a render surface,
    // and T has a significant transform. This will yield the following
    // transform tree:
    //    R
    //    |
    //    T
    //   /|
    //  S F
    // In this example, T will have id 2, S will have id 3, and F will have id
    // 4. When walking up the ancestor chain from F, the first node with a
    // smaller id than S will be T, the lowest common ancestor of these nodes.
    // We compute the transform from T to S here, and then from F to T in the
    // loop below.
    DCHECK(IsDescendant(dest_id, current->id));
    CombineInversesBetween(current->id, dest_id, &combined_transform);
    DCHECK(combined_transform.IsApproximatelyIdentityOrTranslation(
        SkDoubleToMScalar(1e-4)));
  }

  size_t source_to_destination_size = source_to_destination.size();
  for (size_t i = 0; i < source_to_destination_size; ++i) {
    size_t index = source_to_destination_size - 1 - i;
    const TransformNode* node = Node(source_to_destination[index]);
    if (node->flattens_inherited_transform)
      combined_transform.FlattenTo2d();
    combined_transform.PreconcatTransform(node->to_parent);
  }

  transform->ConcatTransform(combined_transform);
}

bool TransformTree::CombineInversesBetween(int source_id,
                                           int dest_id,
                                           gfx::Transform* transform) const {
  DCHECK(source_id < dest_id);
  const TransformNode* current = Node(dest_id);
  const TransformNode* dest = Node(source_id);
  // Just as in CombineTransformsBetween, we can use screen space transforms in
  // this computation only when there isn't any non-trivial flattening
  // involved.
  if (current->ancestors_are_invertible &&
      current->node_and_ancestors_are_flat) {
    transform->PreconcatTransform(FromScreen(current->id));
    if (dest)
      transform->PreconcatTransform(ToScreen(dest->id));
    return true;
  }

  // Inverting a flattening is not equivalent to flattening an inverse. This
  // means we cannot, for example, use the inverse of each node's to_parent
  // transform, flattening where needed. Instead, we must compute the transform
  // from the destination to the source, with flattening, and then invert the
  // result.
  gfx::Transform dest_to_source;
  CombineTransformsBetween(dest_id, source_id, &dest_to_source);
  gfx::Transform source_to_dest;
  bool all_are_invertible = dest_to_source.GetInverse(&source_to_dest);
  transform->PreconcatTransform(source_to_dest);
  return all_are_invertible;
}

void TransformTree::UpdateLocalTransform(TransformNode* node) {
  gfx::Transform transform = node->post_local;
  if (NeedsSourceToParentUpdate(node)) {
    gfx::Transform to_parent;
    ComputeTranslation(node->source_node_id, node->parent_id, &to_parent);
    gfx::Vector2dF unsnapping;
    TransformNode* current;
    TransformNode* parent_node;
    for (current = Node(node->source_node_id); current->id > node->parent_id;
         current = parent(current)) {
      unsnapping.Subtract(current->scroll_snap);
    }
    for (parent_node = Node(node->parent_id);
         parent_node->id > node->source_node_id;
         parent_node = parent(parent_node)) {
      unsnapping.Add(parent_node->scroll_snap);
    }
    // If a node NeedsSourceToParentUpdate, the node is either a fixed position
    // node or a scroll child.
    // If the node has a fixed position, the parent of the node is an ancestor
    // of source node, current->id should be equal to node->parent_id.
    // Otherwise, the node's source node is always an ancestor of the node owned
    // by the scroll parent, so parent_node->id should be equal to
    // node->source_node_id.
    DCHECK(current->id == node->parent_id ||
           parent_node->id == node->source_node_id);
    to_parent.Translate(unsnapping.x(), unsnapping.y());
    node->source_to_parent = to_parent.To2dTranslation();
  }

  gfx::Vector2dF fixed_position_adjustment;
  gfx::Vector2dF inner_viewport_bounds_delta =
      property_trees()->inner_viewport_container_bounds_delta();
  gfx::Vector2dF outer_viewport_bounds_delta =
      property_trees()->outer_viewport_container_bounds_delta();
  if (node->affected_by_inner_viewport_bounds_delta_x)
    fixed_position_adjustment.set_x(inner_viewport_bounds_delta.x());
  else if (node->affected_by_outer_viewport_bounds_delta_x)
    fixed_position_adjustment.set_x(outer_viewport_bounds_delta.x());

  if (node->affected_by_inner_viewport_bounds_delta_y)
    fixed_position_adjustment.set_y(inner_viewport_bounds_delta.y());
  else if (node->affected_by_outer_viewport_bounds_delta_y)
    fixed_position_adjustment.set_y(outer_viewport_bounds_delta.y());

  transform.Translate(node->source_to_parent.x() - node->scroll_offset.x() +
                          fixed_position_adjustment.x(),
                      node->source_to_parent.y() - node->scroll_offset.y() +
                          fixed_position_adjustment.y());
  transform.PreconcatTransform(node->local);
  transform.PreconcatTransform(node->pre_local);
  node->set_to_parent(transform);
  node->needs_local_transform_update = false;
}

void TransformTree::UpdateScreenSpaceTransform(TransformNode* node,
                                               TransformNode* parent_node,
                                               TransformNode* target_node) {
  if (!parent_node) {
    SetToScreen(node->id, node->to_parent);
    node->ancestors_are_invertible = true;
    node->to_screen_is_potentially_animated = false;
    node->node_and_ancestors_are_flat = node->to_parent.IsFlat();
  } else {
    gfx::Transform to_screen_space_transform = ToScreen(parent_node->id);
    if (node->flattens_inherited_transform)
      to_screen_space_transform.FlattenTo2d();
    to_screen_space_transform.PreconcatTransform(node->to_parent);
    node->ancestors_are_invertible = parent_node->ancestors_are_invertible;
    node->node_and_ancestors_are_flat =
        parent_node->node_and_ancestors_are_flat && node->to_parent.IsFlat();
    SetToScreen(node->id, to_screen_space_transform);
  }

  gfx::Transform from_screen;
  if (!ToScreen(node->id).GetInverse(&from_screen))
    node->ancestors_are_invertible = false;
  SetFromScreen(node->id, from_screen);
}

void TransformTree::UpdateSurfaceContentsScale(TransformNode* node) {
  // The surface contents scale depends on the screen space transform, so update
  // it too.
  if (!node->needs_surface_contents_scale) {
    node->surface_contents_scale = gfx::Vector2dF(1.0f, 1.0f);
    return;
  }

  float layer_scale_factor =
      device_scale_factor_ * device_transform_scale_factor_;
  if (node->in_subtree_of_page_scale_layer)
    layer_scale_factor *= page_scale_factor_;
  node->surface_contents_scale = MathUtil::ComputeTransform2dScaleComponents(
      ToScreen(node->id), layer_scale_factor);
}

void TransformTree::UpdateTargetSpaceTransform(TransformNode* node,
                                               TransformNode* target_node) {
  gfx::Transform target_space_transform;
  if (node->needs_surface_contents_scale) {
    target_space_transform.MakeIdentity();
    target_space_transform.Scale(node->surface_contents_scale.x(),
                                 node->surface_contents_scale.y());
  } else {
    // In order to include the root transform for the root surface, we walk up
    // to the root of the transform tree in ComputeTransform.
    int target_id = target_node->id;
    ComputeTransform(node->id, target_id, &target_space_transform);
    if (target_id != kRootNodeId) {
      target_space_transform.matrix().postScale(
          target_node->surface_contents_scale.x(),
          target_node->surface_contents_scale.y(), 1.f);
    }
  }

  gfx::Transform from_target;
  if (!target_space_transform.GetInverse(&from_target))
    node->ancestors_are_invertible = false;
  SetToTarget(node->id, target_space_transform);
  SetFromTarget(node->id, from_target);
}

void TransformTree::UpdateAnimationProperties(TransformNode* node,
                                              TransformNode* parent_node) {
  bool ancestor_is_animating = false;
  if (parent_node)
    ancestor_is_animating = parent_node->to_screen_is_potentially_animated;
  node->to_screen_is_potentially_animated =
      node->has_potential_animation || ancestor_is_animating;
}

void TransformTree::UndoSnapping(TransformNode* node) {
  // to_parent transform has the scroll snap from previous frame baked in.
  // We need to undo it and use the un-snapped transform to compute current
  // target and screen space transforms.
  node->to_parent.Translate(-node->scroll_snap.x(), -node->scroll_snap.y());
}

void TransformTree::UpdateSnapping(TransformNode* node) {
  if (!node->scrolls || node->to_screen_is_potentially_animated ||
      !ToScreen(node->id).IsScaleOrTranslation() ||
      !node->ancestors_are_invertible) {
    return;
  }

  // Scroll snapping must be done in screen space (the pixels we care about).
  // This means we effectively snap the screen space transform. If ST is the
  // screen space transform and ST' is ST with its translation components
  // rounded, then what we're after is the scroll delta X, where ST * X = ST'.
  // I.e., we want a transform that will realize our scroll snap. It follows
  // that X = ST^-1 * ST'. We cache ST and ST^-1 to make this more efficient.
  gfx::Transform rounded = ToScreen(node->id);
  rounded.RoundTranslationComponents();
  gfx::Transform delta = FromScreen(node->id);
  delta *= rounded;

  DCHECK(delta.IsApproximatelyIdentityOrTranslation(SkDoubleToMScalar(1e-4)))
      << delta.ToString();

  gfx::Vector2dF translation = delta.To2dTranslation();

  // Now that we have our scroll delta, we must apply it to each of our
  // combined, to/from matrices.
  SetToScreen(node->id, rounded);
  node->to_parent.Translate(translation.x(), translation.y());
  gfx::Transform from_screen = FromScreen(node->id);
  from_screen.matrix().postTranslate(-translation.x(), -translation.y(), 0);
  SetFromScreen(node->id, from_screen);
  node->scroll_snap = translation;
}

void TransformTree::UpdateTransformChanged(TransformNode* node,
                                           TransformNode* parent_node,
                                           TransformNode* source_node) {
  if (parent_node && parent_node->transform_changed) {
    node->transform_changed = true;
    return;
  }

  if (source_node && source_node->id != parent_node->id &&
      source_to_parent_updates_allowed_ && source_node->transform_changed)
    node->transform_changed = true;
}

void TransformTree::UpdateNodeAndAncestorsAreAnimatedOrInvertible(
    TransformNode* node,
    TransformNode* parent_node) {
  if (!parent_node) {
    node->node_and_ancestors_are_animated_or_invertible =
        node->has_potential_animation || node->is_invertible;
    return;
  }
  if (!parent_node->node_and_ancestors_are_animated_or_invertible) {
    node->node_and_ancestors_are_animated_or_invertible = false;
    return;
  }
  bool is_invertible = node->is_invertible;
  // Even when the current node's transform and the parent's screen space
  // transform are invertible, the current node's screen space transform can
  // become uninvertible due to floating-point arithmetic.
  if (!node->ancestors_are_invertible && parent_node->ancestors_are_invertible)
    is_invertible = false;
  node->node_and_ancestors_are_animated_or_invertible =
      node->has_potential_animation || is_invertible;
}

void TransformTree::SetDeviceTransform(const gfx::Transform& transform,
                                       gfx::PointF root_position) {
  gfx::Transform root_post_local = transform;
  TransformNode* node = Node(1);
  root_post_local.Scale(node->post_local_scale_factor,
                        node->post_local_scale_factor);
  root_post_local.Translate(root_position.x(), root_position.y());
  if (node->post_local == root_post_local)
    return;

  node->post_local = root_post_local;
  node->needs_local_transform_update = true;
  set_needs_update(true);
}

void TransformTree::SetDeviceTransformScaleFactor(
    const gfx::Transform& transform) {
  gfx::Vector2dF device_transform_scale_components =
      MathUtil::ComputeTransform2dScaleComponents(transform, 1.f);

  // Not handling the rare case of different x and y device scale.
  device_transform_scale_factor_ =
      std::max(device_transform_scale_components.x(),
               device_transform_scale_components.y());
}

void TransformTree::UpdateInnerViewportContainerBoundsDelta() {
  if (nodes_affected_by_inner_viewport_bounds_delta_.empty())
    return;

  set_needs_update(true);
  for (int i : nodes_affected_by_inner_viewport_bounds_delta_)
    Node(i)->needs_local_transform_update = true;
}

void TransformTree::UpdateOuterViewportContainerBoundsDelta() {
  if (nodes_affected_by_outer_viewport_bounds_delta_.empty())
    return;

  set_needs_update(true);
  for (int i : nodes_affected_by_outer_viewport_bounds_delta_)
    Node(i)->needs_local_transform_update = true;
}

void TransformTree::AddNodeAffectedByInnerViewportBoundsDelta(int node_id) {
  nodes_affected_by_inner_viewport_bounds_delta_.push_back(node_id);
}

void TransformTree::AddNodeAffectedByOuterViewportBoundsDelta(int node_id) {
  nodes_affected_by_outer_viewport_bounds_delta_.push_back(node_id);
}

bool TransformTree::HasNodesAffectedByInnerViewportBoundsDelta() const {
  return !nodes_affected_by_inner_viewport_bounds_delta_.empty();
}

bool TransformTree::HasNodesAffectedByOuterViewportBoundsDelta() const {
  return !nodes_affected_by_outer_viewport_bounds_delta_.empty();
}

const gfx::Transform& TransformTree::FromTarget(int node_id,
                                                int effect_id) const {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  if (effect_id != kInvalidNodeId &&
      property_trees()->verify_transform_tree_calculations) {
    const gfx::Transform& transform =
        property_trees()->GetDrawTransforms(node_id, effect_id).from_target;
    CHECK(transform.ApproximatelyEqual(cached_data_[node_id].from_target));
  }
  return cached_data_[node_id].from_target;
}

void TransformTree::SetFromTarget(int node_id,
                                  const gfx::Transform& transform) {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  cached_data_[node_id].from_target = transform;
}

const gfx::Transform& TransformTree::ToTarget(int node_id,
                                              int effect_id) const {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  if (effect_id != kInvalidNodeId &&
      property_trees()->verify_transform_tree_calculations) {
    const gfx::Transform& transform =
        property_trees()->GetDrawTransforms(node_id, effect_id).to_target;
    if (property_trees()->non_root_surfaces_enabled)
      CHECK(transform.ApproximatelyEqual(cached_data_[node_id].to_target));
    else
      CHECK(transform.ApproximatelyEqual(cached_data_[node_id].to_screen));
  }
  return cached_data_[node_id].to_target;
}

void TransformTree::SetToTarget(int node_id, const gfx::Transform& transform) {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  cached_data_[node_id].to_target = transform;
}

const gfx::Transform& TransformTree::FromScreen(int node_id) const {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  return cached_data_[node_id].from_screen;
}

void TransformTree::SetFromScreen(int node_id,
                                  const gfx::Transform& transform) {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  cached_data_[node_id].from_screen = transform;
}

const gfx::Transform& TransformTree::ToScreen(int node_id) const {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  return cached_data_[node_id].to_screen;
}

void TransformTree::SetToScreen(int node_id, const gfx::Transform& transform) {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  cached_data_[node_id].to_screen = transform;
}

int TransformTree::TargetId(int node_id) const {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  return cached_data_[node_id].target_id;
}

void TransformTree::SetTargetId(int node_id, int target_id) {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  cached_data_[node_id].target_id = target_id;
}

int TransformTree::ContentTargetId(int node_id) const {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  return cached_data_[node_id].content_target_id;
}

void TransformTree::SetContentTargetId(int node_id, int content_target_id) {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  cached_data_[node_id].content_target_id = content_target_id;
}

bool TransformTree::operator==(const TransformTree& other) const {
  return PropertyTree::operator==(other) &&
         source_to_parent_updates_allowed_ ==
             other.source_to_parent_updates_allowed() &&
         page_scale_factor_ == other.page_scale_factor() &&
         device_scale_factor_ == other.device_scale_factor() &&
         device_transform_scale_factor_ ==
             other.device_transform_scale_factor() &&
         nodes_affected_by_inner_viewport_bounds_delta_ ==
             other.nodes_affected_by_inner_viewport_bounds_delta() &&
         nodes_affected_by_outer_viewport_bounds_delta_ ==
             other.nodes_affected_by_outer_viewport_bounds_delta() &&
         cached_data_ == other.cached_data();
}

void TransformTree::ToProtobuf(proto::PropertyTree* proto) const {
  DCHECK(!proto->has_property_type());
  proto->set_property_type(proto::PropertyTree::Transform);

  PropertyTree::ToProtobuf(proto);
  proto::TransformTreeData* data = proto->mutable_transform_tree_data();

  data->set_source_to_parent_updates_allowed(source_to_parent_updates_allowed_);
  data->set_page_scale_factor(page_scale_factor_);
  data->set_device_scale_factor(device_scale_factor_);
  data->set_device_transform_scale_factor(device_transform_scale_factor_);

  for (auto i : nodes_affected_by_inner_viewport_bounds_delta_)
    data->add_nodes_affected_by_inner_viewport_bounds_delta(i);

  for (auto i : nodes_affected_by_outer_viewport_bounds_delta_)
    data->add_nodes_affected_by_outer_viewport_bounds_delta(i);

  for (int i = 0; i < static_cast<int>(cached_data_.size()); ++i)
    cached_data_[i].ToProtobuf(data->add_cached_data());
}

void TransformTree::FromProtobuf(
    const proto::PropertyTree& proto,
    std::unordered_map<int, int>* node_id_to_index_map) {
  DCHECK(proto.has_property_type());
  DCHECK_EQ(proto.property_type(), proto::PropertyTree::Transform);

  PropertyTree::FromProtobuf(proto, node_id_to_index_map);
  const proto::TransformTreeData& data = proto.transform_tree_data();

  source_to_parent_updates_allowed_ = data.source_to_parent_updates_allowed();
  page_scale_factor_ = data.page_scale_factor();
  device_scale_factor_ = data.device_scale_factor();
  device_transform_scale_factor_ = data.device_transform_scale_factor();

  DCHECK(nodes_affected_by_inner_viewport_bounds_delta_.empty());
  for (int i = 0; i < data.nodes_affected_by_inner_viewport_bounds_delta_size();
       ++i) {
    nodes_affected_by_inner_viewport_bounds_delta_.push_back(
        data.nodes_affected_by_inner_viewport_bounds_delta(i));
  }

  DCHECK(nodes_affected_by_outer_viewport_bounds_delta_.empty());
  for (int i = 0; i < data.nodes_affected_by_outer_viewport_bounds_delta_size();
       ++i) {
    nodes_affected_by_outer_viewport_bounds_delta_.push_back(
        data.nodes_affected_by_outer_viewport_bounds_delta(i));
  }

  DCHECK_EQ(static_cast<int>(cached_data_.size()), 1);
  cached_data_.back().FromProtobuf(data.cached_data(0));
  for (int i = 1; i < data.cached_data_size(); ++i) {
    cached_data_.push_back(TransformCachedNodeData());
    cached_data_.back().FromProtobuf(data.cached_data(i));
  }
}

EffectTree::EffectTree() {}

EffectTree::~EffectTree() {}

void EffectTree::clear() {
  PropertyTree<EffectNode>::clear();
  mask_replica_layer_ids_.clear();

#if DCHECK_IS_ON()
  EffectTree tree;
  DCHECK(tree == *this);
#endif
}

float EffectTree::EffectiveOpacity(const EffectNode* node) const {
  return node->subtree_hidden ? 0.f : node->opacity;
}

void EffectTree::UpdateOpacities(EffectNode* node, EffectNode* parent_node) {
  node->screen_space_opacity = EffectiveOpacity(node);

  if (parent_node)
    node->screen_space_opacity *= parent_node->screen_space_opacity;
}

void EffectTree::UpdateIsDrawn(EffectNode* node, EffectNode* parent_node) {
  // Nodes that have screen space opacity 0 are hidden. So they are not drawn.
  // Exceptions:
  // 1) Nodes that contribute to copy requests, whether hidden or not, must be
  //    drawn.
  // 2) Nodes that have a background filter.
  // 3) Nodes with animating screen space opacity on main thread or pending tree
  //    are drawn if their parent is drawn irrespective of their opacity.
  if (node->has_copy_request)
    node->is_drawn = true;
  else if (EffectiveOpacity(node) == 0.f &&
           (!node->has_potential_opacity_animation ||
            property_trees()->is_active) &&
           node->background_filters.IsEmpty())
    node->is_drawn = false;
  else if (parent_node)
    node->is_drawn = parent_node->is_drawn;
  else
    node->is_drawn = true;
}

void EffectTree::UpdateEffectChanged(EffectNode* node,
                                     EffectNode* parent_node) {
  if (parent_node && parent_node->effect_changed) {
    node->effect_changed = true;
  }
}

void EffectTree::UpdateBackfaceVisibility(EffectNode* node,
                                          EffectNode* parent_node) {
  if (!parent_node) {
    node->hidden_by_backface_visibility = false;
    return;
  }
  if (parent_node->hidden_by_backface_visibility) {
    node->hidden_by_backface_visibility = true;
    return;
  }

  TransformTree& transform_tree = property_trees()->transform_tree;
  if (node->has_render_surface && !node->double_sided) {
    TransformNode* transform_node = transform_tree.Node(node->transform_id);
    if (transform_node->is_invertible &&
        transform_node->ancestors_are_invertible) {
      if (transform_node->sorting_context_id) {
        const TransformNode* parent_transform_node =
            transform_tree.parent(transform_node);
        if (parent_transform_node &&
            parent_transform_node->sorting_context_id ==
                transform_node->sorting_context_id) {
          gfx::Transform surface_draw_transform;
          property_trees()->ComputeTransformToTarget(
              transform_node->id, node->target_id, &surface_draw_transform);
          node->hidden_by_backface_visibility =
              surface_draw_transform.IsBackFaceVisible();
        } else {
          node->hidden_by_backface_visibility =
              transform_node->local.IsBackFaceVisible();
        }
        return;
      }
    }
  }
  node->hidden_by_backface_visibility = false;
}

void EffectTree::UpdateSurfaceContentsScale(EffectNode* effect_node) {
  if (!effect_node->has_render_surface ||
      effect_node->transform_id == kRootNodeId) {
    effect_node->surface_contents_scale = gfx::Vector2dF(1.0f, 1.0f);
    return;
  }

  TransformTree& transform_tree = property_trees()->transform_tree;
  float layer_scale_factor = transform_tree.device_scale_factor() *
                             transform_tree.device_transform_scale_factor();
  TransformNode* transform_node =
      transform_tree.Node(effect_node->transform_id);
  if (transform_node->in_subtree_of_page_scale_layer)
    layer_scale_factor *= transform_tree.page_scale_factor();
  effect_node->surface_contents_scale =
      MathUtil::ComputeTransform2dScaleComponents(
          transform_tree.ToScreen(transform_node->id), layer_scale_factor);
}

void EffectTree::OnOpacityAnimated(float opacity,
                                   int id,
                                   LayerTreeImpl* layer_tree_impl) {
  EffectNode* node = Node(id);
  layer_tree_impl->AddToOpacityAnimationsMap(node->owner_id, opacity);
  if (node->opacity == opacity)
    return;
  node->opacity = opacity;
  node->effect_changed = true;
  property_trees()->changed = true;
  property_trees()->effect_tree.set_needs_update(true);
  layer_tree_impl->set_needs_update_draw_properties();
}

void EffectTree::OnFilterAnimated(const FilterOperations& filters,
                                  int id,
                                  LayerTreeImpl* layer_tree_impl) {
  EffectNode* node = Node(id);
  layer_tree_impl->AddToFilterAnimationsMap(node->owner_id, filters);
  if (node->filters == filters)
    return;
  node->filters = filters;
  node->effect_changed = true;
  property_trees()->changed = true;
  property_trees()->effect_tree.set_needs_update(true);
  layer_tree_impl->set_needs_update_draw_properties();
}

void EffectTree::UpdateEffects(int id) {
  EffectNode* node = Node(id);
  EffectNode* parent_node = parent(node);

  UpdateOpacities(node, parent_node);
  UpdateIsDrawn(node, parent_node);
  UpdateEffectChanged(node, parent_node);
  UpdateBackfaceVisibility(node, parent_node);
  UpdateSurfaceContentsScale(node);
}

void EffectTree::AddCopyRequest(int node_id,
                                std::unique_ptr<CopyOutputRequest> request) {
  copy_requests_.insert(std::make_pair(node_id, std::move(request)));
}

void EffectTree::PushCopyRequestsTo(EffectTree* other_tree) {
  // If other_tree still has copy requests, this means there was a commit
  // without a draw. This only happens in some edge cases during lost context or
  // visibility changes, so don't try to handle preserving these output
  // requests.
  if (!other_tree->copy_requests_.empty()) {
    // Destroying these copy requests will abort them.
    other_tree->copy_requests_.clear();
  }

  if (copy_requests_.empty())
    return;

  for (auto& request : copy_requests_) {
    other_tree->copy_requests_.insert(
        std::make_pair(request.first, std::move(request.second)));
  }
  copy_requests_.clear();

  // Property trees need to get rebuilt since effect nodes (and render surfaces)
  // that were created only for the copy requests we just pushed are no longer
  // needed.
  if (property_trees()->is_main_thread)
    property_trees()->needs_rebuild = true;
}

void EffectTree::TakeCopyRequestsAndTransformToSurface(
    int node_id,
    std::vector<std::unique_ptr<CopyOutputRequest>>* requests) {
  EffectNode* effect_node = Node(node_id);
  DCHECK(effect_node->has_render_surface);
  DCHECK(effect_node->has_copy_request);

  auto range = copy_requests_.equal_range(node_id);
  for (auto it = range.first; it != range.second; ++it)
    requests->push_back(std::move(it->second));
  copy_requests_.erase(range.first, range.second);

  for (auto& it : *requests) {
    if (!it->has_area())
      continue;

    // The area needs to be transformed from the space of content that draws to
    // the surface to the space of the surface itself.
    int destination_id = effect_node->transform_id;
    int source_id;
    if (effect_node->parent_id != -1) {
      // For non-root surfaces, transform only by sub-layer scale.
      source_id = destination_id;
    } else {
      // The root surface doesn't have the notion of sub-layer scale, but
      // instead has a similar notion of transforming from the space of the root
      // layer to the space of the screen.
      DCHECK_EQ(kRootNodeId, destination_id);
      source_id = TransformTree::kContentsRootNodeId;
    }
    gfx::Transform transform;
    property_trees()->transform_tree.ComputeTransform(source_id, destination_id,
                                                      &transform);
    if (effect_node->id != kContentsRootNodeId) {
      transform.matrix().postScale(effect_node->surface_contents_scale.x(),
                                   effect_node->surface_contents_scale.y(),
                                   1.f);
    }
    it->set_area(MathUtil::MapEnclosingClippedRect(transform, it->area()));
  }
}

bool EffectTree::HasCopyRequests() const {
  return !copy_requests_.empty();
}

void EffectTree::ClearCopyRequests() {
  for (auto& node : nodes()) {
    node.num_copy_requests_in_subtree = 0;
    node.has_copy_request = false;
  }

  // Any copy requests that are still left will be aborted (sending an empty
  // result) on destruction.
  copy_requests_.clear();
  set_needs_update(true);
}

int EffectTree::ClosestAncestorWithCopyRequest(int id) const {
  DCHECK_GE(id, 0);
  const EffectNode* node = Node(id);
  while (node->id > 1) {
    if (node->has_copy_request)
      return node->id;

    node = parent(node);
  }

  if (node->has_copy_request)
    return node->id;
  else
    return -1;
}

void EffectTree::AddMaskOrReplicaLayerId(int id) {
  mask_replica_layer_ids_.push_back(id);
}

bool EffectTree::ContributesToDrawnSurface(int id) {
  // All drawn nodes contribute to drawn surface.
  // Exception : Nodes that are hidden and are drawn only for the sake of
  // copy requests.
  EffectNode* node = Node(id);
  EffectNode* parent_node = parent(node);
  return node->is_drawn && (!parent_node || parent_node->is_drawn);
}

void EffectTree::ResetChangeTracking() {
  for (int id = 1; id < static_cast<int>(size()); ++id) {
    EffectNode* node = Node(id);
    node->effect_changed = false;
  }
}

void TransformTree::UpdateNodeAndAncestorsHaveIntegerTranslations(
    TransformNode* node,
    TransformNode* parent_node) {
  node->node_and_ancestors_have_only_integer_translation =
      node->to_parent.IsIdentityOrIntegerTranslation();
  if (parent_node)
    node->node_and_ancestors_have_only_integer_translation =
        node->node_and_ancestors_have_only_integer_translation &&
        parent_node->node_and_ancestors_have_only_integer_translation;
}

void ClipTree::SetViewportClip(gfx::RectF viewport_rect) {
  if (size() < 2)
    return;
  ClipNode* node = Node(1);
  if (viewport_rect == node->clip)
    return;
  node->clip = viewport_rect;
  set_needs_update(true);
}

gfx::RectF ClipTree::ViewportClip() {
  const unsigned long min_size = 1;
  DCHECK_GT(size(), min_size);
  return Node(kViewportNodeId)->clip;
}

bool ClipTree::operator==(const ClipTree& other) const {
  return PropertyTree::operator==(other);
}

void ClipTree::ToProtobuf(proto::PropertyTree* proto) const {
  DCHECK(!proto->has_property_type());
  proto->set_property_type(proto::PropertyTree::Clip);

  PropertyTree::ToProtobuf(proto);
}

void ClipTree::FromProtobuf(
    const proto::PropertyTree& proto,
    std::unordered_map<int, int>* node_id_to_index_map) {
  DCHECK(proto.has_property_type());
  DCHECK_EQ(proto.property_type(), proto::PropertyTree::Clip);

  PropertyTree::FromProtobuf(proto, node_id_to_index_map);
}

EffectTree& EffectTree::operator=(const EffectTree& from) {
  PropertyTree::operator=(from);
  mask_replica_layer_ids_ = from.mask_replica_layer_ids_;
  // copy_requests_ are omitted here, since these need to be moved rather
  // than copied or assigned.
  return *this;
}

bool EffectTree::operator==(const EffectTree& other) const {
  return PropertyTree::operator==(other) &&
         mask_replica_layer_ids_ == other.mask_replica_layer_ids_;
}

void EffectTree::ToProtobuf(proto::PropertyTree* proto) const {
  DCHECK(!proto->has_property_type());
  proto->set_property_type(proto::PropertyTree::Effect);

  PropertyTree::ToProtobuf(proto);
  proto::EffectTreeData* data = proto->mutable_effect_tree_data();

  for (auto i : mask_replica_layer_ids_)
    data->add_mask_replica_layer_ids(i);
}

void EffectTree::FromProtobuf(
    const proto::PropertyTree& proto,
    std::unordered_map<int, int>* node_id_to_index_map) {
  DCHECK(proto.has_property_type());
  DCHECK_EQ(proto.property_type(), proto::PropertyTree::Effect);

  PropertyTree::FromProtobuf(proto, node_id_to_index_map);
  const proto::EffectTreeData& data = proto.effect_tree_data();

  DCHECK(mask_replica_layer_ids_.empty());
  for (int i = 0; i < data.mask_replica_layer_ids_size(); ++i) {
    mask_replica_layer_ids_.push_back(data.mask_replica_layer_ids(i));
  }
}

ScrollTree::ScrollTree()
    : currently_scrolling_node_id_(kInvalidNodeId),
      layer_id_to_scroll_offset_map_(ScrollTree::ScrollOffsetMap()) {}

ScrollTree::~ScrollTree() {}

ScrollTree& ScrollTree::operator=(const ScrollTree& from) {
  PropertyTree::operator=(from);
  currently_scrolling_node_id_ = kInvalidNodeId;
  // layer_id_to_scroll_offset_map_ is intentionally omitted in operator=,
  // because we do not want to simply copy the map when property tree is
  // propagating from pending to active.
  // In the main to pending case, we do want to copy it, but this can be done by
  // calling UpdateScrollOffsetMap after the assignment;
  // In the other case, we want pending and active property trees to share the
  // same map.
  return *this;
}

bool ScrollTree::operator==(const ScrollTree& other) const {
  const ScrollTree::ScrollOffsetMap& other_scroll_offset_map =
      other.scroll_offset_map();
  if (layer_id_to_scroll_offset_map_.size() != other_scroll_offset_map.size())
    return false;

  for (auto map_entry : layer_id_to_scroll_offset_map_) {
    int key = map_entry.first;
    if (other_scroll_offset_map.find(key) == other_scroll_offset_map.end() ||
        map_entry.second != layer_id_to_scroll_offset_map_.at(key))
      return false;
  }

  bool is_currently_scrolling_node_equal =
      currently_scrolling_node_id_ == other.currently_scrolling_node_id_;

  return PropertyTree::operator==(other) && is_currently_scrolling_node_equal;
}

void ScrollTree::ToProtobuf(proto::PropertyTree* proto) const {
  DCHECK(!proto->has_property_type());
  proto->set_property_type(proto::PropertyTree::Scroll);

  PropertyTree::ToProtobuf(proto);
  proto::ScrollTreeData* data = proto->mutable_scroll_tree_data();

  data->set_currently_scrolling_node_id(currently_scrolling_node_id_);
  for (auto i : layer_id_to_scroll_offset_map_) {
    data->add_layer_id_to_scroll_offset_map();
    proto::ScrollOffsetMapEntry* entry =
        data->mutable_layer_id_to_scroll_offset_map(
            data->layer_id_to_scroll_offset_map_size() - 1);
    entry->set_layer_id(i.first);
    SyncedScrollOffsetToProto(*i.second.get(), entry->mutable_scroll_offset());
  }
}

void ScrollTree::FromProtobuf(
    const proto::PropertyTree& proto,
    std::unordered_map<int, int>* node_id_to_index_map) {
  DCHECK(proto.has_property_type());
  DCHECK_EQ(proto.property_type(), proto::PropertyTree::Scroll);

  PropertyTree::FromProtobuf(proto, node_id_to_index_map);
  const proto::ScrollTreeData& data = proto.scroll_tree_data();

  currently_scrolling_node_id_ = data.currently_scrolling_node_id();

  // TODO(khushalsagar): This should probably be removed if the copy constructor
  // for ScrollTree copies the |layer_id_to_scroll_offset_map_| as well.
  layer_id_to_scroll_offset_map_.clear();
  for (int i = 0; i < data.layer_id_to_scroll_offset_map_size(); ++i) {
    const proto::ScrollOffsetMapEntry entry =
        data.layer_id_to_scroll_offset_map(i);
    layer_id_to_scroll_offset_map_[entry.layer_id()] = new SyncedScrollOffset();
    ProtoToSyncedScrollOffset(
        entry.scroll_offset(),
        layer_id_to_scroll_offset_map_[entry.layer_id()].get());
  }
}

void ScrollTree::clear() {
  PropertyTree<ScrollNode>::clear();

  if (property_trees()->is_main_thread) {
    currently_scrolling_node_id_ = kInvalidNodeId;
    layer_id_to_scroll_offset_map_.clear();
  }

#if DCHECK_IS_ON()
  ScrollTree tree;
  if (!property_trees()->is_main_thread) {
    tree.currently_scrolling_node_id_ = currently_scrolling_node_id_;
    tree.layer_id_to_scroll_offset_map_ = layer_id_to_scroll_offset_map_;
  }
  DCHECK(tree == *this);
#endif
}

gfx::ScrollOffset ScrollTree::MaxScrollOffset(int scroll_node_id) const {
  const ScrollNode* scroll_node = Node(scroll_node_id);
  gfx::SizeF scroll_bounds =
      gfx::SizeF(scroll_node->bounds.width(), scroll_node->bounds.height());

  if (scroll_node->is_inner_viewport_scroll_layer) {
    scroll_bounds.Enlarge(
        property_trees()->inner_viewport_scroll_bounds_delta().x(),
        property_trees()->inner_viewport_scroll_bounds_delta().y());
  }

  if (!scroll_node->scrollable || scroll_bounds.IsEmpty())
    return gfx::ScrollOffset();

  TransformTree& transform_tree = property_trees()->transform_tree;
  float scale_factor = 1.f;
  if (scroll_node->max_scroll_offset_affected_by_page_scale)
    scale_factor = transform_tree.page_scale_factor();

  gfx::SizeF scaled_scroll_bounds = gfx::ScaleSize(scroll_bounds, scale_factor);
  scaled_scroll_bounds.SetSize(std::floor(scaled_scroll_bounds.width()),
                               std::floor(scaled_scroll_bounds.height()));

  gfx::Size clip_layer_bounds = scroll_clip_layer_bounds(scroll_node->id);

  gfx::ScrollOffset max_offset(
      scaled_scroll_bounds.width() - clip_layer_bounds.width(),
      scaled_scroll_bounds.height() - clip_layer_bounds.height());

  max_offset.Scale(1 / scale_factor);
  max_offset.SetToMax(gfx::ScrollOffset());
  return max_offset;
}

gfx::Size ScrollTree::scroll_clip_layer_bounds(int scroll_node_id) const {
  const ScrollNode* scroll_node = Node(scroll_node_id);
  gfx::Size scroll_clip_layer_bounds = scroll_node->scroll_clip_layer_bounds;

  gfx::Vector2dF scroll_clip_layer_bounds_delta;
  if (scroll_node->is_inner_viewport_scroll_layer) {
    scroll_clip_layer_bounds_delta.Add(
        property_trees()->inner_viewport_container_bounds_delta());
  } else if (scroll_node->is_outer_viewport_scroll_layer) {
    scroll_clip_layer_bounds_delta.Add(
        property_trees()->outer_viewport_container_bounds_delta());
  }

  gfx::Vector2d delta = gfx::ToCeiledVector2d(scroll_clip_layer_bounds_delta);
  scroll_clip_layer_bounds.SetSize(
      scroll_clip_layer_bounds.width() + delta.x(),
      scroll_clip_layer_bounds.height() + delta.y());

  return scroll_clip_layer_bounds;
}

ScrollNode* ScrollTree::CurrentlyScrollingNode() {
  ScrollNode* scroll_node = Node(currently_scrolling_node_id_);
  return scroll_node;
}

const ScrollNode* ScrollTree::CurrentlyScrollingNode() const {
  const ScrollNode* scroll_node = Node(currently_scrolling_node_id_);
  return scroll_node;
}

#if DCHECK_IS_ON()
int ScrollTree::CurrentlyScrollingNodeId() const {
  return currently_scrolling_node_id_;
}
#endif

void ScrollTree::set_currently_scrolling_node(int scroll_node_id) {
  currently_scrolling_node_id_ = scroll_node_id;
}

gfx::Transform ScrollTree::ScreenSpaceTransform(int scroll_node_id) const {
  const ScrollNode* scroll_node = Node(scroll_node_id);
  const TransformTree& transform_tree = property_trees()->transform_tree;
  const TransformNode* transform_node =
      transform_tree.Node(scroll_node->transform_id);
  gfx::Transform screen_space_transform(
      1, 0, 0, 1, scroll_node->offset_to_transform_parent.x(),
      scroll_node->offset_to_transform_parent.y());
  screen_space_transform.ConcatTransform(
      transform_tree.ToScreen(transform_node->id));
  if (scroll_node->should_flatten)
    screen_space_transform.FlattenTo2d();
  return screen_space_transform;
}

SyncedScrollOffset* ScrollTree::synced_scroll_offset(int layer_id) {
  if (layer_id_to_scroll_offset_map_.find(layer_id) ==
      layer_id_to_scroll_offset_map_.end()) {
    layer_id_to_scroll_offset_map_[layer_id] = new SyncedScrollOffset;
  }
  return layer_id_to_scroll_offset_map_[layer_id].get();
}

const SyncedScrollOffset* ScrollTree::synced_scroll_offset(int layer_id) const {
  if (layer_id_to_scroll_offset_map_.find(layer_id) ==
      layer_id_to_scroll_offset_map_.end()) {
    return nullptr;
  }
  return layer_id_to_scroll_offset_map_.at(layer_id).get();
}

const gfx::ScrollOffset ScrollTree::current_scroll_offset(int layer_id) const {
  return synced_scroll_offset(layer_id)
             ? synced_scroll_offset(layer_id)->Current(
                   property_trees()->is_active)
             : gfx::ScrollOffset();
}

gfx::ScrollOffset ScrollTree::PullDeltaForMainThread(
    SyncedScrollOffset* scroll_offset) {
  // TODO(miletus): Remove all this temporary flooring machinery when
  // Blink fully supports fractional scrolls.
  gfx::ScrollOffset current_offset =
      scroll_offset->Current(property_trees()->is_active);
  gfx::ScrollOffset current_delta = property_trees()->is_active
                                        ? scroll_offset->Delta()
                                        : scroll_offset->PendingDelta().get();
  gfx::ScrollOffset floored_delta(floor(current_delta.x()),
                                  floor(current_delta.y()));
  gfx::ScrollOffset diff_delta = floored_delta - current_delta;
  gfx::ScrollOffset tmp_offset = current_offset + diff_delta;
  scroll_offset->SetCurrent(tmp_offset);
  gfx::ScrollOffset delta = scroll_offset->PullDeltaForMainThread();
  scroll_offset->SetCurrent(current_offset);
  return delta;
}

void ScrollTree::CollectScrollDeltas(ScrollAndScaleSet* scroll_info,
                                     int inner_viewport_layer_id) {
  for (auto map_entry : layer_id_to_scroll_offset_map_) {
    gfx::ScrollOffset scroll_delta =
        PullDeltaForMainThread(map_entry.second.get());

    gfx::Vector2d scroll_delta_vector(scroll_delta.x(), scroll_delta.y());
    int layer_id = map_entry.first;

    if (!scroll_delta.IsZero()) {
      if (layer_id == inner_viewport_layer_id) {
        // Inner (visual) viewport is stored separately.
        scroll_info->inner_viewport_scroll.layer_id = layer_id;
        scroll_info->inner_viewport_scroll.scroll_delta = scroll_delta_vector;
      } else {
        LayerTreeHostCommon::ScrollUpdateInfo scroll;
        scroll.layer_id = layer_id;
        scroll.scroll_delta = scroll_delta_vector;
        scroll_info->scrolls.push_back(scroll);
      }
    }
  }
}

void ScrollTree::CollectScrollDeltasForTesting() {
  for (auto map_entry : layer_id_to_scroll_offset_map_) {
    PullDeltaForMainThread(map_entry.second.get());
  }
}

void ScrollTree::UpdateScrollOffsetMapEntry(
    int key,
    ScrollTree::ScrollOffsetMap* new_scroll_offset_map,
    LayerTreeImpl* layer_tree_impl) {
  bool changed = false;
  // If we are pushing scroll offset from main to pending tree, we create a new
  // instance of synced scroll offset; if we are pushing from pending to active,
  // we reuse the pending tree's value in the map.
  if (!property_trees()->is_active) {
    changed = synced_scroll_offset(key)->PushFromMainThread(
        new_scroll_offset_map->at(key)->PendingBase());

    if (new_scroll_offset_map->at(key)->clobber_active_value()) {
      synced_scroll_offset(key)->set_clobber_active_value();
    }
    if (changed) {
      layer_tree_impl->DidUpdateScrollOffset(key, kInvalidNodeId);
    }
  } else {
    layer_id_to_scroll_offset_map_[key] = new_scroll_offset_map->at(key);
    changed |= synced_scroll_offset(key)->PushPendingToActive();
    if (changed) {
      layer_tree_impl->DidUpdateScrollOffset(key, kInvalidNodeId);
    }
  }
}

void ScrollTree::UpdateScrollOffsetMap(
    ScrollTree::ScrollOffsetMap* new_scroll_offset_map,
    LayerTreeImpl* layer_tree_impl) {
  if (layer_tree_impl && !layer_tree_impl->LayerListIsEmpty()) {
    DCHECK(!property_trees()->is_main_thread);
    for (auto map_entry = layer_id_to_scroll_offset_map_.begin();
         map_entry != layer_id_to_scroll_offset_map_.end();) {
      int key = map_entry->first;
      if (new_scroll_offset_map->find(key) != new_scroll_offset_map->end()) {
        UpdateScrollOffsetMapEntry(key, new_scroll_offset_map, layer_tree_impl);
        ++map_entry;
      } else {
        map_entry = layer_id_to_scroll_offset_map_.erase(map_entry);
      }
    }

    for (auto& map_entry : *new_scroll_offset_map) {
      int key = map_entry.first;
      if (layer_id_to_scroll_offset_map_.find(key) ==
          layer_id_to_scroll_offset_map_.end())
        UpdateScrollOffsetMapEntry(key, new_scroll_offset_map, layer_tree_impl);
    }
  }
}

ScrollTree::ScrollOffsetMap& ScrollTree::scroll_offset_map() {
  return layer_id_to_scroll_offset_map_;
}

const ScrollTree::ScrollOffsetMap& ScrollTree::scroll_offset_map() const {
  return layer_id_to_scroll_offset_map_;
}

void ScrollTree::ApplySentScrollDeltasFromAbortedCommit() {
  DCHECK(property_trees()->is_active);
  for (auto& map_entry : layer_id_to_scroll_offset_map_)
    map_entry.second->AbortCommit();
}

bool ScrollTree::SetBaseScrollOffset(int layer_id,
                                     const gfx::ScrollOffset& scroll_offset) {
  return synced_scroll_offset(layer_id)->PushFromMainThread(scroll_offset);
}

bool ScrollTree::SetScrollOffset(int layer_id,
                                 const gfx::ScrollOffset& scroll_offset) {
  if (property_trees()->is_main_thread)
    return synced_scroll_offset(layer_id)->PushFromMainThread(scroll_offset);
  else if (property_trees()->is_active)
    return synced_scroll_offset(layer_id)->SetCurrent(scroll_offset);
  return false;
}

bool ScrollTree::UpdateScrollOffsetBaseForTesting(
    int layer_id,
    const gfx::ScrollOffset& offset) {
  DCHECK(!property_trees()->is_main_thread);
  bool changed = synced_scroll_offset(layer_id)->PushFromMainThread(offset);
  if (property_trees()->is_active)
    changed |= synced_scroll_offset(layer_id)->PushPendingToActive();
  return changed;
}

bool ScrollTree::SetScrollOffsetDeltaForTesting(int layer_id,
                                                const gfx::Vector2dF& delta) {
  return synced_scroll_offset(layer_id)->SetCurrent(
      synced_scroll_offset(layer_id)->ActiveBase() + gfx::ScrollOffset(delta));
}

const gfx::ScrollOffset ScrollTree::GetScrollOffsetBaseForTesting(
    int layer_id) const {
  DCHECK(!property_trees()->is_main_thread);
  if (synced_scroll_offset(layer_id))
    return property_trees()->is_active
               ? synced_scroll_offset(layer_id)->ActiveBase()
               : synced_scroll_offset(layer_id)->PendingBase();
  else
    return gfx::ScrollOffset();
}

const gfx::ScrollOffset ScrollTree::GetScrollOffsetDeltaForTesting(
    int layer_id) const {
  DCHECK(!property_trees()->is_main_thread);
  if (synced_scroll_offset(layer_id))
    return property_trees()->is_active
               ? synced_scroll_offset(layer_id)->Delta()
               : synced_scroll_offset(layer_id)->PendingDelta().get();
  else
    return gfx::ScrollOffset();
}

void ScrollTree::DistributeScroll(ScrollNode* scroll_node,
                                  ScrollState* scroll_state) {
  DCHECK(scroll_node && scroll_state);
  if (scroll_state->FullyConsumed())
    return;
  scroll_state->DistributeToScrollChainDescendant();

  // If the scroll doesn't propagate, and we're currently scrolling
  // a node other than this one, prevent the scroll from
  // propagating to this node.
  if (!scroll_state->should_propagate() &&
      scroll_state->delta_consumed_for_scroll_sequence() &&
      scroll_state->current_native_scrolling_node()->id != scroll_node->id) {
    return;
  }

  scroll_state->layer_tree_impl()->ApplyScroll(scroll_node, scroll_state);
}

gfx::Vector2dF ScrollTree::ScrollBy(ScrollNode* scroll_node,
                                    const gfx::Vector2dF& scroll,
                                    LayerTreeImpl* layer_tree_impl) {
  gfx::ScrollOffset adjusted_scroll(scroll);
  if (!scroll_node->user_scrollable_horizontal)
    adjusted_scroll.set_x(0);
  if (!scroll_node->user_scrollable_vertical)
    adjusted_scroll.set_y(0);
  DCHECK(scroll_node->scrollable);
  gfx::ScrollOffset old_offset = current_scroll_offset(scroll_node->owner_id);
  gfx::ScrollOffset new_offset =
      ClampScrollOffsetToLimits(old_offset + adjusted_scroll, scroll_node);
  if (SetScrollOffset(scroll_node->owner_id, new_offset))
    layer_tree_impl->DidUpdateScrollOffset(scroll_node->owner_id,
                                           scroll_node->transform_id);

  gfx::ScrollOffset unscrolled =
      old_offset + gfx::ScrollOffset(scroll) - new_offset;
  return gfx::Vector2dF(unscrolled.x(), unscrolled.y());
}

gfx::ScrollOffset ScrollTree::ClampScrollOffsetToLimits(
    gfx::ScrollOffset offset,
    ScrollNode* scroll_node) const {
  offset.SetToMin(MaxScrollOffset(scroll_node->id));
  offset.SetToMax(gfx::ScrollOffset());
  return offset;
}

PropertyTreesCachedData::PropertyTreesCachedData()
    : property_tree_update_number(0) {
  animation_scales.clear();
}

PropertyTreesCachedData::~PropertyTreesCachedData() {}

PropertyTrees::PropertyTrees()
    : needs_rebuild(true),
      non_root_surfaces_enabled(true),
      changed(false),
      full_tree_damaged(false),
      sequence_number(0),
      is_main_thread(true),
      is_active(false),
      verify_transform_tree_calculations(false) {
  transform_tree.SetPropertyTrees(this);
  effect_tree.SetPropertyTrees(this);
  clip_tree.SetPropertyTrees(this);
  scroll_tree.SetPropertyTrees(this);
}

PropertyTrees::~PropertyTrees() {}

bool PropertyTrees::operator==(const PropertyTrees& other) const {
  return transform_tree == other.transform_tree &&
         effect_tree == other.effect_tree && clip_tree == other.clip_tree &&
         scroll_tree == other.scroll_tree &&
         transform_id_to_index_map == other.transform_id_to_index_map &&
         effect_id_to_index_map == other.effect_id_to_index_map &&
         clip_id_to_index_map == other.clip_id_to_index_map &&
         scroll_id_to_index_map == other.scroll_id_to_index_map &&
         always_use_active_tree_opacity_effect_ids ==
             other.always_use_active_tree_opacity_effect_ids &&
         needs_rebuild == other.needs_rebuild && changed == other.changed &&
         full_tree_damaged == other.full_tree_damaged &&
         is_main_thread == other.is_main_thread &&
         is_active == other.is_active &&
         non_root_surfaces_enabled == other.non_root_surfaces_enabled &&
         sequence_number == other.sequence_number;
}

PropertyTrees& PropertyTrees::operator=(const PropertyTrees& from) {
  transform_tree = from.transform_tree;
  effect_tree = from.effect_tree;
  clip_tree = from.clip_tree;
  scroll_tree = from.scroll_tree;
  transform_id_to_index_map = from.transform_id_to_index_map;
  effect_id_to_index_map = from.effect_id_to_index_map;
  always_use_active_tree_opacity_effect_ids =
      from.always_use_active_tree_opacity_effect_ids;
  clip_id_to_index_map = from.clip_id_to_index_map;
  scroll_id_to_index_map = from.scroll_id_to_index_map;
  needs_rebuild = from.needs_rebuild;
  changed = from.changed;
  full_tree_damaged = from.full_tree_damaged;
  non_root_surfaces_enabled = from.non_root_surfaces_enabled;
  sequence_number = from.sequence_number;
  is_main_thread = from.is_main_thread;
  is_active = from.is_active;
  verify_transform_tree_calculations = from.verify_transform_tree_calculations;
  inner_viewport_container_bounds_delta_ =
      from.inner_viewport_container_bounds_delta();
  outer_viewport_container_bounds_delta_ =
      from.outer_viewport_container_bounds_delta();
  inner_viewport_scroll_bounds_delta_ =
      from.inner_viewport_scroll_bounds_delta();
  transform_tree.SetPropertyTrees(this);
  effect_tree.SetPropertyTrees(this);
  clip_tree.SetPropertyTrees(this);
  scroll_tree.SetPropertyTrees(this);
  ResetCachedData();
  return *this;
}

void PropertyTrees::ToProtobuf(proto::PropertyTrees* proto) const {
  // TODO(khushalsagar): Add support for sending diffs when serializaing
  // property trees. See crbug/555370.
  transform_tree.ToProtobuf(proto->mutable_transform_tree());
  effect_tree.ToProtobuf(proto->mutable_effect_tree());
  clip_tree.ToProtobuf(proto->mutable_clip_tree());
  scroll_tree.ToProtobuf(proto->mutable_scroll_tree());
  proto->set_needs_rebuild(needs_rebuild);
  proto->set_changed(changed);
  proto->set_full_tree_damaged(full_tree_damaged);
  proto->set_non_root_surfaces_enabled(non_root_surfaces_enabled);
  proto->set_is_main_thread(is_main_thread);
  proto->set_is_active(is_active);
  proto->set_verify_transform_tree_calculations(
      verify_transform_tree_calculations);

  // TODO(khushalsagar): Consider using the sequence number to decide if
  // property trees need to be serialized again for a commit. See crbug/555370.
  proto->set_sequence_number(sequence_number);

  for (auto i : always_use_active_tree_opacity_effect_ids)
    proto->add_always_use_active_tree_opacity_effect_ids(i);
}

// static
void PropertyTrees::FromProtobuf(const proto::PropertyTrees& proto) {
  transform_tree.FromProtobuf(proto.transform_tree(),
                              &transform_id_to_index_map);
  effect_tree.FromProtobuf(proto.effect_tree(), &effect_id_to_index_map);
  clip_tree.FromProtobuf(proto.clip_tree(), &clip_id_to_index_map);
  scroll_tree.FromProtobuf(proto.scroll_tree(), &scroll_id_to_index_map);

  needs_rebuild = proto.needs_rebuild();
  changed = proto.changed();
  full_tree_damaged = proto.full_tree_damaged();
  non_root_surfaces_enabled = proto.non_root_surfaces_enabled();
  sequence_number = proto.sequence_number();
  is_main_thread = proto.is_main_thread();
  is_active = proto.is_active();
  verify_transform_tree_calculations =
      proto.verify_transform_tree_calculations();

  transform_tree.SetPropertyTrees(this);
  effect_tree.SetPropertyTrees(this);
  clip_tree.SetPropertyTrees(this);
  scroll_tree.SetPropertyTrees(this);
  for (auto i : proto.always_use_active_tree_opacity_effect_ids())
    always_use_active_tree_opacity_effect_ids.push_back(i);
}

void PropertyTrees::clear() {
  transform_tree.clear();
  clip_tree.clear();
  effect_tree.clear();
  scroll_tree.clear();
  transform_id_to_index_map.clear();
  effect_id_to_index_map.clear();
  clip_id_to_index_map.clear();
  scroll_id_to_index_map.clear();
  always_use_active_tree_opacity_effect_ids.clear();

  needs_rebuild = true;
  full_tree_damaged = false;
  changed = false;
  non_root_surfaces_enabled = true;

#if DCHECK_IS_ON()
  PropertyTrees tree;
  tree.transform_tree = transform_tree;
  tree.effect_tree = effect_tree;
  tree.clip_tree = clip_tree;
  tree.scroll_tree = scroll_tree;
  // Scroll offset map and currently scrolling node id may not be copied
  // during operator=.
  ScrollTree::ScrollOffsetMap& map = tree.scroll_tree.scroll_offset_map();
  map = scroll_tree.scroll_offset_map();
  tree.scroll_tree.set_currently_scrolling_node(
      scroll_tree.CurrentlyScrollingNodeId());

  tree.sequence_number = sequence_number;
  tree.is_main_thread = is_main_thread;
  tree.is_active = is_active;
  DCHECK(tree == *this);
#endif
}

void PropertyTrees::SetInnerViewportContainerBoundsDelta(
    gfx::Vector2dF bounds_delta) {
  if (inner_viewport_container_bounds_delta_ == bounds_delta)
    return;

  inner_viewport_container_bounds_delta_ = bounds_delta;
  transform_tree.UpdateInnerViewportContainerBoundsDelta();
}

void PropertyTrees::SetOuterViewportContainerBoundsDelta(
    gfx::Vector2dF bounds_delta) {
  if (outer_viewport_container_bounds_delta_ == bounds_delta)
    return;

  outer_viewport_container_bounds_delta_ = bounds_delta;
  transform_tree.UpdateOuterViewportContainerBoundsDelta();
}

void PropertyTrees::SetInnerViewportScrollBoundsDelta(
    gfx::Vector2dF bounds_delta) {
  inner_viewport_scroll_bounds_delta_ = bounds_delta;
}

void PropertyTrees::PushOpacityIfNeeded(PropertyTrees* target_tree) {
  for (int id : target_tree->always_use_active_tree_opacity_effect_ids) {
    if (effect_id_to_index_map.find(id) == effect_id_to_index_map.end())
      continue;
    EffectNode* source_effect_node =
        effect_tree.Node(effect_id_to_index_map[id]);
    EffectNode* target_effect_node =
        target_tree->effect_tree.Node(target_tree->effect_id_to_index_map[id]);
    float source_opacity = source_effect_node->opacity;
    float target_opacity = target_effect_node->opacity;
    if (source_opacity == target_opacity)
      continue;
    target_effect_node->opacity = source_opacity;
    target_tree->effect_tree.set_needs_update(true);
  }
}

void PropertyTrees::RemoveIdFromIdToIndexMaps(int id) {
  transform_id_to_index_map.erase(id);
  effect_id_to_index_map.erase(id);
  clip_id_to_index_map.erase(id);
  scroll_id_to_index_map.erase(id);
}

bool PropertyTrees::IsInIdToIndexMap(TreeType tree_type, int id) {
  std::unordered_map<int, int>* id_to_index_map = nullptr;
  switch (tree_type) {
    case TRANSFORM:
      id_to_index_map = &transform_id_to_index_map;
      break;
    case EFFECT:
      id_to_index_map = &effect_id_to_index_map;
      break;
    case CLIP:
      id_to_index_map = &clip_id_to_index_map;
      break;
    case SCROLL:
      id_to_index_map = &scroll_id_to_index_map;
      break;
  }
  return id_to_index_map->find(id) != id_to_index_map->end();
}

void PropertyTrees::UpdateChangeTracking() {
  for (int id = 1; id < static_cast<int>(effect_tree.size()); ++id) {
    EffectNode* node = effect_tree.Node(id);
    EffectNode* parent_node = effect_tree.parent(node);
    effect_tree.UpdateEffectChanged(node, parent_node);
  }
  for (int i = 1; i < static_cast<int>(transform_tree.size()); ++i) {
    TransformNode* node = transform_tree.Node(i);
    TransformNode* parent_node = transform_tree.parent(node);
    TransformNode* source_node = transform_tree.Node(node->source_node_id);
    transform_tree.UpdateTransformChanged(node, parent_node, source_node);
  }
}

void PropertyTrees::PushChangeTrackingTo(PropertyTrees* tree) {
  for (int id = 1; id < static_cast<int>(effect_tree.size()); ++id) {
    EffectNode* node = effect_tree.Node(id);
    if (node->effect_changed) {
      EffectNode* target_node = tree->effect_tree.Node(node->id);
      target_node->effect_changed = true;
    }
  }
  for (int id = 1; id < static_cast<int>(transform_tree.size()); ++id) {
    TransformNode* node = transform_tree.Node(id);
    if (node->transform_changed) {
      TransformNode* target_node = tree->transform_tree.Node(node->id);
      target_node->transform_changed = true;
    }
  }
  // Ensure that change tracking is updated even if property trees don't have
  // other reasons to get updated.
  tree->UpdateChangeTracking();
  tree->full_tree_damaged = full_tree_damaged;
}

void PropertyTrees::ResetAllChangeTracking() {
  transform_tree.ResetChangeTracking();
  effect_tree.ResetChangeTracking();
  changed = false;
  full_tree_damaged = false;
}

std::unique_ptr<base::trace_event::TracedValue> PropertyTrees::AsTracedValue()
    const {
  auto value = base::WrapUnique(new base::trace_event::TracedValue);

  value->SetInteger("sequence_number", sequence_number);

  value->BeginDictionary("transform_tree");
  transform_tree.AsValueInto(value.get());
  value->EndDictionary();

  value->BeginDictionary("effect_tree");
  effect_tree.AsValueInto(value.get());
  value->EndDictionary();

  value->BeginDictionary("clip_tree");
  clip_tree.AsValueInto(value.get());
  value->EndDictionary();

  value->BeginDictionary("scroll_tree");
  scroll_tree.AsValueInto(value.get());
  value->EndDictionary();

  return value;
}

CombinedAnimationScale PropertyTrees::GetAnimationScales(
    int transform_node_id,
    LayerTreeImpl* layer_tree_impl) {
  if (cached_data_.animation_scales[transform_node_id].update_number !=
      cached_data_.property_tree_update_number) {
    if (!layer_tree_impl->settings()
             .layer_transforms_should_scale_layer_contents) {
      cached_data_.animation_scales[transform_node_id].update_number =
          cached_data_.property_tree_update_number;
      cached_data_.animation_scales[transform_node_id]
          .combined_maximum_animation_target_scale = 0.f;
      cached_data_.animation_scales[transform_node_id]
          .combined_starting_animation_scale = 0.f;
      return CombinedAnimationScale(
          cached_data_.animation_scales[transform_node_id]
              .combined_maximum_animation_target_scale,
          cached_data_.animation_scales[transform_node_id]
              .combined_starting_animation_scale);
    }

    TransformNode* node = transform_tree.Node(transform_node_id);
    TransformNode* parent_node = transform_tree.parent(node);
    bool ancestor_is_animating_scale = false;
    float ancestor_maximum_target_scale = 0.f;
    float ancestor_starting_animation_scale = 0.f;
    if (parent_node) {
      CombinedAnimationScale combined_animation_scale =
          GetAnimationScales(parent_node->id, layer_tree_impl);
      ancestor_maximum_target_scale =
          combined_animation_scale.maximum_animation_scale;
      ancestor_starting_animation_scale =
          combined_animation_scale.starting_animation_scale;
      ancestor_is_animating_scale =
          cached_data_.animation_scales[parent_node->id]
              .to_screen_has_scale_animation;
    }

    cached_data_.animation_scales[transform_node_id]
        .to_screen_has_scale_animation =
        !node->has_only_translation_animations || ancestor_is_animating_scale;

    // Once we've failed to compute a maximum animated scale at an ancestor, we
    // continue to fail.
    bool failed_at_ancestor =
        ancestor_is_animating_scale && ancestor_maximum_target_scale == 0.f;

    // Computing maximum animated scale in the presence of non-scale/translation
    // transforms isn't supported.
    bool failed_for_non_scale_or_translation =
        !transform_tree.Node(transform_node_id)
             ->to_parent.IsScaleOrTranslation();

    // We don't attempt to accumulate animation scale from multiple nodes with
    // scale animations, because of the risk of significant overestimation. For
    // example, one node might be increasing scale from 1 to 10 at the same time
    // as another node is decreasing scale from 10 to 1. Naively combining these
    // scales would produce a scale of 100.
    bool failed_for_multiple_scale_animations =
        ancestor_is_animating_scale && !node->has_only_translation_animations;

    if (failed_at_ancestor || failed_for_non_scale_or_translation ||
        failed_for_multiple_scale_animations) {
      // This ensures that descendants know we've failed to compute a maximum
      // animated scale.
      cached_data_.animation_scales[transform_node_id]
          .to_screen_has_scale_animation = true;

      cached_data_.animation_scales[transform_node_id]
          .combined_maximum_animation_target_scale = 0.f;
      cached_data_.animation_scales[transform_node_id]
          .combined_starting_animation_scale = 0.f;
    } else if (!cached_data_.animation_scales[transform_node_id]
                    .to_screen_has_scale_animation) {
      cached_data_.animation_scales[transform_node_id]
          .combined_maximum_animation_target_scale = 0.f;
      cached_data_.animation_scales[transform_node_id]
          .combined_starting_animation_scale = 0.f;
    } else if (node->has_only_translation_animations) {
      // An ancestor is animating scale.
      gfx::Vector2dF local_scales =
          MathUtil::ComputeTransform2dScaleComponents(node->local, 0.f);
      float max_local_scale = std::max(local_scales.x(), local_scales.y());
      cached_data_.animation_scales[transform_node_id]
          .combined_maximum_animation_target_scale =
          max_local_scale * ancestor_maximum_target_scale;
      cached_data_.animation_scales[transform_node_id]
          .combined_starting_animation_scale =
          max_local_scale * ancestor_starting_animation_scale;
    } else {
      // TODO(sunxd): make LayerTreeImpl::MaximumTargetScale take layer id as
      // parameter.
      LayerImpl* layer_impl = layer_tree_impl->LayerById(node->owner_id);
      layer_impl->GetAnimationHost()->MaximumTargetScale(
          layer_impl->element_id(), layer_impl->GetElementTypeForAnimation(),
          &cached_data_.animation_scales[transform_node_id]
               .local_maximum_animation_target_scale);
      layer_impl->GetAnimationHost()->AnimationStartScale(
          layer_impl->element_id(), layer_impl->GetElementTypeForAnimation(),
          &cached_data_.animation_scales[transform_node_id]
               .local_starting_animation_scale);
      gfx::Vector2dF local_scales =
          MathUtil::ComputeTransform2dScaleComponents(node->local, 0.f);
      float max_local_scale = std::max(local_scales.x(), local_scales.y());

      if (cached_data_.animation_scales[transform_node_id]
                  .local_starting_animation_scale == 0.f ||
          cached_data_.animation_scales[transform_node_id]
                  .local_maximum_animation_target_scale == 0.f) {
        cached_data_.animation_scales[transform_node_id]
            .combined_maximum_animation_target_scale =
            max_local_scale * ancestor_maximum_target_scale;
        cached_data_.animation_scales[transform_node_id]
            .combined_starting_animation_scale =
            max_local_scale * ancestor_starting_animation_scale;
      } else {
        gfx::Vector2dF ancestor_scales =
            parent_node ? MathUtil::ComputeTransform2dScaleComponents(
                              transform_tree.ToScreen(parent_node->id), 0.f)
                        : gfx::Vector2dF(1.f, 1.f);

        float max_ancestor_scale =
            std::max(ancestor_scales.x(), ancestor_scales.y());
        cached_data_.animation_scales[transform_node_id]
            .combined_maximum_animation_target_scale =
            max_ancestor_scale *
            cached_data_.animation_scales[transform_node_id]
                .local_maximum_animation_target_scale;
        cached_data_.animation_scales[transform_node_id]
            .combined_starting_animation_scale =
            max_ancestor_scale *
            cached_data_.animation_scales[transform_node_id]
                .local_starting_animation_scale;
      }
    }
    cached_data_.animation_scales[transform_node_id].update_number =
        cached_data_.property_tree_update_number;
  }
  return CombinedAnimationScale(cached_data_.animation_scales[transform_node_id]
                                    .combined_maximum_animation_target_scale,
                                cached_data_.animation_scales[transform_node_id]
                                    .combined_starting_animation_scale);
}

void PropertyTrees::SetAnimationScalesForTesting(
    int transform_id,
    float maximum_animation_scale,
    float starting_animation_scale) {
  cached_data_.animation_scales[transform_id]
      .combined_maximum_animation_target_scale = maximum_animation_scale;
  cached_data_.animation_scales[transform_id]
      .combined_starting_animation_scale = starting_animation_scale;
  cached_data_.animation_scales[transform_id].update_number =
      cached_data_.property_tree_update_number;
}

const DrawTransforms& PropertyTrees::GetDrawTransforms(int transform_id,
                                                       int effect_id) const {
  if (cached_data_.draw_transforms[effect_id][transform_id].update_number !=
      cached_data_.property_tree_update_number) {
    gfx::Transform target_space_transform;
    gfx::Transform from_target;
    const TransformNode* transform_node = transform_tree.Node(transform_id);
    const EffectNode* effect_node = effect_tree.Node(effect_id);
    const TransformNode* dest_node =
        transform_tree.Node(effect_node->transform_id);
    DCHECK(effect_id == effect_tree.kRootNodeId ||
           effect_node->has_render_surface);
    bool already_computed_inverse = false;
    if (transform_id == effect_node->transform_id) {
      target_space_transform.Scale(effect_node->surface_contents_scale.x(),
                                   effect_node->surface_contents_scale.y());
    } else if (!dest_node || (dest_node->ancestors_are_invertible &&
                              dest_node->node_and_ancestors_are_flat)) {
      // Compute transform from transform_id to effect_node->transform using
      // screen space transforms.
      target_space_transform.ConcatTransform(
          transform_tree.ToScreen(transform_id));
      if (dest_node)
        target_space_transform.ConcatTransform(
            transform_tree.FromScreen(dest_node->id));
      if (dest_node->needs_surface_contents_scale)
        target_space_transform.matrix().postScale(
            dest_node->surface_contents_scale.x(),
            dest_node->surface_contents_scale.y(), 1.f);
    } else if (transform_node->id > dest_node->id) {
      target_space_transform =
          GetDrawTransforms(transform_node->parent_id, effect_id).to_target;
      if (transform_node->flattens_inherited_transform)
        target_space_transform.FlattenTo2d();
      target_space_transform.PreconcatTransform(transform_node->to_parent);
    } else {
      const TransformNode* current = dest_node;
      std::vector<int> source_to_destination;
      source_to_destination.push_back(current->id);
      current = transform_tree.parent(current);
      for (; current && current->id > transform_node->id;
           current = transform_tree.parent(current)) {
        source_to_destination.push_back(current->id);
      }
      DCHECK_EQ(current, transform_node);
      gfx::Transform combined_transform;
      size_t source_to_destination_size = source_to_destination.size();
      for (size_t i = 0; i < source_to_destination_size; ++i) {
        size_t index = source_to_destination_size - 1 - i;
        const TransformNode* node =
            transform_tree.Node(source_to_destination[index]);
        if (node->flattens_inherited_transform)
          combined_transform.FlattenTo2d();
        combined_transform.PreconcatTransform(node->to_parent);
      }
      if (effect_node->surface_contents_scale.x() != 0.f &&
          effect_node->surface_contents_scale.y() != 0.f)
        combined_transform.Scale(
            1.0f / effect_node->surface_contents_scale.x(),
            1.0f / effect_node->surface_contents_scale.y());
      cached_data_.draw_transforms[effect_id][transform_id]
          .transforms.invertible =
          combined_transform.GetInverse(&target_space_transform);
      from_target = combined_transform;
      already_computed_inverse = true;
    }
    if (!already_computed_inverse) {
      cached_data_.draw_transforms[effect_id][transform_id]
          .transforms.invertible =
          target_space_transform.GetInverse(&from_target);
    }
    cached_data_.draw_transforms[effect_id][transform_id].update_number =
        cached_data_.property_tree_update_number;
    cached_data_.draw_transforms[effect_id][transform_id]
        .transforms.from_target = from_target;
    cached_data_.draw_transforms[effect_id][transform_id].transforms.to_target =
        target_space_transform;
  }
  return cached_data_.draw_transforms[effect_id][transform_id].transforms;
}

void PropertyTrees::ResetCachedData() {
  cached_data_.property_tree_update_number = 0;
  cached_data_.animation_scales = std::vector<AnimationScaleData>(
      transform_tree.nodes().size(), AnimationScaleData());
  cached_data_.draw_transforms =
      std::vector<std::unordered_map<int, DrawTransformData>>(
          effect_tree.nodes().size(),
          std::unordered_map<int, DrawTransformData>());
}

void PropertyTrees::UpdateCachedNumber() {
  cached_data_.property_tree_update_number++;
}

gfx::Transform PropertyTrees::ToScreenSpaceTransformWithoutSurfaceContentsScale(
    int transform_id,
    int effect_id) const {
  DCHECK_GT(transform_id, 0);
  if (transform_id == 1) {
    return gfx::Transform();
  }
  gfx::Transform screen_space_transform = transform_tree.ToScreen(transform_id);
  const EffectNode* effect_node = effect_tree.Node(effect_id);

  if (effect_node->surface_contents_scale.x() != 0.0 &&
      effect_node->surface_contents_scale.y() != 0.0)
    screen_space_transform.Scale(1.0 / effect_node->surface_contents_scale.x(),
                                 1.0 / effect_node->surface_contents_scale.y());
  return screen_space_transform;
}

bool PropertyTrees::ComputeTransformToTarget(int transform_id,
                                             int effect_id,
                                             gfx::Transform* transform) const {
  transform->MakeIdentity();

  if (transform_id == TransformTree::kInvalidNodeId)
    return true;

  int target_transform_id;
  const EffectNode* effect_node = effect_tree.Node(effect_id);
  if (effect_id == EffectTree::kInvalidNodeId) {
    // This can happen when PaintArtifactCompositor builds property trees as
    // it doesn't set effect ids on clip nodes. We want to compute transform
    // to the root in this case.
    target_transform_id = TransformTree::kRootNodeId;
  } else {
    DCHECK(effect_node->has_render_surface ||
           effect_node->id == EffectTree::kRootNodeId);
    target_transform_id = effect_node->transform_id;
  }

  bool success = transform_tree.ComputeTransform(
      transform_id, target_transform_id, transform);
  if (verify_transform_tree_calculations) {
    gfx::Transform to_target;
    to_target.ConcatTransform(
        GetDrawTransforms(transform_id, effect_id).to_target);
    if (effect_node->surface_contents_scale.x() != 0.f &&
        effect_node->surface_contents_scale.y() != 0.f)
      to_target.matrix().postScale(
          1.0f / effect_node->surface_contents_scale.x(),
          1.0f / effect_node->surface_contents_scale.y(), 1.0f);
    DCHECK(to_target.ApproximatelyEqual(*transform));
  }
  return success;
}

bool PropertyTrees::ComputeTransformFromTarget(
    int transform_id,
    int effect_id,
    gfx::Transform* transform) const {
  transform->MakeIdentity();

  if (transform_id == TransformTree::kInvalidNodeId)
    return true;

  int target_transform_id;
  const EffectNode* effect_node = effect_tree.Node(effect_id);
  if (effect_id == EffectTree::kInvalidNodeId) {
    // This can happen when PaintArtifactCompositor builds property trees as
    // it doesn't set effect ids on clip nodes. We want to compute transform
    // to the root in this case.
    target_transform_id = TransformTree::kRootNodeId;
  } else {
    DCHECK(effect_node->has_render_surface ||
           effect_node->id == EffectTree::kRootNodeId);
    target_transform_id = effect_node->transform_id;
  }

  bool success = transform_tree.ComputeTransform(target_transform_id,
                                                 transform_id, transform);
  if (verify_transform_tree_calculations) {
    auto draw_transforms = GetDrawTransforms(transform_id, effect_id);
    gfx::Transform from_target;
    from_target.ConcatTransform(draw_transforms.from_target);
    from_target.Scale(effect_node->surface_contents_scale.x(),
                      effect_node->surface_contents_scale.y());
    DCHECK(from_target.ApproximatelyEqual(*transform) ||
           !draw_transforms.invertible);
  }
  return success;
}

}  // namespace cc
