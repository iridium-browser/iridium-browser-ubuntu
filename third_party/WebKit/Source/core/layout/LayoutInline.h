/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef LayoutInline_h
#define LayoutInline_h

#include "core/CoreExport.h"
#include "core/editing/PositionWithAffinity.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/api/LineLayoutItem.h"
#include "core/layout/line/InlineFlowBox.h"
#include "core/layout/line/LineBoxList.h"

namespace blink {

class CORE_EXPORT LayoutInline : public LayoutBoxModelObject {
public:
    explicit LayoutInline(Element*);

    static LayoutInline* createAnonymous(Document*);

    LayoutObject* firstChild() const { ASSERT(children() == virtualChildren()); return children()->firstChild(); }
    LayoutObject* lastChild() const { ASSERT(children() == virtualChildren()); return children()->lastChild(); }

    // If you have a LayoutInline, use firstChild or lastChild instead.
    void slowFirstChild() const = delete;
    void slowLastChild() const = delete;

    void addChild(LayoutObject* newChild, LayoutObject* beforeChild = nullptr) override;

    Element* node() const { return toElement(LayoutBoxModelObject::node()); }

    LayoutRectOutsets marginBoxOutsets() const final;
    LayoutUnit marginLeft() const final;
    LayoutUnit marginRight() const final;
    LayoutUnit marginTop() const final;
    LayoutUnit marginBottom() const final;
    LayoutUnit marginBefore(const ComputedStyle* otherStyle = nullptr) const final;
    LayoutUnit marginAfter(const ComputedStyle* otherStyle = nullptr) const final;
    LayoutUnit marginStart(const ComputedStyle* otherStyle = nullptr) const final;
    LayoutUnit marginEnd(const ComputedStyle* otherStyle = nullptr) const final;

    void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const final;
    void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const override;

    LayoutSize offsetFromContainer(const LayoutObject*, const LayoutPoint&, bool* offsetDependsOnPoint = nullptr) const final;

    IntRect linesBoundingBox() const;
    LayoutRect linesVisualOverflowBoundingBox() const;

    InlineFlowBox* createAndAppendInlineFlowBox();

    void dirtyLineBoxes(bool fullLayout);

    LineBoxList* lineBoxes() { return &m_lineBoxes; }
    const LineBoxList* lineBoxes() const { return &m_lineBoxes; }

    InlineFlowBox* firstLineBox() const { return m_lineBoxes.firstLineBox(); }
    InlineFlowBox* lastLineBox() const { return m_lineBoxes.lastLineBox(); }
    InlineBox* firstLineBoxIncludingCulling() const { return alwaysCreateLineBoxes() ? firstLineBox() : culledInlineFirstLineBox(); }
    InlineBox* lastLineBoxIncludingCulling() const { return alwaysCreateLineBoxes() ? lastLineBox() : culledInlineLastLineBox(); }

    LayoutBoxModelObject* virtualContinuation() const final { return continuation(); }
    LayoutInline* inlineElementContinuation() const;

    void updateDragState(bool dragOn) final;

    LayoutSize offsetForInFlowPositionedInline(const LayoutBox& child) const;

    void addOutlineRects(Vector<LayoutRect>&, const LayoutPoint& additionalOffset) const final;
    // The following methods are called from the container if it has already added outline rects for line boxes
    // and/or children of this LayoutInline.
    void addOutlineRectsForChildrenAndContinuations(Vector<LayoutRect>&, const LayoutPoint& additionalOffset) const;
    void addOutlineRectsForContinuations(Vector<LayoutRect>&, const LayoutPoint& additionalOffset) const;

    using LayoutBoxModelObject::continuation;
    using LayoutBoxModelObject::setContinuation;

    bool alwaysCreateLineBoxes() const { return alwaysCreateLineBoxesForLayoutInline(); }
    void setAlwaysCreateLineBoxes(bool alwaysCreateLineBoxes = true) { setAlwaysCreateLineBoxesForLayoutInline(alwaysCreateLineBoxes); }
    void updateAlwaysCreateLineBoxes(bool fullLayout);

    LayoutRect localCaretRect(InlineBox*, int, LayoutUnit* extraWidthToEndOfLine) final;

    bool hitTestCulledInline(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset);

    const char* name() const override { return "LayoutInline"; }

protected:
    void willBeDestroyed() override;

    void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) override;

    void computeSelfHitTestRects(Vector<LayoutRect>& rects, const LayoutPoint& layerOffset) const override;

    void invalidateDisplayItemClients(const LayoutBoxModelObject& paintInvalidationContainer) const override;

private:
    LayoutObjectChildList* virtualChildren() final { return children(); }
    const LayoutObjectChildList* virtualChildren() const final { return children(); }
    const LayoutObjectChildList* children() const { return &m_children; }
    LayoutObjectChildList* children() { return &m_children; }

    bool isLayoutInline() const final { return true; }

    LayoutRect culledInlineVisualOverflowBoundingBox() const;
    InlineBox* culledInlineFirstLineBox() const;
    InlineBox* culledInlineLastLineBox() const;

    template<typename GeneratorContext>
    void generateLineBoxRects(GeneratorContext& yield) const;
    template<typename GeneratorContext>
    void generateCulledLineBoxRects(GeneratorContext& yield, const LayoutInline* container) const;

    void addChildToContinuation(LayoutObject* newChild, LayoutObject* beforeChild);
    void addChildIgnoringContinuation(LayoutObject* newChild, LayoutObject* beforeChild = nullptr) final;

    void moveChildrenToIgnoringContinuation(LayoutInline* to, LayoutObject* startChild);

    void splitInlines(LayoutBlock* fromBlock, LayoutBlock* toBlock, LayoutBlock* middleBlock,
        LayoutObject* beforeChild, LayoutBoxModelObject* oldCont);
    void splitFlow(LayoutObject* beforeChild, LayoutBlock* newBlockBox,
        LayoutObject* newChild, LayoutBoxModelObject* oldCont);

    void layout() final { ASSERT_NOT_REACHED(); } // Do nothing for layout()

    void paint(const PaintInfo&, const LayoutPoint&) final;

    bool nodeAtPoint(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) final;

    DeprecatedPaintLayerType layerTypeRequired() const override { return isInFlowPositioned() || createsGroup() || hasClipPath() || style()->shouldCompositeForCurrentAnimations() || style()->hasCompositorProxy() ? NormalDeprecatedPaintLayer : NoDeprecatedPaintLayer; }

    LayoutUnit offsetLeft() const final;
    LayoutUnit offsetTop() const final;
    LayoutUnit offsetWidth() const final { return linesBoundingBox().width(); }
    LayoutUnit offsetHeight() const final { return linesBoundingBox().height(); }

    LayoutRect absoluteClippedOverflowRect() const override;
    LayoutRect clippedOverflowRectForPaintInvalidation(const LayoutBoxModelObject* paintInvalidationContainer, const PaintInvalidationState* = nullptr) const override;
    LayoutRect rectWithOutlineForPaintInvalidation(const LayoutBoxModelObject* paintInvalidationContainer, LayoutUnit outlineWidth, const PaintInvalidationState* = nullptr) const final;
    void mapRectToPaintInvalidationBacking(const LayoutBoxModelObject* paintInvalidationContainer, LayoutRect&, const PaintInvalidationState*) const final;

    // This method differs from clippedOverflowRectForPaintInvalidation in that it includes
    // the rects for culled inline boxes, which aren't necessary for paint invalidation.
    LayoutRect clippedOverflowRect(const LayoutBoxModelObject*, const PaintInvalidationState* = nullptr) const;

    void mapLocalToContainer(const LayoutBoxModelObject* paintInvalidationContainer, TransformState&, MapCoordinatesFlags = ApplyContainerFlip, bool* wasFixed = 0, const PaintInvalidationState* = nullptr) const override;

    PositionWithAffinity positionForPoint(const LayoutPoint&) final;

    IntRect borderBoundingBox() const final
    {
        IntRect boundingBox = linesBoundingBox();
        return IntRect(0, 0, boundingBox.width(), boundingBox.height());
    }

    virtual InlineFlowBox* createInlineFlowBox(); // Subclassed by SVG and Ruby

    void dirtyLinesFromChangedChild(LayoutObject* child) final { m_lineBoxes.dirtyLinesFromChangedChild(LineLayoutItem(this), LineLayoutItem(child)); }

    LayoutUnit lineHeight(bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const final;
    int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const final;

    void childBecameNonInline(LayoutObject* child) final;

    void updateHitTestResult(HitTestResult&, const LayoutPoint&) final;

    void imageChanged(WrappedImagePtr, const IntRect* = nullptr) final;

    void addAnnotatedRegions(Vector<AnnotatedRegionValue>&) final;

    void updateFromStyle() final;

    LayoutInline* clone() const;

    LayoutBoxModelObject* continuationBefore(LayoutObject* beforeChild);

    LayoutObjectChildList m_children;
    LineBoxList m_lineBoxes; // All of the line boxes created for this inline flow.  For example, <i>Hello<br>world.</i> will have two <i> line boxes.
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutInline, isLayoutInline());

} // namespace blink

#endif // LayoutInline_h
