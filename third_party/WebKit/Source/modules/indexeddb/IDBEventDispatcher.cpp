/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/indexeddb/IDBEventDispatcher.h"

#include "core/frame/UseCounter.h"
#include "modules/EventModules.h"
#include "modules/EventTargetModules.h"

namespace blink {

DispatchEventResult IDBEventDispatcher::dispatch(
    Event* event,
    HeapVector<Member<EventTarget>>& eventTargets) {
  size_t size = eventTargets.size();
  ASSERT(size);

  event->setEventPhase(Event::kCapturingPhase);
  for (size_t i = size - 1; i; --i) {  // Don't do the first element.
    event->setCurrentTarget(eventTargets[i].get());
    eventTargets[i]->fireEventListeners(event);
    if (event->propagationStopped())
      goto doneDispatching;
  }

  event->setEventPhase(Event::kAtTarget);
  event->setCurrentTarget(eventTargets[0].get());
  eventTargets[0]->fireEventListeners(event);
  if (event->propagationStopped() || !event->bubbles())
    goto doneDispatching;
  if (event->bubbles() && event->cancelBubble()) {
    for (size_t i = 1; i < size; ++i) {  // Don't do the first element.
      if (eventTargets[i]->hasEventListeners(event->type()))
        UseCounter::count(eventTargets[i]->getExecutionContext(),
                          UseCounter::EventCancelBubbleAffected);
    }
    goto doneDispatching;
  }

  event->setEventPhase(Event::kBubblingPhase);
  for (size_t i = 1; i < size; ++i) {  // Don't do the first element.
    event->setCurrentTarget(eventTargets[i].get());
    eventTargets[i]->fireEventListeners(event);
    if (event->propagationStopped())
      goto doneDispatching;
    if (event->cancelBubble()) {
      for (size_t j = i + 1; j < size; ++j) {
        if (eventTargets[j]->hasEventListeners(event->type()))
          UseCounter::count(eventTargets[j]->getExecutionContext(),
                            UseCounter::EventCancelBubbleAffected);
      }
      goto doneDispatching;
    }
  }

doneDispatching:
  event->setCurrentTarget(nullptr);
  event->setEventPhase(Event::kNone);
  return EventTarget::dispatchEventResult(*event);
}

}  // namespace blink
