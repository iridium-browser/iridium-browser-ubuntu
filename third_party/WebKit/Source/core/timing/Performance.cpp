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

#include "config.h"
#include "core/timing/Performance.h"

#include "core/dom/Document.h"
#include "core/events/Event.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/DocumentLoader.h"
#include "core/timing/PerformanceCompositeTiming.h"
#include "core/timing/PerformanceRenderTiming.h"
#include "core/timing/PerformanceResourceTiming.h"
#include "core/timing/PerformanceTiming.h"
#include "core/timing/PerformanceUserTiming.h"
#include "core/timing/ResourceTimingInfo.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/CurrentTime.h"

namespace blink {

static const size_t defaultResourceTimingBufferSize = 150;
static const size_t defaultFrameTimingBufferSize = 150;

Performance::Performance(LocalFrame* frame)
    : DOMWindowProperty(frame)
    , m_frameTimingBufferSize(defaultFrameTimingBufferSize)
    , m_resourceTimingBufferSize(defaultResourceTimingBufferSize)
    , m_referenceTime(frame && frame->host() ? frame->document()->loader()->timing().referenceMonotonicTime() : 0.0)
    , m_userTiming(nullptr)
{
}

Performance::~Performance()
{
}

const AtomicString& Performance::interfaceName() const
{
    return EventTargetNames::Performance;
}

ExecutionContext* Performance::executionContext() const
{
    if (!frame())
        return nullptr;
    return frame()->document();
}

MemoryInfo* Performance::memory()
{
    return MemoryInfo::create();
}

PerformanceNavigation* Performance::navigation() const
{
    if (!m_navigation)
        m_navigation = PerformanceNavigation::create(m_frame);

    return m_navigation.get();
}

PerformanceTiming* Performance::timing() const
{
    if (!m_timing)
        m_timing = PerformanceTiming::create(m_frame);

    return m_timing.get();
}

PerformanceEntryVector Performance::getEntries() const
{
    PerformanceEntryVector entries;

    entries.appendVector(m_resourceTimingBuffer);
    entries.appendVector(m_frameTimingBuffer);

    if (m_userTiming) {
        entries.appendVector(m_userTiming->getMarks());
        entries.appendVector(m_userTiming->getMeasures());
    }

    std::sort(entries.begin(), entries.end(), PerformanceEntry::startTimeCompareLessThan);
    return entries;
}

PerformanceEntryVector Performance::getEntriesByType(const String& entryType)
{
    PerformanceEntryVector entries;

    if (equalIgnoringCase(entryType, "resource")) {
        for (const auto& resource : m_resourceTimingBuffer)
            entries.append(resource);
    }

    if (equalIgnoringCase(entryType, "composite")
        || equalIgnoringCase(entryType, "render")) {
        for (const auto& frame : m_frameTimingBuffer) {
            if (equalIgnoringCase(entryType, frame->entryType())) {
                entries.append(frame);
            }
        }
    }

    if (m_userTiming) {
        if (equalIgnoringCase(entryType, "mark"))
            entries.appendVector(m_userTiming->getMarks());
        else if (equalIgnoringCase(entryType, "measure"))
            entries.appendVector(m_userTiming->getMeasures());
    }

    std::sort(entries.begin(), entries.end(), PerformanceEntry::startTimeCompareLessThan);
    return entries;
}

PerformanceEntryVector Performance::getEntriesByName(const String& name, const String& entryType)
{
    PerformanceEntryVector entries;

    if (entryType.isNull() || equalIgnoringCase(entryType, "resource")) {
        for (const auto& resource : m_resourceTimingBuffer) {
            if (resource->name() == name)
                entries.append(resource);
        }
    }

    if (entryType.isNull() || equalIgnoringCase(entryType, "composite")
        || equalIgnoringCase(entryType, "render")) {
        for (const auto& frame : m_frameTimingBuffer) {
            if (frame->name() == name && (entryType.isNull()
                || equalIgnoringCase(entryType, frame->entryType()))) {
                entries.append(frame);
            }
        }
    }

    if (m_userTiming) {
        if (entryType.isNull() || equalIgnoringCase(entryType, "mark"))
            entries.appendVector(m_userTiming->getMarks(name));
        if (entryType.isNull() || equalIgnoringCase(entryType, "measure"))
            entries.appendVector(m_userTiming->getMeasures(name));
    }

    std::sort(entries.begin(), entries.end(), PerformanceEntry::startTimeCompareLessThan);
    return entries;
}

void Performance::webkitClearResourceTimings()
{
    m_resourceTimingBuffer.clear();
}

void Performance::webkitSetResourceTimingBufferSize(unsigned size)
{
    m_resourceTimingBufferSize = size;
    if (isResourceTimingBufferFull())
        dispatchEvent(Event::create(EventTypeNames::webkitresourcetimingbufferfull));
}

void Performance::clearFrameTimings()
{
    m_frameTimingBuffer.clear();
}

void Performance::setFrameTimingBufferSize(unsigned size)
{
    m_frameTimingBufferSize = size;
    if (isFrameTimingBufferFull())
        dispatchEvent(Event::create(EventTypeNames::frametimingbufferfull));
}

static bool passesTimingAllowCheck(const ResourceResponse& response, Document* requestingDocument, const AtomicString& originalTimingAllowOrigin)
{
    AtomicallyInitializedStaticReference(AtomicString, timingAllowOrigin, new AtomicString("timing-allow-origin"));

    RefPtr<SecurityOrigin> resourceOrigin = SecurityOrigin::create(response.url());
    if (resourceOrigin->isSameSchemeHostPort(requestingDocument->securityOrigin()))
        return true;

    const AtomicString& timingAllowOriginString = originalTimingAllowOrigin.isEmpty() ? response.httpHeaderField(timingAllowOrigin) : originalTimingAllowOrigin;
    if (timingAllowOriginString.isEmpty() || equalIgnoringCase(timingAllowOriginString, "null"))
        return false;

    if (timingAllowOriginString == starAtom)
        return true;

    const String& securityOrigin = requestingDocument->securityOrigin()->toString();
    Vector<String> timingAllowOrigins;
    timingAllowOriginString.string().split(' ', timingAllowOrigins);
    for (const String& allowOrigin : timingAllowOrigins) {
        if (allowOrigin == securityOrigin)
            return true;
    }

    return false;
}

static bool allowsTimingRedirect(const Vector<ResourceResponse>& redirectChain, const ResourceResponse& finalResponse, Document* initiatorDocument)
{
    if (!passesTimingAllowCheck(finalResponse, initiatorDocument, emptyAtom))
        return false;

    for (const ResourceResponse& response : redirectChain) {
        if (!passesTimingAllowCheck(response, initiatorDocument, emptyAtom))
            return false;
    }

    return true;
}

void Performance::addResourceTiming(const ResourceTimingInfo& info, Document* initiatorDocument)
{
    if (isResourceTimingBufferFull())
        return;

    const ResourceResponse& finalResponse = info.finalResponse();
    bool allowTimingDetails = passesTimingAllowCheck(finalResponse, initiatorDocument, info.originalTimingAllowOrigin());
    double startTime = info.initialTime();

    if (info.redirectChain().isEmpty()) {
        PerformanceEntry* entry = PerformanceResourceTiming::create(info, initiatorDocument, startTime, allowTimingDetails);
        addResourceTimingBuffer(entry);
        return;
    }

    const Vector<ResourceResponse>& redirectChain = info.redirectChain();
    bool allowRedirectDetails = allowsTimingRedirect(redirectChain, finalResponse, initiatorDocument);

    if (!allowRedirectDetails) {
        ResourceLoadTiming* finalTiming = finalResponse.resourceLoadTiming();
        ASSERT(finalTiming);
        if (finalTiming)
            startTime = finalTiming->requestTime();
    }

    ResourceLoadTiming* lastRedirectTiming = redirectChain.last().resourceLoadTiming();
    ASSERT(lastRedirectTiming);
    double lastRedirectEndTime = lastRedirectTiming->receiveHeadersEnd();

    PerformanceEntry* entry = PerformanceResourceTiming::create(info, initiatorDocument, startTime, lastRedirectEndTime, allowTimingDetails, allowRedirectDetails);
    addResourceTimingBuffer(entry);
}

void Performance::addResourceTimingBuffer(PerformanceEntry* entry)
{
    m_resourceTimingBuffer.append(entry);

    if (isResourceTimingBufferFull())
        dispatchEvent(Event::create(EventTypeNames::webkitresourcetimingbufferfull));
}

bool Performance::isResourceTimingBufferFull()
{
    return m_resourceTimingBuffer.size() >= m_resourceTimingBufferSize;
}

void Performance::addRenderTiming(Document* initiatorDocument, unsigned sourceFrame, double startTime, double finishTime)
{
    if (isFrameTimingBufferFull())
        return;

    PerformanceEntry* entry = PerformanceRenderTiming::create(initiatorDocument, sourceFrame, startTime, finishTime);
    addFrameTimingBuffer(entry);
}

void Performance::addCompositeTiming(Document* initiatorDocument, unsigned sourceFrame, double startTime)
{
    if (isFrameTimingBufferFull())
        return;

    PerformanceEntry* entry = PerformanceCompositeTiming::create(initiatorDocument, sourceFrame, startTime);
    addFrameTimingBuffer(entry);
}

void Performance::addFrameTimingBuffer(PerformanceEntry* entry)
{
    m_frameTimingBuffer.append(entry);

    if (isFrameTimingBufferFull())
        dispatchEvent(Event::create(EventTypeNames::frametimingbufferfull));
}

bool Performance::isFrameTimingBufferFull()
{
    return m_frameTimingBuffer.size() >= m_frameTimingBufferSize;
}

void Performance::mark(const String& markName, ExceptionState& exceptionState)
{
    if (!m_userTiming)
        m_userTiming = UserTiming::create(this);
    m_userTiming->mark(markName, exceptionState);
}

void Performance::clearMarks(const String& markName)
{
    if (!m_userTiming)
        m_userTiming = UserTiming::create(this);
    m_userTiming->clearMarks(markName);
}

void Performance::measure(const String& measureName, const String& startMark, const String& endMark, ExceptionState& exceptionState)
{
    if (!m_userTiming)
        m_userTiming = UserTiming::create(this);
    m_userTiming->measure(measureName, startMark, endMark, exceptionState);
}

void Performance::clearMeasures(const String& measureName)
{
    if (!m_userTiming)
        m_userTiming = UserTiming::create(this);
    m_userTiming->clearMeasures(measureName);
}

double Performance::now() const
{
    return 1000.0 * (monotonicallyIncreasingTime() - m_referenceTime);
}

DEFINE_TRACE(Performance)
{
    visitor->trace(m_navigation);
    visitor->trace(m_timing);
    visitor->trace(m_frameTimingBuffer);
    visitor->trace(m_resourceTimingBuffer);
    visitor->trace(m_userTiming);
    EventTargetWithInlineData::trace(visitor);
    DOMWindowProperty::trace(visitor);
}

} // namespace blink
