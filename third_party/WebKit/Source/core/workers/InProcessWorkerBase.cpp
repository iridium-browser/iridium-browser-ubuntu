// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/workers/InProcessWorkerBase.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/events/MessageEvent.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/workers/WorkerGlobalScopeProxy.h"
#include "core/workers/WorkerScriptLoader.h"
#include "core/workers/WorkerThread.h"
#include "platform/network/ContentSecurityPolicyResponseHeaders.h"
#include "wtf/MainThread.h"

namespace blink {

InProcessWorkerBase::InProcessWorkerBase(ExecutionContext* context)
    : AbstractWorker(context)
    , m_contextProxy(nullptr)
{
}

InProcessWorkerBase::~InProcessWorkerBase()
{
    ASSERT(isMainThread());
    if (!m_contextProxy)
        return;
    m_contextProxy->workerObjectDestroyed();
}

void InProcessWorkerBase::postMessage(ExecutionContext*, PassRefPtr<SerializedScriptValue> message, const MessagePortArray* ports, ExceptionState& exceptionState)
{
    ASSERT(m_contextProxy);
    // Disentangle the port in preparation for sending it to the remote context.
    OwnPtr<MessagePortChannelArray> channels = MessagePort::disentanglePorts(ports, exceptionState);
    if (exceptionState.hadException())
        return;
    m_contextProxy->postMessageToWorkerGlobalScope(message, channels.release());
}

bool InProcessWorkerBase::initialize(ExecutionContext* context, const String& url, ExceptionState& exceptionState)
{
    suspendIfNeeded();

    KURL scriptURL = resolveURL(url, exceptionState);
    if (scriptURL.isEmpty())
        return false;

    m_scriptLoader = WorkerScriptLoader::create();
    m_scriptLoader->loadAsynchronously(*context, scriptURL, DenyCrossOriginRequests, this);

    m_contextProxy = createWorkerGlobalScopeProxy(context);

    return true;
}

void InProcessWorkerBase::terminate()
{
    if (m_contextProxy)
        m_contextProxy->terminateWorkerGlobalScope();
}

void InProcessWorkerBase::stop()
{
    terminate();
}

bool InProcessWorkerBase::hasPendingActivity() const
{
    // The worker context does not exist while loading, so we must ensure that the worker object is not collected, nor are its event listeners.
    return (m_contextProxy && m_contextProxy->hasPendingActivity()) || m_scriptLoader;
}

PassRefPtr<ContentSecurityPolicy> InProcessWorkerBase::contentSecurityPolicy()
{
    return m_contentSecurityPolicy;
}

void InProcessWorkerBase::didReceiveResponse(unsigned long identifier, const ResourceResponse& response)
{
    if (!response.url().protocolIs("blob") && !response.url().protocolIs("file") && !response.url().protocolIs("filesystem")) {
        m_contentSecurityPolicy = ContentSecurityPolicy::create();
        m_contentSecurityPolicy->setOverrideURLForSelf(response.url());
        m_contentSecurityPolicy->didReceiveHeaders(ContentSecurityPolicyResponseHeaders(response));
    }
    InspectorInstrumentation::didReceiveScriptResponse(executionContext(), identifier);
}

void InProcessWorkerBase::notifyFinished()
{
    if (m_scriptLoader->failed()) {
        dispatchEvent(Event::createCancelable(EventTypeNames::error));
    } else {
        ASSERT(m_contextProxy);
        WorkerThreadStartMode startMode = DontPauseWorkerGlobalScopeOnStart;
        if (InspectorInstrumentation::shouldPauseDedicatedWorkerOnStart(executionContext()))
            startMode = PauseWorkerGlobalScopeOnStart;
        m_contextProxy->startWorkerGlobalScope(m_scriptLoader->url(), executionContext()->userAgent(m_scriptLoader->url()), m_scriptLoader->script(), startMode);
        InspectorInstrumentation::scriptImported(executionContext(), m_scriptLoader->identifier(), m_scriptLoader->script());
    }
    m_scriptLoader = nullptr;
}

DEFINE_TRACE(InProcessWorkerBase)
{
    AbstractWorker::trace(visitor);
}

} // namespace blink
