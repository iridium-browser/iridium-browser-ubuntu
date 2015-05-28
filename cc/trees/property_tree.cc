// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <vector>

#include "base/logging.h"
#include "cc/base/math_util.h"
#include "cc/trees/property_tree.h"

namespace cc {

template <typename T>
PropertyTree<T>::PropertyTree() {
  nodes_.push_back(T());
  back()->id = 0;
  back()->parent_id = -1;
}

template <typename T>
PropertyTree<T>::~PropertyTree() {
}

template <typename T>
int PropertyTree<T>::Insert(const T& tree_node, int parent_id) {
  DCHECK_GT(nodes_.size(), 0u);
  nodes_.push_back(tree_node);
  T& node = nodes_.back();
  node.parent_id = parent_id;
  node.id = static_cast<int>(nodes_.size()) - 1;
  return node.id;
}

template class PropertyTree<TransformNode>;
template class PropertyTree<ClipNode>;
template class PropertyTree<OpacityNode>;

TransformNodeData::TransformNodeData()
    : target_id(-1),
      content_target_id(-1),
      needs_local_transform_update(true),
      is_invertible(true),
      ancestors_are_invertible(true),
      is_animated(false),
      to_screen_is_animated(false),
      flattens_inherited_transform(false),
      node_and_ancestors_are_flat(true),
      scrolls(false),
      needs_sublayer_scale(false),
      layer_scale_factor(1.0f) {
}

TransformNodeData::~TransformNodeData() {
}

ClipNodeData::ClipNodeData() : transform_id(-1), target_id(-1) {
}

bool TransformTree::ComputeTransform(int source_id,
                                     int dest_id,
                                     gfx::Transform* transform) const {
  transform->MakeIdentity();

  if (source_id == dest_id)
    return true;

  if (source_id > dest_id && IsDescendant(source_id, dest_id))
    return CombineTransformsBetween(source_id, dest_id, transform);

  if (dest_id > source_id && IsDescendant(dest_id, source_id))
    return CombineInversesBetween(source_id, dest_id, transform);

  int lca = LowestCommonAncestor(source_id, dest_id);

  bool no_singular_matrices_to_lca =
      CombineTransformsBetween(source_id, lca, transform);

  bool no_singular_matrices_from_lca =
      CombineInversesBetween(lca, dest_id, transform);

  return no_singular_matrices_to_lca && no_singular_matrices_from_lca;
}

bool TransformTree::Are2DAxisAligned(int source_id, int dest_id) const {
  gfx::Transform transform;
  return ComputeTransform(source_id, dest_id, &transform) &&
         transform.Preserves2dAxisAlignment();
}

void TransformTree::UpdateTransforms(int id) {
  TransformNode* node = Node(id);
  TransformNode* parent_node = parent(node);
  TransformNode* target_node = Node(node->data.target_id);
  if (node->data.needs_local_transform_update)
    UpdateLocalTransform(node);
  UpdateScreenSpaceTransform(node, parent_node, target_node);
  UpdateSublayerScale(node);
  UpdateTargetSpaceTransform(node, target_node);
  UpdateIsAnimated(node, parent_node);
  UpdateSnapping(node);
}

bool TransformTree::IsDescendant(int desc_id, int source_id) const {
  while (desc_id != source_id) {
    if (desc_id < 0)
      return false;
    desc_id = Node(desc_id)->parent_id;
  }
  return true;
}

int TransformTree::LowestCommonAncestor(int a, int b) const {
  std::set<int> chain_a;
  std::set<int> chain_b;
  while (a || b) {
    if (a) {
      a = Node(a)->parent_id;
      if (a > -1 && chain_b.find(a) != chain_b.end())
        return a;
      chain_a.insert(a);
    }
    if (b) {
      b = Node(b)->parent_id;
      if (b > -1 && chain_a.find(b) != chain_a.end())
        return b;
      chain_b.insert(b);
    }
  }
  NOTREACHED();
  return 0;
}

bool TransformTree::CombineTransformsBetween(int source_id,
                                             int dest_id,
                                             gfx::Transform* transform) const {
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
  if (!dest || (dest->data.ancestors_are_invertible &&
                current->data.node_and_ancestors_are_flat)) {
    transform->ConcatTransform(current->data.to_screen);
    if (dest)
      transform->ConcatTransform(dest->data.from_screen);
    return true;
  }

  // Flattening is defined in a way that requires it to be applied while
  // traversing downward in the tree. We first identify nodes that are on the
  // path from the source to the destination (this is traversing upward), and
  // then we visit these nodes in reverse order, flattening as needed. We
  // early-out if we get to a node whose target node is the destination, since
  // we can then re-use the target space transform stored at that node.
  std::vector<int> source_to_destination;
  source_to_destination.push_back(current->id);
  current = parent(current);
  for (; current && current->id > dest_id; current = parent(current)) {
    if (current->data.target_id == dest_id &&
        current->data.content_target_id == dest_id)
      break;
    source_to_destination.push_back(current->id);
  }

  gfx::Transform combined_transform;
  if (current->id > dest_id) {
    combined_transform = current->data.to_target;
    // The stored target space transform has sublayer scale baked in, but we
    // need the unscaled transform.
    combined_transform.Scale(1.0f / dest->data.sublayer_scale.x(),
                             1.0f / dest->data.sublayer_scale.y());
  }

  for (int i = source_to_destination.size() - 1; i >= 0; i--) {
    const TransformNode* node = Node(source_to_destination[i]);
    if (node->data.flattens_inherited_transform)
      combined_transform.FlattenTo2d();
    combined_transform.PreconcatTransform(node->data.to_parent);
  }

  transform->ConcatTransform(combined_transform);
  return true;
}

bool TransformTree::CombineInversesBetween(int source_id,
                                           int dest_id,
                                           gfx::Transform* transform) const {
  const TransformNode* current = Node(dest_id);
  const TransformNode* dest = Node(source_id);
  // Just as in CombineTransformsBetween, we can use screen space transforms in
  // this computation only when there isn't any non-trivial flattening
  // involved.
  if (current->data.ancestors_are_invertible &&
      current->data.node_and_ancestors_are_flat) {
    transform->PreconcatTransform(current->data.from_screen);
    if (dest)
      transform->PreconcatTransform(dest->data.to_screen);
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
  gfx::Transform transform = node->data.post_local;
  transform.Translate(-node->data.scroll_offset.x(),
                      -node->data.scroll_offset.y());
  transform.PreconcatTransform(node->data.local);
  transform.PreconcatTransform(node->data.pre_local);
  node->data.set_to_parent(transform);
  node->data.needs_local_transform_update = false;
}

void TransformTree::UpdateScreenSpaceTransform(TransformNode* node,
                                               TransformNode* parent_node,
                                               TransformNode* target_node) {
  if (!parent_node) {
    node->data.to_screen = node->data.to_parent;
    node->data.ancestors_are_invertible = true;
    node->data.to_screen_is_animated = false;
    node->data.node_and_ancestors_are_flat = node->data.to_parent.IsFlat();
  } else {
    node->data.to_screen = parent_node->data.to_screen;
    if (node->data.flattens_inherited_transform)
      node->data.to_screen.FlattenTo2d();
    node->data.to_screen.PreconcatTransform(node->data.to_parent);
    node->data.ancestors_are_invertible =
        parent_node->data.ancestors_are_invertible;
    node->data.node_and_ancestors_are_flat =
        parent_node->data.node_and_ancestors_are_flat &&
        node->data.to_parent.IsFlat();
  }

  if (!node->data.to_screen.GetInverse(&node->data.from_screen))
    node->data.ancestors_are_invertible = false;
}

void TransformTree::UpdateSublayerScale(TransformNode* node) {
  // The sublayer scale depends on the screen space transform, so update it too.
  node->data.sublayer_scale =
      node->data.needs_sublayer_scale
          ? MathUtil::ComputeTransform2dScaleComponents(
                node->data.to_screen, node->data.layer_scale_factor)
          : gfx::Vector2dF(1.0f, 1.0f);
}

void TransformTree::UpdateTargetSpaceTransform(TransformNode* node,
                                               TransformNode* target_node) {
  node->data.to_target.MakeIdentity();
  if (node->data.needs_sublayer_scale) {
    node->data.to_target.Scale(node->data.sublayer_scale.x(),
                               node->data.sublayer_scale.y());
  } else {
    const bool target_is_root_surface = target_node->id == 1;
    // In order to include the root transform for the root surface, we walk up
    // to the root of the transform tree in ComputeTransform.
    int target_id = target_is_root_surface ? 0 : target_node->id;
    if (target_node) {
      node->data.to_target.Scale(target_node->data.sublayer_scale.x(),
                                 target_node->data.sublayer_scale.y());
    }

    gfx::Transform unscaled_target_transform;
    ComputeTransform(node->id, target_id, &unscaled_target_transform);
    node->data.to_target.PreconcatTransform(unscaled_target_transform);
  }

  if (!node->data.to_target.GetInverse(&node->data.from_target))
    node->data.ancestors_are_invertible = false;
}

void TransformTree::UpdateIsAnimated(TransformNode* node,
                                     TransformNode* parent_node) {
  if (parent_node) {
    node->data.to_screen_is_animated =
        node->data.is_animated || parent_node->data.to_screen_is_animated;
  }
}

void TransformTree::UpdateSnapping(TransformNode* node) {
  if (!node->data.scrolls || node->data.to_screen_is_animated ||
      !node->data.to_target.IsScaleOrTranslation()) {
    return;
  }

  // Scroll snapping must be done in target space (the pixels we care about).
  // This means we effectively snap the target space transform. If TT is the
  // target space transform and TT' is TT with its translation components
  // rounded, then what we're after is the scroll delta X, where TT * X = TT'.
  // I.e., we want a transform that will realize our scroll snap. It follows
  // that X = TT^-1 * TT'. We cache TT and TT^-1 to make this more efficient.
  gfx::Transform rounded = node->data.to_target;
  rounded.RoundTranslationComponents();
  gfx::Transform delta = node->data.from_target;
  delta *= rounded;

  DCHECK(delta.IsIdentityOr2DTranslation());

  gfx::Vector2dF translation = delta.To2dTranslation();

  // Now that we have our scroll delta, we must apply it to each of our
  // combined, to/from matrices.
  node->data.to_parent.Translate(translation.x(), translation.y());
  node->data.to_target.Translate(translation.x(), translation.y());
  node->data.from_target.matrix().postTranslate(-translation.x(),
                                                -translation.y(), 0);
  node->data.to_screen.Translate(translation.x(), translation.y());
  node->data.from_screen.matrix().postTranslate(-translation.x(),
                                                -translation.y(), 0);

  node->data.scroll_snap = translation;
}

}  // namespace cc
