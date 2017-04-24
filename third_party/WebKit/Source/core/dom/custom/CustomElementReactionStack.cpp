// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementReactionStack.h"

#include "bindings/core/v8/Microtask.h"
#include "core/dom/Element.h"
#include "core/dom/custom/CEReactionsScope.h"
#include "core/dom/custom/CustomElementReactionQueue.h"
#include "core/frame/FrameHost.h"

namespace blink {

namespace {

Persistent<CustomElementReactionStack>& customElementReactionStack() {
  DEFINE_STATIC_LOCAL(Persistent<CustomElementReactionStack>,
                      customElementReactionStack,
                      (new CustomElementReactionStack));
  return customElementReactionStack;
}

}  // namespace

// TODO(dominicc): Consider using linked heap structures, avoiding
// finalizers, to make short-lived entries fast.

CustomElementReactionStack::CustomElementReactionStack() {}

DEFINE_TRACE(CustomElementReactionStack) {
  visitor->trace(m_map);
  visitor->trace(m_stack);
  visitor->trace(m_backupQueue);
}

DEFINE_TRACE_WRAPPERS(CustomElementReactionStack) {
  for (auto key : m_map.keys()) {
    visitor->traceWrappers(key);
  }
}

void CustomElementReactionStack::push() {
  m_stack.push_back(nullptr);
}

void CustomElementReactionStack::popInvokingReactions() {
  ElementQueue* queue = m_stack.back();
  if (queue)
    invokeReactions(*queue);
  m_stack.pop_back();
}

void CustomElementReactionStack::invokeReactions(ElementQueue& queue) {
  for (size_t i = 0; i < queue.size(); ++i) {
    Element* element = queue[i];
    if (CustomElementReactionQueue* reactions = m_map.at(element)) {
      reactions->invokeReactions(element);
      CHECK(reactions->isEmpty());
      m_map.erase(element);
    }
  }
}

void CustomElementReactionStack::enqueueToCurrentQueue(
    Element* element,
    CustomElementReaction* reaction) {
  enqueue(m_stack.back(), element, reaction);
}

void CustomElementReactionStack::enqueue(Member<ElementQueue>& queue,
                                         Element* element,
                                         CustomElementReaction* reaction) {
  if (!queue)
    queue = new ElementQueue();
  queue->push_back(element);

  CustomElementReactionQueue* reactions = m_map.at(element);
  if (!reactions) {
    reactions = new CustomElementReactionQueue();
    m_map.insert(TraceWrapperMember<Element>(this, element), reactions);
  }

  reactions->add(reaction);
}

void CustomElementReactionStack::enqueueToBackupQueue(
    Element* element,
    CustomElementReaction* reaction) {
  // https://html.spec.whatwg.org/multipage/scripting.html#backup-element-queue

  DCHECK(!CEReactionsScope::current());
  DCHECK(m_stack.isEmpty());
  DCHECK(isMainThread());

  // If the processing the backup element queue is not set:
  if (!m_backupQueue || m_backupQueue->isEmpty()) {
    Microtask::enqueueMicrotask(
        WTF::bind(&CustomElementReactionStack::invokeBackupQueue,
                  Persistent<CustomElementReactionStack>(this)));
  }

  enqueue(m_backupQueue, element, reaction);
}

void CustomElementReactionStack::clearQueue(Element* element) {
  if (CustomElementReactionQueue* reactions = m_map.at(element))
    reactions->clear();
}

void CustomElementReactionStack::invokeBackupQueue() {
  DCHECK(isMainThread());
  invokeReactions(*m_backupQueue);
  m_backupQueue->clear();
}

CustomElementReactionStack& CustomElementReactionStack::current() {
  return *customElementReactionStack();
}

CustomElementReactionStack*
CustomElementReactionStackTestSupport::setCurrentForTest(
    CustomElementReactionStack* newStack) {
  Persistent<CustomElementReactionStack>& stack = customElementReactionStack();
  CustomElementReactionStack* oldStack = stack.get();
  stack = newStack;
  return oldStack;
}

}  // namespace blink
