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

#ifndef FrameFetchContext_h
#define FrameFetchContext_h

#include "core/CoreExport.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/loader/LinkLoader.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/FetchContext.h"
#include "platform/loader/fetch/FetchRequest.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/network/ResourceRequest.h"
#include "wtf/Forward.h"

namespace blink {

class ClientHintsPreferences;
class Document;
class DocumentLoader;
class LocalFrameClient;
class LocalFrame;
class ResourceError;
class ResourceResponse;

class CORE_EXPORT FrameFetchContext final : public FetchContext {
 public:
  static ResourceFetcher* createFetcherFromDocumentLoader(
      DocumentLoader* loader) {
    return ResourceFetcher::create(new FrameFetchContext(loader, nullptr));
  }
  static ResourceFetcher* createFetcherFromDocument(Document* document) {
    return ResourceFetcher::create(new FrameFetchContext(nullptr, document));
  }

  static void provideDocumentToContext(FetchContext& context,
                                       Document* document) {
    DCHECK(document);
    CHECK(context.isLiveContext());
    static_cast<FrameFetchContext&>(context).m_document = document;
  }

  ~FrameFetchContext();

  bool isLiveContext() { return true; }

  void addAdditionalRequestHeaders(ResourceRequest&,
                                   FetchResourceType) override;
  CachePolicy getCachePolicy() const override;
  WebCachePolicy resourceRequestCachePolicy(
      ResourceRequest&,
      Resource::Type,
      FetchRequest::DeferOption) const override;
  void dispatchDidChangeResourcePriority(unsigned long identifier,
                                         ResourceLoadPriority,
                                         int intraPriorityValue) override;
  void dispatchWillSendRequest(
      unsigned long identifier,
      ResourceRequest&,
      const ResourceResponse& redirectResponse,
      const FetchInitiatorInfo& = FetchInitiatorInfo()) override;
  void dispatchDidLoadResourceFromMemoryCache(
      unsigned long identifier,
      Resource*,
      WebURLRequest::FrameType,
      WebURLRequest::RequestContext) override;
  void dispatchDidReceiveResponse(unsigned long identifier,
                                  const ResourceResponse&,
                                  WebURLRequest::FrameType,
                                  WebURLRequest::RequestContext,
                                  Resource*) override;
  void dispatchDidReceiveData(unsigned long identifier,
                              const char* data,
                              int dataLength) override;
  void dispatchDidReceiveEncodedData(unsigned long identifier,
                                     int encodedDataLength) override;
  void dispatchDidDownloadData(unsigned long identifier,
                               int dataLength,
                               int encodedDataLength) override;
  void dispatchDidFinishLoading(unsigned long identifier,
                                double finishTime,
                                int64_t encodedDataLength,
                                int64_t decodedBodyLength) override;
  void dispatchDidFail(unsigned long identifier,
                       const ResourceError&,
                       int64_t encodedDataLength,
                       bool isInternalRequest) override;

  bool shouldLoadNewResource(Resource::Type) const override;
  void willStartLoadingResource(unsigned long identifier,
                                ResourceRequest&,
                                Resource::Type,
                                const AtomicString& fetchInitiatorName,
                                V8ActivityLoggingPolicy) override;
  void didLoadResource(Resource*) override;

  void addResourceTiming(const ResourceTimingInfo&) override;
  bool allowImage(bool imagesEnabled, const KURL&) const override;
  ResourceRequestBlockedReason canRequest(
      Resource::Type,
      const ResourceRequest&,
      const KURL&,
      const ResourceLoaderOptions&,
      SecurityViolationReportingPolicy,
      FetchRequest::OriginRestriction) const override;
  ResourceRequestBlockedReason allowResponse(
      Resource::Type,
      const ResourceRequest&,
      const KURL&,
      const ResourceLoaderOptions&) const override;

  bool isControlledByServiceWorker() const override;
  int64_t serviceWorkerID() const override;

  bool isMainFrame() const override;
  bool defersLoading() const override;
  bool isLoadComplete() const override;
  bool pageDismissalEventBeingDispatched() const override;
  bool updateTimingInfoForIFrameNavigation(ResourceTimingInfo*) override;
  void sendImagePing(const KURL&) override;
  void addConsoleMessage(const String&,
                         LogMessageType = LogErrorMessage) const override;
  SecurityOrigin* getSecurityOrigin() const override;

  void populateResourceRequest(Resource::Type,
                               const ClientHintsPreferences&,
                               const FetchRequest::ResourceWidth&,
                               ResourceRequest&) override;
  void setFirstPartyCookieAndRequestorOrigin(ResourceRequest&) override;

  // Exposed for testing.
  void modifyRequestForCSP(ResourceRequest&);
  void addClientHintsIfNecessary(const ClientHintsPreferences&,
                                 const FetchRequest::ResourceWidth&,
                                 ResourceRequest&);

  MHTMLArchive* archive() const override;

  ResourceLoadPriority modifyPriorityForExperiments(
      ResourceLoadPriority) override;

  RefPtr<WebTaskRunner> loadingTaskRunner() const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  FrameFetchContext(DocumentLoader*, Document*);
  inline DocumentLoader* masterDocumentLoader() const;

  LocalFrame* frameOfImportsController() const;
  LocalFrame* frame() const;

  LocalFrameClient* localFrameClient() const;

  void printAccessDeniedMessage(const KURL&) const;
  ResourceRequestBlockedReason canRequestInternal(
      Resource::Type,
      const ResourceRequest&,
      const KURL&,
      const ResourceLoaderOptions&,
      SecurityViolationReportingPolicy,
      FetchRequest::OriginRestriction,
      ResourceRequest::RedirectStatus) const;

  void prepareRequest(ResourceRequest&);

  void dispatchDidReceiveResponseInternal(unsigned long identifier,
                                          const ResourceResponse&,
                                          WebURLRequest::FrameType,
                                          WebURLRequest::RequestContext,
                                          Resource*,
                                          LinkLoader::CanLoadResources);

  void addCSPHeaderIfNecessary(Resource::Type, ResourceRequest&);

  // FIXME: Oilpan: Ideally this should just be a traced Member but that will
  // currently leak because ComputedStyle and its data are not on the heap.
  // See crbug.com/383860 for details.
  WeakMember<Document> m_document;
  Member<DocumentLoader> m_documentLoader;
};

}  // namespace blink

#endif
