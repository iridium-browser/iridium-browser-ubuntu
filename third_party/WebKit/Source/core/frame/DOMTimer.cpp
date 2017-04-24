/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#include "core/frame/DOMTimer.h"

#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/PerformanceMonitor.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "wtf/CurrentTime.h"

namespace blink {

static const int maxIntervalForUserGestureForwarding =
    1000;  // One second matches Gecko.
static const int maxTimerNestingLevel = 5;
static const double oneMillisecond = 0.001;
// Chromium uses a minimum timer interval of 4ms. We'd like to go
// lower; however, there are poorly coded websites out there which do
// create CPU-spinning loops.  Using 4ms prevents the CPU from
// spinning too busily and provides a balance between CPU spinning and
// the smallest possible interval timer.
static const double minimumInterval = 0.004;

static inline bool shouldForwardUserGesture(int interval, int nestingLevel) {
  return UserGestureIndicator::processingUserGestureThreadSafe() &&
         interval <= maxIntervalForUserGestureForwarding &&
         nestingLevel ==
             1;  // Gestures should not be forwarded to nested timers.
}

int DOMTimer::install(ExecutionContext* context,
                      ScheduledAction* action,
                      int timeout,
                      bool singleShot) {
  int timeoutID = context->timers()->installNewTimeout(context, action, timeout,
                                                       singleShot);
  TRACE_EVENT_INSTANT1("devtools.timeline", "TimerInstall",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       InspectorTimerInstallEvent::data(context, timeoutID,
                                                        timeout, singleShot));
  return timeoutID;
}

void DOMTimer::removeByID(ExecutionContext* context, int timeoutID) {
  DOMTimer* timer = context->timers()->removeTimeoutByID(timeoutID);
  TRACE_EVENT_INSTANT1("devtools.timeline", "TimerRemove",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       InspectorTimerRemoveEvent::data(context, timeoutID));
  // Eagerly unregister as ExecutionContext observer.
  if (timer)
    timer->clearContext();
}

DOMTimer::DOMTimer(ExecutionContext* context,
                   ScheduledAction* action,
                   int interval,
                   bool singleShot,
                   int timeoutID)
    : SuspendableTimer(context, TaskType::Timer),
      m_timeoutID(timeoutID),
      m_nestingLevel(context->timers()->timerNestingLevel() + 1),
      m_action(action) {
  ASSERT(timeoutID > 0);
  if (shouldForwardUserGesture(interval, m_nestingLevel)) {
    // Thread safe because shouldForwardUserGesture will only return true if
    // execution is on the the main thread.
    m_userGestureToken = UserGestureIndicator::currentToken();
  }

  double intervalMilliseconds =
      std::max(oneMillisecond, interval * oneMillisecond);
  if (intervalMilliseconds < minimumInterval &&
      m_nestingLevel >= maxTimerNestingLevel)
    intervalMilliseconds = minimumInterval;
  if (singleShot)
    startOneShot(intervalMilliseconds, BLINK_FROM_HERE);
  else
    startRepeating(intervalMilliseconds, BLINK_FROM_HERE);

  suspendIfNeeded();
  probe::asyncTaskScheduledBreakable(
      context, singleShot ? "setTimeout" : "setInterval", this, !singleShot);
}

DOMTimer::~DOMTimer() {
  if (m_action)
    m_action->dispose();
}

void DOMTimer::stop() {
  probe::asyncTaskCanceledBreakable(
      getExecutionContext(),
      repeatInterval() ? "clearInterval" : "clearTimeout", this);

  m_userGestureToken = nullptr;
  // Need to release JS objects potentially protected by ScheduledAction
  // because they can form circular references back to the ExecutionContext
  // which will cause a memory leak.
  if (m_action)
    m_action->dispose();
  m_action = nullptr;
  SuspendableTimer::stop();
}

void DOMTimer::contextDestroyed(ExecutionContext*) {
  stop();
}

void DOMTimer::fired() {
  ExecutionContext* context = getExecutionContext();
  ASSERT(context);
  context->timers()->setTimerNestingLevel(m_nestingLevel);
  DCHECK(!context->isContextSuspended());
  // Only the first execution of a multi-shot timer should get an affirmative
  // user gesture indicator.
  UserGestureIndicator gestureIndicator(std::move(m_userGestureToken));

  TRACE_EVENT1("devtools.timeline", "TimerFire", "data",
               InspectorTimerFireEvent::data(context, m_timeoutID));
  PerformanceMonitor::HandlerCall handlerCall(
      context, repeatInterval() ? "setInterval" : "setTimeout", true);
  probe::AsyncTask asyncTask(context, this, "timerFired");

  // Simple case for non-one-shot timers.
  if (isActive()) {
    if (repeatInterval() && repeatInterval() < minimumInterval) {
      m_nestingLevel++;
      if (m_nestingLevel >= maxTimerNestingLevel)
        augmentRepeatInterval(minimumInterval - repeatInterval());
    }

    // No access to member variables after this point, it can delete the timer.
    m_action->execute(context);

    context->timers()->setTimerNestingLevel(0);

    return;
  }

  // Unregister the timer from ExecutionContext before executing the action
  // for one-shot timers.
  ScheduledAction* action = m_action.release();
  context->timers()->removeTimeoutByID(m_timeoutID);

  action->execute(context);

  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "UpdateCounters", TRACE_EVENT_SCOPE_THREAD, "data",
                       InspectorUpdateCountersEvent::data());

  // ExecutionContext might be already gone when we executed action->execute().
  ExecutionContext* executionContext = getExecutionContext();
  if (!executionContext)
    return;

  executionContext->timers()->setTimerNestingLevel(0);
  // Eagerly unregister as ExecutionContext observer.
  clearContext();
  // Eagerly clear out |action|'s resources.
  action->dispose();
}

RefPtr<WebTaskRunner> DOMTimer::timerTaskRunner() const {
  return getExecutionContext()->timers()->timerTaskRunner();
}

DEFINE_TRACE(DOMTimer) {
  visitor->trace(m_action);
  SuspendableTimer::trace(visitor);
}

}  // namespace blink
