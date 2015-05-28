/*
 * Copyright (C) 2008, 2010 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
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
 *
 */

#include "config.h"
#include "core/workers/Worker.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/MessageEvent.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/UseCounter.h"
#include "core/workers/WorkerGlobalScopeProxy.h"
#include "core/workers/WorkerGlobalScopeProxyProvider.h"
#include "core/workers/WorkerScriptLoader.h"
#include "core/workers/WorkerThread.h"
#include "wtf/MainThread.h"

namespace blink {

Worker::Worker(ExecutionContext* context)
    : AbstractWorker(context)
    , m_contextProxy(nullptr)
{
}

PassRefPtrWillBeRawPtr<Worker> Worker::create(ExecutionContext* context, const String& url, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    Document* document = toDocument(context);
    UseCounter::count(context, UseCounter::WorkerStart);
    if (!document->page()) {
        exceptionState.throwDOMException(InvalidAccessError, "The context provided is invalid.");
        return nullptr;
    }
    RefPtrWillBeRawPtr<Worker> worker = adoptRefWillBeNoop(new Worker(context));
    if (worker->initialize(context, url, exceptionState))
        return worker.release();
    return nullptr;
}

Worker::~Worker()
{
    ASSERT(isMainThread());
    if (!m_contextProxy)
        return;
    m_contextProxy->workerObjectDestroyed();
}

const AtomicString& Worker::interfaceName() const
{
    return EventTargetNames::Worker;
}

void Worker::postMessage(ExecutionContext*, PassRefPtr<SerializedScriptValue> message, const MessagePortArray* ports, ExceptionState& exceptionState)
{
    ASSERT(m_contextProxy);
    // Disentangle the port in preparation for sending it to the remote context.
    OwnPtr<MessagePortChannelArray> channels = MessagePort::disentanglePorts(ports, exceptionState);
    if (exceptionState.hadException())
        return;
    m_contextProxy->postMessageToWorkerGlobalScope(message, channels.release());
}

bool Worker::initialize(ExecutionContext* context, const String& url, ExceptionState& exceptionState)
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

WorkerGlobalScopeProxy* Worker::createWorkerGlobalScopeProxy(ExecutionContext* context)
{
    Document* document = toDocument(context);
    WorkerGlobalScopeProxyProvider* proxyProvider = WorkerGlobalScopeProxyProvider::from(*document->page());
    ASSERT(proxyProvider);
    return proxyProvider->createWorkerGlobalScopeProxy(this);
}

void Worker::terminate()
{
    if (m_contextProxy)
        m_contextProxy->terminateWorkerGlobalScope();
}

void Worker::stop()
{
    terminate();
}

bool Worker::hasPendingActivity() const
{
    // The worker context does not exist while loading, so we must ensure that the worker object is not collected, nor are its event listeners.
    return (m_contextProxy && m_contextProxy->hasPendingActivity()) || m_scriptLoader;
}

void Worker::didReceiveResponse(unsigned long identifier, const ResourceResponse&)
{
    InspectorInstrumentation::didReceiveScriptResponse(executionContext(), identifier);
}

void Worker::notifyFinished()
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

DEFINE_TRACE(Worker)
{
    AbstractWorker::trace(visitor);
}

} // namespace blink
