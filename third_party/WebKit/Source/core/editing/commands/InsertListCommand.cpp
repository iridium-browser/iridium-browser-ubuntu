/*
 * Copyright (C) 2006, 2010 Apple Inc. All rights reserved.
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

#include "core/editing/commands/InsertListCommand.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/html/HTMLBRElement.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLLIElement.h"
#include "core/html/HTMLUListElement.h"

namespace blink {

using namespace HTMLNames;

static Node* enclosingListChild(Node* node, Node* listNode) {
  Node* listChild = enclosingListChild(node);
  while (listChild && enclosingList(listChild) != listNode)
    listChild = enclosingListChild(listChild->parentNode());
  return listChild;
}

HTMLUListElement* InsertListCommand::fixOrphanedListChild(
    Node* node,
    EditingState* editingState) {
  HTMLUListElement* listElement = HTMLUListElement::create(document());
  insertNodeBefore(listElement, node, editingState);
  if (editingState->isAborted())
    return nullptr;
  removeNode(node, editingState);
  if (editingState->isAborted())
    return nullptr;
  appendNode(node, listElement, editingState);
  if (editingState->isAborted())
    return nullptr;
  return listElement;
}

HTMLElement* InsertListCommand::mergeWithNeighboringLists(
    HTMLElement* passedList,
    EditingState* editingState) {
  HTMLElement* list = passedList;
  Element* previousList = ElementTraversal::previousSibling(*list);
  document().updateStyleAndLayoutIgnorePendingStylesheets();
  if (canMergeLists(previousList, list)) {
    mergeIdenticalElements(previousList, list, editingState);
    if (editingState->isAborted())
      return nullptr;
  }

  if (!list)
    return nullptr;

  Element* nextSibling = ElementTraversal::nextSibling(*list);
  if (!nextSibling || !nextSibling->isHTMLElement())
    return list;

  HTMLElement* nextList = toHTMLElement(nextSibling);
  document().updateStyleAndLayoutIgnorePendingStylesheets();
  if (canMergeLists(list, nextList)) {
    mergeIdenticalElements(list, nextList, editingState);
    if (editingState->isAborted())
      return nullptr;
    return nextList;
  }
  return list;
}

bool InsertListCommand::selectionHasListOfType(
    const VisibleSelection& selection,
    const HTMLQualifiedName& listTag) {
  DCHECK(!document().needsLayoutTreeUpdate());
  DocumentLifecycle::DisallowTransitionScope disallowTransition(
      document().lifecycle());

  VisiblePosition start = selection.visibleStart();

  if (!enclosingList(start.deepEquivalent().anchorNode()))
    return false;

  VisiblePosition end = startOfParagraph(selection.visibleEnd());
  while (start.isNotNull() && start.deepEquivalent() != end.deepEquivalent()) {
    HTMLElement* listElement =
        enclosingList(start.deepEquivalent().anchorNode());
    if (!listElement || !listElement->hasTagName(listTag))
      return false;
    start = startOfNextParagraph(start);
  }

  return true;
}

InsertListCommand::InsertListCommand(Document& document, Type type)
    : CompositeEditCommand(document), m_type(type) {}

static bool inSameTreeAndOrdered(const Position& shouldBeFormer,
                                 const Position& shouldBeLater) {
  // Input positions must be canonical positions.
  DCHECK_EQ(shouldBeFormer, canonicalPositionOf(shouldBeFormer))
      << shouldBeFormer;
  DCHECK_EQ(shouldBeLater, canonicalPositionOf(shouldBeLater)) << shouldBeLater;
  return Position::commonAncestorTreeScope(shouldBeFormer, shouldBeLater) &&
         comparePositions(shouldBeFormer, shouldBeLater) <= 0;
}

void InsertListCommand::doApply(EditingState* editingState) {
  // Only entry points are Editor::Command::execute and
  // IndentOutdentCommand::outdentParagraph, both of which ensure clean layout.
  DCHECK(!document().needsLayoutTreeUpdate());

  if (!endingSelection().isNonOrphanedCaretOrRange())
    return;

  if (!endingSelection().rootEditableElement())
    return;

  VisiblePosition visibleEnd = endingSelection().visibleEnd();
  VisiblePosition visibleStart = endingSelection().visibleStart();
  // When a selection ends at the start of a paragraph, we rarely paint
  // the selection gap before that paragraph, because there often is no gap.
  // In a case like this, it's not obvious to the user that the selection
  // ends "inside" that paragraph, so it would be confusing if
  // InsertUn{Ordered}List operated on that paragraph.
  // FIXME: We paint the gap before some paragraphs that are indented with left
  // margin/padding, but not others.  We should make the gap painting more
  // consistent and then use a left margin/padding rule here.
  if (visibleEnd.deepEquivalent() != visibleStart.deepEquivalent() &&
      isStartOfParagraph(visibleEnd, CanSkipOverEditingBoundary)) {
    const VisiblePosition& newEnd =
        previousPositionOf(visibleEnd, CannotCrossEditingBoundary);
    SelectionInDOMTree::Builder builder;
    builder.setIsDirectional(endingSelection().isDirectional());
    builder.collapse(visibleStart.toPositionWithAffinity());
    if (newEnd.isNotNull())
      builder.extend(newEnd.deepEquivalent());
    setEndingSelection(builder.build());
    if (!endingSelection().rootEditableElement())
      return;
  }

  const HTMLQualifiedName& listTag = (m_type == OrderedList) ? olTag : ulTag;
  if (endingSelection().isRange()) {
    bool forceListCreation = false;
    VisibleSelection selection =
        selectionForParagraphIteration(endingSelection());
    DCHECK(selection.isRange());

    VisiblePosition visibleStartOfSelection = selection.visibleStart();
    VisiblePosition visibleEndOfSelection = selection.visibleEnd();
    PositionWithAffinity startOfSelection =
        visibleStartOfSelection.toPositionWithAffinity();
    PositionWithAffinity endOfSelection =
        visibleEndOfSelection.toPositionWithAffinity();
    Position startOfLastParagraph =
        startOfParagraph(visibleEndOfSelection, CanSkipOverEditingBoundary)
            .deepEquivalent();

    Range* currentSelection = firstRangeOf(endingSelection());
    ContainerNode* scopeForStartOfSelection = nullptr;
    ContainerNode* scopeForEndOfSelection = nullptr;
    // FIXME: This is an inefficient way to keep selection alive because
    // indexForVisiblePosition walks from the beginning of the document to the
    // visibleEndOfSelection everytime this code is executed. But not using
    // index is hard because there are so many ways we can lose selection inside
    // doApplyForSingleParagraph.
    int indexForStartOfSelection = indexForVisiblePosition(
        visibleStartOfSelection, scopeForStartOfSelection);
    int indexForEndOfSelection =
        indexForVisiblePosition(visibleEndOfSelection, scopeForEndOfSelection);

    if (startOfParagraph(visibleStartOfSelection, CanSkipOverEditingBoundary)
            .deepEquivalent() != startOfLastParagraph) {
      forceListCreation = !selectionHasListOfType(selection, listTag);

      VisiblePosition startOfCurrentParagraph = visibleStartOfSelection;
      while (inSameTreeAndOrdered(startOfCurrentParagraph.deepEquivalent(),
                                  startOfLastParagraph) &&
             !inSameParagraph(startOfCurrentParagraph,
                              createVisiblePosition(startOfLastParagraph),
                              CanCrossEditingBoundary)) {
        // doApply() may operate on and remove the last paragraph of the
        // selection from the document if it's in the same list item as
        // startOfCurrentParagraph. Return early to avoid an infinite loop and
        // because there is no more work to be done.
        // FIXME(<rdar://problem/5983974>): The endingSelection() may be
        // incorrect here.  Compute the new location of visibleEndOfSelection
        // and use it as the end of the new selection.
        if (!startOfLastParagraph.isConnected())
          return;
        setEndingSelection(
            SelectionInDOMTree::Builder()
                .collapse(startOfCurrentParagraph.deepEquivalent())
                .build());

        // Save and restore visibleEndOfSelection and startOfLastParagraph when
        // necessary since moveParagraph and movePragraphWithClones can remove
        // nodes.
        bool singleParagraphResult = doApplyForSingleParagraph(
            forceListCreation, listTag, *currentSelection, editingState);
        if (editingState->isAborted())
          return;
        if (!singleParagraphResult)
          break;

        document().updateStyleAndLayoutIgnorePendingStylesheets();

        // Make |visibleEndOfSelection| valid again.
        if (!endOfSelection.isConnected() ||
            !startOfLastParagraph.isConnected()) {
          visibleEndOfSelection = visiblePositionForIndex(
              indexForEndOfSelection, scopeForEndOfSelection);
          endOfSelection = visibleEndOfSelection.toPositionWithAffinity();
          // If visibleEndOfSelection is null, then some contents have been
          // deleted from the document. This should never happen and if it did,
          // exit early immediately because we've lost the loop invariant.
          DCHECK(visibleEndOfSelection.isNotNull());
          if (visibleEndOfSelection.isNull() ||
              !rootEditableElementOf(visibleEndOfSelection))
            return;
          startOfLastParagraph = startOfParagraph(visibleEndOfSelection,
                                                  CanSkipOverEditingBoundary)
                                     .deepEquivalent();
        } else {
          visibleEndOfSelection = createVisiblePosition(endOfSelection);
        }

        startOfCurrentParagraph =
            startOfNextParagraph(endingSelection().visibleStart());
      }
      setEndingSelection(SelectionInDOMTree::Builder()
                             .collapse(visibleEndOfSelection.deepEquivalent())
                             .build());
    }
    doApplyForSingleParagraph(forceListCreation, listTag, *currentSelection,
                              editingState);
    if (editingState->isAborted())
      return;

    document().updateStyleAndLayoutIgnorePendingStylesheets();

    // Fetch the end of the selection, for the reason mentioned above.
    if (!endOfSelection.isConnected()) {
      visibleEndOfSelection = visiblePositionForIndex(indexForEndOfSelection,
                                                      scopeForEndOfSelection);
      if (visibleEndOfSelection.isNull())
        return;
    } else {
      visibleEndOfSelection = createVisiblePosition(endOfSelection);
    }

    if (!startOfSelection.isConnected()) {
      visibleStartOfSelection = visiblePositionForIndex(
          indexForStartOfSelection, scopeForStartOfSelection);
      if (visibleStartOfSelection.isNull())
        return;
    } else {
      visibleStartOfSelection = createVisiblePosition(startOfSelection);
    }

    setEndingSelection(SelectionInDOMTree::Builder()
                           .setAffinity(visibleStartOfSelection.affinity())
                           .setBaseAndExtentDeprecated(
                               visibleStartOfSelection.deepEquivalent(),
                               visibleEndOfSelection.deepEquivalent())
                           .setIsDirectional(endingSelection().isDirectional())
                           .build());
    return;
  }

  DCHECK(firstRangeOf(endingSelection()));
  doApplyForSingleParagraph(false, listTag, *firstRangeOf(endingSelection()),
                            editingState);
}

InputEvent::InputType InsertListCommand::inputType() const {
  return m_type == OrderedList ? InputEvent::InputType::InsertOrderedList
                               : InputEvent::InputType::InsertUnorderedList;
}

bool InsertListCommand::doApplyForSingleParagraph(
    bool forceCreateList,
    const HTMLQualifiedName& listTag,
    Range& currentSelection,
    EditingState* editingState) {
  // FIXME: This will produce unexpected results for a selection that starts
  // just before a table and ends inside the first cell,
  // selectionForParagraphIteration should probably be renamed and deployed
  // inside setEndingSelection().
  Node* selectionNode = endingSelection().start().anchorNode();
  Node* listChildNode = enclosingListChild(selectionNode);
  bool switchListType = false;
  if (listChildNode) {
    if (!hasEditableStyle(*listChildNode->parentNode()))
      return false;
    // Remove the list child.
    HTMLElement* listElement = enclosingList(listChildNode);
    if (listElement) {
      if (!hasEditableStyle(*listElement)) {
        // Since, |listElement| is uneditable, we can't move |listChild|
        // out from |listElement|.
        return false;
      }
      if (!hasEditableStyle(*listElement->parentNode())) {
        // Since parent of |listElement| is uneditable, we can not remove
        // |listElement| for switching list type neither unlistify.
        return false;
      }
    }
    if (!listElement) {
      listElement = fixOrphanedListChild(listChildNode, editingState);
      if (editingState->isAborted())
        return false;
      listElement = mergeWithNeighboringLists(listElement, editingState);
      if (editingState->isAborted())
        return false;
      document().updateStyleAndLayoutIgnorePendingStylesheets();
    }
    DCHECK(hasEditableStyle(*listElement));
    DCHECK(hasEditableStyle(*listElement->parentNode()));
    if (!listElement->hasTagName(listTag)) {
      // |listChildNode| will be removed from the list and a list of type
      // |m_type| will be created.
      switchListType = true;
    }

    // If the list is of the desired type, and we are not removing the list,
    // then exit early.
    if (!switchListType && forceCreateList)
      return true;

    // If the entire list is selected, then convert the whole list.
    if (switchListType &&
        isNodeVisiblyContainedWithin(*listElement, currentSelection)) {
      bool rangeStartIsInList =
          visiblePositionBeforeNode(*listElement).deepEquivalent() ==
          createVisiblePosition(currentSelection.startPosition())
              .deepEquivalent();
      bool rangeEndIsInList =
          visiblePositionAfterNode(*listElement).deepEquivalent() ==
          createVisiblePosition(currentSelection.endPosition())
              .deepEquivalent();

      HTMLElement* newList = createHTMLElement(document(), listTag);
      insertNodeBefore(newList, listElement, editingState);
      if (editingState->isAborted())
        return false;

      document().updateStyleAndLayoutIgnorePendingStylesheets();
      Node* firstChildInList =
          enclosingListChild(VisiblePosition::firstPositionInNode(listElement)
                                 .deepEquivalent()
                                 .anchorNode(),
                             listElement);
      Element* outerBlock =
          firstChildInList && isBlockFlowElement(*firstChildInList)
              ? toElement(firstChildInList)
              : listElement;

      moveParagraphWithClones(VisiblePosition::firstPositionInNode(listElement),
                              VisiblePosition::lastPositionInNode(listElement),
                              newList, outerBlock, editingState);
      if (editingState->isAborted())
        return false;

      // Manually remove listNode because moveParagraphWithClones sometimes
      // leaves it behind in the document. See the bug 33668 and
      // editing/execCommand/insert-list-orphaned-item-with-nested-lists.html.
      // FIXME: This might be a bug in moveParagraphWithClones or
      // deleteSelection.
      if (listElement && listElement->isConnected()) {
        removeNode(listElement, editingState);
        if (editingState->isAborted())
          return false;
      }

      newList = mergeWithNeighboringLists(newList, editingState);
      if (editingState->isAborted())
        return false;

      // Restore the start and the end of current selection if they started
      // inside listNode because moveParagraphWithClones could have removed
      // them.
      if (rangeStartIsInList && newList)
        currentSelection.setStart(newList, 0, IGNORE_EXCEPTION_FOR_TESTING);
      if (rangeEndIsInList && newList) {
        currentSelection.setEnd(newList, Position::lastOffsetInNode(newList),
                                IGNORE_EXCEPTION_FOR_TESTING);
      }

      setEndingSelection(SelectionInDOMTree::Builder()
                             .collapse(Position::firstPositionInNode(newList))
                             .build());

      return true;
    }

    unlistifyParagraph(endingSelection().visibleStart(), listElement,
                       listChildNode, editingState);
    if (editingState->isAborted())
      return false;
    document().updateStyleAndLayoutIgnorePendingStylesheets();
  }

  if (!listChildNode || switchListType || forceCreateList)
    listifyParagraph(endingSelection().visibleStart(), listTag, editingState);

  return true;
}

void InsertListCommand::unlistifyParagraph(const VisiblePosition& originalStart,
                                           HTMLElement* listElement,
                                           Node* listChildNode,
                                           EditingState* editingState) {
  // Since, unlistify paragraph inserts nodes into parent and removes node
  // from parent, if parent of |listElement| should be editable.
  DCHECK(hasEditableStyle(*listElement->parentNode()));
  Node* nextListChild;
  Node* previousListChild;
  VisiblePosition start;
  VisiblePosition end;
  DCHECK(listChildNode);
  if (isHTMLLIElement(*listChildNode)) {
    start = VisiblePosition::firstPositionInNode(listChildNode);
    end = VisiblePosition::lastPositionInNode(listChildNode);
    nextListChild = listChildNode->nextSibling();
    previousListChild = listChildNode->previousSibling();
  } else {
    // A paragraph is visually a list item minus a list marker.  The paragraph
    // will be moved.
    start = startOfParagraph(originalStart, CanSkipOverEditingBoundary);
    end = endOfParagraph(start, CanSkipOverEditingBoundary);
    nextListChild = enclosingListChild(
        nextPositionOf(end).deepEquivalent().anchorNode(), listElement);
    DCHECK_NE(nextListChild, listChildNode);
    previousListChild = enclosingListChild(
        previousPositionOf(start).deepEquivalent().anchorNode(), listElement);
    DCHECK_NE(previousListChild, listChildNode);
  }

  // Helpers for making |start| and |end| valid again after DOM changes.
  PositionWithAffinity startPosition = start.toPositionWithAffinity();
  PositionWithAffinity endPosition = end.toPositionWithAffinity();

  // When removing a list, we must always create a placeholder to act as a point
  // of insertion for the list content being removed.
  HTMLBRElement* placeholder = HTMLBRElement::create(document());
  HTMLElement* elementToInsert = placeholder;
  // If the content of the list item will be moved into another list, put it in
  // a list item so that we don't create an orphaned list child.
  if (enclosingList(listElement)) {
    elementToInsert = HTMLLIElement::create(document());
    appendNode(placeholder, elementToInsert, editingState);
    if (editingState->isAborted())
      return;
  }

  if (nextListChild && previousListChild) {
    // We want to pull listChildNode out of listNode, and place it before
    // nextListChild and after previousListChild, so we split listNode and
    // insert it between the two lists.
    // But to split listNode, we must first split ancestors of listChildNode
    // between it and listNode, if any exist.
    // FIXME: We appear to split at nextListChild as opposed to listChildNode so
    // that when we remove listChildNode below in moveParagraphs,
    // previousListChild will be removed along with it if it is unrendered. But
    // we ought to remove nextListChild too, if it is unrendered.
    splitElement(listElement, splitTreeToNode(nextListChild, listElement));
    insertNodeBefore(elementToInsert, listElement, editingState);
  } else if (nextListChild || listChildNode->parentNode() != listElement) {
    // Just because listChildNode has no previousListChild doesn't mean there
    // isn't any content in listNode that comes before listChildNode, as
    // listChildNode could have ancestors between it and listNode. So, we split
    // up to listNode before inserting the placeholder where we're about to move
    // listChildNode to.
    if (listChildNode->parentNode() != listElement)
      splitElement(listElement, splitTreeToNode(listChildNode, listElement));
    insertNodeBefore(elementToInsert, listElement, editingState);
  } else {
    insertNodeAfter(elementToInsert, listElement, editingState);
  }
  if (editingState->isAborted())
    return;

  document().updateStyleAndLayoutIgnorePendingStylesheets();

  // Make |start| and |end| valid again.
  start = createVisiblePosition(startPosition);
  end = createVisiblePosition(endPosition);

  VisiblePosition insertionPoint = VisiblePosition::beforeNode(placeholder);
  moveParagraphs(start, end, insertionPoint, editingState, PreserveSelection,
                 PreserveStyle, listChildNode);
}

static HTMLElement* adjacentEnclosingList(const VisiblePosition& pos,
                                          const VisiblePosition& adjacentPos,
                                          const HTMLQualifiedName& listTag) {
  HTMLElement* listElement =
      outermostEnclosingList(adjacentPos.deepEquivalent().anchorNode());

  if (!listElement)
    return 0;

  Element* previousCell = enclosingTableCell(pos.deepEquivalent());
  Element* currentCell = enclosingTableCell(adjacentPos.deepEquivalent());

  if (!listElement->hasTagName(listTag) ||
      listElement->contains(pos.deepEquivalent().anchorNode()) ||
      previousCell != currentCell ||
      enclosingList(listElement) !=
          enclosingList(pos.deepEquivalent().anchorNode()))
    return 0;

  return listElement;
}

void InsertListCommand::listifyParagraph(const VisiblePosition& originalStart,
                                         const HTMLQualifiedName& listTag,
                                         EditingState* editingState) {
  const VisiblePosition& start =
      startOfParagraph(originalStart, CanSkipOverEditingBoundary);
  const VisiblePosition& end =
      endOfParagraph(start, CanSkipOverEditingBoundary);

  if (start.isNull() || end.isNull())
    return;

  // Check for adjoining lists.
  HTMLElement* const previousList = adjacentEnclosingList(
      start, previousPositionOf(start, CannotCrossEditingBoundary), listTag);
  HTMLElement* const nextList = adjacentEnclosingList(
      start, nextPositionOf(end, CannotCrossEditingBoundary), listTag);
  if (previousList || nextList) {
    // Place list item into adjoining lists.
    HTMLLIElement* listItemElement = HTMLLIElement::create(document());
    if (previousList)
      appendNode(listItemElement, previousList, editingState);
    else
      insertNodeAt(listItemElement, Position::beforeNode(nextList),
                   editingState);
    if (editingState->isAborted())
      return;

    moveParagraphOverPositionIntoEmptyListItem(start, listItemElement,
                                               editingState);
    if (editingState->isAborted())
      return;

    document().updateStyleAndLayoutIgnorePendingStylesheets();
    if (canMergeLists(previousList, nextList))
      mergeIdenticalElements(previousList, nextList, editingState);

    return;
  }

  // Create new list element.

  // Inserting the list into an empty paragraph that isn't held open
  // by a br or a '\n', will invalidate start and end.  Insert
  // a placeholder and then recompute start and end.
  Position startPos = start.deepEquivalent();
  if (start.deepEquivalent() == end.deepEquivalent() &&
      isEnclosingBlock(start.deepEquivalent().anchorNode())) {
    HTMLBRElement* placeholder = insertBlockPlaceholder(startPos, editingState);
    if (editingState->isAborted())
      return;
    startPos = Position::beforeNode(placeholder);
  }

  document().updateStyleAndLayoutIgnorePendingStylesheets();

  // Insert the list at a position visually equivalent to start of the
  // paragraph that is being moved into the list.
  // Try to avoid inserting it somewhere where it will be surrounded by
  // inline ancestors of start, since it is easier for editing to produce
  // clean markup when inline elements are pushed down as far as possible.
  Position insertionPos(mostBackwardCaretPosition(startPos));
  // Also avoid the containing list item.
  Node* const listChild = enclosingListChild(insertionPos.anchorNode());
  if (isHTMLLIElement(listChild))
    insertionPos = Position::inParentBeforeNode(*listChild);

  HTMLElement* listElement = createHTMLElement(document(), listTag);
  insertNodeAt(listElement, insertionPos, editingState);
  if (editingState->isAborted())
    return;
  HTMLLIElement* listItemElement = HTMLLIElement::create(document());
  appendNode(listItemElement, listElement, editingState);
  if (editingState->isAborted())
    return;

  // We inserted the list at the start of the content we're about to move.
  // https://bugs.webkit.org/show_bug.cgi?id=19066: Update the start of content,
  // so we don't try to move the list into itself.
  // Layout is necessary since start's node's inline layoutObjects may have been
  // destroyed by the insertion The end of the content may have changed after
  // the insertion and layout so update it as well.
  if (insertionPos == startPos) {
    moveParagraphOverPositionIntoEmptyListItem(originalStart, listItemElement,
                                               editingState);
  } else {
    document().updateStyleAndLayoutIgnorePendingStylesheets();
    moveParagraphOverPositionIntoEmptyListItem(createVisiblePosition(startPos),
                                               listItemElement, editingState);
  }
  if (editingState->isAborted())
    return;

  mergeWithNeighboringLists(listElement, editingState);
}

// TODO(xiaochengh): Stop storing VisiblePositions through mutations.
void InsertListCommand::moveParagraphOverPositionIntoEmptyListItem(
    const VisiblePosition& pos,
    HTMLLIElement* listItemElement,
    EditingState* editingState) {
  DCHECK(!listItemElement->hasChildren());
  HTMLBRElement* placeholder = HTMLBRElement::create(document());
  appendNode(placeholder, listItemElement, editingState);
  if (editingState->isAborted())
    return;
  // Inserting list element and list item list may change start of pargraph
  // to move. We calculate start of paragraph again.
  document().updateStyleAndLayoutIgnorePendingStylesheets();
  const VisiblePosition& validPos =
      createVisiblePosition(pos.toPositionWithAffinity());
  const VisiblePosition& start =
      startOfParagraph(validPos, CanSkipOverEditingBoundary);
  const VisiblePosition& end =
      endOfParagraph(validPos, CanSkipOverEditingBoundary);
  moveParagraph(start, end, VisiblePosition::beforeNode(placeholder),
                editingState, PreserveSelection);
}

DEFINE_TRACE(InsertListCommand) {
  CompositeEditCommand::trace(visitor);
}

}  // namespace blink
