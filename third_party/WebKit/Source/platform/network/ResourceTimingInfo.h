/*
 * Copyright (C) 2013 Intel Inc. All rights reserved.
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

#ifndef ResourceTimingInfo_h
#define ResourceTimingInfo_h

#include "platform/CrossThreadCopier.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "wtf/Allocator.h"
#include "wtf/Functional.h"
#include "wtf/Noncopyable.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/AtomicString.h"
#include <memory>

namespace blink {

struct CrossThreadResourceTimingInfoData;

class PLATFORM_EXPORT ResourceTimingInfo {
  USING_FAST_MALLOC(ResourceTimingInfo);
  WTF_MAKE_NONCOPYABLE(ResourceTimingInfo);

 public:
  static std::unique_ptr<ResourceTimingInfo> create(const AtomicString& type,
                                                    const double time,
                                                    bool isMainResource) {
    return WTF::wrapUnique(new ResourceTimingInfo(type, time, isMainResource));
  }
  static std::unique_ptr<ResourceTimingInfo> adopt(
      std::unique_ptr<CrossThreadResourceTimingInfoData>);

  // Gets a copy of the data suitable for passing to another thread.
  std::unique_ptr<CrossThreadResourceTimingInfoData> copyData() const;

  double initialTime() const { return m_initialTime; }
  bool isMainResource() const { return m_isMainResource; }

  void setInitiatorType(const AtomicString& type) { m_type = type; }
  const AtomicString& initiatorType() const { return m_type; }

  void setOriginalTimingAllowOrigin(
      const AtomicString& originalTimingAllowOrigin) {
    m_originalTimingAllowOrigin = originalTimingAllowOrigin;
  }
  const AtomicString& originalTimingAllowOrigin() const {
    return m_originalTimingAllowOrigin;
  }

  void setLoadFinishTime(double time) { m_loadFinishTime = time; }
  double loadFinishTime() const { return m_loadFinishTime; }

  void setInitialURL(const KURL& url) { m_initialURL = url; }
  const KURL& initialURL() const { return m_initialURL; }

  void setFinalResponse(const ResourceResponse& response) {
    m_finalResponse = response;
  }
  const ResourceResponse& finalResponse() const { return m_finalResponse; }

  void addRedirect(const ResourceResponse& redirectResponse, bool crossOrigin);
  const Vector<ResourceResponse>& redirectChain() const {
    return m_redirectChain;
  }

  void addFinalTransferSize(long long encodedDataLength) {
    m_transferSize += encodedDataLength;
  }
  long long transferSize() const { return m_transferSize; }

  void clearLoadTimings() {
    m_finalResponse.setResourceLoadTiming(nullptr);
    for (ResourceResponse& redirect : m_redirectChain)
      redirect.setResourceLoadTiming(nullptr);
  }

 private:
  ResourceTimingInfo(const AtomicString& type,
                     const double time,
                     bool isMainResource)
      : m_type(type),
        m_initialTime(time),
        m_transferSize(0),
        m_isMainResource(isMainResource),
        m_hasCrossOriginRedirect(false) {}

  AtomicString m_type;
  AtomicString m_originalTimingAllowOrigin;
  double m_initialTime;
  double m_loadFinishTime;
  KURL m_initialURL;
  ResourceResponse m_finalResponse;
  Vector<ResourceResponse> m_redirectChain;
  long long m_transferSize;
  bool m_isMainResource;
  bool m_hasCrossOriginRedirect;
};

struct CrossThreadResourceTimingInfoData {
  WTF_MAKE_NONCOPYABLE(CrossThreadResourceTimingInfoData);
  USING_FAST_MALLOC(CrossThreadResourceTimingInfoData);

 public:
  CrossThreadResourceTimingInfoData() {}

  String m_type;
  String m_originalTimingAllowOrigin;
  double m_initialTime;
  double m_loadFinishTime;
  KURL m_initialURL;
  std::unique_ptr<CrossThreadResourceResponseData> m_finalResponse;
  Vector<std::unique_ptr<CrossThreadResourceResponseData>> m_redirectChain;
  long long m_transferSize;
  bool m_isMainResource;
};

template <>
struct CrossThreadCopier<ResourceTimingInfo> {
  typedef WTF::PassedWrapper<std::unique_ptr<CrossThreadResourceTimingInfoData>>
      Type;
  static Type copy(const ResourceTimingInfo& info) {
    return WTF::passed(info.copyData());
  }
};

}  // namespace blink

#endif
