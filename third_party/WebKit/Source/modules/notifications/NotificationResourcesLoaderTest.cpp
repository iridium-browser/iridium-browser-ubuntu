// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/notifications/NotificationResourcesLoader.h"

#include <memory>
#include "core/testing/DummyPageHolder.h"
#include "platform/heap/Heap.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/modules/notifications/WebNotificationConstants.h"
#include "public/platform/modules/notifications/WebNotificationData.h"
#include "public/platform/modules/notifications/WebNotificationResources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Functional.h"
#include "wtf/text/WTFString.h"

namespace blink {
namespace {

constexpr char kBaseUrl[] = "http://test.com/";
constexpr char kBaseDir[] = "notifications/";
constexpr char kIcon48x48[] = "48x48.png";
constexpr char kIcon100x100[] = "100x100.png";
constexpr char kIcon110x110[] = "110x110.png";
constexpr char kIcon120x120[] = "120x120.png";
constexpr char kIcon500x500[] = "500x500.png";
constexpr char kIcon3000x1000[] = "3000x1000.png";
constexpr char kIcon3000x2000[] = "3000x2000.png";

class NotificationResourcesLoaderTest : public ::testing::Test {
 public:
  NotificationResourcesLoaderTest()
      : m_page(DummyPageHolder::create()),
        m_loader(new NotificationResourcesLoader(
            bind(&NotificationResourcesLoaderTest::didFetchResources,
                 WTF::unretained(this)))) {}

  ~NotificationResourcesLoaderTest() override {
    m_loader->stop();
    Platform::current()
        ->getURLLoaderMockFactory()
        ->unregisterAllURLsAndClearMemoryCache();
  }

 protected:
  ExecutionContext* executionContext() const { return &m_page->document(); }

  NotificationResourcesLoader* loader() const { return m_loader.get(); }

  WebNotificationResources* resources() const { return m_resources.get(); }

  void didFetchResources(NotificationResourcesLoader* loader) {
    m_resources = loader->getResources();
  }

  // Registers a mocked url. When fetched, |fileName| will be loaded from the
  // test data directory.
  WebURL registerMockedURL(const String& fileName) {
    WebURL registeredUrl = URLTestHelpers::registerMockedURLLoadFromBase(
        kBaseUrl, testing::webTestDataPath(kBaseDir), fileName, "image/png");
    return registeredUrl;
  }

  // Registers a mocked url that will fail to be fetched, with a 404 error.
  WebURL registerMockedErrorURL(const String& fileName) {
    WebURL url(KURL(ParsedURLString, kBaseUrl + fileName));
    URLTestHelpers::registerMockedErrorURLLoad(url);
    return url;
  }

 private:
  std::unique_ptr<DummyPageHolder> m_page;
  Persistent<NotificationResourcesLoader> m_loader;
  std::unique_ptr<WebNotificationResources> m_resources;
};

TEST_F(NotificationResourcesLoaderTest, LoadMultipleResources) {
  WebNotificationData notificationData;
  notificationData.image = registerMockedURL(kIcon500x500);
  notificationData.icon = registerMockedURL(kIcon100x100);
  notificationData.badge = registerMockedURL(kIcon48x48);
  notificationData.actions =
      WebVector<WebNotificationAction>(static_cast<size_t>(2));
  notificationData.actions[0].icon = registerMockedURL(kIcon110x110);
  notificationData.actions[1].icon = registerMockedURL(kIcon120x120);

  ASSERT_FALSE(resources());

  loader()->start(executionContext(), notificationData);
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();

  ASSERT_TRUE(resources());

  ASSERT_FALSE(resources()->image.drawsNothing());
  ASSERT_EQ(500, resources()->image.width());
  ASSERT_EQ(500, resources()->image.height());

  ASSERT_FALSE(resources()->icon.drawsNothing());
  ASSERT_EQ(100, resources()->icon.width());

  ASSERT_FALSE(resources()->badge.drawsNothing());
  ASSERT_EQ(48, resources()->badge.width());

  ASSERT_EQ(2u, resources()->actionIcons.size());
  ASSERT_FALSE(resources()->actionIcons[0].drawsNothing());
  ASSERT_EQ(110, resources()->actionIcons[0].width());
  ASSERT_FALSE(resources()->actionIcons[1].drawsNothing());
  ASSERT_EQ(120, resources()->actionIcons[1].width());
}

TEST_F(NotificationResourcesLoaderTest, LargeIconsAreScaledDown) {
  WebNotificationData notificationData;
  notificationData.icon = registerMockedURL(kIcon500x500);
  notificationData.badge = notificationData.icon;
  notificationData.actions =
      WebVector<WebNotificationAction>(static_cast<size_t>(1));
  notificationData.actions[0].icon = notificationData.icon;

  ASSERT_FALSE(resources());

  loader()->start(executionContext(), notificationData);
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();

  ASSERT_TRUE(resources());

  ASSERT_FALSE(resources()->icon.drawsNothing());
  ASSERT_EQ(kWebNotificationMaxIconSizePx, resources()->icon.width());
  ASSERT_EQ(kWebNotificationMaxIconSizePx, resources()->icon.height());

  ASSERT_FALSE(resources()->badge.drawsNothing());
  ASSERT_EQ(kWebNotificationMaxBadgeSizePx, resources()->badge.width());
  ASSERT_EQ(kWebNotificationMaxBadgeSizePx, resources()->badge.height());

  ASSERT_EQ(1u, resources()->actionIcons.size());
  ASSERT_FALSE(resources()->actionIcons[0].drawsNothing());
  ASSERT_EQ(kWebNotificationMaxActionIconSizePx,
            resources()->actionIcons[0].width());
  ASSERT_EQ(kWebNotificationMaxActionIconSizePx,
            resources()->actionIcons[0].height());
}

TEST_F(NotificationResourcesLoaderTest, DownscalingPreserves3_1AspectRatio) {
  WebNotificationData notificationData;
  notificationData.image = registerMockedURL(kIcon3000x1000);

  ASSERT_FALSE(resources());

  loader()->start(executionContext(), notificationData);
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();

  ASSERT_TRUE(resources());

  ASSERT_FALSE(resources()->image.drawsNothing());
  ASSERT_EQ(kWebNotificationMaxImageWidthPx, resources()->image.width());
  ASSERT_EQ(kWebNotificationMaxImageWidthPx / 3, resources()->image.height());
}

TEST_F(NotificationResourcesLoaderTest, DownscalingPreserves3_2AspectRatio) {
  WebNotificationData notificationData;
  notificationData.image = registerMockedURL(kIcon3000x2000);

  ASSERT_FALSE(resources());

  loader()->start(executionContext(), notificationData);
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();

  ASSERT_TRUE(resources());

  ASSERT_FALSE(resources()->image.drawsNothing());
  ASSERT_EQ(kWebNotificationMaxImageHeightPx * 3 / 2,
            resources()->image.width());
  ASSERT_EQ(kWebNotificationMaxImageHeightPx, resources()->image.height());
}

TEST_F(NotificationResourcesLoaderTest, EmptyDataYieldsEmptyResources) {
  WebNotificationData notificationData;

  ASSERT_FALSE(resources());

  loader()->start(executionContext(), notificationData);
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();

  ASSERT_TRUE(resources());

  ASSERT_TRUE(resources()->image.drawsNothing());
  ASSERT_TRUE(resources()->icon.drawsNothing());
  ASSERT_TRUE(resources()->badge.drawsNothing());
  ASSERT_EQ(0u, resources()->actionIcons.size());
}

TEST_F(NotificationResourcesLoaderTest, EmptyResourcesIfAllImagesFailToLoad) {
  WebNotificationData notificationData;
  notificationData.image = notificationData.icon;
  notificationData.icon = registerMockedErrorURL(kIcon100x100);
  notificationData.badge = notificationData.icon;
  notificationData.actions =
      WebVector<WebNotificationAction>(static_cast<size_t>(1));
  notificationData.actions[0].icon = notificationData.icon;

  ASSERT_FALSE(resources());

  loader()->start(executionContext(), notificationData);
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();

  ASSERT_TRUE(resources());

  // The test received resources but they are all empty. This ensures that a
  // notification can still be shown even if the images fail to load.
  ASSERT_TRUE(resources()->image.drawsNothing());
  ASSERT_TRUE(resources()->icon.drawsNothing());
  ASSERT_TRUE(resources()->badge.drawsNothing());
  ASSERT_EQ(1u, resources()->actionIcons.size());
  ASSERT_TRUE(resources()->actionIcons[0].drawsNothing());
}

TEST_F(NotificationResourcesLoaderTest, OneImageFailsToLoad) {
  WebNotificationData notificationData;
  notificationData.icon = registerMockedURL(kIcon100x100);
  notificationData.badge = registerMockedErrorURL(kIcon48x48);

  ASSERT_FALSE(resources());

  loader()->start(executionContext(), notificationData);
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();

  ASSERT_TRUE(resources());

  // The test received resources even though one image failed to load. This
  // ensures that a notification can still be shown, though slightly degraded.
  ASSERT_TRUE(resources()->image.drawsNothing());
  ASSERT_FALSE(resources()->icon.drawsNothing());
  ASSERT_EQ(100, resources()->icon.width());
  ASSERT_TRUE(resources()->badge.drawsNothing());
  ASSERT_EQ(0u, resources()->actionIcons.size());
}

TEST_F(NotificationResourcesLoaderTest, StopYieldsNoResources) {
  WebNotificationData notificationData;
  notificationData.image = registerMockedURL(kIcon500x500);
  notificationData.icon = registerMockedURL(kIcon100x100);
  notificationData.badge = registerMockedURL(kIcon48x48);
  notificationData.actions =
      WebVector<WebNotificationAction>(static_cast<size_t>(2));
  notificationData.actions[0].icon = registerMockedURL(kIcon110x110);
  notificationData.actions[1].icon = registerMockedURL(kIcon120x120);

  ASSERT_FALSE(resources());

  loader()->start(executionContext(), notificationData);

  // Check that starting the loader did not synchronously fail, providing
  // empty resources. The requests should be pending now.
  ASSERT_FALSE(resources());

  // The loader would stop e.g. when the execution context is destroyed or
  // when the loader is about to be destroyed, as a pre-finalizer.
  loader()->stop();
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();

  // Loading should have been cancelled when |stop| was called so no resources
  // should have been received by the test even though
  // |serveAsynchronousRequests| was called.
  ASSERT_FALSE(resources());
}

}  // namespace
}  // namespace blink
