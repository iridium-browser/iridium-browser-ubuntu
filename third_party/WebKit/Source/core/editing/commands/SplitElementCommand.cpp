/*
 * Copyright (C) 2005, 2008 Apple Inc. All rights reserved.
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

#include "core/editing/commands/SplitElementCommand.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/dom/Element.h"
#include "core/editing/EditingUtilities.h"
#include "wtf/Assertions.h"

namespace blink {

SplitElementCommand::SplitElementCommand(Element* element, Node* atChild)
    : SimpleEditCommand(element->document()),
      m_element2(element),
      m_atChild(atChild) {
  DCHECK(m_element2);
  DCHECK(m_atChild);
  DCHECK_EQ(m_atChild->parentNode(), m_element2);
}

void SplitElementCommand::executeApply() {
  if (m_atChild->parentNode() != m_element2)
    return;

  HeapVector<Member<Node>> children;
  for (Node* node = m_element2->firstChild(); node != m_atChild;
       node = node->nextSibling())
    children.push_back(node);

  DummyExceptionStateForTesting exceptionState;

  ContainerNode* parent = m_element2->parentNode();
  if (!parent || !hasEditableStyle(*parent))
    return;
  parent->insertBefore(m_element1.get(), m_element2.get(), exceptionState);
  if (exceptionState.hadException())
    return;

  // Delete id attribute from the second element because the same id cannot be
  // used for more than one element
  m_element2->removeAttribute(HTMLNames::idAttr);

  for (const auto& child : children)
    m_element1->appendChild(child, exceptionState);
}

void SplitElementCommand::doApply(EditingState*) {
  m_element1 = m_element2->cloneElementWithoutChildren();

  executeApply();
}

void SplitElementCommand::doUnapply() {
  if (!m_element1 || !hasEditableStyle(*m_element1) ||
      !hasEditableStyle(*m_element2))
    return;

  NodeVector children;
  getChildNodes(*m_element1, children);

  Node* refChild = m_element2->firstChild();

  for (const auto& child : children)
    m_element2->insertBefore(child, refChild, IGNORE_EXCEPTION_FOR_TESTING);

  // Recover the id attribute of the original element.
  const AtomicString& id = m_element1->getAttribute(HTMLNames::idAttr);
  if (!id.isNull())
    m_element2->setAttribute(HTMLNames::idAttr, id);

  m_element1->remove(IGNORE_EXCEPTION_FOR_TESTING);
}

void SplitElementCommand::doReapply() {
  if (!m_element1)
    return;

  executeApply();
}

DEFINE_TRACE(SplitElementCommand) {
  visitor->trace(m_element1);
  visitor->trace(m_element2);
  visitor->trace(m_atChild);
  SimpleEditCommand::trace(visitor);
}

}  // namespace blink
