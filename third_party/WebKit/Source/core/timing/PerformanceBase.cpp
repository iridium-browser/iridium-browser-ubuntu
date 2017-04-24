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

#include "core/timing/PerformanceBase.h"

#include "core/dom/Document.h"
#include "core/dom/DocumentTiming.h"
#include "core/events/Event.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentLoader.h"
#include "core/timing/PerformanceLongTaskTiming.h"
#include "core/timing/PerformanceObserver.h"
#include "core/timing/PerformanceResourceTiming.h"
#include "core/timing/PerformanceUserTiming.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/network/ResourceResponse.h"
#include "platform/network/ResourceTimingInfo.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/CurrentTime.h"
#include <algorithm>

namespace blink {

namespace {

SecurityOrigin* getSecurityOrigin(ExecutionContext* context) {
  if (context)
    return context->getSecurityOrigin();
  return nullptr;
}

}  // namespace

using PerformanceObserverVector = HeapVector<Member<PerformanceObserver>>;

static const size_t defaultResourceTimingBufferSize = 150;
static const size_t defaultFrameTimingBufferSize = 150;

PerformanceBase::PerformanceBase(double timeOrigin,
                                 RefPtr<WebTaskRunner> taskRunner)
    : m_frameTimingBufferSize(defaultFrameTimingBufferSize),
      m_resourceTimingBufferSize(defaultResourceTimingBufferSize),
      m_userTiming(nullptr),
      m_timeOrigin(timeOrigin),
      m_observerFilterOptions(PerformanceEntry::Invalid),
      m_deliverObservationsTimer(
          std::move(taskRunner),
          this,
          &PerformanceBase::deliverObservationsTimerFired) {}

PerformanceBase::~PerformanceBase() {}

PerformanceNavigationTiming::NavigationType PerformanceBase::getNavigationType(
    NavigationType type,
    const Document* document) {
  if (document &&
      document->pageVisibilityState() == PageVisibilityStatePrerender) {
    return PerformanceNavigationTiming::NavigationType::Prerender;
  }
  switch (type) {
    case NavigationTypeReload:
      return PerformanceNavigationTiming::NavigationType::Reload;
    case NavigationTypeBackForward:
      return PerformanceNavigationTiming::NavigationType::BackForward;
    case NavigationTypeLinkClicked:
    case NavigationTypeFormSubmitted:
    case NavigationTypeFormResubmitted:
    case NavigationTypeOther:
      return PerformanceNavigationTiming::NavigationType::Navigate;
  }
  NOTREACHED();
  return PerformanceNavigationTiming::NavigationType::Navigate;
}

const AtomicString& PerformanceBase::interfaceName() const {
  return EventTargetNames::Performance;
}

PerformanceTiming* PerformanceBase::timing() const {
  return nullptr;
}

PerformanceEntryVector PerformanceBase::getEntries() const {
  PerformanceEntryVector entries;

  entries.appendVector(m_resourceTimingBuffer);
  if (m_navigationTiming)
    entries.push_back(m_navigationTiming);
  entries.appendVector(m_frameTimingBuffer);

  if (m_userTiming) {
    entries.appendVector(m_userTiming->getMarks());
    entries.appendVector(m_userTiming->getMeasures());
  }

  std::sort(entries.begin(), entries.end(),
            PerformanceEntry::startTimeCompareLessThan);
  return entries;
}

PerformanceEntryVector PerformanceBase::getEntriesByType(
    const String& entryType) {
  PerformanceEntryVector entries;
  PerformanceEntry::EntryType type =
      PerformanceEntry::toEntryTypeEnum(entryType);

  switch (type) {
    case PerformanceEntry::Resource:
      for (const auto& resource : m_resourceTimingBuffer)
        entries.push_back(resource);
      break;
    case PerformanceEntry::Navigation:
      if (m_navigationTiming)
        entries.push_back(m_navigationTiming);
      break;
    case PerformanceEntry::Composite:
    case PerformanceEntry::Render:
      for (const auto& frame : m_frameTimingBuffer) {
        if (type == frame->entryTypeEnum()) {
          entries.push_back(frame);
        }
      }
      break;
    case PerformanceEntry::Mark:
      if (m_userTiming)
        entries.appendVector(m_userTiming->getMarks());
      break;
    case PerformanceEntry::Measure:
      if (m_userTiming)
        entries.appendVector(m_userTiming->getMeasures());
      break;
    // Unsupported for Paint, LongTask, TaskAttribution.
    // Per the spec, these entries can only be accessed via
    // Performance Observer. No separate buffer is maintained.
    case PerformanceEntry::Paint:
      break;
    case PerformanceEntry::LongTask:
      break;
    case PerformanceEntry::TaskAttribution:
      break;
    case PerformanceEntry::Invalid:
      break;
  }

  std::sort(entries.begin(), entries.end(),
            PerformanceEntry::startTimeCompareLessThan);
  return entries;
}

PerformanceEntryVector PerformanceBase::getEntriesByName(
    const String& name,
    const String& entryType) {
  PerformanceEntryVector entries;
  PerformanceEntry::EntryType type =
      PerformanceEntry::toEntryTypeEnum(entryType);

  if (!entryType.isNull() && type == PerformanceEntry::Invalid)
    return entries;

  if (entryType.isNull() || type == PerformanceEntry::Resource) {
    for (const auto& resource : m_resourceTimingBuffer) {
      if (resource->name() == name)
        entries.push_back(resource);
    }
  }

  if (entryType.isNull() || type == PerformanceEntry::Navigation) {
    if (m_navigationTiming && m_navigationTiming->name() == name)
      entries.push_back(m_navigationTiming);
  }

  if (entryType.isNull() || type == PerformanceEntry::Composite ||
      type == PerformanceEntry::Render) {
    for (const auto& frame : m_frameTimingBuffer) {
      if (frame->name() == name &&
          (entryType.isNull() || entryType == frame->entryType()))
        entries.push_back(frame);
    }
  }

  if (m_userTiming) {
    if (entryType.isNull() || type == PerformanceEntry::Mark)
      entries.appendVector(m_userTiming->getMarks(name));
    if (entryType.isNull() || type == PerformanceEntry::Measure)
      entries.appendVector(m_userTiming->getMeasures(name));
  }

  std::sort(entries.begin(), entries.end(),
            PerformanceEntry::startTimeCompareLessThan);
  return entries;
}

void PerformanceBase::clearResourceTimings() {
  m_resourceTimingBuffer.clear();
}

void PerformanceBase::setResourceTimingBufferSize(unsigned size) {
  m_resourceTimingBufferSize = size;
  if (isResourceTimingBufferFull())
    dispatchEvent(Event::create(EventTypeNames::resourcetimingbufferfull));
}

void PerformanceBase::clearFrameTimings() {
  m_frameTimingBuffer.clear();
}

void PerformanceBase::setFrameTimingBufferSize(unsigned size) {
  m_frameTimingBufferSize = size;
  if (isFrameTimingBufferFull())
    dispatchEvent(Event::create(EventTypeNames::frametimingbufferfull));
}

bool PerformanceBase::passesTimingAllowCheck(
    const ResourceResponse& response,
    const SecurityOrigin& initiatorSecurityOrigin,
    const AtomicString& originalTimingAllowOrigin,
    ExecutionContext* context) {
  RefPtr<SecurityOrigin> resourceOrigin =
      SecurityOrigin::create(response.url());
  if (resourceOrigin->isSameSchemeHostPort(&initiatorSecurityOrigin))
    return true;

  const AtomicString& timingAllowOriginString =
      originalTimingAllowOrigin.isEmpty()
          ? response.httpHeaderField(HTTPNames::Timing_Allow_Origin)
          : originalTimingAllowOrigin;
  if (timingAllowOriginString.isEmpty() ||
      equalIgnoringASCIICase(timingAllowOriginString, "null"))
    return false;

  if (timingAllowOriginString == "*") {
    UseCounter::count(context, UseCounter::StarInTimingAllowOrigin);
    return true;
  }

  const String& securityOrigin = initiatorSecurityOrigin.toString();
  Vector<String> timingAllowOrigins;
  timingAllowOriginString.getString().split(' ', timingAllowOrigins);
  if (timingAllowOrigins.size() > 1)
    UseCounter::count(context, UseCounter::MultipleOriginsInTimingAllowOrigin);
  else if (timingAllowOrigins.size() == 1)
    UseCounter::count(context, UseCounter::SingleOriginInTimingAllowOrigin);
  for (const String& allowOrigin : timingAllowOrigins) {
    if (allowOrigin == securityOrigin)
      return true;
  }

  return false;
}

bool PerformanceBase::allowsTimingRedirect(
    const Vector<ResourceResponse>& redirectChain,
    const ResourceResponse& finalResponse,
    const SecurityOrigin& initiatorSecurityOrigin,
    ExecutionContext* context) {
  if (!passesTimingAllowCheck(finalResponse, initiatorSecurityOrigin,
                              AtomicString(), context))
    return false;

  for (const ResourceResponse& response : redirectChain) {
    if (!passesTimingAllowCheck(response, initiatorSecurityOrigin,
                                AtomicString(), context))
      return false;
  }

  return true;
}

void PerformanceBase::addResourceTiming(const ResourceTimingInfo& info) {
  if (isResourceTimingBufferFull() &&
      !hasObserverFor(PerformanceEntry::Resource))
    return;
  ExecutionContext* context = getExecutionContext();
  SecurityOrigin* securityOrigin = getSecurityOrigin(context);
  if (!securityOrigin)
    return;

  const ResourceResponse& finalResponse = info.finalResponse();
  bool allowTimingDetails =
      passesTimingAllowCheck(finalResponse, *securityOrigin,
                             info.originalTimingAllowOrigin(), context);
  double startTime = info.initialTime();

  if (info.redirectChain().isEmpty()) {
    PerformanceEntry* entry = PerformanceResourceTiming::create(
        info, timeOrigin(), startTime, allowTimingDetails);
    notifyObserversOfEntry(*entry);
    if (!isResourceTimingBufferFull())
      addResourceTimingBuffer(*entry);
    return;
  }

  const Vector<ResourceResponse>& redirectChain = info.redirectChain();
  bool allowRedirectDetails = allowsTimingRedirect(redirectChain, finalResponse,
                                                   *securityOrigin, context);

  if (!allowRedirectDetails) {
    ResourceLoadTiming* finalTiming = finalResponse.resourceLoadTiming();
    ASSERT(finalTiming);
    if (finalTiming)
      startTime = finalTiming->requestTime();
  }

  ResourceLoadTiming* lastRedirectTiming =
      redirectChain.back().resourceLoadTiming();
  ASSERT(lastRedirectTiming);
  double lastRedirectEndTime = lastRedirectTiming->receiveHeadersEnd();

  PerformanceEntry* entry = PerformanceResourceTiming::create(
      info, timeOrigin(), startTime, lastRedirectEndTime, allowTimingDetails,
      allowRedirectDetails);
  notifyObserversOfEntry(*entry);
  if (!isResourceTimingBufferFull())
    addResourceTimingBuffer(*entry);
}

void PerformanceBase::addNavigationTiming(LocalFrame* frame) {
  if (!RuntimeEnabledFeatures::performanceNavigationTiming2Enabled())
    return;
  DCHECK(frame);
  const DocumentLoader* documentLoader = frame->loader().documentLoader();
  DCHECK(documentLoader);

  const DocumentLoadTiming& documentLoadTiming = documentLoader->timing();

  const DocumentTiming* documentTiming =
      frame->document() ? &(frame->document()->timing()) : nullptr;

  ResourceTimingInfo* navigationTimingInfo =
      documentLoader->getNavigationTimingInfo();
  if (!navigationTimingInfo)
    return;

  const ResourceResponse& finalResponse = navigationTimingInfo->finalResponse();

  ResourceLoadTiming* resourceLoadTiming = finalResponse.resourceLoadTiming();
  // Don't create a navigation timing instance when
  // resourceLoadTiming is null, which could happen when visiting non-http sites
  // such as about:blank or in some error cases.
  if (!resourceLoadTiming)
    return;
  double lastRedirectEndTime = documentLoadTiming.redirectEnd();
  double finishTime = documentLoadTiming.loadEventEnd();

  ExecutionContext* context = getExecutionContext();
  SecurityOrigin* securityOrigin = getSecurityOrigin(context);
  if (!securityOrigin)
    return;

  bool allowRedirectDetails =
      allowsTimingRedirect(navigationTimingInfo->redirectChain(), finalResponse,
                           *securityOrigin, context);

  unsigned long long transferSize = navigationTimingInfo->transferSize();
  unsigned long long encodedBodyLength = finalResponse.encodedBodyLength();
  unsigned long long decodedBodyLength = finalResponse.decodedBodyLength();
  bool didReuseConnection = finalResponse.connectionReused();
  PerformanceNavigationTiming::NavigationType type =
      getNavigationType(documentLoader->getNavigationType(), frame->document());

  m_navigationTiming = new PerformanceNavigationTiming(
      timeOrigin(), navigationTimingInfo->initialURL().getString(),
      documentLoadTiming.unloadEventStart(),
      documentLoadTiming.unloadEventEnd(), documentLoadTiming.loadEventStart(),
      documentLoadTiming.loadEventEnd(), documentLoadTiming.redirectCount(),
      documentTiming ? documentTiming->domInteractive() : 0,
      documentTiming ? documentTiming->domContentLoadedEventStart() : 0,
      documentTiming ? documentTiming->domContentLoadedEventEnd() : 0,
      documentTiming ? documentTiming->domComplete() : 0, type,
      documentLoadTiming.redirectStart(), documentLoadTiming.redirectEnd(),
      documentLoadTiming.fetchStart(), documentLoadTiming.responseEnd(),
      allowRedirectDetails,
      documentLoadTiming.hasSameOriginAsPreviousDocument(), resourceLoadTiming,
      lastRedirectEndTime, finishTime, transferSize, encodedBodyLength,
      decodedBodyLength, didReuseConnection);
  notifyObserversOfEntry(*m_navigationTiming);
}

void PerformanceBase::addFirstPaintTiming(double startTime) {
  addPaintTiming(PerformancePaintTiming::PaintType::FirstPaint, startTime);
}

void PerformanceBase::addFirstContentfulPaintTiming(double startTime) {
  addPaintTiming(PerformancePaintTiming::PaintType::FirstContentfulPaint,
                 startTime);
}

void PerformanceBase::addPaintTiming(PerformancePaintTiming::PaintType type,
                                     double startTime) {
  if (!RuntimeEnabledFeatures::performancePaintTimingEnabled())
    return;
  PerformanceEntry* entry = new PerformancePaintTiming(
      type, monotonicTimeToDOMHighResTimeStamp(startTime));
  notifyObserversOfEntry(*entry);
}

void PerformanceBase::addResourceTimingBuffer(PerformanceEntry& entry) {
  m_resourceTimingBuffer.push_back(&entry);

  if (isResourceTimingBufferFull())
    dispatchEvent(Event::create(EventTypeNames::resourcetimingbufferfull));
}

bool PerformanceBase::isResourceTimingBufferFull() {
  return m_resourceTimingBuffer.size() >= m_resourceTimingBufferSize;
}

void PerformanceBase::addFrameTimingBuffer(PerformanceEntry& entry) {
  m_frameTimingBuffer.push_back(&entry);

  if (isFrameTimingBufferFull())
    dispatchEvent(Event::create(EventTypeNames::frametimingbufferfull));
}

bool PerformanceBase::isFrameTimingBufferFull() {
  return m_frameTimingBuffer.size() >= m_frameTimingBufferSize;
}

void PerformanceBase::addLongTaskTiming(double startTime,
                                        double endTime,
                                        const String& name,
                                        const String& frameSrc,
                                        const String& frameId,
                                        const String& frameName) {
  if (!hasObserverFor(PerformanceEntry::LongTask))
    return;
  PerformanceEntry* entry = PerformanceLongTaskTiming::create(
      monotonicTimeToDOMHighResTimeStamp(startTime),
      monotonicTimeToDOMHighResTimeStamp(endTime), name, frameSrc, frameId,
      frameName);
  notifyObserversOfEntry(*entry);
}

void PerformanceBase::mark(const String& markName,
                           ExceptionState& exceptionState) {
  if (!m_userTiming)
    m_userTiming = UserTiming::create(*this);
  if (PerformanceEntry* entry = m_userTiming->mark(markName, exceptionState))
    notifyObserversOfEntry(*entry);
}

void PerformanceBase::clearMarks(const String& markName) {
  if (!m_userTiming)
    m_userTiming = UserTiming::create(*this);
  m_userTiming->clearMarks(markName);
}

void PerformanceBase::measure(const String& measureName,
                              const String& startMark,
                              const String& endMark,
                              ExceptionState& exceptionState) {
  if (!m_userTiming)
    m_userTiming = UserTiming::create(*this);
  if (PerformanceEntry* entry = m_userTiming->measure(measureName, startMark,
                                                      endMark, exceptionState))
    notifyObserversOfEntry(*entry);
}

void PerformanceBase::clearMeasures(const String& measureName) {
  if (!m_userTiming)
    m_userTiming = UserTiming::create(*this);
  m_userTiming->clearMeasures(measureName);
}

void PerformanceBase::registerPerformanceObserver(
    PerformanceObserver& observer) {
  m_observerFilterOptions |= observer.filterOptions();
  m_observers.insert(&observer);
  updateLongTaskInstrumentation();
}

void PerformanceBase::unregisterPerformanceObserver(
    PerformanceObserver& oldObserver) {
  ASSERT(isMainThread());
  // Deliver any pending observations on this observer before unregistering.
  if (m_activeObservers.contains(&oldObserver) &&
      !oldObserver.shouldBeSuspended()) {
    oldObserver.deliver();
    m_activeObservers.remove(&oldObserver);
  }
  m_observers.remove(&oldObserver);
  updatePerformanceObserverFilterOptions();
  updateLongTaskInstrumentation();
}

void PerformanceBase::updatePerformanceObserverFilterOptions() {
  m_observerFilterOptions = PerformanceEntry::Invalid;
  for (const auto& observer : m_observers) {
    m_observerFilterOptions |= observer->filterOptions();
  }
  updateLongTaskInstrumentation();
}

void PerformanceBase::notifyObserversOfEntry(PerformanceEntry& entry) {
  for (auto& observer : m_observers) {
    if (observer->filterOptions() & entry.entryTypeEnum())
      observer->enqueuePerformanceEntry(entry);
  }
}

bool PerformanceBase::hasObserverFor(
    PerformanceEntry::EntryType filterType) const {
  return m_observerFilterOptions & filterType;
}

void PerformanceBase::activateObserver(PerformanceObserver& observer) {
  if (m_activeObservers.isEmpty())
    m_deliverObservationsTimer.startOneShot(0, BLINK_FROM_HERE);

  m_activeObservers.insert(&observer);
}

void PerformanceBase::resumeSuspendedObservers() {
  ASSERT(isMainThread());
  if (m_suspendedObservers.isEmpty())
    return;

  PerformanceObserverVector suspended;
  copyToVector(m_suspendedObservers, suspended);
  for (size_t i = 0; i < suspended.size(); ++i) {
    if (!suspended[i]->shouldBeSuspended()) {
      m_suspendedObservers.remove(suspended[i]);
      activateObserver(*suspended[i]);
    }
  }
}

void PerformanceBase::deliverObservationsTimerFired(TimerBase*) {
  ASSERT(isMainThread());
  PerformanceObservers observers;
  m_activeObservers.swap(observers);
  for (const auto& observer : observers) {
    if (observer->shouldBeSuspended())
      m_suspendedObservers.insert(observer);
    else
      observer->deliver();
  }
}

// static
double PerformanceBase::clampTimeResolution(double timeSeconds) {
  const double resolutionSeconds = 0.000005;
  return floor(timeSeconds / resolutionSeconds) * resolutionSeconds;
}

DOMHighResTimeStamp PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
    double timeOrigin,
    double monotonicTime) {
  // Avoid exposing raw platform timestamps.
  if (!monotonicTime || !timeOrigin)
    return 0.0;

  double timeInSeconds = monotonicTime - timeOrigin;
  if (timeInSeconds < 0)
    return 0.0;
  return convertSecondsToDOMHighResTimeStamp(
      clampTimeResolution(timeInSeconds));
}

DOMHighResTimeStamp PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
    double monotonicTime) const {
  return monotonicTimeToDOMHighResTimeStamp(m_timeOrigin, monotonicTime);
}

DOMHighResTimeStamp PerformanceBase::now() const {
  return monotonicTimeToDOMHighResTimeStamp(monotonicallyIncreasingTime());
}

DEFINE_TRACE(PerformanceBase) {
  visitor->trace(m_frameTimingBuffer);
  visitor->trace(m_resourceTimingBuffer);
  visitor->trace(m_navigationTiming);
  visitor->trace(m_userTiming);
  visitor->trace(m_observers);
  visitor->trace(m_activeObservers);
  visitor->trace(m_suspendedObservers);
  EventTargetWithInlineData::trace(visitor);
}

}  // namespace blink
