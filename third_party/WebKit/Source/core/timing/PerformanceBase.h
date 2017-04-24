/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Inc. All rights reserved.
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

#ifndef PerformanceBase_h
#define PerformanceBase_h

#include "core/CoreExport.h"
#include "core/dom/DOMHighResTimeStamp.h"
#include "core/events/EventTarget.h"
#include "core/loader/FrameLoaderTypes.h"
#include "core/timing/PerformanceEntry.h"
#include "core/timing/PerformanceNavigationTiming.h"
#include "core/timing/PerformancePaintTiming.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/HashSet.h"
#include "wtf/ListHashSet.h"
#include "wtf/Vector.h"

namespace blink {

class ExceptionState;
class LocalFrame;
class PerformanceObserver;
class PerformanceTiming;
class ResourceResponse;
class ResourceTimingInfo;
class UserTiming;

using PerformanceEntryVector = HeapVector<Member<PerformanceEntry>>;
using PerformanceObservers = HeapListHashSet<Member<PerformanceObserver>>;

class CORE_EXPORT PerformanceBase : public EventTargetWithInlineData {
  friend class PerformanceBaseTest;

 public:
  ~PerformanceBase() override;

  const AtomicString& interfaceName() const override;

  virtual PerformanceTiming* timing() const;

  virtual void updateLongTaskInstrumentation() {}

  // Reduce the resolution to 5µs to prevent timing attacks. See:
  // http://www.w3.org/TR/hr-time-2/#privacy-security
  static double clampTimeResolution(double timeSeconds);

  static DOMHighResTimeStamp monotonicTimeToDOMHighResTimeStamp(
      double timeOrigin,
      double monotonicTime);

  // Translate given platform monotonic time in seconds into a high resolution
  // DOMHighResTimeStamp in milliseconds. The result timestamp is relative to
  // document's time origin and has a time resolution that is safe for
  // exposing to web.
  DOMHighResTimeStamp monotonicTimeToDOMHighResTimeStamp(double) const;
  DOMHighResTimeStamp now() const;

  double timeOrigin() const { return m_timeOrigin; }

  PerformanceEntryVector getEntries() const;
  PerformanceEntryVector getEntriesByType(const String& entryType);
  PerformanceEntryVector getEntriesByName(const String& name,
                                          const String& entryType);

  void clearResourceTimings();
  void setResourceTimingBufferSize(unsigned);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(resourcetimingbufferfull);

  void clearFrameTimings();
  void setFrameTimingBufferSize(unsigned);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(frametimingbufferfull);

  void addLongTaskTiming(double startTime,
                         double endTime,
                         const String& name,
                         const String& culpritFrameSrc,
                         const String& culpritFrameId,
                         const String& culpritFrameName);

  void addResourceTiming(const ResourceTimingInfo&);

  void addNavigationTiming(LocalFrame*);

  void addFirstPaintTiming(double startTime);

  void addFirstContentfulPaintTiming(double startTime);

  void mark(const String& markName, ExceptionState&);
  void clearMarks(const String& markName);

  void measure(const String& measureName,
               const String& startMark,
               const String& endMark,
               ExceptionState&);
  void clearMeasures(const String& measureName);

  void unregisterPerformanceObserver(PerformanceObserver&);
  void registerPerformanceObserver(PerformanceObserver&);
  void updatePerformanceObserverFilterOptions();
  void activateObserver(PerformanceObserver&);
  void resumeSuspendedObservers();

  DECLARE_VIRTUAL_TRACE();

 private:
  static PerformanceNavigationTiming::NavigationType getNavigationType(
      NavigationType,
      const Document*);

  static bool allowsTimingRedirect(const Vector<ResourceResponse>&,
                                   const ResourceResponse&,
                                   const SecurityOrigin&,
                                   ExecutionContext*);

  static bool passesTimingAllowCheck(const ResourceResponse&,
                                     const SecurityOrigin&,
                                     const AtomicString&,
                                     ExecutionContext*);

  void addPaintTiming(PerformancePaintTiming::PaintType, double startTime);

 protected:
  explicit PerformanceBase(double timeOrigin, RefPtr<WebTaskRunner>);

  bool isResourceTimingBufferFull();
  void addResourceTimingBuffer(PerformanceEntry&);

  bool isFrameTimingBufferFull();
  void addFrameTimingBuffer(PerformanceEntry&);

  void notifyObserversOfEntry(PerformanceEntry&);
  bool hasObserverFor(PerformanceEntry::EntryType) const;

  void deliverObservationsTimerFired(TimerBase*);

  PerformanceEntryVector m_frameTimingBuffer;
  unsigned m_frameTimingBufferSize;
  PerformanceEntryVector m_resourceTimingBuffer;
  unsigned m_resourceTimingBufferSize;
  Member<PerformanceEntry> m_navigationTiming;
  Member<UserTiming> m_userTiming;

  double m_timeOrigin;

  PerformanceEntryTypeMask m_observerFilterOptions;
  PerformanceObservers m_observers;
  PerformanceObservers m_activeObservers;
  PerformanceObservers m_suspendedObservers;
  TaskRunnerTimer<PerformanceBase> m_deliverObservationsTimer;
};

}  // namespace blink

#endif  // PerformanceBase_h
