// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementReactionQueue.h"

#include "core/dom/Element.h"
#include "core/dom/custom/CustomElementReaction.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

CustomElementReactionQueue::CustomElementReactionQueue() : m_index(0u) {}

CustomElementReactionQueue::~CustomElementReactionQueue() {}

DEFINE_TRACE(CustomElementReactionQueue) {
  visitor->trace(m_reactions);
}

void CustomElementReactionQueue::add(CustomElementReaction* reaction) {
  m_reactions.push_back(reaction);
}

// There is one queue per element, so this could be invoked
// recursively.
void CustomElementReactionQueue::invokeReactions(Element* element) {
  TRACE_EVENT1("blink", "CustomElementReactionQueue::invokeReactions", "name",
               element->localName().utf8());
  while (m_index < m_reactions.size()) {
    CustomElementReaction* reaction = m_reactions[m_index];
    m_reactions[m_index++] = nullptr;
    reaction->invoke(element);
  }
  // Unlike V0CustomElementsCallbackQueue, reactions are always
  // inserted by steps which bump the global element queue. This
  // means we do not need queue "owner" guards.
  // https://html.spec.whatwg.org/multipage/scripting.html#custom-element-reactions
  clear();
}

void CustomElementReactionQueue::clear() {
  m_index = 0;
  m_reactions.resize(0);
}

}  // namespace blink
