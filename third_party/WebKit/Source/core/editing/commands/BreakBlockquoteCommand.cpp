/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#include "core/editing/commands/BreakBlockquoteCommand.h"

#include "core/HTMLNames.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/Text.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/VisiblePosition.h"
#include "core/html/HTMLBRElement.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLQuoteElement.h"
#include "core/layout/LayoutListItem.h"

namespace blink {

using namespace HTMLNames;

namespace {

bool isFirstVisiblePositionInNode(const VisiblePosition& visiblePosition,
                                  const ContainerNode* node) {
  if (visiblePosition.isNull())
    return false;

  if (!visiblePosition.deepEquivalent().computeContainerNode()->isDescendantOf(
          node))
    return false;

  VisiblePosition previous = previousPositionOf(visiblePosition);
  return previous.isNull() ||
         !previous.deepEquivalent().anchorNode()->isDescendantOf(node);
}

bool isLastVisiblePositionInNode(const VisiblePosition& visiblePosition,
                                 const ContainerNode* node) {
  if (visiblePosition.isNull())
    return false;

  if (!visiblePosition.deepEquivalent().computeContainerNode()->isDescendantOf(
          node))
    return false;

  VisiblePosition next = nextPositionOf(visiblePosition);
  return next.isNull() ||
         !next.deepEquivalent().anchorNode()->isDescendantOf(node);
}

}  // namespace

BreakBlockquoteCommand::BreakBlockquoteCommand(Document& document)
    : CompositeEditCommand(document) {}

void BreakBlockquoteCommand::doApply(EditingState* editingState) {
  if (endingSelection().isNone())
    return;

  // Delete the current selection.
  if (endingSelection().isRange()) {
    deleteSelection(editingState, false, false);
    if (editingState->isAborted())
      return;
  }

  // This is a scenario that should never happen, but we want to
  // make sure we don't dereference a null pointer below.

  DCHECK(!endingSelection().isNone());

  if (endingSelection().isNone())
    return;

  document().updateStyleAndLayoutIgnorePendingStylesheets();

  VisiblePosition visiblePos = endingSelection().visibleStart();

  // pos is a position equivalent to the caret.  We use downstream() so that pos
  // will be in the first node that we need to move (there are a few exceptions
  // to this, see below).
  Position pos = mostForwardCaretPosition(endingSelection().start());

  // Find the top-most blockquote from the start.
  HTMLQuoteElement* topBlockquote = toHTMLQuoteElement(
      highestEnclosingNodeOfType(pos, isMailHTMLBlockquoteElement));
  if (!topBlockquote || !topBlockquote->parentNode())
    return;

  HTMLBRElement* breakElement = HTMLBRElement::create(document());

  bool isLastVisPosInNode =
      isLastVisiblePositionInNode(visiblePos, topBlockquote);

  // If the position is at the beginning of the top quoted content, we don't
  // need to break the quote. Instead, insert the break before the blockquote,
  // unless the position is as the end of the the quoted content.
  if (isFirstVisiblePositionInNode(visiblePos, topBlockquote) &&
      !isLastVisPosInNode) {
    insertNodeBefore(breakElement, topBlockquote, editingState);
    if (editingState->isAborted())
      return;
    setEndingSelection(SelectionInDOMTree::Builder()
                           .collapse(Position::beforeNode(breakElement))
                           .setIsDirectional(endingSelection().isDirectional())
                           .build());
    rebalanceWhitespace();
    return;
  }

  // Insert a break after the top blockquote.
  insertNodeAfter(breakElement, topBlockquote, editingState);
  if (editingState->isAborted())
    return;

  document().updateStyleAndLayoutIgnorePendingStylesheets();

  // If we're inserting the break at the end of the quoted content, we don't
  // need to break the quote.
  if (isLastVisPosInNode) {
    setEndingSelection(SelectionInDOMTree::Builder()
                           .collapse(Position::beforeNode(breakElement))
                           .setIsDirectional(endingSelection().isDirectional())
                           .build());
    rebalanceWhitespace();
    return;
  }

  // Don't move a line break just after the caret.  Doing so would create an
  // extra, empty paragraph in the new blockquote.
  if (lineBreakExistsAtVisiblePosition(visiblePos)) {
    pos = nextPositionOf(pos, PositionMoveType::GraphemeCluster);
  }

  // Adjust the position so we don't split at the beginning of a quote.
  while (isFirstVisiblePositionInNode(createVisiblePosition(pos),
                                      toHTMLQuoteElement(enclosingNodeOfType(
                                          pos, isMailHTMLBlockquoteElement)))) {
    pos = previousPositionOf(pos, PositionMoveType::GraphemeCluster);
  }

  // startNode is the first node that we need to move to the new blockquote.
  Node* startNode = pos.anchorNode();
  DCHECK(startNode);

  // Split at pos if in the middle of a text node.
  if (startNode->isTextNode()) {
    Text* textNode = toText(startNode);
    int textOffset = pos.computeOffsetInContainerNode();
    if ((unsigned)textOffset >= textNode->length()) {
      startNode = NodeTraversal::next(*startNode);
      DCHECK(startNode);
    } else if (textOffset > 0) {
      splitTextNode(textNode, textOffset);
    }
  } else if (pos.computeEditingOffset() > 0) {
    Node* childAtOffset =
        NodeTraversal::childAt(*startNode, pos.computeEditingOffset());
    startNode = childAtOffset ? childAtOffset : NodeTraversal::next(*startNode);
    DCHECK(startNode);
  }

  // If there's nothing inside topBlockquote to move, we're finished.
  if (!startNode->isDescendantOf(topBlockquote)) {
    setEndingSelection(SelectionInDOMTree::Builder()
                           .collapse(firstPositionInOrBeforeNode(startNode))
                           .setIsDirectional(endingSelection().isDirectional())
                           .build());
    return;
  }

  // Build up list of ancestors in between the start node and the top
  // blockquote.
  HeapVector<Member<Element>> ancestors;
  for (Element* node = startNode->parentElement();
       node && node != topBlockquote; node = node->parentElement())
    ancestors.push_back(node);

  // Insert a clone of the top blockquote after the break.
  Element* clonedBlockquote = topBlockquote->cloneElementWithoutChildren();
  insertNodeAfter(clonedBlockquote, breakElement, editingState);
  if (editingState->isAborted())
    return;

  // Clone startNode's ancestors into the cloned blockquote.
  // On exiting this loop, clonedAncestor is the lowest ancestor
  // that was cloned (i.e. the clone of either ancestors.last()
  // or clonedBlockquote if ancestors is empty).
  Element* clonedAncestor = clonedBlockquote;
  for (size_t i = ancestors.size(); i != 0; --i) {
    Element* clonedChild = ancestors[i - 1]->cloneElementWithoutChildren();
    // Preserve list item numbering in cloned lists.
    if (isHTMLOListElement(*clonedChild)) {
      Node* listChildNode = i > 1 ? ancestors[i - 2].get() : startNode;
      // The first child of the cloned list might not be a list item element,
      // find the first one so that we know where to start numbering.
      while (listChildNode && !isHTMLLIElement(*listChildNode))
        listChildNode = listChildNode->nextSibling();
      if (isListItem(listChildNode))
        setNodeAttribute(
            clonedChild, startAttr,
            AtomicString::number(
                toLayoutListItem(listChildNode->layoutObject())->value()));
    }

    appendNode(clonedChild, clonedAncestor, editingState);
    if (editingState->isAborted())
      return;
    clonedAncestor = clonedChild;
  }

  moveRemainingSiblingsToNewParent(startNode, 0, clonedAncestor, editingState);
  if (editingState->isAborted())
    return;

  if (!ancestors.isEmpty()) {
    // Split the tree up the ancestor chain until the topBlockquote
    // Throughout this loop, clonedParent is the clone of ancestor's parent.
    // This is so we can clone ancestor's siblings and place the clones
    // into the clone corresponding to the ancestor's parent.
    Element* ancestor = nullptr;
    Element* clonedParent = nullptr;
    for (ancestor = ancestors.front(),
        clonedParent = clonedAncestor->parentElement();
         ancestor && ancestor != topBlockquote;
         ancestor = ancestor->parentElement(),
        clonedParent = clonedParent->parentElement()) {
      moveRemainingSiblingsToNewParent(ancestor->nextSibling(), 0, clonedParent,
                                       editingState);
      if (editingState->isAborted())
        return;
    }

    // If the startNode's original parent is now empty, remove it
    Element* originalParent = ancestors.front().get();
    if (!originalParent->hasChildren()) {
      removeNode(originalParent, editingState);
      if (editingState->isAborted())
        return;
    }
  }

  // Make sure the cloned block quote renders.
  addBlockPlaceholderIfNeeded(clonedBlockquote, editingState);
  if (editingState->isAborted())
    return;

  // Put the selection right before the break.
  setEndingSelection(SelectionInDOMTree::Builder()
                         .collapse(Position::beforeNode(breakElement))
                         .setIsDirectional(endingSelection().isDirectional())
                         .build());
  rebalanceWhitespace();
}

}  // namespace blink
