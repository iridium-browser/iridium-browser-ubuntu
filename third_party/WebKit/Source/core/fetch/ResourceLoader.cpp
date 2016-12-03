/*
 * Copyright (C) 2006, 2007, 2010, 2011 Apple Inc. All rights reserved.
 *           (C) 2007 Graham Dennis (graham.dennis@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/fetch/ResourceLoader.h"

#include "core/fetch/CSSStyleSheetResource.h"
#include "core/fetch/Resource.h"
#include "core/fetch/ResourceFetcher.h"
#include "platform/SharedBuffer.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/network/ResourceError.h"
#include "public/platform/Platform.h"
#include "public/platform/WebData.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"
#include "wtf/Assertions.h"
#include "wtf/CurrentTime.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

ResourceLoader* ResourceLoader::create(ResourceFetcher* fetcher, Resource* resource)
{
    return new ResourceLoader(fetcher, resource);
}

ResourceLoader::ResourceLoader(ResourceFetcher* fetcher, Resource* resource)
    : m_fetcher(fetcher)
    , m_resource(resource)
{
    ASSERT(m_resource);
    ASSERT(m_fetcher);
    m_resource->setLoader(this);
}

ResourceLoader::~ResourceLoader()
{
    DCHECK(!m_loader);
}

DEFINE_TRACE(ResourceLoader)
{
    visitor->trace(m_fetcher);
    visitor->trace(m_resource);
}

void ResourceLoader::start(const ResourceRequest& request, WebTaskRunner* loadingTaskRunner, bool defersLoading)
{
    ASSERT(!m_loader);
    if (m_resource->options().synchronousPolicy == RequestSynchronously && defersLoading) {
        cancel();
        return;
    }

    m_loader = wrapUnique(Platform::current()->createURLLoader());
    m_loader->setDefersLoading(defersLoading);
    ASSERT(m_loader);
    m_loader->setLoadingTaskRunner(loadingTaskRunner);

    if (m_resource->options().synchronousPolicy == RequestSynchronously)
        requestSynchronously(request);
    else
        m_loader->loadAsynchronously(WrappedResourceRequest(request), this);
}

void ResourceLoader::restartForServiceWorkerFallback(const ResourceRequest& request)
{
    m_loader.reset();
    m_loader = wrapUnique(Platform::current()->createURLLoader());
    DCHECK(m_loader);
    m_loader->loadAsynchronously(WrappedResourceRequest(request), this);
}

void ResourceLoader::setDefersLoading(bool defers)
{
    ASSERT(m_loader);
    m_loader->setDefersLoading(defers);
}

void ResourceLoader::didDownloadData(WebURLLoader*, int length, int encodedDataLength)
{
    m_fetcher->didDownloadData(m_resource.get(), length, encodedDataLength);
    m_resource->didDownloadData(length);
}

void ResourceLoader::didChangePriority(ResourceLoadPriority loadPriority, int intraPriorityValue)
{
    if (m_loader)
        m_loader->didChangePriority(static_cast<WebURLRequest::Priority>(loadPriority), intraPriorityValue);
}

void ResourceLoader::cancel()
{
    didFail(nullptr, ResourceError::cancelledError(m_resource->lastResourceRequest().url()));
}

void ResourceLoader::willFollowRedirect(WebURLLoader*, WebURLRequest& passedNewRequest, const WebURLResponse& passedRedirectResponse, int64_t encodedDataLength)
{
    ASSERT(!passedNewRequest.isNull());
    ASSERT(!passedRedirectResponse.isNull());

    ResourceRequest& newRequest(passedNewRequest.toMutableResourceRequest());
    const ResourceResponse& redirectResponse(passedRedirectResponse.toResourceResponse());
    newRequest.setRedirectStatus(ResourceRequest::RedirectStatus::FollowedRedirect);

    if (m_fetcher->willFollowRedirect(m_resource.get(), newRequest, redirectResponse, encodedDataLength)) {
        m_resource->willFollowRedirect(newRequest, redirectResponse);
    } else {
        m_resource->willNotFollowRedirect();
        if (m_loader)
            didFail(nullptr, ResourceError::cancelledDueToAccessCheckError(newRequest.url()));
    }
}

void ResourceLoader::didReceiveCachedMetadata(WebURLLoader*, const char* data, int length)
{
    m_resource->setSerializedCachedMetadata(data, length);
}

void ResourceLoader::didSendData(WebURLLoader*, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    m_resource->didSendData(bytesSent, totalBytesToBeSent);
}

void ResourceLoader::didReceiveResponse(WebURLLoader*, const WebURLResponse& response, WebDataConsumerHandle* handle)
{
    DCHECK(!response.isNull());
    m_fetcher->didReceiveResponse(m_resource.get(), response.toResourceResponse(), handle);
}

void ResourceLoader::didReceiveResponse(WebURLLoader* loader, const WebURLResponse& response)
{
    didReceiveResponse(loader, response, nullptr);
}

void ResourceLoader::didReceiveData(WebURLLoader*, const char* data, int length, int encodedDataLength, int encodedBodyLength)
{
    RELEASE_ASSERT(length >= 0);
    m_fetcher->didReceiveData(m_resource.get(), data, length, encodedDataLength);
    m_resource->addToEncodedBodyLength(encodedBodyLength);
    m_resource->addToDecodedBodyLength(length);
    m_resource->appendData(data, length);
}

void ResourceLoader::didFinishLoadingFirstPartInMultipart()
{
    m_fetcher->didFinishLoading(m_resource.get(), 0, WebURLLoaderClient::kUnknownEncodedDataLength, ResourceFetcher::DidFinishFirstPartInMultipart);
}

void ResourceLoader::didFinishLoading(WebURLLoader*, double finishTime, int64_t encodedDataLength)
{
    m_loader.reset();
    m_fetcher->didFinishLoading(m_resource.get(), finishTime, encodedDataLength, ResourceFetcher::DidFinishLoading);
}

void ResourceLoader::didFail(WebURLLoader*, const WebURLError& error)
{
    m_loader.reset();
    m_fetcher->didFailLoading(m_resource.get(), error);
}

void ResourceLoader::requestSynchronously(const ResourceRequest& request)
{
    // downloadToFile is not supported for synchronous requests.
    ASSERT(!request.downloadToFile());
    ASSERT(m_loader);
    DCHECK(request.priority() == ResourceLoadPriorityHighest);

    WrappedResourceRequest requestIn(request);
    WebURLResponse responseOut;
    WebURLError errorOut;
    WebData dataOut;
    int64_t encodedDataLength = WebURLLoaderClient::kUnknownEncodedDataLength;
    m_loader->loadSynchronously(requestIn, responseOut, errorOut, dataOut, encodedDataLength);

    // A message dispatched while synchronously fetching the resource
    // can bring about the cancellation of this load.
    if (!m_loader)
        return;
    if (errorOut.reason) {
        didFail(0, errorOut);
        return;
    }
    didReceiveResponse(0, responseOut);
    if (!m_loader)
        return;
    DCHECK_GE(responseOut.toResourceResponse().encodedBodyLength(), 0);

    // Follow the async case convention of not calling didReceiveData or
    // appending data to m_resource if the response body is empty. Copying the
    // empty buffer is a noop in most cases, but is destructive in the case of
    // a 304, where it will overwrite the cached data we should be reusing.
    if (dataOut.size()) {
        m_fetcher->didReceiveData(m_resource.get(), dataOut.data(), dataOut.size(), encodedDataLength);
        m_resource->setResourceBuffer(dataOut);
    }
    didFinishLoading(0, monotonicallyIncreasingTime(), encodedDataLength);
}

} // namespace blink
