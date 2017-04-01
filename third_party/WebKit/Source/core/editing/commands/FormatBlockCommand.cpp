/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "core/editing/commands/FormatBlockCommand.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/dom/Element.h"
#include "core/dom/Range.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/VisibleUnits.h"
#include "core/html/HTMLBRElement.h"
#include "core/html/HTMLElement.h"

namespace blink {

using namespace HTMLNames;

static Node* enclosingBlockToSplitTreeTo(Node* startNode);
static bool isElementForFormatBlock(const QualifiedName& tagName);
static inline bool isElementForFormatBlock(Node* node) {
  return node->isElementNode() &&
         isElementForFormatBlock(toElement(node)->tagQName());
}

static Element* enclosingBlockFlowElement(
    const VisiblePosition& visiblePosition) {
  if (visiblePosition.isNull())
    return nullptr;
  return enclosingBlockFlowElement(
      *visiblePosition.deepEquivalent().anchorNode());
}

FormatBlockCommand::FormatBlockCommand(Document& document,
                                       const QualifiedName& tagName)
    : ApplyBlockElementCommand(document, tagName), m_didApply(false) {}

void FormatBlockCommand::formatSelection(
    const VisiblePosition& startOfSelection,
    const VisiblePosition& endOfSelection,
    EditingState* editingState) {
  if (!isElementForFormatBlock(tagName()))
    return;
  ApplyBlockElementCommand::formatSelection(startOfSelection, endOfSelection,
                                            editingState);
  m_didApply = true;
}

void FormatBlockCommand::formatRange(const Position& start,
                                     const Position& end,
                                     const Position& endOfSelection,
                                     HTMLElement*& blockElement,
                                     EditingState* editingState) {
  Element* refElement = enclosingBlockFlowElement(createVisiblePosition(end));
  Element* root = rootEditableElementOf(start);
  // Root is null for elements with contenteditable=false.
  if (!root || !refElement)
    return;

  Node* nodeToSplitTo = enclosingBlockToSplitTreeTo(start.anchorNode());
  Node* outerBlock = (start.anchorNode() == nodeToSplitTo)
                         ? start.anchorNode()
                         : splitTreeToNode(start.anchorNode(), nodeToSplitTo);
  Node* nodeAfterInsertionPosition = outerBlock;
  Range* range = Range::create(document(), start, endOfSelection);

  document().updateStyleAndLayoutIgnorePendingStylesheets();
  if (isElementForFormatBlock(refElement->tagQName()) &&
      createVisiblePosition(start).deepEquivalent() ==
          startOfBlock(createVisiblePosition(start)).deepEquivalent() &&
      (createVisiblePosition(end).deepEquivalent() ==
           endOfBlock(createVisiblePosition(end)).deepEquivalent() ||
       isNodeVisiblyContainedWithin(*refElement, *range)) &&
      refElement != root && !root->isDescendantOf(refElement)) {
    // Already in a block element that only contains the current paragraph
    if (refElement->hasTagName(tagName()))
      return;
    nodeAfterInsertionPosition = refElement;
  }

  if (!blockElement) {
    // Create a new blockquote and insert it as a child of the root editable
    // element. We accomplish this by splitting all parents of the current
    // paragraph up to that point.
    blockElement = createBlockElement();
    insertNodeBefore(blockElement, nodeAfterInsertionPosition, editingState);
    if (editingState->isAborted())
      return;
    document().updateStyleAndLayoutIgnorePendingStylesheets();
  }

  Position lastParagraphInBlockNode =
      blockElement->lastChild() ? Position::afterNode(blockElement->lastChild())
                                : Position();
  bool wasEndOfParagraph =
      isEndOfParagraph(createVisiblePosition(lastParagraphInBlockNode));

  moveParagraphWithClones(createVisiblePosition(start),
                          createVisiblePosition(end), blockElement, outerBlock,
                          editingState);
  if (editingState->isAborted())
    return;

  // Copy the inline style of the original block element to the newly created
  // block-style element.
  if (outerBlock != nodeAfterInsertionPosition &&
      toHTMLElement(nodeAfterInsertionPosition)->hasAttribute(styleAttr))
    blockElement->setAttribute(
        styleAttr,
        toHTMLElement(nodeAfterInsertionPosition)->getAttribute(styleAttr));

  document().updateStyleAndLayoutIgnorePendingStylesheets();

  if (wasEndOfParagraph &&
      !isEndOfParagraph(createVisiblePosition(lastParagraphInBlockNode)) &&
      !isStartOfParagraph(createVisiblePosition(lastParagraphInBlockNode)))
    insertBlockPlaceholder(lastParagraphInBlockNode, editingState);
}

Element* FormatBlockCommand::elementForFormatBlockCommand(Range* range) {
  if (!range)
    return 0;

  Node* commonAncestor = range->commonAncestorContainer();
  while (commonAncestor && !isElementForFormatBlock(commonAncestor))
    commonAncestor = commonAncestor->parentNode();

  if (!commonAncestor)
    return 0;

  Element* element = rootEditableElement(*range->startContainer());
  if (!element || commonAncestor->contains(element))
    return 0;

  return commonAncestor->isElementNode() ? toElement(commonAncestor) : 0;
}

bool isElementForFormatBlock(const QualifiedName& tagName) {
  DEFINE_STATIC_LOCAL(
      HashSet<QualifiedName>, blockTags,
      ({
          addressTag, articleTag, asideTag,  blockquoteTag, ddTag,     divTag,
          dlTag,      dtTag,      footerTag, h1Tag,         h2Tag,     h3Tag,
          h4Tag,      h5Tag,      h6Tag,     headerTag,     hgroupTag, mainTag,
          navTag,     pTag,       preTag,    sectionTag,
      }));
  return blockTags.contains(tagName);
}

Node* enclosingBlockToSplitTreeTo(Node* startNode) {
  DCHECK(startNode);
  Node* lastBlock = startNode;
  for (Node& runner : NodeTraversal::inclusiveAncestorsOf(*startNode)) {
    if (!hasEditableStyle(runner))
      return lastBlock;
    if (isTableCell(&runner) || isHTMLBodyElement(&runner) ||
        !runner.parentNode() || !hasEditableStyle(*runner.parentNode()) ||
        isElementForFormatBlock(&runner))
      return &runner;
    if (isEnclosingBlock(&runner))
      lastBlock = &runner;
    if (isHTMLListElement(&runner))
      return hasEditableStyle(*runner.parentNode()) ? runner.parentNode()
                                                    : &runner;
  }
  return lastBlock;
}

}  // namespace blink
