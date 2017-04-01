/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 */

#ifndef Timer_h
#define Timer_h

#include "platform/PlatformExport.h"
#include "platform/WebTaskRunner.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/AddressSanitizer.h"
#include "wtf/Allocator.h"
#include "wtf/CurrentTime.h"
#include "wtf/Noncopyable.h"
#include "wtf/Threading.h"
#include "wtf/Vector.h"
#include "wtf/WeakPtr.h"

namespace blink {

// Time intervals are all in seconds.

class PLATFORM_EXPORT TimerBase {
  WTF_MAKE_NONCOPYABLE(TimerBase);

 public:
  explicit TimerBase(RefPtr<WebTaskRunner>);
  virtual ~TimerBase();

  void start(double nextFireInterval,
             double repeatInterval,
             const WebTraceLocation&);

  void startRepeating(double repeatInterval, const WebTraceLocation& caller) {
    start(repeatInterval, repeatInterval, caller);
  }
  void startOneShot(double interval, const WebTraceLocation& caller) {
    start(interval, 0, caller);
  }

  // Timer cancellation is fast enough that you shouldn't have to worry
  // about it unless you're canceling tens of thousands of tasks.
  virtual void stop();
  bool isActive() const;
  const WebTraceLocation& location() const { return m_location; }

  double nextFireInterval() const;
  double repeatInterval() const { return m_repeatInterval; }

  void augmentRepeatInterval(double delta) {
    double now = timerMonotonicallyIncreasingTime();
    setNextFireTime(now, std::max(m_nextFireTime - now + delta, 0.0));
    m_repeatInterval += delta;
  }

  void moveToNewTaskRunner(RefPtr<WebTaskRunner>);

  struct PLATFORM_EXPORT Comparator {
    bool operator()(const TimerBase* a, const TimerBase* b) const;
  };

 protected:
  static RefPtr<WebTaskRunner> getTimerTaskRunner();
  static RefPtr<WebTaskRunner> getUnthrottledTaskRunner();

 private:
  virtual void fired() = 0;

  virtual RefPtr<WebTaskRunner> timerTaskRunner() const;

  NO_SANITIZE_ADDRESS
  virtual bool canFire() const { return true; }

  double timerMonotonicallyIncreasingTime() const;

  void setNextFireTime(double now, double delay);

  void runInternal();

  double m_nextFireTime;    // 0 if inactive
  double m_repeatInterval;  // 0 if not repeating
  WebTraceLocation m_location;
  RefPtr<WebTaskRunner> m_webTaskRunner;

#if DCHECK_IS_ON()
  ThreadIdentifier m_thread;
#endif
  WTF::WeakPtrFactory<TimerBase> m_weakPtrFactory;

  friend class ThreadTimers;
  friend class TimerHeapLessThanFunction;
  friend class TimerHeapReference;
};

template <typename T, bool = IsGarbageCollectedType<T>::value>
class TimerIsObjectAliveTrait {
 public:
  static bool isHeapObjectAlive(T*) { return true; }
};

template <typename T>
class TimerIsObjectAliveTrait<T, true> {
 public:
  static bool isHeapObjectAlive(T* objectPointer) {
    return !ThreadHeap::willObjectBeLazilySwept(objectPointer);
  }
};

template <typename TimerFiredClass>
class TaskRunnerTimer : public TimerBase {
 public:
  using TimerFiredFunction = void (TimerFiredClass::*)(TimerBase*);

  TaskRunnerTimer(RefPtr<WebTaskRunner> webTaskRunner,
                  TimerFiredClass* o,
                  TimerFiredFunction f)
      : TimerBase(std::move(webTaskRunner)), m_object(o), m_function(f) {}

  ~TaskRunnerTimer() override {}

 protected:
  void fired() override { (m_object->*m_function)(this); }

  NO_SANITIZE_ADDRESS
  bool canFire() const override {
    // Oilpan: if a timer fires while Oilpan heaps are being lazily
    // swept, it is not safe to proceed if the object is about to
    // be swept (and this timer will be stopped while doing so.)
    return TimerIsObjectAliveTrait<TimerFiredClass>::isHeapObjectAlive(
        m_object);
  }

 private:
  // FIXME: Oilpan: TimerBase should be moved to the heap and m_object should be
  // traced.  This raw pointer is safe as long as Timer<X> is held by the X
  // itself (That's the case
  // in the current code base).
  GC_PLUGIN_IGNORE("363031")
  TimerFiredClass* m_object;
  TimerFiredFunction m_function;
};

// TODO(dcheng): Consider removing this overload once all timers are using the
// appropriate task runner. https://crbug.com/624694
template <typename TimerFiredClass>
class Timer : public TaskRunnerTimer<TimerFiredClass> {
 public:
  using TimerFiredFunction =
      typename TaskRunnerTimer<TimerFiredClass>::TimerFiredFunction;

  ~Timer() override {}

  Timer(TimerFiredClass* timerFiredClass, TimerFiredFunction timerFiredFunction)
      : TaskRunnerTimer<TimerFiredClass>(TimerBase::getTimerTaskRunner(),
                                         timerFiredClass,
                                         timerFiredFunction) {}
};

// This subclass of Timer posts its tasks on the current thread's default task
// runner.  Tasks posted on there are not throttled when the tab is in the
// background.
//
// DEPRECATED: Use TaskRunnerHelper::get with TaskType::Unthrottled.
template <typename TimerFiredClass>
class UnthrottledThreadTimer : public TaskRunnerTimer<TimerFiredClass> {
 public:
  using TimerFiredFunction =
      typename TaskRunnerTimer<TimerFiredClass>::TimerFiredFunction;

  ~UnthrottledThreadTimer() override {}

  UnthrottledThreadTimer(TimerFiredClass* timerFiredClass,
                         TimerFiredFunction timerFiredFunction)
      : TaskRunnerTimer<TimerFiredClass>(TimerBase::getUnthrottledTaskRunner(),
                                         timerFiredClass,
                                         timerFiredFunction) {}
};

NO_SANITIZE_ADDRESS
inline bool TimerBase::isActive() const {
#if DCHECK_IS_ON()
  DCHECK_EQ(m_thread, currentThread());
#endif
  return m_weakPtrFactory.hasWeakPtrs();
}

}  // namespace blink

#endif  // Timer_h
