/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009, 2010, 2011 Google Inc. All rights reserved.
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

#include "core/editing/commands/ReplaceSelectionCommand.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/CSSPropertyNames.h"
#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/css/CSSStyleDeclaration.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/Element.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/Text.h"
#include "core/editing/EditingStyle.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/commands/ApplyStyleCommand.h"
#include "core/editing/commands/BreakBlockquoteCommand.h"
#include "core/editing/commands/SimplifyMarkupCommand.h"
#include "core/editing/commands/SmartReplace.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/editing/serializers/HTMLInterchange.h"
#include "core/editing/serializers/Serialization.h"
#include "core/events/BeforeTextInsertedEvent.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLBRElement.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLLIElement.h"
#include "core/html/HTMLQuoteElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/html/HTMLSpanElement.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutText.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Vector.h"

namespace blink {

using namespace HTMLNames;

enum EFragmentType { EmptyFragment, SingleTextNodeFragment, TreeFragment };

// --- ReplacementFragment helper class

class ReplacementFragment final {
  WTF_MAKE_NONCOPYABLE(ReplacementFragment);
  STACK_ALLOCATED();

 public:
  ReplacementFragment(Document*, DocumentFragment*, const VisibleSelection&);

  Node* firstChild() const;
  Node* lastChild() const;

  bool isEmpty() const;

  bool hasInterchangeNewlineAtStart() const {
    return m_hasInterchangeNewlineAtStart;
  }
  bool hasInterchangeNewlineAtEnd() const {
    return m_hasInterchangeNewlineAtEnd;
  }

  void removeNode(Node*);
  void removeNodePreservingChildren(ContainerNode*);

 private:
  HTMLElement* insertFragmentForTestRendering(Element* rootEditableElement);
  void removeUnrenderedNodes(ContainerNode*);
  void restoreAndRemoveTestRenderingNodesToFragment(Element*);
  void removeInterchangeNodes(ContainerNode*);

  void insertNodeBefore(Node*, Node* refNode);

  Member<Document> m_document;
  Member<DocumentFragment> m_fragment;
  bool m_hasInterchangeNewlineAtStart;
  bool m_hasInterchangeNewlineAtEnd;
};

static bool isInterchangeHTMLBRElement(const Node* node) {
  DEFINE_STATIC_LOCAL(String, interchangeNewlineClassString,
                      (AppleInterchangeNewline));
  if (!isHTMLBRElement(node) ||
      toHTMLBRElement(node)->getAttribute(classAttr) !=
          interchangeNewlineClassString)
    return false;
  UseCounter::count(node->document(),
                    UseCounter::EditingAppleInterchangeNewline);
  return true;
}

static bool isHTMLInterchangeConvertedSpaceSpan(const Node* node) {
  DEFINE_STATIC_LOCAL(String, convertedSpaceSpanClassString,
                      (AppleConvertedSpace));
  if (!node->isHTMLElement() ||
      toHTMLElement(node)->getAttribute(classAttr) !=
          convertedSpaceSpanClassString)
    return false;
  UseCounter::count(node->document(), UseCounter::EditingAppleConvertedSpace);
  return true;
}

static Position positionAvoidingPrecedingNodes(Position pos) {
  // If we're already on a break, it's probably a placeholder and we shouldn't
  // change our position.
  if (editingIgnoresContent(*pos.anchorNode()))
    return pos;

  // We also stop when changing block flow elements because even though the
  // visual position is the same.  E.g.,
  //   <div>foo^</div>^
  // The two positions above are the same visual position, but we want to stay
  // in the same block.
  Element* enclosingBlockElement = enclosingBlock(pos.computeContainerNode());
  for (Position nextPosition = pos;
       nextPosition.computeContainerNode() != enclosingBlockElement;
       pos = nextPosition) {
    if (lineBreakExistsAtPosition(pos))
      break;

    if (pos.computeContainerNode()->nonShadowBoundaryParentNode())
      nextPosition = Position::inParentAfterNode(*pos.computeContainerNode());

    if (nextPosition == pos ||
        enclosingBlock(nextPosition.computeContainerNode()) !=
            enclosingBlockElement ||
        createVisiblePosition(pos).deepEquivalent() !=
            createVisiblePosition(nextPosition).deepEquivalent())
      break;
  }
  return pos;
}

ReplacementFragment::ReplacementFragment(Document* document,
                                         DocumentFragment* fragment,
                                         const VisibleSelection& selection)
    : m_document(document),
      m_fragment(fragment),
      m_hasInterchangeNewlineAtStart(false),
      m_hasInterchangeNewlineAtEnd(false) {
  if (!m_document)
    return;
  if (!m_fragment || !m_fragment->hasChildren())
    return;

  TRACE_EVENT0("blink", "ReplacementFragment constructor");
  Element* editableRoot = selection.rootEditableElement();
  DCHECK(editableRoot);
  if (!editableRoot)
    return;

  m_document->updateStyleAndLayoutIgnorePendingStylesheets();

  Element* shadowAncestorElement;
  if (editableRoot->isInShadowTree())
    shadowAncestorElement = editableRoot->ownerShadowHost();
  else
    shadowAncestorElement = editableRoot;

  if (!editableRoot->getAttributeEventListener(
          EventTypeNames::webkitBeforeTextInserted)
      // FIXME: Remove these checks once textareas and textfields actually
      // register an event handler.
      &&
      !(shadowAncestorElement && shadowAncestorElement->layoutObject() &&
        shadowAncestorElement->layoutObject()->isTextControl()) &&
      hasRichlyEditableStyle(*editableRoot)) {
    removeInterchangeNodes(m_fragment.get());
    return;
  }

  if (!hasRichlyEditableStyle(*editableRoot)) {
    bool isPlainText = true;
    for (Node& node : NodeTraversal::childrenOf(*m_fragment)) {
      if (isInterchangeHTMLBRElement(&node) && &node == m_fragment->lastChild())
        continue;
      if (!node.isTextNode()) {
        isPlainText = false;
        break;
      }
    }
    // We don't need TestRendering for plain-text editing + plain-text
    // insertion.
    if (isPlainText) {
      removeInterchangeNodes(m_fragment.get());
      String originalText = m_fragment->textContent();
      BeforeTextInsertedEvent* event =
          BeforeTextInsertedEvent::create(originalText);
      editableRoot->dispatchEvent(event);
      if (originalText != event->text()) {
        m_fragment = createFragmentFromText(
            selection.toNormalizedEphemeralRange(), event->text());
        removeInterchangeNodes(m_fragment.get());
      }
      return;
    }
  }

  HTMLElement* holder = insertFragmentForTestRendering(editableRoot);
  if (!holder) {
    removeInterchangeNodes(m_fragment.get());
    return;
  }

  const EphemeralRange range =
      createVisibleSelection(
          SelectionInDOMTree::Builder().selectAllChildren(*holder).build())
          .toNormalizedEphemeralRange();
  const TextIteratorBehavior& behavior = TextIteratorBehavior::Builder()
                                             .setEmitsOriginalText(true)
                                             .setIgnoresStyleVisibility(true)
                                             .build();
  const String& text = plainText(range, behavior);

  removeInterchangeNodes(holder);
  removeUnrenderedNodes(holder);
  restoreAndRemoveTestRenderingNodesToFragment(holder);

  // Give the root a chance to change the text.
  BeforeTextInsertedEvent* evt = BeforeTextInsertedEvent::create(text);
  editableRoot->dispatchEvent(evt);
  if (text != evt->text() || !hasRichlyEditableStyle(*editableRoot)) {
    restoreAndRemoveTestRenderingNodesToFragment(holder);

    // TODO(editing-dev): Use of updateStyleAndLayoutIgnorePendingStylesheets
    // needs to be audited.  See http://crbug.com/590369 for more details.
    document->updateStyleAndLayoutIgnorePendingStylesheets();

    m_fragment = createFragmentFromText(selection.toNormalizedEphemeralRange(),
                                        evt->text());
    if (!m_fragment->hasChildren())
      return;

    holder = insertFragmentForTestRendering(editableRoot);
    removeInterchangeNodes(holder);
    removeUnrenderedNodes(holder);
    restoreAndRemoveTestRenderingNodesToFragment(holder);
  }
}

bool ReplacementFragment::isEmpty() const {
  return (!m_fragment || !m_fragment->hasChildren()) &&
         !m_hasInterchangeNewlineAtStart && !m_hasInterchangeNewlineAtEnd;
}

Node* ReplacementFragment::firstChild() const {
  return m_fragment ? m_fragment->firstChild() : 0;
}

Node* ReplacementFragment::lastChild() const {
  return m_fragment ? m_fragment->lastChild() : 0;
}

void ReplacementFragment::removeNodePreservingChildren(ContainerNode* node) {
  if (!node)
    return;

  while (Node* n = node->firstChild()) {
    removeNode(n);
    insertNodeBefore(n, node);
  }
  removeNode(node);
}

void ReplacementFragment::removeNode(Node* node) {
  if (!node)
    return;

  ContainerNode* parent = node->nonShadowBoundaryParentNode();
  if (!parent)
    return;

  parent->removeChild(node);
}

void ReplacementFragment::insertNodeBefore(Node* node, Node* refNode) {
  if (!node || !refNode)
    return;

  ContainerNode* parent = refNode->nonShadowBoundaryParentNode();
  if (!parent)
    return;

  parent->insertBefore(node, refNode);
}

HTMLElement* ReplacementFragment::insertFragmentForTestRendering(
    Element* rootEditableElement) {
  TRACE_EVENT0("blink", "ReplacementFragment::insertFragmentForTestRendering");
  DCHECK(m_document);
  HTMLElement* holder = createDefaultParagraphElement(*m_document.get());

  holder->appendChild(m_fragment);
  rootEditableElement->appendChild(holder);

  // TODO(editing-dev): Hoist this call to the call sites.
  m_document->updateStyleAndLayoutIgnorePendingStylesheets();

  return holder;
}

void ReplacementFragment::restoreAndRemoveTestRenderingNodesToFragment(
    Element* holder) {
  if (!holder)
    return;

  while (Node* node = holder->firstChild()) {
    holder->removeChild(node);
    m_fragment->appendChild(node);
  }

  removeNode(holder);
}

void ReplacementFragment::removeUnrenderedNodes(ContainerNode* holder) {
  HeapVector<Member<Node>> unrendered;

  for (Node& node : NodeTraversal::descendantsOf(*holder)) {
    if (!isNodeRendered(node) && !isTableStructureNode(&node))
      unrendered.push_back(&node);
  }

  for (auto& node : unrendered)
    removeNode(node);
}

void ReplacementFragment::removeInterchangeNodes(ContainerNode* container) {
  m_hasInterchangeNewlineAtStart = false;
  m_hasInterchangeNewlineAtEnd = false;

  // Interchange newlines at the "start" of the incoming fragment must be
  // either the first node in the fragment or the first leaf in the fragment.
  Node* node = container->firstChild();
  while (node) {
    if (isInterchangeHTMLBRElement(node)) {
      m_hasInterchangeNewlineAtStart = true;
      removeNode(node);
      break;
    }
    node = node->firstChild();
  }
  if (!container->hasChildren())
    return;
  // Interchange newlines at the "end" of the incoming fragment must be
  // either the last node in the fragment or the last leaf in the fragment.
  node = container->lastChild();
  while (node) {
    if (isInterchangeHTMLBRElement(node)) {
      m_hasInterchangeNewlineAtEnd = true;
      removeNode(node);
      break;
    }
    node = node->lastChild();
  }

  node = container->firstChild();
  while (node) {
    Node* next = NodeTraversal::next(*node);
    if (isHTMLInterchangeConvertedSpaceSpan(node)) {
      HTMLElement& element = toHTMLElement(*node);
      next = NodeTraversal::nextSkippingChildren(element);
      removeNodePreservingChildren(&element);
    }
    node = next;
  }
}

inline void ReplaceSelectionCommand::InsertedNodes::respondToNodeInsertion(
    Node& node) {
  if (!m_firstNodeInserted)
    m_firstNodeInserted = &node;

  m_lastNodeInserted = &node;
}

inline void
ReplaceSelectionCommand::InsertedNodes::willRemoveNodePreservingChildren(
    Node& node) {
  if (m_firstNodeInserted.get() == node)
    m_firstNodeInserted = NodeTraversal::next(node);
  if (m_lastNodeInserted.get() == node)
    m_lastNodeInserted = node.lastChild()
                             ? node.lastChild()
                             : NodeTraversal::nextSkippingChildren(node);
  if (m_refNode.get() == node)
    m_refNode = NodeTraversal::next(node);
}

inline void ReplaceSelectionCommand::InsertedNodes::willRemoveNode(Node& node) {
  if (m_firstNodeInserted.get() == node && m_lastNodeInserted.get() == node) {
    m_firstNodeInserted = nullptr;
    m_lastNodeInserted = nullptr;
  } else if (m_firstNodeInserted.get() == node) {
    m_firstNodeInserted =
        NodeTraversal::nextSkippingChildren(*m_firstNodeInserted);
  } else if (m_lastNodeInserted.get() == node) {
    m_lastNodeInserted =
        NodeTraversal::previousSkippingChildren(*m_lastNodeInserted);
  }
  if (node.contains(m_refNode))
    m_refNode = NodeTraversal::nextSkippingChildren(node);
}

inline void ReplaceSelectionCommand::InsertedNodes::didReplaceNode(
    Node& node,
    Node& newNode) {
  if (m_firstNodeInserted.get() == node)
    m_firstNodeInserted = &newNode;
  if (m_lastNodeInserted.get() == node)
    m_lastNodeInserted = &newNode;
  if (m_refNode.get() == node)
    m_refNode = &newNode;
}

ReplaceSelectionCommand::ReplaceSelectionCommand(
    Document& document,
    DocumentFragment* fragment,
    CommandOptions options,
    InputEvent::InputType inputType)
    : CompositeEditCommand(document),
      m_selectReplacement(options & SelectReplacement),
      m_smartReplace(options & SmartReplace),
      m_matchStyle(options & MatchStyle),
      m_documentFragment(fragment),
      m_preventNesting(options & PreventNesting),
      m_movingParagraph(options & MovingParagraph),
      m_inputType(inputType),
      m_sanitizeFragment(options & SanitizeFragment),
      m_shouldMergeEnd(false) {}

static bool hasMatchingQuoteLevel(VisiblePosition endOfExistingContent,
                                  VisiblePosition endOfInsertedContent) {
  Position existing = endOfExistingContent.deepEquivalent();
  Position inserted = endOfInsertedContent.deepEquivalent();
  bool isInsideMailBlockquote = enclosingNodeOfType(
      inserted, isMailHTMLBlockquoteElement, CanCrossEditingBoundary);
  return isInsideMailBlockquote && (numEnclosingMailBlockquotes(existing) ==
                                    numEnclosingMailBlockquotes(inserted));
}

bool ReplaceSelectionCommand::shouldMergeStart(
    bool selectionStartWasStartOfParagraph,
    bool fragmentHasInterchangeNewlineAtStart,
    bool selectionStartWasInsideMailBlockquote) {
  if (m_movingParagraph)
    return false;

  VisiblePosition startOfInsertedContent = positionAtStartOfInsertedContent();
  VisiblePosition prev =
      previousPositionOf(startOfInsertedContent, CannotCrossEditingBoundary);
  if (prev.isNull())
    return false;

  // When we have matching quote levels, its ok to merge more frequently.
  // For a successful merge, we still need to make sure that the inserted
  // content starts with the beginning of a paragraph. And we should only merge
  // here if the selection start was inside a mail blockquote. This prevents
  // against removing a blockquote from newly pasted quoted content that was
  // pasted into an unquoted position. If that unquoted position happens to be
  // right after another blockquote, we don't want to merge and risk stripping a
  // valid block (and newline) from the pasted content.
  if (isStartOfParagraph(startOfInsertedContent) &&
      selectionStartWasInsideMailBlockquote &&
      hasMatchingQuoteLevel(prev, positionAtEndOfInsertedContent()))
    return true;

  return !selectionStartWasStartOfParagraph &&
         !fragmentHasInterchangeNewlineAtStart &&
         isStartOfParagraph(startOfInsertedContent) &&
         !isHTMLBRElement(
             *startOfInsertedContent.deepEquivalent().anchorNode()) &&
         shouldMerge(startOfInsertedContent, prev);
}

bool ReplaceSelectionCommand::shouldMergeEnd(
    bool selectionEndWasEndOfParagraph) {
  VisiblePosition endOfInsertedContent(positionAtEndOfInsertedContent());
  VisiblePosition next =
      nextPositionOf(endOfInsertedContent, CannotCrossEditingBoundary);
  if (next.isNull())
    return false;

  return !selectionEndWasEndOfParagraph &&
         isEndOfParagraph(endOfInsertedContent) &&
         !isHTMLBRElement(
             *endOfInsertedContent.deepEquivalent().anchorNode()) &&
         shouldMerge(endOfInsertedContent, next);
}

static bool isHTMLHeaderElement(const Node* a) {
  if (!a || !a->isHTMLElement())
    return false;

  const HTMLElement& element = toHTMLElement(*a);
  return element.hasTagName(h1Tag) || element.hasTagName(h2Tag) ||
         element.hasTagName(h3Tag) || element.hasTagName(h4Tag) ||
         element.hasTagName(h5Tag) || element.hasTagName(h6Tag);
}

static bool haveSameTagName(Element* a, Element* b) {
  return a && b && a->tagName() == b->tagName();
}

bool ReplaceSelectionCommand::shouldMerge(const VisiblePosition& source,
                                          const VisiblePosition& destination) {
  if (source.isNull() || destination.isNull())
    return false;

  Node* sourceNode = source.deepEquivalent().anchorNode();
  Node* destinationNode = destination.deepEquivalent().anchorNode();
  Element* sourceBlock = enclosingBlock(sourceNode);
  Element* destinationBlock = enclosingBlock(destinationNode);
  return sourceBlock && (!sourceBlock->hasTagName(blockquoteTag) ||
                         isMailHTMLBlockquoteElement(sourceBlock)) &&
         enclosingListChild(sourceBlock) ==
             enclosingListChild(destinationNode) &&
         enclosingTableCell(source.deepEquivalent()) ==
             enclosingTableCell(destination.deepEquivalent()) &&
         (!isHTMLHeaderElement(sourceBlock) ||
          haveSameTagName(sourceBlock, destinationBlock))
         // Don't merge to or from a position before or after a block because it
         // would be a no-op and cause infinite recursion.
         && !isEnclosingBlock(sourceNode) && !isEnclosingBlock(destinationNode);
}

// Style rules that match just inserted elements could change their appearance,
// like a div inserted into a document with div { display:inline; }.
void ReplaceSelectionCommand::removeRedundantStylesAndKeepStyleSpanInline(
    InsertedNodes& insertedNodes,
    EditingState* editingState) {
  Node* pastEndNode = insertedNodes.pastLastLeaf();
  Node* next = nullptr;
  for (Node* node = insertedNodes.firstNodeInserted();
       node && node != pastEndNode; node = next) {
    // FIXME: <rdar://problem/5371536> Style rules that match pasted content can
    // change it's appearance

    next = NodeTraversal::next(*node);
    if (!node->isStyledElement())
      continue;

    Element* element = toElement(node);

    const StylePropertySet* inlineStyle = element->inlineStyle();
    EditingStyle* newInlineStyle = EditingStyle::create(inlineStyle);
    if (inlineStyle) {
      if (element->isHTMLElement()) {
        Vector<QualifiedName> attributes;
        HTMLElement* htmlElement = toHTMLElement(element);
        DCHECK(htmlElement);

        if (newInlineStyle->conflictsWithImplicitStyleOfElement(htmlElement)) {
          // e.g. <b style="font-weight: normal;"> is converted to <span
          // style="font-weight: normal;">
          element = replaceElementWithSpanPreservingChildrenAndAttributes(
              htmlElement);
          inlineStyle = element->inlineStyle();
          insertedNodes.didReplaceNode(*htmlElement, *element);
        } else if (newInlineStyle->extractConflictingImplicitStyleOfAttributes(
                       htmlElement, EditingStyle::PreserveWritingDirection, 0,
                       attributes, EditingStyle::DoNotExtractMatchingStyle)) {
          // e.g. <font size="3" style="font-size: 20px;"> is converted to <font
          // style="font-size: 20px;">
          for (size_t i = 0; i < attributes.size(); i++)
            removeElementAttribute(htmlElement, attributes[i]);
        }
      }

      ContainerNode* context = element->parentNode();

      // If Mail wraps the fragment with a Paste as Quotation blockquote, or if
      // you're pasting into a quoted region, styles from blockquoteNode are
      // allowed to override those from the source document, see
      // <rdar://problem/4930986> and <rdar://problem/5089327>.
      HTMLQuoteElement* blockquoteElement =
          !context ? toHTMLQuoteElement(context)
                   : toHTMLQuoteElement(enclosingNodeOfType(
                         Position::firstPositionInNode(context),
                         isMailHTMLBlockquoteElement, CanCrossEditingBoundary));

      // EditingStyle::removeStyleFromRulesAndContext() uses StyleResolver,
      // which requires clean style.
      // TODO(editing-dev): There is currently no way to update style without
      // updating layout. We might want to have updateLifcycleToStyleClean()
      // similar to FrameView::updateLifecylceToLayoutClean() in Document.
      document().updateStyleAndLayoutIgnorePendingStylesheets();

      if (blockquoteElement)
        newInlineStyle->removeStyleFromRulesAndContext(
            element, document().documentElement());

      newInlineStyle->removeStyleFromRulesAndContext(element, context);
    }

    if (!inlineStyle || newInlineStyle->isEmpty()) {
      if (isStyleSpanOrSpanWithOnlyStyleAttribute(element) ||
          isEmptyFontTag(element, AllowNonEmptyStyleAttribute)) {
        insertedNodes.willRemoveNodePreservingChildren(*element);
        removeNodePreservingChildren(element, editingState);
        if (editingState->isAborted())
          return;
        continue;
      }
      removeElementAttribute(element, styleAttr);
    } else if (newInlineStyle->style()->propertyCount() !=
               inlineStyle->propertyCount()) {
      setNodeAttribute(element, styleAttr,
                       AtomicString(newInlineStyle->style()->asText()));
    }

    document().updateStyleAndLayoutIgnorePendingStylesheets();

    // FIXME: Tolerate differences in id, class, and style attributes.
    if (element->parentNode() && isNonTableCellHTMLBlockElement(element) &&
        areIdenticalElements(*element, *element->parentNode()) &&
        VisiblePosition::firstPositionInNode(element->parentNode())
                .deepEquivalent() ==
            VisiblePosition::firstPositionInNode(element).deepEquivalent() &&
        VisiblePosition::lastPositionInNode(element->parentNode())
                .deepEquivalent() ==
            VisiblePosition::lastPositionInNode(element).deepEquivalent()) {
      insertedNodes.willRemoveNodePreservingChildren(*element);
      removeNodePreservingChildren(element, editingState);
      if (editingState->isAborted())
        return;
      continue;
    }

    if (element->parentNode() &&
        hasRichlyEditableStyle(*element->parentNode()) &&
        hasRichlyEditableStyle(*element)) {
      removeElementAttribute(element, contenteditableAttr);
    }
  }
}

static bool isProhibitedParagraphChild(const AtomicString& name) {
  // https://dvcs.w3.org/hg/editing/raw-file/57abe6d3cb60/editing.html#prohibited-paragraph-child
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, elements,
                      ({
                          addressTag.localName(),   articleTag.localName(),
                          asideTag.localName(),     blockquoteTag.localName(),
                          captionTag.localName(),   centerTag.localName(),
                          colTag.localName(),       colgroupTag.localName(),
                          ddTag.localName(),        detailsTag.localName(),
                          dirTag.localName(),       divTag.localName(),
                          dlTag.localName(),        dtTag.localName(),
                          fieldsetTag.localName(),  figcaptionTag.localName(),
                          figureTag.localName(),    footerTag.localName(),
                          formTag.localName(),      h1Tag.localName(),
                          h2Tag.localName(),        h3Tag.localName(),
                          h4Tag.localName(),        h5Tag.localName(),
                          h6Tag.localName(),        headerTag.localName(),
                          hgroupTag.localName(),    hrTag.localName(),
                          liTag.localName(),        listingTag.localName(),
                          mainTag.localName(),  // Missing in the specification.
                          menuTag.localName(),      navTag.localName(),
                          olTag.localName(),        pTag.localName(),
                          plaintextTag.localName(), preTag.localName(),
                          sectionTag.localName(),   summaryTag.localName(),
                          tableTag.localName(),     tbodyTag.localName(),
                          tdTag.localName(),        tfootTag.localName(),
                          thTag.localName(),        theadTag.localName(),
                          trTag.localName(),        ulTag.localName(),
                          xmpTag.localName(),
                      }));
  return elements.contains(name);
}

void ReplaceSelectionCommand::
    makeInsertedContentRoundTrippableWithHTMLTreeBuilder(
        const InsertedNodes& insertedNodes,
        EditingState* editingState) {
  Node* pastEndNode = insertedNodes.pastLastLeaf();
  Node* next = nullptr;
  for (Node* node = insertedNodes.firstNodeInserted();
       node && node != pastEndNode; node = next) {
    next = NodeTraversal::next(*node);

    if (!node->isHTMLElement())
      continue;
    // moveElementOutOfAncestor() in a previous iteration might have failed,
    // and |node| might have been detached from the document tree.
    if (!node->isConnected())
      continue;

    HTMLElement& element = toHTMLElement(*node);
    if (isProhibitedParagraphChild(element.localName())) {
      if (HTMLElement* paragraphElement = toHTMLElement(enclosingElementWithTag(
              Position::inParentBeforeNode(element), pTag))) {
        moveElementOutOfAncestor(&element, paragraphElement, editingState);
        if (editingState->isAborted())
          return;
      }
    }

    if (isHTMLHeaderElement(&element)) {
      if (HTMLElement* headerElement = toHTMLElement(highestEnclosingNodeOfType(
              Position::inParentBeforeNode(element), isHTMLHeaderElement))) {
        moveElementOutOfAncestor(&element, headerElement, editingState);
        if (editingState->isAborted())
          return;
      }
    }
  }
}

void ReplaceSelectionCommand::moveElementOutOfAncestor(
    Element* element,
    Element* ancestor,
    EditingState* editingState) {
  if (!hasEditableStyle(*ancestor->parentNode()))
    return;

  document().updateStyleAndLayoutIgnorePendingStylesheets();
  VisiblePosition positionAtEndOfNode =
      createVisiblePosition(lastPositionInOrAfterNode(element));
  VisiblePosition lastPositionInParagraph =
      VisiblePosition::lastPositionInNode(ancestor);
  if (positionAtEndOfNode.deepEquivalent() ==
      lastPositionInParagraph.deepEquivalent()) {
    removeNode(element, editingState);
    if (editingState->isAborted())
      return;
    if (ancestor->nextSibling())
      insertNodeBefore(element, ancestor->nextSibling(), editingState);
    else
      appendNode(element, ancestor->parentNode(), editingState);
    if (editingState->isAborted())
      return;
  } else {
    Node* nodeToSplitTo = splitTreeToNode(element, ancestor, true);
    removeNode(element, editingState);
    if (editingState->isAborted())
      return;
    insertNodeBefore(element, nodeToSplitTo, editingState);
    if (editingState->isAborted())
      return;
  }
  if (!ancestor->hasChildren())
    removeNode(ancestor, editingState);
}

static inline bool nodeHasVisibleLayoutText(Text& text) {
  return text.layoutObject() && text.layoutObject()->resolvedTextLength() > 0;
}

void ReplaceSelectionCommand::removeUnrenderedTextNodesAtEnds(
    InsertedNodes& insertedNodes) {
  document().updateStyleAndLayoutIgnorePendingStylesheets();

  Node* lastLeafInserted = insertedNodes.lastLeafInserted();
  if (lastLeafInserted && lastLeafInserted->isTextNode() &&
      !nodeHasVisibleLayoutText(toText(*lastLeafInserted)) &&
      !enclosingElementWithTag(firstPositionInOrBeforeNode(lastLeafInserted),
                               selectTag) &&
      !enclosingElementWithTag(firstPositionInOrBeforeNode(lastLeafInserted),
                               scriptTag)) {
    insertedNodes.willRemoveNode(*lastLeafInserted);
    // Removing a Text node won't dispatch synchronous events.
    removeNode(lastLeafInserted, ASSERT_NO_EDITING_ABORT);
  }

  // We don't have to make sure that firstNodeInserted isn't inside a select or
  // script element, because it is a top level node in the fragment and the user
  // can't insert into those elements.
  Node* firstNodeInserted = insertedNodes.firstNodeInserted();
  if (firstNodeInserted && firstNodeInserted->isTextNode() &&
      !nodeHasVisibleLayoutText(toText(*firstNodeInserted))) {
    insertedNodes.willRemoveNode(*firstNodeInserted);
    // Removing a Text node won't dispatch synchronous events.
    removeNode(firstNodeInserted, ASSERT_NO_EDITING_ABORT);
  }
}

VisiblePosition ReplaceSelectionCommand::positionAtEndOfInsertedContent()
    const {
  // TODO(xiaochengh): Hoist the call and change it into a DCHECK.
  document().updateStyleAndLayoutIgnorePendingStylesheets();
  // TODO(yosin): We should set |m_endOfInsertedContent| not in SELECT
  // element, since contents of SELECT elements, e.g. OPTION, OPTGROUP, are
  // not editable, or SELECT element is an atomic on editing.
  HTMLSelectElement* enclosingSelect = toHTMLSelectElement(
      enclosingElementWithTag(m_endOfInsertedContent, selectTag));
  if (enclosingSelect)
    return createVisiblePosition(lastPositionInOrAfterNode(enclosingSelect));
  if (m_endOfInsertedContent.isOrphan())
    return VisiblePosition();
  return createVisiblePosition(m_endOfInsertedContent);
}

VisiblePosition ReplaceSelectionCommand::positionAtStartOfInsertedContent()
    const {
  // TODO(xiaochengh): Hoist the call and change it into a DCHECK.
  document().updateStyleAndLayoutIgnorePendingStylesheets();
  if (m_startOfInsertedContent.isOrphan())
    return VisiblePosition();
  return createVisiblePosition(m_startOfInsertedContent);
}

static void removeHeadContents(ReplacementFragment& fragment) {
  Node* next = nullptr;
  for (Node* node = fragment.firstChild(); node; node = next) {
    if (isHTMLBaseElement(*node) || isHTMLLinkElement(*node) ||
        isHTMLMetaElement(*node) || isHTMLTitleElement(*node)) {
      next = NodeTraversal::nextSkippingChildren(*node);
      fragment.removeNode(node);
    } else {
      next = NodeTraversal::next(*node);
    }
  }
}

static bool followBlockElementStyle(const Node* node) {
  if (!node->isHTMLElement())
    return false;

  const HTMLElement& element = toHTMLElement(*node);
  return isListItem(node) || isTableCell(node) || element.hasTagName(preTag) ||
         element.hasTagName(h1Tag) || element.hasTagName(h2Tag) ||
         element.hasTagName(h3Tag) || element.hasTagName(h4Tag) ||
         element.hasTagName(h5Tag) || element.hasTagName(h6Tag);
}

// Remove style spans before insertion if they are unnecessary.  It's faster
// because we'll avoid doing a layout.
static void handleStyleSpansBeforeInsertion(ReplacementFragment& fragment,
                                            const Position& insertionPos) {
  Node* topNode = fragment.firstChild();
  if (!isHTMLSpanElement(topNode))
    return;

  // Handling the case where we are doing Paste as Quotation or pasting into
  // quoted content is more complicated (see handleStyleSpans) and doesn't
  // receive the optimization.
  if (enclosingNodeOfType(firstPositionInOrBeforeNode(topNode),
                          isMailHTMLBlockquoteElement, CanCrossEditingBoundary))
    return;

  // Remove style spans to follow the styles of parent block element when
  // |fragment| becomes a part of it. See bugs http://crbug.com/226941 and
  // http://crbug.com/335955.
  HTMLSpanElement* wrappingStyleSpan = toHTMLSpanElement(topNode);
  const Node* node = insertionPos.anchorNode();
  // |node| can be an inline element like <br> under <li>
  // e.g.) editing/execCommand/switch-list-type.html
  //       editing/deleting/backspace-merge-into-block.html
  if (isInline(node)) {
    node = enclosingBlock(insertionPos.anchorNode());
    if (!node)
      return;
  }

  if (followBlockElementStyle(node)) {
    fragment.removeNodePreservingChildren(wrappingStyleSpan);
    return;
  }

  EditingStyle* styleAtInsertionPos =
      EditingStyle::create(insertionPos.parentAnchoredEquivalent());
  String styleText = styleAtInsertionPos->style()->asText();

  // FIXME: This string comparison is a naive way of comparing two styles.
  // We should be taking the diff and check that the diff is empty.
  if (styleText != wrappingStyleSpan->getAttribute(styleAttr))
    return;

  fragment.removeNodePreservingChildren(wrappingStyleSpan);
}

void ReplaceSelectionCommand::mergeEndIfNeeded(EditingState* editingState) {
  if (!m_shouldMergeEnd)
    return;

  VisiblePosition startOfInsertedContent(positionAtStartOfInsertedContent());
  VisiblePosition endOfInsertedContent(positionAtEndOfInsertedContent());

  // Bail to avoid infinite recursion.
  if (m_movingParagraph) {
    NOTREACHED();
    return;
  }

  // Merging two paragraphs will destroy the moved one's block styles.  Always
  // move the end of inserted forward to preserve the block style of the
  // paragraph already in the document, unless the paragraph to move would
  // include the what was the start of the selection that was pasted into, so
  // that we preserve that paragraph's block styles.
  bool mergeForward =
      !(inSameParagraph(startOfInsertedContent, endOfInsertedContent) &&
        !isStartOfParagraph(startOfInsertedContent));

  VisiblePosition destination = mergeForward
                                    ? nextPositionOf(endOfInsertedContent)
                                    : endOfInsertedContent;
  // TODO(xiaochengh): Stop storing VisiblePositions through mutations.
  VisiblePosition startOfParagraphToMove =
      mergeForward ? startOfParagraph(endOfInsertedContent)
                   : nextPositionOf(endOfInsertedContent);

  // Merging forward could result in deleting the destination anchor node.
  // To avoid this, we add a placeholder node before the start of the paragraph.
  if (endOfParagraph(startOfParagraphToMove).deepEquivalent() ==
      destination.deepEquivalent()) {
    HTMLBRElement* placeholder = HTMLBRElement::create(document());
    insertNodeBefore(placeholder,
                     startOfParagraphToMove.deepEquivalent().anchorNode(),
                     editingState);
    if (editingState->isAborted())
      return;

    // TODO(editing-dev): Use of updateStyleAndLayoutIgnorePendingStylesheets()
    // needs to be audited.  See http://crbug.com/590369 for more details.
    document().updateStyleAndLayoutIgnorePendingStylesheets();

    destination = VisiblePosition::beforeNode(placeholder);
    startOfParagraphToMove =
        createVisiblePosition(startOfParagraphToMove.toPositionWithAffinity());
  }

  moveParagraph(startOfParagraphToMove, endOfParagraph(startOfParagraphToMove),
                destination, editingState);
  if (editingState->isAborted())
    return;

  document().updateStyleAndLayoutIgnorePendingStylesheets();

  // Merging forward will remove m_endOfInsertedContent from the document.
  if (mergeForward) {
    if (m_startOfInsertedContent.isOrphan()) {
      m_startOfInsertedContent =
          endingSelection().visibleStart().deepEquivalent();
    }
    m_endOfInsertedContent = endingSelection().visibleEnd().deepEquivalent();
    // If we merged text nodes, m_endOfInsertedContent could be null. If
    // this is the case, we use m_startOfInsertedContent.
    if (m_endOfInsertedContent.isNull())
      m_endOfInsertedContent = m_startOfInsertedContent;
  }
}

static Node* enclosingInline(Node* node) {
  while (ContainerNode* parent = node->parentNode()) {
    if (isBlockFlowElement(*parent) || isHTMLBodyElement(*parent))
      return node;
    // Stop if any previous sibling is a block.
    for (Node* sibling = node->previousSibling(); sibling;
         sibling = sibling->previousSibling()) {
      if (isBlockFlowElement(*sibling))
        return node;
    }
    node = parent;
  }
  return node;
}

static bool isInlineHTMLElementWithStyle(const Node* node) {
  // We don't want to skip over any block elements.
  if (isEnclosingBlock(node))
    return false;

  if (!node->isHTMLElement())
    return false;

  // We can skip over elements whose class attribute is
  // one of our internal classes.
  const HTMLElement* element = toHTMLElement(node);
  const AtomicString& classAttributeValue = element->getAttribute(classAttr);
  if (classAttributeValue == AppleTabSpanClass) {
    UseCounter::count(element->document(),
                      UseCounter::EditingAppleTabSpanClass);
    return true;
  }
  if (classAttributeValue == AppleConvertedSpace) {
    UseCounter::count(element->document(),
                      UseCounter::EditingAppleConvertedSpace);
    return true;
  }

  return EditingStyle::elementIsStyledSpanOrHTMLEquivalent(element);
}

static inline HTMLElement*
elementToSplitToAvoidPastingIntoInlineElementsWithStyle(
    const Position& insertionPos) {
  Element* containingBlock =
      enclosingBlock(insertionPos.computeContainerNode());
  return toHTMLElement(
      highestEnclosingNodeOfType(insertionPos, isInlineHTMLElementWithStyle,
                                 CannotCrossEditingBoundary, containingBlock));
}

void ReplaceSelectionCommand::doApply(EditingState* editingState) {
  TRACE_EVENT0("blink", "ReplaceSelectionCommand::doApply");
  const VisibleSelection selection = endingSelection();
  DCHECK(!selection.isNone());
  DCHECK(selection.start().anchorNode());
  if (!selection.isNonOrphanedCaretOrRange() || !selection.start().anchorNode())
    return;

  if (!selection.rootEditableElement())
    return;

  ReplacementFragment fragment(&document(), m_documentFragment.get(),
                               selection);
  bool trivialReplaceResult = performTrivialReplace(fragment, editingState);
  if (editingState->isAborted())
    return;
  if (trivialReplaceResult)
    return;

  document().updateStyleAndLayoutIgnorePendingStylesheets();

  // We can skip matching the style if the selection is plain text.
  if ((selection.start().anchorNode()->layoutObject() &&
       selection.start().anchorNode()->layoutObject()->style()->userModify() ==
           READ_WRITE_PLAINTEXT_ONLY) &&
      (selection.end().anchorNode()->layoutObject() &&
       selection.end().anchorNode()->layoutObject()->style()->userModify() ==
           READ_WRITE_PLAINTEXT_ONLY))
    m_matchStyle = false;

  if (m_matchStyle) {
    m_insertionStyle = EditingStyle::create(selection.start());
    m_insertionStyle->mergeTypingStyle(&document());
  }

  const VisiblePosition visibleStart = selection.visibleStart();
  const VisiblePosition visibleEnd = selection.visibleEnd();

  const bool selectionEndWasEndOfParagraph = isEndOfParagraph(visibleEnd);
  const bool selectionStartWasStartOfParagraph =
      isStartOfParagraph(visibleStart);

  Element* enclosingBlockOfVisibleStart =
      enclosingBlock(visibleStart.deepEquivalent().anchorNode());

  const bool startIsInsideMailBlockquote = enclosingNodeOfType(
      selection.start(), isMailHTMLBlockquoteElement, CanCrossEditingBoundary);
  const bool selectionIsPlainText = !selection.isContentRichlyEditable();
  Element* currentRoot = selection.rootEditableElement();

  if ((selectionStartWasStartOfParagraph && selectionEndWasEndOfParagraph &&
       !startIsInsideMailBlockquote) ||
      enclosingBlockOfVisibleStart == currentRoot ||
      isListItem(enclosingBlockOfVisibleStart) || selectionIsPlainText) {
    m_preventNesting = false;
  }

  if (selection.isRange()) {
    // When the end of the selection being pasted into is at the end of a
    // paragraph, and that selection spans multiple blocks, not merging may
    // leave an empty line.
    // When the start of the selection being pasted into is at the start of a
    // block, not merging will leave hanging block(s).
    // Merge blocks if the start of the selection was in a Mail blockquote,
    // since we handle that case specially to prevent nesting.
    bool mergeBlocksAfterDelete = startIsInsideMailBlockquote ||
                                  isEndOfParagraph(visibleEnd) ||
                                  isStartOfBlock(visibleStart);
    // FIXME: We should only expand to include fully selected special elements
    // if we are copying a selection and pasting it on top of itself.
    deleteSelection(editingState, false, mergeBlocksAfterDelete, false);
    if (editingState->isAborted())
      return;
    if (fragment.hasInterchangeNewlineAtStart()) {
      document().updateStyleAndLayoutIgnorePendingStylesheets();
      VisiblePosition startAfterDelete = endingSelection().visibleStart();
      if (isEndOfParagraph(startAfterDelete) &&
          !isStartOfParagraph(startAfterDelete) &&
          !isEndOfEditableOrNonEditableContent(startAfterDelete)) {
        setEndingSelection(
            SelectionInDOMTree::Builder()
                .collapse(nextPositionOf(startAfterDelete).deepEquivalent())
                .build());
      } else {
        insertParagraphSeparator(editingState);
      }
      if (editingState->isAborted())
        return;
    }
  } else {
    DCHECK(selection.isCaret());
    if (fragment.hasInterchangeNewlineAtStart()) {
      const VisiblePosition next =
          nextPositionOf(visibleStart, CannotCrossEditingBoundary);
      if (isEndOfParagraph(visibleStart) && !isStartOfParagraph(visibleStart) &&
          next.isNotNull()) {
        setEndingSelection(SelectionInDOMTree::Builder()
                               .collapse(next.deepEquivalent())
                               .build());
      } else {
        insertParagraphSeparator(editingState);
        if (editingState->isAborted())
          return;
        document().updateStyleAndLayoutIgnorePendingStylesheets();
      }
    }
    // We split the current paragraph in two to avoid nesting the blocks from
    // the fragment inside the current block.
    //
    // For example, paste
    //   <div>foo</div><div>bar</div><div>baz</div>
    // into
    //   <div>x^x</div>
    // where ^ is the caret.
    //
    // As long as the div styles are the same, visually you'd expect:
    //   <div>xbar</div><div>bar</div><div>bazx</div>
    // not
    //   <div>xbar<div>bar</div><div>bazx</div></div>
    // Don't do this if the selection started in a Mail blockquote.
    if (m_preventNesting && !startIsInsideMailBlockquote &&
        !isEndOfParagraph(endingSelection().visibleStart()) &&
        !isStartOfParagraph(endingSelection().visibleStart())) {
      insertParagraphSeparator(editingState);
      if (editingState->isAborted())
        return;
      document().updateStyleAndLayoutIgnorePendingStylesheets();
      setEndingSelection(
          SelectionInDOMTree::Builder()
              .collapse(previousPositionOf(endingSelection().visibleStart())
                            .deepEquivalent())
              .build());
    }
  }

  Position insertionPos = endingSelection().start();
  // We don't want any of the pasted content to end up nested in a Mail
  // blockquote, so first break out of any surrounding Mail blockquotes. Unless
  // we're inserting in a table, in which case breaking the blockquote will
  // prevent the content from actually being inserted in the table.
  if (enclosingNodeOfType(insertionPos, isMailHTMLBlockquoteElement,
                          CanCrossEditingBoundary) &&
      m_preventNesting &&
      !(enclosingNodeOfType(insertionPos, &isTableStructureNode))) {
    applyCommandToComposite(BreakBlockquoteCommand::create(document()),
                            editingState);
    if (editingState->isAborted())
      return;
    // This will leave a br between the split.
    Node* br = endingSelection().start().anchorNode();
    DCHECK(isHTMLBRElement(br)) << br;
    // Insert content between the two blockquotes, but remove the br (since it
    // was just a placeholder).
    insertionPos = Position::inParentBeforeNode(*br);
    removeNode(br, editingState);
    if (editingState->isAborted())
      return;
  }

  // Inserting content could cause whitespace to collapse, e.g. inserting
  // <div>foo</div> into hello^ world.
  prepareWhitespaceAtPositionForSplit(insertionPos);

  document().updateStyleAndLayoutIgnorePendingStylesheets();

  // If the downstream node has been removed there's no point in continuing.
  if (!mostForwardCaretPosition(insertionPos).anchorNode())
    return;

  // NOTE: This would be an incorrect usage of downstream() if downstream() were
  // changed to mean the last position after p that maps to the same visible
  // position as p (since in the case where a br is at the end of a block and
  // collapsed away, there are positions after the br which map to the same
  // visible position as [br, 0]).
  HTMLBRElement* endBR =
      isHTMLBRElement(*mostForwardCaretPosition(insertionPos).anchorNode())
          ? toHTMLBRElement(mostForwardCaretPosition(insertionPos).anchorNode())
          : 0;
  VisiblePosition originalVisPosBeforeEndBR;
  if (endBR)
    originalVisPosBeforeEndBR =
        previousPositionOf(VisiblePosition::beforeNode(endBR));

  Element* enclosingBlockOfInsertionPos =
      enclosingBlock(insertionPos.anchorNode());

  // Adjust |enclosingBlockOfInsertionPos| to prevent nesting.
  // If the start was in a Mail blockquote, we will have already handled
  // adjusting |enclosingBlockOfInsertionPos| above.
  if (m_preventNesting && enclosingBlockOfInsertionPos &&
      enclosingBlockOfInsertionPos != currentRoot &&
      !isTableCell(enclosingBlockOfInsertionPos) &&
      !startIsInsideMailBlockquote) {
    VisiblePosition visibleInsertionPos = createVisiblePosition(insertionPos);
    if (isEndOfBlock(visibleInsertionPos) &&
        !(isStartOfBlock(visibleInsertionPos) &&
          fragment.hasInterchangeNewlineAtEnd()))
      insertionPos = Position::inParentAfterNode(*enclosingBlockOfInsertionPos);
    else if (isStartOfBlock(visibleInsertionPos))
      insertionPos =
          Position::inParentBeforeNode(*enclosingBlockOfInsertionPos);
  }

  // Paste at start or end of link goes outside of link.
  insertionPos =
      positionAvoidingSpecialElementBoundary(insertionPos, editingState);
  if (editingState->isAborted())
    return;

  // FIXME: Can this wait until after the operation has been performed?  There
  // doesn't seem to be any work performed after this that queries or uses the
  // typing style.
  if (LocalFrame* frame = document().frame())
    frame->editor().clearTypingStyle();

  removeHeadContents(fragment);

  document().updateStyleAndLayoutIgnorePendingStylesheets();

  // We don't want the destination to end up inside nodes that weren't selected.
  // To avoid that, we move the position forward without changing the visible
  // position so we're still at the same visible location, but outside of
  // preceding tags.
  insertionPos = positionAvoidingPrecedingNodes(insertionPos);

  // Paste into run of tabs splits the tab span.
  insertionPos = positionOutsideTabSpan(insertionPos);

  handleStyleSpansBeforeInsertion(fragment, insertionPos);

  // We're finished if there is nothing to add.
  if (fragment.isEmpty() || !fragment.firstChild())
    return;

  // If we are not trying to match the destination style we prefer a position
  // that is outside inline elements that provide style.
  // This way we can produce a less verbose markup.
  // We can skip this optimization for fragments not wrapped in one of
  // our style spans and for positions inside list items
  // since insertAsListItems already does the right thing.
  if (!m_matchStyle && !enclosingList(insertionPos.computeContainerNode())) {
    if (insertionPos.computeContainerNode()->isTextNode() &&
        insertionPos.offsetInContainerNode() &&
        !insertionPos.atLastEditingPositionForNode()) {
      splitTextNode(toText(insertionPos.computeContainerNode()),
                    insertionPos.offsetInContainerNode());
      insertionPos =
          Position::firstPositionInNode(insertionPos.computeContainerNode());
    }

    if (HTMLElement* elementToSplitTo =
            elementToSplitToAvoidPastingIntoInlineElementsWithStyle(
                insertionPos)) {
      if (insertionPos.computeContainerNode() !=
          elementToSplitTo->parentNode()) {
        Node* splitStart = insertionPos.computeNodeAfterPosition();
        if (!splitStart)
          splitStart = insertionPos.computeContainerNode();
        Node* nodeToSplitTo =
            splitTreeToNode(splitStart, elementToSplitTo->parentNode());
        insertionPos = Position::inParentBeforeNode(*nodeToSplitTo);
      }
    }
  }

  // FIXME: When pasting rich content we're often prevented from heading down
  // the fast path by style spans.  Try again here if they've been removed.

  // 1) Insert the content.
  // 2) Remove redundant styles and style tags, this inner <b> for example:
  // <b>foo <b>bar</b> baz</b>.
  // 3) Merge the start of the added content with the content before the
  //    position being pasted into.
  // 4) Do one of the following:
  //    a) expand the last br if the fragment ends with one and it collapsed,
  //    b) merge the last paragraph of the incoming fragment with the paragraph
  //       that contained the end of the selection that was pasted into, or
  //    c) handle an interchange newline at the end of the incoming fragment.
  // 5) Add spaces for smart replace.
  // 6) Select the replacement if requested, and match style if requested.

  InsertedNodes insertedNodes;
  insertedNodes.setRefNode(fragment.firstChild());
  DCHECK(insertedNodes.refNode());
  Node* node = insertedNodes.refNode()->nextSibling();

  fragment.removeNode(insertedNodes.refNode());

  Element* blockStart = enclosingBlock(insertionPos.anchorNode());
  if ((isHTMLListElement(insertedNodes.refNode()) ||
       (isHTMLListElement(insertedNodes.refNode()->firstChild()))) &&
      blockStart && blockStart->layoutObject()->isListItem() &&
      hasEditableStyle(*blockStart->parentNode())) {
    insertedNodes.setRefNode(
        insertAsListItems(toHTMLElement(insertedNodes.refNode()), blockStart,
                          insertionPos, insertedNodes, editingState));
    if (editingState->isAborted())
      return;
  } else {
    insertNodeAt(insertedNodes.refNode(), insertionPos, editingState);
    if (editingState->isAborted())
      return;
    insertedNodes.respondToNodeInsertion(*insertedNodes.refNode());
  }

  // Mutation events (bug 22634) may have already removed the inserted content
  if (!insertedNodes.refNode()->isConnected())
    return;

  bool plainTextFragment = isPlainTextMarkup(insertedNodes.refNode());

  while (node) {
    Node* next = node->nextSibling();
    fragment.removeNode(node);
    insertNodeAfter(node, insertedNodes.refNode(), editingState);
    if (editingState->isAborted())
      return;
    insertedNodes.respondToNodeInsertion(*node);

    // Mutation events (bug 22634) may have already removed the inserted content
    if (!node->isConnected())
      return;

    insertedNodes.setRefNode(node);
    if (node && plainTextFragment)
      plainTextFragment = isPlainTextMarkup(node);
    node = next;
  }

  if (isRichlyEditablePosition(insertionPos))
    removeUnrenderedTextNodesAtEnds(insertedNodes);

  document().updateStyleAndLayoutIgnorePendingStylesheets();

  // Mutation events (bug 20161) may have already removed the inserted content
  if (!insertedNodes.firstNodeInserted() ||
      !insertedNodes.firstNodeInserted()->isConnected())
    return;

  // Scripts specified in javascript protocol may remove
  // |enclosingBlockOfInsertionPos| during insertion, e.g. <iframe
  // src="javascript:...">
  if (enclosingBlockOfInsertionPos &&
      !enclosingBlockOfInsertionPos->isConnected())
    enclosingBlockOfInsertionPos = nullptr;

  VisiblePosition startOfInsertedContent = createVisiblePosition(
      firstPositionInOrBeforeNode(insertedNodes.firstNodeInserted()));

  // We inserted before the enclosingBlockOfInsertionPos to prevent nesting, and
  // the content before the enclosingBlockOfInsertionPos wasn't in its own block
  // and didn't have a br after it, so the inserted content ended up in the same
  // paragraph.
  if (!startOfInsertedContent.isNull() && enclosingBlockOfInsertionPos &&
      insertionPos.anchorNode() == enclosingBlockOfInsertionPos->parentNode() &&
      (unsigned)insertionPos.computeEditingOffset() <
          enclosingBlockOfInsertionPos->nodeIndex() &&
      !isStartOfParagraph(startOfInsertedContent)) {
    insertNodeAt(HTMLBRElement::create(document()),
                 startOfInsertedContent.deepEquivalent(), editingState);
    if (editingState->isAborted())
      return;
  }

  document().updateStyleAndLayoutIgnorePendingStylesheets();
  if (endBR &&
      (plainTextFragment ||
       (shouldRemoveEndBR(endBR, originalVisPosBeforeEndBR) &&
        !(fragment.hasInterchangeNewlineAtEnd() && selectionIsPlainText)))) {
    ContainerNode* parent = endBR->parentNode();
    insertedNodes.willRemoveNode(*endBR);
    removeNode(endBR, editingState);
    if (editingState->isAborted())
      return;
    if (Node* nodeToRemove = highestNodeToRemoveInPruning(parent)) {
      insertedNodes.willRemoveNode(*nodeToRemove);
      removeNode(nodeToRemove, editingState);
      if (editingState->isAborted())
        return;
    }
  }

  makeInsertedContentRoundTrippableWithHTMLTreeBuilder(insertedNodes,
                                                       editingState);
  if (editingState->isAborted())
    return;

  {
    // TODO(dominicc): refNode may not be connected, for example in
    // LayoutTests/editing/inserting/insert-table-in-paragraph-crash.html .
    // Refactor this so there's a relationship between the conditions
    // where refNode is dereferenced and refNode is connected.
    bool refNodeWasConnected = insertedNodes.refNode()->isConnected();
    removeRedundantStylesAndKeepStyleSpanInline(insertedNodes, editingState);
    if (editingState->isAborted())
      return;
    DCHECK_EQ(insertedNodes.refNode()->isConnected(), refNodeWasConnected)
        << insertedNodes.refNode();
  }

  if (m_sanitizeFragment && insertedNodes.firstNodeInserted()) {
    applyCommandToComposite(SimplifyMarkupCommand::create(
                                document(), insertedNodes.firstNodeInserted(),
                                insertedNodes.pastLastLeaf()),
                            editingState);
    if (editingState->isAborted())
      return;
  }

  // Setup m_startOfInsertedContent and m_endOfInsertedContent. This should be
  // the last two lines of code that access insertedNodes.
  m_startOfInsertedContent =
      firstPositionInOrBeforeNode(insertedNodes.firstNodeInserted());
  m_endOfInsertedContent =
      lastPositionInOrAfterNode(insertedNodes.lastLeafInserted());

  // Determine whether or not we should merge the end of inserted content with
  // what's after it before we do the start merge so that the start merge
  // doesn't effect our decision.
  m_shouldMergeEnd = shouldMergeEnd(selectionEndWasEndOfParagraph);

  if (shouldMergeStart(selectionStartWasStartOfParagraph,
                       fragment.hasInterchangeNewlineAtStart(),
                       startIsInsideMailBlockquote)) {
    VisiblePosition startOfParagraphToMove = positionAtStartOfInsertedContent();
    VisiblePosition destination = previousPositionOf(startOfParagraphToMove);

    // Helpers for making the VisiblePositions valid again after DOM changes.
    PositionWithAffinity startOfParagraphToMovePosition =
        startOfParagraphToMove.toPositionWithAffinity();
    PositionWithAffinity destinationPosition =
        destination.toPositionWithAffinity();

    // We need to handle the case where we need to merge the end
    // but our destination node is inside an inline that is the last in the
    // block.
    // We insert a placeholder before the newly inserted content to avoid being
    // merged into the inline.
    Node* destinationNode = destination.deepEquivalent().anchorNode();
    if (m_shouldMergeEnd &&
        destinationNode != enclosingInline(destinationNode) &&
        enclosingInline(destinationNode)->nextSibling()) {
      insertNodeBefore(HTMLBRElement::create(document()),
                       insertedNodes.refNode(), editingState);
      if (editingState->isAborted())
        return;
    }

    // Merging the the first paragraph of inserted content with the content that
    // came before the selection that was pasted into would also move content
    // after the selection that was pasted into if: only one paragraph was being
    // pasted, and it was not wrapped in a block, the selection that was pasted
    // into ended at the end of a block and the next paragraph didn't start at
    // the start of a block.
    // Insert a line break just after the inserted content to separate it from
    // what comes after and prevent that from happening.
    VisiblePosition endOfInsertedContent = positionAtEndOfInsertedContent();
    if (startOfParagraph(endOfInsertedContent).deepEquivalent() ==
        startOfParagraphToMovePosition.position()) {
      insertNodeAt(HTMLBRElement::create(document()),
                   endOfInsertedContent.deepEquivalent(), editingState);
      if (editingState->isAborted())
        return;
      // Mutation events (bug 22634) triggered by inserting the <br> might have
      // removed the content we're about to move
      if (!startOfParagraphToMovePosition.isConnected())
        return;
    }

    document().updateStyleAndLayoutIgnorePendingStylesheets();

    // Making the two VisiblePositions valid again.
    startOfParagraphToMove =
        createVisiblePosition(startOfParagraphToMovePosition);
    destination = createVisiblePosition(destinationPosition);

    // FIXME: Maintain positions for the start and end of inserted content
    // instead of keeping nodes.  The nodes are only ever used to create
    // positions where inserted content starts/ends.
    moveParagraph(startOfParagraphToMove,
                  endOfParagraph(startOfParagraphToMove), destination,
                  editingState);
    if (editingState->isAborted())
      return;

    document().updateStyleAndLayoutIgnorePendingStylesheets();
    m_startOfInsertedContent = mostForwardCaretPosition(
        endingSelection().visibleStart().deepEquivalent());
    if (m_endOfInsertedContent.isOrphan()) {
      m_endOfInsertedContent = mostBackwardCaretPosition(
          endingSelection().visibleEnd().deepEquivalent());
    }
  }

  Position lastPositionToSelect;
  if (fragment.hasInterchangeNewlineAtEnd()) {
    VisiblePosition endOfInsertedContent = positionAtEndOfInsertedContent();
    VisiblePosition next =
        nextPositionOf(endOfInsertedContent, CannotCrossEditingBoundary);

    if (selectionEndWasEndOfParagraph ||
        !isEndOfParagraph(endOfInsertedContent) || next.isNull()) {
      if (TextControlElement* textControl = enclosingTextControl(currentRoot)) {
        if (!insertedNodes.lastLeafInserted()->nextSibling()) {
          insertNodeAfter(textControl->createPlaceholderBreakElement(),
                          insertedNodes.lastLeafInserted(), editingState);
          if (editingState->isAborted())
            return;
        }
        setEndingSelection(
            SelectionInDOMTree::Builder()
                .collapse(Position::afterNode(insertedNodes.lastLeafInserted()))
                .build());
        // Select up to the paragraph separator that was added.
        lastPositionToSelect =
            endingSelection().visibleStart().deepEquivalent();
      } else if (!isStartOfParagraph(endOfInsertedContent)) {
        setEndingSelection(SelectionInDOMTree::Builder()
                               .collapse(endOfInsertedContent.deepEquivalent())
                               .build());
        Element* enclosingBlockElement =
            enclosingBlock(endOfInsertedContent.deepEquivalent().anchorNode());
        if (isListItem(enclosingBlockElement)) {
          HTMLLIElement* newListItem = HTMLLIElement::create(document());
          insertNodeAfter(newListItem, enclosingBlockElement, editingState);
          if (editingState->isAborted())
            return;
          setEndingSelection(
              SelectionInDOMTree::Builder()
                  .collapse(Position::firstPositionInNode(newListItem))
                  .build());
        } else {
          // Use a default paragraph element (a plain div) for the empty
          // paragraph, using the last paragraph block's style seems to annoy
          // users.
          insertParagraphSeparator(
              editingState, true,
              !startIsInsideMailBlockquote &&
                  highestEnclosingNodeOfType(
                      endOfInsertedContent.deepEquivalent(),
                      isMailHTMLBlockquoteElement, CannotCrossEditingBoundary,
                      insertedNodes.firstNodeInserted()->parentNode()));
          if (editingState->isAborted())
            return;
        }

        document().updateStyleAndLayoutIgnorePendingStylesheets();

        // Select up to the paragraph separator that was added.
        lastPositionToSelect =
            endingSelection().visibleStart().deepEquivalent();
        updateNodesInserted(lastPositionToSelect.anchorNode());
      }
    } else {
      // Select up to the beginning of the next paragraph.
      lastPositionToSelect = mostForwardCaretPosition(next.deepEquivalent());
    }
  } else {
    mergeEndIfNeeded(editingState);
    if (editingState->isAborted())
      return;
  }

  if (shouldPerformSmartReplace()) {
    addSpacesForSmartReplace(editingState);
    if (editingState->isAborted())
      return;
  }

  // If we are dealing with a fragment created from plain text
  // no style matching is necessary.
  if (plainTextFragment)
    m_matchStyle = false;

  completeHTMLReplacement(lastPositionToSelect, editingState);
}

bool ReplaceSelectionCommand::shouldRemoveEndBR(
    HTMLBRElement* endBR,
    const VisiblePosition& originalVisPosBeforeEndBR) {
  if (!endBR || !endBR->isConnected())
    return false;

  VisiblePosition visiblePos = VisiblePosition::beforeNode(endBR);

  // Don't remove the br if nothing was inserted.
  if (previousPositionOf(visiblePos).deepEquivalent() ==
      originalVisPosBeforeEndBR.deepEquivalent())
    return false;

  // Remove the br if it is collapsed away and so is unnecessary.
  if (!document().inNoQuirksMode() && isEndOfBlock(visiblePos) &&
      !isStartOfParagraph(visiblePos))
    return true;

  // A br that was originally holding a line open should be displaced by
  // inserted content or turned into a line break.
  // A br that was originally acting as a line break should still be acting as a
  // line break, not as a placeholder.
  return isStartOfParagraph(visiblePos) && isEndOfParagraph(visiblePos);
}

bool ReplaceSelectionCommand::shouldPerformSmartReplace() const {
  if (!m_smartReplace)
    return false;

  TextControlElement* textControl =
      enclosingTextControl(positionAtStartOfInsertedContent().deepEquivalent());
  if (isHTMLInputElement(textControl) &&
      toHTMLInputElement(textControl)->type() == InputTypeNames::password)
    return false;  // Disable smart replace for password fields.

  return true;
}

static bool isCharacterSmartReplaceExemptConsideringNonBreakingSpace(
    UChar32 character,
    bool previousCharacter) {
  return isCharacterSmartReplaceExempt(
      character == noBreakSpaceCharacter ? ' ' : character, previousCharacter);
}

void ReplaceSelectionCommand::addSpacesForSmartReplace(
    EditingState* editingState) {
  VisiblePosition endOfInsertedContent = positionAtEndOfInsertedContent();
  Position endUpstream =
      mostBackwardCaretPosition(endOfInsertedContent.deepEquivalent());
  Node* endNode = endUpstream.computeNodeBeforePosition();
  int endOffset =
      endNode && endNode->isTextNode() ? toText(endNode)->length() : 0;
  if (endUpstream.isOffsetInAnchor()) {
    endNode = endUpstream.computeContainerNode();
    endOffset = endUpstream.offsetInContainerNode();
  }

  bool needsTrailingSpace =
      !isEndOfParagraph(endOfInsertedContent) &&
      !isCharacterSmartReplaceExemptConsideringNonBreakingSpace(
          characterAfter(endOfInsertedContent), false);
  if (needsTrailingSpace && endNode) {
    bool collapseWhiteSpace =
        !endNode->layoutObject() ||
        endNode->layoutObject()->style()->collapseWhiteSpace();
    if (endNode->isTextNode()) {
      insertTextIntoNode(toText(endNode), endOffset,
                         collapseWhiteSpace ? nonBreakingSpaceString() : " ");
      if (m_endOfInsertedContent.computeContainerNode() == endNode)
        m_endOfInsertedContent = Position(
            endNode, m_endOfInsertedContent.offsetInContainerNode() + 1);
    } else {
      Text* node = document().createEditingTextNode(
          collapseWhiteSpace ? nonBreakingSpaceString() : " ");
      insertNodeAfter(node, endNode, editingState);
      if (editingState->isAborted())
        return;
      // Make sure that |updateNodesInserted| does not change
      // |m_startOfInsertedContent|.
      DCHECK(m_startOfInsertedContent.isNotNull());
      updateNodesInserted(node);
    }
  }

  document().updateStyleAndLayout();

  VisiblePosition startOfInsertedContent = positionAtStartOfInsertedContent();
  Position startDownstream =
      mostForwardCaretPosition(startOfInsertedContent.deepEquivalent());
  Node* startNode = startDownstream.computeNodeAfterPosition();
  unsigned startOffset = 0;
  if (startDownstream.isOffsetInAnchor()) {
    startNode = startDownstream.computeContainerNode();
    startOffset = startDownstream.offsetInContainerNode();
  }

  bool needsLeadingSpace =
      !isStartOfParagraph(startOfInsertedContent) &&
      !isCharacterSmartReplaceExemptConsideringNonBreakingSpace(
          characterBefore(startOfInsertedContent), true);
  if (needsLeadingSpace && startNode) {
    bool collapseWhiteSpace =
        !startNode->layoutObject() ||
        startNode->layoutObject()->style()->collapseWhiteSpace();
    if (startNode->isTextNode()) {
      insertTextIntoNode(toText(startNode), startOffset,
                         collapseWhiteSpace ? nonBreakingSpaceString() : " ");
      if (m_endOfInsertedContent.computeContainerNode() == startNode &&
          m_endOfInsertedContent.offsetInContainerNode())
        m_endOfInsertedContent = Position(
            startNode, m_endOfInsertedContent.offsetInContainerNode() + 1);
    } else {
      Text* node = document().createEditingTextNode(
          collapseWhiteSpace ? nonBreakingSpaceString() : " ");
      // Don't updateNodesInserted. Doing so would set m_endOfInsertedContent to
      // be the node containing the leading space, but m_endOfInsertedContent is
      // supposed to mark the end of pasted content.
      insertNodeBefore(node, startNode, editingState);
      if (editingState->isAborted())
        return;
      m_startOfInsertedContent = Position::firstPositionInNode(node);
    }
  }
}

void ReplaceSelectionCommand::completeHTMLReplacement(
    const Position& lastPositionToSelect,
    EditingState* editingState) {
  Position start = positionAtStartOfInsertedContent().deepEquivalent();
  Position end = positionAtEndOfInsertedContent().deepEquivalent();

  // Mutation events may have deleted start or end
  if (start.isNotNull() && !start.isOrphan() && end.isNotNull() &&
      !end.isOrphan()) {
    // FIXME (11475): Remove this and require that the creator of the fragment
    // to use nbsps.
    rebalanceWhitespaceAt(start);
    rebalanceWhitespaceAt(end);

    if (m_matchStyle) {
      DCHECK(m_insertionStyle);
      applyStyle(m_insertionStyle.get(), start, end, editingState);
      if (editingState->isAborted())
        return;
    }

    if (lastPositionToSelect.isNotNull())
      end = lastPositionToSelect;

    mergeTextNodesAroundPosition(start, end, editingState);
    if (editingState->isAborted())
      return;
  } else if (lastPositionToSelect.isNotNull()) {
    start = end = lastPositionToSelect;
  } else {
    return;
  }

  m_startOfInsertedRange = start;
  m_endOfInsertedRange = end;

  if (m_selectReplacement) {
    setEndingSelection(SelectionInDOMTree::Builder()
                           .setBaseAndExtentDeprecated(start, end)
                           .setIsDirectional(endingSelection().isDirectional())
                           .build());
    return;
  }

  if (end.isNotNull()) {
    setEndingSelection(SelectionInDOMTree::Builder()
                           .collapse(end)
                           .setIsDirectional(endingSelection().isDirectional())
                           .build());
    return;
  }
  setEndingSelection(SelectionInDOMTree());
}

void ReplaceSelectionCommand::mergeTextNodesAroundPosition(
    Position& position,
    Position& positionOnlyToBeUpdated,
    EditingState* editingState) {
  bool positionIsOffsetInAnchor = position.isOffsetInAnchor();
  bool positionOnlyToBeUpdatedIsOffsetInAnchor =
      positionOnlyToBeUpdated.isOffsetInAnchor();
  Text* text = nullptr;
  if (positionIsOffsetInAnchor && position.computeContainerNode() &&
      position.computeContainerNode()->isTextNode()) {
    text = toText(position.computeContainerNode());
  } else {
    Node* before = position.computeNodeBeforePosition();
    if (before && before->isTextNode()) {
      text = toText(before);
    } else {
      Node* after = position.computeNodeAfterPosition();
      if (after && after->isTextNode())
        text = toText(after);
    }
  }
  if (!text)
    return;

  // Merging Text nodes causes an additional layout. We'd like to skip it if the
  // editable text is huge.
  // TODO(tkent): 1024 was chosen by my intuition.  We need data.
  const unsigned kMergeSizeLimit = 1024;
  bool hasIncompleteSurrogate =
      text->data().length() >= 1 &&
      (U16_IS_TRAIL(text->data()[0]) ||
       U16_IS_LEAD(text->data()[text->data().length() - 1]));
  if (!hasIncompleteSurrogate && text->data().length() > kMergeSizeLimit)
    return;
  if (text->previousSibling() && text->previousSibling()->isTextNode()) {
    Text* previous = toText(text->previousSibling());
    if (hasIncompleteSurrogate ||
        previous->data().length() <= kMergeSizeLimit) {
      insertTextIntoNode(text, 0, previous->data());

      if (positionIsOffsetInAnchor) {
        position =
            Position(position.computeContainerNode(),
                     previous->length() + position.offsetInContainerNode());
      } else {
        position = computePositionForNodeRemoval(position, *previous);
      }

      if (positionOnlyToBeUpdatedIsOffsetInAnchor) {
        if (positionOnlyToBeUpdated.computeContainerNode() == text)
          positionOnlyToBeUpdated = Position(
              text, previous->length() +
                        positionOnlyToBeUpdated.offsetInContainerNode());
        else if (positionOnlyToBeUpdated.computeContainerNode() == previous)
          positionOnlyToBeUpdated =
              Position(text, positionOnlyToBeUpdated.offsetInContainerNode());
      } else {
        positionOnlyToBeUpdated =
            computePositionForNodeRemoval(positionOnlyToBeUpdated, *previous);
      }

      removeNode(previous, editingState);
      if (editingState->isAborted())
        return;
    }
  }
  if (text->nextSibling() && text->nextSibling()->isTextNode()) {
    Text* next = toText(text->nextSibling());
    if (!hasIncompleteSurrogate && next->data().length() > kMergeSizeLimit)
      return;
    unsigned originalLength = text->length();
    insertTextIntoNode(text, originalLength, next->data());

    if (!positionIsOffsetInAnchor)
      position = computePositionForNodeRemoval(position, *next);

    if (positionOnlyToBeUpdatedIsOffsetInAnchor &&
        positionOnlyToBeUpdated.computeContainerNode() == next) {
      positionOnlyToBeUpdated =
          Position(text, originalLength +
                             positionOnlyToBeUpdated.offsetInContainerNode());
    } else {
      positionOnlyToBeUpdated =
          computePositionForNodeRemoval(positionOnlyToBeUpdated, *next);
    }

    removeNode(next, editingState);
    if (editingState->isAborted())
      return;
  }
}

InputEvent::InputType ReplaceSelectionCommand::inputType() const {
  // |ReplaceSelectionCommand| could be used with Paste, Drag&Drop,
  // InsertFragment and |TypingCommand|.
  // 1. Paste, Drag&Drop, InsertFragment should rely on correct |m_inputType|.
  // 2. |TypingCommand| will supply the |inputType()|, so |m_inputType| could
  //    default to |InputType::None|.
  return m_inputType;
}

// If the user is inserting a list into an existing list, instead of nesting the
// list, we put the list items into the existing list.
Node* ReplaceSelectionCommand::insertAsListItems(HTMLElement* listElement,
                                                 Element* insertionBlock,
                                                 const Position& insertPos,
                                                 InsertedNodes& insertedNodes,
                                                 EditingState* editingState) {
  while (listElement->hasOneChild() &&
         isHTMLListElement(listElement->firstChild()))
    listElement = toHTMLElement(listElement->firstChild());

  document().updateStyleAndLayoutIgnorePendingStylesheets();
  bool isStart = isStartOfParagraph(createVisiblePosition(insertPos));
  bool isEnd = isEndOfParagraph(createVisiblePosition(insertPos));
  bool isMiddle = !isStart && !isEnd;
  Node* lastNode = insertionBlock;

  // If we're in the middle of a list item, we should split it into two separate
  // list items and insert these nodes between them.
  if (isMiddle) {
    int textNodeOffset = insertPos.offsetInContainerNode();
    if (insertPos.anchorNode()->isTextNode() && textNodeOffset > 0)
      splitTextNode(toText(insertPos.anchorNode()), textNodeOffset);
    splitTreeToNode(insertPos.anchorNode(), lastNode, true);
  }

  while (Node* listItem = listElement->firstChild()) {
    listElement->removeChild(listItem, ASSERT_NO_EXCEPTION);
    if (isStart || isMiddle) {
      insertNodeBefore(listItem, lastNode, editingState);
      if (editingState->isAborted())
        return nullptr;
      insertedNodes.respondToNodeInsertion(*listItem);
    } else if (isEnd) {
      insertNodeAfter(listItem, lastNode, editingState);
      if (editingState->isAborted())
        return nullptr;
      insertedNodes.respondToNodeInsertion(*listItem);
      lastNode = listItem;
    } else {
      NOTREACHED();
    }
  }
  if (isStart || isMiddle) {
    if (Node* node = lastNode->previousSibling())
      return node;
  }
  return lastNode;
}

void ReplaceSelectionCommand::updateNodesInserted(Node* node) {
  if (!node)
    return;

  if (m_startOfInsertedContent.isNull())
    m_startOfInsertedContent = firstPositionInOrBeforeNode(node);

  m_endOfInsertedContent =
      lastPositionInOrAfterNode(&NodeTraversal::lastWithinOrSelf(*node));
}

// During simple pastes, where we're just pasting a text node into a run of
// text, we insert the text node directly into the text node that holds the
// selection.  This is much faster than the generalized code in
// ReplaceSelectionCommand, and works around
// <https://bugs.webkit.org/show_bug.cgi?id=6148> since we don't split text
// nodes.
bool ReplaceSelectionCommand::performTrivialReplace(
    const ReplacementFragment& fragment,
    EditingState* editingState) {
  if (!fragment.firstChild() || fragment.firstChild() != fragment.lastChild() ||
      !fragment.firstChild()->isTextNode())
    return false;

  // FIXME: Would be nice to handle smart replace in the fast path.
  if (m_smartReplace || fragment.hasInterchangeNewlineAtStart() ||
      fragment.hasInterchangeNewlineAtEnd())
    return false;

  // e.g. when "bar" is inserted after "foo" in <div><u>foo</u></div>, "bar"
  // should not be underlined.
  if (elementToSplitToAvoidPastingIntoInlineElementsWithStyle(
          endingSelection().start()))
    return false;

  // TODO(editing-dev): Use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  document().updateStyleAndLayoutIgnorePendingStylesheets();

  Node* nodeAfterInsertionPos =
      mostForwardCaretPosition(endingSelection().end()).anchorNode();
  Text* textNode = toText(fragment.firstChild());
  // Our fragment creation code handles tabs, spaces, and newlines, so we don't
  // have to worry about those here.

  Position start = endingSelection().start();
  Position end = replaceSelectedTextInNode(textNode->data());
  if (end.isNull())
    return false;

  document().updateStyleAndLayoutIgnorePendingStylesheets();

  if (nodeAfterInsertionPos && nodeAfterInsertionPos->parentNode() &&
      isHTMLBRElement(*nodeAfterInsertionPos) &&
      shouldRemoveEndBR(toHTMLBRElement(nodeAfterInsertionPos),
                        VisiblePosition::beforeNode(nodeAfterInsertionPos))) {
    removeNodeAndPruneAncestors(nodeAfterInsertionPos, editingState);
    if (editingState->isAborted())
      return false;
  }

  m_startOfInsertedRange = start;
  m_endOfInsertedRange = end;

  setEndingSelection(
      SelectionInDOMTree::Builder()
          .setBaseAndExtentDeprecated(m_selectReplacement ? start : end, end)
          .build());

  return true;
}

bool ReplaceSelectionCommand::isReplaceSelectionCommand() const {
  return true;
}

EphemeralRange ReplaceSelectionCommand::insertedRange() const {
  return EphemeralRange(m_startOfInsertedRange, m_endOfInsertedRange);
}

DEFINE_TRACE(ReplaceSelectionCommand) {
  visitor->trace(m_startOfInsertedContent);
  visitor->trace(m_endOfInsertedContent);
  visitor->trace(m_insertionStyle);
  visitor->trace(m_documentFragment);
  visitor->trace(m_startOfInsertedRange);
  visitor->trace(m_endOfInsertedRange);
  CompositeEditCommand::trace(visitor);
}

}  // namespace blink
