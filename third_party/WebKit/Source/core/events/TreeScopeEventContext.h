/*
 * Copyright (C) 2014 Google Inc. All Rights Reserved.
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
 *
 */

#ifndef TreeScopeEventContext_h
#define TreeScopeEventContext_h

#include "core/CoreExport.h"
#include "core/dom/Node.h"
#include "core/dom/TreeScope.h"
#include "core/events/EventTarget.h"
#include "wtf/Vector.h"

namespace blink {

class ContainerNode;
class EventPath;
class EventTarget;
template <typename NodeType>
class StaticNodeTypeList;
using StaticNodeList = StaticNodeTypeList<Node>;
class TouchEventContext;
class TreeScope;

class CORE_EXPORT TreeScopeEventContext final
    : public GarbageCollected<TreeScopeEventContext> {
 public:
  static TreeScopeEventContext* create(TreeScope&);
  DECLARE_TRACE();

  TreeScope& treeScope() const { return *m_treeScope; }
  ContainerNode& rootNode() const { return m_treeScope->rootNode(); }

  EventTarget* target() const { return m_target.get(); }
  void setTarget(EventTarget*);

  EventTarget* relatedTarget() const { return m_relatedTarget.get(); }
  void setRelatedTarget(EventTarget*);

  TouchEventContext* touchEventContext() const {
    return m_touchEventContext.get();
  }
  TouchEventContext* ensureTouchEventContext();

  HeapVector<Member<EventTarget>>& ensureEventPath(EventPath&);

  bool isInclusiveAncestorOf(const TreeScopeEventContext&) const;
  bool isDescendantOf(const TreeScopeEventContext&) const;
#if DCHECK_IS_ON()
  bool isExclusivePartOf(const TreeScopeEventContext&) const;
#endif
  void addChild(TreeScopeEventContext& child) { m_children.push_back(&child); }

  // For ancestor-descendant relationship check in O(1).
  // Preprocessing takes O(N).
  int calculateTreeOrderAndSetNearestAncestorClosedTree(
      int orderNumber,
      TreeScopeEventContext* nearestAncestorClosedTreeScopeEventContext);

  TreeScopeEventContext* containingClosedShadowTree() const {
    return m_containingClosedShadowTree.get();
  }

 private:
  TreeScopeEventContext(TreeScope&);

  void checkReachableNode(EventTarget&);

  bool isUnclosedTreeOf(const TreeScopeEventContext& other);

  Member<TreeScope> m_treeScope;
  Member<EventTarget> m_target;
  Member<EventTarget> m_relatedTarget;
  Member<HeapVector<Member<EventTarget>>> m_eventPath;
  Member<TouchEventContext> m_touchEventContext;
  Member<TreeScopeEventContext> m_containingClosedShadowTree;

  HeapVector<Member<TreeScopeEventContext>> m_children;
  int m_preOrder;
  int m_postOrder;
};

#if DCHECK_IS_ON()
inline void TreeScopeEventContext::checkReachableNode(EventTarget& target) {
  if (!target.toNode())
    return;
  // FIXME: Checks also for SVG elements.
  if (target.toNode()->isSVGElement())
    return;
  DCHECK(
      target.toNode()
          ->treeScope()
          .isInclusiveOlderSiblingShadowRootOrAncestorTreeScopeOf(treeScope()));
}
#else
inline void TreeScopeEventContext::checkReachableNode(EventTarget&) {}
#endif

inline void TreeScopeEventContext::setTarget(EventTarget* target) {
  DCHECK(target);
  checkReachableNode(*target);
  m_target = target;
}

inline void TreeScopeEventContext::setRelatedTarget(
    EventTarget* relatedTarget) {
  DCHECK(relatedTarget);
  checkReachableNode(*relatedTarget);
  m_relatedTarget = relatedTarget;
}

inline bool TreeScopeEventContext::isInclusiveAncestorOf(
    const TreeScopeEventContext& other) const {
  DCHECK_NE(m_preOrder, -1);
  DCHECK_NE(m_postOrder, -1);
  DCHECK_NE(other.m_preOrder, -1);
  DCHECK_NE(other.m_postOrder, -1);
  return m_preOrder <= other.m_preOrder && other.m_postOrder <= m_postOrder;
}

inline bool TreeScopeEventContext::isDescendantOf(
    const TreeScopeEventContext& other) const {
  DCHECK_NE(m_preOrder, -1);
  DCHECK_NE(m_postOrder, -1);
  DCHECK_NE(other.m_preOrder, -1);
  DCHECK_NE(other.m_postOrder, -1);
  return other.m_preOrder < m_preOrder && m_postOrder < other.m_postOrder;
}

#if DCHECK_IS_ON()
inline bool TreeScopeEventContext::isExclusivePartOf(
    const TreeScopeEventContext& other) const {
  DCHECK_NE(m_preOrder, -1);
  DCHECK_NE(m_postOrder, -1);
  DCHECK_NE(other.m_preOrder, -1);
  DCHECK_NE(other.m_postOrder, -1);
  return (m_preOrder < other.m_preOrder && m_postOrder < other.m_preOrder) ||
         (m_preOrder > other.m_preOrder && m_preOrder > other.m_postOrder);
}
#endif

}  // namespace blink

#endif  // TreeScopeEventContext_h
