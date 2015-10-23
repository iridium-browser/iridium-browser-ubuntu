// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Note that although this is not a "browser" test, it runs as part of
// browser_tests.  This is because WebKit does not work properly if it is
// shutdown and re-initialized.  Since browser_tests runs each test in a
// new process, this avoids the problem.

#include "chrome/renderer/safe_browsing/phishing_dom_feature_extractor.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/renderer/safe_browsing/features.h"
#include "chrome/renderer/safe_browsing/mock_feature_extractor_clock.h"
#include "chrome/renderer/safe_browsing/test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/renderer/render_view.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebView.h"

using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;

namespace {

// The first RenderFrame is routing ID 1, and the first RenderView is 2.
const int kRenderViewRoutingId = 2;

}

namespace safe_browsing {

class PhishingDOMFeatureExtractorTest : public InProcessBrowserTest {
 public:
  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Helper for the SubframeRemoval test that posts a message to remove
  // the iframe "frame1" from the document.
  void ScheduleRemoveIframe() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&PhishingDOMFeatureExtractorTest::RemoveIframe,
                              weak_factory_.GetWeakPtr()));
  }

 protected:
  PhishingDOMFeatureExtractorTest() : weak_factory_(this) {}

  ~PhishingDOMFeatureExtractorTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kSingleProcess);
#if defined(OS_WIN)
    // Don't want to try to create a GPU process.
    command_line->AppendSwitch(switches::kDisableGpu);
#endif
  }

  void SetUpOnMainThread() override {
    extractor_.reset(new PhishingDOMFeatureExtractor(
        content::RenderView::FromRoutingID(kRenderViewRoutingId), &clock_));

    ASSERT_TRUE(StartTestServer());
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  // Runs the DOMFeatureExtractor on the RenderView, waiting for the
  // completion callback.  Returns the success boolean from the callback.
  bool ExtractFeatures(FeatureMap* features) {
    success_ = false;
    PostTaskToInProcessRendererAndWait(
        base::Bind(&PhishingDOMFeatureExtractorTest::ExtractFeaturesInternal,
        base::Unretained(this),
        features));
    return success_;
  }

  void ExtractFeaturesInternal(FeatureMap* features) {
    scoped_refptr<content::MessageLoopRunner> message_loop =
        new content::MessageLoopRunner;
    extractor_->ExtractFeatures(
        features,
        base::Bind(&PhishingDOMFeatureExtractorTest::ExtractionDone,
                   base::Unretained(this),
                   message_loop->QuitClosure()));
    message_loop->Run();
  }

  // Completion callback for feature extraction.
  void ExtractionDone(const base::Closure& quit_closure,
                      bool success) {
    success_ = success;
    quit_closure.Run();
  }

  // Does the actual work of removing the iframe "frame1" from the document.
  void RemoveIframe() {
    content::RenderView* render_view =
        content::RenderView::FromRoutingID(kRenderViewRoutingId);
    blink::WebFrame* main_frame = render_view->GetWebView()->mainFrame();
    ASSERT_TRUE(main_frame);
    main_frame->executeScript(
        blink::WebString(
            "document.body.removeChild(document.getElementById('frame1'));"));
  }

  bool StartTestServer() {
    CHECK(!embedded_test_server_);
    embedded_test_server_.reset(new net::test_server::EmbeddedTestServer());
    embedded_test_server_->RegisterRequestHandler(
        base::Bind(&PhishingDOMFeatureExtractorTest::HandleRequest,
                   base::Unretained(this)));
    return embedded_test_server_->InitializeAndWaitUntilReady();
  }

  scoped_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    std::map<std::string, std::string>::const_iterator host_it =
        request.headers.find("Host");
    if (host_it == request.headers.end())
      return scoped_ptr<net::test_server::HttpResponse>();

    std::string url =
        std::string("http://") + host_it->second + request.relative_url;
    std::map<std::string, std::string>::const_iterator it =
        responses_.find(url);
    if (it == responses_.end())
      return scoped_ptr<net::test_server::HttpResponse>();

    scoped_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse());
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/html");
    http_response->set_content(it->second);
    return http_response.Pass();
  }

  GURL GetURL(const std::string& host, const std::string& path) {
    GURL::Replacements replace;
    replace.SetHostStr(host);
    replace.SetPathStr(path);
    return embedded_test_server_->base_url().ReplaceComponents(replace);
  }

  // Returns the URL that was loaded.
  GURL LoadHtml(const std::string& host, const std::string& content) {
    GURL url(GetURL(host, ""));
    responses_[url.spec()] = content;
    ui_test_utils::NavigateToURL(browser(), url);
    return url;
  }

  // Map of url -> response body for network requests from the renderer.
  // Any urls not in this map are served a 404 error.
  std::map<std::string, std::string> responses_;

  scoped_ptr<net::test_server::EmbeddedTestServer> embedded_test_server_;
  MockFeatureExtractorClock clock_;
  scoped_ptr<PhishingDOMFeatureExtractor> extractor_;
  bool success_;  // holds the success value from ExtractFeatures
  base::WeakPtrFactory<PhishingDOMFeatureExtractorTest> weak_factory_;
};

IN_PROC_BROWSER_TEST_F(PhishingDOMFeatureExtractorTest, FormFeatures) {
  // This test doesn't exercise the extraction timing.
  EXPECT_CALL(clock_, Now()).WillRepeatedly(Return(base::TimeTicks::Now()));

  FeatureMap expected_features;
  expected_features.AddBooleanFeature(features::kPageHasForms);
  expected_features.AddRealFeature(features::kPageActionOtherDomainFreq, 0.25);
  expected_features.AddBooleanFeature(features::kPageHasTextInputs);
  expected_features.AddBooleanFeature(features::kPageHasCheckInputs);
  expected_features.AddBooleanFeature(features::kPageActionURL +
      std::string("http://cgi.host.com/submit"));
  expected_features.AddBooleanFeature(features::kPageActionURL +
      std::string("http://other.com/"));
  expected_features.AddBooleanFeature(features::kPageActionURL +
      std::string("http://host.com:") +
      base::IntToString(embedded_test_server_->port()) +
      std::string("/query"));

  FeatureMap features;
  LoadHtml(
      "host.com",
      "<html><head><body>"
      "<form action=\"query\"><input type=text><input type=checkbox></form>"
      "<form action=\"http://cgi.host.com/submit\"></form>"
      "<form action=\"http://other.com/\"></form>"
      "<form action=\"query\"></form>"
      "<form></form></body></html>");
  ASSERT_TRUE(ExtractFeatures(&features));
  ExpectFeatureMapsAreEqual(features, expected_features);

  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kPageHasRadioInputs);
  expected_features.AddBooleanFeature(features::kPageHasPswdInputs);

  features.Clear();
  LoadHtml(
      "host.com",
      "<html><head><body>"
      "<input type=\"radio\"><input type=password></body></html>");
  ASSERT_TRUE(ExtractFeatures(&features));
  ExpectFeatureMapsAreEqual(features, expected_features);

  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kPageHasTextInputs);

  features.Clear();
  LoadHtml(
      "host.com",
      "<html><head><body><input></body></html>");
  ASSERT_TRUE(ExtractFeatures(&features));
  ExpectFeatureMapsAreEqual(features, expected_features);

  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kPageHasTextInputs);

  features.Clear();
  LoadHtml(
      "host.com",
      "<html><head><body><input type=\"invalid\"></body></html>");
  ASSERT_TRUE(ExtractFeatures(&features));
  ExpectFeatureMapsAreEqual(features, expected_features);
}

IN_PROC_BROWSER_TEST_F(PhishingDOMFeatureExtractorTest, LinkFeatures) {
  // This test doesn't exercise the extraction timing.
  EXPECT_CALL(clock_, Now()).WillRepeatedly(Return(base::TimeTicks::Now()));

  FeatureMap expected_features;
  expected_features.AddRealFeature(features::kPageExternalLinksFreq, 0.5);
  expected_features.AddRealFeature(features::kPageSecureLinksFreq, 0.0);
  expected_features.AddBooleanFeature(features::kPageLinkDomain +
                                      std::string("chromium.org"));

  FeatureMap features;
  LoadHtml(
      "www.host.com",
      "<html><head><body>"
      "<a href=\"http://www2.host.com/abc\">link</a>"
      "<a name=page_anchor></a>"
      "<a href=\"http://www.chromium.org/\">chromium</a>"
      "</body></html");
  ASSERT_TRUE(ExtractFeatures(&features));
  ExpectFeatureMapsAreEqual(features, expected_features);

  expected_features.Clear();
  expected_features.AddRealFeature(features::kPageExternalLinksFreq, 0.25);
  expected_features.AddRealFeature(features::kPageSecureLinksFreq, 0.5);
  expected_features.AddBooleanFeature(features::kPageLinkDomain +
                                      std::string("chromium.org"));

  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  // The PhishingDOMFeatureExtractor depends on URLs being domains and not IPs,
  // so use a domain.
  std::string url_str = "https://host.com:";
  url_str += base::IntToString(https_server.host_port_pair().port());
  url_str += "/files/safe_browsing/secure_link_features.html";
  ui_test_utils::NavigateToURL(browser(), GURL(url_str));

  // Click through the certificate error interstitial.
  content::InterstitialPage* interstitial_page =
      GetWebContents()->GetInterstitialPage();
  interstitial_page->Proceed();
  content::WaitForLoadStop(GetWebContents());

  features.Clear();
  ASSERT_TRUE(ExtractFeatures(&features));
  ExpectFeatureMapsAreEqual(features, expected_features);
}

// Flaky on Win/Linux.  https://crbug.com/373155.
#if defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_ScriptAndImageFeatures DISABLED_ScriptAndImageFeatures
#else
#define MAYBE_ScriptAndImageFeatures ScriptAndImageFeatures
#endif
IN_PROC_BROWSER_TEST_F(PhishingDOMFeatureExtractorTest,
                       MAYBE_ScriptAndImageFeatures) {
  // This test doesn't exercise the extraction timing.
  EXPECT_CALL(clock_, Now()).WillRepeatedly(Return(base::TimeTicks::Now()));

  FeatureMap expected_features;
  expected_features.AddBooleanFeature(features::kPageNumScriptTagsGTOne);

  FeatureMap features;
  LoadHtml(
      "host.com",
      "<html><head><script></script><script></script></head></html>");
  ASSERT_TRUE(ExtractFeatures(&features));
  ExpectFeatureMapsAreEqual(features, expected_features);

  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kPageNumScriptTagsGTOne);
  expected_features.AddBooleanFeature(features::kPageNumScriptTagsGTSix);
  expected_features.AddRealFeature(features::kPageImgOtherDomainFreq, 0.5);

  features.Clear();
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  // The PhishingDOMFeatureExtractor depends on URLs being domains and not IPs,
  // so use a domain.
  std::string url_str = "https://host.com:";
  url_str += base::IntToString(https_server.host_port_pair().port());
  url_str += "/files/safe_browsing/secure_script_and_image.html";
  ui_test_utils::NavigateToURL(browser(), GURL(url_str));

  // Click through the certificate error interstitial.
  content::InterstitialPage* interstitial_page =
      GetWebContents()->GetInterstitialPage();
  interstitial_page->Proceed();
  content::WaitForLoadStop(GetWebContents());

  ASSERT_TRUE(ExtractFeatures(&features));
  ExpectFeatureMapsAreEqual(features, expected_features);
}

IN_PROC_BROWSER_TEST_F(PhishingDOMFeatureExtractorTest, SubFrames) {
  // This test doesn't exercise the extraction timing.
  EXPECT_CALL(clock_, Now()).WillRepeatedly(Return(base::TimeTicks::Now()));

  // Test that features are aggregated across all frames.

  std::string port = base::IntToString(embedded_test_server_->port());
  responses_[GetURL("host2.com", "").spec()] =
      "<html><head><script></script><body>"
      "<form action=\"http://host4.com/\"><input type=checkbox></form>"
      "<form action=\"http://host2.com/submit\"></form>"
      "<a href=\"http://www.host2.com/home\">link</a>"
      "<iframe src=\"nested.html\"></iframe>"
      "<body></html>";

  responses_[GetURL("host2.com", "nested.html").spec()] =
      "<html><body><input type=password>"
      "<a href=\"https://host4.com/\">link</a>"
      "<a href=\"relative\">another</a>"
      "</body></html>";

  responses_[GetURL("host3.com", "").spec()] =
      "<html><head><script></script><body>"
      "<img src=\"http://host.com/123.png\">"
      "</body></html>";

  FeatureMap expected_features;
  expected_features.AddBooleanFeature(features::kPageHasForms);
  // Form action domains are compared to the URL of the document they're in,
  // not the URL of the toplevel page.  So http://host2.com/ has two form
  // actions, one of which is external.
  expected_features.AddRealFeature(features::kPageActionOtherDomainFreq, 0.5);
  expected_features.AddBooleanFeature(features::kPageHasTextInputs);
  expected_features.AddBooleanFeature(features::kPageHasPswdInputs);
  expected_features.AddBooleanFeature(features::kPageHasCheckInputs);
  expected_features.AddRealFeature(features::kPageExternalLinksFreq, 0.25);
  expected_features.AddBooleanFeature(features::kPageLinkDomain +
                                      std::string("host4.com"));
  expected_features.AddRealFeature(features::kPageSecureLinksFreq, 0.25);
  expected_features.AddBooleanFeature(features::kPageNumScriptTagsGTOne);
  expected_features.AddRealFeature(features::kPageImgOtherDomainFreq, 1.0);
  expected_features.AddBooleanFeature(features::kPageActionURL +
      std::string("http://host2.com/submit"));
  expected_features.AddBooleanFeature(features::kPageActionURL +
      std::string("http://host4.com/"));

  FeatureMap features;
  std::string html(
      "<html><body><input type=text><a href=\"info.html\">link</a>"
      "<iframe src=\"http://host2.com:");
  html += port;
  html += std::string(
      "/\"></iframe>"
      "<iframe src=\"http://host3.com:");
  html += port;
  html += std::string("/\"></iframe></body></html>");

  LoadHtml("host.com", html);
  ASSERT_TRUE(ExtractFeatures(&features));
  ExpectFeatureMapsAreEqual(features, expected_features);
}

// Test flakes with LSAN enabled. See http://crbug.com/373155.
#if defined(LEAK_SANITIZER)
#define MAYBE_Continuation DISABLED_Continuation
#else
#define MAYBE_Continuation Continuation
#endif
IN_PROC_BROWSER_TEST_F(PhishingDOMFeatureExtractorTest, MAYBE_Continuation) {
  // For this test, we'll cause the feature extraction to run multiple
  // iterations by incrementing the clock.

  // This page has a total of 50 elements.  For the external forms feature to
  // be computed correctly, the extractor has to examine the whole document.
  // Note: the empty HEAD is important -- WebKit will synthesize a HEAD if
  // there isn't one present, which can be confusing for the element counts.
  std::string response = "<html><head></head><body>"
      "<form action=\"ondomain\"></form>";
  for (int i = 0; i < 45; ++i) {
    response.append("<p>");
  }
  response.append("<form action=\"http://host2.com/\"></form></body></html>");

  // Advance the clock 6 ms every 10 elements processed, 10 ms between chunks.
  // Note that this assumes kClockCheckGranularity = 10 and
  // kMaxTimePerChunkMs = 10.
  base::TimeTicks now = base::TimeTicks::Now();
  EXPECT_CALL(clock_, Now())
      // Time check at the start of extraction.
      .WillOnce(Return(now))
      // Time check at the start of the first chunk of work.
      .WillOnce(Return(now))
      // Time check after the first 10 elements.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(6)))
      // Time check after the next 10 elements.  This is over the chunk
      // time limit, so a continuation task will be posted.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(12)))
      // Time check at the start of the second chunk of work.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(22)))
      // Time check after resuming iteration for the second chunk.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(24)))
      // Time check after the next 10 elements.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(30)))
      // Time check after the next 10 elements.  This will trigger another
      // continuation task.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(36)))
      // Time check at the start of the third chunk of work.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(46)))
      // Time check after resuming iteration for the third chunk.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(48)))
      // Time check after the last 10 elements.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(54)))
      // A final time check for the histograms.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(56)));

  FeatureMap expected_features;
  expected_features.AddBooleanFeature(features::kPageHasForms);
  expected_features.AddRealFeature(features::kPageActionOtherDomainFreq, 0.5);
  expected_features.AddBooleanFeature(features::kPageActionURL +
      std::string("http://host.com:") +
      base::IntToString(embedded_test_server_->port()) +
      std::string("/ondomain"));
  expected_features.AddBooleanFeature(features::kPageActionURL +
      std::string("http://host2.com/"));

  FeatureMap features;
  LoadHtml("host.com", response);
  ASSERT_TRUE(ExtractFeatures(&features));
  ExpectFeatureMapsAreEqual(features, expected_features);
  // Make sure none of the mock expectations carry over to the next test.
  ::testing::Mock::VerifyAndClearExpectations(&clock_);

  // Now repeat the test with the same page, but advance the clock faster so
  // that the extraction time exceeds the maximum total time for the feature
  // extractor.  Extraction should fail.  Note that this assumes
  // kMaxTotalTimeMs = 500.
  EXPECT_CALL(clock_, Now())
      // Time check at the start of extraction.
      .WillOnce(Return(now))
      // Time check at the start of the first chunk of work.
      .WillOnce(Return(now))
      // Time check after the first 10 elements.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(300)))
      // Time check at the start of the second chunk of work.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(350)))
      // Time check after resuming iteration for the second chunk.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(360)))
      // Time check after the next 10 elements.  This is over the limit.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(600)))
      // A final time check for the histograms.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(620)));

  features.Clear();
  EXPECT_FALSE(ExtractFeatures(&features));
}

IN_PROC_BROWSER_TEST_F(PhishingDOMFeatureExtractorTest, SubframeRemoval) {
  // In this test, we'll advance the feature extractor so that it is positioned
  // inside an iframe, and have it pause due to exceeding the chunk time limit.
  // Then, prior to continuation, the iframe is removed from the document.
  // As currently implemented, this should finish extraction from the removed
  // iframe document.
  responses_[GetURL("host.com", "frame.html").spec()] =
      "<html><body><p><p><p><input type=password></body></html>";

  base::TimeTicks now = base::TimeTicks::Now();
  EXPECT_CALL(clock_, Now())
      // Time check at the start of extraction.
      .WillOnce(Return(now))
      // Time check at the start of the first chunk of work.
      .WillOnce(Return(now))
      // Time check after the first 10 elements.  Enough time has passed
      // to stop extraction.  Schedule the iframe removal to happen as soon as
      // the feature extractor returns control to the message loop.
      .WillOnce(DoAll(
          Invoke(this, &PhishingDOMFeatureExtractorTest::ScheduleRemoveIframe),
          Return(now + base::TimeDelta::FromMilliseconds(21))))
      // Time check at the start of the second chunk of work.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(25)))
      // Time check after resuming iteration for the second chunk.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(27)))
      // A final time check for the histograms.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(33)));

  FeatureMap expected_features;
  expected_features.AddBooleanFeature(features::kPageHasForms);
  expected_features.AddBooleanFeature(features::kPageHasPswdInputs);

  FeatureMap features;
  LoadHtml(
      "host.com",
      "<html><head></head><body>"
      "<iframe src=\"frame.html\" id=\"frame1\"></iframe>"
      "<form></form></body></html>");
  ASSERT_TRUE(ExtractFeatures(&features));
  ExpectFeatureMapsAreEqual(features, expected_features);
}

}  // namespace safe_browsing
