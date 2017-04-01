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

#include "core/workers/WorkerScriptLoader.h"

#include "core/dom/ExecutionContext.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/loader/WorkerThreadableLoader.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/HTTPNames.h"
#include "platform/network/ContentSecurityPolicyResponseHeaders.h"
#include "platform/network/NetworkUtils.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebAddressSpace.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

WorkerScriptLoader::WorkerScriptLoader()
    : m_responseCallback(nullptr),
      m_finishedCallback(nullptr),
      m_requestContext(WebURLRequest::RequestContextWorker),
      m_responseAddressSpace(WebAddressSpacePublic) {}

WorkerScriptLoader::~WorkerScriptLoader() {
  // If |m_threadableLoader| is still working, we have to cancel it here.
  // Otherwise WorkerScriptLoader::didFail() of the deleted |this| will be
  // called from DocumentThreadableLoader::notifyFinished() when the frame
  // will be destroyed.
  if (m_needToCancel)
    cancel();
}

void WorkerScriptLoader::loadSynchronously(
    ExecutionContext& executionContext,
    const KURL& url,
    CrossOriginRequestPolicy crossOriginRequestPolicy,
    WebAddressSpace creationAddressSpace) {
  m_url = url;

  ResourceRequest request(createResourceRequest(creationAddressSpace));
  SECURITY_DCHECK(executionContext.isWorkerGlobalScope());

  ThreadableLoaderOptions options;
  options.crossOriginRequestPolicy = crossOriginRequestPolicy;
  // FIXME: Should we add EnforceScriptSrcDirective here?
  options.contentSecurityPolicyEnforcement = DoNotEnforceContentSecurityPolicy;

  ResourceLoaderOptions resourceLoaderOptions;
  resourceLoaderOptions.allowCredentials = AllowStoredCredentials;

  // TODO(yhirano): Remove this CHECK once https://crbug.com/667254 is fixed.
  CHECK(!m_threadableLoader);
  WorkerThreadableLoader::loadResourceSynchronously(
      toWorkerGlobalScope(executionContext), request, *this, options,
      resourceLoaderOptions);
}

void WorkerScriptLoader::loadAsynchronously(
    ExecutionContext& executionContext,
    const KURL& url,
    CrossOriginRequestPolicy crossOriginRequestPolicy,
    WebAddressSpace creationAddressSpace,
    std::unique_ptr<WTF::Closure> responseCallback,
    std::unique_ptr<WTF::Closure> finishedCallback) {
  DCHECK(responseCallback || finishedCallback);
  m_responseCallback = std::move(responseCallback);
  m_finishedCallback = std::move(finishedCallback);
  m_url = url;

  ResourceRequest request(createResourceRequest(creationAddressSpace));
  ThreadableLoaderOptions options;
  options.crossOriginRequestPolicy = crossOriginRequestPolicy;

  ResourceLoaderOptions resourceLoaderOptions;
  resourceLoaderOptions.allowCredentials = AllowStoredCredentials;

  // During create, callbacks may happen which could remove the last reference
  // to this object, while some of the callchain assumes that the client and
  // loader wouldn't be deleted within callbacks.
  // (E.g. see crbug.com/524694 for why we can't easily remove this protect)
  RefPtr<WorkerScriptLoader> protect(this);
  m_needToCancel = true;
  // TODO(yhirano): Remove this CHECK once https://crbug.com/667254 is fixed.
  CHECK(!m_threadableLoader);
  m_threadableLoader = ThreadableLoader::create(
      executionContext, this, options, resourceLoaderOptions,
      ThreadableLoader::ClientSpec::kWorkerScriptLoader);
  m_threadableLoader->start(request);
  if (m_failed)
    notifyFinished();
}

const KURL& WorkerScriptLoader::responseURL() const {
  DCHECK(!failed());
  return m_responseURL;
}

ResourceRequest WorkerScriptLoader::createResourceRequest(
    WebAddressSpace creationAddressSpace) {
  ResourceRequest request(m_url);
  request.setHTTPMethod(HTTPNames::GET);
  request.setRequestContext(m_requestContext);
  request.setExternalRequestStateFromRequestorAddressSpace(
      creationAddressSpace);
  return request;
}

void WorkerScriptLoader::didReceiveResponse(
    unsigned long identifier,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(!handle);
  if (response.httpStatusCode() / 100 != 2 && response.httpStatusCode()) {
    notifyError();
    return;
  }
  m_identifier = identifier;
  m_responseURL = response.url();
  m_responseEncoding = response.textEncodingName();
  m_appCacheID = response.appCacheID();

  m_referrerPolicy = response.httpHeaderField(HTTPNames::Referrer_Policy);
  processContentSecurityPolicy(response);
  m_originTrialTokens = OriginTrialContext::parseHeaderValue(
      response.httpHeaderField(HTTPNames::Origin_Trial));

  if (NetworkUtils::isReservedIPAddress(response.remoteIPAddress())) {
    m_responseAddressSpace =
        SecurityOrigin::create(m_responseURL)->isLocalhost()
            ? WebAddressSpaceLocal
            : WebAddressSpacePrivate;
  }

  if (m_responseCallback)
    (*m_responseCallback)();
}

void WorkerScriptLoader::didReceiveData(const char* data, unsigned len) {
  if (m_failed)
    return;

  if (!m_decoder) {
    if (!m_responseEncoding.isEmpty())
      m_decoder =
          TextResourceDecoder::create("text/javascript", m_responseEncoding);
    else
      m_decoder = TextResourceDecoder::create("text/javascript", "UTF-8");
  }

  if (!len)
    return;

  m_script.append(m_decoder->decode(data, len));
}

void WorkerScriptLoader::didReceiveCachedMetadata(const char* data, int size) {
  m_cachedMetadata = WTF::makeUnique<Vector<char>>(size);
  memcpy(m_cachedMetadata->data(), data, size);
}

void WorkerScriptLoader::didFinishLoading(unsigned long identifier, double) {
  m_needToCancel = false;
  if (!m_failed && m_decoder)
    m_script.append(m_decoder->flush());

  notifyFinished();
}

void WorkerScriptLoader::didFail(const ResourceError& error) {
  m_needToCancel = false;
  m_canceled = error.isCancellation();
  notifyError();
}

void WorkerScriptLoader::didFailRedirectCheck() {
  // When didFailRedirectCheck() is called, the ResourceLoader for the script
  // is not canceled yet. So we don't reset |m_needToCancel| here.
  notifyError();
}

void WorkerScriptLoader::cancel() {
  m_needToCancel = false;
  if (m_threadableLoader)
    m_threadableLoader->cancel();
}

String WorkerScriptLoader::script() {
  return m_script.toString();
}

void WorkerScriptLoader::notifyError() {
  m_failed = true;
  // notifyError() could be called before ThreadableLoader::create() returns
  // e.g. from didFail(), and in that case m_threadableLoader is not yet set
  // (i.e. still null).
  // Since the callback invocation in notifyFinished() potentially delete
  // |this| object, the callback invocation should be postponed until the
  // create() call returns. See loadAsynchronously() for the postponed call.
  if (m_threadableLoader)
    notifyFinished();
}

void WorkerScriptLoader::notifyFinished() {
  if (!m_finishedCallback)
    return;

  std::unique_ptr<WTF::Closure> callback = std::move(m_finishedCallback);
  (*callback)();
}

void WorkerScriptLoader::processContentSecurityPolicy(
    const ResourceResponse& response) {
  // Per http://www.w3.org/TR/CSP2/#processing-model-workers, if the Worker's
  // URL is not a GUID, then it grabs its CSP from the response headers
  // directly.  Otherwise, the Worker inherits the policy from the parent
  // document (which is implemented in WorkerMessagingProxy, and
  // m_contentSecurityPolicy should be left as nullptr to inherit the policy).
  if (!response.url().protocolIs("blob") &&
      !response.url().protocolIs("file") &&
      !response.url().protocolIs("filesystem")) {
    m_contentSecurityPolicy = ContentSecurityPolicy::create();
    m_contentSecurityPolicy->setOverrideURLForSelf(response.url());
    m_contentSecurityPolicy->didReceiveHeaders(
        ContentSecurityPolicyResponseHeaders(response));
  }
}

}  // namespace blink
