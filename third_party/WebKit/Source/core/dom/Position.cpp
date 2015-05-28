/*
 * Copyright (C) 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
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
#include "core/dom/Position.h"

#include "core/HTMLNames.h"
#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/dom/PositionIterator.h"
#include "core/dom/Text.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/htmlediting.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLTableElement.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutText.h"
#include "core/layout/line/InlineIterator.h"
#include "core/layout/line/InlineTextBox.h"
#include "platform/Logging.h"
#include "wtf/text/CString.h"
#include "wtf/unicode/CharacterNames.h"
#include <stdio.h>

namespace blink {

using namespace HTMLNames;

static Node* nextRenderedEditable(Node* node)
{
    for (node = node->nextLeafNode(); node; node = node->nextLeafNode()) {
        LayoutObject* renderer = node->layoutObject();
        if (!renderer)
            continue;
        if (!node->hasEditableStyle())
            continue;
        if ((renderer->isBox() && toLayoutBox(renderer)->inlineBoxWrapper()) || (renderer->isText() && toLayoutText(renderer)->firstTextBox()))
            return node;
    }
    return 0;
}

static Node* previousRenderedEditable(Node* node)
{
    for (node = node->previousLeafNode(); node; node = node->previousLeafNode()) {
        LayoutObject* renderer = node->layoutObject();
        if (!renderer)
            continue;
        if (!node->hasEditableStyle())
            continue;
        if ((renderer->isBox() && toLayoutBox(renderer)->inlineBoxWrapper()) || (renderer->isText() && toLayoutText(renderer)->firstTextBox()))
            return node;
    }
    return 0;
}

const TreeScope* Position::commonAncestorTreeScope(const Position& a, const Position& b)
{
    if (!a.containerNode() || !b.containerNode())
        return nullptr;
    return a.containerNode()->treeScope().commonAncestorTreeScope(b.containerNode()->treeScope());
}

Position::Position(PassRefPtrWillBeRawPtr<Node> anchorNode, LegacyEditingOffset offset)
    : m_anchorNode(anchorNode)
    , m_offset(offset.value())
    , m_anchorType(anchorTypeForLegacyEditingPosition(m_anchorNode.get(), m_offset))
    , m_isLegacyEditingPosition(true)
{
    ASSERT(!m_anchorNode || !m_anchorNode->isPseudoElement() || m_anchorNode->isFirstLetterPseudoElement());
}

Position::Position(PassRefPtrWillBeRawPtr<Node> anchorNode, AnchorType anchorType)
    : m_anchorNode(anchorNode)
    , m_offset(0)
    , m_anchorType(anchorType)
    , m_isLegacyEditingPosition(false)
{
    ASSERT(!m_anchorNode || !m_anchorNode->isPseudoElement() || m_anchorNode->isFirstLetterPseudoElement());

    ASSERT(anchorType != PositionIsOffsetInAnchor);
    ASSERT(!((anchorType == PositionIsBeforeChildren || anchorType == PositionIsAfterChildren)
        && (m_anchorNode->isTextNode() || editingIgnoresContent(m_anchorNode.get()))));
}

Position::Position(PassRefPtrWillBeRawPtr<Node> anchorNode, int offset, AnchorType anchorType)
    : m_anchorNode(anchorNode)
    , m_offset(offset)
    , m_anchorType(anchorType)
    , m_isLegacyEditingPosition(false)
{
    ASSERT(!m_anchorNode || !m_anchorNode->isPseudoElement() || m_anchorNode->isFirstLetterPseudoElement());

    ASSERT(anchorType == PositionIsOffsetInAnchor);
}

Position::Position(PassRefPtrWillBeRawPtr<Text> textNode, unsigned offset)
    : m_anchorNode(textNode)
    , m_offset(static_cast<int>(offset))
    , m_anchorType(PositionIsOffsetInAnchor)
    , m_isLegacyEditingPosition(false)
{
    ASSERT(m_anchorNode);
}

void Position::moveToPosition(PassRefPtrWillBeRawPtr<Node> node, int offset)
{
    ASSERT(!editingIgnoresContent(node.get()));
    ASSERT(anchorType() == PositionIsOffsetInAnchor || m_isLegacyEditingPosition);
    m_anchorNode = node;
    m_offset = offset;
    if (m_isLegacyEditingPosition)
        m_anchorType = anchorTypeForLegacyEditingPosition(m_anchorNode.get(), m_offset);
}
void Position::moveToOffset(int offset)
{
    ASSERT(anchorType() == PositionIsOffsetInAnchor || m_isLegacyEditingPosition);
    m_offset = offset;
    if (m_isLegacyEditingPosition)
        m_anchorType = anchorTypeForLegacyEditingPosition(m_anchorNode.get(), m_offset);
}

Node* Position::containerNode() const
{
    if (!m_anchorNode)
        return 0;

    switch (anchorType()) {
    case PositionIsBeforeChildren:
    case PositionIsAfterChildren:
    case PositionIsOffsetInAnchor:
        return m_anchorNode.get();
    case PositionIsBeforeAnchor:
    case PositionIsAfterAnchor:
        return m_anchorNode->parentNode();
    }
    ASSERT_NOT_REACHED();
    return 0;
}

Text* Position::containerText() const
{
    switch (anchorType()) {
    case PositionIsOffsetInAnchor:
        return m_anchorNode && m_anchorNode->isTextNode() ? toText(m_anchorNode) : 0;
    case PositionIsBeforeAnchor:
    case PositionIsAfterAnchor:
        return 0;
    case PositionIsBeforeChildren:
    case PositionIsAfterChildren:
        ASSERT(!m_anchorNode || !m_anchorNode->isTextNode());
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

int Position::computeOffsetInContainerNode() const
{
    if (!m_anchorNode)
        return 0;

    switch (anchorType()) {
    case PositionIsBeforeChildren:
        return 0;
    case PositionIsAfterChildren:
        return lastOffsetInNode(m_anchorNode.get());
    case PositionIsOffsetInAnchor:
        return minOffsetForNode(m_anchorNode.get(), m_offset);
    case PositionIsBeforeAnchor:
        return m_anchorNode->nodeIndex();
    case PositionIsAfterAnchor:
        return m_anchorNode->nodeIndex() + 1;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

int Position::offsetForPositionAfterAnchor() const
{
    ASSERT(m_anchorType == PositionIsAfterAnchor || m_anchorType == PositionIsAfterChildren);
    ASSERT(!m_isLegacyEditingPosition);
    return lastOffsetForEditing(m_anchorNode.get());
}

// Neighbor-anchored positions are invalid DOM positions, so they need to be
// fixed up before handing them off to the Range object.
Position Position::parentAnchoredEquivalent() const
{
    if (!m_anchorNode)
        return Position();

    // FIXME: This should only be necessary for legacy positions, but is also needed for positions before and after Tables
    if (m_offset <= 0 && (m_anchorType != PositionIsAfterAnchor && m_anchorType != PositionIsAfterChildren)) {
        if (m_anchorNode->parentNode() && (editingIgnoresContent(m_anchorNode.get()) || isRenderedHTMLTableElement(m_anchorNode.get())))
            return positionInParentBeforeNode(*m_anchorNode);
        return Position(m_anchorNode.get(), 0, PositionIsOffsetInAnchor);
    }
    if (!m_anchorNode->offsetInCharacters()
        && (m_anchorType == PositionIsAfterAnchor || m_anchorType == PositionIsAfterChildren || static_cast<unsigned>(m_offset) == m_anchorNode->countChildren())
        && (editingIgnoresContent(m_anchorNode.get()) || isRenderedHTMLTableElement(m_anchorNode.get()))
        && containerNode()) {
        return positionInParentAfterNode(*m_anchorNode);
    }

    return Position(containerNode(), computeOffsetInContainerNode(), PositionIsOffsetInAnchor);
}

Position Position::toOffsetInAnchor() const
{
    if (isNull())
        return Position();

    return Position(containerNode(), computeOffsetInContainerNode(), Position::PositionIsOffsetInAnchor);
}

Node* Position::computeNodeBeforePosition() const
{
    if (!m_anchorNode)
        return 0;
    switch (anchorType()) {
    case PositionIsBeforeChildren:
        return 0;
    case PositionIsAfterChildren:
        return m_anchorNode->lastChild();
    case PositionIsOffsetInAnchor:
        return m_offset ? NodeTraversal::childAt(*m_anchorNode, m_offset - 1) : 0;
    case PositionIsBeforeAnchor:
        return m_anchorNode->previousSibling();
    case PositionIsAfterAnchor:
        return m_anchorNode.get();
    }
    ASSERT_NOT_REACHED();
    return 0;
}

Node* Position::computeNodeAfterPosition() const
{
    if (!m_anchorNode)
        return 0;

    switch (anchorType()) {
    case PositionIsBeforeChildren:
        return m_anchorNode->firstChild();
    case PositionIsAfterChildren:
        return 0;
    case PositionIsOffsetInAnchor:
        return NodeTraversal::childAt(*m_anchorNode, m_offset);
    case PositionIsBeforeAnchor:
        return m_anchorNode.get();
    case PositionIsAfterAnchor:
        return m_anchorNode->nextSibling();
    }
    ASSERT_NOT_REACHED();
    return 0;
}

// An implementation of |Range::firstNode()|.
Node* Position::nodeAsRangeFirstNode() const
{
    ASSERT(m_anchorType == PositionIsOffsetInAnchor);
    if (!m_anchorNode)
        return nullptr;
    if (m_anchorNode->offsetInCharacters())
        return m_anchorNode.get();
    if (Node* child = NodeTraversal::childAt(*m_anchorNode, m_offset))
        return child;
    if (!m_offset)
        return m_anchorNode.get();
    return NodeTraversal::nextSkippingChildren(*m_anchorNode);
}

// An implementation of |Range::pastLastNode()|.
Node* Position::nodeAsRangePastLastNode() const
{
    ASSERT(m_anchorType == PositionIsOffsetInAnchor);
    if (!m_anchorNode)
        return nullptr;
    if (m_anchorNode->offsetInCharacters())
        return NodeTraversal::nextSkippingChildren(*m_anchorNode);
    if (Node* child = NodeTraversal::childAt(*m_anchorNode, m_offset))
        return child;
    return NodeTraversal::nextSkippingChildren(*m_anchorNode);
}

Position::AnchorType Position::anchorTypeForLegacyEditingPosition(Node* anchorNode, int offset)
{
    if (anchorNode && editingIgnoresContent(anchorNode)) {
        if (offset == 0)
            return Position::PositionIsBeforeAnchor;
        return Position::PositionIsAfterAnchor;
    }
    return Position::PositionIsOffsetInAnchor;
}

// FIXME: This method is confusing (does it return anchorNode() or containerNode()?) and should be renamed or removed
Element* Position::element() const
{
    Node* node = anchorNode();
    if (!node || node->isElementNode())
        return toElement(node);
    return node->parentElement();
}

PassRefPtrWillBeRawPtr<CSSComputedStyleDeclaration> Position::ensureComputedStyle() const
{
    Element* elem = element();
    if (!elem)
        return nullptr;
    return CSSComputedStyleDeclaration::create(elem);
}

Position Position::previous(PositionMoveType moveType) const
{
    Node* node = deprecatedNode();
    if (!node)
        return *this;

    int offset = deprecatedEditingOffset();
    // FIXME: Negative offsets shouldn't be allowed. We should catch this earlier.
    ASSERT(offset >= 0);

    if (offset > 0) {
        if (Node* child = NodeTraversal::childAt(*node, offset - 1))
            return lastPositionInOrAfterNode(child);

        // There are two reasons child might be 0:
        //   1) The node is node like a text node that is not an element, and therefore has no children.
        //      Going backward one character at a time is correct.
        //   2) The old offset was a bogus offset like (<br>, 1), and there is no child.
        //      Going from 1 to 0 is correct.
        switch (moveType) {
        case CodePoint:
            return createLegacyEditingPosition(node, offset - 1);
        case Character:
            return createLegacyEditingPosition(node, uncheckedPreviousOffset(node, offset));
        case BackwardDeletion:
            return createLegacyEditingPosition(node, uncheckedPreviousOffsetForBackwardDeletion(node, offset));
        }
    }

    if (ContainerNode* parent = node->parentNode())
        return createLegacyEditingPosition(parent, node->nodeIndex());
    return *this;
}

Position Position::next(PositionMoveType moveType) const
{
    ASSERT(moveType != BackwardDeletion);

    Node* node = deprecatedNode();
    if (!node)
        return *this;

    int offset = deprecatedEditingOffset();
    // FIXME: Negative offsets shouldn't be allowed. We should catch this earlier.
    ASSERT(offset >= 0);

    if (Node* child = NodeTraversal::childAt(*node, offset))
        return firstPositionInOrBeforeNode(child);

    if (!node->hasChildren() && offset < lastOffsetForEditing(node)) {
        // There are two reasons child might be 0:
        //   1) The node is node like a text node that is not an element, and therefore has no children.
        //      Going forward one character at a time is correct.
        //   2) The new offset is a bogus offset like (<br>, 1), and there is no child.
        //      Going from 0 to 1 is correct.
        return createLegacyEditingPosition(node, (moveType == Character) ? uncheckedNextOffset(node, offset) : offset + 1);
    }

    if (ContainerNode* parent = node->parentNode())
        return createLegacyEditingPosition(parent, node->nodeIndex() + 1);
    return *this;
}

int Position::uncheckedPreviousOffset(const Node* n, int current)
{
    return n->layoutObject() ? n->layoutObject()->previousOffset(current) : current - 1;
}

int Position::uncheckedPreviousOffsetForBackwardDeletion(const Node* n, int current)
{
    return n->layoutObject() ? n->layoutObject()->previousOffsetForBackwardDeletion(current) : current - 1;
}

int Position::uncheckedNextOffset(const Node* n, int current)
{
    return n->layoutObject() ? n->layoutObject()->nextOffset(current) : current + 1;
}

bool Position::atFirstEditingPositionForNode() const
{
    if (isNull())
        return true;
    // FIXME: Position before anchor shouldn't be considered as at the first editing position for node
    // since that position resides outside of the node.
    switch (m_anchorType) {
    case PositionIsOffsetInAnchor:
        return m_offset <= 0;
    case PositionIsBeforeChildren:
    case PositionIsBeforeAnchor:
        return true;
    case PositionIsAfterChildren:
    case PositionIsAfterAnchor:
        return !lastOffsetForEditing(deprecatedNode());
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool Position::atLastEditingPositionForNode() const
{
    if (isNull())
        return true;
    // FIXME: Position after anchor shouldn't be considered as at the first editing position for node
    // since that position resides outside of the node.
    return m_anchorType == PositionIsAfterAnchor || m_anchorType == PositionIsAfterChildren || m_offset >= lastOffsetForEditing(deprecatedNode());
}

// A position is considered at editing boundary if one of the following is true:
// 1. It is the first position in the node and the next visually equivalent position
//    is non editable.
// 2. It is the last position in the node and the previous visually equivalent position
//    is non editable.
// 3. It is an editable position and both the next and previous visually equivalent
//    positions are both non editable.
bool Position::atEditingBoundary() const
{
    Position nextPosition = downstream(CanCrossEditingBoundary);
    if (atFirstEditingPositionForNode() && nextPosition.isNotNull() && !nextPosition.deprecatedNode()->hasEditableStyle())
        return true;

    Position prevPosition = upstream(CanCrossEditingBoundary);
    if (atLastEditingPositionForNode() && prevPosition.isNotNull() && !prevPosition.deprecatedNode()->hasEditableStyle())
        return true;

    return nextPosition.isNotNull() && !nextPosition.deprecatedNode()->hasEditableStyle()
        && prevPosition.isNotNull() && !prevPosition.deprecatedNode()->hasEditableStyle();
}

Node* Position::parentEditingBoundary() const
{
    if (!m_anchorNode)
        return 0;

    Node* documentElement = m_anchorNode->document().documentElement();
    if (!documentElement)
        return 0;

    Node* boundary = containerNode();
    while (boundary != documentElement && boundary->nonShadowBoundaryParentNode() && m_anchorNode->hasEditableStyle() == boundary->parentNode()->hasEditableStyle())
        boundary = boundary->nonShadowBoundaryParentNode();

    return boundary;
}


bool Position::atStartOfTree() const
{
    if (isNull())
        return true;
    return !deprecatedNode()->parentNode() && m_offset <= 0;
}

bool Position::atEndOfTree() const
{
    if (isNull())
        return true;
    return !deprecatedNode()->parentNode() && m_offset >= lastOffsetForEditing(deprecatedNode());
}

int Position::renderedOffset() const
{
    if (!deprecatedNode()->isTextNode())
        return m_offset;

    if (!deprecatedNode()->layoutObject())
        return m_offset;

    int result = 0;
    LayoutText* textRenderer = toLayoutText(deprecatedNode()->layoutObject());
    for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
        int start = box->start();
        int end = box->start() + box->len();
        if (m_offset < start)
            return result;
        if (m_offset <= end) {
            result += m_offset - start;
            return result;
        }
        result += box->len();
    }
    return result;
}

// Whether or not [node, 0] and [node, lastOffsetForEditing(node)] are their own VisiblePositions.
// If true, adjacent candidates are visually distinct.
// FIXME: Disregard nodes with renderers that have no height, as we do in isCandidate.
// FIXME: Share code with isCandidate, if possible.
static bool endsOfNodeAreVisuallyDistinctPositions(Node* node)
{
    if (!node || !node->layoutObject())
        return false;

    if (!node->layoutObject()->isInline())
        return true;

    // Don't include inline tables.
    if (isHTMLTableElement(*node))
        return false;

    // A Marquee elements are moving so we should assume their ends are always
    // visibily distinct.
    if (isHTMLMarqueeElement(*node))
        return true;

    // There is a VisiblePosition inside an empty inline-block container.
    return node->layoutObject()->isReplaced() && canHaveChildrenForEditing(node) && toLayoutBox(node->layoutObject())->size().height() != 0 && !node->hasChildren();
}

static Node* enclosingVisualBoundary(Node* node)
{
    while (node && !endsOfNodeAreVisuallyDistinctPositions(node))
        node = node->parentNode();

    return node;
}

// upstream() and downstream() want to return positions that are either in a
// text node or at just before a non-text node.  This method checks for that.
static bool isStreamer(const PositionIterator& pos)
{
    if (!pos.node())
        return true;

    if (isAtomicNode(pos.node()))
        return true;

    return pos.atStartOfNode();
}

// This function and downstream() are used for moving back and forth between visually equivalent candidates.
// For example, for the text node "foo     bar" where whitespace is collapsible, there are two candidates
// that map to the VisiblePosition between 'b' and the space.  This function will return the left candidate
// and downstream() will return the right one.
// Also, upstream() will return [boundary, 0] for any of the positions from [boundary, 0] to the first candidate
// in boundary, where endsOfNodeAreVisuallyDistinctPositions(boundary) is true.
Position Position::upstream(EditingBoundaryCrossingRule rule) const
{
    Node* startNode = deprecatedNode();
    if (!startNode)
        return Position();

    // iterate backward from there, looking for a qualified position
    Node* boundary = enclosingVisualBoundary(startNode);
    // FIXME: PositionIterator should respect Before and After positions.
    PositionIterator lastVisible = m_anchorType == PositionIsAfterAnchor ? createLegacyEditingPosition(m_anchorNode.get(), caretMaxOffset(m_anchorNode.get())) : *this;
    PositionIterator currentPos = lastVisible;
    bool startEditable = startNode->hasEditableStyle();
    Node* lastNode = startNode;
    bool boundaryCrossed = false;
    for (; !currentPos.atStart(); currentPos.decrement()) {
        Node* currentNode = currentPos.node();

        // Don't check for an editability change if we haven't moved to a different node,
        // to avoid the expense of computing hasEditableStyle().
        if (currentNode != lastNode) {
            // Don't change editability.
            bool currentEditable = currentNode->hasEditableStyle();
            if (startEditable != currentEditable) {
                if (rule == CannotCrossEditingBoundary)
                    break;
                boundaryCrossed = true;
            }
            lastNode = currentNode;
        }

        // If we've moved to a position that is visually distinct, return the last saved position. There
        // is code below that terminates early if we're *about* to move to a visually distinct position.
        if (endsOfNodeAreVisuallyDistinctPositions(currentNode) && currentNode != boundary)
            return lastVisible;

        // skip position in unrendered or invisible node
        LayoutObject* renderer = currentNode->layoutObject();
        if (!renderer || renderer->style()->visibility() != VISIBLE)
            continue;

        if (rule == CanCrossEditingBoundary && boundaryCrossed) {
            lastVisible = currentPos;
            break;
        }

        // track last visible streamer position
        if (isStreamer(currentPos))
            lastVisible = currentPos;

        // Don't move past a position that is visually distinct.  We could rely on code above to terminate and
        // return lastVisible on the next iteration, but we terminate early to avoid doing a nodeIndex() call.
        if (endsOfNodeAreVisuallyDistinctPositions(currentNode) && currentPos.atStartOfNode())
            return lastVisible;

        // Return position after tables and nodes which have content that can be ignored.
        if (editingIgnoresContent(currentNode) || isRenderedHTMLTableElement(currentNode)) {
            if (currentPos.atEndOfNode())
                return positionAfterNode(currentNode);
            continue;
        }

        // return current position if it is in rendered text
        if (renderer->isText() && toLayoutText(renderer)->firstTextBox()) {
            if (currentNode != startNode) {
                // This assertion fires in layout tests in the case-transform.html test because
                // of a mix-up between offsets in the text in the DOM tree with text in the
                // render tree which can have a different length due to case transformation.
                // Until we resolve that, disable this so we can run the layout tests!
                //ASSERT(currentOffset >= renderer->caretMaxOffset());
                return createLegacyEditingPosition(currentNode, renderer->caretMaxOffset());
            }

            unsigned textOffset = currentPos.offsetInLeafNode();
            LayoutText* textRenderer = toLayoutText(renderer);
            InlineTextBox* lastTextBox = textRenderer->lastTextBox();
            for (InlineTextBox* box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
                if (textOffset <= box->start() + box->len()) {
                    if (textOffset > box->start())
                        return currentPos;
                    continue;
                }

                if (box == lastTextBox || textOffset != box->start() + box->len() + 1)
                    continue;

                // The text continues on the next line only if the last text box is not on this line and
                // none of the boxes on this line have a larger start offset.

                bool continuesOnNextLine = true;
                InlineBox* otherBox = box;
                while (continuesOnNextLine) {
                    otherBox = otherBox->nextLeafChild();
                    if (!otherBox)
                        break;
                    if (otherBox == lastTextBox || (otherBox->layoutObject() == textRenderer && toInlineTextBox(otherBox)->start() > textOffset))
                        continuesOnNextLine = false;
                }

                otherBox = box;
                while (continuesOnNextLine) {
                    otherBox = otherBox->prevLeafChild();
                    if (!otherBox)
                        break;
                    if (otherBox == lastTextBox || (otherBox->layoutObject() == textRenderer && toInlineTextBox(otherBox)->start() > textOffset))
                        continuesOnNextLine = false;
                }

                if (continuesOnNextLine)
                    return currentPos;
            }
        }
    }

    return lastVisible;
}

// This function and upstream() are used for moving back and forth between visually equivalent candidates.
// For example, for the text node "foo     bar" where whitespace is collapsible, there are two candidates
// that map to the VisiblePosition between 'b' and the space.  This function will return the right candidate
// and upstream() will return the left one.
// Also, downstream() will return the last position in the last atomic node in boundary for all of the positions
// in boundary after the last candidate, where endsOfNodeAreVisuallyDistinctPositions(boundary).
// FIXME: This function should never be called when the line box tree is dirty. See https://bugs.webkit.org/show_bug.cgi?id=97264
Position Position::downstream(EditingBoundaryCrossingRule rule) const
{
    Node* startNode = deprecatedNode();
    if (!startNode)
        return Position();

    // iterate forward from there, looking for a qualified position
    Node* boundary = enclosingVisualBoundary(startNode);
    // FIXME: PositionIterator should respect Before and After positions.
    PositionIterator lastVisible = m_anchorType == PositionIsAfterAnchor ? createLegacyEditingPosition(m_anchorNode.get(), caretMaxOffset(m_anchorNode.get())) : *this;
    PositionIterator currentPos = lastVisible;
    bool startEditable = startNode->hasEditableStyle();
    Node* lastNode = startNode;
    bool boundaryCrossed = false;
    for (; !currentPos.atEnd(); currentPos.increment()) {
        Node* currentNode = currentPos.node();

        // Don't check for an editability change if we haven't moved to a different node,
        // to avoid the expense of computing hasEditableStyle().
        if (currentNode != lastNode) {
            // Don't change editability.
            bool currentEditable = currentNode->hasEditableStyle();
            if (startEditable != currentEditable) {
                if (rule == CannotCrossEditingBoundary)
                    break;
                boundaryCrossed = true;
            }

            lastNode = currentNode;
        }

        // stop before going above the body, up into the head
        // return the last visible streamer position
        if (isHTMLBodyElement(*currentNode) && currentPos.atEndOfNode())
            break;

        // Do not move to a visually distinct position.
        if (endsOfNodeAreVisuallyDistinctPositions(currentNode) && currentNode != boundary)
            return lastVisible;
        // Do not move past a visually disinct position.
        // Note: The first position after the last in a node whose ends are visually distinct
        // positions will be [boundary->parentNode(), originalBlock->nodeIndex() + 1].
        if (boundary && boundary->parentNode() == currentNode)
            return lastVisible;

        // skip position in unrendered or invisible node
        LayoutObject* renderer = currentNode->layoutObject();
        if (!renderer || renderer->style()->visibility() != VISIBLE)
            continue;

        if (rule == CanCrossEditingBoundary && boundaryCrossed) {
            lastVisible = currentPos;
            break;
        }

        // track last visible streamer position
        if (isStreamer(currentPos))
            lastVisible = currentPos;

        // Return position before tables and nodes which have content that can be ignored.
        if (editingIgnoresContent(currentNode) || isRenderedHTMLTableElement(currentNode)) {
            if (currentPos.offsetInLeafNode() <= renderer->caretMinOffset())
                return createLegacyEditingPosition(currentNode, renderer->caretMinOffset());
            continue;
        }

        // return current position if it is in rendered text
        if (renderer->isText() && toLayoutText(renderer)->firstTextBox()) {
            if (currentNode != startNode) {
                ASSERT(currentPos.atStartOfNode());
                return createLegacyEditingPosition(currentNode, renderer->caretMinOffset());
            }

            unsigned textOffset = currentPos.offsetInLeafNode();
            LayoutText* textRenderer = toLayoutText(renderer);
            InlineTextBox* lastTextBox = textRenderer->lastTextBox();
            for (InlineTextBox* box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
                if (textOffset <= box->end()) {
                    if (textOffset >= box->start())
                        return currentPos;
                    continue;
                }

                if (box == lastTextBox || textOffset != box->start() + box->len())
                    continue;

                // The text continues on the next line only if the last text box is not on this line and
                // none of the boxes on this line have a larger start offset.

                bool continuesOnNextLine = true;
                InlineBox* otherBox = box;
                while (continuesOnNextLine) {
                    otherBox = otherBox->nextLeafChild();
                    if (!otherBox)
                        break;
                    if (otherBox == lastTextBox || (otherBox->layoutObject() == textRenderer && toInlineTextBox(otherBox)->start() >= textOffset))
                        continuesOnNextLine = false;
                }

                otherBox = box;
                while (continuesOnNextLine) {
                    otherBox = otherBox->prevLeafChild();
                    if (!otherBox)
                        break;
                    if (otherBox == lastTextBox || (otherBox->layoutObject() == textRenderer && toInlineTextBox(otherBox)->start() >= textOffset))
                        continuesOnNextLine = false;
                }

                if (continuesOnNextLine)
                    return currentPos;
            }
        }
    }

    return lastVisible;
}

static int boundingBoxLogicalHeight(LayoutObject *o, const IntRect &rect)
{
    return o->style()->isHorizontalWritingMode() ? rect.height() : rect.width();
}

bool Position::hasRenderedNonAnonymousDescendantsWithHeight(LayoutObject* renderer)
{
    LayoutObject* stop = renderer->nextInPreOrderAfterChildren();
    for (LayoutObject *o = renderer->slowFirstChild(); o && o != stop; o = o->nextInPreOrder())
        if (o->nonPseudoNode()) {
            if ((o->isText() && boundingBoxLogicalHeight(o, toLayoutText(o)->linesBoundingBox()))
                || (o->isBox() && toLayoutBox(o)->pixelSnappedLogicalHeight())
                || (o->isLayoutInline() && isEmptyInline(o) && boundingBoxLogicalHeight(o, toLayoutInline(o)->linesBoundingBox())))
                return true;
        }
    return false;
}

bool Position::nodeIsUserSelectNone(Node* node)
{
    return node && node->layoutObject() && !node->layoutObject()->isSelectable();
}

bool Position::nodeIsUserSelectAll(const Node* node)
{
    return RuntimeEnabledFeatures::userSelectAllEnabled() && node && node->layoutObject() && node->layoutObject()->style()->userSelect() == SELECT_ALL;
}

Node* Position::rootUserSelectAllForNode(Node* node)
{
    if (!node || !nodeIsUserSelectAll(node))
        return 0;
    Node* parent = node->parentNode();
    if (!parent)
        return node;

    Node* candidateRoot = node;
    while (parent) {
        if (!parent->layoutObject()) {
            parent = parent->parentNode();
            continue;
        }
        if (!nodeIsUserSelectAll(parent))
            break;
        candidateRoot = parent;
        parent = candidateRoot->parentNode();
    }
    return candidateRoot;
}

bool Position::isCandidate() const
{
    if (isNull())
        return false;

    LayoutObject* renderer = deprecatedNode()->layoutObject();
    if (!renderer)
        return false;

    if (renderer->style()->visibility() != VISIBLE)
        return false;

    if (renderer->isBR())
        // FIXME: The condition should be m_anchorType == PositionIsBeforeAnchor, but for now we still need to support legacy positions.
        return !m_offset && m_anchorType != PositionIsAfterAnchor && !nodeIsUserSelectNone(deprecatedNode()->parentNode());

    if (renderer->isText())
        return !nodeIsUserSelectNone(deprecatedNode()) && inRenderedText();

    if (renderer->isSVG()) {
        // We don't consider SVG elements are contenteditable except for
        // associated renderer returns isText() true, e.g. LayoutSVGInlineText.
        return false;
    }

    if (isRenderedHTMLTableElement(deprecatedNode()) || editingIgnoresContent(deprecatedNode()))
        return (atFirstEditingPositionForNode() || atLastEditingPositionForNode()) && !nodeIsUserSelectNone(deprecatedNode()->parentNode());

    if (isHTMLHtmlElement(*m_anchorNode))
        return false;

    if (renderer->isLayoutBlockFlow() || renderer->isFlexibleBox() || renderer->isLayoutGrid()) {
        if (toLayoutBlock(renderer)->logicalHeight() || isHTMLBodyElement(*m_anchorNode)) {
            if (!Position::hasRenderedNonAnonymousDescendantsWithHeight(renderer))
                return atFirstEditingPositionForNode() && !Position::nodeIsUserSelectNone(deprecatedNode());
            return m_anchorNode->hasEditableStyle() && !Position::nodeIsUserSelectNone(deprecatedNode()) && atEditingBoundary();
        }
    } else {
        LocalFrame* frame = m_anchorNode->document().frame();
        bool caretBrowsing = frame->settings() && frame->settings()->caretBrowsingEnabled();
        return (caretBrowsing || m_anchorNode->hasEditableStyle()) && !Position::nodeIsUserSelectNone(deprecatedNode()) && atEditingBoundary();
    }

    return false;
}

bool Position::inRenderedText() const
{
    if (isNull() || !deprecatedNode()->isTextNode())
        return false;

    LayoutObject* renderer = deprecatedNode()->layoutObject();
    if (!renderer)
        return false;

    LayoutText* textRenderer = toLayoutText(renderer);
    for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
        if (m_offset < static_cast<int>(box->start()) && !textRenderer->containsReversedText()) {
            // The offset we're looking for is before this node
            // this means the offset must be in content that is
            // not rendered. Return false.
            return false;
        }
        if (box->containsCaretOffset(m_offset))
            // Return false for offsets inside composed characters.
            return m_offset == 0 || m_offset == textRenderer->nextOffset(textRenderer->previousOffset(m_offset));
    }

    return false;
}

bool Position::isRenderedCharacter() const
{
    if (isNull() || !deprecatedNode()->isTextNode())
        return false;

    LayoutObject* renderer = deprecatedNode()->layoutObject();
    if (!renderer)
        return false;

    LayoutText* textRenderer = toLayoutText(renderer);
    for (InlineTextBox* box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
        if (m_offset < static_cast<int>(box->start()) && !textRenderer->containsReversedText()) {
            // The offset we're looking for is before this node
            // this means the offset must be in content that is
            // not rendered. Return false.
            return false;
        }
        if (m_offset >= static_cast<int>(box->start()) && m_offset < static_cast<int>(box->start() + box->len()))
            return true;
    }

    return false;
}

bool Position::rendersInDifferentPosition(const Position &pos) const
{
    if (isNull() || pos.isNull())
        return false;

    LayoutObject* renderer = deprecatedNode()->layoutObject();
    if (!renderer)
        return false;

    LayoutObject* posRenderer = pos.deprecatedNode()->layoutObject();
    if (!posRenderer)
        return false;

    if (renderer->style()->visibility() != VISIBLE ||
        posRenderer->style()->visibility() != VISIBLE)
        return false;

    if (deprecatedNode() == pos.deprecatedNode()) {
        if (isHTMLBRElement(*deprecatedNode()))
            return false;

        if (m_offset == pos.deprecatedEditingOffset())
            return false;

        if (!deprecatedNode()->isTextNode() && !pos.deprecatedNode()->isTextNode()) {
            if (m_offset != pos.deprecatedEditingOffset())
                return true;
        }
    }

    if (isHTMLBRElement(*deprecatedNode()) && pos.isCandidate())
        return true;

    if (isHTMLBRElement(*pos.deprecatedNode()) && isCandidate())
        return true;

    if (!inSameContainingBlockFlowElement(deprecatedNode(), pos.deprecatedNode()))
        return true;

    if (deprecatedNode()->isTextNode() && !inRenderedText())
        return false;

    if (pos.deprecatedNode()->isTextNode() && !pos.inRenderedText())
        return false;

    int thisRenderedOffset = renderedOffset();
    int posRenderedOffset = pos.renderedOffset();

    if (renderer == posRenderer && thisRenderedOffset == posRenderedOffset)
        return false;

    int ignoredCaretOffset;
    InlineBox* b1;
    getInlineBoxAndOffset(DOWNSTREAM, b1, ignoredCaretOffset);
    InlineBox* b2;
    pos.getInlineBoxAndOffset(DOWNSTREAM, b2, ignoredCaretOffset);

    WTF_LOG(Editing, "renderer:               %p [%p]\n", renderer, b1);
    WTF_LOG(Editing, "thisRenderedOffset:         %d\n", thisRenderedOffset);
    WTF_LOG(Editing, "posRenderer:            %p [%p]\n", posRenderer, b2);
    WTF_LOG(Editing, "posRenderedOffset:      %d\n", posRenderedOffset);
    WTF_LOG(Editing, "node min/max:           %d:%d\n", caretMinOffset(deprecatedNode()), caretMaxOffset(deprecatedNode()));
    WTF_LOG(Editing, "pos node min/max:       %d:%d\n", caretMinOffset(pos.deprecatedNode()), caretMaxOffset(pos.deprecatedNode()));
    WTF_LOG(Editing, "----------------------------------------------------------------------\n");

    if (!b1 || !b2) {
        return false;
    }

    if (b1->root() != b2->root()) {
        return true;
    }

    if (nextRenderedEditable(deprecatedNode()) == pos.deprecatedNode()
        && thisRenderedOffset == caretMaxOffset(deprecatedNode()) && !posRenderedOffset) {
        return false;
    }

    if (previousRenderedEditable(deprecatedNode()) == pos.deprecatedNode()
        && !thisRenderedOffset && posRenderedOffset == caretMaxOffset(pos.deprecatedNode())) {
        return false;
    }

    return true;
}

void Position::getInlineBoxAndOffset(EAffinity affinity, InlineBox*& inlineBox, int& caretOffset) const
{
    getInlineBoxAndOffset(affinity, primaryDirection(), inlineBox, caretOffset);
}

static bool isNonTextLeafChild(LayoutObject* object)
{
    if (object->slowFirstChild())
        return false;
    if (object->isText())
        return false;
    return true;
}

static InlineTextBox* searchAheadForBetterMatch(LayoutObject* renderer)
{
    LayoutBlock* container = renderer->containingBlock();
    for (LayoutObject* next = renderer->nextInPreOrder(container); next; next = next->nextInPreOrder(container)) {
        if (next->isLayoutBlock())
            return 0;
        if (next->isBR())
            return 0;
        if (isNonTextLeafChild(next))
            return 0;
        if (next->isText()) {
            InlineTextBox* match = 0;
            int minOffset = INT_MAX;
            for (InlineTextBox* box = toLayoutText(next)->firstTextBox(); box; box = box->nextTextBox()) {
                int caretMinOffset = box->caretMinOffset();
                if (caretMinOffset < minOffset) {
                    match = box;
                    minOffset = caretMinOffset;
                }
            }
            if (match)
                return match;
        }
    }
    return 0;
}

static Position downstreamIgnoringEditingBoundaries(Position position)
{
    Position lastPosition;
    while (position != lastPosition) {
        lastPosition = position;
        position = position.downstream(CanCrossEditingBoundary);
    }
    return position;
}

static Position upstreamIgnoringEditingBoundaries(Position position)
{
    Position lastPosition;
    while (position != lastPosition) {
        lastPosition = position;
        position = position.upstream(CanCrossEditingBoundary);
    }
    return position;
}

void Position::getInlineBoxAndOffset(EAffinity affinity, TextDirection primaryDirection, InlineBox*& inlineBox, int& caretOffset) const
{
    caretOffset = deprecatedEditingOffset();
    LayoutObject* renderer = deprecatedNode()->layoutObject();

    if (!renderer->isText()) {
        inlineBox = 0;
        if (canHaveChildrenForEditing(deprecatedNode()) && renderer->isLayoutBlockFlow() && hasRenderedNonAnonymousDescendantsWithHeight(renderer)) {
            // Try a visually equivalent position with possibly opposite editability. This helps in case |this| is in
            // an editable block but surrounded by non-editable positions. It acts to negate the logic at the beginning
            // of LayoutObject::createVisiblePosition().
            Position equivalent = downstreamIgnoringEditingBoundaries(*this);
            if (equivalent == *this) {
                equivalent = upstreamIgnoringEditingBoundaries(*this);
                if (equivalent == *this || downstreamIgnoringEditingBoundaries(equivalent) == *this)
                    return;
            }

            equivalent.getInlineBoxAndOffset(UPSTREAM, primaryDirection, inlineBox, caretOffset);
            return;
        }
        if (renderer->isBox()) {
            inlineBox = toLayoutBox(renderer)->inlineBoxWrapper();
            if (!inlineBox || (caretOffset > inlineBox->caretMinOffset() && caretOffset < inlineBox->caretMaxOffset()))
                return;
        }
    } else {
        LayoutText* textRenderer = toLayoutText(renderer);

        InlineTextBox* box;
        InlineTextBox* candidate = 0;

        for (box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
            int caretMinOffset = box->caretMinOffset();
            int caretMaxOffset = box->caretMaxOffset();

            if (caretOffset < caretMinOffset || caretOffset > caretMaxOffset || (caretOffset == caretMaxOffset && box->isLineBreak()))
                continue;

            if (caretOffset > caretMinOffset && caretOffset < caretMaxOffset) {
                inlineBox = box;
                return;
            }

            if (((caretOffset == caretMaxOffset) ^ (affinity == DOWNSTREAM))
                || ((caretOffset == caretMinOffset) ^ (affinity == UPSTREAM))
                || (caretOffset == caretMaxOffset && box->nextLeafChild() && box->nextLeafChild()->isLineBreak()))
                break;

            candidate = box;
        }
        if (candidate && candidate == textRenderer->lastTextBox() && affinity == DOWNSTREAM) {
            box = searchAheadForBetterMatch(textRenderer);
            if (box)
                caretOffset = box->caretMinOffset();
        }
        inlineBox = box ? box : candidate;
    }

    if (!inlineBox)
        return;

    unsigned char level = inlineBox->bidiLevel();

    if (inlineBox->direction() == primaryDirection) {
        if (caretOffset == inlineBox->caretRightmostOffset()) {
            InlineBox* nextBox = inlineBox->nextLeafChild();
            if (!nextBox || nextBox->bidiLevel() >= level)
                return;

            level = nextBox->bidiLevel();
            InlineBox* prevBox = inlineBox;
            do {
                prevBox = prevBox->prevLeafChild();
            } while (prevBox && prevBox->bidiLevel() > level);

            if (prevBox && prevBox->bidiLevel() == level)   // For example, abc FED 123 ^ CBA
                return;

            // For example, abc 123 ^ CBA
            while (InlineBox* nextBox = inlineBox->nextLeafChild()) {
                if (nextBox->bidiLevel() < level)
                    break;
                inlineBox = nextBox;
            }
            caretOffset = inlineBox->caretRightmostOffset();
        } else {
            InlineBox* prevBox = inlineBox->prevLeafChild();
            if (!prevBox || prevBox->bidiLevel() >= level)
                return;

            level = prevBox->bidiLevel();
            InlineBox* nextBox = inlineBox;
            do {
                nextBox = nextBox->nextLeafChild();
            } while (nextBox && nextBox->bidiLevel() > level);

            if (nextBox && nextBox->bidiLevel() == level)
                return;

            while (InlineBox* prevBox = inlineBox->prevLeafChild()) {
                if (prevBox->bidiLevel() < level)
                    break;
                inlineBox = prevBox;
            }
            caretOffset = inlineBox->caretLeftmostOffset();
        }
        return;
    }

    if (caretOffset == inlineBox->caretLeftmostOffset()) {
        InlineBox* prevBox = inlineBox->prevLeafChildIgnoringLineBreak();
        if (!prevBox || prevBox->bidiLevel() < level) {
            // Left edge of a secondary run. Set to the right edge of the entire run.
            while (InlineBox* nextBox = inlineBox->nextLeafChildIgnoringLineBreak()) {
                if (nextBox->bidiLevel() < level)
                    break;
                inlineBox = nextBox;
            }
            caretOffset = inlineBox->caretRightmostOffset();
        } else if (prevBox->bidiLevel() > level) {
            // Right edge of a "tertiary" run. Set to the left edge of that run.
            while (InlineBox* tertiaryBox = inlineBox->prevLeafChildIgnoringLineBreak()) {
                if (tertiaryBox->bidiLevel() <= level)
                    break;
                inlineBox = tertiaryBox;
            }
            caretOffset = inlineBox->caretLeftmostOffset();
        }
    } else if (renderer && renderer->style()->unicodeBidi() == Plaintext) {
        if (inlineBox->bidiLevel() < level)
            caretOffset = inlineBox->caretLeftmostOffset();
        else
            caretOffset = inlineBox->caretRightmostOffset();
    } else {
        InlineBox* nextBox = inlineBox->nextLeafChildIgnoringLineBreak();
        if (!nextBox || nextBox->bidiLevel() < level) {
            // Right edge of a secondary run. Set to the left edge of the entire run.
            while (InlineBox* prevBox = inlineBox->prevLeafChildIgnoringLineBreak()) {
                if (prevBox->bidiLevel() < level)
                    break;
                inlineBox = prevBox;
            }
            caretOffset = inlineBox->caretLeftmostOffset();
        } else if (nextBox->bidiLevel() > level) {
            // Left edge of a "tertiary" run. Set to the right edge of that run.
            while (InlineBox* tertiaryBox = inlineBox->nextLeafChildIgnoringLineBreak()) {
                if (tertiaryBox->bidiLevel() <= level)
                    break;
                inlineBox = tertiaryBox;
            }
            caretOffset = inlineBox->caretRightmostOffset();
        }
    }
}

TextDirection Position::primaryDirection() const
{
    TextDirection primaryDirection = LTR;
    for (const LayoutObject* r = m_anchorNode->layoutObject(); r; r = r->parent()) {
        if (r->isLayoutBlockFlow()) {
            primaryDirection = r->style()->direction();
            break;
        }
    }

    return primaryDirection;
}

DEFINE_TRACE(Position)
{
    visitor->trace(m_anchorNode);
}

void Position::debugPosition(const char* msg) const
{
    static const char* const anchorTypes[] = {
        "OffsetInAnchor",
        "BeforeAnchor",
        "AfterAnchor",
        "BeforeChildren",
        "AfterChildren",
        "Invalid",
    };

    if (isNull()) {
        fprintf(stderr, "Position [%s]: null\n", msg);
        return;
    }

    const char* anchorType = anchorTypes[std::min(static_cast<size_t>(m_anchorType), WTF_ARRAY_LENGTH(anchorTypes) - 1)];
    if (m_anchorNode->isTextNode()) {
        fprintf(stderr, "Position [%s]: %s%s [%p] %s, (%s) at %d\n", msg, m_isLegacyEditingPosition ? "LEGACY, " : "", deprecatedNode()->nodeName().utf8().data(), deprecatedNode(), anchorType, m_anchorNode->nodeValue().utf8().data(), m_offset);
        return;
    }

    fprintf(stderr, "Position [%s]: %s%s [%p] %s at %d\n", msg, m_isLegacyEditingPosition ? "LEGACY, " : "", deprecatedNode()->nodeName().utf8().data(), deprecatedNode(), anchorType, m_offset);
}

#ifndef NDEBUG

void Position::formatForDebugger(char* buffer, unsigned length) const
{
    StringBuilder result;

    if (isNull())
        result.appendLiteral("<null>");
    else {
        char s[1024];
        result.appendLiteral("offset ");
        result.appendNumber(m_offset);
        result.appendLiteral(" of ");
        deprecatedNode()->formatForDebugger(s, sizeof(s));
        result.append(s);
    }

    strncpy(buffer, result.toString().utf8().data(), length - 1);
}

void Position::showAnchorTypeAndOffset() const
{
    if (m_isLegacyEditingPosition)
        fputs("legacy, ", stderr);
    switch (anchorType()) {
    case PositionIsOffsetInAnchor:
        fputs("offset", stderr);
        break;
    case PositionIsBeforeChildren:
        fputs("beforeChildren", stderr);
        break;
    case PositionIsAfterChildren:
        fputs("afterChildren", stderr);
        break;
    case PositionIsBeforeAnchor:
        fputs("before", stderr);
        break;
    case PositionIsAfterAnchor:
        fputs("after", stderr);
        break;
    }
    fprintf(stderr, ", offset:%d\n", m_offset);
}

void Position::showTreeForThis() const
{
    if (anchorNode()) {
        anchorNode()->showTreeForThis();
        showAnchorTypeAndOffset();
    }
}

#endif



} // namespace blink

#ifndef NDEBUG

void showTree(const blink::Position& pos)
{
    pos.showTreeForThis();
}

void showTree(const blink::Position* pos)
{
    if (pos)
        pos->showTreeForThis();
}

#endif
