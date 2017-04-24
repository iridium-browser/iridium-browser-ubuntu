/*
 * Copyright (c) 2015, Google Inc. All rights reserved.
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

#include "core/loader/FrameFetchContext.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameOwner.h"
#include "core/frame/FrameTypes.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/EmptyClients.h"
#include "core/loader/SubresourceFilter.h"
#include "core/page/Page.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/loader/fetch/FetchInitiatorInfo.h"
#include "platform/loader/fetch/UniqueIdentifier.h"
#include "platform/loader/testing/MockResource.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityViolationReportingPolicy.h"
#include "public/platform/WebAddressSpace.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebDocumentSubresourceFilter.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class StubLocalFrameClientWithParent final : public EmptyLocalFrameClient {
 public:
  static StubLocalFrameClientWithParent* create(Frame* parent) {
    return new StubLocalFrameClientWithParent(parent);
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_parent);
    EmptyLocalFrameClient::trace(visitor);
  }

  Frame* parent() const override { return m_parent.get(); }

 private:
  explicit StubLocalFrameClientWithParent(Frame* parent) : m_parent(parent) {}

  Member<Frame> m_parent;
};

class MockLocalFrameClient : public EmptyLocalFrameClient {
 public:
  MockLocalFrameClient() : EmptyLocalFrameClient() {}
  MOCK_METHOD1(didDisplayContentWithCertificateErrors, void(const KURL&));
  MOCK_METHOD2(dispatchDidLoadResourceFromMemoryCache,
               void(const ResourceRequest&, const ResourceResponse&));
};

class FixedPolicySubresourceFilter : public WebDocumentSubresourceFilter {
 public:
  FixedPolicySubresourceFilter(LoadPolicy policy, int* filteredLoadCounter)
      : m_policy(policy), m_filteredLoadCounter(filteredLoadCounter) {}

  LoadPolicy getLoadPolicy(const WebURL& resourceUrl,
                           WebURLRequest::RequestContext) override {
    return m_policy;
  }

  void reportDisallowedLoad() override { ++*m_filteredLoadCounter; }

 private:
  const LoadPolicy m_policy;
  int* m_filteredLoadCounter;
};

class FrameFetchContextTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dummyPageHolder = DummyPageHolder::create(IntSize(500, 500));
    dummyPageHolder->page().setDeviceScaleFactorDeprecated(1.0);
    document = &dummyPageHolder->document();
    fetchContext =
        static_cast<FrameFetchContext*>(&document->fetcher()->context());
    owner = DummyFrameOwner::create();
    FrameFetchContext::provideDocumentToContext(*fetchContext, document.get());
  }

  void TearDown() override {
    if (childFrame)
      childFrame->detach(FrameDetachType::Remove);
  }

  FrameFetchContext* createChildFrame() {
    childClient = StubLocalFrameClientWithParent::create(document->frame());
    childFrame = LocalFrame::create(childClient.get(),
                                    document->frame()->host(), owner.get());
    childFrame->setView(FrameView::create(*childFrame, IntSize(500, 500)));
    childFrame->init();
    childDocument = childFrame->document();
    FrameFetchContext* childFetchContext = static_cast<FrameFetchContext*>(
        &childFrame->loader().documentLoader()->fetcher()->context());
    FrameFetchContext::provideDocumentToContext(*childFetchContext,
                                                childDocument.get());
    return childFetchContext;
  }

  std::unique_ptr<DummyPageHolder> dummyPageHolder;
  // We don't use the DocumentLoader directly in any tests, but need to keep it
  // around as long as the ResourceFetcher and Document live due to indirect
  // usage.
  Persistent<Document> document;
  Persistent<FrameFetchContext> fetchContext;

  Persistent<StubLocalFrameClientWithParent> childClient;
  Persistent<LocalFrame> childFrame;
  Persistent<Document> childDocument;
  Persistent<DummyFrameOwner> owner;
};

class FrameFetchContextSubresourceFilterTest : public FrameFetchContextTest {
 protected:
  void SetUp() override {
    FrameFetchContextTest::SetUp();
    m_filteredLoadCallbackCounter = 0;
  }

  void TearDown() override {
    document->loader()->setSubresourceFilter(nullptr);
    FrameFetchContextTest::TearDown();
  }

  int getFilteredLoadCallCount() const { return m_filteredLoadCallbackCounter; }

  void setFilterPolicy(WebDocumentSubresourceFilter::LoadPolicy policy) {
    document->loader()->setSubresourceFilter(SubresourceFilter::create(
        document->loader(), WTF::makeUnique<FixedPolicySubresourceFilter>(
                                policy, &m_filteredLoadCallbackCounter)));
  }

  ResourceRequestBlockedReason canRequest() {
    return canRequestInternal(SecurityViolationReportingPolicy::Report);
  }

  ResourceRequestBlockedReason canRequestPreload() {
    return canRequestInternal(
        SecurityViolationReportingPolicy::SuppressReporting);
  }

 private:
  ResourceRequestBlockedReason canRequestInternal(
      SecurityViolationReportingPolicy reportingPolicy) {
    KURL inputURL(ParsedURLString, "http://example.com/");
    ResourceRequest resourceRequest(inputURL);
    return fetchContext->canRequest(
        Resource::Image, resourceRequest, inputURL, ResourceLoaderOptions(),
        reportingPolicy, FetchRequest::UseDefaultOriginRestrictionForType);
  }

  int m_filteredLoadCallbackCounter;
};

// This test class sets up a mock frame loader client.
class FrameFetchContextMockedLocalFrameClientTest
    : public FrameFetchContextTest {
 protected:
  void SetUp() override {
    url = KURL(KURL(), "https://example.test/foo");
    mainResourceUrl = KURL(KURL(), "https://www.example.test");
    client = new testing::NiceMock<MockLocalFrameClient>();
    dummyPageHolder =
        DummyPageHolder::create(IntSize(500, 500), nullptr, client);
    dummyPageHolder->page().setDeviceScaleFactorDeprecated(1.0);
    document = &dummyPageHolder->document();
    document->setURL(mainResourceUrl);
    fetchContext =
        static_cast<FrameFetchContext*>(&document->fetcher()->context());
    owner = DummyFrameOwner::create();
    FrameFetchContext::provideDocumentToContext(*fetchContext, document.get());
  }

  KURL url;
  KURL mainResourceUrl;

  Persistent<testing::NiceMock<MockLocalFrameClient>> client;
};

class FrameFetchContextModifyRequestTest : public FrameFetchContextTest {
 public:
  FrameFetchContextModifyRequestTest()
      : exampleOrigin(SecurityOrigin::create(
            KURL(ParsedURLString, "https://example.test/"))),
        secureOrigin(SecurityOrigin::create(
            KURL(ParsedURLString, "https://secureorigin.test/image.png"))) {}

 protected:
  void expectUpgrade(const char* input, const char* expected) {
    expectUpgrade(input, WebURLRequest::RequestContextScript,
                  WebURLRequest::FrameTypeNone, expected);
  }

  void expectUpgrade(const char* input,
                     WebURLRequest::RequestContext requestContext,
                     WebURLRequest::FrameType frameType,
                     const char* expected) {
    KURL inputURL(ParsedURLString, input);
    KURL expectedURL(ParsedURLString, expected);

    ResourceRequest resourceRequest(inputURL);
    resourceRequest.setRequestContext(requestContext);
    resourceRequest.setFrameType(frameType);

    fetchContext->modifyRequestForCSP(resourceRequest);

    EXPECT_EQ(expectedURL.getString(), resourceRequest.url().getString());
    EXPECT_EQ(expectedURL.protocol(), resourceRequest.url().protocol());
    EXPECT_EQ(expectedURL.host(), resourceRequest.url().host());
    EXPECT_EQ(expectedURL.port(), resourceRequest.url().port());
    EXPECT_EQ(expectedURL.hasPort(), resourceRequest.url().hasPort());
    EXPECT_EQ(expectedURL.path(), resourceRequest.url().path());
  }

  void expectUpgradeInsecureRequestHeader(const char* input,
                                          WebURLRequest::FrameType frameType,
                                          bool shouldPrefer) {
    KURL inputURL(ParsedURLString, input);

    ResourceRequest resourceRequest(inputURL);
    resourceRequest.setRequestContext(WebURLRequest::RequestContextScript);
    resourceRequest.setFrameType(frameType);

    fetchContext->modifyRequestForCSP(resourceRequest);

    EXPECT_EQ(
        shouldPrefer ? String("1") : String(),
        resourceRequest.httpHeaderField(HTTPNames::Upgrade_Insecure_Requests));

    // Calling modifyRequestForCSP more than once shouldn't affect the
    // header.
    if (shouldPrefer) {
      fetchContext->modifyRequestForCSP(resourceRequest);
      EXPECT_EQ("1", resourceRequest.httpHeaderField(
                         HTTPNames::Upgrade_Insecure_Requests));
    }
  }

  void expectSetEmbeddingCSPRequestHeader(
      const char* input,
      WebURLRequest::FrameType frameType,
      const AtomicString& expectedEmbeddingCSP) {
    KURL inputURL(ParsedURLString, input);
    ResourceRequest resourceRequest(inputURL);
    resourceRequest.setRequestContext(WebURLRequest::RequestContextScript);
    resourceRequest.setFrameType(frameType);

    fetchContext->modifyRequestForCSP(resourceRequest);

    EXPECT_EQ(expectedEmbeddingCSP,
              resourceRequest.httpHeaderField(HTTPNames::Embedding_CSP));
  }

  void setFrameOwnerBasedOnFrameType(WebURLRequest::FrameType frameType,
                                     HTMLIFrameElement* iframe,
                                     const AtomicString& potentialValue) {
    if (frameType != WebURLRequest::FrameTypeNested) {
      document->frame()->setOwner(nullptr);
      return;
    }

    iframe->setAttribute(HTMLNames::cspAttr, potentialValue);
    document->frame()->setOwner(iframe);
  }

  RefPtr<SecurityOrigin> exampleOrigin;
  RefPtr<SecurityOrigin> secureOrigin;
};

TEST_F(FrameFetchContextModifyRequestTest, UpgradeInsecureResourceRequests) {
  struct TestCase {
    const char* original;
    const char* upgraded;
  } tests[] = {
      {"http://example.test/image.png", "https://example.test/image.png"},
      {"http://example.test:80/image.png",
       "https://example.test:443/image.png"},
      {"http://example.test:1212/image.png",
       "https://example.test:1212/image.png"},

      {"https://example.test/image.png", "https://example.test/image.png"},
      {"https://example.test:80/image.png",
       "https://example.test:80/image.png"},
      {"https://example.test:1212/image.png",
       "https://example.test:1212/image.png"},

      {"ftp://example.test/image.png", "ftp://example.test/image.png"},
      {"ftp://example.test:21/image.png", "ftp://example.test:21/image.png"},
      {"ftp://example.test:1212/image.png",
       "ftp://example.test:1212/image.png"},
  };

  FrameFetchContext::provideDocumentToContext(*fetchContext, document.get());
  document->setInsecureRequestPolicy(kUpgradeInsecureRequests);

  for (const auto& test : tests) {
    document->insecureNavigationsToUpgrade()->clear();

    // We always upgrade for FrameTypeNone and FrameTypeNested.
    expectUpgrade(test.original, WebURLRequest::RequestContextScript,
                  WebURLRequest::FrameTypeNone, test.upgraded);
    expectUpgrade(test.original, WebURLRequest::RequestContextScript,
                  WebURLRequest::FrameTypeNested, test.upgraded);

    // We do not upgrade for FrameTypeTopLevel or FrameTypeAuxiliary...
    expectUpgrade(test.original, WebURLRequest::RequestContextScript,
                  WebURLRequest::FrameTypeTopLevel, test.original);
    expectUpgrade(test.original, WebURLRequest::RequestContextScript,
                  WebURLRequest::FrameTypeAuxiliary, test.original);

    // unless the request context is RequestContextForm.
    expectUpgrade(test.original, WebURLRequest::RequestContextForm,
                  WebURLRequest::FrameTypeTopLevel, test.upgraded);
    expectUpgrade(test.original, WebURLRequest::RequestContextForm,
                  WebURLRequest::FrameTypeAuxiliary, test.upgraded);

    // Or unless the host of the resource is in the document's
    // InsecureNavigationsSet:
    document->addInsecureNavigationUpgrade(
        exampleOrigin->host().impl()->hash());
    expectUpgrade(test.original, WebURLRequest::RequestContextScript,
                  WebURLRequest::FrameTypeTopLevel, test.upgraded);
    expectUpgrade(test.original, WebURLRequest::RequestContextScript,
                  WebURLRequest::FrameTypeAuxiliary, test.upgraded);
  }
}

TEST_F(FrameFetchContextModifyRequestTest,
       DoNotUpgradeInsecureResourceRequests) {
  FrameFetchContext::provideDocumentToContext(*fetchContext, document.get());
  document->setSecurityOrigin(secureOrigin);
  document->setInsecureRequestPolicy(kLeaveInsecureRequestsAlone);

  expectUpgrade("http://example.test/image.png",
                "http://example.test/image.png");
  expectUpgrade("http://example.test:80/image.png",
                "http://example.test:80/image.png");
  expectUpgrade("http://example.test:1212/image.png",
                "http://example.test:1212/image.png");

  expectUpgrade("https://example.test/image.png",
                "https://example.test/image.png");
  expectUpgrade("https://example.test:80/image.png",
                "https://example.test:80/image.png");
  expectUpgrade("https://example.test:1212/image.png",
                "https://example.test:1212/image.png");

  expectUpgrade("ftp://example.test/image.png", "ftp://example.test/image.png");
  expectUpgrade("ftp://example.test:21/image.png",
                "ftp://example.test:21/image.png");
  expectUpgrade("ftp://example.test:1212/image.png",
                "ftp://example.test:1212/image.png");
}

TEST_F(FrameFetchContextModifyRequestTest, SendUpgradeInsecureRequestHeader) {
  struct TestCase {
    const char* toRequest;
    WebURLRequest::FrameType frameType;
    bool shouldPrefer;
  } tests[] = {
      {"http://example.test/page.html", WebURLRequest::FrameTypeAuxiliary,
       true},
      {"http://example.test/page.html", WebURLRequest::FrameTypeNested, true},
      {"http://example.test/page.html", WebURLRequest::FrameTypeNone, false},
      {"http://example.test/page.html", WebURLRequest::FrameTypeTopLevel, true},
      {"https://example.test/page.html", WebURLRequest::FrameTypeAuxiliary,
       true},
      {"https://example.test/page.html", WebURLRequest::FrameTypeNested, true},
      {"https://example.test/page.html", WebURLRequest::FrameTypeNone, false},
      {"https://example.test/page.html", WebURLRequest::FrameTypeTopLevel,
       true}};

  // This should work correctly both when the FrameFetchContext has a Document,
  // and when it doesn't (e.g. during main frame navigations), so run through
  // the tests both before and after providing a document to the context.
  for (const auto& test : tests) {
    document->setInsecureRequestPolicy(kLeaveInsecureRequestsAlone);
    expectUpgradeInsecureRequestHeader(test.toRequest, test.frameType,
                                       test.shouldPrefer);

    document->setInsecureRequestPolicy(kUpgradeInsecureRequests);
    expectUpgradeInsecureRequestHeader(test.toRequest, test.frameType,
                                       test.shouldPrefer);
  }

  FrameFetchContext::provideDocumentToContext(*fetchContext, document.get());

  for (const auto& test : tests) {
    document->setInsecureRequestPolicy(kLeaveInsecureRequestsAlone);
    expectUpgradeInsecureRequestHeader(test.toRequest, test.frameType,
                                       test.shouldPrefer);

    document->setInsecureRequestPolicy(kUpgradeInsecureRequests);
    expectUpgradeInsecureRequestHeader(test.toRequest, test.frameType,
                                       test.shouldPrefer);
  }
}

TEST_F(FrameFetchContextModifyRequestTest, SendEmbeddingCSPHeader) {
  struct TestCase {
    const char* toRequest;
    WebURLRequest::FrameType frameType;
  } tests[] = {
      {"https://example.test/page.html", WebURLRequest::FrameTypeAuxiliary},
      {"https://example.test/page.html", WebURLRequest::FrameTypeNested},
      {"https://example.test/page.html", WebURLRequest::FrameTypeNone},
      {"https://example.test/page.html", WebURLRequest::FrameTypeTopLevel}};

  HTMLIFrameElement* iframe = HTMLIFrameElement::create(*document);
  const AtomicString& requiredCSP = AtomicString("default-src 'none'");
  const AtomicString& anotherRequiredCSP = AtomicString("default-src 'self'");

  for (const auto& test : tests) {
    setFrameOwnerBasedOnFrameType(test.frameType, iframe, requiredCSP);
    expectSetEmbeddingCSPRequestHeader(
        test.toRequest, test.frameType,
        test.frameType == WebURLRequest::FrameTypeNested ? requiredCSP
                                                         : nullAtom);

    setFrameOwnerBasedOnFrameType(test.frameType, iframe, anotherRequiredCSP);
    expectSetEmbeddingCSPRequestHeader(
        test.toRequest, test.frameType,
        test.frameType == WebURLRequest::FrameTypeNested ? anotherRequiredCSP
                                                         : nullAtom);
  }
}

class FrameFetchContextHintsTest : public FrameFetchContextTest {
 public:
  FrameFetchContextHintsTest() {}

 protected:
  void expectHeader(const char* input,
                    const char* headerName,
                    bool isPresent,
                    const char* headerValue,
                    float width = 0) {
    ClientHintsPreferences hintsPreferences;

    FetchRequest::ResourceWidth resourceWidth;
    if (width > 0) {
      resourceWidth.width = width;
      resourceWidth.isSet = true;
    }

    KURL inputURL(ParsedURLString, input);
    ResourceRequest resourceRequest(inputURL);

    fetchContext->addClientHintsIfNecessary(hintsPreferences, resourceWidth,
                                            resourceRequest);

    EXPECT_EQ(isPresent ? String(headerValue) : String(),
              resourceRequest.httpHeaderField(headerName));
  }
};

TEST_F(FrameFetchContextHintsTest, MonitorDPRHints) {
  expectHeader("http://www.example.com/1.gif", "DPR", false, "");
  ClientHintsPreferences preferences;
  preferences.setShouldSendDPR(true);
  document->clientHintsPreferences().updateFrom(preferences);
  expectHeader("http://www.example.com/1.gif", "DPR", true, "1");
  dummyPageHolder->page().setDeviceScaleFactorDeprecated(2.5);
  expectHeader("http://www.example.com/1.gif", "DPR", true, "2.5");
  expectHeader("http://www.example.com/1.gif", "Width", false, "");
  expectHeader("http://www.example.com/1.gif", "Viewport-Width", false, "");
}

TEST_F(FrameFetchContextHintsTest, MonitorResourceWidthHints) {
  expectHeader("http://www.example.com/1.gif", "Width", false, "");
  ClientHintsPreferences preferences;
  preferences.setShouldSendResourceWidth(true);
  document->clientHintsPreferences().updateFrom(preferences);
  expectHeader("http://www.example.com/1.gif", "Width", true, "500", 500);
  expectHeader("http://www.example.com/1.gif", "Width", true, "667", 666.6666);
  expectHeader("http://www.example.com/1.gif", "DPR", false, "");
  dummyPageHolder->page().setDeviceScaleFactorDeprecated(2.5);
  expectHeader("http://www.example.com/1.gif", "Width", true, "1250", 500);
  expectHeader("http://www.example.com/1.gif", "Width", true, "1667", 666.6666);
}

TEST_F(FrameFetchContextHintsTest, MonitorViewportWidthHints) {
  expectHeader("http://www.example.com/1.gif", "Viewport-Width", false, "");
  ClientHintsPreferences preferences;
  preferences.setShouldSendViewportWidth(true);
  document->clientHintsPreferences().updateFrom(preferences);
  expectHeader("http://www.example.com/1.gif", "Viewport-Width", true, "500");
  dummyPageHolder->frameView().setLayoutSizeFixedToFrameSize(false);
  dummyPageHolder->frameView().setLayoutSize(IntSize(800, 800));
  expectHeader("http://www.example.com/1.gif", "Viewport-Width", true, "800");
  expectHeader("http://www.example.com/1.gif", "Viewport-Width", true, "800",
               666.6666);
  expectHeader("http://www.example.com/1.gif", "DPR", false, "");
}

TEST_F(FrameFetchContextHintsTest, MonitorAllHints) {
  expectHeader("http://www.example.com/1.gif", "DPR", false, "");
  expectHeader("http://www.example.com/1.gif", "Viewport-Width", false, "");
  expectHeader("http://www.example.com/1.gif", "Width", false, "");

  ClientHintsPreferences preferences;
  preferences.setShouldSendDPR(true);
  preferences.setShouldSendResourceWidth(true);
  preferences.setShouldSendViewportWidth(true);
  document->clientHintsPreferences().updateFrom(preferences);
  expectHeader("http://www.example.com/1.gif", "DPR", true, "1");
  expectHeader("http://www.example.com/1.gif", "Width", true, "400", 400);
  expectHeader("http://www.example.com/1.gif", "Viewport-Width", true, "500");
}

TEST_F(FrameFetchContextTest, MainResource) {
  // Default case
  ResourceRequest request("http://www.example.com");
  EXPECT_EQ(WebCachePolicy::UseProtocolCachePolicy,
            fetchContext->resourceRequestCachePolicy(
                request, Resource::MainResource, FetchRequest::NoDefer));

  // Post
  ResourceRequest postRequest("http://www.example.com");
  postRequest.setHTTPMethod("POST");
  EXPECT_EQ(WebCachePolicy::ValidatingCacheData,
            fetchContext->resourceRequestCachePolicy(
                postRequest, Resource::MainResource, FetchRequest::NoDefer));

  // Re-post
  document->loader()->setLoadType(FrameLoadTypeBackForward);
  EXPECT_EQ(WebCachePolicy::ReturnCacheDataDontLoad,
            fetchContext->resourceRequestCachePolicy(
                postRequest, Resource::MainResource, FetchRequest::NoDefer));

  // FrameLoadTypeReloadMainResource
  document->loader()->setLoadType(FrameLoadTypeReloadMainResource);
  EXPECT_EQ(WebCachePolicy::ValidatingCacheData,
            fetchContext->resourceRequestCachePolicy(
                request, Resource::MainResource, FetchRequest::NoDefer));

  // Conditional request
  document->loader()->setLoadType(FrameLoadTypeStandard);
  ResourceRequest conditional("http://www.example.com");
  conditional.setHTTPHeaderField(HTTPNames::If_Modified_Since, "foo");
  EXPECT_EQ(WebCachePolicy::ValidatingCacheData,
            fetchContext->resourceRequestCachePolicy(
                conditional, Resource::MainResource, FetchRequest::NoDefer));

  // Set up a child frame
  FrameFetchContext* childFetchContext = createChildFrame();

  // Child frame as part of back/forward
  document->loader()->setLoadType(FrameLoadTypeBackForward);
  EXPECT_EQ(WebCachePolicy::ReturnCacheDataElseLoad,
            childFetchContext->resourceRequestCachePolicy(
                request, Resource::MainResource, FetchRequest::NoDefer));

  // Child frame as part of reload
  document->loader()->setLoadType(FrameLoadTypeReload);
  EXPECT_EQ(WebCachePolicy::ValidatingCacheData,
            childFetchContext->resourceRequestCachePolicy(
                request, Resource::MainResource, FetchRequest::NoDefer));

  // Child frame as part of reload bypassing cache
  document->loader()->setLoadType(FrameLoadTypeReloadBypassingCache);
  EXPECT_EQ(WebCachePolicy::BypassingCache,
            childFetchContext->resourceRequestCachePolicy(
                request, Resource::MainResource, FetchRequest::NoDefer));
}

TEST_F(FrameFetchContextTest, SetFirstPartyCookieAndRequestorOrigin) {
  struct TestCase {
    const char* documentURL;
    bool documentSandboxed;
    const char* requestorOrigin;  // "" => unique origin
    WebURLRequest::FrameType frameType;
    const char* serializedOrigin;  // "" => unique origin
  } cases[] = {
      // No document origin => unique request origin
      {"", false, "", WebURLRequest::FrameTypeNone, "null"},
      {"", true, "", WebURLRequest::FrameTypeNone, "null"},

      // Document origin => request origin
      {"http://example.test", false, "", WebURLRequest::FrameTypeNone,
       "http://example.test"},
      {"http://example.test", true, "", WebURLRequest::FrameTypeNone,
       "http://example.test"},

      // If the request already has a requestor origin, then
      // 'setFirstPartyCookieAndRequestorOrigin' leaves it alone:
      {"http://example.test", false, "http://not-example.test",
       WebURLRequest::FrameTypeNone, "http://not-example.test"},
      {"http://example.test", true, "http://not-example.test",
       WebURLRequest::FrameTypeNone, "http://not-example.test"},

      // If the request's frame type is not 'none', then
      // 'setFirstPartyCookieAndRequestorOrigin'
      // leaves it alone:
      {"http://example.test", false, "", WebURLRequest::FrameTypeTopLevel, ""},
      {"http://example.test", false, "", WebURLRequest::FrameTypeAuxiliary, ""},
      {"http://example.test", false, "", WebURLRequest::FrameTypeNested, ""},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(::testing::Message() << test.documentURL << " => "
                                      << test.serializedOrigin);
    // Set up a new document to ensure sandbox flags are cleared:
    dummyPageHolder = DummyPageHolder::create(IntSize(500, 500));
    dummyPageHolder->page().setDeviceScaleFactorDeprecated(1.0);
    document = &dummyPageHolder->document();
    FrameFetchContext::provideDocumentToContext(*fetchContext, document.get());

    // Setup the test:
    document->setURL(KURL(ParsedURLString, test.documentURL));
    document->setSecurityOrigin(SecurityOrigin::create(document->url()));

    if (test.documentSandboxed)
      document->enforceSandboxFlags(SandboxOrigin);

    ResourceRequest request("http://example.test/");
    request.setFrameType(test.frameType);
    if (strlen(test.requestorOrigin) > 0) {
      request.setRequestorOrigin(
          SecurityOrigin::create(KURL(ParsedURLString, test.requestorOrigin)));
    }

    // Compare the populated |requestorOrigin| against |test.serializedOrigin|
    fetchContext->setFirstPartyCookieAndRequestorOrigin(request);
    if (strlen(test.serializedOrigin) == 0) {
      EXPECT_TRUE(request.requestorOrigin()->isUnique());
    } else {
      EXPECT_EQ(String(test.serializedOrigin),
                request.requestorOrigin()->toString());
    }

    EXPECT_EQ(document->firstPartyForCookies(), request.firstPartyForCookies());
  }
}

TEST_F(FrameFetchContextTest, ModifyPriorityForLowPriorityIframes) {
  Settings* settings = document->frame()->settings();
  settings->setLowPriorityIframes(false);
  FrameFetchContext* childFetchContext = createChildFrame();

  // No low priority iframes, expect default values.
  EXPECT_EQ(ResourceLoadPriorityVeryHigh,
            childFetchContext->modifyPriorityForExperiments(
                ResourceLoadPriorityVeryHigh));
  EXPECT_EQ(ResourceLoadPriorityMedium,
            childFetchContext->modifyPriorityForExperiments(
                ResourceLoadPriorityMedium));

  // Low priority iframes enabled, everything should be low priority
  settings->setLowPriorityIframes(true);
  EXPECT_EQ(ResourceLoadPriorityVeryLow,
            childFetchContext->modifyPriorityForExperiments(
                ResourceLoadPriorityVeryHigh));
  EXPECT_EQ(ResourceLoadPriorityVeryLow,
            childFetchContext->modifyPriorityForExperiments(
                ResourceLoadPriorityMedium));
}

TEST_F(FrameFetchContextTest, EnableDataSaver) {
  Settings* settings = document->frame()->settings();
  settings->setDataSaverEnabled(true);
  ResourceRequest resourceRequest("http://www.example.com");
  fetchContext->addAdditionalRequestHeaders(resourceRequest, FetchMainResource);
  EXPECT_EQ("on", resourceRequest.httpHeaderField("Save-Data"));

  // Subsequent call to addAdditionalRequestHeaders should not append to the
  // save-data header.
  fetchContext->addAdditionalRequestHeaders(resourceRequest, FetchMainResource);
  EXPECT_EQ("on", resourceRequest.httpHeaderField("Save-Data"));
}

TEST_F(FrameFetchContextTest, DisabledDataSaver) {
  ResourceRequest resourceRequest("http://www.example.com");
  fetchContext->addAdditionalRequestHeaders(resourceRequest, FetchMainResource);
  EXPECT_EQ(String(), resourceRequest.httpHeaderField("Save-Data"));
}

// Tests that the embedder gets correct notification when a resource is loaded
// from the memory cache.
TEST_F(FrameFetchContextMockedLocalFrameClientTest,
       DispatchDidLoadResourceFromMemoryCache) {
  ResourceRequest resourceRequest(url);
  Resource* resource = MockResource::create(resourceRequest);
  EXPECT_CALL(
      *client,
      dispatchDidLoadResourceFromMemoryCache(
          testing::AllOf(testing::Property(&ResourceRequest::url, url),
                         testing::Property(&ResourceRequest::frameType,
                                           WebURLRequest::FrameTypeNone),
                         testing::Property(&ResourceRequest::requestContext,
                                           WebURLRequest::RequestContextImage)),
          ResourceResponse()));
  fetchContext->dispatchDidLoadResourceFromMemoryCache(
      createUniqueIdentifier(), resource, WebURLRequest::FrameTypeNone,
      WebURLRequest::RequestContextImage);
}

// Tests that when a resource with certificate errors is loaded from the memory
// cache, the embedder is notified.
TEST_F(FrameFetchContextMockedLocalFrameClientTest,
       MemoryCacheCertificateError) {
  ResourceRequest resourceRequest(url);
  ResourceResponse response;
  response.setURL(url);
  response.setHasMajorCertificateErrors(true);
  Resource* resource = MockResource::create(resourceRequest);
  resource->setResponse(response);
  EXPECT_CALL(*client, didDisplayContentWithCertificateErrors(url));
  fetchContext->dispatchDidLoadResourceFromMemoryCache(
      createUniqueIdentifier(), resource, WebURLRequest::FrameTypeNone,
      WebURLRequest::RequestContextImage);
}

TEST_F(FrameFetchContextTest, SetIsExternalRequestForPublicDocument) {
  EXPECT_EQ(WebAddressSpacePublic, document->addressSpace());

  struct TestCase {
    const char* url;
    bool isExternalExpectation;
  } cases[] = {
      {"data:text/html,whatever", false},  {"file:///etc/passwd", false},
      {"blob:http://example.com/", false},

      {"http://example.com/", false},      {"https://example.com/", false},

      {"http://192.168.1.1:8000/", true},  {"http://10.1.1.1:8000/", true},

      {"http://localhost/", true},         {"http://127.0.0.1/", true},
      {"http://127.0.0.1:8000/", true}};
  RuntimeEnabledFeatures::setCorsRFC1918Enabled(false);
  for (const auto& test : cases) {
    SCOPED_TRACE(test.url);
    ResourceRequest mainRequest(test.url);
    fetchContext->addAdditionalRequestHeaders(mainRequest, FetchMainResource);
    EXPECT_FALSE(mainRequest.isExternalRequest());

    ResourceRequest subRequest(test.url);
    fetchContext->addAdditionalRequestHeaders(subRequest, FetchSubresource);
    EXPECT_FALSE(subRequest.isExternalRequest());
  }

  RuntimeEnabledFeatures::setCorsRFC1918Enabled(true);
  for (const auto& test : cases) {
    SCOPED_TRACE(test.url);
    ResourceRequest mainRequest(test.url);
    fetchContext->addAdditionalRequestHeaders(mainRequest, FetchMainResource);
    EXPECT_EQ(test.isExternalExpectation, mainRequest.isExternalRequest());

    ResourceRequest subRequest(test.url);
    fetchContext->addAdditionalRequestHeaders(subRequest, FetchSubresource);
    EXPECT_EQ(test.isExternalExpectation, subRequest.isExternalRequest());
  }
}

TEST_F(FrameFetchContextTest, SetIsExternalRequestForPrivateDocument) {
  document->setAddressSpace(WebAddressSpacePrivate);
  EXPECT_EQ(WebAddressSpacePrivate, document->addressSpace());

  struct TestCase {
    const char* url;
    bool isExternalExpectation;
  } cases[] = {
      {"data:text/html,whatever", false},  {"file:///etc/passwd", false},
      {"blob:http://example.com/", false},

      {"http://example.com/", false},      {"https://example.com/", false},

      {"http://192.168.1.1:8000/", false}, {"http://10.1.1.1:8000/", false},

      {"http://localhost/", true},         {"http://127.0.0.1/", true},
      {"http://127.0.0.1:8000/", true}};
  RuntimeEnabledFeatures::setCorsRFC1918Enabled(false);
  for (const auto& test : cases) {
    SCOPED_TRACE(test.url);
    ResourceRequest mainRequest(test.url);
    fetchContext->addAdditionalRequestHeaders(mainRequest, FetchMainResource);
    EXPECT_FALSE(mainRequest.isExternalRequest());

    ResourceRequest subRequest(test.url);
    fetchContext->addAdditionalRequestHeaders(subRequest, FetchSubresource);
    EXPECT_FALSE(subRequest.isExternalRequest());
  }

  RuntimeEnabledFeatures::setCorsRFC1918Enabled(true);
  for (const auto& test : cases) {
    SCOPED_TRACE(test.url);
    ResourceRequest mainRequest(test.url);
    fetchContext->addAdditionalRequestHeaders(mainRequest, FetchMainResource);
    EXPECT_EQ(test.isExternalExpectation, mainRequest.isExternalRequest());

    ResourceRequest subRequest(test.url);
    fetchContext->addAdditionalRequestHeaders(subRequest, FetchSubresource);
    EXPECT_EQ(test.isExternalExpectation, subRequest.isExternalRequest());
  }
}

TEST_F(FrameFetchContextTest, SetIsExternalRequestForLocalDocument) {
  document->setAddressSpace(WebAddressSpaceLocal);
  EXPECT_EQ(WebAddressSpaceLocal, document->addressSpace());

  struct TestCase {
    const char* url;
    bool isExternalExpectation;
  } cases[] = {
      {"data:text/html,whatever", false},  {"file:///etc/passwd", false},
      {"blob:http://example.com/", false},

      {"http://example.com/", false},      {"https://example.com/", false},

      {"http://192.168.1.1:8000/", false}, {"http://10.1.1.1:8000/", false},

      {"http://localhost/", false},        {"http://127.0.0.1/", false},
      {"http://127.0.0.1:8000/", false}};

  RuntimeEnabledFeatures::setCorsRFC1918Enabled(false);
  for (const auto& test : cases) {
    ResourceRequest mainRequest(test.url);
    fetchContext->addAdditionalRequestHeaders(mainRequest, FetchMainResource);
    EXPECT_FALSE(mainRequest.isExternalRequest());

    ResourceRequest subRequest(test.url);
    fetchContext->addAdditionalRequestHeaders(subRequest, FetchSubresource);
    EXPECT_FALSE(subRequest.isExternalRequest());
  }

  RuntimeEnabledFeatures::setCorsRFC1918Enabled(true);
  for (const auto& test : cases) {
    ResourceRequest mainRequest(test.url);
    fetchContext->addAdditionalRequestHeaders(mainRequest, FetchMainResource);
    EXPECT_EQ(test.isExternalExpectation, mainRequest.isExternalRequest());

    ResourceRequest subRequest(test.url);
    fetchContext->addAdditionalRequestHeaders(subRequest, FetchSubresource);
    EXPECT_EQ(test.isExternalExpectation, subRequest.isExternalRequest());
  }
}

TEST_F(FrameFetchContextSubresourceFilterTest, Filter) {
  setFilterPolicy(WebDocumentSubresourceFilter::Disallow);

  EXPECT_EQ(ResourceRequestBlockedReason::SubresourceFilter, canRequest());
  EXPECT_EQ(1, getFilteredLoadCallCount());

  EXPECT_EQ(ResourceRequestBlockedReason::SubresourceFilter, canRequest());
  EXPECT_EQ(2, getFilteredLoadCallCount());

  EXPECT_EQ(ResourceRequestBlockedReason::SubresourceFilter,
            canRequestPreload());
  EXPECT_EQ(2, getFilteredLoadCallCount());

  EXPECT_EQ(ResourceRequestBlockedReason::SubresourceFilter, canRequest());
  EXPECT_EQ(3, getFilteredLoadCallCount());
}

TEST_F(FrameFetchContextSubresourceFilterTest, Allow) {
  setFilterPolicy(WebDocumentSubresourceFilter::Allow);

  EXPECT_EQ(ResourceRequestBlockedReason::None, canRequest());
  EXPECT_EQ(0, getFilteredLoadCallCount());

  EXPECT_EQ(ResourceRequestBlockedReason::None, canRequestPreload());
  EXPECT_EQ(0, getFilteredLoadCallCount());
}

TEST_F(FrameFetchContextSubresourceFilterTest, WouldDisallow) {
  setFilterPolicy(WebDocumentSubresourceFilter::WouldDisallow);

  EXPECT_EQ(ResourceRequestBlockedReason::None, canRequest());
  EXPECT_EQ(0, getFilteredLoadCallCount());

  EXPECT_EQ(ResourceRequestBlockedReason::None, canRequestPreload());
  EXPECT_EQ(0, getFilteredLoadCallCount());
}

}  // namespace blink
