/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef PerformanceResourceTiming_h
#define PerformanceResourceTiming_h

#include "core/dom/DOMHighResTimeStamp.h"
#include "core/timing/PerformanceEntry.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"

namespace blink {

class ResourceLoadTiming;
class ResourceTimingInfo;

class CORE_EXPORT PerformanceResourceTiming : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~PerformanceResourceTiming() override;
  static PerformanceResourceTiming* create(const ResourceTimingInfo& info,
                                           double timeOrigin,
                                           double startTime,
                                           double lastRedirectEndTime,
                                           bool allowTimingDetails,
                                           bool allowRedirectDetails) {
    return new PerformanceResourceTiming(
        info, timeOrigin, startTime, lastRedirectEndTime, allowTimingDetails,
        allowRedirectDetails);
  }

  static PerformanceResourceTiming* create(const ResourceTimingInfo& info,
                                           double timeOrigin,
                                           double startTime,
                                           bool allowTimingDetails) {
    return new PerformanceResourceTiming(info, timeOrigin, startTime, 0.0,
                                         allowTimingDetails, false);
  }

  AtomicString initiatorType() const;

  DOMHighResTimeStamp workerStart() const;
  virtual DOMHighResTimeStamp redirectStart() const;
  virtual DOMHighResTimeStamp redirectEnd() const;
  virtual DOMHighResTimeStamp fetchStart() const;
  DOMHighResTimeStamp domainLookupStart() const;
  DOMHighResTimeStamp domainLookupEnd() const;
  DOMHighResTimeStamp connectStart() const;
  DOMHighResTimeStamp connectEnd() const;
  DOMHighResTimeStamp secureConnectionStart() const;
  DOMHighResTimeStamp requestStart() const;
  DOMHighResTimeStamp responseStart() const;
  virtual DOMHighResTimeStamp responseEnd() const;
  unsigned long long transferSize() const;
  unsigned long long encodedBodySize() const;
  unsigned long long decodedBodySize() const;

 protected:
  void buildJSONValue(V8ObjectBuilder&) const override;

  PerformanceResourceTiming(const AtomicString& initiatorType,
                            double timeOrigin,
                            ResourceLoadTiming*,
                            double lastRedirectEndTime,
                            double finishTime,
                            unsigned long long transferSize,
                            unsigned long long encodedBodyLength,
                            unsigned long long decodedBodyLength,
                            bool didReuseConnection,
                            bool allowTimingDetails,
                            bool allowRedirectDetails,
                            const String& name,
                            const String& entryType,
                            double startTime);

 private:
  PerformanceResourceTiming(const ResourceTimingInfo&,
                            double timeOrigin,
                            double startTime,
                            double lastRedirectEndTime,
                            bool m_allowTimingDetails,
                            bool m_allowRedirectDetails);

  double workerReady() const;

  AtomicString m_initiatorType;
  double m_timeOrigin;
  RefPtr<ResourceLoadTiming> m_timing;
  double m_lastRedirectEndTime;
  double m_finishTime;
  unsigned long long m_transferSize;
  unsigned long long m_encodedBodySize;
  unsigned long long m_decodedBodySize;
  bool m_didReuseConnection;
  bool m_allowTimingDetails;
  bool m_allowRedirectDetails;
};

}  // namespace blink

#endif  // PerformanceResourceTiming_h
