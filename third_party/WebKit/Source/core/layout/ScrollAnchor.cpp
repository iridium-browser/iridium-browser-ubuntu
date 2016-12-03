// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ScrollAnchor.h"

#include "core/frame/FrameView.h"
#include "core/frame/UseCounter.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "platform/Histogram.h"

namespace blink {

using Corner = ScrollAnchor::Corner;

ScrollAnchor::ScrollAnchor()
    : m_anchorObject(nullptr)
    , m_corner(Corner::TopLeft)
    , m_scrollAnchorDisablingStyleChanged(false)
{
}

ScrollAnchor::ScrollAnchor(ScrollableArea* scroller)
    : ScrollAnchor()
{
    setScroller(scroller);
}

ScrollAnchor::~ScrollAnchor()
{
}

void ScrollAnchor::setScroller(ScrollableArea* scroller)
{
    DCHECK(!m_scroller);
    DCHECK(scroller);
    DCHECK(scroller->isRootFrameViewport() || scroller->isFrameView() || scroller->isPaintLayerScrollableArea());
    m_scroller = scroller;
}

// TODO(pilgrim) replace all instances of scrollerLayoutBox with scrollerLayoutBoxItem
// https://crbug.com/499321
static LayoutBox* scrollerLayoutBox(const ScrollableArea* scroller)
{
    LayoutBox* box = scroller->layoutBox();
    DCHECK(box);
    return box;
}

static LayoutBoxItem scrollerLayoutBoxItem(const ScrollableArea* scroller)
{
    return LayoutBoxItem(scrollerLayoutBox(scroller));
}

static Corner cornerFromCandidateRect(const LayoutObject* layoutObject)
{
    ASSERT(layoutObject);
    if (layoutObject->style()->isFlippedBlocksWritingMode()
        || !layoutObject->style()->isLeftToRightDirection())
        return Corner::TopRight;
    return Corner::TopLeft;
}

static LayoutPoint cornerPointOfRect(LayoutRect rect, Corner whichCorner)
{
    switch (whichCorner) {
    case Corner::TopLeft: return rect.minXMinYCorner();
    case Corner::TopRight: return rect.maxXMinYCorner();
    }
    ASSERT_NOT_REACHED();
    return LayoutPoint();
}

// Bounds of the LayoutObject relative to the scroller's visible content rect.
static LayoutRect relativeBounds(const LayoutObject* layoutObject, const ScrollableArea* scroller)
{
    LayoutRect localBounds;
    if (layoutObject->isBox()) {
        localBounds = toLayoutBox(layoutObject)->borderBoxRect();
        if (!layoutObject->hasOverflowClip()) {
            // borderBoxRect doesn't include overflow content and floats.
            LayoutUnit maxHeight = std::max(localBounds.height(), toLayoutBox(layoutObject)->layoutOverflowRect().height());
            if (layoutObject->isLayoutBlockFlow() && toLayoutBlockFlow(layoutObject)->containsFloats()) {
                // Note that lowestFloatLogicalBottom doesn't include floating
                // grandchildren.
                maxHeight = std::max(maxHeight, toLayoutBlockFlow(layoutObject)->lowestFloatLogicalBottom());
            }
            localBounds.setHeight(maxHeight);
        }
    } else if (layoutObject->isText()) {
        // TODO(skobes): Use first and last InlineTextBox only?
        for (InlineTextBox* box = toLayoutText(layoutObject)->firstTextBox(); box; box = box->nextTextBox())
            localBounds.unite(box->calculateBoundaries());
    } else {
        // Only LayoutBox and LayoutText are supported.
        ASSERT_NOT_REACHED();
    }
    LayoutRect relativeBounds = LayoutRect(layoutObject->localToAncestorQuad(
        FloatRect(localBounds), scrollerLayoutBox(scroller)).boundingBox());
    // When the scroller is the FrameView, localToAncestorQuad returns document coords,
    // so we must subtract scroll offset to get viewport coords. We discard the fractional
    // part of the scroll offset so that the rounding in restore() matches the snapping of
    // the anchor node to the pixel grid of the layer it paints into. For non-FrameView
    // scrollers, we rely on the flooring behavior of LayoutBox::scrolledContentOffset.
    if (scroller->isFrameView() || scroller->isRootFrameViewport())
        relativeBounds.moveBy(-flooredIntPoint(scroller->scrollPositionDouble()));
    return relativeBounds;
}

static LayoutPoint computeRelativeOffset(const LayoutObject* layoutObject, const ScrollableArea* scroller, Corner corner)
{
    return cornerPointOfRect(relativeBounds(layoutObject, scroller), corner);
}

static bool candidateMayMoveWithScroller(const LayoutObject* candidate, const ScrollableArea* scroller)
{
    if (const ComputedStyle* style = candidate->style()) {
        if (style->hasViewportConstrainedPosition())
            return false;
    }

    bool skippedByContainerLookup = false;
    candidate->container(scrollerLayoutBox(scroller), &skippedByContainerLookup);
    return !skippedByContainerLookup;
}

ScrollAnchor::ExamineResult ScrollAnchor::examine(const LayoutObject* candidate) const
{
    if (candidate->isLayoutInline())
        return ExamineResult(Continue);

    // Anonymous blocks are not in the DOM tree and it may be hard for
    // developers to reason about the anchor node.
    if (candidate->isAnonymous())
        return ExamineResult(Continue);

    if (!candidate->isText() && !candidate->isBox())
        return ExamineResult(Skip);

    if (!candidateMayMoveWithScroller(candidate, m_scroller))
        return ExamineResult(Skip);

    if (candidate->style()->overflowAnchor() == AnchorNone)
        return ExamineResult(Skip);

    LayoutRect candidateRect = relativeBounds(candidate, m_scroller);
    LayoutRect visibleRect = scrollerLayoutBoxItem(m_scroller).overflowClipRect(LayoutPoint());

    bool occupiesSpace = candidateRect.width() > 0 && candidateRect.height() > 0;
    if (occupiesSpace && visibleRect.intersects(candidateRect)) {
        return ExamineResult(
            visibleRect.contains(candidateRect) ? Return : Constrain,
            cornerFromCandidateRect(candidate));
    } else {
        return ExamineResult(Skip);
    }
}

void ScrollAnchor::findAnchor()
{
    TRACE_EVENT0("blink", "ScrollAnchor::findAnchor");
    SCOPED_BLINK_UMA_HISTOGRAM_TIMER("Layout.ScrollAnchor.TimeToFindAnchor");

    LayoutObject* stayWithin = scrollerLayoutBox(m_scroller);
    LayoutObject* candidate = stayWithin->nextInPreOrder(stayWithin);
    while (candidate) {
        ExamineResult result = examine(candidate);
        if (result.viable) {
            m_anchorObject = candidate;
            m_corner = result.corner;
        }
        switch (result.status) {
        case Skip:
            candidate = candidate->nextInPreOrderAfterChildren(stayWithin);
            break;
        case Constrain:
            stayWithin = candidate;
            // fall through
        case Continue:
            candidate = candidate->nextInPreOrder(stayWithin);
            break;
        case Return:
            return;
        }
    }
}

bool ScrollAnchor::computeScrollAnchorDisablingStyleChanged()
{
    LayoutObject* current = anchorObject();
    if (!current)
        return false;

    LayoutObject* scrollerBox = scrollerLayoutBox(m_scroller);
    while (true) {
        DCHECK(current);
        if (current->scrollAnchorDisablingStyleChanged())
            return true;
        if (current == scrollerBox)
            return false;
        current = current->parent();
    }
}

void ScrollAnchor::save()
{
    DCHECK(m_scroller);
    if (m_scroller->scrollPosition() == IntPoint::zero()) {
        clear();
        return;
    }

    if (!m_anchorObject) {
        findAnchor();
        if (!m_anchorObject)
            return;

        m_anchorObject->setIsScrollAnchorObject();
        m_savedRelativeOffset = computeRelativeOffset(
            m_anchorObject, m_scroller, m_corner);
    }

    // Note that we must compute this during save() since the scroller's
    // descendants have finished layout (and had the bit cleared) by the
    // time restore() is called.
    m_scrollAnchorDisablingStyleChanged = computeScrollAnchorDisablingStyleChanged();
}

IntSize ScrollAnchor::computeAdjustment() const
{
    // The anchor node can report fractional positions, but it is DIP-snapped when
    // painting (crbug.com/610805), so we must round the offsets to determine the
    // visual delta. If we scroll by the delta in LayoutUnits, the snapping of the
    // anchor node may round differently from the snapping of the scroll position.
    // (For example, anchor moving from 2.4px -> 2.6px is really 2px -> 3px, so we
    // should scroll by 1px instead of 0.2px.) This is true regardless of whether
    // the ScrollableArea actually uses fractional scroll positions.
    return roundedIntSize(computeRelativeOffset(m_anchorObject, m_scroller, m_corner)) -
        roundedIntSize(m_savedRelativeOffset);
}

void ScrollAnchor::restore()
{
    DCHECK(m_scroller);
    if (!m_anchorObject)
        return;
    IntSize adjustment = computeAdjustment();
    if (adjustment.isZero())
        return;

    if (m_scrollAnchorDisablingStyleChanged) {
        // Note that we only clear if the adjustment would have been non-zero.
        // This minimizes redundant calls to findAnchor.
        // TODO(skobes): add UMA metric for this.
        clear();
        return;
    }

    m_scroller->setScrollPosition(m_scroller->scrollPositionDouble() + adjustment, AnchoringScroll);

    // Update UMA metric.
    DEFINE_STATIC_LOCAL(EnumerationHistogram, adjustedOffsetHistogram,
        ("Layout.ScrollAnchor.AdjustedScrollOffset", 2));
    adjustedOffsetHistogram.count(1);
    UseCounter::count(scrollerLayoutBox(m_scroller)->document(), UseCounter::ScrollAnchored);
}

void ScrollAnchor::clear()
{
    LayoutObject* anchorObject = m_anchorObject;
    m_anchorObject = nullptr;

    if (anchorObject)
        anchorObject->maybeClearIsScrollAnchorObject();
}

bool ScrollAnchor::refersTo(const LayoutObject* layoutObject) const
{
    return m_anchorObject == layoutObject;
}

void ScrollAnchor::notifyRemoved(LayoutObject* layoutObject)
{
    if (m_anchorObject == layoutObject)
        clear();
}

} // namespace blink
