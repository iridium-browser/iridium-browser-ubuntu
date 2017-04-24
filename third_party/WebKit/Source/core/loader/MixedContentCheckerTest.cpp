// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/MixedContentChecker.h"

#include "core/frame/Settings.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebMixedContent.h"
#include "public/platform/WebMixedContentContextType.h"
#include "testing/gmock/include/gmock/gmock-generated-function-mockers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/RefPtr.h"
#include <base/macros.h>
#include <memory>

namespace blink {

// Tests that MixedContentChecker::isMixedContent correctly detects or ignores
// many cases where there is or there is not mixed content, respectively.
// Note: Renderer side version of
// MixedContentNavigationThrottleTest.IsMixedContent. Must be kept in sync
// manually!
TEST(MixedContentCheckerTest, IsMixedContent) {
  struct TestCase {
    const char* origin;
    const char* target;
    bool expectation;
  } cases[] = {
      {"http://example.com/foo", "http://example.com/foo", false},
      {"http://example.com/foo", "https://example.com/foo", false},
      {"http://example.com/foo", "data:text/html,<p>Hi!</p>", false},
      {"http://example.com/foo", "about:blank", false},
      {"https://example.com/foo", "https://example.com/foo", false},
      {"https://example.com/foo", "wss://example.com/foo", false},
      {"https://example.com/foo", "data:text/html,<p>Hi!</p>", false},
      {"https://example.com/foo", "http://127.0.0.1/", false},
      {"https://example.com/foo", "http://[::1]/", false},
      {"https://example.com/foo", "blob:https://example.com/foo", false},
      {"https://example.com/foo", "blob:http://example.com/foo", false},
      {"https://example.com/foo", "blob:null/foo", false},
      {"https://example.com/foo", "filesystem:https://example.com/foo", false},
      {"https://example.com/foo", "filesystem:http://example.com/foo", false},

      {"https://example.com/foo", "http://example.com/foo", true},
      {"https://example.com/foo", "http://google.com/foo", true},
      {"https://example.com/foo", "ws://example.com/foo", true},
      {"https://example.com/foo", "ws://google.com/foo", true},
      {"https://example.com/foo", "http://192.168.1.1/", true},
      {"https://example.com/foo", "http://localhost/", true},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(::testing::Message() << "Origin: " << test.origin
                                      << ", Target: " << test.target
                                      << ", Expectation: " << test.expectation);
    KURL originUrl(KURL(), test.origin);
    RefPtr<SecurityOrigin> securityOrigin(SecurityOrigin::create(originUrl));
    KURL targetUrl(KURL(), test.target);
    EXPECT_EQ(test.expectation, MixedContentChecker::isMixedContent(
                                    securityOrigin.get(), targetUrl));
  }
}

TEST(MixedContentCheckerTest, ContextTypeForInspector) {
  std::unique_ptr<DummyPageHolder> dummyPageHolder =
      DummyPageHolder::create(IntSize(1, 1));
  dummyPageHolder->frame().document()->setSecurityOrigin(
      SecurityOrigin::createFromString("http://example.test"));

  ResourceRequest notMixedContent("https://example.test/foo.jpg");
  notMixedContent.setFrameType(WebURLRequest::FrameTypeAuxiliary);
  notMixedContent.setRequestContext(WebURLRequest::RequestContextScript);
  EXPECT_EQ(WebMixedContentContextType::NotMixedContent,
            MixedContentChecker::contextTypeForInspector(
                &dummyPageHolder->frame(), notMixedContent));

  dummyPageHolder->frame().document()->setSecurityOrigin(
      SecurityOrigin::createFromString("https://example.test"));
  EXPECT_EQ(WebMixedContentContextType::NotMixedContent,
            MixedContentChecker::contextTypeForInspector(
                &dummyPageHolder->frame(), notMixedContent));

  ResourceRequest blockableMixedContent("http://example.test/foo.jpg");
  blockableMixedContent.setFrameType(WebURLRequest::FrameTypeAuxiliary);
  blockableMixedContent.setRequestContext(WebURLRequest::RequestContextScript);
  EXPECT_EQ(WebMixedContentContextType::Blockable,
            MixedContentChecker::contextTypeForInspector(
                &dummyPageHolder->frame(), blockableMixedContent));

  ResourceRequest optionallyBlockableMixedContent(
      "http://example.test/foo.jpg");
  blockableMixedContent.setFrameType(WebURLRequest::FrameTypeAuxiliary);
  blockableMixedContent.setRequestContext(WebURLRequest::RequestContextImage);
  EXPECT_EQ(WebMixedContentContextType::OptionallyBlockable,
            MixedContentChecker::contextTypeForInspector(
                &dummyPageHolder->frame(), blockableMixedContent));
}

namespace {

class MockLocalFrameClient : public EmptyLocalFrameClient {
 public:
  MockLocalFrameClient() : EmptyLocalFrameClient() {}
  MOCK_METHOD1(didDisplayContentWithCertificateErrors, void(const KURL&));
  MOCK_METHOD1(didRunContentWithCertificateErrors, void(const KURL&));
};

}  // namespace

TEST(MixedContentCheckerTest, HandleCertificateError) {
  MockLocalFrameClient* client = new MockLocalFrameClient;
  std::unique_ptr<DummyPageHolder> dummyPageHolder =
      DummyPageHolder::create(IntSize(1, 1), nullptr, client);

  KURL mainResourceUrl(KURL(), "https://example.test");
  KURL displayedUrl(KURL(), "https://example-displayed.test");
  KURL ranUrl(KURL(), "https://example-ran.test");

  dummyPageHolder->frame().document()->setURL(mainResourceUrl);
  ResourceResponse response1;
  response1.setURL(ranUrl);
  EXPECT_CALL(*client, didRunContentWithCertificateErrors(ranUrl));
  MixedContentChecker::handleCertificateError(
      &dummyPageHolder->frame(), response1, WebURLRequest::FrameTypeNone,
      WebURLRequest::RequestContextScript);

  ResourceResponse response2;
  WebURLRequest::RequestContext requestContext =
      WebURLRequest::RequestContextImage;
  ASSERT_EQ(
      WebMixedContentContextType::OptionallyBlockable,
      WebMixedContent::contextTypeFromRequestContext(
          requestContext, dummyPageHolder->frame()
                              .settings()
                              ->getStrictMixedContentCheckingForPlugin()));
  response2.setURL(displayedUrl);
  EXPECT_CALL(*client, didDisplayContentWithCertificateErrors(displayedUrl));
  MixedContentChecker::handleCertificateError(
      &dummyPageHolder->frame(), response2, WebURLRequest::FrameTypeNone,
      requestContext);
}

}  // namespace blink
