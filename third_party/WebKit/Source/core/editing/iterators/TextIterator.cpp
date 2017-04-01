/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov.
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

#include "core/editing/iterators/TextIterator.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/dom/Document.h"
#include "core/dom/FirstLetterPseudoElement.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/Position.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/iterators/CharacterIterator.h"
#include "core/editing/iterators/WordAwareIterator.h"
#include "core/frame/FrameView.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/TextControlElement.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTableRow.h"
#include "core/layout/LayoutTextFragment.h"
#include "core/layout/line/InlineTextBox.h"
#include "platform/fonts/Font.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuilder.h"
#include <algorithm>
#include <unicode/utf16.h>

using namespace WTF::Unicode;

namespace blink {

using namespace HTMLNames;

namespace {

const int kInvalidOffset = -1;

template <typename Strategy>
TextIteratorBehaviorFlags adjustBehaviorFlags(TextIteratorBehaviorFlags);

template <>
TextIteratorBehaviorFlags adjustBehaviorFlags<EditingStrategy>(
    TextIteratorBehaviorFlags flags) {
  if (flags & TextIteratorForSelectionToString)
    return flags | TextIteratorExcludeAutofilledValue;
  return flags;
}

template <>
TextIteratorBehaviorFlags adjustBehaviorFlags<EditingInFlatTreeStrategy>(
    TextIteratorBehaviorFlags flags) {
  if (flags & TextIteratorForSelectionToString)
    flags |= TextIteratorExcludeAutofilledValue;
  return flags &
         ~(TextIteratorEntersOpenShadowRoots | TextIteratorEntersTextControls);
}

// Checks if |advance()| skips the descendants of |node|, which is the case if
// |node| is neither a shadow root nor the owner of a layout object.
static bool notSkipping(const Node& node) {
  return node.layoutObject() ||
         (node.isShadowRoot() && node.ownerShadowHost()->layoutObject());
}

// This function is like Range::pastLastNode, except for the fact that it can
// climb up out of shadow trees and ignores all nodes that will be skipped in
// |advance()|.
template <typename Strategy>
Node* pastLastNode(const Node& rangeEndContainer, int rangeEndOffset) {
  if (rangeEndOffset >= 0 && !rangeEndContainer.isCharacterDataNode() &&
      notSkipping(rangeEndContainer)) {
    for (Node* next = Strategy::childAt(rangeEndContainer, rangeEndOffset);
         next; next = Strategy::nextSibling(*next)) {
      if (notSkipping(*next))
        return next;
    }
  }
  for (const Node* node = &rangeEndContainer; node;) {
    const Node* parent = parentCrossingShadowBoundaries<Strategy>(*node);
    if (parent && notSkipping(*parent)) {
      if (Node* next = Strategy::nextSibling(*node))
        return next;
    }
    node = parent;
  }
  return nullptr;
}

// Figure out the initial value of m_shadowDepth: the depth of startContainer's
// tree scope from the common ancestor tree scope.
template <typename Strategy>
int shadowDepthOf(const Node& startContainer, const Node& endContainer);

template <>
int shadowDepthOf<EditingStrategy>(const Node& startContainer,
                                   const Node& endContainer) {
  const TreeScope* commonAncestorTreeScope =
      startContainer.treeScope().commonAncestorTreeScope(
          endContainer.treeScope());
  DCHECK(commonAncestorTreeScope);
  int shadowDepth = 0;
  for (const TreeScope* treeScope = &startContainer.treeScope();
       treeScope != commonAncestorTreeScope;
       treeScope = treeScope->parentTreeScope())
    ++shadowDepth;
  return shadowDepth;
}

template <>
int shadowDepthOf<EditingInFlatTreeStrategy>(const Node& startContainer,
                                             const Node& endContainer) {
  return 0;
}

}  // namespace

template <typename Strategy>
TextIteratorAlgorithm<Strategy>::TextIteratorAlgorithm(
    const PositionTemplate<Strategy>& start,
    const PositionTemplate<Strategy>& end,
    TextIteratorBehaviorFlags behavior)
    : m_offset(0),
      m_startContainer(nullptr),
      m_startOffset(0),
      m_endContainer(nullptr),
      m_endOffset(0),
      m_needsAnotherNewline(false),
      m_textBox(nullptr),
      m_remainingTextBox(nullptr),
      m_firstLetterText(nullptr),
      m_lastTextNode(nullptr),
      m_lastTextNodeEndedWithCollapsedSpace(false),
      m_sortedTextBoxesPosition(0),
      m_behavior(adjustBehaviorFlags<Strategy>(behavior)),
      m_handledFirstLetter(false),
      m_shouldStop(false),
      m_handleShadowRoot(false),
      m_firstLetterStartOffset(kInvalidOffset),
      m_remainingTextStartOffset(kInvalidOffset),
      // The call to emitsOriginalText() must occur after m_behavior is
      // initialized.
      m_textState(emitsOriginalText()) {
  DCHECK(start.isNotNull());
  DCHECK(end.isNotNull());

  // TODO(dglazkov): TextIterator should not be created for documents that don't
  // have a frame, but it currently still happens in some cases. See
  // http://crbug.com/591877 for details.
  DCHECK(!start.document()->view() || !start.document()->view()->needsLayout());
  DCHECK(!start.document()->needsLayoutTreeUpdate());

  if (start.compareTo(end) > 0) {
    initialize(end.computeContainerNode(), end.computeOffsetInContainerNode(),
               start.computeContainerNode(),
               start.computeOffsetInContainerNode());
    return;
  }
  initialize(start.computeContainerNode(), start.computeOffsetInContainerNode(),
             end.computeContainerNode(), end.computeOffsetInContainerNode());
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::prepareForFirstLetterInitialization() {
  if (m_node != m_startContainer)
    return false;

  if (m_node->getNodeType() != Node::kTextNode)
    return false;

  Text* textNode = toText(m_node);
  LayoutText* layoutObject = textNode->layoutObject();
  if (!layoutObject || !layoutObject->isTextFragment())
    return false;

  LayoutTextFragment* textFragment = toLayoutTextFragment(layoutObject);
  if (!textFragment->isRemainingTextLayoutObject())
    return false;

  if (static_cast<unsigned>(m_startOffset) >= textFragment->textStartOffset()) {
    m_remainingTextStartOffset =
        m_startOffset - textFragment->textStartOffset();
  } else {
    m_firstLetterStartOffset = m_startOffset;
  }
  m_offset = 0;

  return true;
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::hasNotAdvancedToStartPosition() {
  if (atEnd())
    return false;
  if (m_remainingTextStartOffset == kInvalidOffset)
    return false;
  return m_node == m_startContainer;
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::initialize(Node* startContainer,
                                                 int startOffset,
                                                 Node* endContainer,
                                                 int endOffset) {
  DCHECK(startContainer);
  DCHECK(endContainer);

  // Remember the range - this does not change.
  m_startContainer = startContainer;
  m_startOffset = startOffset;
  m_endContainer = endContainer;
  m_endOffset = endOffset;
  m_endNode =
      endContainer && !endContainer->isCharacterDataNode() && endOffset > 0
          ? Strategy::childAt(*endContainer, endOffset - 1)
          : nullptr;

  m_shadowDepth = shadowDepthOf<Strategy>(*startContainer, *endContainer);

  // Set up the current node for processing.
  if (startContainer->isCharacterDataNode())
    m_node = startContainer;
  else if (Node* child = Strategy::childAt(*startContainer, startOffset))
    m_node = child;
  else if (!startOffset)
    m_node = startContainer;
  else
    m_node = Strategy::nextSkippingChildren(*startContainer);

  if (!m_node)
    return;

  m_fullyClippedStack.setUpFullyClippedStack(m_node);
  if (!prepareForFirstLetterInitialization())
    m_offset = m_node == m_startContainer ? m_startOffset : 0;
  m_iterationProgress = HandledNone;

  // Calculate first out of bounds node.
  m_pastEndNode =
      endContainer ? pastLastNode<Strategy>(*endContainer, endOffset) : nullptr;

  // Identify the first run.
  advance();

  // The current design cannot start in a text node with arbitrary offset, if
  // the node has :first-letter. Instead, we start with offset 0, and have extra
  // advance() calls until we have moved to/past the starting position.
  while (hasNotAdvancedToStartPosition())
    advance();

  // Clear temporary data for initialization with :first-letter.
  m_firstLetterStartOffset = kInvalidOffset;
  m_remainingTextStartOffset = kInvalidOffset;
}

template <typename Strategy>
TextIteratorAlgorithm<Strategy>::~TextIteratorAlgorithm() {
  if (!m_handleShadowRoot)
    return;
  Document* document = ownerDocument();
  if (!document)
    return;
  if (m_behavior & TextIteratorForInnerText)
    UseCounter::count(document, UseCounter::InnerTextWithShadowTree);
  if (m_behavior & TextIteratorForSelectionToString)
    UseCounter::count(document, UseCounter::SelectionToStringWithShadowTree);
  if (m_behavior & TextIteratorForWindowFind)
    UseCounter::count(document, UseCounter::WindowFindWithShadowTree);
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::isInsideAtomicInlineElement() const {
  if (atEnd() || length() != 1 || !m_node)
    return false;

  LayoutObject* layoutObject = m_node->layoutObject();
  return layoutObject && layoutObject->isAtomicInlineLevel();
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::advance() {
  if (m_shouldStop)
    return;

  if (m_node)
    DCHECK(!m_node->document().needsLayoutTreeUpdate()) << m_node;

  m_textState.resetRunInformation();

  // handle remembered node that needed a newline after the text node's newline
  if (m_needsAnotherNewline) {
    // Emit the extra newline, and position it *inside* m_node, after m_node's
    // contents, in case it's a block, in the same way that we position the
    // first newline. The range for the emitted newline should start where the
    // line break begins.
    // FIXME: It would be cleaner if we emitted two newlines during the last
    // iteration, instead of using m_needsAnotherNewline.
    Node* lastChild = Strategy::lastChild(*m_node);
    Node* baseNode = lastChild ? lastChild : m_node.get();
    spliceBuffer('\n', Strategy::parent(*baseNode), baseNode, 1, 1);
    m_needsAnotherNewline = false;
    return;
  }

  if (!m_textBox && m_remainingTextBox) {
    m_textBox = m_remainingTextBox;
    m_remainingTextBox = 0;
    m_firstLetterText = nullptr;
    m_offset = 0;
  }
  // handle remembered text box
  if (m_textBox) {
    handleTextBox();
    if (m_textState.positionNode())
      return;
  }

  while (m_node && (m_node != m_pastEndNode || m_shadowDepth > 0)) {
    if (!m_shouldStop && stopsOnFormControls() &&
        HTMLFormControlElement::enclosingFormControlElement(m_node))
      m_shouldStop = true;

    // if the range ends at offset 0 of an element, represent the
    // position, but not the content, of that element e.g. if the
    // node is a blockflow element, emit a newline that
    // precedes the element
    if (m_node == m_endContainer && !m_endOffset) {
      representNodeOffsetZero();
      m_node = nullptr;
      return;
    }

    LayoutObject* layoutObject = m_node->layoutObject();
    if (!layoutObject) {
      if (m_node->isShadowRoot()) {
        // A shadow root doesn't have a layoutObject, but we want to visit
        // children anyway.
        m_iterationProgress = m_iterationProgress < HandledNode
                                  ? HandledNode
                                  : m_iterationProgress;
        m_handleShadowRoot = true;
      } else {
        m_iterationProgress = HandledChildren;
      }
    } else {
      // Enter author shadow roots, from youngest, if any and if necessary.
      if (m_iterationProgress < HandledOpenShadowRoots) {
        if (entersOpenShadowRoots() && m_node->isElementNode() &&
            toElement(m_node)->openShadowRoot()) {
          ShadowRoot* youngestShadowRoot = toElement(m_node)->openShadowRoot();
          DCHECK(youngestShadowRoot->type() == ShadowRootType::V0 ||
                 youngestShadowRoot->type() == ShadowRootType::Open);
          m_node = youngestShadowRoot;
          m_iterationProgress = HandledNone;
          ++m_shadowDepth;
          m_fullyClippedStack.pushFullyClippedState(m_node);
          continue;
        }

        m_iterationProgress = HandledOpenShadowRoots;
      }

      // Enter user-agent shadow root, if necessary.
      if (m_iterationProgress < HandledUserAgentShadowRoot) {
        if (entersTextControls() && layoutObject->isTextControl()) {
          ShadowRoot* userAgentShadowRoot =
              toElement(m_node)->userAgentShadowRoot();
          DCHECK(userAgentShadowRoot->type() == ShadowRootType::UserAgent);
          m_node = userAgentShadowRoot;
          m_iterationProgress = HandledNone;
          ++m_shadowDepth;
          m_fullyClippedStack.pushFullyClippedState(m_node);
          continue;
        }
        m_iterationProgress = HandledUserAgentShadowRoot;
      }

      // Handle the current node according to its type.
      if (m_iterationProgress < HandledNode) {
        bool handledNode = false;
        if (layoutObject->isText() &&
            m_node->getNodeType() ==
                Node::kTextNode) {  // FIXME: What about kCdataSectionNode?
          if (!m_fullyClippedStack.top() || ignoresStyleVisibility())
            handledNode = handleTextNode();
        } else if (layoutObject &&
                   (layoutObject->isImage() || layoutObject->isLayoutPart() ||
                    (m_node && m_node->isHTMLElement() &&
                     (isHTMLFormControlElement(toHTMLElement(*m_node)) ||
                      isHTMLLegendElement(toHTMLElement(*m_node)) ||
                      isHTMLImageElement(toHTMLElement(*m_node)) ||
                      isHTMLMeterElement(toHTMLElement(*m_node)) ||
                      isHTMLProgressElement(toHTMLElement(*m_node)))))) {
          handledNode = handleReplacedElement();
        } else {
          handledNode = handleNonTextNode();
        }
        if (handledNode)
          m_iterationProgress = HandledNode;
        if (m_textState.positionNode())
          return;
      }
    }

    // Find a new current node to handle in depth-first manner,
    // calling exitNode() as we come back thru a parent node.
    //
    // 1. Iterate over child nodes, if we haven't done yet.
    // To support |TextIteratorEmitsImageAltText|, we don't traversal child
    // nodes, in flat tree.
    Node* next =
        m_iterationProgress < HandledChildren && !isHTMLImageElement(*m_node)
            ? Strategy::firstChild(*m_node)
            : nullptr;
    m_offset = 0;
    if (!next) {
      // 2. If we've already iterated children or they are not available, go to
      // the next sibling node.
      next = Strategy::nextSibling(*m_node);
      if (!next) {
        // 3. If we are at the last child, go up the node tree until we find a
        // next sibling.
        ContainerNode* parentNode = Strategy::parent(*m_node);
        while (!next && parentNode) {
          if (m_node == m_endNode ||
              Strategy::isDescendantOf(*m_endContainer, *parentNode))
            return;
          bool haveLayoutObject = m_node->layoutObject();
          m_node = parentNode;
          m_fullyClippedStack.pop();
          parentNode = Strategy::parent(*m_node);
          if (haveLayoutObject)
            exitNode();
          if (m_textState.positionNode()) {
            m_iterationProgress = HandledChildren;
            return;
          }
          next = Strategy::nextSibling(*m_node);
        }

        if (!next && !parentNode && m_shadowDepth > 0) {
          // 4. Reached the top of a shadow root. If it's created by author,
          // then try to visit the next
          // sibling shadow root, if any.
          if (!m_node->isShadowRoot()) {
            NOTREACHED();
            m_shouldStop = true;
            return;
          }
          ShadowRoot* shadowRoot = toShadowRoot(m_node);
          if (shadowRoot->type() == ShadowRootType::V0 ||
              shadowRoot->type() == ShadowRootType::Open) {
            ShadowRoot* nextShadowRoot = shadowRoot->olderShadowRoot();
            if (nextShadowRoot &&
                nextShadowRoot->type() == ShadowRootType::V0) {
              m_fullyClippedStack.pop();
              m_node = nextShadowRoot;
              m_iterationProgress = HandledNone;
              // m_shadowDepth is unchanged since we exit from a shadow root and
              // enter another.
              m_fullyClippedStack.pushFullyClippedState(m_node);
            } else {
              // We are the last shadow root; exit from here and go back to
              // where we were.
              m_node = &shadowRoot->host();
              m_iterationProgress = HandledOpenShadowRoots;
              --m_shadowDepth;
              m_fullyClippedStack.pop();
            }
          } else {
            // If we are in a closed or user-agent shadow root, then go back to
            // the host.
            // TODO(kochi): Make sure we treat closed shadow as user agent
            // shadow here.
            DCHECK(shadowRoot->type() == ShadowRootType::Closed ||
                   shadowRoot->type() == ShadowRootType::UserAgent);
            m_node = &shadowRoot->host();
            m_iterationProgress = HandledUserAgentShadowRoot;
            --m_shadowDepth;
            m_fullyClippedStack.pop();
          }
          m_handledFirstLetter = false;
          m_firstLetterText = nullptr;
          continue;
        }
      }
      m_fullyClippedStack.pop();
    }

    // set the new current node
    m_node = next;
    if (m_node)
      m_fullyClippedStack.pushFullyClippedState(m_node);
    m_iterationProgress = HandledNone;
    m_handledFirstLetter = false;
    m_firstLetterText = nullptr;

    // how would this ever be?
    if (m_textState.positionNode())
      return;
  }
}

static bool hasVisibleTextNode(LayoutText* layoutObject) {
  if (layoutObject->style()->visibility() == EVisibility::kVisible)
    return true;

  if (!layoutObject->isTextFragment())
    return false;

  LayoutTextFragment* fragment = toLayoutTextFragment(layoutObject);
  if (!fragment->isRemainingTextLayoutObject())
    return false;

  DCHECK(fragment->firstLetterPseudoElement());
  LayoutObject* pseudoElementLayoutObject =
      fragment->firstLetterPseudoElement()->layoutObject();
  return pseudoElementLayoutObject &&
         pseudoElementLayoutObject->style()->visibility() ==
             EVisibility::kVisible;
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::handleTextNode() {
  if (excludesAutofilledValue()) {
    TextControlElement* control = enclosingTextControl(m_node);
    // For security reason, we don't expose suggested value if it is
    // auto-filled.
    if (control && control->isAutofilled())
      return true;
  }

  Text* textNode = toText(m_node);
  LayoutText* layoutObject = textNode->layoutObject();

  m_lastTextNode = textNode;
  String str = layoutObject->text();

  // handle pre-formatted text
  if (!layoutObject->style()->collapseWhiteSpace()) {
    int runStart = m_offset;
    if (m_lastTextNodeEndedWithCollapsedSpace &&
        hasVisibleTextNode(layoutObject)) {
      if (m_behavior & TextIteratorCollapseTrailingSpace) {
        if (runStart > 0 && str[runStart - 1] == ' ') {
          spliceBuffer(spaceCharacter, textNode, 0, runStart, runStart);
          return false;
        }
      } else {
        spliceBuffer(spaceCharacter, textNode, 0, runStart, runStart);
        return false;
      }
    }
    if (!m_handledFirstLetter && layoutObject->isTextFragment() && !m_offset) {
      handleTextNodeFirstLetter(toLayoutTextFragment(layoutObject));
      if (m_firstLetterText) {
        String firstLetter = m_firstLetterText->text();
        emitText(textNode, m_firstLetterText, m_offset,
                 m_offset + firstLetter.length());
        m_firstLetterText = nullptr;
        m_textBox = 0;
        return false;
      }
    }
    if (layoutObject->style()->visibility() != EVisibility::kVisible &&
        !ignoresStyleVisibility())
      return false;
    int strLength = str.length();
    int end = (textNode == m_endContainer) ? m_endOffset : INT_MAX;
    int runEnd = std::min(strLength, end);

    if (runStart >= runEnd)
      return true;

    emitText(textNode, textNode->layoutObject(), runStart, runEnd);
    return true;
  }

  if (layoutObject->firstTextBox())
    m_textBox = layoutObject->firstTextBox();

  bool shouldHandleFirstLetter =
      !m_handledFirstLetter && layoutObject->isTextFragment() && !m_offset;
  if (shouldHandleFirstLetter)
    handleTextNodeFirstLetter(toLayoutTextFragment(layoutObject));

  if (!layoutObject->firstTextBox() && str.length() > 0 &&
      !shouldHandleFirstLetter) {
    if (layoutObject->style()->visibility() != EVisibility::kVisible &&
        !ignoresStyleVisibility())
      return false;
    m_lastTextNodeEndedWithCollapsedSpace =
        true;  // entire block is collapsed space
    return true;
  }

  if (m_firstLetterText)
    layoutObject = m_firstLetterText;

  // Used when text boxes are out of order (Hebrew/Arabic w/ embeded LTR text)
  if (layoutObject->containsReversedText()) {
    m_sortedTextBoxes.clear();
    for (InlineTextBox* textBox = layoutObject->firstTextBox(); textBox;
         textBox = textBox->nextTextBox()) {
      m_sortedTextBoxes.append(textBox);
    }
    std::sort(m_sortedTextBoxes.begin(), m_sortedTextBoxes.end(),
              InlineTextBox::compareByStart);
    m_sortedTextBoxesPosition = 0;
    m_textBox = m_sortedTextBoxes.isEmpty() ? 0 : m_sortedTextBoxes[0];
  }

  handleTextBox();
  return true;
}

// Restore the collapsed space for copy & paste. See http://crbug.com/318925
template <typename Strategy>
size_t TextIteratorAlgorithm<Strategy>::restoreCollapsedTrailingSpace(
    InlineTextBox* nextTextBox,
    size_t subrunEnd) {
  if (nextTextBox || !m_textBox->root().nextRootBox() ||
      m_textBox->root().lastChild() != m_textBox)
    return subrunEnd;

  const String& text = toLayoutText(m_node->layoutObject())->text();
  if (text.endsWith(' ') == 0 || subrunEnd != text.length() - 1 ||
      text[subrunEnd - 1] == ' ')
    return subrunEnd;

  // If there is the leading space in the next line, we don't need to restore
  // the trailing space.
  // Example: <div style="width: 2em;"><b><i>foo </i></b> bar</div>
  InlineBox* firstBoxOfNextLine = m_textBox->root().nextRootBox()->firstChild();
  if (!firstBoxOfNextLine)
    return subrunEnd + 1;
  Node* firstNodeOfNextLine = firstBoxOfNextLine->getLineLayoutItem().node();
  if (!firstNodeOfNextLine || firstNodeOfNextLine->nodeValue()[0] != ' ')
    return subrunEnd + 1;

  return subrunEnd;
}

template <typename Strategy>
unsigned TextIteratorAlgorithm<Strategy>::restoreCollapsedLeadingSpace(
    unsigned runStart) {
  if (emitsImageAltText() || doesNotBreakAtReplacedElement() ||
      forInnerText() || !m_textBox->root().prevRootBox() ||
      m_textBox->root().firstChild() != m_textBox)
    return runStart;

  const String& text = toLayoutText(m_node->layoutObject())->text();
  InlineBox* lastBoxOfPrevLine = m_textBox->root().prevRootBox()->lastChild();
  if (m_textBox->getLineLayoutItem() ==
          lastBoxOfPrevLine->getLineLayoutItem() ||
      lastBoxOfPrevLine->getLineLayoutItem().isBR() ||
      lastBoxOfPrevLine->isInlineFlowBox())
    return runStart;
  if (runStart > 0 && text.length() >= 2 && text[0] == ' ' && text[1] != ' ')
    return runStart - 1;

  return runStart;
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::handleTextBox() {
  LayoutText* layoutObject = m_firstLetterText
                                 ? m_firstLetterText
                                 : toLayoutText(m_node->layoutObject());

  if (layoutObject->style()->visibility() != EVisibility::kVisible &&
      !ignoresStyleVisibility()) {
    m_textBox = nullptr;
  } else {
    String str = layoutObject->text();
    unsigned start = m_offset;
    unsigned end = (m_node == m_endContainer)
                       ? static_cast<unsigned>(m_endOffset)
                       : INT_MAX;
    while (m_textBox) {
      unsigned textBoxStart = m_textBox->start();
      unsigned runStart = std::max(textBoxStart, start);

      // Check for collapsed space at the start of this run.
      InlineTextBox* firstTextBox =
          layoutObject->containsReversedText()
              ? (m_sortedTextBoxes.isEmpty() ? 0 : m_sortedTextBoxes[0])
              : layoutObject->firstTextBox();
      bool needSpace = m_lastTextNodeEndedWithCollapsedSpace ||
                       (m_textBox == firstTextBox && textBoxStart == runStart &&
                        runStart > 0);
      if (needSpace &&
          !layoutObject->style()->isCollapsibleWhiteSpace(
              m_textState.lastCharacter()) &&
          m_textState.lastCharacter()) {
        if (m_lastTextNode == m_node && runStart > 0 &&
            str[runStart - 1] == ' ') {
          unsigned spaceRunStart = runStart - 1;
          while (spaceRunStart > 0 && str[spaceRunStart - 1] == ' ')
            --spaceRunStart;
          emitText(m_node, layoutObject, spaceRunStart, spaceRunStart + 1);
        } else {
          spliceBuffer(spaceCharacter, m_node, 0, runStart, runStart);
        }
        return;
      }
      unsigned textBoxEnd = textBoxStart + m_textBox->len();
      unsigned runEnd = std::min(textBoxEnd, end);

      // Determine what the next text box will be, but don't advance yet
      InlineTextBox* nextTextBox = nullptr;
      if (layoutObject->containsReversedText()) {
        if (m_sortedTextBoxesPosition + 1 < m_sortedTextBoxes.size())
          nextTextBox = m_sortedTextBoxes[m_sortedTextBoxesPosition + 1];
      } else {
        nextTextBox = m_textBox->nextTextBox();
      }

      // FIXME: Based on the outcome of crbug.com/446502 it's possible we can
      //   remove this block. The reason we new it now is because BIDI and
      //   FirstLetter seem to have different ideas of where things can split.
      //   FirstLetter takes the punctuation + first letter, and BIDI will
      //   split out the punctuation and possibly reorder it.
      if (nextTextBox &&
          !(nextTextBox->getLineLayoutItem().isEqual(layoutObject))) {
        m_textBox = 0;
        return;
      }
      DCHECK(!nextTextBox ||
             nextTextBox->getLineLayoutItem().isEqual(layoutObject));

      if (runStart < runEnd) {
        // Handle either a single newline character (which becomes a space),
        // or a run of characters that does not include a newline.
        // This effectively translates newlines to spaces without copying the
        // text.
        if (str[runStart] == '\n') {
          // We need to preserve new lines in case of PreLine.
          // See bug crbug.com/317365.
          if (layoutObject->style()->whiteSpace() == EWhiteSpace::kPreLine)
            spliceBuffer('\n', m_node, 0, runStart, runStart);
          else
            spliceBuffer(spaceCharacter, m_node, 0, runStart, runStart + 1);
          m_offset = runStart + 1;
        } else {
          size_t subrunEnd = str.find('\n', runStart);
          if (subrunEnd == kNotFound || subrunEnd > runEnd) {
            subrunEnd = runEnd;
            runStart = restoreCollapsedLeadingSpace(runStart);
            subrunEnd = restoreCollapsedTrailingSpace(nextTextBox, subrunEnd);
          }

          m_offset = subrunEnd;
          emitText(m_node, layoutObject, runStart, subrunEnd);
        }

        // If we are doing a subrun that doesn't go to the end of the text box,
        // come back again to finish handling this text box; don't advance to
        // the next one.
        if (static_cast<unsigned>(m_textState.positionEndOffset()) < textBoxEnd)
          return;

        // Advance and return
        unsigned nextRunStart =
            nextTextBox ? nextTextBox->start() : str.length();
        if (nextRunStart > runEnd)
          m_lastTextNodeEndedWithCollapsedSpace =
              true;  // collapsed space between runs or at the end

        m_textBox = nextTextBox;
        if (layoutObject->containsReversedText())
          ++m_sortedTextBoxesPosition;
        return;
      }
      // Advance and continue
      m_textBox = nextTextBox;
      if (layoutObject->containsReversedText())
        ++m_sortedTextBoxesPosition;
    }
  }

  if (!m_textBox && m_remainingTextBox) {
    m_textBox = m_remainingTextBox;
    m_remainingTextBox = 0;
    m_firstLetterText = nullptr;
    m_offset = 0;
    handleTextBox();
  }
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::handleTextNodeFirstLetter(
    LayoutTextFragment* layoutObject) {
  m_handledFirstLetter = true;

  if (!layoutObject->isRemainingTextLayoutObject())
    return;

  FirstLetterPseudoElement* firstLetterElement =
      layoutObject->firstLetterPseudoElement();
  if (!firstLetterElement)
    return;

  LayoutObject* pseudoLayoutObject = firstLetterElement->layoutObject();
  if (pseudoLayoutObject->style()->visibility() != EVisibility::kVisible &&
      !ignoresStyleVisibility())
    return;

  LayoutObject* firstLetter = pseudoLayoutObject->slowFirstChild();
  DCHECK(firstLetter);

  m_remainingTextBox = m_textBox;
  m_textBox = toLayoutText(firstLetter)->firstTextBox();
  m_sortedTextBoxes.clear();
  m_firstLetterText = toLayoutText(firstLetter);
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::supportsAltText(Node* node) {
  if (!node->isHTMLElement())
    return false;
  HTMLElement& element = toHTMLElement(*node);

  // FIXME: Add isSVGImageElement.
  if (isHTMLImageElement(element))
    return true;
  if (isHTMLInputElement(toHTMLElement(*node)) &&
      toHTMLInputElement(*node).type() == InputTypeNames::image)
    return true;
  return false;
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::handleReplacedElement() {
  if (m_fullyClippedStack.top())
    return false;

  LayoutObject* layoutObject = m_node->layoutObject();
  if (layoutObject->style()->visibility() != EVisibility::kVisible &&
      !ignoresStyleVisibility())
    return false;

  if (emitsObjectReplacementCharacter()) {
    spliceBuffer(objectReplacementCharacter, Strategy::parent(*m_node), m_node,
                 0, 1);
    return true;
  }

  if (m_behavior & TextIteratorCollapseTrailingSpace) {
    if (m_lastTextNode) {
      String str = m_lastTextNode->layoutObject()->text();
      if (m_lastTextNodeEndedWithCollapsedSpace && m_offset > 0 &&
          str[m_offset - 1] == ' ') {
        spliceBuffer(spaceCharacter, Strategy::parent(*m_lastTextNode),
                     m_lastTextNode, 1, 1);
        return false;
      }
    }
  } else if (m_lastTextNodeEndedWithCollapsedSpace) {
    spliceBuffer(spaceCharacter, Strategy::parent(*m_lastTextNode),
                 m_lastTextNode, 1, 1);
    return false;
  }

  if (entersTextControls() && layoutObject->isTextControl()) {
    // The shadow tree should be already visited.
    return true;
  }

  if (emitsCharactersBetweenAllVisiblePositions()) {
    // We want replaced elements to behave like punctuation for boundary
    // finding, and to simply take up space for the selection preservation
    // code in moveParagraphs, so we use a comma.
    spliceBuffer(',', Strategy::parent(*m_node), m_node, 0, 1);
    return true;
  }

  m_textState.updateForReplacedElement(m_node);

  if (emitsImageAltText() && TextIterator::supportsAltText(m_node)) {
    m_textState.emitAltText(m_node);
    if (m_textState.length())
      return true;
  }

  return true;
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::shouldEmitTabBeforeNode(Node* node) {
  LayoutObject* r = node->layoutObject();

  // Table cells are delimited by tabs.
  if (!r || !isTableCell(node))
    return false;

  // Want a tab before every cell other than the first one
  LayoutTableCell* rc = toLayoutTableCell(r);
  LayoutTable* t = rc->table();
  return t && (t->cellBefore(rc) || t->cellAbove(rc));
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::shouldEmitNewlineForNode(
    Node* node,
    bool emitsOriginalText) {
  LayoutObject* layoutObject = node->layoutObject();

  if (layoutObject ? !layoutObject->isBR() : !isHTMLBRElement(node))
    return false;
  return emitsOriginalText ||
         !(node->isInShadowTree() &&
           isHTMLInputElement(*node->ownerShadowHost()));
}

static bool shouldEmitNewlinesBeforeAndAfterNode(Node& node) {
  // Block flow (versus inline flow) is represented by having
  // a newline both before and after the element.
  LayoutObject* r = node.layoutObject();
  if (!r) {
    return (node.hasTagName(blockquoteTag) || node.hasTagName(ddTag) ||
            node.hasTagName(divTag) || node.hasTagName(dlTag) ||
            node.hasTagName(dtTag) || node.hasTagName(h1Tag) ||
            node.hasTagName(h2Tag) || node.hasTagName(h3Tag) ||
            node.hasTagName(h4Tag) || node.hasTagName(h5Tag) ||
            node.hasTagName(h6Tag) || node.hasTagName(hrTag) ||
            node.hasTagName(liTag) || node.hasTagName(listingTag) ||
            node.hasTagName(olTag) || node.hasTagName(pTag) ||
            node.hasTagName(preTag) || node.hasTagName(trTag) ||
            node.hasTagName(ulTag));
  }

  // Need to make an exception for option and optgroup, because we want to
  // keep the legacy behavior before we added layoutObjects to them.
  if (isHTMLOptionElement(node) || isHTMLOptGroupElement(node))
    return false;

  // Need to make an exception for table cells, because they are blocks, but we
  // want them tab-delimited rather than having newlines before and after.
  if (isTableCell(&node))
    return false;

  // Need to make an exception for table row elements, because they are neither
  // "inline" or "LayoutBlock", but we want newlines for them.
  if (r->isTableRow()) {
    LayoutTable* t = toLayoutTableRow(r)->table();
    if (t && !t->isInline())
      return true;
  }

  return !r->isInline() && r->isLayoutBlock() &&
         !r->isFloatingOrOutOfFlowPositioned() && !r->isBody() &&
         !r->isRubyText();
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::shouldEmitNewlineAfterNode(Node& node) {
  // FIXME: It should be better but slower to create a VisiblePosition here.
  if (!shouldEmitNewlinesBeforeAndAfterNode(node))
    return false;
  // Check if this is the very last layoutObject in the document.
  // If so, then we should not emit a newline.
  Node* next = &node;
  do {
    next = Strategy::nextSkippingChildren(*next);
    if (next && next->layoutObject())
      return true;
  } while (next);
  return false;
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::shouldEmitNewlineBeforeNode(Node& node) {
  return shouldEmitNewlinesBeforeAndAfterNode(node);
}

static bool shouldEmitExtraNewlineForNode(Node* node) {
  // When there is a significant collapsed bottom margin, emit an extra
  // newline for a more realistic result. We end up getting the right
  // result even without margin collapsing. For example: <div><p>text</p></div>
  // will work right even if both the <div> and the <p> have bottom margins.
  LayoutObject* r = node->layoutObject();
  if (!r || !r->isBox())
    return false;

  // NOTE: We only do this for a select set of nodes, and fwiw WinIE appears
  // not to do this at all
  if (node->hasTagName(h1Tag) || node->hasTagName(h2Tag) ||
      node->hasTagName(h3Tag) || node->hasTagName(h4Tag) ||
      node->hasTagName(h5Tag) || node->hasTagName(h6Tag) ||
      node->hasTagName(pTag)) {
    const ComputedStyle* style = r->style();
    if (style) {
      int bottomMargin = toLayoutBox(r)->collapsedMarginAfter().toInt();
      int fontSize = style->getFontDescription().computedPixelSize();
      if (bottomMargin * 2 >= fontSize)
        return true;
    }
  }

  return false;
}

// Whether or not we should emit a character as we enter m_node (if it's a
// container) or as we hit it (if it's atomic).
template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::shouldRepresentNodeOffsetZero() {
  if (emitsCharactersBetweenAllVisiblePositions() &&
      isDisplayInsideTable(m_node))
    return true;

  // Leave element positioned flush with start of a paragraph
  // (e.g. do not insert tab before a table cell at the start of a paragraph)
  if (m_textState.lastCharacter() == '\n')
    return false;

  // Otherwise, show the position if we have emitted any characters
  if (m_textState.hasEmitted())
    return true;

  // We've not emitted anything yet. Generally, there is no need for any
  // positioning then. The only exception is when the element is visually not in
  // the same line as the start of the range (e.g. the range starts at the end
  // of the previous paragraph).
  // NOTE: Creating VisiblePositions and comparing them is relatively expensive,
  // so we make quicker checks to possibly avoid that. Another check that we
  // could make is is whether the inline vs block flow changed since the
  // previous visible element. I think we're already in a special enough case
  // that that won't be needed, tho.

  // No character needed if this is the first node in the range.
  if (m_node == m_startContainer)
    return false;

  // If we are outside the start container's subtree, assume we need to emit.
  // FIXME: m_startContainer could be an inline block
  if (!Strategy::isDescendantOf(*m_node, *m_startContainer))
    return true;

  // If we started as m_startContainer offset 0 and the current node is a
  // descendant of the start container, we already had enough context to
  // correctly decide whether to emit after a preceding block. We chose not to
  // emit (m_hasEmitted is false), so don't second guess that now.
  // NOTE: Is this really correct when m_node is not a leftmost descendant?
  // Probably immaterial since we likely would have already emitted something by
  // now.
  if (!m_startOffset)
    return false;

  // If this node is unrendered or invisible the VisiblePosition checks below
  // won't have much meaning.
  // Additionally, if the range we are iterating over contains huge sections of
  // unrendered content, we would create VisiblePositions on every call to this
  // function without this check.
  if (!m_node->layoutObject() ||
      m_node->layoutObject()->style()->visibility() != EVisibility::kVisible ||
      (m_node->layoutObject()->isLayoutBlockFlow() &&
       !toLayoutBlock(m_node->layoutObject())->size().height() &&
       !isHTMLBodyElement(*m_node)))
    return false;

  // The startPos.isNotNull() check is needed because the start could be before
  // the body, and in that case we'll get null. We don't want to put in newlines
  // at the start in that case.
  // The currPos.isNotNull() check is needed because positions in non-HTML
  // content (like SVG) do not have visible positions, and we don't want to emit
  // for them either.
  VisiblePosition startPos =
      createVisiblePosition(Position(m_startContainer, m_startOffset));
  VisiblePosition currPos = VisiblePosition::beforeNode(m_node);
  return startPos.isNotNull() && currPos.isNotNull() &&
         !inSameLine(startPos, currPos);
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::shouldEmitSpaceBeforeAndAfterNode(
    Node* node) {
  return isDisplayInsideTable(node) &&
         (node->layoutObject()->isInline() ||
          emitsCharactersBetweenAllVisiblePositions());
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::representNodeOffsetZero() {
  // Emit a character to show the positioning of m_node.

  // When we haven't been emitting any characters,
  // shouldRepresentNodeOffsetZero() can create VisiblePositions, which is
  // expensive. So, we perform the inexpensive checks on m_node to see if it
  // necessitates emitting a character first and will early return before
  // encountering shouldRepresentNodeOffsetZero()s worse case behavior.
  if (shouldEmitTabBeforeNode(m_node)) {
    if (shouldRepresentNodeOffsetZero())
      spliceBuffer('\t', Strategy::parent(*m_node), m_node, 0, 0);
  } else if (shouldEmitNewlineBeforeNode(*m_node)) {
    if (shouldRepresentNodeOffsetZero())
      spliceBuffer('\n', Strategy::parent(*m_node), m_node, 0, 0);
  } else if (shouldEmitSpaceBeforeAndAfterNode(m_node)) {
    if (shouldRepresentNodeOffsetZero())
      spliceBuffer(spaceCharacter, Strategy::parent(*m_node), m_node, 0, 0);
  }
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::handleNonTextNode() {
  if (shouldEmitNewlineForNode(m_node, emitsOriginalText()))
    spliceBuffer('\n', Strategy::parent(*m_node), m_node, 0, 1);
  else if (emitsCharactersBetweenAllVisiblePositions() &&
           m_node->layoutObject() && m_node->layoutObject()->isHR())
    spliceBuffer(spaceCharacter, Strategy::parent(*m_node), m_node, 0, 1);
  else
    representNodeOffsetZero();

  return true;
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::exitNode() {
  // prevent emitting a newline when exiting a collapsed block at beginning of
  // the range
  // FIXME: !m_hasEmitted does not necessarily mean there was a collapsed
  // block... it could have been an hr (e.g.). Also, a collapsed block could
  // have height (e.g. a table) and therefore look like a blank line.
  if (!m_textState.hasEmitted())
    return;

  // Emit with a position *inside* m_node, after m_node's contents, in
  // case it is a block, because the run should start where the
  // emitted character is positioned visually.
  Node* lastChild = Strategy::lastChild(*m_node);
  Node* baseNode = lastChild ? lastChild : m_node.get();
  // FIXME: This shouldn't require the m_lastTextNode to be true, but we can't
  // change that without making the logic in _web_attributedStringFromRange
  // match. We'll get that for free when we switch to use TextIterator in
  // _web_attributedStringFromRange. See <rdar://problem/5428427> for an example
  // of how this mismatch will cause problems.
  if (m_lastTextNode && shouldEmitNewlineAfterNode(*m_node)) {
    // use extra newline to represent margin bottom, as needed
    bool addNewline = shouldEmitExtraNewlineForNode(m_node);

    // FIXME: We need to emit a '\n' as we leave an empty block(s) that
    // contain a VisiblePosition when doing selection preservation.
    if (m_textState.lastCharacter() != '\n') {
      // insert a newline with a position following this block's contents.
      spliceBuffer(newlineCharacter, Strategy::parent(*baseNode), baseNode, 1,
                   1);
      // remember whether to later add a newline for the current node
      DCHECK(!m_needsAnotherNewline);
      m_needsAnotherNewline = addNewline;
    } else if (addNewline) {
      // insert a newline with a position following this block's contents.
      spliceBuffer(newlineCharacter, Strategy::parent(*baseNode), baseNode, 1,
                   1);
    }
  }

  // If nothing was emitted, see if we need to emit a space.
  if (!m_textState.positionNode() && shouldEmitSpaceBeforeAndAfterNode(m_node))
    spliceBuffer(spaceCharacter, Strategy::parent(*baseNode), baseNode, 1, 1);
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::spliceBuffer(UChar c,
                                                   Node* textNode,
                                                   Node* offsetBaseNode,
                                                   int textStartOffset,
                                                   int textEndOffset) {
  // Since m_lastTextNodeEndedWithCollapsedSpace seems better placed in
  // TextIterator, but is always reset when we call spliceBuffer, we
  // wrap TextIteratorTextState::spliceBuffer() with this function.
  m_textState.spliceBuffer(c, textNode, offsetBaseNode, textStartOffset,
                           textEndOffset);
  m_lastTextNodeEndedWithCollapsedSpace = false;
}

template <typename Strategy>
int TextIteratorAlgorithm<Strategy>::adjustedStartForFirstLetter(
    const Node& textNode,
    const LayoutText& layoutObject,
    int textStartOffset,
    int textEndOffset) {
  if (m_firstLetterStartOffset == kInvalidOffset)
    return textStartOffset;
  if (textNode != m_startContainer)
    return textStartOffset;
  if (!layoutObject.isTextFragment())
    return textStartOffset;
  if (toLayoutTextFragment(layoutObject).isRemainingTextLayoutObject())
    return textStartOffset;
  if (textEndOffset <= m_firstLetterStartOffset)
    return textStartOffset;
  int adjustedOffset = std::max(textStartOffset, m_firstLetterStartOffset);
  m_firstLetterStartOffset = kInvalidOffset;
  return adjustedOffset;
}

template <typename Strategy>
int TextIteratorAlgorithm<Strategy>::adjustedStartForRemainingText(
    const Node& textNode,
    const LayoutText& layoutObject,
    int textStartOffset,
    int textEndOffset) {
  if (m_remainingTextStartOffset == kInvalidOffset)
    return textStartOffset;
  if (textNode != m_startContainer)
    return textStartOffset;
  if (!layoutObject.isTextFragment())
    return textStartOffset;
  if (!toLayoutTextFragment(layoutObject).isRemainingTextLayoutObject())
    return textStartOffset;
  if (textEndOffset <= m_remainingTextStartOffset)
    return textStartOffset;
  int adjustedOffset = std::max(textStartOffset, m_remainingTextStartOffset);
  m_remainingTextStartOffset = kInvalidOffset;
  return adjustedOffset;
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::emitText(Node* textNode,
                                               LayoutText* layoutObject,
                                               int textStartOffset,
                                               int textEndOffset) {
  textStartOffset = adjustedStartForFirstLetter(*textNode, *layoutObject,
                                                textStartOffset, textEndOffset);
  textStartOffset = adjustedStartForRemainingText(
      *textNode, *layoutObject, textStartOffset, textEndOffset);
  // Since m_lastTextNodeEndedWithCollapsedSpace seems better placed in
  // TextIterator, but is always reset when we call spliceBuffer, we
  // wrap TextIteratorTextState::spliceBuffer() with this function.
  m_textState.emitText(textNode, layoutObject, textStartOffset, textEndOffset);
  m_lastTextNodeEndedWithCollapsedSpace = false;
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy> TextIteratorAlgorithm<Strategy>::range()
    const {
  // use the current run information, if we have it
  if (m_textState.positionNode()) {
    return EphemeralRangeTemplate<Strategy>(startPositionInCurrentContainer(),
                                            endPositionInCurrentContainer());
  }

  // otherwise, return the end of the overall range we were given
  if (m_endContainer)
    return EphemeralRangeTemplate<Strategy>(
        PositionTemplate<Strategy>(m_endContainer, m_endOffset));

  return EphemeralRangeTemplate<Strategy>();
}

template <typename Strategy>
Document* TextIteratorAlgorithm<Strategy>::ownerDocument() const {
  if (m_textState.positionNode())
    return &m_textState.positionNode()->document();
  if (m_endContainer)
    return &m_endContainer->document();
  return 0;
}

template <typename Strategy>
Node* TextIteratorAlgorithm<Strategy>::node() const {
  if (m_textState.positionNode() || m_endContainer) {
    Node* node = currentContainer();
    if (node->isCharacterDataNode())
      return node;
    return Strategy::childAt(*node, startOffsetInCurrentContainer());
  }
  return 0;
}

template <typename Strategy>
int TextIteratorAlgorithm<Strategy>::startOffsetInCurrentContainer() const {
  if (m_textState.positionNode()) {
    m_textState.flushPositionOffsets();
    return m_textState.positionStartOffset() + m_textState.textStartOffset();
  }
  DCHECK(m_endContainer);
  return m_endOffset;
}

template <typename Strategy>
int TextIteratorAlgorithm<Strategy>::endOffsetInCurrentContainer() const {
  if (m_textState.positionNode()) {
    m_textState.flushPositionOffsets();
    return m_textState.positionEndOffset() + m_textState.textStartOffset();
  }
  DCHECK(m_endContainer);
  return m_endOffset;
}

template <typename Strategy>
Node* TextIteratorAlgorithm<Strategy>::currentContainer() const {
  if (m_textState.positionNode()) {
    return m_textState.positionNode();
  }
  DCHECK(m_endContainer);
  return m_endContainer;
}

template <typename Strategy>
PositionTemplate<Strategy>
TextIteratorAlgorithm<Strategy>::startPositionInCurrentContainer() const {
  return PositionTemplate<Strategy>::editingPositionOf(
      currentContainer(), startOffsetInCurrentContainer());
}

template <typename Strategy>
PositionTemplate<Strategy>
TextIteratorAlgorithm<Strategy>::endPositionInCurrentContainer() const {
  return PositionTemplate<Strategy>::editingPositionOf(
      currentContainer(), endOffsetInCurrentContainer());
}

template <typename Strategy>
int TextIteratorAlgorithm<Strategy>::rangeLength(
    const PositionTemplate<Strategy>& start,
    const PositionTemplate<Strategy>& end,
    bool forSelectionPreservation) {
  DCHECK(start.document());
  DocumentLifecycle::DisallowTransitionScope disallowTransition(
      start.document()->lifecycle());

  int length = 0;
  TextIteratorBehaviorFlags behaviorFlags =
      TextIteratorEmitsObjectReplacementCharacter;
  if (forSelectionPreservation)
    behaviorFlags |= TextIteratorEmitsCharactersBetweenAllVisiblePositions;
  for (TextIteratorAlgorithm<Strategy> it(start, end, behaviorFlags);
       !it.atEnd(); it.advance())
    length += it.length();

  return length;
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::isInTextSecurityMode() const {
  return isTextSecurityNode(node());
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::isBetweenSurrogatePair(
    int position) const {
  DCHECK_GE(position, 0);
  return position > 0 && position < length() &&
         U16_IS_LEAD(characterAt(position - 1)) &&
         U16_IS_TRAIL(characterAt(position));
}

template <typename Strategy>
int TextIteratorAlgorithm<Strategy>::copyTextTo(ForwardsTextBuffer* output,
                                                int position,
                                                int minLength) const {
  int end = std::min(length(), position + minLength);
  if (isBetweenSurrogatePair(end))
    ++end;
  int copiedLength = end - position;
  copyCodeUnitsTo(output, position, copiedLength);
  return copiedLength;
}

template <typename Strategy>
int TextIteratorAlgorithm<Strategy>::copyTextTo(ForwardsTextBuffer* output,
                                                int position) const {
  return copyTextTo(output, position, length() - position);
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::copyCodeUnitsTo(
    ForwardsTextBuffer* output,
    int position,
    int copyLength) const {
  m_textState.appendTextTo(output, position, copyLength);
}

// --------

template <typename Strategy>
static String createPlainText(const EphemeralRangeTemplate<Strategy>& range,
                              TextIteratorBehaviorFlags behavior) {
  if (range.isNull())
    return emptyString();

  DocumentLifecycle::DisallowTransitionScope disallowTransition(
      range.startPosition().document()->lifecycle());

  TextIteratorAlgorithm<Strategy> it(range.startPosition(), range.endPosition(),
                                     behavior);

  if (it.atEnd())
    return emptyString();

  // The initial buffer size can be critical for performance:
  // https://bugs.webkit.org/show_bug.cgi?id=81192
  static const unsigned initialCapacity = 1 << 15;

  StringBuilder builder;
  builder.reserveCapacity(initialCapacity);

  for (; !it.atEnd(); it.advance())
    it.text().appendTextToStringBuilder(builder);

  if (builder.isEmpty())
    return emptyString();

  return builder.toString();
}

String plainText(const EphemeralRange& range,
                 TextIteratorBehaviorFlags behavior) {
  return createPlainText<EditingStrategy>(range, behavior);
}

String plainText(const EphemeralRangeInFlatTree& range,
                 TextIteratorBehaviorFlags behavior) {
  return createPlainText<EditingInFlatTreeStrategy>(range, behavior);
}

template class CORE_TEMPLATE_EXPORT TextIteratorAlgorithm<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT
    TextIteratorAlgorithm<EditingInFlatTreeStrategy>;

}  // namespace blink
