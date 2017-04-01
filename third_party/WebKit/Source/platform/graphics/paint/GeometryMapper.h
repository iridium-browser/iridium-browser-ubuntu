// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GeometryMapper_h
#define GeometryMapper_h

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/PropertyTreeState.h"
#include "platform/transforms/TransformationMatrix.h"
#include "wtf/HashMap.h"

namespace blink {

struct PrecomputedDataForAncestor {
  // Maps from a transform node that is a descendant of the ancestor to the
  // combined transform between the descendant's and the ancestor's coordinate
  // space.
  HashMap<const TransformPaintPropertyNode*, TransformationMatrix>
      toAncestorTransforms;

  // Maps from a descendant clip node to its equivalent "clip visual rect" in
  // the space of the ancestor. The clip visual rect is defined as the
  // intersection of all clips between the descendant and the ancestor (*not*
  // including the ancestor) in the clip tree, individually transformed from
  // their localTransformSpace into the ancestor's localTransformSpace.
  HashMap<const ClipPaintPropertyNode*, FloatRect> toAncestorClipRects;

  static std::unique_ptr<PrecomputedDataForAncestor> create() {
    return WTF::makeUnique<PrecomputedDataForAncestor>();
  }
};

// GeometryMapper is a helper class for fast computations of transformed and
// visual rects in different PropertyTreeStates. The design document has a
// number of details on use cases, algorithmic definitions, and running times.
//
// NOTE: A GeometryMapper object is only valid for property trees that do not
// change. If any mutation occurs, a new GeometryMapper object must be allocated
// corresponding to the new state.
//
// Design document: http://bit.ly/28P4FDA
//
// TODO(chrishtr): take effect tree into account.
class PLATFORM_EXPORT GeometryMapper {
 public:
  GeometryMapper() {}
  // The runtime of m calls among localToAncestorVisualRect, localToAncestorRect
  // or ancestorToLocalRect with the same |ancestorState| parameter is
  // guaranteed to be O(n + m), where n is the number of transform and clip
  // nodes in their respective property trees.

  // If the clips and transforms of |sourceState| are equal to or descendants of
  // those of |destinationState|, returns the same value as
  // localToAncestorVisualRect. Otherwise, maps the input rect to the
  // transform state which is the lowest common ancestor of
  // |sourceState.transform| and |destinationState.transform|, then multiplies
  // it by the the inverse transform mapping from the lowest common ancestor to
  // |destinationState.transform|.
  //
  // DCHECK fails if the clip of |destinationState| is not an ancestor of the
  // clip of |sourceState|, or the inverse transform is not invertible.
  FloatRect sourceToDestinationVisualRect(
      const FloatRect&,
      const PropertyTreeState& sourceState,
      const PropertyTreeState& destinationState);

  // Same as sourceToDestinationVisualRect() except that only transforms are
  // applied.
  FloatRect sourceToDestinationRect(
      const FloatRect&,
      const TransformPaintPropertyNode* sourceTransformNode,
      const TransformPaintPropertyNode* destinationTransformNode);

  // Maps from a rect in |localTransformSpace| to its visual rect in
  // |ancestorState|. This is computed by multiplying the rect by its combined
  // transform between |localTransformSpace| and |ancestorSpace|, then
  // flattening into 2D space, then intersecting by the "clip visual rect" for
  // |localTransformState|'s clips. See above for the definition of "clip visual
  // rect".
  //
  // Note that the clip of |ancestorState| is *not* applied.
  //
  // DCHECK fails if any of the paint property tree nodes in
  // |localTransformState| are not equal to or a descendant of that in
  // |ancestorState|.
  FloatRect localToAncestorVisualRect(
      const FloatRect&,
      const PropertyTreeState& localTransformState,
      const PropertyTreeState& ancestorState);

  // Maps from a rect in |localTransformNode| space to its transformed rect in
  // |ancestorTransformNode| space. This is computed by multiplying the rect by
  // the combined transform between |localTransformNode| and
  // |ancestorTransformNode|, then flattening into 2D space.
  //
  // DCHECK fails if |localTransformNode| is not equal to or a descendant of
  // |ancestorTransformNode|.
  FloatRect localToAncestorRect(
      const FloatRect&,
      const TransformPaintPropertyNode* localTransformNode,
      const TransformPaintPropertyNode* ancestorTransformNode);

  // Maps from a rect in |ancestorTransformNode| space to its transformed rect
  // in |localTransformNode| space. This is computed by multiplying the rect by
  // the inverse combined transform between |localTransformNode| and
  // |ancestorTransformNode|, if the transform is invertible.
  //
  // DCHECK fails if the combined transform is not invertible, or
  // |localTransformNode| is not equal to or a descendant of
  // |ancestorTransformNode|.
  FloatRect ancestorToLocalRect(
      const FloatRect&,
      const TransformPaintPropertyNode* ancestorTransformNode,
      const TransformPaintPropertyNode* localTransformNode);

  // Returns the matrix used in |LocalToAncestorRect|. DCHECK fails iff
  // |localTransformNode| is not equal to or a descendant of
  // |ancestorTransformNode|.
  const TransformationMatrix& localToAncestorMatrix(
      const TransformPaintPropertyNode* localTransformNode,
      const TransformPaintPropertyNode* ancestorTransformNode);

  // Returns the "clip visual rect" between |localTransformState| and
  // |ancestorState|. See above for the definition of "clip visual rect".
  FloatRect localToAncestorClipRect(
      const PropertyTreeState& localTransformState,
      const PropertyTreeState& ancestorState);

  // Returns the lowest common ancestor in the paint property tree.
  template <typename NodeType>
  static PLATFORM_EXPORT const NodeType* lowestCommonAncestor(const NodeType*,
                                                              const NodeType*);

 private:
  // The internal methods do the same things as their public counterparts, but
  // take an extra |success| parameter which indicates if the function is
  // successful on return. See comments of the public functions for failure
  // conditions.

  FloatRect sourceToDestinationVisualRectInternal(
      const FloatRect&,
      const PropertyTreeState& sourceState,
      const PropertyTreeState& destinationState,
      bool& success);

  FloatRect localToAncestorVisualRectInternal(
      const FloatRect&,
      const PropertyTreeState& localTransformState,
      const PropertyTreeState& ancestorState,
      bool& success);

  FloatRect localToAncestorRectInternal(
      const FloatRect&,
      const TransformPaintPropertyNode* localTransformNode,
      const TransformPaintPropertyNode* ancestorTransformNode,
      bool& success);

  const TransformationMatrix& localToAncestorMatrixInternal(
      const TransformPaintPropertyNode* localTransformNode,
      const TransformPaintPropertyNode* ancestorTransformNode,
      bool& success);

  FloatRect localToAncestorClipRectInternal(
      const PropertyTreeState& localTransformState,
      const PropertyTreeState& ancestorState,
      bool& success);

  // Returns the precomputed data if already set, or adds and memoizes a new
  // PrecomputedDataForAncestor otherwise.
  PrecomputedDataForAncestor& getPrecomputedDataForAncestor(
      const TransformPaintPropertyNode*);

  friend class GeometryMapperTest;
  friend class PaintLayerClipperTest;

  HashMap<const TransformPaintPropertyNode*,
          std::unique_ptr<PrecomputedDataForAncestor>>
      m_data;

  const TransformationMatrix m_identity;

  DISALLOW_COPY_AND_ASSIGN(GeometryMapper);
};

}  // namespace blink

#endif  // GeometryMapper_h
