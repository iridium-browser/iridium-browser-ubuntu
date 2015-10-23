// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/fetch/FetchManager.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/fetch/FetchUtils.h"
#include "core/fileapi/Blob.h"
#include "core/frame/Frame.h"
#include "core/frame/SubresourceIntegrity.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/ThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "modules/fetch/Body.h"
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/CompositeDataConsumerHandle.h"
#include "modules/fetch/DataConsumerHandleUtil.h"
#include "modules/fetch/FetchFormDataConsumerHandle.h"
#include "modules/fetch/FetchRequestData.h"
#include "modules/fetch/Response.h"
#include "modules/fetch/ResponseInit.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/HashSet.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace {

bool IsRedirectStatusCode(int statusCode)
{
    return (statusCode == 301 || statusCode == 302 || statusCode == 303 || statusCode == 307 || statusCode == 308);
}

} // namespace

class FetchManager::Loader final : public NoBaseWillBeGarbageCollectedFinalized<FetchManager::Loader>, public ThreadableLoaderClient, public ContextLifecycleObserver {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(FetchManager::Loader);
public:
    static PassOwnPtrWillBeRawPtr<Loader> create(ExecutionContext* executionContext, FetchManager* fetchManager, ScriptPromiseResolver* resolver, FetchRequestData* request)
    {
        return adoptPtrWillBeNoop(new Loader(executionContext, fetchManager, resolver, request));
    }

    ~Loader() override;
    DECLARE_VIRTUAL_TRACE();

    void didReceiveResponse(unsigned long, const ResourceResponse&, PassOwnPtr<WebDataConsumerHandle>) override;
    void didFinishLoading(unsigned long, double) override;
    void didFail(const ResourceError&) override;
    void didFailAccessControlCheck(const ResourceError&) override;
    void didFailRedirectCheck() override;

    void start();
    void dispose();

    class SRIVerifier final : public GarbageCollectedFinalized<SRIVerifier>, public WebDataConsumerHandle::Client {
    public:
        // SRIVerifier takes ownership of |handle| and |response|.
        // |updater| must be garbage collected. The other arguments
        // all must have the lifetime of the give loader.
        SRIVerifier(PassOwnPtr<WebDataConsumerHandle> handle, CompositeDataConsumerHandle::Updater* updater, Response* response, FetchManager::Loader* loader, String integrityMetadata, const KURL& url)
            : m_handle(handle)
            , m_updater(updater)
            , m_response(response)
            , m_loader(loader)
            , m_integrityMetadata(integrityMetadata)
            , m_url(url)
            , m_finished(false)
        {
            m_reader = m_handle->obtainReader(this);
        }

        void didGetReadable() override
        {
            ASSERT(m_reader);
            ASSERT(m_loader);
            ASSERT(m_response);

            WebDataConsumerHandle::Result r = WebDataConsumerHandle::Ok;
            while (r == WebDataConsumerHandle::Ok) {
                const void* buffer;
                size_t size;
                r = m_reader->beginRead(&buffer, WebDataConsumerHandle::FlagNone, &size);
                if (r == WebDataConsumerHandle::Ok) {
                    m_buffer.append(static_cast<const char*>(buffer), size);
                    m_reader->endRead(size);
                }
            }
            if (r == WebDataConsumerHandle::ShouldWait)
                return;
            String errorMessage = "Unknown error occurred while trying to verify integrity.";
            m_finished = true;
            if (r == WebDataConsumerHandle::Done) {
                if (SubresourceIntegrity::CheckSubresourceIntegrity(m_integrityMetadata, String(m_buffer.data(), m_buffer.size()), m_url, *m_loader->document(), errorMessage)) {
                    m_updater->update(FetchFormDataConsumerHandle::create(m_buffer.data(), m_buffer.size()));
                    m_loader->m_resolver->resolve(m_response);
                    m_loader->m_resolver.clear();
                    // FetchManager::Loader::didFinishLoading() can
                    // be called before didGetReadable() is called
                    // when the data is ready. In that case,
                    // didFinishLoading() doesn't clean up and call
                    // notifyFinished(), so it is necessary to
                    // explicitly finish the loader here.
                    if (m_loader->m_didFinishLoading)
                        m_loader->loadSucceeded();
                    return;
                }
            }
            m_updater->update(createUnexpectedErrorDataConsumerHandle());
            m_loader->performNetworkError(errorMessage);
        }

        bool isFinished() const { return m_finished; }

        DEFINE_INLINE_TRACE()
        {
            visitor->trace(m_updater);
            visitor->trace(m_response);
            visitor->trace(m_loader);
        }
    private:
        OwnPtr<WebDataConsumerHandle> m_handle;
        Member<CompositeDataConsumerHandle::Updater> m_updater;
        Member<Response> m_response;
        RawPtrWillBeMember<FetchManager::Loader> m_loader;
        String m_integrityMetadata;
        KURL m_url;
        OwnPtr<WebDataConsumerHandle::Reader> m_reader;
        Vector<char> m_buffer;
        bool m_finished;
    };

private:
    Loader(ExecutionContext*, FetchManager*, ScriptPromiseResolver*, FetchRequestData*);

    void performBasicFetch();
    void performNetworkError(const String& message);
    void performHTTPFetch(bool corsFlag, bool corsPreflightFlag);
    void failed(const String& message);
    void notifyFinished();
    Document* document() const;
    void loadSucceeded();

    RawPtrWillBeMember<FetchManager> m_fetchManager;
    PersistentWillBeMember<ScriptPromiseResolver> m_resolver;
    PersistentWillBeMember<FetchRequestData> m_request;
    RefPtr<ThreadableLoader> m_loader;
    bool m_failed;
    bool m_finished;
    int m_responseHttpStatusCode;
    PersistentWillBeMember<SRIVerifier> m_integrityVerifier;
    bool m_didFinishLoading;
};

FetchManager::Loader::Loader(ExecutionContext* executionContext, FetchManager* fetchManager, ScriptPromiseResolver* resolver, FetchRequestData* request)
    : ContextLifecycleObserver(executionContext)
    , m_fetchManager(fetchManager)
    , m_resolver(resolver)
    , m_request(request)
    , m_failed(false)
    , m_finished(false)
    , m_responseHttpStatusCode(0)
    , m_integrityVerifier(nullptr)
    , m_didFinishLoading(false)
{
}

FetchManager::Loader::~Loader()
{
    ASSERT(!m_loader);
}

DEFINE_TRACE(FetchManager::Loader)
{
    visitor->trace(m_fetchManager);
    visitor->trace(m_resolver);
    visitor->trace(m_request);
    visitor->trace(m_integrityVerifier);
    ContextLifecycleObserver::trace(visitor);
}

void FetchManager::Loader::didReceiveResponse(unsigned long, const ResourceResponse& response, PassOwnPtr<WebDataConsumerHandle> handle)
{
    ASSERT(handle);

    m_responseHttpStatusCode = response.httpStatusCode();

    // Recompute the tainting if the request was redirected to a different
    // origin.
    if (!SecurityOrigin::create(response.url())->isSameSchemeHostPort(m_request->origin().get())) {
        switch (m_request->mode()) {
        case WebURLRequest::FetchRequestModeSameOrigin:
            ASSERT_NOT_REACHED();
            break;
        case WebURLRequest::FetchRequestModeNoCORS:
            m_request->setResponseTainting(FetchRequestData::OpaqueTainting);
            break;
        case WebURLRequest::FetchRequestModeCORS:
        case WebURLRequest::FetchRequestModeCORSWithForcedPreflight:
            m_request->setResponseTainting(FetchRequestData::CORSTainting);
            break;
        }
    }

    FetchResponseData* responseData = nullptr;
    CompositeDataConsumerHandle::Updater* updater = nullptr;
    if (m_request->integrity().isEmpty())
        responseData = FetchResponseData::createWithBuffer(new BodyStreamBuffer(createFetchDataConsumerHandleFromWebHandle(handle)));
    else
        responseData = FetchResponseData::createWithBuffer(new BodyStreamBuffer(createFetchDataConsumerHandleFromWebHandle(CompositeDataConsumerHandle::create(createWaitingDataConsumerHandle(), &updater))));
    responseData->setStatus(response.httpStatusCode());
    responseData->setStatusMessage(response.httpStatusText());
    for (auto& it : response.httpHeaderFields())
        responseData->headerList()->append(it.key, it.value);
    responseData->setURL(response.url());
    responseData->setMIMEType(response.mimeType());

    FetchResponseData* taintedResponse = nullptr;

    if (IsRedirectStatusCode(m_responseHttpStatusCode)) {
        Vector<String> locations;
        responseData->headerList()->getAll("location", locations);
        if (locations.size() > 1) {
            performNetworkError("Multiple Location header.");
            return;
        }
        if (locations.size() == 1) {
            KURL locationURL(m_request->url(), locations[0]);
            if (!locationURL.isValid()) {
                performNetworkError("Invalid Location header.");
                return;
            }
            ASSERT(m_request->redirect() == WebURLRequest::FetchRedirectModeManual);
            taintedResponse = responseData->createOpaqueRedirectFilteredResponse();
        }
        // When the location header doesn't exist, we don't treat the response
        // as a redirect response, and execute tainting.
    }
    if (!taintedResponse) {
        switch (m_request->tainting()) {
        case FetchRequestData::BasicTainting:
            taintedResponse = responseData->createBasicFilteredResponse();
            break;
        case FetchRequestData::CORSTainting:
            taintedResponse = responseData->createCORSFilteredResponse();
            break;
        case FetchRequestData::OpaqueTainting:
            taintedResponse = responseData->createOpaqueFilteredResponse();
            break;
        }
    }

    Response* r = Response::create(m_resolver->executionContext(), taintedResponse);
    r->headers()->setGuard(Headers::ImmutableGuard);

    if (m_request->integrity().isEmpty()) {
        m_resolver->resolve(r);
        m_resolver.clear();
    } else {
        ASSERT(!m_integrityVerifier);
        m_integrityVerifier = new SRIVerifier(handle, updater, r, this, m_request->integrity(), response.url());
    }
}

void FetchManager::Loader::didFinishLoading(unsigned long, double)
{
    m_didFinishLoading = true;
    // If there is an integrity verifier, and it has not already finished, it
    // will take care of finishing the load or performing a network error when
    // verification is complete.
    if (m_integrityVerifier && !m_integrityVerifier->isFinished())
        return;

    loadSucceeded();
}

void FetchManager::Loader::didFail(const ResourceError& error)
{
    if (error.isCancellation() || error.isTimeout() || error.domain() != errorDomainBlinkInternal)
        failed(String());
    else
        failed("Fetch API cannot load " + error.failingURL() + ". " + error.localizedDescription());
}

void FetchManager::Loader::didFailAccessControlCheck(const ResourceError& error)
{
    if (error.isCancellation() || error.isTimeout() || error.domain() != errorDomainBlinkInternal)
        failed(String());
    else
        failed("Fetch API cannot load " + error.failingURL() + ". " + error.localizedDescription());
}

void FetchManager::Loader::didFailRedirectCheck()
{
    failed("Fetch API cannot load " + m_request->url().string() + ". Redirect failed.");
}

Document* FetchManager::Loader::document() const
{
    if (executionContext()->isDocument()) {
        return toDocument(executionContext());
    }
    return nullptr;
}

void FetchManager::Loader::loadSucceeded()
{
    ASSERT(!m_failed);

    m_finished = true;

    if (document() && document()->frame() && document()->frame()->page()
        && m_responseHttpStatusCode >= 200 && m_responseHttpStatusCode < 300) {
        document()->frame()->page()->chromeClient().ajaxSucceeded(document()->frame());
    }
    InspectorInstrumentation::didFinishFetch(executionContext(), this, m_request->method(), m_request->url().string());
    notifyFinished();
}

void FetchManager::Loader::start()
{
    // "1. If |request|'s url contains a Known HSTS Host, modify it per the
    // requirements of the 'URI [sic] Loading and Port Mapping' chapter of HTTP
    // Strict Transport Security."
    // FIXME: Implement this.

    // "2. If |request|'s referrer is not none, set |request|'s referrer to the
    // result of invoking determine |request|'s referrer."
    // We set the referrer using workerGlobalScope's URL in
    // WorkerThreadableLoader.

    // "3. If |request|'s synchronous flag is unset and fetch is not invoked
    // recursively, run the remaining steps asynchronously."
    // We don't support synchronous flag.

    // "4. Let response be the value corresponding to the first matching
    // statement:"

    // "- should fetching |request| be blocked as mixed content returns blocked"
    // We do mixed content checking in ResourceFetcher.

    // "- should fetching |request| be blocked as content security returns
    //    blocked"
    if (!ContentSecurityPolicy::shouldBypassMainWorld(executionContext()) && !executionContext()->contentSecurityPolicy()->allowConnectToSource(m_request->url())) {
        // "A network error."
        performNetworkError("Refused to connect to '" + m_request->url().elidedString() + "' because it violates the document's Content Security Policy.");
        return;
    }

    // "- |request|'s url's origin is |request|'s origin and the |CORS flag| is
    //    unset"
    // "- |request|'s url's scheme is 'data' and |request|'s same-origin data
    //    URL flag is set"
    // "- |request|'s url's scheme is 'about'"
    // Note we don't support to call this method with |CORS flag|.
    if ((SecurityOrigin::create(m_request->url())->isSameSchemeHostPortAndSuborigin(m_request->origin().get()))
        || (m_request->url().protocolIsData() && m_request->sameOriginDataURLFlag())
        || (m_request->url().protocolIsAbout())) {
        // "The result of performing a basic fetch using request."
        performBasicFetch();
        return;
    }

    // "- |request|'s mode is |same-origin|"
    if (m_request->mode() == WebURLRequest::FetchRequestModeSameOrigin) {
        // "A network error."
        performNetworkError("Fetch API cannot load " + m_request->url().string() + ". Request mode is \"same-origin\" but the URL\'s origin is not same as the request origin " + m_request->origin()->toString() + ".");
        return;
    }

    // "- |request|'s mode is |no CORS|"
    if (m_request->mode() == WebURLRequest::FetchRequestModeNoCORS) {
        // "Set |request|'s response tainting to |opaque|."
        m_request->setResponseTainting(FetchRequestData::OpaqueTainting);
        // "The result of performing a basic fetch using |request|."
        performBasicFetch();
        return;
    }

    // "- |request|'s url's scheme is not one of 'http' and 'https'"
    if (!m_request->url().protocolIsInHTTPFamily()) {
        // "A network error."
        performNetworkError("Fetch API cannot load " + m_request->url().string() + ". URL scheme must be \"http\" or \"https\" for CORS request.");
        return;
    }

    // "- |request|'s mode is |CORS-with-forced-preflight|.
    // "- |request|'s unsafe request flag is set and either |request|'s method
    // is not a simple method or a header in |request|'s header list is not a
    // simple header"
    if (m_request->mode() == WebURLRequest::FetchRequestModeCORSWithForcedPreflight
        || (m_request->unsafeRequestFlag()
            && (!FetchUtils::isSimpleMethod(m_request->method())
                || m_request->headerList()->containsNonSimpleHeader()))) {
        // "Set |request|'s response tainting to |CORS|."
        m_request->setResponseTainting(FetchRequestData::CORSTainting);
        // "The result of performing an HTTP fetch using |request| with the
        // |CORS flag| and |CORS preflight flag| set."
        performHTTPFetch(true, true);
        return;
    }

    // "- Otherwise
    //     Set |request|'s response tainting to |CORS|."
    m_request->setResponseTainting(FetchRequestData::CORSTainting);
    // "The result of performing an HTTP fetch using |request| with the
    // |CORS flag| set."
    performHTTPFetch(true, false);
}

void FetchManager::Loader::dispose()
{
    // Prevent notification
    m_fetchManager = nullptr;
    if (m_loader) {
        m_loader->cancel();
        m_loader.clear();
    }
}

void FetchManager::Loader::performBasicFetch()
{
    // "To perform a basic fetch using |request|, switch on |request|'s url's
    // scheme, and run the associated steps:"
    if (m_request->url().protocolIsInHTTPFamily()) {
        // "Return the result of performing an HTTP fetch using |request|."
        performHTTPFetch(false, false);
    } else {
        // FIXME: implement other protocols.
        performNetworkError("Fetch API cannot load " + m_request->url().string() + ". URL scheme \"" + m_request->url().protocol() + "\" is not supported.");
    }
}

void FetchManager::Loader::performNetworkError(const String& message)
{
    failed(message);
}

void FetchManager::Loader::performHTTPFetch(bool corsFlag, bool corsPreflightFlag)
{
    ASSERT(m_request->url().protocolIsInHTTPFamily());
    // CORS preflight fetch procedure is implemented inside DocumentThreadableLoader.

    // "1. Let |HTTPRequest| be a copy of |request|, except that |HTTPRequest|'s
    //  body is a tee of |request|'s body."
    // We use ResourceRequest class for HTTPRequest.
    // FIXME: Support body.
    ResourceRequest request(m_request->url());
    request.setRequestContext(m_request->context());
    request.setHTTPMethod(m_request->method());
    const Vector<OwnPtr<FetchHeaderList::Header>>& list = m_request->headerList()->list();
    for (size_t i = 0; i < list.size(); ++i) {
        request.addHTTPHeaderField(AtomicString(list[i]->first), AtomicString(list[i]->second));
    }

    if (m_request->method() != "GET" && m_request->method() != "HEAD") {
        if (m_request->buffer()->hasBody()) {
            request.setHTTPBody(m_request->buffer()->drainAsFormData());
        }
    }
    request.setFetchRedirectMode(m_request->redirect());
    request.setUseStreamOnResponse(true);

    // "2. Append `Referer`/empty byte sequence, if |HTTPRequest|'s |referrer|
    // is none, and `Referer`/|HTTPRequest|'s referrer, serialized and utf-8
    // encoded, otherwise, to HTTPRequest's header list.
    // We set the referrer using workerGlobalScope's URL in
    // WorkerThreadableLoader.

    // "3. Append `Host`, ..."
    // FIXME: Implement this when the spec is fixed.

    // "4.If |HTTPRequest|'s force Origin header flag is set, append `Origin`/
    // |HTTPRequest|'s origin, serialized and utf-8 encoded, to |HTTPRequest|'s
    // header list."
    // We set Origin header in updateRequestForAccessControl() called from
    // DocumentThreadableLoader::makeCrossOriginAccessRequest

    // "5. Let |credentials flag| be set if either |HTTPRequest|'s credentials
    // mode is |include|, or |HTTPRequest|'s credentials mode is |same-origin|
    // and the |CORS flag| is unset, and unset otherwise.
    ResourceLoaderOptions resourceLoaderOptions;
    resourceLoaderOptions.dataBufferingPolicy = DoNotBufferData;
    if (m_request->credentials() == WebURLRequest::FetchCredentialsModeInclude
        || (m_request->credentials() == WebURLRequest::FetchCredentialsModeSameOrigin && !corsFlag)) {
        resourceLoaderOptions.allowCredentials = AllowStoredCredentials;
    }
    if (m_request->credentials() == WebURLRequest::FetchCredentialsModeInclude)
        resourceLoaderOptions.credentialsRequested = ClientRequestedCredentials;
    resourceLoaderOptions.securityOrigin = m_request->origin().get();

    ThreadableLoaderOptions threadableLoaderOptions;
    threadableLoaderOptions.contentSecurityPolicyEnforcement = ContentSecurityPolicy::shouldBypassMainWorld(executionContext()) ? DoNotEnforceContentSecurityPolicy : EnforceConnectSrcDirective;
    if (corsPreflightFlag)
        threadableLoaderOptions.preflightPolicy = ForcePreflight;
    switch (m_request->mode()) {
    case WebURLRequest::FetchRequestModeSameOrigin:
        threadableLoaderOptions.crossOriginRequestPolicy = DenyCrossOriginRequests;
        break;
    case WebURLRequest::FetchRequestModeNoCORS:
        threadableLoaderOptions.crossOriginRequestPolicy = AllowCrossOriginRequests;
        break;
    case WebURLRequest::FetchRequestModeCORS:
    case WebURLRequest::FetchRequestModeCORSWithForcedPreflight:
        threadableLoaderOptions.crossOriginRequestPolicy = UseAccessControl;
        break;
    }
    InspectorInstrumentation::willStartFetch(executionContext(), this);
    m_loader = ThreadableLoader::create(*executionContext(), this, request, threadableLoaderOptions, resourceLoaderOptions);
    if (!m_loader)
        performNetworkError("Can't create ThreadableLoader");
}

void FetchManager::Loader::failed(const String& message)
{
    if (m_failed || m_finished)
        return;
    m_failed = true;
    if (!message.isEmpty())
        executionContext()->addConsoleMessage(ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, message));
    if (m_resolver) {
        if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
            return;
        ScriptState* state = m_resolver->scriptState();
        ScriptState::Scope scope(state);
        m_resolver->reject(V8ThrowException::createTypeError(state->isolate(), "Failed to fetch"));
    }
    InspectorInstrumentation::didFailFetch(executionContext(), this);
    notifyFinished();
}

void FetchManager::Loader::notifyFinished()
{
    if (m_fetchManager)
        m_fetchManager->onLoaderFinished(this);
}

FetchManager::FetchManager(ExecutionContext* executionContext)
    : m_executionContext(executionContext)
    , m_isStopped(false)
{
}

FetchManager::~FetchManager()
{
#if !ENABLE(OILPAN)
    if (!m_isStopped)
        stop();
#endif
}

ScriptPromise FetchManager::fetch(ScriptState* scriptState, FetchRequestData* request)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    request->setContext(WebURLRequest::RequestContextFetch);

    OwnPtrWillBeRawPtr<Loader> ownLoader = Loader::create(m_executionContext, this, resolver, request);
    Loader* loader = m_loaders.add(ownLoader.release()).storedValue->get();
    loader->start();
    return promise;
}

void FetchManager::stop()
{
    ASSERT(!m_isStopped);
    m_isStopped = true;
    for (auto& loader : m_loaders)
        loader->dispose();
}

void FetchManager::onLoaderFinished(Loader* loader)
{
    // We don't use remove here, because it may cause recursive deletion.
    OwnPtrWillBeRawPtr<Loader> p = m_loaders.take(loader);
    ASSERT(p);
    p->dispose();
}

DEFINE_TRACE(FetchManager)
{
#if ENABLE(OILPAN)
    visitor->trace(m_executionContext);
    visitor->trace(m_loaders);
#endif
}

} // namespace blink
