// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationAvailability.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/Document.h"
#include "core/events/Event.h"
#include "core/frame/UseCounter.h"
#include "modules/EventTargetModulesNames.h"
#include "modules/presentation/PresentationController.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/presentation/WebPresentationClient.h"
#include "wtf/Vector.h"

namespace blink {

namespace {

WebPresentationClient* presentationClient(ExecutionContext* executionContext) {
  if (!executionContext)
    return nullptr;
  DCHECK(executionContext->isDocument());
  Document* document = toDocument(executionContext);
  if (!document->frame())
    return nullptr;
  PresentationController* controller =
      PresentationController::from(*document->frame());
  return controller ? controller->client() : nullptr;
}

}  // anonymous namespace

// static
PresentationAvailability* PresentationAvailability::take(
    PresentationAvailabilityProperty* resolver,
    const WTF::Vector<KURL>& urls,
    bool value) {
  PresentationAvailability* presentationAvailability =
      new PresentationAvailability(resolver->getExecutionContext(), urls,
                                   value);
  presentationAvailability->suspendIfNeeded();
  presentationAvailability->updateListening();
  return presentationAvailability;
}

PresentationAvailability::PresentationAvailability(
    ExecutionContext* executionContext,
    const WTF::Vector<KURL>& urls,
    bool value)
    : SuspendableObject(executionContext),
      PageVisibilityObserver(toDocument(executionContext)->page()),
      m_urls(urls),
      m_value(value),
      m_state(State::Active) {
  ASSERT(executionContext->isDocument());
  WebVector<WebURL> data(urls.size());
  for (size_t i = 0; i < urls.size(); ++i)
    data[i] = WebURL(urls[i]);

  m_urls.swap(data);
}

PresentationAvailability::~PresentationAvailability() {}

const AtomicString& PresentationAvailability::interfaceName() const {
  return EventTargetNames::PresentationAvailability;
}

ExecutionContext* PresentationAvailability::getExecutionContext() const {
  return SuspendableObject::getExecutionContext();
}

void PresentationAvailability::addedEventListener(
    const AtomicString& eventType,
    RegisteredEventListener& registeredListener) {
  EventTargetWithInlineData::addedEventListener(eventType, registeredListener);
  if (eventType == EventTypeNames::change)
    UseCounter::count(getExecutionContext(),
                      UseCounter::PresentationAvailabilityChangeEventListener);
}

void PresentationAvailability::availabilityChanged(bool value) {
  if (m_value == value)
    return;

  m_value = value;
  dispatchEvent(Event::create(EventTypeNames::change));
}

bool PresentationAvailability::hasPendingActivity() const {
  return m_state != State::Inactive;
}

void PresentationAvailability::resume() {
  setState(State::Active);
}

void PresentationAvailability::suspend() {
  setState(State::Suspended);
}

void PresentationAvailability::contextDestroyed(ExecutionContext*) {
  setState(State::Inactive);
}

void PresentationAvailability::pageVisibilityChanged() {
  if (m_state == State::Inactive)
    return;
  updateListening();
}

void PresentationAvailability::setState(State state) {
  m_state = state;
  updateListening();
}

void PresentationAvailability::updateListening() {
  WebPresentationClient* client = presentationClient(getExecutionContext());
  if (!client)
    return;

  if (m_state == State::Active &&
      (toDocument(getExecutionContext())->pageVisibilityState() ==
       PageVisibilityStateVisible))
    client->startListening(this);
  else
    client->stopListening(this);
}

const WebVector<WebURL>& PresentationAvailability::urls() const {
  return m_urls;
}

bool PresentationAvailability::value() const {
  return m_value;
}

DEFINE_TRACE(PresentationAvailability) {
  EventTargetWithInlineData::trace(visitor);
  PageVisibilityObserver::trace(visitor);
  SuspendableObject::trace(visitor);
}

}  // namespace blink
