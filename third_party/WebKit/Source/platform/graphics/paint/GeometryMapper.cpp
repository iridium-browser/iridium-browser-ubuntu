// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/GeometryMapper.h"

#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"

namespace blink {

FloatRect GeometryMapper::mapToVisualRectInDestinationSpace(const FloatRect& rect,
    const PropertyTreeState& sourceState,
    const PropertyTreeState& destinationState,
    bool& success)
{
    FloatRect result = localToVisualRectInAncestorSpace(rect, sourceState, destinationState, success);
    if (success)
        return result;
    return slowMapToVisualRectInDestinationSpace(rect, sourceState, destinationState, success);
}

FloatRect GeometryMapper::mapRectToDestinationSpace(const FloatRect& rect,
    const PropertyTreeState& sourceState,
    const PropertyTreeState& destinationState,
    bool& success)
{
    FloatRect result = localToAncestorRect(rect, sourceState, destinationState, success);
    if (success)
        return result;
    return slowMapRectToDestinationSpace(rect, sourceState, destinationState, success);
}

FloatRect GeometryMapper::slowMapToVisualRectInDestinationSpace(const FloatRect& rect,
    const PropertyTreeState& sourceState,
    const PropertyTreeState& destinationState,
    bool& success)
{
    const TransformPaintPropertyNode* lcaTransform = propertyTreeNearestCommonAncestor<TransformPaintPropertyNode>(sourceState.transform.get(), destinationState.transform.get());
    DCHECK(lcaTransform);

    // Assume that the clip of destinationState is an ancestor of the clip of sourceState
    // and is under the space of lcaTransform. Otherwise localToAncestorClipRect() will fail.
    PropertyTreeState lcaState = destinationState;
    lcaState.transform = lcaTransform;

    const auto clipRect = localToAncestorClipRect(sourceState, lcaState, success);
    if (!success)
        return rect;

    FloatRect result = localToAncestorRect(rect, sourceState, lcaState, success);
    DCHECK(success);
    result.intersect(clipRect);

    const TransformationMatrix& destinationToLca = localToAncestorMatrix(destinationState.transform.get(), lcaState, success);
    DCHECK(success);
    if (destinationToLca.isInvertible()) {
        success = true;
        return destinationToLca.inverse().mapRect(result);
    }
    success = false;
    return rect;
}

FloatRect GeometryMapper::slowMapRectToDestinationSpace(const FloatRect& rect,
    const PropertyTreeState& sourceState,
    const PropertyTreeState& destinationState,
    bool& success)
{
    const TransformPaintPropertyNode* lcaTransform = propertyTreeNearestCommonAncestor<TransformPaintPropertyNode>(sourceState.transform.get(), destinationState.transform.get());
    DCHECK(lcaTransform);
    PropertyTreeState lcaState = sourceState;
    lcaState.transform = lcaTransform;

    FloatRect result = localToAncestorRect(rect, sourceState, lcaState, success);
    DCHECK(success);

    const TransformationMatrix& destinationToLca = localToAncestorMatrix(destinationState.transform.get(), lcaState, success);
    DCHECK(success);
    if (destinationToLca.isInvertible()) {
        success = true;
        return destinationToLca.inverse().mapRect(result);
    }
    success = false;
    return rect;
}

FloatRect GeometryMapper::localToVisualRectInAncestorSpace(
    const FloatRect& rect,
    const PropertyTreeState& localState,
    const PropertyTreeState& ancestorState, bool& success)
{
    const auto& transformMatrix = localToAncestorMatrix(localState.transform.get(), ancestorState, success);
    if (!success)
        return rect;

    FloatRect mappedRect = transformMatrix.mapRect(rect);

    const auto clipRect = localToAncestorClipRect(localState, ancestorState, success);
    DCHECK(success);

    mappedRect.intersect(clipRect);
    return mappedRect;
}

FloatRect GeometryMapper::localToAncestorRect(
    const FloatRect& rect,
    const PropertyTreeState& localState,
    const PropertyTreeState& ancestorState,
    bool& success)
{
    const auto& transformMatrix = localToAncestorMatrix(localState.transform.get(), ancestorState, success);
    if (!success)
        return rect;
    return transformMatrix.mapRect(rect);
}

FloatRect GeometryMapper::ancestorToLocalRect(
    const FloatRect& rect,
    const PropertyTreeState& localState,
    const PropertyTreeState& ancestorState, bool& success)
{
    const auto& transformMatrix = localToAncestorMatrix(localState.transform.get(), ancestorState, success);
    if (!success)
        return rect;

    if (!transformMatrix.isInvertible()) {
        success = false;
        return rect;
    }
    success = true;

    // TODO(chrishtr): Cache the inverse?
    return transformMatrix.inverse().mapRect(rect);
}

PrecomputedDataForAncestor& GeometryMapper::getPrecomputedDataForAncestor(const PropertyTreeState& ancestorState)
{
    auto addResult = m_data.add(ancestorState.transform.get(), nullptr);
    if (addResult.isNewEntry)
        addResult.storedValue->value = PrecomputedDataForAncestor::create();
    return *addResult.storedValue->value;
}

FloatRect GeometryMapper::localToAncestorClipRect(
    const PropertyTreeState& localState,
    const PropertyTreeState& ancestorState,
    bool& success)
{
    PrecomputedDataForAncestor& precomputedData = getPrecomputedDataForAncestor(ancestorState);
    const ClipPaintPropertyNode* clipNode = localState.clip.get();
    Vector<const ClipPaintPropertyNode*> intermediateNodes;
    FloatRect clip(LayoutRect::infiniteIntRect());

    bool found = false;
    // Iterate over the path from localState.clip to ancestorState.clip. Stop if we've found a memoized (precomputed) clip
    // for any particular node.
    while (clipNode) {
        auto it = precomputedData.toAncestorClipRects.find(clipNode);
        if (it != precomputedData.toAncestorClipRects.end()) {
            clip = it->value;
            found = true;
            break;
        }
        intermediateNodes.append(clipNode);

        if (clipNode == ancestorState.clip)
            break;

        clipNode = clipNode->parent();
    }
    if (clipNode != ancestorState.clip && !found) {
        success = false;
        return clip;
    }

    // Iterate down from the top intermediate node found in the previous loop, computing and memoizing clip rects as we go.
    for (auto it = intermediateNodes.rbegin(); it != intermediateNodes.rend(); ++it) {
        if ((*it) != ancestorState.clip) {
            success = false;
            const TransformationMatrix& transformMatrix = localToAncestorMatrix((*it)->localTransformSpace(), ancestorState, success);
            if (!success)
                return clip;
            FloatRect mappedRect = transformMatrix.mapRect((*it)->clipRect().rect());
            clip.intersect(mappedRect);
        }

        precomputedData.toAncestorClipRects.set(*it, clip);
    }

    success = true;
    return precomputedData.toAncestorClipRects.find(localState.clip.get())->value;
}

const TransformationMatrix& GeometryMapper::localToAncestorMatrix(
    const TransformPaintPropertyNode* localTransformNode,
    const PropertyTreeState& ancestorState, bool& success) {
    PrecomputedDataForAncestor& precomputedData = getPrecomputedDataForAncestor(ancestorState);

    const TransformPaintPropertyNode* transformNode = localTransformNode;
    Vector<const TransformPaintPropertyNode*> intermediateNodes;
    TransformationMatrix transformMatrix;

    bool found = false;
    // Iterate over the path from localTransformNode to ancestorState.transform. Stop if we've found a memoized (precomputed) transform
    // for any particular node.
    while (transformNode) {
        auto it = precomputedData.toAncestorTransforms.find(transformNode);
        if (it != precomputedData.toAncestorTransforms.end()) {
            transformMatrix = it->value;
            found = true;
            break;
        }

        intermediateNodes.append(transformNode);

        if (transformNode == ancestorState.transform)
            break;

        transformNode = transformNode->parent();
    }
    if (!found && transformNode != ancestorState.transform) {
        success = false;
        return m_identity;
    }

    // Iterate down from the top intermediate node found in the previous loop, computing and memoizing transforms as we go.
    for (auto it = intermediateNodes.rbegin(); it != intermediateNodes.rend(); it++) {
        if ((*it) != ancestorState.transform) {
            TransformationMatrix localTransformMatrix = (*it)->matrix();
            localTransformMatrix.applyTransformOrigin((*it)->origin());
            transformMatrix =  transformMatrix * localTransformMatrix;
        }

        precomputedData.toAncestorTransforms.set(*it, transformMatrix);
    }
    success = true;
    return precomputedData.toAncestorTransforms.find(localTransformNode)->value;
}

} // namespace blink
