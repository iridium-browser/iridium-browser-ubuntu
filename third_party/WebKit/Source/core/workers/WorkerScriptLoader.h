/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009, 2011 Google Inc. All Rights Reserved.
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
 *
 */

#ifndef WorkerScriptLoader_h
#define WorkerScriptLoader_h

#include "core/CoreExport.h"
#include "core/loader/ThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/FastAllocBase.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

class ResourceRequest;
class ResourceResponse;
class ExecutionContext;
class TextResourceDecoder;
class WorkerScriptLoaderClient;

class CORE_EXPORT WorkerScriptLoader final : public RefCounted<WorkerScriptLoader>, public ThreadableLoaderClient {
    WTF_MAKE_FAST_ALLOCATED(WorkerScriptLoader);
public:
    static PassRefPtr<WorkerScriptLoader> create()
    {
        return adoptRef(new WorkerScriptLoader());
    }

    void loadSynchronously(ExecutionContext&, const KURL&, CrossOriginRequestPolicy);
    void loadAsynchronously(ExecutionContext&, const KURL&, CrossOriginRequestPolicy, WorkerScriptLoaderClient*);

    void notifyError();

    // This will immediately lead to notifyFinished() if loadAsynchronously
    // is in progress.
    void cancel();

    void setClient(WorkerScriptLoaderClient* client) { m_client = client; }

    String script();
    const KURL& url() const { return m_url; }
    const KURL& responseURL() const;
    bool failed() const { return m_failed; }
    unsigned long identifier() const { return m_identifier; }
    PassOwnPtr<Vector<char>> releaseCachedMetadata() { return m_cachedMetadata.release(); }
    const Vector<char>* cachedMetadata() const { return m_cachedMetadata.get(); }

    virtual void didReceiveResponse(unsigned long /*identifier*/, const ResourceResponse&, PassOwnPtr<WebDataConsumerHandle>) override;
    virtual void didReceiveData(const char* data, unsigned dataLength) override;
    virtual void didReceiveCachedMetadata(const char*, int /*dataLength*/) override;
    virtual void didFinishLoading(unsigned long identifier, double) override;
    virtual void didFail(const ResourceError&) override;
    virtual void didFailRedirectCheck() override;

    void setRequestContext(WebURLRequest::RequestContext requestContext) { m_requestContext = requestContext; }

private:
    friend class WTF::RefCounted<WorkerScriptLoader>;

    WorkerScriptLoader();
    virtual ~WorkerScriptLoader();

    PassOwnPtr<ResourceRequest> createResourceRequest();
    void notifyFinished();

    WorkerScriptLoaderClient* m_client;
    RefPtr<ThreadableLoader> m_threadableLoader;
    String m_responseEncoding;
    OwnPtr<TextResourceDecoder> m_decoder;
    StringBuilder m_script;
    KURL m_url;
    KURL m_responseURL;
    bool m_failed;
    unsigned long m_identifier;
    bool m_finishing;
    OwnPtr<Vector<char>> m_cachedMetadata;
    WebURLRequest::RequestContext m_requestContext;
};

} // namespace blink

#endif // WorkerScriptLoader_h
