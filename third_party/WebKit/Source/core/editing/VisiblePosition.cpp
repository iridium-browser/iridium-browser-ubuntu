/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2011 Motorola Mobility, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/editing/VisiblePosition.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Range.h"
#include "core/dom/Text.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/TextAffinity.h"
#include "core/editing/VisibleUnits.h"
#include "core/html/HTMLElement.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/line/RootInlineBox.h"
#include "platform/geometry/FloatQuad.h"
#include "wtf/text/CString.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace blink {

using namespace HTMLNames;

VisiblePosition::VisiblePosition(const Position &pos, TextAffinity affinity)
{
    init(pos, affinity);
}

VisiblePosition::VisiblePosition(const PositionInComposedTree& pos, TextAffinity affinity)
{
    init(pos, affinity);
}

VisiblePosition::VisiblePosition(const PositionWithAffinity& positionWithAffinity)
{
    init(positionWithAffinity.position(), positionWithAffinity.affinity());
}

VisiblePosition VisiblePosition::next(EditingBoundaryCrossingRule rule) const
{
    VisiblePosition next(nextVisuallyDistinctCandidate(m_deepPosition), m_affinity);

    switch (rule) {
    case CanCrossEditingBoundary:
        return next;
    case CannotCrossEditingBoundary:
        return honorEditingBoundaryAtOrAfter(next);
    case CanSkipOverEditingBoundary:
        return skipToEndOfEditingBoundary(next);
    }
    ASSERT_NOT_REACHED();
    return honorEditingBoundaryAtOrAfter(next);
}

VisiblePosition VisiblePosition::previous(EditingBoundaryCrossingRule rule) const
{
    Position pos = previousVisuallyDistinctCandidate(m_deepPosition);

    // return null visible position if there is no previous visible position
    if (pos.atStartOfTree())
        return VisiblePosition();

    VisiblePosition prev = VisiblePosition(pos);
    ASSERT(prev.deepEquivalent() != m_deepPosition);

#if ENABLE(ASSERT)
    // we should always be able to make the affinity |TextAffinity::Downstream|,
    // because going previous from an |TextAffinity::Upstream| position can
    // never yield another |TextAffinity::Upstream position| (unless line wrap
    // length is 0!).
    if (prev.isNotNull() && m_affinity == TextAffinity::Upstream) {
        ASSERT(inSameLine(PositionWithAffinity(prev.deepEquivalent()), PositionWithAffinity(prev.deepEquivalent(), TextAffinity::Upstream)));
    }
#endif

    switch (rule) {
    case CanCrossEditingBoundary:
        return prev;
    case CannotCrossEditingBoundary:
        return honorEditingBoundaryAtOrBefore(prev);
    case CanSkipOverEditingBoundary:
        return skipToStartOfEditingBoundary(prev);
    }

    ASSERT_NOT_REACHED();
    return honorEditingBoundaryAtOrBefore(prev);
}

Position VisiblePosition::leftVisuallyDistinctCandidate() const
{
    Position p = m_deepPosition;
    if (p.isNull())
        return Position();

    Position downstreamStart = mostForwardCaretPosition(p);
    TextDirection primaryDirection = primaryDirectionOf(*p.anchorNode());

    while (true) {
        InlineBoxPosition boxPosition = computeInlineBoxPosition(p, m_affinity, primaryDirection);
        InlineBox* box = boxPosition.inlineBox;
        int offset = boxPosition.offsetInBox;
        if (!box)
            return primaryDirection == LTR ? previousVisuallyDistinctCandidate(m_deepPosition) : nextVisuallyDistinctCandidate(m_deepPosition);

        LayoutObject* layoutObject = &box->layoutObject();

        while (true) {
            if ((layoutObject->isReplaced() || layoutObject->isBR()) && offset == box->caretRightmostOffset())
                return box->isLeftToRightDirection() ? previousVisuallyDistinctCandidate(m_deepPosition) : nextVisuallyDistinctCandidate(m_deepPosition);

            if (!layoutObject->node()) {
                box = box->prevLeafChild();
                if (!box)
                    return primaryDirection == LTR ? previousVisuallyDistinctCandidate(m_deepPosition) : nextVisuallyDistinctCandidate(m_deepPosition);
                layoutObject = &box->layoutObject();
                offset = box->caretRightmostOffset();
                continue;
            }

            offset = box->isLeftToRightDirection() ? layoutObject->previousOffset(offset) : layoutObject->nextOffset(offset);

            int caretMinOffset = box->caretMinOffset();
            int caretMaxOffset = box->caretMaxOffset();

            if (offset > caretMinOffset && offset < caretMaxOffset)
                break;

            if (box->isLeftToRightDirection() ? offset < caretMinOffset : offset > caretMaxOffset) {
                // Overshot to the left.
                InlineBox* prevBox = box->prevLeafChildIgnoringLineBreak();
                if (!prevBox) {
                    Position positionOnLeft = primaryDirection == LTR ? previousVisuallyDistinctCandidate(m_deepPosition) : nextVisuallyDistinctCandidate(m_deepPosition);
                    if (positionOnLeft.isNull())
                        return Position();

                    InlineBox* boxOnLeft = computeInlineBoxPosition(positionOnLeft, m_affinity, primaryDirection).inlineBox;
                    if (boxOnLeft && boxOnLeft->root() == box->root())
                        return Position();
                    return positionOnLeft;
                }

                // Reposition at the other logical position corresponding to our edge's visual position and go for another round.
                box = prevBox;
                layoutObject = &box->layoutObject();
                offset = prevBox->caretRightmostOffset();
                continue;
            }

            ASSERT(offset == box->caretLeftmostOffset());

            unsigned char level = box->bidiLevel();
            InlineBox* prevBox = box->prevLeafChild();

            if (box->direction() == primaryDirection) {
                if (!prevBox) {
                    InlineBox* logicalStart = 0;
                    if (primaryDirection == LTR ? box->root().getLogicalStartBoxWithNode(logicalStart) : box->root().getLogicalEndBoxWithNode(logicalStart)) {
                        box = logicalStart;
                        layoutObject = &box->layoutObject();
                        offset = primaryDirection == LTR ? box->caretMinOffset() : box->caretMaxOffset();
                    }
                    break;
                }
                if (prevBox->bidiLevel() >= level)
                    break;

                level = prevBox->bidiLevel();

                InlineBox* nextBox = box;
                do {
                    nextBox = nextBox->nextLeafChild();
                } while (nextBox && nextBox->bidiLevel() > level);

                if (nextBox && nextBox->bidiLevel() == level)
                    break;

                box = prevBox;
                layoutObject = &box->layoutObject();
                offset = box->caretRightmostOffset();
                if (box->direction() == primaryDirection)
                    break;
                continue;
            }

            while (prevBox && !prevBox->layoutObject().node())
                prevBox = prevBox->prevLeafChild();

            if (prevBox) {
                box = prevBox;
                layoutObject = &box->layoutObject();
                offset = box->caretRightmostOffset();
                if (box->bidiLevel() > level) {
                    do {
                        prevBox = prevBox->prevLeafChild();
                    } while (prevBox && prevBox->bidiLevel() > level);

                    if (!prevBox || prevBox->bidiLevel() < level)
                        continue;
                }
            } else {
                // Trailing edge of a secondary run. Set to the leading edge of the entire run.
                while (true) {
                    while (InlineBox* nextBox = box->nextLeafChild()) {
                        if (nextBox->bidiLevel() < level)
                            break;
                        box = nextBox;
                    }
                    if (box->bidiLevel() == level)
                        break;
                    level = box->bidiLevel();
                    while (InlineBox* prevBox = box->prevLeafChild()) {
                        if (prevBox->bidiLevel() < level)
                            break;
                        box = prevBox;
                    }
                    if (box->bidiLevel() == level)
                        break;
                    level = box->bidiLevel();
                }
                layoutObject = &box->layoutObject();
                offset = primaryDirection == LTR ? box->caretMinOffset() : box->caretMaxOffset();
            }
            break;
        }

        p = Position::editingPositionOf(layoutObject->node(), offset);

        if ((isVisuallyEquivalentCandidate(p) && mostForwardCaretPosition(p) != downstreamStart) || p.atStartOfTree() || p.atEndOfTree())
            return p;

        ASSERT(p != m_deepPosition);
    }
}

VisiblePosition VisiblePosition::left() const
{
    Position pos = leftVisuallyDistinctCandidate();
    // FIXME: Why can't we move left from the last position in a tree?
    if (pos.atStartOfTree() || pos.atEndOfTree())
        return VisiblePosition();

    VisiblePosition left = VisiblePosition(pos);
    ASSERT(left.deepEquivalent() != m_deepPosition);

    return directionOfEnclosingBlock(left.deepEquivalent()) == LTR ? honorEditingBoundaryAtOrBefore(left) : honorEditingBoundaryAtOrAfter(left);
}

Position VisiblePosition::rightVisuallyDistinctCandidate() const
{
    Position p = m_deepPosition;
    if (p.isNull())
        return Position();

    Position downstreamStart = mostForwardCaretPosition(p);
    TextDirection primaryDirection = primaryDirectionOf(*p.anchorNode());

    while (true) {
        InlineBoxPosition boxPosition = computeInlineBoxPosition(p, m_affinity, primaryDirection);
        InlineBox* box = boxPosition.inlineBox;
        int offset = boxPosition.offsetInBox;
        if (!box)
            return primaryDirection == LTR ? nextVisuallyDistinctCandidate(m_deepPosition) : previousVisuallyDistinctCandidate(m_deepPosition);

        LayoutObject* layoutObject = &box->layoutObject();

        while (true) {
            if ((layoutObject->isReplaced() || layoutObject->isBR()) && offset == box->caretLeftmostOffset())
                return box->isLeftToRightDirection() ? nextVisuallyDistinctCandidate(m_deepPosition) : previousVisuallyDistinctCandidate(m_deepPosition);

            if (!layoutObject->node()) {
                box = box->nextLeafChild();
                if (!box)
                    return primaryDirection == LTR ? nextVisuallyDistinctCandidate(m_deepPosition) : previousVisuallyDistinctCandidate(m_deepPosition);
                layoutObject = &box->layoutObject();
                offset = box->caretLeftmostOffset();
                continue;
            }

            offset = box->isLeftToRightDirection() ? layoutObject->nextOffset(offset) : layoutObject->previousOffset(offset);

            int caretMinOffset = box->caretMinOffset();
            int caretMaxOffset = box->caretMaxOffset();

            if (offset > caretMinOffset && offset < caretMaxOffset)
                break;

            if (box->isLeftToRightDirection() ? offset > caretMaxOffset : offset < caretMinOffset) {
                // Overshot to the right.
                InlineBox* nextBox = box->nextLeafChildIgnoringLineBreak();
                if (!nextBox) {
                    Position positionOnRight = primaryDirection == LTR ? nextVisuallyDistinctCandidate(m_deepPosition) : previousVisuallyDistinctCandidate(m_deepPosition);
                    if (positionOnRight.isNull())
                        return Position();

                    InlineBox* boxOnRight = computeInlineBoxPosition(positionOnRight, m_affinity, primaryDirection).inlineBox;
                    if (boxOnRight && boxOnRight->root() == box->root())
                        return Position();
                    return positionOnRight;
                }

                // Reposition at the other logical position corresponding to our edge's visual position and go for another round.
                box = nextBox;
                layoutObject = &box->layoutObject();
                offset = nextBox->caretLeftmostOffset();
                continue;
            }

            ASSERT(offset == box->caretRightmostOffset());

            unsigned char level = box->bidiLevel();
            InlineBox* nextBox = box->nextLeafChild();

            if (box->direction() == primaryDirection) {
                if (!nextBox) {
                    InlineBox* logicalEnd = 0;
                    if (primaryDirection == LTR ? box->root().getLogicalEndBoxWithNode(logicalEnd) : box->root().getLogicalStartBoxWithNode(logicalEnd)) {
                        box = logicalEnd;
                        layoutObject = &box->layoutObject();
                        offset = primaryDirection == LTR ? box->caretMaxOffset() : box->caretMinOffset();
                    }
                    break;
                }

                if (nextBox->bidiLevel() >= level)
                    break;

                level = nextBox->bidiLevel();

                InlineBox* prevBox = box;
                do {
                    prevBox = prevBox->prevLeafChild();
                } while (prevBox && prevBox->bidiLevel() > level);

                if (prevBox && prevBox->bidiLevel() == level) // For example, abc FED 123 ^ CBA
                    break;

                // For example, abc 123 ^ CBA or 123 ^ CBA abc
                box = nextBox;
                layoutObject = &box->layoutObject();
                offset = box->caretLeftmostOffset();
                if (box->direction() == primaryDirection)
                    break;
                continue;
            }

            while (nextBox && !nextBox->layoutObject().node())
                nextBox = nextBox->nextLeafChild();

            if (nextBox) {
                box = nextBox;
                layoutObject = &box->layoutObject();
                offset = box->caretLeftmostOffset();

                if (box->bidiLevel() > level) {
                    do {
                        nextBox = nextBox->nextLeafChild();
                    } while (nextBox && nextBox->bidiLevel() > level);

                    if (!nextBox || nextBox->bidiLevel() < level)
                        continue;
                }
            } else {
                // Trailing edge of a secondary run. Set to the leading edge of the entire run.
                while (true) {
                    while (InlineBox* prevBox = box->prevLeafChild()) {
                        if (prevBox->bidiLevel() < level)
                            break;
                        box = prevBox;
                    }
                    if (box->bidiLevel() == level)
                        break;
                    level = box->bidiLevel();
                    while (InlineBox* nextBox = box->nextLeafChild()) {
                        if (nextBox->bidiLevel() < level)
                            break;
                        box = nextBox;
                    }
                    if (box->bidiLevel() == level)
                        break;
                    level = box->bidiLevel();
                }
                layoutObject = &box->layoutObject();
                offset = primaryDirection == LTR ? box->caretMaxOffset() : box->caretMinOffset();
            }
            break;
        }

        p = Position::editingPositionOf(layoutObject->node(), offset);

        if ((isVisuallyEquivalentCandidate(p) && mostForwardCaretPosition(p) != downstreamStart) || p.atStartOfTree() || p.atEndOfTree())
            return p;

        ASSERT(p != m_deepPosition);
    }
}

VisiblePosition VisiblePosition::right() const
{
    Position pos = rightVisuallyDistinctCandidate();
    // FIXME: Why can't we move left from the last position in a tree?
    if (pos.atStartOfTree() || pos.atEndOfTree())
        return VisiblePosition();

    VisiblePosition right = VisiblePosition(pos);
    ASSERT(right.deepEquivalent() != m_deepPosition);

    return directionOfEnclosingBlock(right.deepEquivalent()) == LTR ? honorEditingBoundaryAtOrAfter(right) : honorEditingBoundaryAtOrBefore(right);
}

template <typename Strategy>
PositionWithAffinityTemplate<Strategy> honorEditingBoundaryAtOrBeforeAlgorithm(const PositionWithAffinityTemplate<Strategy>& pos, const PositionAlgorithm<Strategy>& anchor)
{
    if (pos.isNull())
        return pos;

    ContainerNode* highestRoot = highestEditableRoot(anchor);

    // Return empty position if pos is not somewhere inside the editable region containing this position
    if (highestRoot && !pos.position().anchorNode()->isDescendantOf(highestRoot))
        return PositionWithAffinityTemplate<Strategy>();

    // Return pos itself if the two are from the very same editable region, or both are non-editable
    // FIXME: In the non-editable case, just because the new position is non-editable doesn't mean movement
    // to it is allowed.  VisibleSelection::adjustForEditableContent has this problem too.
    if (highestEditableRoot(pos.position()) == highestRoot)
        return pos;

    // Return empty position if this position is non-editable, but pos is editable
    // FIXME: Move to the previous non-editable region.
    if (!highestRoot)
        return PositionWithAffinityTemplate<Strategy>();

    // Return the last position before pos that is in the same editable region as this position
    return lastEditablePositionBeforePositionInRoot(pos.position(), highestRoot);
}

PositionWithAffinity honorEditingBoundaryAtOrBeforeOf(const PositionWithAffinity& pos, const Position& anchor)
{
    return honorEditingBoundaryAtOrBeforeAlgorithm(pos, anchor);
}

PositionInComposedTreeWithAffinity honorEditingBoundaryAtOrBeforeOf(const PositionInComposedTreeWithAffinity& pos, const PositionInComposedTree& anchor)
{
    return honorEditingBoundaryAtOrBeforeAlgorithm(pos, anchor);
}

VisiblePosition VisiblePosition::honorEditingBoundaryAtOrBefore(const VisiblePosition &pos) const
{
    return VisiblePosition(honorEditingBoundaryAtOrBeforeOf(pos.toPositionWithAffinity(), deepEquivalent()));
}

VisiblePosition VisiblePosition::honorEditingBoundaryAtOrAfter(const VisiblePosition &pos) const
{
    if (pos.isNull())
        return pos;

    ContainerNode* highestRoot = highestEditableRoot(deepEquivalent());

    // Return empty position if pos is not somewhere inside the editable region containing this position
    if (highestRoot && !pos.deepEquivalent().anchorNode()->isDescendantOf(highestRoot))
        return VisiblePosition();

    // Return pos itself if the two are from the very same editable region, or both are non-editable
    // FIXME: In the non-editable case, just because the new position is non-editable doesn't mean movement
    // to it is allowed.  VisibleSelection::adjustForEditableContent has this problem too.
    if (highestEditableRoot(pos.deepEquivalent()) == highestRoot)
        return pos;

    // Return empty position if this position is non-editable, but pos is editable
    // FIXME: Move to the next non-editable region.
    if (!highestRoot)
        return VisiblePosition();

    // Return the next position after pos that is in the same editable region as this position
    return firstEditableVisiblePositionAfterPositionInRoot(pos.deepEquivalent(), highestRoot);
}

VisiblePosition VisiblePosition::skipToStartOfEditingBoundary(const VisiblePosition &pos) const
{
    if (pos.isNull())
        return pos;

    ContainerNode* highestRoot = highestEditableRoot(deepEquivalent());
    ContainerNode* highestRootOfPos = highestEditableRoot(pos.deepEquivalent());

    // Return pos itself if the two are from the very same editable region, or both are non-editable.
    if (highestRootOfPos == highestRoot)
        return pos;

    // If this is not editable but |pos| has an editable root, skip to the start
    if (!highestRoot && highestRootOfPos)
        return VisiblePosition(previousVisuallyDistinctCandidate(Position(highestRootOfPos, PositionAnchorType::BeforeAnchor).parentAnchoredEquivalent()));

    // That must mean that |pos| is not editable. Return the last position before pos that is in the same editable region as this position
    return lastEditableVisiblePositionBeforePositionInRoot(pos.deepEquivalent(), highestRoot);
}

VisiblePosition VisiblePosition::skipToEndOfEditingBoundary(const VisiblePosition &pos) const
{
    if (pos.isNull())
        return pos;

    ContainerNode* highestRoot = highestEditableRoot(deepEquivalent());
    ContainerNode* highestRootOfPos = highestEditableRoot(pos.deepEquivalent());

    // Return pos itself if the two are from the very same editable region, or both are non-editable.
    if (highestRootOfPos == highestRoot)
        return pos;

    // If this is not editable but |pos| has an editable root, skip to the end
    if (!highestRoot && highestRootOfPos)
        return VisiblePosition(Position(highestRootOfPos, PositionAnchorType::AfterAnchor).parentAnchoredEquivalent());

    // That must mean that |pos| is not editable. Return the next position after pos that is in the same editable region as this position
    return firstEditableVisiblePositionAfterPositionInRoot(pos.deepEquivalent(), highestRoot);
}

template <typename PositionType>
static PositionType canonicalizeCandidate(const PositionType& candidate)
{
    if (candidate.isNull())
        return PositionType();
    ASSERT(isVisuallyEquivalentCandidate(candidate));
    PositionType upstream = mostBackwardCaretPosition(candidate);
    if (isVisuallyEquivalentCandidate(upstream))
        return upstream;
    return candidate;
}

template <typename PositionType>
static PositionType canonicalPosition(const PositionType& passedPosition)
{
    // Sometimes updating selection positions can be extremely expensive and occur
    // frequently.  Often calling preventDefault on mousedown events can avoid
    // doing unnecessary text selection work.  http://crbug.com/472258.
    TRACE_EVENT0("blink", "VisiblePosition::canonicalPosition");

    // The updateLayout call below can do so much that even the position passed
    // in to us might get changed as a side effect. Specifically, there are code
    // paths that pass selection endpoints, and updateLayout can change the selection.
    PositionType position = passedPosition;

    // FIXME (9535):  Canonicalizing to the leftmost candidate means that if we're at a line wrap, we will
    // ask layoutObjects to paint downstream carets for other layoutObjects.
    // To fix this, we need to either a) add code to all paintCarets to pass the responsibility off to
    // the appropriate layoutObject for VisiblePosition's like these, or b) canonicalize to the rightmost candidate
    // unless the affinity is upstream.
    if (position.isNull())
        return PositionType();

    ASSERT(position.document());
    position.document()->updateLayoutIgnorePendingStylesheets();

    Node* node = position.computeContainerNode();

    PositionType candidate = mostBackwardCaretPosition(position);
    if (isVisuallyEquivalentCandidate(candidate))
        return candidate;
    candidate = mostForwardCaretPosition(position);
    if (isVisuallyEquivalentCandidate(candidate))
        return candidate;

    // When neither upstream or downstream gets us to a candidate (upstream/downstream won't leave
    // blocks or enter new ones), we search forward and backward until we find one.
    PositionType next = canonicalizeCandidate(nextCandidate(position));
    PositionType prev = canonicalizeCandidate(previousCandidate(position));
    Node* nextNode = next.anchorNode();
    Node* prevNode = prev.anchorNode();

    // The new position must be in the same editable element. Enforce that first.
    // Unless the descent is from a non-editable html element to an editable body.
    if (isHTMLHtmlElement(node) && !node->hasEditableStyle() && node->document().body() && node->document().body()->hasEditableStyle())
        return next.isNotNull() ? next : prev;

    Element* editingRoot = editableRootForPosition(position);

    // If the html element is editable, descending into its body will look like a descent
    // from non-editable to editable content since rootEditableElement() always stops at the body.
    if (isHTMLHtmlElement(editingRoot) || position.anchorNode()->isDocumentNode())
        return next.isNotNull() ? next : prev;

    bool prevIsInSameEditableElement = prevNode && editableRootForPosition(prev) == editingRoot;
    bool nextIsInSameEditableElement = nextNode && editableRootForPosition(next) == editingRoot;
    if (prevIsInSameEditableElement && !nextIsInSameEditableElement)
        return prev;

    if (nextIsInSameEditableElement && !prevIsInSameEditableElement)
        return next;

    if (!nextIsInSameEditableElement && !prevIsInSameEditableElement)
        return PositionType();

    // The new position should be in the same block flow element. Favor that.
    Element* originalBlock = node ? enclosingBlockFlowElement(*node) : 0;
    bool nextIsOutsideOriginalBlock = !nextNode->isDescendantOf(originalBlock) && nextNode != originalBlock;
    bool prevIsOutsideOriginalBlock = !prevNode->isDescendantOf(originalBlock) && prevNode != originalBlock;
    if (nextIsOutsideOriginalBlock && !prevIsOutsideOriginalBlock)
        return prev;

    return next;
}

Position canonicalPositionOf(const Position& position)
{
    return canonicalPosition(position);
}

PositionInComposedTree canonicalPositionOf(const PositionInComposedTree& position)
{
    return canonicalPosition(position);
}

template<typename Strategy>
void VisiblePosition::init(const PositionAlgorithm<Strategy>& position, TextAffinity affinity)
{
    m_affinity = affinity;

    PositionAlgorithm<Strategy> deepPosition = canonicalPosition(position);
    m_deepPosition = toPositionInDOMTree(deepPosition);

    if (m_affinity != TextAffinity::Upstream)
        return;

    if (isNull()) {
        m_affinity = TextAffinity::Downstream;
        return;
    }

    // When not at a line wrap, make sure to end up with
    // |TextAffinity::Downstream| affinity.
    if (!inSameLine(PositionWithAffinityTemplate<Strategy>(deepPosition), PositionWithAffinityTemplate<Strategy>(deepPosition, TextAffinity::Upstream)))
        return;
    m_affinity = TextAffinity::Downstream;
}

UChar32 VisiblePosition::characterAfter() const
{
    // We canonicalize to the first of two equivalent candidates, but the second
    // of the two candidates is the one that will be inside the text node
    // containing the character after this visible position.
    Position pos = mostForwardCaretPosition(m_deepPosition);
    if (!pos.isOffsetInAnchor())
        return 0;
    Node* containerNode = pos.computeContainerNode();
    if (!containerNode || !containerNode->isTextNode())
        return 0;
    unsigned offset = static_cast<unsigned>(pos.offsetInContainerNode());
    Text* textNode = toText(containerNode);
    unsigned length = textNode->length();
    if (offset >= length)
        return 0;

    return textNode->data().characterStartingAt(offset);
}

LayoutRect VisiblePosition::localCaretRect(LayoutObject*& layoutObject) const
{
    PositionWithAffinity positionWithAffinity(m_deepPosition, m_affinity);
    return localCaretRectOfPosition(positionWithAffinity, layoutObject);
}

IntRect VisiblePosition::absoluteCaretBounds() const
{
    LayoutObject* layoutObject;
    LayoutRect localRect = localCaretRect(layoutObject);
    if (localRect.isEmpty() || !layoutObject)
        return IntRect();

    return layoutObject->localToAbsoluteQuad(FloatRect(localRect)).enclosingBoundingBox();
}

int VisiblePosition::lineDirectionPointForBlockDirectionNavigation() const
{
    if (isNull())
        return 0;

    LayoutObject* layoutObject;
    LayoutRect localRect = localCaretRect(layoutObject);
    if (localRect.isEmpty() || !layoutObject)
        return 0;

    // This ignores transforms on purpose, for now. Vertical navigation is done
    // without consulting transforms, so that 'up' in transformed text is 'up'
    // relative to the text, not absolute 'up'.
    FloatPoint caretPoint = layoutObject->localToAbsolute(FloatPoint(localRect.location()));
    LayoutObject* containingBlock = layoutObject->containingBlock();
    if (!containingBlock)
        containingBlock = layoutObject; // Just use ourselves to determine the writing mode if we have no containing block.
    return containingBlock->isHorizontalWritingMode() ? caretPoint.x() : caretPoint.y();
}

#ifndef NDEBUG

void VisiblePosition::debugPosition(const char* msg) const
{
    if (isNull()) {
        fprintf(stderr, "Position [%s]: null\n", msg);
        return;
    }
    m_deepPosition.debugPosition(msg);
}

void VisiblePosition::formatForDebugger(char* buffer, unsigned length) const
{
    m_deepPosition.formatForDebugger(buffer, length);
}

void VisiblePosition::showTreeForThis() const
{
    m_deepPosition.showTreeForThis();
}

#endif

EphemeralRange makeRange(const VisiblePosition &start, const VisiblePosition &end)
{
    if (start.isNull() || end.isNull())
        return EphemeralRange();

    Position s = start.deepEquivalent().parentAnchoredEquivalent();
    Position e = end.deepEquivalent().parentAnchoredEquivalent();
    if (s.isNull() || e.isNull())
        return EphemeralRange();

    return EphemeralRange(s, e);
}

DEFINE_TRACE(VisiblePosition)
{
    visitor->trace(m_deepPosition);
}

} // namespace blink

#ifndef NDEBUG

void showTree(const blink::VisiblePosition* vpos)
{
    if (vpos)
        vpos->showTreeForThis();
    else
        fprintf(stderr, "Cannot showTree for (nil) VisiblePosition.\n");
}

void showTree(const blink::VisiblePosition& vpos)
{
    vpos.showTreeForThis();
}

#endif
