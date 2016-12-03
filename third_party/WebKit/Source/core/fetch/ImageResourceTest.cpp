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

#include "core/fetch/ImageResource.h"

#include "core/fetch/MemoryCache.h"
#include "core/fetch/MockResourceClients.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ResourceLoader.h"
#include "core/fetch/UniqueIdentifier.h"
#include "platform/SharedBuffer.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/graphics/Image.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/platform/WebURLResponse.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

// An image of size 1x1.
static Vector<unsigned char> jpegImage()
{
    Vector<unsigned char> jpeg;

    static const unsigned char data[] = {
        0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x01, 0x00,
        0x48, 0x00, 0x48, 0x00, 0x00, 0xff, 0xfe, 0x00, 0x13, 0x43, 0x72, 0x65, 0x61, 0x74, 0x65,
        0x64, 0x20, 0x77, 0x69, 0x74, 0x68, 0x20, 0x47, 0x49, 0x4d, 0x50, 0xff, 0xdb, 0x00, 0x43,
        0x00, 0x05, 0x03, 0x04, 0x04, 0x04, 0x03, 0x05, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x06,
        0x07, 0x0c, 0x08, 0x07, 0x07, 0x07, 0x07, 0x0f, 0x0b, 0x0b, 0x09, 0x0c, 0x11, 0x0f, 0x12,
        0x12, 0x11, 0x0f, 0x11, 0x11, 0x13, 0x16, 0x1c, 0x17, 0x13, 0x14, 0x1a, 0x15, 0x11, 0x11,
        0x18, 0x21, 0x18, 0x1a, 0x1d, 0x1d, 0x1f, 0x1f, 0x1f, 0x13, 0x17, 0x22, 0x24, 0x22, 0x1e,
        0x24, 0x1c, 0x1e, 0x1f, 0x1e, 0xff, 0xdb, 0x00, 0x43, 0x01, 0x05, 0x05, 0x05, 0x07, 0x06,
        0x07, 0x0e, 0x08, 0x08, 0x0e, 0x1e, 0x14, 0x11, 0x14, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
        0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
        0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
        0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0xff,
        0xc0, 0x00, 0x11, 0x08, 0x00, 0x01, 0x00, 0x01, 0x03, 0x01, 0x22, 0x00, 0x02, 0x11, 0x01,
        0x03, 0x11, 0x01, 0xff, 0xc4, 0x00, 0x15, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0xff, 0xc4, 0x00, 0x14,
        0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0xff, 0xc4, 0x00, 0x14, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xc4, 0x00, 0x14, 0x11,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0xff, 0xda, 0x00, 0x0c, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3f,
        0x00, 0xb2, 0xc0, 0x07, 0xff, 0xd9
    };

    jpeg.append(data, sizeof(data));
    return jpeg;
}

// An image of size 50x50.
static Vector<unsigned char> jpegImage2()
{
    Vector<unsigned char> jpeg;

    static const unsigned char data[] = {
        0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x01, 0x00, 0x48,
        0x00, 0x48, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xdb, 0x00, 0x43, 0x01, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0,
        0x00, 0x11, 0x08, 0x00, 0x32, 0x00, 0x32, 0x03, 0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11,
        0x01, 0xff, 0xc4, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xc4, 0x00, 0x14, 0x10, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xc4, 0x00,
        0x15, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x02, 0xff, 0xc4, 0x00, 0x14, 0x11, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xda, 0x00, 0x0c, 0x03, 0x01,
        0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3f, 0x00, 0x00, 0x94, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x03, 0xff, 0xd9
    };

    jpeg.append(data, sizeof(data));
    return jpeg;
}

static Vector<unsigned char> svgImage()
{
    static const char data[] =
        "<svg width=\"200\" height=\"200\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\">"
        "<rect x=\"0\" y=\"0\" width=\"100px\" height=\"100px\" fill=\"red\"/>"
        "</svg>";

    Vector<unsigned char> svg;
    svg.append(data, strlen(data));
    return svg;
}

static Vector<unsigned char> svgImage2()
{
    static const char data[] =
        "<svg width=\"300\" height=\"300\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\">"
        "<rect x=\"0\" y=\"0\" width=\"200px\" height=\"200px\" fill=\"green\"/>"
        "</svg>";

    Vector<unsigned char> svg;
    svg.append(data, strlen(data));
    return svg;
}

void receiveResponse(ImageResource* imageResource, const KURL& url, const AtomicString& mimeType, const Vector<unsigned char>& data)
{
    ResourceResponse response;
    response.setURL(url);
    response.setHTTPStatusCode(200);
    response.setMimeType(mimeType);
    imageResource->responseReceived(response, nullptr);
    imageResource->appendData(reinterpret_cast<const char*>(data.data()), data.size());
    imageResource->finish();
}

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

class ImageResourceTestMockFetchContext : public FetchContext {
public:
    static ImageResourceTestMockFetchContext* create()
    {
        return new ImageResourceTestMockFetchContext;
    }

    virtual ~ImageResourceTestMockFetchContext() { }

    bool allowImage(bool imagesEnabled, const KURL&) const override { return true; }
    bool canRequest(Resource::Type, const ResourceRequest&, const KURL&, const ResourceLoaderOptions&, bool forPreload, FetchRequest::OriginRestriction) const override { return true; }
    bool shouldLoadNewResource(Resource::Type) const override { return true; }
    WebTaskRunner* loadingTaskRunner() const override { return m_runner.get(); }

private:
    ImageResourceTestMockFetchContext()
        :  m_runner(wrapUnique(new MockTaskRunner))
    { }

    std::unique_ptr<MockTaskRunner> m_runner;
};

TEST(ImageResourceTest, MultipartImage)
{
    ResourceFetcher* fetcher = ResourceFetcher::create(ImageResourceTestMockFetchContext::create());
    KURL testURL(ParsedURLString, "http://www.test.com/cancelTest.html");
    URLTestHelpers::registerMockedURLLoad(testURL, "cancelTest.html", "text/html");

    // Emulate starting a real load, but don't expect any "real" WebURLLoaderClient callbacks.
    ImageResource* cachedImage = ImageResource::create(ResourceRequest(testURL));
    cachedImage->setIdentifier(createUniqueIdentifier());
    fetcher->startLoad(cachedImage);
    Platform::current()->getURLLoaderMockFactory()->unregisterURL(testURL);

    Persistent<MockImageResourceClient> client = new MockImageResourceClient(cachedImage);
    EXPECT_EQ(Resource::Pending, cachedImage->getStatus());

    // Send the multipart response. No image or data buffer is created.
    // Note that the response must be routed through ResourceLoader to
    // ensure the load is flagged as multipart.
    ResourceResponse multipartResponse(KURL(), "multipart/x-mixed-replace", 0, nullAtom, String());
    multipartResponse.setMultipartBoundary("boundary", strlen("boundary"));
    cachedImage->loader()->didReceiveResponse(nullptr, WrappedResourceResponse(multipartResponse), nullptr);
    ASSERT_FALSE(cachedImage->resourceBuffer());
    ASSERT_FALSE(cachedImage->hasImage());
    ASSERT_EQ(client->imageChangedCount(), 0);
    ASSERT_FALSE(client->notifyFinishedCalled());
    EXPECT_EQ("multipart/x-mixed-replace", cachedImage->response().mimeType());

    const char firstPart[] =
        "--boundary\n"
        "Content-Type: image/svg+xml\n\n";
    cachedImage->appendData(firstPart, strlen(firstPart));
    // Send the response for the first real part. No image or data buffer is created.
    ASSERT_FALSE(cachedImage->resourceBuffer());
    ASSERT_FALSE(cachedImage->hasImage());
    ASSERT_EQ(client->imageChangedCount(), 0);
    ASSERT_FALSE(client->notifyFinishedCalled());
    EXPECT_EQ("image/svg+xml", cachedImage->response().mimeType());

    const char secondPart[] = "<svg xmlns='http://www.w3.org/2000/svg' width='1' height='1'><rect width='1' height='1' fill='green'/></svg>\n";
    // The first bytes arrive. The data buffer is created, but no image is created.
    cachedImage->appendData(secondPart, strlen(secondPart));
    ASSERT_TRUE(cachedImage->resourceBuffer());
    ASSERT_FALSE(cachedImage->hasImage());
    ASSERT_EQ(client->imageChangedCount(), 0);
    ASSERT_FALSE(client->notifyFinishedCalled());

    // Add a client to check an assertion error doesn't happen
    // (crbug.com/630983).
    Persistent<MockImageResourceClient> client2 = new MockImageResourceClient(cachedImage);
    ASSERT_EQ(client2->imageChangedCount(), 0);
    ASSERT_FALSE(client2->notifyFinishedCalled());

    const char thirdPart[] = "--boundary";
    cachedImage->appendData(thirdPart, strlen(thirdPart));
    ASSERT_TRUE(cachedImage->resourceBuffer());
    ASSERT_EQ(cachedImage->resourceBuffer()->size(), strlen(secondPart) - 1);

    // This part finishes. The image is created, callbacks are sent, and the data buffer is cleared.
    cachedImage->loader()->didFinishLoading(nullptr, 0.0, 0);
    ASSERT_TRUE(cachedImage->resourceBuffer());
    ASSERT_FALSE(cachedImage->errorOccurred());
    ASSERT_TRUE(cachedImage->hasImage());
    ASSERT_FALSE(cachedImage->getImage()->isNull());
    ASSERT_EQ(cachedImage->getImage()->width(), 1);
    ASSERT_EQ(cachedImage->getImage()->height(), 1);
    ASSERT_EQ(client->imageChangedCount(), 1);
    ASSERT_TRUE(client->notifyFinishedCalled());
    ASSERT_EQ(client2->imageChangedCount(), 1);
    ASSERT_TRUE(client2->notifyFinishedCalled());
}

TEST(ImageResourceTest, CancelOnDetach)
{
    KURL testURL(ParsedURLString, "http://www.test.com/cancelTest.html");
    URLTestHelpers::registerMockedURLLoad(testURL, "cancelTest.html", "text/html");

    ResourceFetcher* fetcher = ResourceFetcher::create(ImageResourceTestMockFetchContext::create());

    // Emulate starting a real load.
    ImageResource* cachedImage = ImageResource::create(ResourceRequest(testURL));
    cachedImage->setIdentifier(createUniqueIdentifier());

    fetcher->startLoad(cachedImage);
    memoryCache()->add(cachedImage);

    Persistent<MockImageResourceClient> client = new MockImageResourceClient(cachedImage);
    EXPECT_EQ(Resource::Pending, cachedImage->getStatus());

    // The load should still be alive, but a timer should be started to cancel the load inside removeClient().
    client->removeAsClient();
    EXPECT_EQ(Resource::Pending, cachedImage->getStatus());
    EXPECT_NE(reinterpret_cast<Resource*>(0), memoryCache()->resourceForURL(testURL));

    // Trigger the cancel timer, ensure the load was cancelled and the resource was evicted from the cache.
    blink::testing::runPendingTasks();
    EXPECT_EQ(Resource::LoadError, cachedImage->getStatus());
    EXPECT_EQ(reinterpret_cast<Resource*>(0), memoryCache()->resourceForURL(testURL));

    Platform::current()->getURLLoaderMockFactory()->unregisterURL(testURL);
}

TEST(ImageResourceTest, DecodedDataRemainsWhileHasClients)
{
    ImageResource* cachedImage = ImageResource::create(ResourceRequest());
    cachedImage->setStatus(Resource::Pending);

    Persistent<MockImageResourceClient> client = new MockImageResourceClient(cachedImage);

    // Send the image response.
    cachedImage->responseReceived(ResourceResponse(KURL(), "multipart/x-mixed-replace", 0, nullAtom, String()), nullptr);

    Vector<unsigned char> jpeg = jpegImage();
    cachedImage->responseReceived(ResourceResponse(KURL(), "image/jpeg", jpeg.size(), nullAtom, String()), nullptr);
    cachedImage->appendData(reinterpret_cast<const char*>(jpeg.data()), jpeg.size());
    cachedImage->finish();
    ASSERT_FALSE(cachedImage->errorOccurred());
    ASSERT_TRUE(cachedImage->hasImage());
    ASSERT_FALSE(cachedImage->getImage()->isNull());
    ASSERT_TRUE(client->notifyFinishedCalled());

    // The prune comes when the ImageResource still has clients. The image should not be deleted.
    cachedImage->prune();
    ASSERT_TRUE(cachedImage->hasClientsOrObservers());
    ASSERT_TRUE(cachedImage->hasImage());
    ASSERT_FALSE(cachedImage->getImage()->isNull());

    // The ImageResource no longer has clients. The decoded image data should be
    // deleted by prune.
    client->removeAsClient();
    cachedImage->prune();
    ASSERT_FALSE(cachedImage->hasClientsOrObservers());
    ASSERT_TRUE(cachedImage->hasImage());
    // TODO(hajimehoshi): Should check cachedImage doesn't have decoded image
    // data.
}

TEST(ImageResourceTest, UpdateBitmapImages)
{
    ImageResource* cachedImage = ImageResource::create(ResourceRequest());
    cachedImage->setStatus(Resource::Pending);

    Persistent<MockImageResourceClient> client = new MockImageResourceClient(cachedImage);

    // Send the image response.
    Vector<unsigned char> jpeg = jpegImage();
    cachedImage->responseReceived(ResourceResponse(KURL(), "image/jpeg", jpeg.size(), nullAtom, String()), nullptr);
    cachedImage->appendData(reinterpret_cast<const char*>(jpeg.data()), jpeg.size());
    cachedImage->finish();
    ASSERT_FALSE(cachedImage->errorOccurred());
    ASSERT_TRUE(cachedImage->hasImage());
    ASSERT_FALSE(cachedImage->getImage()->isNull());
    ASSERT_EQ(client->imageChangedCount(), 2);
    ASSERT_TRUE(client->notifyFinishedCalled());
    ASSERT_TRUE(cachedImage->getImage()->isBitmapImage());
}

TEST(ImageResourceTest, ReloadIfLoFi)
{
    KURL testURL(ParsedURLString, "http://www.test.com/cancelTest.html");
    URLTestHelpers::registerMockedURLLoad(testURL, "cancelTest.html", "text/html");
    ResourceRequest request = ResourceRequest(testURL);
    request.setLoFiState(WebURLRequest::LoFiOn);
    ImageResource* cachedImage = ImageResource::create(request);
    cachedImage->setStatus(Resource::Pending);

    Persistent<MockImageResourceClient> client = new MockImageResourceClient(cachedImage);
    ResourceFetcher* fetcher = ResourceFetcher::create(ImageResourceTestMockFetchContext::create());

    // Send the image response.
    Vector<unsigned char> jpeg = jpegImage();
    ResourceResponse resourceResponse(KURL(), "image/jpeg", jpeg.size(), nullAtom, String());
    resourceResponse.addHTTPHeaderField("chrome-proxy", "q=low");

    cachedImage->responseReceived(resourceResponse, nullptr);
    cachedImage->appendData(reinterpret_cast<const char*>(jpeg.data()), jpeg.size());
    cachedImage->finish();
    ASSERT_FALSE(cachedImage->errorOccurred());
    ASSERT_TRUE(cachedImage->hasImage());
    ASSERT_FALSE(cachedImage->getImage()->isNull());
    ASSERT_EQ(client->imageChangedCount(), 2);
    ASSERT_TRUE(client->notifyFinishedCalled());
    ASSERT_TRUE(cachedImage->getImage()->isBitmapImage());
    EXPECT_EQ(1, cachedImage->getImage()->width());
    EXPECT_EQ(1, cachedImage->getImage()->height());

    cachedImage->reloadIfLoFi(fetcher);
    ASSERT_FALSE(cachedImage->errorOccurred());
    ASSERT_FALSE(cachedImage->resourceBuffer());
    ASSERT_FALSE(cachedImage->hasImage());
    ASSERT_EQ(client->imageChangedCount(), 3);

    Vector<unsigned char> jpeg2 = jpegImage2();
    cachedImage->loader()->didReceiveResponse(nullptr, WrappedResourceResponse(resourceResponse), nullptr);
    cachedImage->loader()->didReceiveData(nullptr, reinterpret_cast<const char*>(jpeg2.data()), jpeg2.size(), jpeg2.size(), jpeg2.size());
    cachedImage->loader()->didFinishLoading(nullptr, 0.0, jpeg2.size());
    ASSERT_FALSE(cachedImage->errorOccurred());
    ASSERT_TRUE(cachedImage->hasImage());
    ASSERT_FALSE(cachedImage->getImage()->isNull());
    ASSERT_TRUE(client->notifyFinishedCalled());
    ASSERT_TRUE(cachedImage->getImage()->isBitmapImage());
    EXPECT_EQ(50, cachedImage->getImage()->width());
    EXPECT_EQ(50, cachedImage->getImage()->height());
}

TEST(ImageResourceTest, SVGImage)
{
    KURL url(ParsedURLString, "http://127.0.0.1:8000/foo");
    ImageResource* imageResource = ImageResource::create(ResourceRequest(url));
    Persistent<MockImageResourceClient> client = new MockImageResourceClient(imageResource);

    receiveResponse(imageResource, url, "image/svg+xml", svgImage());

    EXPECT_FALSE(imageResource->errorOccurred());
    ASSERT_TRUE(imageResource->hasImage());
    EXPECT_FALSE(imageResource->getImage()->isNull());
    EXPECT_EQ(1, client->imageChangedCount());
    EXPECT_TRUE(client->notifyFinishedCalled());
    EXPECT_FALSE(imageResource->getImage()->isBitmapImage());
}

TEST(ImageResourceTest, SuccessfulRevalidationJpeg)
{
    KURL url(ParsedURLString, "http://127.0.0.1:8000/foo");
    ImageResource* imageResource = ImageResource::create(ResourceRequest(url));
    Persistent<MockImageResourceClient> client = new MockImageResourceClient(imageResource);

    receiveResponse(imageResource, url, "image/jpeg", jpegImage());

    EXPECT_FALSE(imageResource->errorOccurred());
    ASSERT_TRUE(imageResource->hasImage());
    EXPECT_FALSE(imageResource->getImage()->isNull());
    EXPECT_EQ(2, client->imageChangedCount());
    EXPECT_TRUE(client->notifyFinishedCalled());
    EXPECT_TRUE(imageResource->getImage()->isBitmapImage());
    EXPECT_EQ(1, imageResource->getImage()->width());
    EXPECT_EQ(1, imageResource->getImage()->height());

    imageResource->setRevalidatingRequest(ResourceRequest(url));
    ResourceResponse response;
    response.setURL(url);
    response.setHTTPStatusCode(304);

    imageResource->responseReceived(response, nullptr);

    EXPECT_FALSE(imageResource->errorOccurred());
    ASSERT_TRUE(imageResource->hasImage());
    EXPECT_FALSE(imageResource->getImage()->isNull());
    EXPECT_EQ(2, client->imageChangedCount());
    EXPECT_TRUE(client->notifyFinishedCalled());
    EXPECT_TRUE(imageResource->getImage()->isBitmapImage());
    EXPECT_EQ(1, imageResource->getImage()->width());
    EXPECT_EQ(1, imageResource->getImage()->height());
}

TEST(ImageResourceTest, SuccessfulRevalidationSvg)
{
    KURL url(ParsedURLString, "http://127.0.0.1:8000/foo");
    ImageResource* imageResource = ImageResource::create(ResourceRequest(url));
    Persistent<MockImageResourceClient> client = new MockImageResourceClient(imageResource);

    receiveResponse(imageResource, url, "image/svg+xml", svgImage());

    EXPECT_FALSE(imageResource->errorOccurred());
    ASSERT_TRUE(imageResource->hasImage());
    EXPECT_FALSE(imageResource->getImage()->isNull());
    EXPECT_EQ(1, client->imageChangedCount());
    EXPECT_TRUE(client->notifyFinishedCalled());
    EXPECT_FALSE(imageResource->getImage()->isBitmapImage());
    EXPECT_EQ(200, imageResource->getImage()->width());
    EXPECT_EQ(200, imageResource->getImage()->height());

    imageResource->setRevalidatingRequest(ResourceRequest(url));
    ResourceResponse response;
    response.setURL(url);
    response.setHTTPStatusCode(304);
    imageResource->responseReceived(response, nullptr);

    EXPECT_FALSE(imageResource->errorOccurred());
    ASSERT_TRUE(imageResource->hasImage());
    EXPECT_FALSE(imageResource->getImage()->isNull());
    EXPECT_EQ(1, client->imageChangedCount());
    EXPECT_TRUE(client->notifyFinishedCalled());
    EXPECT_FALSE(imageResource->getImage()->isBitmapImage());
    EXPECT_EQ(200, imageResource->getImage()->width());
    EXPECT_EQ(200, imageResource->getImage()->height());
}

TEST(ImageResourceTest, FailedRevalidationJpegToJpeg)
{
    KURL url(ParsedURLString, "http://127.0.0.1:8000/foo");
    ImageResource* imageResource = ImageResource::create(ResourceRequest(url));
    Persistent<MockImageResourceClient> client = new MockImageResourceClient(imageResource);

    receiveResponse(imageResource, url, "image/jpeg", jpegImage());

    EXPECT_FALSE(imageResource->errorOccurred());
    ASSERT_TRUE(imageResource->hasImage());
    EXPECT_FALSE(imageResource->getImage()->isNull());
    EXPECT_EQ(2, client->imageChangedCount());
    EXPECT_TRUE(client->notifyFinishedCalled());
    EXPECT_TRUE(imageResource->getImage()->isBitmapImage());
    EXPECT_EQ(1, imageResource->getImage()->width());
    EXPECT_EQ(1, imageResource->getImage()->height());

    imageResource->setRevalidatingRequest(ResourceRequest(url));
    receiveResponse(imageResource, url, "image/jpeg", jpegImage2());

    EXPECT_FALSE(imageResource->errorOccurred());
    ASSERT_TRUE(imageResource->hasImage());
    EXPECT_FALSE(imageResource->getImage()->isNull());
    EXPECT_EQ(4, client->imageChangedCount());
    EXPECT_TRUE(client->notifyFinishedCalled());
    EXPECT_TRUE(imageResource->getImage()->isBitmapImage());
    EXPECT_EQ(50, imageResource->getImage()->width());
    EXPECT_EQ(50, imageResource->getImage()->height());
}

TEST(ImageResourceTest, FailedRevalidationJpegToSvg)
{
    KURL url(ParsedURLString, "http://127.0.0.1:8000/foo");
    ImageResource* imageResource = ImageResource::create(ResourceRequest(url));
    Persistent<MockImageResourceClient> client = new MockImageResourceClient(imageResource);

    receiveResponse(imageResource, url, "image/jpeg", jpegImage());

    EXPECT_FALSE(imageResource->errorOccurred());
    ASSERT_TRUE(imageResource->hasImage());
    EXPECT_FALSE(imageResource->getImage()->isNull());
    EXPECT_EQ(2, client->imageChangedCount());
    EXPECT_TRUE(client->notifyFinishedCalled());
    EXPECT_TRUE(imageResource->getImage()->isBitmapImage());
    EXPECT_EQ(1, imageResource->getImage()->width());
    EXPECT_EQ(1, imageResource->getImage()->height());

    imageResource->setRevalidatingRequest(ResourceRequest(url));
    receiveResponse(imageResource, url, "image/svg+xml", svgImage());

    EXPECT_FALSE(imageResource->errorOccurred());
    ASSERT_TRUE(imageResource->hasImage());
    EXPECT_FALSE(imageResource->getImage()->isNull());
    EXPECT_EQ(3, client->imageChangedCount());
    EXPECT_TRUE(client->notifyFinishedCalled());
    EXPECT_FALSE(imageResource->getImage()->isBitmapImage());
    EXPECT_EQ(200, imageResource->getImage()->width());
    EXPECT_EQ(200, imageResource->getImage()->height());
}

TEST(ImageResourceTest, FailedRevalidationSvgToJpeg)
{
    KURL url(ParsedURLString, "http://127.0.0.1:8000/foo");
    ImageResource* imageResource = ImageResource::create(ResourceRequest(url));
    Persistent<MockImageResourceClient> client = new MockImageResourceClient(imageResource);

    receiveResponse(imageResource, url, "image/svg+xml", svgImage());

    EXPECT_FALSE(imageResource->errorOccurred());
    ASSERT_TRUE(imageResource->hasImage());
    EXPECT_FALSE(imageResource->getImage()->isNull());
    EXPECT_EQ(1, client->imageChangedCount());
    EXPECT_TRUE(client->notifyFinishedCalled());
    EXPECT_FALSE(imageResource->getImage()->isBitmapImage());
    EXPECT_EQ(200, imageResource->getImage()->width());
    EXPECT_EQ(200, imageResource->getImage()->height());

    imageResource->setRevalidatingRequest(ResourceRequest(url));
    receiveResponse(imageResource, url, "image/jpeg", jpegImage());

    EXPECT_FALSE(imageResource->errorOccurred());
    ASSERT_TRUE(imageResource->hasImage());
    EXPECT_FALSE(imageResource->getImage()->isNull());
    EXPECT_EQ(3, client->imageChangedCount());
    EXPECT_TRUE(client->notifyFinishedCalled());
    EXPECT_TRUE(imageResource->getImage()->isBitmapImage());
    EXPECT_EQ(1, imageResource->getImage()->width());
    EXPECT_EQ(1, imageResource->getImage()->height());
}

TEST(ImageResourceTest, FailedRevalidationSvgToSvg)
{
    KURL url(ParsedURLString, "http://127.0.0.1:8000/foo");
    ImageResource* imageResource = ImageResource::create(ResourceRequest(url));
    Persistent<MockImageResourceClient> client = new MockImageResourceClient(imageResource);

    receiveResponse(imageResource, url, "image/svg+xml", svgImage());

    EXPECT_FALSE(imageResource->errorOccurred());
    ASSERT_TRUE(imageResource->hasImage());
    EXPECT_FALSE(imageResource->getImage()->isNull());
    EXPECT_EQ(client->imageChangedCount(), 1);
    EXPECT_TRUE(client->notifyFinishedCalled());
    EXPECT_FALSE(imageResource->getImage()->isBitmapImage());
    EXPECT_EQ(200, imageResource->getImage()->width());
    EXPECT_EQ(200, imageResource->getImage()->height());

    imageResource->setRevalidatingRequest(ResourceRequest(url));
    receiveResponse(imageResource, url, "image/svg+xml", svgImage2());

    EXPECT_FALSE(imageResource->errorOccurred());
    ASSERT_TRUE(imageResource->hasImage());
    EXPECT_FALSE(imageResource->getImage()->isNull());
    EXPECT_EQ(2, client->imageChangedCount());
    EXPECT_TRUE(client->notifyFinishedCalled());
    EXPECT_FALSE(imageResource->getImage()->isBitmapImage());
    EXPECT_EQ(300, imageResource->getImage()->width());
    EXPECT_EQ(300, imageResource->getImage()->height());
}

// Tests for pruning.

TEST(ImageResourceTest, AddClientAfterPrune)
{
    KURL url(ParsedURLString, "http://127.0.0.1:8000/foo");
    ImageResource* imageResource = ImageResource::create(ResourceRequest(url));

    // Adds a ResourceClient but not ImageResourceObserver.
    Persistent<MockResourceClient> client1 = new MockResourceClient(imageResource);

    receiveResponse(imageResource, url, "image/jpeg", jpegImage());

    EXPECT_FALSE(imageResource->errorOccurred());
    ASSERT_TRUE(imageResource->hasImage());
    EXPECT_FALSE(imageResource->getImage()->isNull());
    EXPECT_EQ(1, imageResource->getImage()->width());
    EXPECT_EQ(1, imageResource->getImage()->height());
    EXPECT_TRUE(client1->notifyFinishedCalled());

    client1->removeAsClient();

    EXPECT_FALSE(imageResource->hasClientsOrObservers());

    imageResource->prune();

    EXPECT_TRUE(imageResource->hasImage());

    // Re-adds a ResourceClient but not ImageResourceObserver.
    Persistent<MockResourceClient> client2 = new MockResourceClient(imageResource);

    ASSERT_TRUE(imageResource->hasImage());
    EXPECT_FALSE(imageResource->getImage()->isNull());
    EXPECT_EQ(1, imageResource->getImage()->width());
    EXPECT_EQ(1, imageResource->getImage()->height());
    EXPECT_TRUE(client2->notifyFinishedCalled());
}

TEST(ImageResourceTest, CancelOnDecodeError)
{
    KURL testURL(ParsedURLString, "http://www.test.com/cancelTest.html");
    URLTestHelpers::registerMockedURLLoad(testURL, "cancelTest.html", "text/html");

    ResourceFetcher* fetcher = ResourceFetcher::create(ImageResourceTestMockFetchContext::create());
    FetchRequest request(testURL, FetchInitiatorInfo());
    ImageResource* cachedImage = ImageResource::fetch(request, fetcher);
    Platform::current()->getURLLoaderMockFactory()->unregisterURL(testURL);

    cachedImage->loader()->didReceiveResponse(nullptr, WrappedResourceResponse(ResourceResponse(testURL, "image/jpeg", 18, nullAtom, String())), nullptr);
    cachedImage->loader()->didReceiveData(nullptr, "notactuallyanimage", 18, 18, 18);
    EXPECT_EQ(Resource::DecodeError, cachedImage->getStatus());
    EXPECT_FALSE(cachedImage->isLoading());
}

} // namespace blink
