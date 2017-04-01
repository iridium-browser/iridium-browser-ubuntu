/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "core/events/ScopedEventQueue.h"

#include "core/events/Event.h"
#include "core/events/EventDispatchMediator.h"
#include "core/events/EventDispatcher.h"
#include "core/events/EventTarget.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

ScopedEventQueue* ScopedEventQueue::s_instance = nullptr;

ScopedEventQueue::ScopedEventQueue() : m_scopingLevel(0) {}

ScopedEventQueue::~ScopedEventQueue() {
  DCHECK(!m_scopingLevel);
  DCHECK(!m_queuedEventDispatchMediators.size());
}

void ScopedEventQueue::initialize() {
  DCHECK(!s_instance);
  std::unique_ptr<ScopedEventQueue> instance =
      WTF::wrapUnique(new ScopedEventQueue);
  s_instance = instance.release();
}

void ScopedEventQueue::enqueueEventDispatchMediator(
    EventDispatchMediator* mediator) {
  if (shouldQueueEvents())
    m_queuedEventDispatchMediators.push_back(mediator);
  else
    dispatchEvent(mediator);
}

void ScopedEventQueue::dispatchAllEvents() {
  HeapVector<Member<EventDispatchMediator>> queuedEventDispatchMediators;
  queuedEventDispatchMediators.swap(m_queuedEventDispatchMediators);

  for (auto& mediator : queuedEventDispatchMediators)
    dispatchEvent(mediator.release());
}

void ScopedEventQueue::dispatchEvent(EventDispatchMediator* mediator) const {
  DCHECK(mediator->event().target());
  Node* node = mediator->event().target()->toNode();
  EventDispatcher::dispatchEvent(*node, mediator);
}

ScopedEventQueue* ScopedEventQueue::instance() {
  if (!s_instance)
    initialize();

  return s_instance;
}

void ScopedEventQueue::incrementScopingLevel() {
  m_scopingLevel++;
}

void ScopedEventQueue::decrementScopingLevel() {
  DCHECK(m_scopingLevel);
  m_scopingLevel--;
  if (!m_scopingLevel)
    dispatchAllEvents();
}

}  // namespace blink
