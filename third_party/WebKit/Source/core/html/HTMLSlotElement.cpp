/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/HTMLSlotElement.h"

#include "bindings/core/v8/Microtask.h"
#include "core/HTMLNames.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/SlotAssignment.h"
#include "core/events/Event.h"
#include "core/html/AssignedNodesOptions.h"
#include "core/inspector/InspectorInstrumentation.h"

namespace blink {

using namespace HTMLNames;

inline HTMLSlotElement::HTMLSlotElement(Document& document)
    : HTMLElement(slotTag, document) {
  setHasCustomStyleCallbacks();
}

DEFINE_NODE_FACTORY(HTMLSlotElement);

// static
AtomicString HTMLSlotElement::normalizeSlotName(const AtomicString& name) {
  return (name.isNull() || name.isEmpty()) ? emptyAtom : name;
}

const HeapVector<Member<Node>>& HTMLSlotElement::assignedNodes() {
  DCHECK(!needsDistributionRecalc());
  DCHECK(isInShadowTree() || m_assignedNodes.isEmpty());
  return m_assignedNodes;
}

const HeapVector<Member<Node>> HTMLSlotElement::assignedNodesForBinding(
    const AssignedNodesOptions& options) {
  updateDistribution();
  if (options.hasFlatten() && options.flatten())
    return getDistributedNodesForBinding();
  return m_assignedNodes;
}

const HeapVector<Member<Node>>
HTMLSlotElement::getDistributedNodesForBinding() {
  DCHECK(!needsDistributionRecalc());
  if (supportsDistribution())
    return m_distributedNodes;

  // If a slot does not support distribution, its m_distributedNodes should not
  // be used.  Instead, calculate distribution manually here. This happens only
  // in a slot in non-shadow trees, so its assigned nodes are always empty.
  HeapVector<Member<Node>> distributedNodes;
  Node* child = NodeTraversal::firstChild(*this);
  while (child) {
    if (!child->isSlotable()) {
      child = NodeTraversal::nextSkippingChildren(*child, this);
      continue;
    }
    if (isHTMLSlotElement(child)) {
      child = NodeTraversal::next(*child, this);
    } else {
      distributedNodes.push_back(child);
      child = NodeTraversal::nextSkippingChildren(*child, this);
    }
  }
  return distributedNodes;
}

const HeapVector<Member<Node>>& HTMLSlotElement::getDistributedNodes() {
  DCHECK(!needsDistributionRecalc());
  DCHECK(supportsDistribution() || m_distributedNodes.isEmpty());
  return m_distributedNodes;
}

void HTMLSlotElement::appendAssignedNode(Node& hostChild) {
  DCHECK(hostChild.isSlotable());
  m_assignedNodes.push_back(&hostChild);
}

void HTMLSlotElement::resolveDistributedNodes() {
  for (auto& node : m_assignedNodes) {
    DCHECK(node->isSlotable());
    if (isHTMLSlotElement(*node))
      appendDistributedNodesFrom(toHTMLSlotElement(*node));
    else
      appendDistributedNode(*node);

    if (isChildOfV1ShadowHost())
      parentElementShadow()->setNeedsDistributionRecalc();
  }
}

void HTMLSlotElement::appendDistributedNode(Node& node) {
  size_t size = m_distributedNodes.size();
  m_distributedNodes.push_back(&node);
  m_distributedIndices.set(&node, size);
}

void HTMLSlotElement::appendDistributedNodesFrom(const HTMLSlotElement& other) {
  size_t index = m_distributedNodes.size();
  m_distributedNodes.appendVector(other.m_distributedNodes);
  for (const auto& node : other.m_distributedNodes)
    m_distributedIndices.set(node.get(), index++);
}

void HTMLSlotElement::clearDistribution() {
  // TODO(hayato): Figure out when to call
  // lazyReattachDistributedNodesIfNeeded()
  m_assignedNodes.clear();
  m_distributedNodes.clear();
  m_distributedIndices.clear();
}

void HTMLSlotElement::saveAndClearDistribution() {
  m_oldDistributedNodes.swap(m_distributedNodes);
  clearDistribution();
}

void HTMLSlotElement::dispatchSlotChangeEvent() {
  Event* event = Event::createBubble(EventTypeNames::slotchange);
  event->setTarget(this);
  dispatchScopedEvent(event);
}

Node* HTMLSlotElement::distributedNodeNextTo(const Node& node) const {
  DCHECK(supportsDistribution());
  const auto& it = m_distributedIndices.find(&node);
  if (it == m_distributedIndices.end())
    return nullptr;
  size_t index = it->value;
  if (index + 1 == m_distributedNodes.size())
    return nullptr;
  return m_distributedNodes[index + 1].get();
}

Node* HTMLSlotElement::distributedNodePreviousTo(const Node& node) const {
  DCHECK(supportsDistribution());
  const auto& it = m_distributedIndices.find(&node);
  if (it == m_distributedIndices.end())
    return nullptr;
  size_t index = it->value;
  if (index == 0)
    return nullptr;
  return m_distributedNodes[index - 1].get();
}

AtomicString HTMLSlotElement::name() const {
  return normalizeSlotName(fastGetAttribute(HTMLNames::nameAttr));
}

void HTMLSlotElement::attachLayoutTree(const AttachContext& context) {
  if (supportsDistribution()) {
    for (auto& node : m_distributedNodes) {
      if (node->needsAttach())
        node->attachLayoutTree(context);
    }
  }
  HTMLElement::attachLayoutTree(context);
}

void HTMLSlotElement::detachLayoutTree(const AttachContext& context) {
  if (supportsDistribution()) {
    for (auto& node : m_distributedNodes)
      node->lazyReattachIfAttached();
  }
  HTMLElement::detachLayoutTree(context);
}

void HTMLSlotElement::attributeChanged(
    const AttributeModificationParams& params) {
  if (params.name == nameAttr) {
    if (ShadowRoot* root = containingShadowRoot()) {
      if (root->isV1() && params.oldValue != params.newValue) {
        root->slotAssignment().slotRenamed(normalizeSlotName(params.oldValue),
                                           *this);
      }
    }
  }
  HTMLElement::attributeChanged(params);
}

static bool wasInShadowTreeBeforeInserted(HTMLSlotElement& slot,
                                          ContainerNode& insertionPoint) {
  ShadowRoot* root1 = slot.containingShadowRoot();
  ShadowRoot* root2 = insertionPoint.containingShadowRoot();
  if (root1 && root2 && root1 == root2)
    return false;
  return root1;
}

Node::InsertionNotificationRequest HTMLSlotElement::insertedInto(
    ContainerNode* insertionPoint) {
  HTMLElement::insertedInto(insertionPoint);
  ShadowRoot* root = containingShadowRoot();
  if (root) {
    DCHECK(root->owner());
    root->owner()->setNeedsDistributionRecalc();
    // Relevant DOM Standard: https://dom.spec.whatwg.org/#concept-node-insert
    // - 6.4:  Run assign slotables for a tree with node's tree and a set
    // containing each inclusive descendant of node that is a slot.
    if (root->isV1() && !wasInShadowTreeBeforeInserted(*this, *insertionPoint))
      root->didAddSlot(*this);
  }

  // We could have been distributed into in a detached subtree, make sure to
  // clear the distribution when inserted again to avoid cycles.
  clearDistribution();

  return InsertionDone;
}

static ShadowRoot* containingShadowRootBeforeRemoved(
    Node& removedDescendant,
    ContainerNode& insertionPoint) {
  if (ShadowRoot* root = removedDescendant.containingShadowRoot())
    return root;
  return insertionPoint.containingShadowRoot();
}

void HTMLSlotElement::removedFrom(ContainerNode* insertionPoint) {
  // `removedFrom` is called after the node is removed from the tree.
  // That means:
  // 1. If this slot is still in a tree scope, it means the slot has been in a
  // shadow tree. An inclusive shadow-including ancestor of the shadow host was
  // originally removed from its parent.
  // 2. Or (this slot is now not in a tree scope), this slot's inclusive
  // ancestor was orginally removed from its parent (== insertion point). This
  // slot and the originally removed node was in the same tree.

  ShadowRoot* root = containingShadowRootBeforeRemoved(*this, *insertionPoint);
  if (root) {
    if (ElementShadow* rootOwner = root->owner())
      rootOwner->setNeedsDistributionRecalc();
  }

  // Since this insertion point is no longer visible from the shadow subtree, it
  // need to clean itself up.
  clearDistribution();

  if (root && root->isV1() && root == insertionPoint->treeScope().rootNode()) {
    // This slot was in a shadow tree and got disconnected from the shadow root.
    root->slotAssignment().slotRemoved(*this);
  }

  HTMLElement::removedFrom(insertionPoint);
}

void HTMLSlotElement::willRecalcStyle(StyleRecalcChange change) {
  if (change < IndependentInherit && getStyleChangeType() < SubtreeStyleChange)
    return;

  for (auto& node : m_distributedNodes)
    node->setNeedsStyleRecalc(
        LocalStyleChange,
        StyleChangeReasonForTracing::create(
            StyleChangeReason::PropagateInheritChangeToDistributedNodes));
}

void HTMLSlotElement::updateDistributedNodesWithFallback() {
  if (!m_distributedNodes.isEmpty())
    return;
  for (auto& child : NodeTraversal::childrenOf(*this)) {
    if (!child.isSlotable())
      continue;
    if (isHTMLSlotElement(child))
      appendDistributedNodesFrom(toHTMLSlotElement(child));
    else
      appendDistributedNode(child);
  }
}

void HTMLSlotElement::lazyReattachDistributedNodesIfNeeded() {
  // TODO(hayato): Figure out an exact condition where reattach is required
  if (m_oldDistributedNodes != m_distributedNodes) {
    for (auto& node : m_oldDistributedNodes)
      node->lazyReattachIfAttached();
    for (auto& node : m_distributedNodes)
      node->lazyReattachIfAttached();
    InspectorInstrumentation::didPerformSlotDistribution(this);
  }
  m_oldDistributedNodes.clear();
}

void HTMLSlotElement::didSlotChange(SlotChangeType slotChangeType) {
  if (slotChangeType == SlotChangeType::Initial)
    enqueueSlotChangeEvent();
  ShadowRoot* root = containingShadowRoot();
  // TODO(hayato): Relax this check if slots in non-shadow trees are well
  // supported.
  DCHECK(root);
  DCHECK(root->isV1());
  root->owner()->setNeedsDistributionRecalc();
  // Check slotchange recursively since this slotchange may cause another
  // slotchange.
  checkSlotChange(SlotChangeType::Chained);
}

void HTMLSlotElement::enqueueSlotChangeEvent() {
  if (m_slotchangeEventEnqueued)
    return;
  MutationObserver::enqueueSlotChange(*this);
  m_slotchangeEventEnqueued = true;
}

bool HTMLSlotElement::hasAssignedNodesSlow() const {
  ShadowRoot* root = containingShadowRoot();
  DCHECK(root);
  DCHECK(root->isV1());
  SlotAssignment& assignment = root->slotAssignment();
  if (assignment.findSlotByName(name()) != this)
    return false;
  return assignment.findHostChildBySlotName(name());
}

bool HTMLSlotElement::findHostChildWithSameSlotName() const {
  ShadowRoot* root = containingShadowRoot();
  DCHECK(root);
  DCHECK(root->isV1());
  SlotAssignment& assignment = root->slotAssignment();
  return assignment.findHostChildBySlotName(name());
}

int HTMLSlotElement::tabIndex() const {
  return Element::tabIndex();
}

DEFINE_TRACE(HTMLSlotElement) {
  visitor->trace(m_assignedNodes);
  visitor->trace(m_distributedNodes);
  visitor->trace(m_oldDistributedNodes);
  visitor->trace(m_distributedIndices);
  HTMLElement::trace(visitor);
}

}  // namespace blink
