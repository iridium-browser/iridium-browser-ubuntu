// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintInvalidator_h
#define PaintInvalidator_h

#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/paint/GeometryMapper.h"
#include "wtf/Vector.h"

namespace blink {

class FrameView;
class LayoutBoxModelObject;
class LayoutObject;
class PaintLayer;
struct PaintPropertyTreeBuilderContext;

struct PaintInvalidatorContext {
    PaintInvalidatorContext(const PaintPropertyTreeBuilderContext& treeBuilderContext)
        : treeBuilderContext(treeBuilderContext) { }

    PaintInvalidatorContext(const PaintPropertyTreeBuilderContext& treeBuilderContext, const PaintInvalidatorContext& parentContext)
        : treeBuilderContext(treeBuilderContext)
        , forcedSubtreeInvalidationFlags(parentContext.forcedSubtreeInvalidationFlags)
        , paintInvalidationContainer(parentContext.paintInvalidationContainer)
        , paintInvalidationContainerForStackedContents(parentContext.paintInvalidationContainerForStackedContents)
        , paintingLayer(parentContext.paintingLayer)
    { }

    // This method is temporary to adapt PaintInvalidatorContext and the legacy PaintInvalidationState
    // for code shared by old code and new code.
    virtual void mapLocalRectToPaintInvalidationBacking(const LayoutObject&, LayoutRect&) const;

    const PaintPropertyTreeBuilderContext& treeBuilderContext;

    enum ForcedSubtreeInvalidationFlag {
        ForcedSubtreeInvalidationChecking = 1 << 0,
        ForcedSubtreeInvalidationRectUpdate = 1 << 1,
        ForcedSubtreeFullInvalidation = 1 << 2,
        ForcedSubtreeFullInvalidationForStackedContents = 1 << 3,
    };
    unsigned forcedSubtreeInvalidationFlags = 0;

    // The following fields can be null only before PaintInvalidator::updateContext().

    // The current paint invalidation container for normal flow objects.
    // It is the enclosing composited object.
    const LayoutBoxModelObject* paintInvalidationContainer = nullptr;

    // The current paint invalidation container for stacked contents (stacking contexts or positioned objects).
    // It is the nearest ancestor composited object which establishes a stacking context.
    // See Source/core/paint/README.md ### PaintInvalidationState for details on how stacked contents'
    // paint invalidation containers differ.
    const LayoutBoxModelObject* paintInvalidationContainerForStackedContents = nullptr;

    PaintLayer* paintingLayer = nullptr;

    LayoutRect oldBounds;
    LayoutRect newBounds;
    LayoutPoint oldLocation;
    LayoutPoint newLocation;
};

class PaintInvalidator {
public:
    void invalidatePaintIfNeeded(FrameView&, PaintInvalidatorContext&);
    void invalidatePaintIfNeeded(const LayoutObject&, PaintInvalidatorContext&);

    // Process objects needing paint invalidation on the next frame.
    // See the definition of PaintInvalidationDelayedFull for more details.
    void processPendingDelayedPaintInvalidations();

private:
    LayoutRect mapLocalRectToPaintInvalidationBacking(const LayoutObject&, const FloatRect&, const PaintInvalidatorContext&);
    LayoutRect computePaintInvalidationRectInBacking(const LayoutObject&, const PaintInvalidatorContext&);
    LayoutPoint computeLocationFromPaintInvalidationBacking(const LayoutObject&, const PaintInvalidatorContext&);
    void updatePaintingLayer(const LayoutObject&, PaintInvalidatorContext&);
    void updateContext(const LayoutObject&, PaintInvalidatorContext&);

    Vector<const LayoutObject*> m_pendingDelayedPaintInvalidations;
    GeometryMapper m_geometryMapper;
};

} // namespace blink

#endif // PaintInvalidator_h
