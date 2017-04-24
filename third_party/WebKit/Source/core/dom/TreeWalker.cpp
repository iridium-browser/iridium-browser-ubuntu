/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/dom/TreeWalker.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/NodeTraversal.h"

namespace blink {

TreeWalker::TreeWalker(Node* rootNode, unsigned whatToShow, NodeFilter* filter)
    : NodeIteratorBase(this, rootNode, whatToShow, filter), m_current(root()) {}

void TreeWalker::setCurrentNode(Node* node) {
  DCHECK(node);
  m_current = node;
}

inline Node* TreeWalker::setCurrent(Node* node) {
  m_current = node;
  return m_current.get();
}

Node* TreeWalker::parentNode(ExceptionState& exceptionState) {
  Node* node = m_current;
  while (node != root()) {
    node = node->parentNode();
    if (!node)
      return 0;
    unsigned acceptNodeResult = acceptNode(node, exceptionState);
    if (exceptionState.hadException())
      return 0;
    if (acceptNodeResult == NodeFilter::kFilterAccept)
      return setCurrent(node);
  }
  return 0;
}

Node* TreeWalker::firstChild(ExceptionState& exceptionState) {
  for (Node* node = m_current->firstChild(); node;) {
    unsigned acceptNodeResult = acceptNode(node, exceptionState);
    if (exceptionState.hadException())
      return 0;
    switch (acceptNodeResult) {
      case NodeFilter::kFilterAccept:
        m_current = node;
        return m_current.get();
      case NodeFilter::kFilterSkip:
        if (node->hasChildren()) {
          node = node->firstChild();
          continue;
        }
        break;
      case NodeFilter::kFilterReject:
        break;
    }
    do {
      if (node->nextSibling()) {
        node = node->nextSibling();
        break;
      }
      ContainerNode* parent = node->parentNode();
      if (!parent || parent == root() || parent == m_current)
        return 0;
      node = parent;
    } while (node);
  }
  return 0;
}

Node* TreeWalker::lastChild(ExceptionState& exceptionState) {
  for (Node* node = m_current->lastChild(); node;) {
    unsigned acceptNodeResult = acceptNode(node, exceptionState);
    if (exceptionState.hadException())
      return 0;
    switch (acceptNodeResult) {
      case NodeFilter::kFilterAccept:
        m_current = node;
        return m_current.get();
      case NodeFilter::kFilterSkip:
        if (node->lastChild()) {
          node = node->lastChild();
          continue;
        }
        break;
      case NodeFilter::kFilterReject:
        break;
    }
    do {
      if (node->previousSibling()) {
        node = node->previousSibling();
        break;
      }
      ContainerNode* parent = node->parentNode();
      if (!parent || parent == root() || parent == m_current)
        return 0;
      node = parent;
    } while (node);
  }
  return 0;
}

Node* TreeWalker::previousSibling(ExceptionState& exceptionState) {
  Node* node = m_current;
  if (node == root())
    return 0;
  while (1) {
    for (Node* sibling = node->previousSibling(); sibling;) {
      unsigned acceptNodeResult = acceptNode(sibling, exceptionState);
      if (exceptionState.hadException())
        return 0;
      switch (acceptNodeResult) {
        case NodeFilter::kFilterAccept:
          m_current = sibling;
          return m_current.get();
        case NodeFilter::kFilterSkip:
          if (sibling->lastChild()) {
            sibling = sibling->lastChild();
            node = sibling;
            continue;
          }
          break;
        case NodeFilter::kFilterReject:
          break;
      }
      sibling = sibling->previousSibling();
    }
    node = node->parentNode();
    if (!node || node == root())
      return 0;
    unsigned acceptNodeResult = acceptNode(node, exceptionState);
    if (exceptionState.hadException())
      return 0;
    if (acceptNodeResult == NodeFilter::kFilterAccept)
      return 0;
  }
}

Node* TreeWalker::nextSibling(ExceptionState& exceptionState) {
  Node* node = m_current;
  if (node == root())
    return 0;
  while (1) {
    for (Node* sibling = node->nextSibling(); sibling;) {
      unsigned acceptNodeResult = acceptNode(sibling, exceptionState);
      if (exceptionState.hadException())
        return 0;
      switch (acceptNodeResult) {
        case NodeFilter::kFilterAccept:
          m_current = sibling;
          return m_current.get();
        case NodeFilter::kFilterSkip:
          if (sibling->hasChildren()) {
            sibling = sibling->firstChild();
            node = sibling;
            continue;
          }
          break;
        case NodeFilter::kFilterReject:
          break;
      }
      sibling = sibling->nextSibling();
    }
    node = node->parentNode();
    if (!node || node == root())
      return 0;
    unsigned acceptNodeResult = acceptNode(node, exceptionState);
    if (exceptionState.hadException())
      return 0;
    if (acceptNodeResult == NodeFilter::kFilterAccept)
      return 0;
  }
}

Node* TreeWalker::previousNode(ExceptionState& exceptionState) {
  Node* node = m_current;
  while (node != root()) {
    while (Node* previousSibling = node->previousSibling()) {
      node = previousSibling;
      unsigned acceptNodeResult = acceptNode(node, exceptionState);
      if (exceptionState.hadException())
        return 0;
      if (acceptNodeResult == NodeFilter::kFilterReject)
        continue;
      while (Node* lastChild = node->lastChild()) {
        node = lastChild;
        acceptNodeResult = acceptNode(node, exceptionState);
        if (exceptionState.hadException())
          return 0;
        if (acceptNodeResult == NodeFilter::kFilterReject)
          break;
      }
      if (acceptNodeResult == NodeFilter::kFilterAccept) {
        m_current = node;
        return m_current.get();
      }
    }
    if (node == root())
      return 0;
    ContainerNode* parent = node->parentNode();
    if (!parent)
      return 0;
    node = parent;
    unsigned acceptNodeResult = acceptNode(node, exceptionState);
    if (exceptionState.hadException())
      return 0;
    if (acceptNodeResult == NodeFilter::kFilterAccept)
      return setCurrent(node);
  }
  return 0;
}

Node* TreeWalker::nextNode(ExceptionState& exceptionState) {
  Node* node = m_current;
Children:
  while (Node* firstChild = node->firstChild()) {
    node = firstChild;
    unsigned acceptNodeResult = acceptNode(node, exceptionState);
    if (exceptionState.hadException())
      return 0;
    if (acceptNodeResult == NodeFilter::kFilterAccept)
      return setCurrent(node);
    if (acceptNodeResult == NodeFilter::kFilterReject)
      break;
  }
  while (Node* nextSibling =
             NodeTraversal::nextSkippingChildren(*node, root())) {
    node = nextSibling;
    unsigned acceptNodeResult = acceptNode(node, exceptionState);
    if (exceptionState.hadException())
      return 0;
    if (acceptNodeResult == NodeFilter::kFilterAccept)
      return setCurrent(node);
    if (acceptNodeResult == NodeFilter::kFilterSkip)
      goto Children;
  }
  return 0;
}

DEFINE_TRACE(TreeWalker) {
  visitor->trace(m_current);
  NodeIteratorBase::trace(visitor);
}

DEFINE_TRACE_WRAPPERS(TreeWalker) {
  NodeIteratorBase::traceWrappers(visitor);
}

}  // namespace blink
