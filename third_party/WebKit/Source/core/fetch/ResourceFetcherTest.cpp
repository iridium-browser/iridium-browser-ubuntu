/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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

#include "core/fetch/ResourceFetcher.h"

#include "core/fetch/FetchInitiatorInfo.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/RawResource.h"
#include "core/fetch/ResourceLoader.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/heap/Handle.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/heap/Member.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceTimingInfo.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/weburl_loader_mock.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTaskRunner.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/platform/WebURLResponse.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Allocator.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

namespace {

const char testImageFilename[] = "white-1x1.png";
const int testImageSize = 103; // size of web/tests/data/white-1x1.png

class MockTaskRunner : public blink::WebTaskRunner {
    void postTask(const WebTraceLocation&, Task*) override { }
    void postDelayedTask(const WebTraceLocation&, Task*, double) override { }
    bool runsTasksOnCurrentThread() override { return true; }
    std::unique_ptr<WebTaskRunner> clone() override { return nullptr; }
    double virtualTimeSeconds() const override { return 0.0; }
    double monotonicallyIncreasingVirtualTimeSeconds() const override { return 0.0; }
    SingleThreadTaskRunner* taskRunner() override { return nullptr; }
};

}

class ResourceFetcherTestMockFetchContext : public FetchContext {
public:
    static ResourceFetcherTestMockFetchContext* create()
    {
        return new ResourceFetcherTestMockFetchContext;
    }

    virtual ~ResourceFetcherTestMockFetchContext() { }

    bool allowImage(bool imagesEnabled, const KURL&) const override { return true; }
    bool canRequest(Resource::Type, const ResourceRequest&, const KURL&, const ResourceLoaderOptions&, bool forPreload, FetchRequest::OriginRestriction) const override { return true; }
    bool shouldLoadNewResource(Resource::Type) const override { return true; }
    WebTaskRunner* loadingTaskRunner() const override { return m_runner.get(); }

    void setCachePolicy(CachePolicy policy) { m_policy = policy; }
    CachePolicy getCachePolicy() const override { return m_policy; }
    void setLoadComplete(bool complete) { m_complete = complete; }
    bool isLoadComplete() const override { return m_complete; }

    void addResourceTiming(const ResourceTimingInfo& resourceTimingInfo) override { m_transferSize = resourceTimingInfo.transferSize(); }
    long long getTransferSize() const { return m_transferSize; }

private:
    ResourceFetcherTestMockFetchContext()
        : m_policy(CachePolicyVerify)
        , m_runner(wrapUnique(new MockTaskRunner))
        , m_complete(false)
        , m_transferSize(-1)
    { }

    CachePolicy m_policy;
    std::unique_ptr<MockTaskRunner> m_runner;
    bool m_complete;
    long long m_transferSize;
};

class ResourceFetcherTest : public ::testing::Test {
};

class TestResourceFactory : public ResourceFactory {
public:
    TestResourceFactory(Resource::Type type = Resource::Raw)
        : ResourceFactory(type) { }

    Resource* create(const ResourceRequest& request, const ResourceLoaderOptions& options, const String& charset) const override
    {
        return Resource::create(request, type(), options);
    }
};

TEST_F(ResourceFetcherTest, StartLoadAfterFrameDetach)
{
    KURL secureURL(ParsedURLString, "https://secureorigin.test/image.png");
    // Try to request a url. The request should fail, no resource should be returned,
    // and no resource should be present in the cache.
    ResourceFetcher* fetcher = ResourceFetcher::create(nullptr);
    FetchRequest fetchRequest = FetchRequest(ResourceRequest(secureURL), FetchInitiatorInfo());
    Resource* resource = fetcher->requestResource(fetchRequest, TestResourceFactory());
    EXPECT_EQ(resource, static_cast<Resource*>(nullptr));
    EXPECT_EQ(memoryCache()->resourceForURL(secureURL), static_cast<Resource*>(nullptr));

    // Start by calling startLoad() directly, rather than via requestResource().
    // This shouldn't crash.
    fetcher->startLoad(Resource::create(secureURL, Resource::Raw));
}

TEST_F(ResourceFetcherTest, UseExistingResource)
{
    ResourceFetcher* fetcher = ResourceFetcher::create(ResourceFetcherTestMockFetchContext::create());

    KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
    Resource* resource = Resource::create(url, Resource::Image);
    memoryCache()->add(resource);
    ResourceResponse response;
    response.setURL(url);
    response.setHTTPStatusCode(200);
    response.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=3600");
    resource->responseReceived(response, nullptr);
    resource->finish();

    FetchRequest fetchRequest = FetchRequest(url, FetchInitiatorInfo());
    Resource* newResource = fetcher->requestResource(fetchRequest, TestResourceFactory(Resource::Image));
    EXPECT_EQ(resource, newResource);
    memoryCache()->remove(resource);
}

TEST_F(ResourceFetcherTest, Vary)
{
    KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
    Resource* resource = Resource::create(url, Resource::Raw);
    memoryCache()->add(resource);
    ResourceResponse response;
    response.setURL(url);
    response.setHTTPStatusCode(200);
    response.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=3600");
    response.setHTTPHeaderField(HTTPNames::Vary, "*");
    resource->responseReceived(response, nullptr);
    resource->finish();
    ASSERT_TRUE(resource->hasVaryHeader());

    ResourceFetcher* fetcher = ResourceFetcher::create(ResourceFetcherTestMockFetchContext::create());
    FetchRequest fetchRequest = FetchRequest(url, FetchInitiatorInfo());
    Platform::current()->getURLLoaderMockFactory()->registerURL(url, WebURLResponse(), "");
    Resource* newResource = fetcher->requestResource(fetchRequest, TestResourceFactory());
    EXPECT_NE(resource, newResource);
    newResource->loader()->cancel();
    memoryCache()->remove(newResource);
    Platform::current()->getURLLoaderMockFactory()->unregisterURL(url);

    memoryCache()->remove(resource);
}

TEST_F(ResourceFetcherTest, VaryOnBack)
{
    ResourceFetcherTestMockFetchContext* context = ResourceFetcherTestMockFetchContext::create();
    context->setCachePolicy(CachePolicyHistoryBuffer);
    ResourceFetcher* fetcher = ResourceFetcher::create(context);

    KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
    Resource* resource = Resource::create(url, Resource::Raw);
    memoryCache()->add(resource);
    ResourceResponse response;
    response.setURL(url);
    response.setHTTPStatusCode(200);
    response.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=3600");
    response.setHTTPHeaderField(HTTPNames::Vary, "*");
    resource->responseReceived(response, nullptr);
    resource->finish();
    ASSERT_TRUE(resource->hasVaryHeader());

    FetchRequest fetchRequest = FetchRequest(url, FetchInitiatorInfo());
    Resource* newResource = fetcher->requestResource(fetchRequest, TestResourceFactory());
    EXPECT_EQ(resource, newResource);

    memoryCache()->remove(newResource);
}

TEST_F(ResourceFetcherTest, VaryImage)
{
    ResourceFetcher* fetcher = ResourceFetcher::create(ResourceFetcherTestMockFetchContext::create());

    KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
    ResourceResponse response;
    response.setURL(url);
    response.setHTTPStatusCode(200);
    response.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=3600");
    response.setHTTPHeaderField(HTTPNames::Vary, "*");
    URLTestHelpers::registerMockedURLLoadWithCustomResponse(url, testImageFilename, WebString::fromUTF8(""), WrappedResourceResponse(response));

    FetchRequest fetchRequestOriginal = FetchRequest(url, FetchInitiatorInfo());
    Resource* resource = fetcher->requestResource(fetchRequestOriginal, TestResourceFactory(Resource::Image));
    ASSERT_TRUE(resource);
    Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
    ASSERT_TRUE(resource->hasVaryHeader());

    FetchRequest fetchRequest = FetchRequest(url, FetchInitiatorInfo());
    Resource* newResource = fetcher->requestResource(fetchRequest, TestResourceFactory(Resource::Image));
    EXPECT_EQ(resource, newResource);

    memoryCache()->remove(newResource);
    Platform::current()->getURLLoaderMockFactory()->unregisterURL(url);
}

class RequestSameResourceOnComplete : public GarbageCollectedFinalized<RequestSameResourceOnComplete>, public RawResourceClient {
    USING_GARBAGE_COLLECTED_MIXIN(RequestSameResourceOnComplete);
public:
    explicit RequestSameResourceOnComplete(Resource* resource)
        : m_resource(resource)
        , m_notifyFinishedCalled(false)
    {
    }

    void notifyFinished(Resource* resource) override
    {
        ASSERT_EQ(m_resource, resource);
        ResourceFetcherTestMockFetchContext* context = ResourceFetcherTestMockFetchContext::create();
        context->setCachePolicy(CachePolicyRevalidate);
        ResourceFetcher* fetcher2 = ResourceFetcher::create(context);
        FetchRequest fetchRequest2(m_resource->url(), FetchInitiatorInfo());
        Resource* resource2 = fetcher2->requestResource(fetchRequest2, TestResourceFactory(Resource::Image));
        EXPECT_EQ(m_resource, resource2);
        m_notifyFinishedCalled = true;
    }
    bool notifyFinishedCalled() const { return m_notifyFinishedCalled; }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_resource);
        RawResourceClient::trace(visitor);
    }

    String debugName() const override { return "RequestSameResourceOnComplete"; }

private:
    Member<Resource> m_resource;
    bool m_notifyFinishedCalled;
};

TEST_F(ResourceFetcherTest, RevalidateWhileFinishingLoading)
{
    KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
    ResourceResponse response;
    response.setURL(url);
    response.setHTTPStatusCode(200);
    response.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=3600");
    response.setHTTPHeaderField(HTTPNames::ETag, "1234567890");
    Platform::current()->getURLLoaderMockFactory()->registerURL(url, WrappedResourceResponse(response), "");

    ResourceFetcher* fetcher1 = ResourceFetcher::create(ResourceFetcherTestMockFetchContext::create());
    ResourceRequest request1(url);
    request1.setHTTPHeaderField(HTTPNames::Cache_Control, "no-cache");
    FetchRequest fetchRequest1 = FetchRequest(request1, FetchInitiatorInfo());
    Resource* resource1 = fetcher1->requestResource(fetchRequest1, TestResourceFactory(Resource::Image));
    Persistent<RequestSameResourceOnComplete> client = new RequestSameResourceOnComplete(resource1);
    resource1->addClient(client);
    Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
    Platform::current()->getURLLoaderMockFactory()->unregisterURL(url);
    EXPECT_TRUE(client->notifyFinishedCalled());
    resource1->removeClient(client);
    memoryCache()->remove(resource1);
}

TEST_F(ResourceFetcherTest, RevalidateDeferedResourceFromTwoInitiators)
{
    KURL url(ParsedURLString, "http://127.0.0.1:8000/font.woff");
    ResourceResponse response;
    response.setURL(url);
    response.setHTTPStatusCode(200);
    response.setHTTPHeaderField(HTTPNames::ETag, "1234567890");
    Platform::current()->getURLLoaderMockFactory()->registerURL(url, WrappedResourceResponse(response), "");

    ResourceFetcherTestMockFetchContext* context = ResourceFetcherTestMockFetchContext::create();
    ResourceFetcher* fetcher = ResourceFetcher::create(context);

    // Fetch to cache a resource.
    ResourceRequest request1(url);
    FetchRequest fetchRequest1 = FetchRequest(request1, FetchInitiatorInfo());
    Resource* resource1 = fetcher->requestResource(fetchRequest1, TestResourceFactory(Resource::Font));
    ASSERT_TRUE(resource1);
    fetcher->startLoad(resource1);
    Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
    EXPECT_TRUE(resource1->isLoaded());
    EXPECT_FALSE(resource1->errorOccurred());

    // Set the context as it is on reloads.
    context->setLoadComplete(true);
    context->setCachePolicy(CachePolicyRevalidate);

    // Revalidate the resource.
    ResourceRequest request2(url);
    FetchRequest fetchRequest2 = FetchRequest(request2, FetchInitiatorInfo());
    Resource* resource2 = fetcher->requestResource(fetchRequest2, TestResourceFactory(Resource::Font));
    ASSERT_TRUE(resource2);
    EXPECT_EQ(resource1, resource2);
    EXPECT_TRUE(resource2->isCacheValidator());
    EXPECT_TRUE(resource2->stillNeedsLoad());

    // Fetch the same resource again before actual load operation starts.
    ResourceRequest request3(url);
    FetchRequest fetchRequest3 = FetchRequest(request3, FetchInitiatorInfo());
    Resource* resource3 = fetcher->requestResource(fetchRequest3, TestResourceFactory(Resource::Font));
    ASSERT_TRUE(resource3);
    EXPECT_EQ(resource2, resource3);
    EXPECT_TRUE(resource3->isCacheValidator());
    EXPECT_TRUE(resource3->stillNeedsLoad());

    // startLoad() can be called from any initiator. Here, call it from the latter.
    fetcher->startLoad(resource3);
    Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
    EXPECT_TRUE(resource3->isLoaded());
    EXPECT_FALSE(resource3->errorOccurred());
    EXPECT_TRUE(resource2->isLoaded());
    EXPECT_FALSE(resource2->errorOccurred());

    memoryCache()->remove(resource1);
}

TEST_F(ResourceFetcherTest, DontReuseMediaDataUrl)
{
    ResourceFetcher* fetcher = ResourceFetcher::create(ResourceFetcherTestMockFetchContext::create());
    ResourceRequest request(KURL(ParsedURLString, "data:text/html,foo"));
    ResourceLoaderOptions options;
    options.dataBufferingPolicy = DoNotBufferData;
    FetchRequest fetchRequest = FetchRequest(request, FetchInitiatorTypeNames::internal, options);
    Resource* resource1 = fetcher->requestResource(fetchRequest, TestResourceFactory(Resource::Media));
    Resource* resource2 = fetcher->requestResource(fetchRequest, TestResourceFactory(Resource::Media));
    EXPECT_NE(resource1, resource2);
    memoryCache()->remove(resource2);
}

class ServeRequestsOnCompleteClient final : public GarbageCollectedFinalized<ServeRequestsOnCompleteClient>, public RawResourceClient {
    USING_GARBAGE_COLLECTED_MIXIN(ServeRequestsOnCompleteClient);
public:
    void notifyFinished(Resource*) override
    {
        Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
    }

    // No callbacks should be received except for the notifyFinished()
    // triggered by ResourceLoader::cancel().
    void dataSent(Resource*, unsigned long long, unsigned long long) override { ASSERT_TRUE(false); }
    void responseReceived(Resource*, const ResourceResponse&, std::unique_ptr<WebDataConsumerHandle>) override { ASSERT_TRUE(false); }
    void setSerializedCachedMetadata(Resource*, const char*, size_t) override { ASSERT_TRUE(false); }
    void dataReceived(Resource*, const char*, size_t) override { ASSERT_TRUE(false); }
    void redirectReceived(Resource*, ResourceRequest&, const ResourceResponse&) override { ASSERT_TRUE(false); }
    void dataDownloaded(Resource*, int) override { ASSERT_TRUE(false); }
    void didReceiveResourceTiming(Resource*, const ResourceTimingInfo&) override { ASSERT_TRUE(false); }

    DEFINE_INLINE_TRACE()
    {
        RawResourceClient::trace(visitor);
    }

    String debugName() const override { return "ServeRequestsOnCompleteClient"; }
};

// Regression test for http://crbug.com/594072.
// This emulates a modal dialog triggering a nested run loop inside
// ResourceLoader::cancel(). If the ResourceLoader doesn't promptly cancel its
// WebURLLoader before notifying its clients, a nested run loop  may send a
// network response, leading to an invalid state transition in ResourceLoader.
TEST_F(ResourceFetcherTest, ResponseOnCancel)
{
    KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
    ResourceResponse response;
    response.setURL(url);
    response.setHTTPStatusCode(200);
    URLTestHelpers::registerMockedURLLoadWithCustomResponse(url, testImageFilename, WebString::fromUTF8(""), WrappedResourceResponse(response));

    ResourceFetcher* fetcher = ResourceFetcher::create(ResourceFetcherTestMockFetchContext::create());
    FetchRequest fetchRequest = FetchRequest(url, FetchInitiatorInfo());
    Resource* resource = fetcher->requestResource(fetchRequest, TestResourceFactory(Resource::Raw));
    Persistent<ServeRequestsOnCompleteClient> client = new ServeRequestsOnCompleteClient();
    resource->addClient(client);
    resource->loader()->cancel();
    resource->removeClient(client);
    Platform::current()->getURLLoaderMockFactory()->unregisterURL(url);
}

class ScopedMockRedirectRequester {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(ScopedMockRedirectRequester);

public:
    ScopedMockRedirectRequester()
        : m_context(nullptr)
    {
    }

    ~ScopedMockRedirectRequester()
    {
        cleanUp();
    }

    void registerRedirect(const WebString& fromURL, const WebString& toURL)
    {
        KURL redirectURL(ParsedURLString, fromURL);
        WebURLResponse redirectResponse;
        redirectResponse.setURL(redirectURL);
        redirectResponse.setHTTPStatusCode(301);
        redirectResponse.setHTTPHeaderField(HTTPNames::Location, toURL);
        Platform::current()->getURLLoaderMockFactory()->registerURL(redirectURL, redirectResponse, "");
    }

    void registerFinalResource(const WebString& url)
    {
        KURL finalURL(ParsedURLString, url);
        WebURLResponse finalResponse;
        finalResponse.setURL(finalURL);
        finalResponse.setHTTPStatusCode(200);
        URLTestHelpers::registerMockedURLLoadWithCustomResponse(finalURL, testImageFilename, "", finalResponse);
    }

    void request(const WebString& url)
    {
        DCHECK(!m_context);
        m_context = ResourceFetcherTestMockFetchContext::create();
        ResourceFetcher* fetcher = ResourceFetcher::create(m_context);
        FetchRequest fetchRequest = FetchRequest(ResourceRequest(url), FetchInitiatorInfo());
        fetcher->requestResource(fetchRequest, TestResourceFactory());
        Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
    }

    void cleanUp()
    {
        Platform::current()->getURLLoaderMockFactory()->unregisterAllURLs();
        memoryCache()->evictResources();
    }

    ResourceFetcherTestMockFetchContext* context() const { return m_context; }

private:
    Member<ResourceFetcherTestMockFetchContext> m_context;
};

TEST_F(ResourceFetcherTest, SameOriginRedirect)
{
    const char redirectURL[] = "http://127.0.0.1:8000/redirect.html";
    const char finalURL[] = "http://127.0.0.1:8000/final.html";
    ScopedMockRedirectRequester requester;
    requester.registerRedirect(redirectURL, finalURL);
    requester.registerFinalResource(finalURL);
    requester.request(redirectURL);

    EXPECT_EQ(kRedirectResponseOverheadBytes + testImageSize, requester.context()->getTransferSize());
}

TEST_F(ResourceFetcherTest, CrossOriginRedirect)
{
    const char redirectURL[] = "http://otherorigin.test/redirect.html";
    const char finalURL[] = "http://127.0.0.1:8000/final.html";
    ScopedMockRedirectRequester requester;
    requester.registerRedirect(redirectURL, finalURL);
    requester.registerFinalResource(finalURL);
    requester.request(redirectURL);

    EXPECT_EQ(testImageSize, requester.context()->getTransferSize());
}

TEST_F(ResourceFetcherTest, ComplexCrossOriginRedirect)
{
    const char redirectURL1[] = "http://127.0.0.1:8000/redirect1.html";
    const char redirectURL2[] = "http://otherorigin.test/redirect2.html";
    const char redirectURL3[] = "http://127.0.0.1:8000/redirect3.html";
    const char finalURL[] = "http://127.0.0.1:8000/final.html";
    ScopedMockRedirectRequester requester;
    requester.registerRedirect(redirectURL1, redirectURL2);
    requester.registerRedirect(redirectURL2, redirectURL3);
    requester.registerRedirect(redirectURL3, finalURL);
    requester.registerFinalResource(finalURL);
    requester.request(redirectURL1);

    EXPECT_EQ(testImageSize, requester.context()->getTransferSize());
}

TEST_F(ResourceFetcherTest, SynchronousRequest)
{
    KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
    ResourceResponse response;
    response.setURL(url);
    response.setHTTPStatusCode(200);
    URLTestHelpers::registerMockedURLLoadWithCustomResponse(url, testImageFilename, WebString::fromUTF8(""), WrappedResourceResponse(response));

    ResourceFetcher* fetcher = ResourceFetcher::create(ResourceFetcherTestMockFetchContext::create());
    FetchRequest request(url, FetchInitiatorInfo());
    request.makeSynchronous();
    Resource* resource = fetcher->requestResource(request, TestResourceFactory());
    EXPECT_TRUE(resource->isLoaded());
    EXPECT_EQ(ResourceLoadPriorityHighest, resource->resourceRequest().priority());

    memoryCache()->remove(resource);
}

} // namespace blink
