// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "ServiceWorkerRegistration.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/Event.h"
#include "modules/EventTargetModules.h"
#include "modules/serviceworkers/ServiceWorkerContainerClient.h"
#include "modules/serviceworkers/ServiceWorkerError.h"
#include "public/platform/WebServiceWorkerProvider.h"

namespace blink {

static void deleteIfNoExistingOwner(WebServiceWorker* serviceWorker)
{
    if (serviceWorker && !serviceWorker->proxy())
        delete serviceWorker;
}

const AtomicString& ServiceWorkerRegistration::interfaceName() const
{
    return EventTargetNames::ServiceWorkerRegistration;
}

void ServiceWorkerRegistration::dispatchUpdateFoundEvent()
{
    dispatchEvent(Event::create(EventTypeNames::updatefound));
}

void ServiceWorkerRegistration::setInstalling(WebServiceWorker* serviceWorker)
{
    if (!executionContext()) {
        deleteIfNoExistingOwner(serviceWorker);
        return;
    }
    m_installing = ServiceWorker::from(executionContext(), serviceWorker);
}

void ServiceWorkerRegistration::setWaiting(WebServiceWorker* serviceWorker)
{
    if (!executionContext()) {
        deleteIfNoExistingOwner(serviceWorker);
        return;
    }
    m_waiting = ServiceWorker::from(executionContext(), serviceWorker);
}

void ServiceWorkerRegistration::setActive(WebServiceWorker* serviceWorker)
{
    if (!executionContext()) {
        deleteIfNoExistingOwner(serviceWorker);
        return;
    }
    m_active = ServiceWorker::from(executionContext(), serviceWorker);
}

ServiceWorkerRegistration* ServiceWorkerRegistration::from(ExecutionContext* executionContext, WebType* registration)
{
    if (!registration)
        return 0;
    return getOrCreate(executionContext, registration);
}

ServiceWorkerRegistration* ServiceWorkerRegistration::take(ScriptPromiseResolver* resolver, WebType* registration)
{
    return from(resolver->scriptState()->executionContext(), registration);
}

void ServiceWorkerRegistration::dispose(WebType* registration)
{
    if (registration && !registration->proxy())
        delete registration;
}

String ServiceWorkerRegistration::scope() const
{
    return m_outerRegistration->scope().string();
}

ScriptPromise ServiceWorkerRegistration::unregister(ScriptState* scriptState)
{
    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    if (!m_provider) {
        resolver->reject(DOMException::create(InvalidStateError, "Failed to unregister a ServiceWorkerRegistration: No associated provider is available."));
        return promise;
    }

    RefPtr<SecurityOrigin> documentOrigin = scriptState->executionContext()->securityOrigin();
    KURL scopeURL = scriptState->executionContext()->completeURL(scope());
    scopeURL.removeFragmentIdentifier();
    if (!scope().isEmpty() && !documentOrigin->canRequest(scopeURL)) {
        RefPtr<SecurityOrigin> scopeOrigin = SecurityOrigin::create(scopeURL);
        resolver->reject(DOMException::create(SecurityError, "Failed to unregister a ServiceWorkerRegistration: The origin of the registration's scope ('" + scopeOrigin->toString() + "') does not match the current origin ('" + documentOrigin->toString() + "')."));
        return promise;
    }

    m_provider->unregisterServiceWorker(scopeURL, new CallbackPromiseAdapter<bool, ServiceWorkerError>(resolver));
    return promise;
}

ServiceWorkerRegistration* ServiceWorkerRegistration::getOrCreate(ExecutionContext* executionContext, WebServiceWorkerRegistration* outerRegistration)
{
    if (!outerRegistration)
        return 0;

    ServiceWorkerRegistration* existingRegistration = static_cast<ServiceWorkerRegistration*>(outerRegistration->proxy());
    if (existingRegistration) {
        ASSERT(existingRegistration->executionContext() == executionContext);
        return existingRegistration;
    }

    ServiceWorkerRegistration* registration = new ServiceWorkerRegistration(executionContext, adoptPtr(outerRegistration));
    registration->suspendIfNeeded();
    return registration;
}

ServiceWorkerRegistration::ServiceWorkerRegistration(ExecutionContext* executionContext, PassOwnPtr<WebServiceWorkerRegistration> outerRegistration)
    : ActiveDOMObject(executionContext)
    , m_outerRegistration(outerRegistration)
    , m_provider(0)
    , m_stopped(false)
{
    ASSERT(m_outerRegistration);
    ThreadState::current()->registerPreFinalizer(*this);

    if (!executionContext)
        return;
    if (ServiceWorkerContainerClient* client = ServiceWorkerContainerClient::from(executionContext))
        m_provider = client->provider();
    m_outerRegistration->setProxy(this);
}

ServiceWorkerRegistration::~ServiceWorkerRegistration()
{
    ASSERT(!m_outerRegistration);
}

void ServiceWorkerRegistration::dispose()
{
    // See ServiceWorker::dispose() comment why this explicit dispose() action is needed.
    m_outerRegistration.clear();
}

DEFINE_TRACE(ServiceWorkerRegistration)
{
    visitor->trace(m_installing);
    visitor->trace(m_waiting);
    visitor->trace(m_active);
    RefCountedGarbageCollectedEventTargetWithInlineData<ServiceWorkerRegistration>::trace(visitor);
    HeapSupplementable<ServiceWorkerRegistration>::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

bool ServiceWorkerRegistration::hasPendingActivity() const
{
    return !m_stopped;
}

void ServiceWorkerRegistration::stop()
{
    if (m_stopped)
        return;
    m_stopped = true;
    m_outerRegistration->proxyStopped();
}

} // namespace blink
