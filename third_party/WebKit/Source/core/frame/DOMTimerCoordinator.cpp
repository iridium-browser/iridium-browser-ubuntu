// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/DOMTimerCoordinator.h"

#include "core/dom/ExecutionContext.h"
#include "core/frame/DOMTimer.h"
#include <algorithm>
#include <memory>

namespace blink {

DOMTimerCoordinator::DOMTimerCoordinator(RefPtr<WebTaskRunner> timerTaskRunner)
    : m_circularSequentialID(0),
      m_timerNestingLevel(0),
      m_timerTaskRunner(std::move(timerTaskRunner)) {}

int DOMTimerCoordinator::installNewTimeout(ExecutionContext* context,
                                           ScheduledAction* action,
                                           int timeout,
                                           bool singleShot) {
  // FIXME: DOMTimers depends heavily on ExecutionContext. Decouple them.
  ASSERT(context->timers() == this);
  int timeoutID = nextID();
  m_timers.insert(timeoutID, DOMTimer::create(context, action, timeout,
                                              singleShot, timeoutID));
  return timeoutID;
}

DOMTimer* DOMTimerCoordinator::removeTimeoutByID(int timeoutID) {
  if (timeoutID <= 0)
    return nullptr;

  DOMTimer* removedTimer = m_timers.take(timeoutID);
  if (removedTimer)
    removedTimer->stop();

  return removedTimer;
}

bool DOMTimerCoordinator::hasInstalledTimeout() const {
  return !m_timers.isEmpty();
}

DEFINE_TRACE(DOMTimerCoordinator) {
  visitor->trace(m_timers);
}

int DOMTimerCoordinator::nextID() {
  while (true) {
    ++m_circularSequentialID;

    if (m_circularSequentialID <= 0)
      m_circularSequentialID = 1;

    if (!m_timers.contains(m_circularSequentialID))
      return m_circularSequentialID;
  }
}

void DOMTimerCoordinator::setTimerTaskRunner(
    RefPtr<WebTaskRunner> timerTaskRunner) {
  m_timerTaskRunner = std::move(timerTaskRunner);
}

}  // namespace blink
