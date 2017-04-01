/*
 * Copyright (C) 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006, 2009 Apple Inc.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/xml/XPathPath.h"

#include "core/dom/Document.h"
#include "core/dom/NodeTraversal.h"
#include "core/xml/XPathPredicate.h"
#include "core/xml/XPathStep.h"
#include "core/xml/XPathValue.h"

namespace blink {
namespace XPath {

Filter::Filter(Expression* expr, HeapVector<Member<Predicate>>& predicates)
    : m_expr(expr) {
  m_predicates.swap(predicates);
  setIsContextNodeSensitive(m_expr->isContextNodeSensitive());
  setIsContextPositionSensitive(m_expr->isContextPositionSensitive());
  setIsContextSizeSensitive(m_expr->isContextSizeSensitive());
}

Filter::~Filter() {}

DEFINE_TRACE(Filter) {
  visitor->trace(m_expr);
  visitor->trace(m_predicates);
  Expression::trace(visitor);
}

Value Filter::evaluate(EvaluationContext& evaluationContext) const {
  Value v = m_expr->evaluate(evaluationContext);

  NodeSet& nodes = v.modifiableNodeSet(evaluationContext);
  nodes.sort();

  for (const auto& predicate : m_predicates) {
    NodeSet* newNodes = NodeSet::create();
    evaluationContext.size = nodes.size();
    evaluationContext.position = 0;

    for (const auto& node : nodes) {
      evaluationContext.node = node;
      ++evaluationContext.position;

      if (predicate->evaluate(evaluationContext))
        newNodes->append(node);
    }
    nodes.swap(*newNodes);
  }

  return v;
}

LocationPath::LocationPath() : m_absolute(false) {
  setIsContextNodeSensitive(true);
}

LocationPath::~LocationPath() {}

DEFINE_TRACE(LocationPath) {
  visitor->trace(m_steps);
  Expression::trace(visitor);
}

Value LocationPath::evaluate(EvaluationContext& evaluationContext) const {
  EvaluationContext clonedContext = evaluationContext;
  // http://www.w3.org/TR/xpath/
  // Section 2, Location Paths:
  // "/ selects the document root (which is always the parent of the document
  // element)"
  // "A / by itself selects the root node of the document containing the context
  // node."
  // In the case of a tree that is detached from the document, we violate
  // the spec and treat / as the root node of the detached tree.
  // This is for compatibility with Firefox, and also seems like a more
  // logical treatment of where you would expect the "root" to be.
  Node* context = evaluationContext.node.get();
  if (m_absolute && context->getNodeType() != Node::kDocumentNode) {
    if (context->isConnected())
      context = context->ownerDocument();
    else
      context = &NodeTraversal::highestAncestorOrSelf(*context);
  }

  NodeSet* nodes = NodeSet::create();
  nodes->append(context);
  evaluate(clonedContext, *nodes);

  return Value(nodes, Value::adopt);
}

void LocationPath::evaluate(EvaluationContext& context, NodeSet& nodes) const {
  bool resultIsSorted = nodes.isSorted();

  for (const auto& step : m_steps) {
    NodeSet* newNodes = NodeSet::create();
    HeapHashSet<Member<Node>> newNodesSet;

    bool needToCheckForDuplicateNodes =
        !nodes.subtreesAreDisjoint() ||
        (step->getAxis() != Step::ChildAxis &&
         step->getAxis() != Step::SelfAxis &&
         step->getAxis() != Step::DescendantAxis &&
         step->getAxis() != Step::DescendantOrSelfAxis &&
         step->getAxis() != Step::AttributeAxis);

    if (needToCheckForDuplicateNodes)
      resultIsSorted = false;

    // This is a simplified check that can be improved to handle more cases.
    if (nodes.subtreesAreDisjoint() && (step->getAxis() == Step::ChildAxis ||
                                        step->getAxis() == Step::SelfAxis))
      newNodes->markSubtreesDisjoint(true);

    for (const auto& inputNode : nodes) {
      NodeSet* matches = NodeSet::create();
      step->evaluate(context, inputNode, *matches);

      if (!matches->isSorted())
        resultIsSorted = false;

      for (const auto& node : *matches) {
        if (!needToCheckForDuplicateNodes || newNodesSet.add(node).isNewEntry)
          newNodes->append(node);
      }
    }

    nodes.swap(*newNodes);
  }

  nodes.markSorted(resultIsSorted);
}

void LocationPath::appendStep(Step* step) {
  unsigned stepCount = m_steps.size();
  if (stepCount && optimizeStepPair(m_steps[stepCount - 1], step))
    return;
  step->optimize();
  m_steps.push_back(step);
}

void LocationPath::insertFirstStep(Step* step) {
  if (m_steps.size() && optimizeStepPair(step, m_steps[0])) {
    m_steps[0] = step;
    return;
  }
  step->optimize();
  m_steps.insert(0, step);
}

Path::Path(Expression* filter, LocationPath* path)
    : m_filter(filter), m_path(path) {
  setIsContextNodeSensitive(filter->isContextNodeSensitive());
  setIsContextPositionSensitive(filter->isContextPositionSensitive());
  setIsContextSizeSensitive(filter->isContextSizeSensitive());
}

Path::~Path() {}

DEFINE_TRACE(Path) {
  visitor->trace(m_filter);
  visitor->trace(m_path);
  Expression::trace(visitor);
}

Value Path::evaluate(EvaluationContext& context) const {
  Value v = m_filter->evaluate(context);

  NodeSet& nodes = v.modifiableNodeSet(context);
  m_path->evaluate(context, nodes);

  return v;
}

}  // namespace XPath

}  // namespace blink
