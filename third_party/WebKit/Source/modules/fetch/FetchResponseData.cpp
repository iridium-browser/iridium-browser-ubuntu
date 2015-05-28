// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/fetch/FetchResponseData.h"

#include "core/dom/DOMArrayBuffer.h"
#include "core/fetch/CrossOriginAccessControl.h"
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/FetchHeaderList.h"
#include "public/platform/WebServiceWorkerResponse.h"

namespace blink {

namespace {

class BranchCanceller : public BodyStreamBuffer::Canceller {
public:
    static void create(BodyStreamBuffer* buffer, BranchCanceller** canceller1, BranchCanceller** canceller2)
    {
        auto context = new Context(buffer);
        *canceller1 = new BranchCanceller(context, First);
        *canceller2 = new BranchCanceller(context, Second);
    }

    void setBuffer(BodyStreamBuffer* buffer) { m_buffer = buffer; }

    void cancel() override
    {
        if (m_tag == First) {
            m_context->isFirstCancelled = true;
        } else {
            ASSERT(m_tag == Second);
            m_context->isSecondCancelled = true;
        }
        ASSERT(m_buffer);
        ASSERT(!m_buffer->isClosed());
        ASSERT(!m_buffer->hasError());
        m_buffer->close();
        if (m_context->isFirstCancelled && m_context->isSecondCancelled)
            m_context->buffer->cancel();
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_context);
        visitor->trace(m_buffer);
        BodyStreamBuffer::Canceller::trace(visitor);
    }

private:
    enum Tag {
        First,
        Second,
    };
    class Context : public GarbageCollected<Context> {
    public:
        explicit Context(BodyStreamBuffer* buffer)
            : buffer(buffer)
            , isFirstCancelled(false)
            , isSecondCancelled(false) { }

        DEFINE_INLINE_VIRTUAL_TRACE()
        {
            visitor->trace(buffer);
        }

        Member<BodyStreamBuffer> buffer;
        bool isFirstCancelled;
        bool isSecondCancelled;
    };

    BranchCanceller(Context* context, Tag tag) : m_context(context), m_tag(tag) { }
    Member<Context> m_context;
    Member<BodyStreamBuffer> m_buffer;
    Tag m_tag;
};

WebServiceWorkerResponseType fetchTypeToWebType(FetchResponseData::Type fetchType)
{
    WebServiceWorkerResponseType webType = WebServiceWorkerResponseTypeDefault;
    switch (fetchType) {
    case FetchResponseData::BasicType:
        webType = WebServiceWorkerResponseTypeBasic;
        break;
    case FetchResponseData::CORSType:
        webType = WebServiceWorkerResponseTypeCORS;
        break;
    case FetchResponseData::DefaultType:
        webType = WebServiceWorkerResponseTypeDefault;
        break;
    case FetchResponseData::ErrorType:
        webType = WebServiceWorkerResponseTypeError;
        break;
    case FetchResponseData::OpaqueType:
        webType = WebServiceWorkerResponseTypeOpaque;
        break;
    }
    return webType;
}

} // namespace

FetchResponseData* FetchResponseData::create()
{
    // "Unless stated otherwise, a response's url is null, status is 200, status
    // message is `OK`, header list is an empty header list, and body is null."
    return new FetchResponseData(DefaultType, 200, "OK");
}

FetchResponseData* FetchResponseData::createNetworkErrorResponse()
{
    // "A network error is a response whose status is always 0, status message
    // is always the empty byte sequence, header list is aways an empty list,
    // and body is always null."
    return new FetchResponseData(ErrorType, 0, "");
}

FetchResponseData* FetchResponseData::createWithBuffer(BodyStreamBuffer* buffer)
{
    FetchResponseData* response = FetchResponseData::create();
    response->m_buffer = buffer;
    return response;
}

FetchResponseData* FetchResponseData::createBasicFilteredResponse()
{
    // "A basic filtered response is a filtered response whose type is |basic|,
    // header list excludes any headers in internal response's header list whose
    // name is `Set-Cookie` or `Set-Cookie2`."
    FetchResponseData* response = new FetchResponseData(BasicType, m_status, m_statusMessage);
    response->m_url = m_url;
    for (size_t i = 0; i < m_headerList->size(); ++i) {
        const FetchHeaderList::Header* header = m_headerList->list()[i].get();
        if (header->first == "set-cookie" || header->first == "set-cookie2")
            continue;
        response->m_headerList->append(header->first, header->second);
    }
    response->m_blobDataHandle = m_blobDataHandle;
    response->m_buffer = m_buffer;
    response->m_mimeType = m_mimeType;
    response->m_internalResponse = this;
    return response;
}

FetchResponseData* FetchResponseData::createCORSFilteredResponse()
{
    // "A CORS filtered response is a filtered response whose type is |CORS|,
    // header list excludes all headers in internal response's header list,
    // except those whose name is either one of `Cache-Control`,
    // `Content-Language`, `Content-Type`, `Expires`, `Last-Modified`, and
    // `Pragma`, and except those whose name is one of the values resulting from
    // parsing `Access-Control-Expose-Headers` in internal response's header
    // list."
    FetchResponseData* response = new FetchResponseData(CORSType, m_status, m_statusMessage);
    response->m_url = m_url;
    HTTPHeaderSet accessControlExposeHeaderSet;
    String accessControlExposeHeaders;
    if (m_headerList->get("access-control-expose-headers", accessControlExposeHeaders))
        parseAccessControlExposeHeadersAllowList(accessControlExposeHeaders, accessControlExposeHeaderSet);
    for (size_t i = 0; i < m_headerList->size(); ++i) {
        const FetchHeaderList::Header* header = m_headerList->list()[i].get();
        if (!isOnAccessControlResponseHeaderWhitelist(header->first) && !accessControlExposeHeaderSet.contains(header->first))
            continue;
        response->m_headerList->append(header->first, header->second);
    }
    response->m_blobDataHandle = m_blobDataHandle;
    response->m_buffer = m_buffer;
    response->m_mimeType = m_mimeType;
    response->m_internalResponse = this;
    return response;
}

FetchResponseData* FetchResponseData::createOpaqueFilteredResponse()
{
    // "An opaque filtered response is a filtered response whose type is
    // |opaque|, status is 0, status message is the empty byte sequence, header
    // list is an empty list, and body is null."
    FetchResponseData* response = new FetchResponseData(OpaqueType, 0, "");
    response->m_internalResponse = this;
    return response;
}

String FetchResponseData::mimeType() const
{
    return m_mimeType;
}

PassRefPtr<BlobDataHandle> FetchResponseData::internalBlobDataHandle() const
{
    if (m_internalResponse) {
        return m_internalResponse->m_blobDataHandle;
    }
    return m_blobDataHandle;
}

BodyStreamBuffer* FetchResponseData::internalBuffer() const
{
    if (m_internalResponse) {
        return m_internalResponse->m_buffer;
    }
    return m_buffer;
}

String FetchResponseData::internalMIMEType() const
{
    if (m_internalResponse) {
        return m_internalResponse->mimeType();
    }
    return m_mimeType;
}

FetchResponseData* FetchResponseData::clone()
{
    FetchResponseData* newResponse = create();
    newResponse->m_type = m_type;
    if (m_terminationReason) {
        newResponse->m_terminationReason = adoptPtr(new TerminationReason);
        *newResponse->m_terminationReason = *m_terminationReason;
    }
    newResponse->m_url = m_url;
    newResponse->m_status = m_status;
    newResponse->m_statusMessage = m_statusMessage;
    newResponse->m_headerList = m_headerList->clone();
    newResponse->m_blobDataHandle = m_blobDataHandle;
    newResponse->m_mimeType = m_mimeType;

    switch (m_type) {
    case BasicType:
    case CORSType:
        ASSERT(m_internalResponse);
        ASSERT(m_blobDataHandle == m_internalResponse->m_blobDataHandle);
        ASSERT(m_buffer == m_internalResponse->m_buffer);
        ASSERT(m_internalResponse->m_type == DefaultType);
        newResponse->m_internalResponse = m_internalResponse->clone();
        m_buffer = m_internalResponse->m_buffer;
        newResponse->m_buffer = newResponse->m_internalResponse->m_buffer;
        break;
    case DefaultType: {
        ASSERT(!m_internalResponse);
        if (!m_buffer)
            return newResponse;
        BodyStreamBuffer* original = m_buffer;
        BranchCanceller* canceller1 = nullptr;
        BranchCanceller* canceller2 = nullptr;
        BranchCanceller::create(original, &canceller1, &canceller2);
        m_buffer = new BodyStreamBuffer(canceller1);
        newResponse->m_buffer = new BodyStreamBuffer(canceller2);
        canceller1->setBuffer(m_buffer);
        canceller2->setBuffer(newResponse->m_buffer);
        original->startTee(m_buffer, newResponse->m_buffer);
        break;
    }
    case ErrorType:
        ASSERT(!m_internalResponse);
        ASSERT(!m_blobDataHandle);
        ASSERT(!m_buffer);
        break;
    case OpaqueType:
        ASSERT(m_internalResponse);
        ASSERT(!m_blobDataHandle);
        ASSERT(!m_buffer);
        ASSERT(m_internalResponse->m_type == DefaultType);
        newResponse->m_internalResponse = m_internalResponse->clone();
        break;
    }
    return newResponse;
}

void FetchResponseData::populateWebServiceWorkerResponse(WebServiceWorkerResponse& response)
{
    if (m_internalResponse) {
        m_internalResponse->populateWebServiceWorkerResponse(response);
        response.setResponseType(fetchTypeToWebType(m_type));
        return;
    }

    response.setURL(url());
    response.setStatus(status());
    response.setStatusText(statusMessage());
    response.setResponseType(fetchTypeToWebType(m_type));
    for (size_t i = 0; i < headerList()->size(); ++i) {
        const FetchHeaderList::Header* header = headerList()->list()[i].get();
        response.appendHeader(header->first, header->second);
    }
    response.setBlobDataHandle(m_blobDataHandle);
}

FetchResponseData::FetchResponseData(Type type, unsigned short status, AtomicString statusMessage)
    : m_type(type)
    , m_status(status)
    , m_statusMessage(statusMessage)
    , m_headerList(FetchHeaderList::create())
{
}

void FetchResponseData::setBlobDataHandle(PassRefPtr<BlobDataHandle> blobDataHandle)
{
    ASSERT(!m_buffer);
    m_blobDataHandle = blobDataHandle;
}

void FetchResponseData::replaceBodyStreamBuffer(BodyStreamBuffer* buffer)
{
    if (m_type == BasicType || m_type == CORSType) {
        ASSERT(m_internalResponse);
        m_internalResponse->m_blobDataHandle = nullptr;
        m_internalResponse->m_buffer = buffer;
        m_blobDataHandle = nullptr;
        m_buffer = buffer;
    } else if (m_type == DefaultType) {
        ASSERT(!m_internalResponse);
        m_blobDataHandle = nullptr;
        m_buffer = buffer;
    }
}

DEFINE_TRACE(FetchResponseData)
{
    visitor->trace(m_headerList);
    visitor->trace(m_internalResponse);
    visitor->trace(m_buffer);
}

} // namespace blink
