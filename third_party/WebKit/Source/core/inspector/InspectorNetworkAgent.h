/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef InspectorNetworkAgent_h
#define InspectorNetworkAgent_h

#include "bindings/core/v8/ScriptString.h"
#include "core/CoreExport.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/protocol/Network.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Document;
class DocumentLoader;
class EncodedFormData;
class ExecutionContext;
struct FetchInitiatorInfo;
class LocalFrame;
class HTTPHeaderMap;
class InspectedFrames;
class KURL;
class NetworkResourcesData;
class Resource;
class ResourceError;
class ResourceResponse;
class ThreadableLoaderClient;
class XHRReplayData;
class XMLHttpRequest;

class WebSocketHandshakeRequest;
class WebSocketHandshakeResponse;

class CORE_EXPORT InspectorNetworkAgent final
    : public InspectorBaseAgent<protocol::Network::Metainfo> {
 public:
  static InspectorNetworkAgent* create(InspectedFrames* inspectedFrames) {
    return new InspectorNetworkAgent(inspectedFrames);
  }

  void restore() override;

  ~InspectorNetworkAgent() override;
  DECLARE_VIRTUAL_TRACE();

  // Called from instrumentation.
  void didBlockRequest(LocalFrame*,
                       const ResourceRequest&,
                       DocumentLoader*,
                       const FetchInitiatorInfo&,
                       ResourceRequestBlockedReason);
  void didChangeResourcePriority(unsigned long identifier,
                                 ResourceLoadPriority);
  void willSendRequest(LocalFrame*,
                       unsigned long identifier,
                       DocumentLoader*,
                       ResourceRequest&,
                       const ResourceResponse& redirectResponse,
                       const FetchInitiatorInfo&);
  void markResourceAsCached(unsigned long identifier);
  void didReceiveResourceResponse(LocalFrame*,
                                  unsigned long identifier,
                                  DocumentLoader*,
                                  const ResourceResponse&,
                                  Resource*);
  void didReceiveData(LocalFrame*,
                      unsigned long identifier,
                      const char* data,
                      int dataLength);
  void didReceiveEncodedDataLength(LocalFrame*,
                                   unsigned long identifier,
                                   int encodedDataLength);
  void didFinishLoading(unsigned long identifier,
                        double monotonicFinishTime,
                        int64_t encodedDataLength);
  void didReceiveCORSRedirectResponse(LocalFrame*,
                                      unsigned long identifier,
                                      DocumentLoader*,
                                      const ResourceResponse&,
                                      Resource*);
  void didFailLoading(unsigned long identifier, const ResourceError&);
  void didCommitLoad(LocalFrame*, DocumentLoader*);
  void scriptImported(unsigned long identifier, const String& sourceString);
  void didReceiveScriptResponse(unsigned long identifier);
  bool shouldForceCORSPreflight();
  bool shouldBlockRequest(const ResourceRequest&);

  void documentThreadableLoaderStartedLoadingForClient(unsigned long identifier,
                                                       ThreadableLoaderClient*);
  void documentThreadableLoaderFailedToStartLoadingForClient(
      ThreadableLoaderClient*);
  void willLoadXHR(XMLHttpRequest*,
                   ThreadableLoaderClient*,
                   const AtomicString& method,
                   const KURL&,
                   bool async,
                   PassRefPtr<EncodedFormData> body,
                   const HTTPHeaderMap& headers,
                   bool includeCrendentials);
  void didFailXHRLoading(ExecutionContext*,
                         XMLHttpRequest*,
                         ThreadableLoaderClient*,
                         const AtomicString&,
                         const String&);
  void didFinishXHRLoading(ExecutionContext*,
                           XMLHttpRequest*,
                           ThreadableLoaderClient*,
                           const AtomicString&,
                           const String&);

  void willStartFetch(ThreadableLoaderClient*);
  void didFailFetch(ThreadableLoaderClient*);
  void didFinishFetch(ExecutionContext*,
                      ThreadableLoaderClient*,
                      const AtomicString& method,
                      const String& url);

  void willSendEventSourceRequest(ThreadableLoaderClient*);
  void willDispatchEventSourceEvent(ThreadableLoaderClient*,
                                    const AtomicString& eventName,
                                    const AtomicString& eventId,
                                    const String& data);
  void didFinishEventSourceRequest(ThreadableLoaderClient*);

  void willDestroyResource(Resource*);

  void applyUserAgentOverride(String* userAgent);

  // FIXME: InspectorNetworkAgent should not be aware of style recalculation.
  void willRecalculateStyle(Document*);
  void didRecalculateStyle();
  void didScheduleStyleRecalculation(Document*);

  void frameScheduledNavigation(LocalFrame*, double);
  void frameClearedScheduledNavigation(LocalFrame*);

  std::unique_ptr<protocol::Network::Initiator> buildInitiatorObject(
      Document*,
      const FetchInitiatorInfo&);

  void didCreateWebSocket(Document*,
                          unsigned long identifier,
                          const KURL& requestURL,
                          const String&);
  void willSendWebSocketHandshakeRequest(Document*,
                                         unsigned long identifier,
                                         const WebSocketHandshakeRequest*);
  void didReceiveWebSocketHandshakeResponse(Document*,
                                            unsigned long identifier,
                                            const WebSocketHandshakeRequest*,
                                            const WebSocketHandshakeResponse*);
  void didCloseWebSocket(Document*, unsigned long identifier);
  void didReceiveWebSocketFrame(unsigned long identifier,
                                int opCode,
                                bool masked,
                                const char* payload,
                                size_t payloadLength);
  void didSendWebSocketFrame(unsigned long identifier,
                             int opCode,
                             bool masked,
                             const char* payload,
                             size_t payloadLength);
  void didReceiveWebSocketFrameError(unsigned long identifier, const String&);

  // Called from frontend
  Response enable(Maybe<int> totalBufferSize,
                  Maybe<int> resourceBufferSize) override;
  Response disable() override;
  Response setUserAgentOverride(const String&) override;
  Response setExtraHTTPHeaders(
      std::unique_ptr<protocol::Network::Headers>) override;
  void getResponseBody(const String& requestId,
                       std::unique_ptr<GetResponseBodyCallback>) override;
  Response addBlockedURL(const String& url) override;
  Response removeBlockedURL(const String& url) override;
  Response replayXHR(const String& requestId) override;
  Response setMonitoringXHREnabled(bool) override;
  Response canClearBrowserCache(bool* result) override;
  Response canClearBrowserCookies(bool* result) override;
  Response emulateNetworkConditions(bool offline,
                                    double latency,
                                    double downloadThroughput,
                                    double uploadThroughput,
                                    Maybe<String> connectionType) override;
  Response setCacheDisabled(bool) override;
  Response setBypassServiceWorker(bool) override;
  Response setDataSizeLimitsForTest(int maxTotalSize,
                                    int maxResourceSize) override;
  Response getCertificate(
      const String& origin,
      std::unique_ptr<protocol::Array<String>>* certificate) override;

  // Called from other agents.
  void setHostId(const String&);
  bool fetchResourceContent(Document*,
                            const KURL&,
                            String* content,
                            bool* base64Encoded);
  bool cacheDisabled();

 private:
  explicit InspectorNetworkAgent(InspectedFrames*);

  void enable(int totalBufferSize, int resourceBufferSize);
  void willSendRequestInternal(LocalFrame*,
                               unsigned long identifier,
                               DocumentLoader*,
                               const ResourceRequest&,
                               const ResourceResponse& redirectResponse,
                               const FetchInitiatorInfo&);
  void delayedRemoveReplayXHR(XMLHttpRequest*);
  void removeFinishedReplayXHRFired(TimerBase*);
  void didFinishXHRInternal(ExecutionContext*,
                            XMLHttpRequest*,
                            ThreadableLoaderClient*,
                            const AtomicString&,
                            const String&,
                            bool);

  bool canGetResponseBodyBlob(const String& requestId);
  void getResponseBodyBlob(const String& requestId,
                           std::unique_ptr<GetResponseBodyCallback>);
  void clearPendingRequestData();

  Member<InspectedFrames> m_inspectedFrames;
  String m_userAgentOverride;
  String m_hostId;
  Member<NetworkResourcesData> m_resourcesData;

  typedef HashMap<ThreadableLoaderClient*, unsigned long>
      ThreadableLoaderClientRequestIdMap;

  // Stores the pending ThreadableLoaderClient till an identifier for
  // the load is generated by the loader and passed to the inspector
  // via the documentThreadableLoaderStartedLoadingForClient() method.
  ThreadableLoaderClient* m_pendingRequest;
  InspectorPageAgent::ResourceType m_pendingRequestType;
  ThreadableLoaderClientRequestIdMap m_knownRequestIdMap;

  Member<XHRReplayData> m_pendingXHRReplayData;

  typedef HashMap<String, std::unique_ptr<protocol::Network::Initiator>>
      FrameNavigationInitiatorMap;
  FrameNavigationInitiatorMap m_frameNavigationInitiatorMap;

  // FIXME: InspectorNetworkAgent should now be aware of style recalculation.
  std::unique_ptr<protocol::Network::Initiator> m_styleRecalculationInitiator;
  bool m_isRecalculatingStyle;

  HeapHashSet<Member<XMLHttpRequest>> m_replayXHRs;
  HeapHashSet<Member<XMLHttpRequest>> m_replayXHRsToBeDeleted;
  Timer<InspectorNetworkAgent> m_removeFinishedReplayXHRTimer;
};

}  // namespace blink

#endif  // !defined(InspectorNetworkAgent_h)
