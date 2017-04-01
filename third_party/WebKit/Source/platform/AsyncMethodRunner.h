/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AsyncMethodRunner_h
#define AsyncMethodRunner_h

#include "platform/Timer.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

namespace blink {

template <typename TargetClass>
class AsyncMethodRunner final
    : public GarbageCollectedFinalized<AsyncMethodRunner<TargetClass>> {
  WTF_MAKE_NONCOPYABLE(AsyncMethodRunner);

 public:
  typedef void (TargetClass::*TargetMethod)();

  static AsyncMethodRunner* create(TargetClass* object, TargetMethod method) {
    return new AsyncMethodRunner(object, method);
  }

  ~AsyncMethodRunner() {}

  // Schedules to run the method asynchronously. Do nothing if it's already
  // scheduled. If it's suspended, remember to schedule to run the method when
  // resume() is called.
  void runAsync() {
    if (m_suspended) {
      ASSERT(!m_timer.isActive());
      m_runWhenResumed = true;
      return;
    }

    // FIXME: runAsync should take a TraceLocation and pass it to timer here.
    if (!m_timer.isActive())
      m_timer.startOneShot(0, BLINK_FROM_HERE);
  }

  // If it's scheduled to run the method, cancel it and remember to schedule
  // it again when resume() is called. Mainly for implementing
  // SuspendableObject::suspend().
  void suspend() {
    if (m_suspended)
      return;
    m_suspended = true;

    if (!m_timer.isActive())
      return;

    m_timer.stop();
    m_runWhenResumed = true;
  }

  // Resumes pending method run.
  void resume() {
    if (!m_suspended)
      return;
    m_suspended = false;

    if (!m_runWhenResumed)
      return;

    m_runWhenResumed = false;
    // FIXME: resume should take a TraceLocation and pass it to timer here.
    m_timer.startOneShot(0, BLINK_FROM_HERE);
  }

  void stop() {
    if (m_suspended) {
      ASSERT(!m_timer.isActive());
      m_runWhenResumed = false;
      m_suspended = false;
      return;
    }

    ASSERT(!m_runWhenResumed);
    m_timer.stop();
  }

  bool isActive() const { return m_timer.isActive(); }

  DEFINE_INLINE_TRACE() { visitor->trace(m_object); }

 private:
  AsyncMethodRunner(TargetClass* object, TargetMethod method)
      : m_timer(this, &AsyncMethodRunner<TargetClass>::fired),
        m_object(object),
        m_method(method),
        m_suspended(false),
        m_runWhenResumed(false) {}

  void fired(TimerBase*) { (m_object->*m_method)(); }

  Timer<AsyncMethodRunner<TargetClass>> m_timer;

  Member<TargetClass> m_object;
  TargetMethod m_method;

  bool m_suspended;
  bool m_runWhenResumed;
};

}  // namespace blink

#endif
