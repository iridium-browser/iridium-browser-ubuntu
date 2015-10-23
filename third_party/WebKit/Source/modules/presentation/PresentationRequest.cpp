// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/presentation/PresentationRequest.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/UseCounter.h"
#include "modules/EventTargetModules.h"
#include "modules/presentation/PresentationAvailability.h"
#include "modules/presentation/PresentationAvailabilityCallbacks.h"
#include "modules/presentation/PresentationController.h"
#include "modules/presentation/PresentationError.h"
#include "modules/presentation/PresentationSession.h"
#include "modules/presentation/PresentationSessionCallbacks.h"
#include "platform/UserGestureIndicator.h"

namespace blink {

namespace {

// TODO(mlamouri): refactor in one common place.
WebPresentationClient* presentationClient(ExecutionContext* executionContext)
{
    ASSERT(executionContext && executionContext->isDocument());

    Document* document = toDocument(executionContext);
    if (!document->frame())
        return nullptr;
    PresentationController* controller = PresentationController::from(*document->frame());
    return controller ? controller->client() : nullptr;
}

} // anonymous namespace

// static
PresentationRequest* PresentationRequest::create(ExecutionContext* executionContext, const String& url, ExceptionState& exceptionState)
{
    KURL parsedUrl = KURL(executionContext->url(), url);
    if (!parsedUrl.isValid() || parsedUrl.protocolIsAbout()) {
        exceptionState.throwTypeError("'" + url + "' can't be resolved to a valid URL.");
        return nullptr;
    }

    PresentationRequest* request = new PresentationRequest(executionContext, parsedUrl);
    request->suspendIfNeeded();
    return request;
}

const AtomicString& PresentationRequest::interfaceName() const
{
    return EventTargetNames::PresentationRequest;
}

ExecutionContext* PresentationRequest::executionContext() const
{
    return ActiveDOMObject::executionContext();
}

bool PresentationRequest::addEventListener(const AtomicString& eventType, PassRefPtrWillBeRawPtr<EventListener> listener, bool capture)
{
    if (eventType == EventTypeNames::sessionconnect)
        UseCounter::count(executionContext(), UseCounter::PresentationRequestSessionConnectEventListener);

    return EventTarget::addEventListener(eventType, listener, capture);
}

bool PresentationRequest::hasPendingActivity() const
{
    // Prevents garbage collecting of this object when not hold by another
    // object but still has listeners registered.
    return hasEventListeners();
}

ScriptPromise PresentationRequest::start(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    if (!UserGestureIndicator::processingUserGesture()) {
        resolver->reject(DOMException::create(InvalidAccessError, "PresentationRequest::start() requires user gesture."));
        return promise;
    }

    WebPresentationClient* client = presentationClient(executionContext());
    if (!client) {
        resolver->reject(DOMException::create(InvalidStateError, "The PresentationRequest is no longer associated to a frame."));
        return promise;
    }
    client->startSession(m_url.string(), new PresentationSessionCallbacks(resolver, this));

    return promise;
}

ScriptPromise PresentationRequest::join(ScriptState* scriptState, const String& id)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    WebPresentationClient* client = presentationClient(executionContext());
    if (!client) {
        resolver->reject(DOMException::create(InvalidStateError, "The PresentationRequest is no longer associated to a frame."));
        return promise;
    }
    client->joinSession(m_url.string(), id, new PresentationSessionCallbacks(resolver, this));

    return promise;
}

ScriptPromise PresentationRequest::getAvailability(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    WebPresentationClient* client = presentationClient(executionContext());
    if (!client) {
        resolver->reject(DOMException::create(InvalidStateError, "The object is no longer associated to a frame."));
        return promise;
    }
    client->getAvailability(m_url.string(), new PresentationAvailabilityCallbacks(resolver, m_url));
    return promise;
}

const KURL& PresentationRequest::url() const
{
    return m_url;
}

DEFINE_TRACE(PresentationRequest)
{
    RefCountedGarbageCollectedEventTargetWithInlineData<PresentationRequest>::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

PresentationRequest::PresentationRequest(ExecutionContext* executionContext, const KURL& url)
    : ActiveDOMObject(executionContext)
    , m_url(url)
{
}

} // namespace blink
