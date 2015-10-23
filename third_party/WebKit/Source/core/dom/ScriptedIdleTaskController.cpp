// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/ScriptedIdleTaskController.h"

#include "core/dom/ExecutionContext.h"
#include "core/dom/IdleRequestCallback.h"
#include "core/loader/DocumentLoadTiming.h"
#include "platform/Logging.h"
#include "platform/TraceEvent.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/CurrentTime.h"
#include "wtf/Functional.h"

namespace blink {

namespace internal {

class IdleRequestCallbackWrapper : public RefCounted<IdleRequestCallbackWrapper> {
public:
    static PassRefPtr<IdleRequestCallbackWrapper> create(ScriptedIdleTaskController::CallbackId id, PassRefPtrWillBeRawPtr<ScriptedIdleTaskController> controller)
    {
        return adoptRef(new IdleRequestCallbackWrapper(id, controller));
    }
    virtual ~IdleRequestCallbackWrapper()
    {
    }

    static void idleTaskFired(PassRefPtr<IdleRequestCallbackWrapper> callbackWrapper, double deadlineSeconds)
    {
        // TODO(rmcilroy): Implement clamping of deadline in some form.
        callbackWrapper->controller()->callbackFired(callbackWrapper->id(), deadlineSeconds, IdleCallbackDeadline::CallbackType::CalledWhenIdle);
    }

    static void timeoutFired(PassRefPtr<IdleRequestCallbackWrapper> callbackWrapper)
    {
        callbackWrapper->controller()->callbackFired(callbackWrapper->id(), monotonicallyIncreasingTime(), IdleCallbackDeadline::CallbackType::CalledByTimeout);
    }

    ScriptedIdleTaskController::CallbackId id() const { return m_id; }
    PassRefPtrWillBeRawPtr<ScriptedIdleTaskController> controller() const { return m_controller; }

private:
    explicit IdleRequestCallbackWrapper(ScriptedIdleTaskController::CallbackId id, PassRefPtrWillBeRawPtr<ScriptedIdleTaskController> controller)
        : m_id(id)
        , m_controller(controller)
    {
    }

    ScriptedIdleTaskController::CallbackId m_id;
    RefPtrWillBePersistent<ScriptedIdleTaskController> m_controller;
};

} // namespace internal

ScriptedIdleTaskController::ScriptedIdleTaskController(ExecutionContext* context, const DocumentLoadTiming& timing)
    : ActiveDOMObject(context)
    , m_timing(timing)
    , m_scheduler(Platform::current()->currentThread()->scheduler())
    , m_nextCallbackId(0)
    , m_suspended(false)
{
    suspendIfNeeded();
}

ScriptedIdleTaskController::~ScriptedIdleTaskController()
{
}

DEFINE_TRACE(ScriptedIdleTaskController)
{
    visitor->trace(m_callbacks);
    ActiveDOMObject::trace(visitor);
}

ScriptedIdleTaskController::CallbackId ScriptedIdleTaskController::registerCallback(IdleRequestCallback* callback, double timeoutMillis)
{
    CallbackId id = ++m_nextCallbackId;
    m_callbacks.set(id, callback);

    RefPtr<internal::IdleRequestCallbackWrapper> callbackWrapper = internal::IdleRequestCallbackWrapper::create(id, this);
    m_scheduler->postIdleTask(FROM_HERE, WTF::bind<double>(&internal::IdleRequestCallbackWrapper::idleTaskFired, callbackWrapper));
    if (timeoutMillis > 0)
        m_scheduler->postTimerTask(FROM_HERE, WTF::bind(&internal::IdleRequestCallbackWrapper::timeoutFired, callbackWrapper), static_cast<long long>(timeoutMillis));

    // TODO(rmcilroy): Add devtools tracing.
    return id;
}

void ScriptedIdleTaskController::cancelCallback(CallbackId id)
{
    // TODO(rmcilroy): Add devtools tracing.
    m_callbacks.remove(id);
}

void ScriptedIdleTaskController::callbackFired(CallbackId id, double deadlineSeconds, IdleCallbackDeadline::CallbackType callbackType)
{
    if (!m_callbacks.contains(id))
        return;

    if (m_suspended) {
        if (callbackType == IdleCallbackDeadline::CallbackType::CalledByTimeout) {
            // Queue for execution when we are resumed.
            m_pendingTimeouts.append(id);
        }
        // Just drop callbacks called while suspended, these will be reposted on the idle task queue when we are resumed.
        return;
    }

    double deadlineMillis = 1000.0 * m_timing.monotonicTimeToZeroBasedDocumentTime(deadlineSeconds);
    runCallback(id, deadlineMillis, callbackType);
}

void ScriptedIdleTaskController::runCallback(CallbackId id, double deadlineMillis, IdleCallbackDeadline::CallbackType callbackType)
{
    ASSERT(!m_suspended);
    auto callback = m_callbacks.take(id);
    if (!callback)
        return;

    // TODO(rmcilroy): Add devtools tracing.
    callback->handleEvent(IdleCallbackDeadline::create(deadlineMillis, callbackType, m_timing));
}

void ScriptedIdleTaskController::stop()
{
    m_callbacks.clear();
}

void ScriptedIdleTaskController::suspend()
{
    m_suspended = true;
}

void ScriptedIdleTaskController::resume()
{
    ASSERT(m_suspended);
    m_suspended = false;

    // Run any pending timeouts.
    Vector<CallbackId> pendingTimeouts;
    m_pendingTimeouts.swap(pendingTimeouts);
    for (auto& id : pendingTimeouts)
        runCallback(id, monotonicallyIncreasingTime(), IdleCallbackDeadline::CallbackType::CalledByTimeout);

    // Repost idle tasks for any remaining callbacks.
    for (auto& callback : m_callbacks) {
        RefPtr<internal::IdleRequestCallbackWrapper> callbackWrapper = internal::IdleRequestCallbackWrapper::create(callback.key, this);
        m_scheduler->postIdleTask(FROM_HERE, WTF::bind<double>(&internal::IdleRequestCallbackWrapper::idleTaskFired, callbackWrapper));
    }
}

bool ScriptedIdleTaskController::hasPendingActivity() const
{
    return !m_callbacks.isEmpty();
}

} // namespace blink
