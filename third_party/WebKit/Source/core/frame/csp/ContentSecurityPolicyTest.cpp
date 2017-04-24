// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/csp/ContentSecurityPolicy.h"

#include "core/dom/Document.h"
#include "core/frame/csp/CSPDirectiveList.h"
#include "core/html/HTMLScriptElement.h"
#include "core/loader/DocumentLoader.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/Crypto.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/loader/fetch/IntegrityMetadata.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebAddressSpace.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ContentSecurityPolicyTest : public ::testing::Test {
 public:
  ContentSecurityPolicyTest()
      : csp(ContentSecurityPolicy::create()),
        secureURL(ParsedURLString, "https://example.test/image.png"),
        secureOrigin(SecurityOrigin::create(secureURL)) {}

 protected:
  virtual void SetUp() {
    document = Document::create();
    document->setSecurityOrigin(secureOrigin);
  }

  Persistent<ContentSecurityPolicy> csp;
  KURL secureURL;
  RefPtr<SecurityOrigin> secureOrigin;
  Persistent<Document> document;
};

TEST_F(ContentSecurityPolicyTest, ParseInsecureRequestPolicy) {
  struct TestCase {
    const char* header;
    WebInsecureRequestPolicy expectedPolicy;
  } cases[] = {{"default-src 'none'", kLeaveInsecureRequestsAlone},
               {"upgrade-insecure-requests", kUpgradeInsecureRequests},
               {"block-all-mixed-content", kBlockAllMixedContent},
               {"upgrade-insecure-requests; block-all-mixed-content",
                kUpgradeInsecureRequests | kBlockAllMixedContent},
               {"upgrade-insecure-requests, block-all-mixed-content",
                kUpgradeInsecureRequests | kBlockAllMixedContent}};

  // Enforced
  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << "[Enforce] Header: `" << test.header
                                    << "`");
    csp = ContentSecurityPolicy::create();
    csp->didReceiveHeader(test.header, ContentSecurityPolicyHeaderTypeEnforce,
                          ContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(test.expectedPolicy, csp->getInsecureRequestPolicy());

    document = Document::create();
    document->setSecurityOrigin(secureOrigin);
    document->setURL(secureURL);
    csp->bindToExecutionContext(document.get());
    EXPECT_EQ(test.expectedPolicy, document->getInsecureRequestPolicy());
    bool expectUpgrade = test.expectedPolicy & kUpgradeInsecureRequests;
    EXPECT_EQ(expectUpgrade, document->insecureNavigationsToUpgrade()->contains(
                                 document->url().host().impl()->hash()));
  }

  // Report-Only
  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << "[Report-Only] Header: `" << test.header
                                    << "`");
    csp = ContentSecurityPolicy::create();
    csp->didReceiveHeader(test.header, ContentSecurityPolicyHeaderTypeReport,
                          ContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(kLeaveInsecureRequestsAlone, csp->getInsecureRequestPolicy());

    document = Document::create();
    document->setSecurityOrigin(secureOrigin);
    csp->bindToExecutionContext(document.get());
    EXPECT_EQ(kLeaveInsecureRequestsAlone,
              document->getInsecureRequestPolicy());
    EXPECT_FALSE(document->insecureNavigationsToUpgrade()->contains(
        secureOrigin->host().impl()->hash()));
  }
}

TEST_F(ContentSecurityPolicyTest, ParseEnforceTreatAsPublicAddressDisabled) {
  RuntimeEnabledFeatures::setCorsRFC1918Enabled(false);
  document->setAddressSpace(WebAddressSpacePrivate);
  EXPECT_EQ(WebAddressSpacePrivate, document->addressSpace());

  csp->didReceiveHeader("treat-as-public-address",
                        ContentSecurityPolicyHeaderTypeEnforce,
                        ContentSecurityPolicyHeaderSourceHTTP);
  csp->bindToExecutionContext(document.get());
  EXPECT_EQ(WebAddressSpacePrivate, document->addressSpace());
}

TEST_F(ContentSecurityPolicyTest, ParseEnforceTreatAsPublicAddressEnabled) {
  RuntimeEnabledFeatures::setCorsRFC1918Enabled(true);
  document->setAddressSpace(WebAddressSpacePrivate);
  EXPECT_EQ(WebAddressSpacePrivate, document->addressSpace());

  csp->didReceiveHeader("treat-as-public-address",
                        ContentSecurityPolicyHeaderTypeEnforce,
                        ContentSecurityPolicyHeaderSourceHTTP);
  csp->bindToExecutionContext(document.get());
  EXPECT_EQ(WebAddressSpacePublic, document->addressSpace());
}

TEST_F(ContentSecurityPolicyTest, CopyStateFrom) {
  csp->didReceiveHeader("script-src 'none'; plugin-types application/x-type-1",
                        ContentSecurityPolicyHeaderTypeReport,
                        ContentSecurityPolicyHeaderSourceHTTP);
  csp->didReceiveHeader("img-src http://example.com",
                        ContentSecurityPolicyHeaderTypeReport,
                        ContentSecurityPolicyHeaderSourceHTTP);

  KURL exampleUrl(KURL(), "http://example.com");
  KURL notExampleUrl(KURL(), "http://not-example.com");

  ContentSecurityPolicy* csp2 = ContentSecurityPolicy::create();
  csp2->copyStateFrom(csp.get());
  EXPECT_FALSE(csp2->allowScriptFromSource(
      exampleUrl, String(), ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(csp2->allowPluginType(
      "application/x-type-1", "application/x-type-1", exampleUrl,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(csp2->allowImageFromSource(
      exampleUrl, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_FALSE(csp2->allowImageFromSource(
      notExampleUrl, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_FALSE(csp2->allowPluginType(
      "application/x-type-2", "application/x-type-2", exampleUrl,
      SecurityViolationReportingPolicy::SuppressReporting));
}

TEST_F(ContentSecurityPolicyTest, CopyPluginTypesFrom) {
  csp->didReceiveHeader("script-src 'none'; plugin-types application/x-type-1",
                        ContentSecurityPolicyHeaderTypeEnforce,
                        ContentSecurityPolicyHeaderSourceHTTP);
  csp->didReceiveHeader("img-src http://example.com",
                        ContentSecurityPolicyHeaderTypeEnforce,
                        ContentSecurityPolicyHeaderSourceHTTP);

  KURL exampleUrl(KURL(), "http://example.com");
  KURL notExampleUrl(KURL(), "http://not-example.com");

  ContentSecurityPolicy* csp2 = ContentSecurityPolicy::create();
  csp2->copyPluginTypesFrom(csp.get());
  EXPECT_TRUE(csp2->allowScriptFromSource(
      exampleUrl, String(), ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(csp2->allowPluginType(
      "application/x-type-1", "application/x-type-1", exampleUrl,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(csp2->allowImageFromSource(
      exampleUrl, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(csp2->allowImageFromSource(
      notExampleUrl, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_FALSE(csp2->allowPluginType(
      "application/x-type-2", "application/x-type-2", exampleUrl,
      SecurityViolationReportingPolicy::SuppressReporting));
}

TEST_F(ContentSecurityPolicyTest, IsFrameAncestorsEnforced) {
  csp->didReceiveHeader("script-src 'none';",
                        ContentSecurityPolicyHeaderTypeEnforce,
                        ContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_FALSE(csp->isFrameAncestorsEnforced());

  csp->didReceiveHeader("frame-ancestors 'self'",
                        ContentSecurityPolicyHeaderTypeReport,
                        ContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_FALSE(csp->isFrameAncestorsEnforced());

  csp->didReceiveHeader("frame-ancestors 'self'",
                        ContentSecurityPolicyHeaderTypeEnforce,
                        ContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(csp->isFrameAncestorsEnforced());
}

// Tests that frame-ancestors directives are discarded from policies
// delivered in <meta> elements.
TEST_F(ContentSecurityPolicyTest, FrameAncestorsInMeta) {
  csp->bindToExecutionContext(document.get());
  csp->didReceiveHeader("frame-ancestors 'none';",
                        ContentSecurityPolicyHeaderTypeEnforce,
                        ContentSecurityPolicyHeaderSourceMeta);
  EXPECT_FALSE(csp->isFrameAncestorsEnforced());
  csp->didReceiveHeader("frame-ancestors 'none';",
                        ContentSecurityPolicyHeaderTypeEnforce,
                        ContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(csp->isFrameAncestorsEnforced());
}

// Tests that sandbox directives are discarded from policies
// delivered in <meta> elements.
TEST_F(ContentSecurityPolicyTest, SandboxInMeta) {
  csp->bindToExecutionContext(document.get());
  csp->didReceiveHeader("sandbox;", ContentSecurityPolicyHeaderTypeEnforce,
                        ContentSecurityPolicyHeaderSourceMeta);
  EXPECT_FALSE(document->getSecurityOrigin()->isUnique());
  csp->didReceiveHeader("sandbox;", ContentSecurityPolicyHeaderTypeEnforce,
                        ContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(document->getSecurityOrigin()->isUnique());
}

// Tests that report-uri directives are discarded from policies
// delivered in <meta> elements.
TEST_F(ContentSecurityPolicyTest, ReportURIInMeta) {
  String policy = "img-src 'none'; report-uri http://foo.test";
  Vector<UChar> characters;
  policy.appendTo(characters);
  const UChar* begin = characters.data();
  const UChar* end = begin + characters.size();
  CSPDirectiveList* directiveList(CSPDirectiveList::create(
      csp, begin, end, ContentSecurityPolicyHeaderTypeEnforce,
      ContentSecurityPolicyHeaderSourceMeta));
  EXPECT_TRUE(directiveList->reportEndpoints().isEmpty());
  directiveList = CSPDirectiveList::create(
      csp, begin, end, ContentSecurityPolicyHeaderTypeEnforce,
      ContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_FALSE(directiveList->reportEndpoints().isEmpty());
}

// Tests that object-src directives are applied to a request to load a
// plugin, but not to subresource requests that the plugin itself
// makes. https://crbug.com/603952
TEST_F(ContentSecurityPolicyTest, ObjectSrc) {
  KURL url(KURL(), "https://example.test");
  csp->bindToExecutionContext(document.get());
  csp->didReceiveHeader("object-src 'none';",
                        ContentSecurityPolicyHeaderTypeEnforce,
                        ContentSecurityPolicyHeaderSourceMeta);
  EXPECT_FALSE(
      csp->allowRequest(WebURLRequest::RequestContextObject, url, String(),
                        IntegrityMetadataSet(), ParserInserted,
                        ResourceRequest::RedirectStatus::NoRedirect,
                        SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_FALSE(csp->allowRequest(
      WebURLRequest::RequestContextEmbed, url, String(), IntegrityMetadataSet(),
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(
      csp->allowRequest(WebURLRequest::RequestContextPlugin, url, String(),
                        IntegrityMetadataSet(), ParserInserted,
                        ResourceRequest::RedirectStatus::NoRedirect,
                        SecurityViolationReportingPolicy::SuppressReporting));
}

TEST_F(ContentSecurityPolicyTest, ConnectSrc) {
  KURL url(KURL(), "https://example.test");
  csp->bindToExecutionContext(document.get());
  csp->didReceiveHeader("connect-src 'none';",
                        ContentSecurityPolicyHeaderTypeEnforce,
                        ContentSecurityPolicyHeaderSourceMeta);
  EXPECT_FALSE(
      csp->allowRequest(WebURLRequest::RequestContextSubresource, url, String(),
                        IntegrityMetadataSet(), ParserInserted,
                        ResourceRequest::RedirectStatus::NoRedirect,
                        SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_FALSE(
      csp->allowRequest(WebURLRequest::RequestContextXMLHttpRequest, url,
                        String(), IntegrityMetadataSet(), ParserInserted,
                        ResourceRequest::RedirectStatus::NoRedirect,
                        SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_FALSE(
      csp->allowRequest(WebURLRequest::RequestContextBeacon, url, String(),
                        IntegrityMetadataSet(), ParserInserted,
                        ResourceRequest::RedirectStatus::NoRedirect,
                        SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_FALSE(csp->allowRequest(
      WebURLRequest::RequestContextFetch, url, String(), IntegrityMetadataSet(),
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(
      csp->allowRequest(WebURLRequest::RequestContextPlugin, url, String(),
                        IntegrityMetadataSet(), ParserInserted,
                        ResourceRequest::RedirectStatus::NoRedirect,
                        SecurityViolationReportingPolicy::SuppressReporting));
}
// Tests that requests for scripts and styles are blocked
// if `require-sri-for` delivered in HTTP header requires integrity be present
TEST_F(ContentSecurityPolicyTest, RequireSRIForInHeaderMissingIntegrity) {
  KURL url(KURL(), "https://example.test");
  // Enforce
  Persistent<ContentSecurityPolicy> policy = ContentSecurityPolicy::create();
  policy->bindToExecutionContext(document.get());
  policy->didReceiveHeader("require-sri-for script style",
                           ContentSecurityPolicyHeaderTypeEnforce,
                           ContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_FALSE(policy->allowRequest(
      WebURLRequest::RequestContextScript, url, String(),
      IntegrityMetadataSet(), ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_FALSE(policy->allowRequest(
      WebURLRequest::RequestContextImport, url, String(),
      IntegrityMetadataSet(), ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_FALSE(policy->allowRequest(
      WebURLRequest::RequestContextStyle, url, String(), IntegrityMetadataSet(),
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_FALSE(policy->allowRequest(
      WebURLRequest::RequestContextServiceWorker, url, String(),
      IntegrityMetadataSet(), ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_FALSE(policy->allowRequest(
      WebURLRequest::RequestContextSharedWorker, url, String(),
      IntegrityMetadataSet(), ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_FALSE(policy->allowRequest(
      WebURLRequest::RequestContextWorker, url, String(),
      IntegrityMetadataSet(), ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextImage, url, String(), IntegrityMetadataSet(),
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  // Report
  policy = ContentSecurityPolicy::create();
  policy->bindToExecutionContext(document.get());
  policy->didReceiveHeader("require-sri-for script style",
                           ContentSecurityPolicyHeaderTypeReport,
                           ContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextScript, url, String(),
      IntegrityMetadataSet(), ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextImport, url, String(),
      IntegrityMetadataSet(), ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextStyle, url, String(), IntegrityMetadataSet(),
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextServiceWorker, url, String(),
      IntegrityMetadataSet(), ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextSharedWorker, url, String(),
      IntegrityMetadataSet(), ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextWorker, url, String(),
      IntegrityMetadataSet(), ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextImage, url, String(), IntegrityMetadataSet(),
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
}

// Tests that requests for scripts and styles are allowed
// if `require-sri-for` delivered in HTTP header requires integrity be present
TEST_F(ContentSecurityPolicyTest, RequireSRIForInHeaderPresentIntegrity) {
  KURL url(KURL(), "https://example.test");
  IntegrityMetadataSet integrityMetadata;
  integrityMetadata.insert(
      IntegrityMetadata("1234", HashAlgorithmSha384).toPair());
  csp->bindToExecutionContext(document.get());
  // Enforce
  Persistent<ContentSecurityPolicy> policy = ContentSecurityPolicy::create();
  policy->bindToExecutionContext(document.get());
  policy->didReceiveHeader("require-sri-for script style",
                           ContentSecurityPolicyHeaderTypeEnforce,
                           ContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextScript, url, String(), integrityMetadata,
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextImport, url, String(), integrityMetadata,
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextStyle, url, String(), integrityMetadata,
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextServiceWorker, url, String(),
      integrityMetadata, ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextSharedWorker, url, String(),
      integrityMetadata, ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextWorker, url, String(), integrityMetadata,
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextImage, url, String(), integrityMetadata,
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  // Content-Security-Policy-Report-Only is not supported in meta element,
  // so nothing should be blocked
  policy = ContentSecurityPolicy::create();
  policy->bindToExecutionContext(document.get());
  policy->didReceiveHeader("require-sri-for script style",
                           ContentSecurityPolicyHeaderTypeReport,
                           ContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextScript, url, String(), integrityMetadata,
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextImport, url, String(), integrityMetadata,
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextStyle, url, String(), integrityMetadata,
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextServiceWorker, url, String(),
      integrityMetadata, ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextSharedWorker, url, String(),
      integrityMetadata, ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextWorker, url, String(), integrityMetadata,
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextImage, url, String(), integrityMetadata,
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
}

// Tests that requests for scripts and styles are blocked
// if `require-sri-for` delivered in meta tag requires integrity be present
TEST_F(ContentSecurityPolicyTest, RequireSRIForInMetaMissingIntegrity) {
  KURL url(KURL(), "https://example.test");
  // Enforce
  Persistent<ContentSecurityPolicy> policy = ContentSecurityPolicy::create();
  policy->bindToExecutionContext(document.get());
  policy->didReceiveHeader("require-sri-for script style",
                           ContentSecurityPolicyHeaderTypeEnforce,
                           ContentSecurityPolicyHeaderSourceMeta);
  EXPECT_FALSE(policy->allowRequest(
      WebURLRequest::RequestContextScript, url, String(),
      IntegrityMetadataSet(), ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_FALSE(policy->allowRequest(
      WebURLRequest::RequestContextImport, url, String(),
      IntegrityMetadataSet(), ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_FALSE(policy->allowRequest(
      WebURLRequest::RequestContextStyle, url, String(), IntegrityMetadataSet(),
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_FALSE(policy->allowRequest(
      WebURLRequest::RequestContextServiceWorker, url, String(),
      IntegrityMetadataSet(), ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_FALSE(policy->allowRequest(
      WebURLRequest::RequestContextSharedWorker, url, String(),
      IntegrityMetadataSet(), ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_FALSE(policy->allowRequest(
      WebURLRequest::RequestContextWorker, url, String(),
      IntegrityMetadataSet(), ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextImage, url, String(), IntegrityMetadataSet(),
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  // Content-Security-Policy-Report-Only is not supported in meta element,
  // so nothing should be blocked
  policy = ContentSecurityPolicy::create();
  policy->bindToExecutionContext(document.get());
  policy->didReceiveHeader("require-sri-for script style",
                           ContentSecurityPolicyHeaderTypeReport,
                           ContentSecurityPolicyHeaderSourceMeta);
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextScript, url, String(),
      IntegrityMetadataSet(), ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextImport, url, String(),
      IntegrityMetadataSet(), ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextStyle, url, String(), IntegrityMetadataSet(),
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextServiceWorker, url, String(),
      IntegrityMetadataSet(), ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextSharedWorker, url, String(),
      IntegrityMetadataSet(), ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextWorker, url, String(),
      IntegrityMetadataSet(), ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextImage, url, String(), IntegrityMetadataSet(),
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
}

// Tests that requests for scripts and styles are allowed
// if `require-sri-for` delivered meta tag requires integrity be present
TEST_F(ContentSecurityPolicyTest, RequireSRIForInMetaPresentIntegrity) {
  KURL url(KURL(), "https://example.test");
  IntegrityMetadataSet integrityMetadata;
  integrityMetadata.insert(
      IntegrityMetadata("1234", HashAlgorithmSha384).toPair());
  csp->bindToExecutionContext(document.get());
  // Enforce
  Persistent<ContentSecurityPolicy> policy = ContentSecurityPolicy::create();
  policy->bindToExecutionContext(document.get());
  policy->didReceiveHeader("require-sri-for script style",
                           ContentSecurityPolicyHeaderTypeEnforce,
                           ContentSecurityPolicyHeaderSourceMeta);
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextScript, url, String(), integrityMetadata,
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextImport, url, String(), integrityMetadata,
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextStyle, url, String(), integrityMetadata,
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextServiceWorker, url, String(),
      integrityMetadata, ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextSharedWorker, url, String(),
      integrityMetadata, ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextWorker, url, String(), integrityMetadata,
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextImage, url, String(), integrityMetadata,
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  // Content-Security-Policy-Report-Only is not supported in meta element,
  // so nothing should be blocked
  policy = ContentSecurityPolicy::create();
  policy->bindToExecutionContext(document.get());
  policy->didReceiveHeader("require-sri-for script style",
                           ContentSecurityPolicyHeaderTypeReport,
                           ContentSecurityPolicyHeaderSourceMeta);
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextScript, url, String(), integrityMetadata,
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextImport, url, String(), integrityMetadata,
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextStyle, url, String(), integrityMetadata,
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextServiceWorker, url, String(),
      integrityMetadata, ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextSharedWorker, url, String(),
      integrityMetadata, ParserInserted,
      ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextWorker, url, String(), integrityMetadata,
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
  EXPECT_TRUE(policy->allowRequest(
      WebURLRequest::RequestContextImage, url, String(), integrityMetadata,
      ParserInserted, ResourceRequest::RedirectStatus::NoRedirect,
      SecurityViolationReportingPolicy::SuppressReporting));
}

TEST_F(ContentSecurityPolicyTest, NonceSinglePolicy) {
  struct TestCase {
    const char* policy;
    const char* url;
    const char* nonce;
    bool allowed;
  } cases[] = {
      {"script-src 'nonce-yay'", "https://example.com/js", "", false},
      {"script-src 'nonce-yay'", "https://example.com/js", "yay", true},
      {"script-src https://example.com", "https://example.com/js", "", true},
      {"script-src https://example.com", "https://example.com/js", "yay", true},
      {"script-src https://example.com 'nonce-yay'",
       "https://not.example.com/js", "", false},
      {"script-src https://example.com 'nonce-yay'",
       "https://not.example.com/js", "yay", true},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << "Policy: `" << test.policy << "`, URL: `"
                                    << test.url << "`, Nonce: `" << test.nonce
                                    << "`");
    KURL resource = KURL(KURL(), test.url);

    unsigned expectedReports = test.allowed ? 0u : 1u;

    // Single enforce-mode policy should match `test.expected`:
    Persistent<ContentSecurityPolicy> policy = ContentSecurityPolicy::create();
    policy->bindToExecutionContext(document.get());
    policy->didReceiveHeader(test.policy,
                             ContentSecurityPolicyHeaderTypeEnforce,
                             ContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(test.allowed, policy->allowScriptFromSource(
                                resource, String(test.nonce), ParserInserted));
    // If this is expected to generate a violation, we should have sent a
    // report.
    EXPECT_EQ(expectedReports, policy->m_violationReportsSent.size());

    // Single report-mode policy should always be `true`:
    policy = ContentSecurityPolicy::create();
    policy->bindToExecutionContext(document.get());
    policy->didReceiveHeader(test.policy, ContentSecurityPolicyHeaderTypeReport,
                             ContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_TRUE(policy->allowScriptFromSource(resource, String(test.nonce),
                                              ParserInserted));
    // If this is expected to generate a violation, we should have sent a
    // report, even though we don't deny access in `allowScriptFromSource`:
    EXPECT_EQ(expectedReports, policy->m_violationReportsSent.size());
  }
}

TEST_F(ContentSecurityPolicyTest, NonceInline) {
  struct TestCase {
    const char* policy;
    const char* nonce;
    bool allowed;
  } cases[] = {
      {"'unsafe-inline'", "", true},
      {"'unsafe-inline'", "yay", true},
      {"'nonce-yay'", "", false},
      {"'nonce-yay'", "yay", true},
      {"'unsafe-inline' 'nonce-yay'", "", false},
      {"'unsafe-inline' 'nonce-yay'", "yay", true},
  };

  String contextURL;
  String content;
  WTF::OrdinalNumber contextLine;
  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << "Policy: `" << test.policy
                                    << "`, Nonce: `" << test.nonce << "`");

    unsigned expectedReports = test.allowed ? 0u : 1u;
    HTMLScriptElement* element = HTMLScriptElement::create(*document, true);

    // Enforce 'script-src'
    Persistent<ContentSecurityPolicy> policy = ContentSecurityPolicy::create();
    policy->bindToExecutionContext(document.get());
    policy->didReceiveHeader(String("script-src ") + test.policy,
                             ContentSecurityPolicyHeaderTypeEnforce,
                             ContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(test.allowed,
              policy->allowInlineScript(element, contextURL, String(test.nonce),
                                        contextLine, content));
    EXPECT_EQ(expectedReports, policy->m_violationReportsSent.size());

    // Enforce 'style-src'
    policy = ContentSecurityPolicy::create();
    policy->bindToExecutionContext(document.get());
    policy->didReceiveHeader(String("style-src ") + test.policy,
                             ContentSecurityPolicyHeaderTypeEnforce,
                             ContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(test.allowed,
              policy->allowInlineStyle(element, contextURL, String(test.nonce),
                                       contextLine, content));
    EXPECT_EQ(expectedReports, policy->m_violationReportsSent.size());

    // Report 'script-src'
    policy = ContentSecurityPolicy::create();
    policy->bindToExecutionContext(document.get());
    policy->didReceiveHeader(String("script-src ") + test.policy,
                             ContentSecurityPolicyHeaderTypeReport,
                             ContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_TRUE(policy->allowInlineScript(
        element, contextURL, String(test.nonce), contextLine, content));
    EXPECT_EQ(expectedReports, policy->m_violationReportsSent.size());

    // Report 'style-src'
    policy = ContentSecurityPolicy::create();
    policy->bindToExecutionContext(document.get());
    policy->didReceiveHeader(String("style-src ") + test.policy,
                             ContentSecurityPolicyHeaderTypeReport,
                             ContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_TRUE(policy->allowInlineStyle(
        element, contextURL, String(test.nonce), contextLine, content));
    EXPECT_EQ(expectedReports, policy->m_violationReportsSent.size());
  }
}

TEST_F(ContentSecurityPolicyTest, NonceMultiplePolicy) {
  struct TestCase {
    const char* policy1;
    const char* policy2;
    const char* url;
    const char* nonce;
    bool allowed1;
    bool allowed2;
  } cases[] = {
      // Passes both:
      {"script-src 'nonce-yay'", "script-src 'nonce-yay'",
       "https://example.com/js", "yay", true, true},
      {"script-src https://example.com", "script-src 'nonce-yay'",
       "https://example.com/js", "yay", true, true},
      {"script-src 'nonce-yay'", "script-src https://example.com",
       "https://example.com/js", "yay", true, true},
      {"script-src https://example.com 'nonce-yay'",
       "script-src https://example.com 'nonce-yay'", "https://example.com/js",
       "yay", true, true},
      {"script-src https://example.com 'nonce-yay'",
       "script-src https://example.com 'nonce-yay'", "https://example.com/js",
       "", true, true},
      {"script-src https://example.com",
       "script-src https://example.com 'nonce-yay'", "https://example.com/js",
       "yay", true, true},
      {"script-src https://example.com 'nonce-yay'",
       "script-src https://example.com", "https://example.com/js", "yay", true,
       true},

      // Fails one:
      {"script-src 'nonce-yay'", "script-src https://example.com",
       "https://example.com/js", "", false, true},
      {"script-src 'nonce-yay'", "script-src 'none'", "https://example.com/js",
       "yay", true, false},
      {"script-src 'nonce-yay'", "script-src https://not.example.com",
       "https://example.com/js", "yay", true, false},

      // Fails both:
      {"script-src 'nonce-yay'", "script-src https://example.com",
       "https://not.example.com/js", "", false, false},
      {"script-src https://example.com", "script-src 'nonce-yay'",
       "https://not.example.com/js", "", false, false},
      {"script-src 'nonce-yay'", "script-src 'none'",
       "https://not.example.com/js", "boo", false, false},
      {"script-src 'nonce-yay'", "script-src https://not.example.com",
       "https://example.com/js", "", false, false},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << "Policy: `" << test.policy1 << "`/`"
                                    << test.policy2 << "`, URL: `" << test.url
                                    << "`, Nonce: `" << test.nonce << "`");
    KURL resource = KURL(KURL(), test.url);

    unsigned expectedReports =
        test.allowed1 != test.allowed2 ? 1u : (test.allowed1 ? 0u : 2u);

    // Enforce / Report
    Persistent<ContentSecurityPolicy> policy = ContentSecurityPolicy::create();
    policy->bindToExecutionContext(document.get());
    policy->didReceiveHeader(test.policy1,
                             ContentSecurityPolicyHeaderTypeEnforce,
                             ContentSecurityPolicyHeaderSourceHTTP);
    policy->didReceiveHeader(test.policy2,
                             ContentSecurityPolicyHeaderTypeReport,
                             ContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(test.allowed1, policy->allowScriptFromSource(
                                 resource, String(test.nonce), ParserInserted));
    EXPECT_EQ(expectedReports, policy->m_violationReportsSent.size());

    // Report / Enforce
    policy = ContentSecurityPolicy::create();
    policy->bindToExecutionContext(document.get());
    policy->didReceiveHeader(test.policy1,
                             ContentSecurityPolicyHeaderTypeReport,
                             ContentSecurityPolicyHeaderSourceHTTP);
    policy->didReceiveHeader(test.policy2,
                             ContentSecurityPolicyHeaderTypeEnforce,
                             ContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(test.allowed2, policy->allowScriptFromSource(
                                 resource, String(test.nonce), ParserInserted));
    EXPECT_EQ(expectedReports, policy->m_violationReportsSent.size());

    // Enforce / Enforce
    policy = ContentSecurityPolicy::create();
    policy->bindToExecutionContext(document.get());
    policy->didReceiveHeader(test.policy1,
                             ContentSecurityPolicyHeaderTypeEnforce,
                             ContentSecurityPolicyHeaderSourceHTTP);
    policy->didReceiveHeader(test.policy2,
                             ContentSecurityPolicyHeaderTypeEnforce,
                             ContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(test.allowed1 && test.allowed2,
              policy->allowScriptFromSource(resource, String(test.nonce),
                                            ParserInserted));
    EXPECT_EQ(expectedReports, policy->m_violationReportsSent.size());

    // Report / Report
    policy = ContentSecurityPolicy::create();
    policy->bindToExecutionContext(document.get());
    policy->didReceiveHeader(test.policy1,
                             ContentSecurityPolicyHeaderTypeReport,
                             ContentSecurityPolicyHeaderSourceHTTP);
    policy->didReceiveHeader(test.policy2,
                             ContentSecurityPolicyHeaderTypeReport,
                             ContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_TRUE(policy->allowScriptFromSource(resource, String(test.nonce),
                                              ParserInserted));
    EXPECT_EQ(expectedReports, policy->m_violationReportsSent.size());
  }
}

TEST_F(ContentSecurityPolicyTest, ShouldEnforceEmbeddersPolicy) {
  struct TestCase {
    const char* resourceURL;
    const bool inherits;
  } cases[] = {
      // Same-origin
      {"https://example.test/index.html", true},
      // Cross-origin
      {"http://example.test/index.html", false},
      {"http://example.test:8443/index.html", false},
      {"https://example.test:8443/index.html", false},
      {"http://not.example.test/index.html", false},
      {"https://not.example.test/index.html", false},
      {"https://not.example.test:8443/index.html", false},

      // Inherit
      {"about:blank", true},
      {"data:text/html,yay", true},
      {"blob:https://example.test/bbe708f3-defd-4852-93b6-cf94e032f08d", true},
      {"filesystem:http://example.test/temporary/index.html", true},
  };

  for (const auto& test : cases) {
    ResourceResponse response;
    response.setURL(KURL(ParsedURLString, test.resourceURL));
    EXPECT_EQ(ContentSecurityPolicy::shouldEnforceEmbeddersPolicy(
                  response, secureOrigin.get()),
              test.inherits);

    response.setHTTPHeaderField(HTTPNames::Allow_CSP_From, AtomicString("*"));
    EXPECT_TRUE(ContentSecurityPolicy::shouldEnforceEmbeddersPolicy(
        response, secureOrigin.get()));

    response.setHTTPHeaderField(HTTPNames::Allow_CSP_From,
                                AtomicString("* not a valid header"));
    EXPECT_EQ(ContentSecurityPolicy::shouldEnforceEmbeddersPolicy(
                  response, secureOrigin.get()),
              test.inherits);

    response.setHTTPHeaderField(HTTPNames::Allow_CSP_From,
                                AtomicString("http://example.test"));
    EXPECT_EQ(ContentSecurityPolicy::shouldEnforceEmbeddersPolicy(
                  response, secureOrigin.get()),
              test.inherits);

    response.setHTTPHeaderField(HTTPNames::Allow_CSP_From,
                                AtomicString("https://example.test"));
    EXPECT_TRUE(ContentSecurityPolicy::shouldEnforceEmbeddersPolicy(
        response, secureOrigin.get()));
  }
}

TEST_F(ContentSecurityPolicyTest, DirectiveType) {
  struct TestCase {
    ContentSecurityPolicy::DirectiveType type;
    const String& name;
  } cases[] = {
      {ContentSecurityPolicy::DirectiveType::BaseURI, "base-uri"},
      {ContentSecurityPolicy::DirectiveType::BlockAllMixedContent,
       "block-all-mixed-content"},
      {ContentSecurityPolicy::DirectiveType::ChildSrc, "child-src"},
      {ContentSecurityPolicy::DirectiveType::ConnectSrc, "connect-src"},
      {ContentSecurityPolicy::DirectiveType::DefaultSrc, "default-src"},
      {ContentSecurityPolicy::DirectiveType::FrameAncestors, "frame-ancestors"},
      {ContentSecurityPolicy::DirectiveType::FrameSrc, "frame-src"},
      {ContentSecurityPolicy::DirectiveType::FontSrc, "font-src"},
      {ContentSecurityPolicy::DirectiveType::FormAction, "form-action"},
      {ContentSecurityPolicy::DirectiveType::ImgSrc, "img-src"},
      {ContentSecurityPolicy::DirectiveType::ManifestSrc, "manifest-src"},
      {ContentSecurityPolicy::DirectiveType::MediaSrc, "media-src"},
      {ContentSecurityPolicy::DirectiveType::ObjectSrc, "object-src"},
      {ContentSecurityPolicy::DirectiveType::PluginTypes, "plugin-types"},
      {ContentSecurityPolicy::DirectiveType::ReportURI, "report-uri"},
      {ContentSecurityPolicy::DirectiveType::RequireSRIFor, "require-sri-for"},
      {ContentSecurityPolicy::DirectiveType::Sandbox, "sandbox"},
      {ContentSecurityPolicy::DirectiveType::ScriptSrc, "script-src"},
      {ContentSecurityPolicy::DirectiveType::StyleSrc, "style-src"},
      {ContentSecurityPolicy::DirectiveType::TreatAsPublicAddress,
       "treat-as-public-address"},
      {ContentSecurityPolicy::DirectiveType::UpgradeInsecureRequests,
       "upgrade-insecure-requests"},
      {ContentSecurityPolicy::DirectiveType::WorkerSrc, "worker-src"},
  };

  EXPECT_EQ(ContentSecurityPolicy::DirectiveType::Undefined,
            ContentSecurityPolicy::getDirectiveType("random"));

  for (const auto& test : cases) {
    const String& nameFromType =
        ContentSecurityPolicy::getDirectiveName(test.type);
    ContentSecurityPolicy::DirectiveType typeFromName =
        ContentSecurityPolicy::getDirectiveType(test.name);
    EXPECT_EQ(nameFromType, test.name);
    EXPECT_EQ(typeFromName, test.type);
    EXPECT_EQ(test.type, ContentSecurityPolicy::getDirectiveType(nameFromType));
    EXPECT_EQ(test.name, ContentSecurityPolicy::getDirectiveName(typeFromName));
  }
}

TEST_F(ContentSecurityPolicyTest, Subsumes) {
  ContentSecurityPolicy* other = ContentSecurityPolicy::create();
  EXPECT_TRUE(csp->subsumes(*other));
  EXPECT_TRUE(other->subsumes(*csp));

  csp->didReceiveHeader("default-src http://example.com;",
                        ContentSecurityPolicyHeaderTypeEnforce,
                        ContentSecurityPolicyHeaderSourceHTTP);
  // If this CSP is not empty, the other must not be empty either.
  EXPECT_FALSE(csp->subsumes(*other));
  EXPECT_TRUE(other->subsumes(*csp));

  // Report-only policies do not impact subsumption.
  other->didReceiveHeader("default-src http://example.com;",
                          ContentSecurityPolicyHeaderTypeReport,
                          ContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_FALSE(csp->subsumes(*other));

  // CSPDirectiveLists have to subsume.
  other->didReceiveHeader("default-src http://example.com https://another.com;",
                          ContentSecurityPolicyHeaderTypeEnforce,
                          ContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_FALSE(csp->subsumes(*other));

  // `other` is stricter than `this`.
  other->didReceiveHeader("default-src https://example.com;",
                          ContentSecurityPolicyHeaderTypeEnforce,
                          ContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(csp->subsumes(*other));
}

}  // namespace blink
