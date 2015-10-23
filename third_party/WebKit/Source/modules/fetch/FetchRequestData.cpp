// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/fetch/FetchRequestData.h"

#include "core/dom/ExecutionContext.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "core/loader/ThreadableLoader.h"
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/DataConsumerTee.h"
#include "modules/fetch/FetchBlobDataConsumerHandle.h"
#include "modules/fetch/FetchHeaderList.h"
#include "platform/network/ResourceRequest.h"
#include "public/platform/WebServiceWorkerRequest.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

FetchRequestData* FetchRequestData::create()
{
    return new FetchRequestData();
}

FetchRequestData* FetchRequestData::create(ExecutionContext* executionContext, const WebServiceWorkerRequest& webRequest)
{
    FetchRequestData* request = FetchRequestData::create();
    request->m_url = webRequest.url();
    request->m_method = webRequest.method();
    for (HTTPHeaderMap::const_iterator it = webRequest.headers().begin(); it != webRequest.headers().end(); ++it)
        request->m_headerList->append(it->key, it->value);
    if (webRequest.blobDataHandle())
        request->setBuffer(new BodyStreamBuffer(FetchBlobDataConsumerHandle::create(executionContext, webRequest.blobDataHandle())));
    request->setContext(webRequest.requestContext());
    request->m_referrer.setURL(webRequest.referrer());
    request->setMode(webRequest.mode());
    request->setCredentials(webRequest.credentialsMode());
    request->setRedirect(webRequest.redirectMode());
    request->setMIMEType(request->m_headerList->extractMIMEType());
    return request;
}

FetchRequestData* FetchRequestData::cloneExceptBody()
{
    FetchRequestData* request = FetchRequestData::create();
    request->m_url = m_url;
    request->m_method = m_method;
    request->m_headerList = m_headerList->clone();
    request->m_unsafeRequestFlag = m_unsafeRequestFlag;
    request->m_origin = m_origin;
    request->m_sameOriginDataURLFlag = m_sameOriginDataURLFlag;
    request->m_context = m_context;
    request->m_referrer = m_referrer;
    request->m_mode = m_mode;
    request->m_credentials = m_credentials;
    request->m_redirect = m_redirect;
    request->m_responseTainting = m_responseTainting;
    request->m_mimeType = m_mimeType;
    request->m_integrity = m_integrity;
    return request;
}

FetchRequestData* FetchRequestData::clone(ExecutionContext* executionContext)
{
    FetchRequestData* request = FetchRequestData::cloneExceptBody();
    if (m_buffer->hasBody()) {
        OwnPtr<FetchDataConsumerHandle> dest1, dest2;
        // TODO(yhirano): unlock the buffer.
        DataConsumerTee::create(executionContext, m_buffer->lock(executionContext), &dest1, &dest2);
        m_buffer = new BodyStreamBuffer(dest1.release());
        request->m_buffer = new BodyStreamBuffer(dest2.release());
    } else {
        m_buffer = new BodyStreamBuffer;
    }
    return request;
}

FetchRequestData* FetchRequestData::pass(ExecutionContext* executionContext)
{
    FetchRequestData* request = FetchRequestData::cloneExceptBody();
    request->m_buffer = m_buffer;
    m_buffer = new BodyStreamBuffer;
    return request;
}

FetchRequestData::~FetchRequestData()
{
}

FetchRequestData::FetchRequestData()
    : m_method("GET")
    , m_headerList(FetchHeaderList::create())
    , m_unsafeRequestFlag(false)
    , m_context(WebURLRequest::RequestContextUnspecified)
    , m_sameOriginDataURLFlag(false)
    , m_mode(WebURLRequest::FetchRequestModeNoCORS)
    , m_credentials(WebURLRequest::FetchCredentialsModeOmit)
    , m_redirect(WebURLRequest::FetchRedirectModeFollow)
    , m_responseTainting(BasicTainting)
    , m_buffer(new BodyStreamBuffer)
{
}

DEFINE_TRACE(FetchRequestData)
{
    visitor->trace(m_buffer);
    visitor->trace(m_headerList);
}

} // namespace blink
