// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <algorithm>

#include "base/bind.h"
#include "base/macros.h"
#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/threat_details.h"
#include "chrome/browser/safe_browsing/threat_details_history.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_service.h"
#include "components/safe_browsing/common/safebrowsing_messages.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;
using content::WebContents;

namespace safe_browsing {

namespace {

// Mixture of HTTP and HTTPS.  No special treatment for HTTPS.
static const char* kOriginalLandingURL =
    "http://www.originallandingpage.com/with/path";
static const char* kDOMChildURL = "https://www.domparent.com/with/path";
static const char* kDOMParentURL = "https://www.domchild.com/with/path";
static const char* kFirstRedirectURL = "http://redirectone.com/with/path";
static const char* kSecondRedirectURL = "https://redirecttwo.com/with/path";
static const char* kReferrerURL = "http://www.referrer.com/with/path";

static const char* kThreatURL = "http://www.threat.com/with/path";
static const char* kThreatURLHttps = "https://www.threat.com/with/path";
static const char* kThreatHeaders =
    "HTTP/1.1 200 OK\n"
    "Content-Type: image/jpeg\n"
    "Some-Other-Header: foo\n";  // Persisted for http, stripped for https
static const char* kThreatData = "exploit();";

static const char* kLandingURL = "http://www.landingpage.com/with/path";
static const char* kLandingHeaders =
    "HTTP/1.1 200 OK\n"
    "Content-Type: text/html\n"
    "Content-Length: 1024\n"
    "Set-Cookie: tastycookie\n";  // This header is stripped.
static const char* kLandingData =
    "<iframe src='http://www.threat.com/with/path'>";

using content::BrowserThread;
using content::WebContents;

void WriteHeaders(disk_cache::Entry* entry, const std::string& headers) {
  net::HttpResponseInfo responseinfo;
  std::string raw_headers =
      net::HttpUtil::AssembleRawHeaders(headers.c_str(), headers.size());
  responseinfo.socket_address = net::HostPortPair("1.2.3.4", 80);
  responseinfo.headers = new net::HttpResponseHeaders(raw_headers);

  base::Pickle pickle;
  responseinfo.Persist(&pickle, false, false);

  scoped_refptr<net::WrappedIOBuffer> buf(
      new net::WrappedIOBuffer(reinterpret_cast<const char*>(pickle.data())));
  int len = static_cast<int>(pickle.size());

  net::TestCompletionCallback cb;
  int rv = entry->WriteData(0, 0, buf.get(), len, cb.callback(), true);
  ASSERT_EQ(len, cb.GetResult(rv));
}

void WriteData(disk_cache::Entry* entry, const std::string& data) {
  if (data.empty())
    return;

  int len = data.length();
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(len));
  memcpy(buf->data(), data.data(), data.length());

  net::TestCompletionCallback cb;
  int rv = entry->WriteData(1, 0, buf.get(), len, cb.callback(), true);
  ASSERT_EQ(len, cb.GetResult(rv));
}

void WriteToEntry(disk_cache::Backend* cache,
                  const std::string& key,
                  const std::string& headers,
                  const std::string& data) {
  net::TestCompletionCallback cb;
  disk_cache::Entry* entry;
  int rv = cache->CreateEntry(key, &entry, cb.callback());
  rv = cb.GetResult(rv);
  if (rv != net::OK) {
    rv = cache->OpenEntry(key, &entry, cb.callback());
    ASSERT_EQ(net::OK, cb.GetResult(rv));
  }

  WriteHeaders(entry, headers);
  WriteData(entry, data);
  entry->Close();
}

void FillCacheBase(net::URLRequestContextGetter* context_getter,
                   bool use_https_threat_url) {
  net::TestCompletionCallback cb;
  disk_cache::Backend* cache;
  int rv = context_getter->GetURLRequestContext()
               ->http_transaction_factory()
               ->GetCache()
               ->GetBackend(&cache, cb.callback());
  ASSERT_EQ(net::OK, cb.GetResult(rv));

  WriteToEntry(cache, use_https_threat_url ? kThreatURLHttps : kThreatURL,
               kThreatHeaders, kThreatData);
  WriteToEntry(cache, kLandingURL, kLandingHeaders, kLandingData);
}
void FillCache(net::URLRequestContextGetter* context_getter) {
  FillCacheBase(context_getter, /*use_https_threat_url=*/false);
}
void FillCacheHttps(net::URLRequestContextGetter* context_getter) {
  FillCacheBase(context_getter, /*use_https_threat_url=*/true);
}

// Lets us provide a MockURLRequestContext with an HTTP Cache we pre-populate.
// Also exposes the constructor.
class ThreatDetailsWrap : public ThreatDetails {
 public:
  ThreatDetailsWrap(
      SafeBrowsingUIManager* ui_manager,
      WebContents* web_contents,
      const security_interstitials::UnsafeResource& unsafe_resource,
      net::URLRequestContextGetter* request_context_getter)
      : ThreatDetails(ui_manager, web_contents, unsafe_resource) {
    request_context_getter_ = request_context_getter;
  }

 private:
  ~ThreatDetailsWrap() override {}
};

class MockSafeBrowsingUIManager : public SafeBrowsingUIManager {
 public:
  base::RunLoop* run_loop_;
  // The safe browsing UI manager does not need a service for this test.
  MockSafeBrowsingUIManager() : SafeBrowsingUIManager(NULL), run_loop_(NULL) {}

  // When the ThreatDetails is done, this is called.
  void SendSerializedThreatDetails(const std::string& serialized) override {
    DVLOG(1) << "SendSerializedThreatDetails";
    run_loop_->Quit();
    run_loop_ = NULL;
    serialized_ = serialized;
  }

  // Used to synchronize SendSerializedThreatDetails() with
  // WaitForSerializedReport(). RunLoop::RunUntilIdle() is not sufficient
  // because the MessageLoop task queue completely drains at some point
  // between the send and the wait.
  void SetRunLoopToQuit(base::RunLoop* run_loop) {
    DCHECK(run_loop_ == NULL);
    run_loop_ = run_loop;
  }

  const std::string& GetSerialized() { return serialized_; }

 private:
  ~MockSafeBrowsingUIManager() override {}

  std::string serialized_;
  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingUIManager);
};

}  // namespace

class ThreatDetailsTest : public ChromeRenderViewHostTestHarness {
 public:
  typedef SafeBrowsingUIManager::UnsafeResource UnsafeResource;

  ThreatDetailsTest() : ui_manager_(new MockSafeBrowsingUIManager()) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ASSERT_TRUE(profile()->CreateHistoryService(true /* delete_file */,
                                                false /* no_db */));
  }

  void TearDown() override {
    profile()->DestroyHistoryService();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  static bool ResourceLessThan(
      const ClientSafeBrowsingReportRequest::Resource* lhs,
      const ClientSafeBrowsingReportRequest::Resource* rhs) {
    return lhs->id() < rhs->id();
  }

  std::string WaitForSerializedReport(ThreatDetails* report,
                                      bool did_proceed,
                                      int num_visit) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&ThreatDetails::FinishCollection, report,
                                       did_proceed, num_visit));
    // Wait for the callback (SendSerializedThreatDetails).
    DVLOG(1) << "Waiting for SendSerializedThreatDetails";
    base::RunLoop run_loop;
    ui_manager_->SetRunLoopToQuit(&run_loop);
    run_loop.Run();
    return ui_manager_->GetSerialized();
  }

  history::HistoryService* history_service() {
    return HistoryServiceFactory::GetForProfile(
        profile(), ServiceAccessType::EXPLICIT_ACCESS);
  }

 protected:
  void InitResource(UnsafeResource* resource,
                    SBThreatType threat_type,
                    bool is_subresource,
                    const GURL& url) {
    resource->url = url;
    resource->is_subresource = is_subresource;
    resource->threat_type = threat_type;
    resource->web_contents_getter =
        SafeBrowsingUIManager::UnsafeResource::GetWebContentsGetter(
            web_contents()->GetRenderProcessHost()->GetID(),
            web_contents()->GetMainFrame()->GetRoutingID());
  }

  void VerifyResults(const ClientSafeBrowsingReportRequest& report_pb,
                     const ClientSafeBrowsingReportRequest& expected_pb) {
    EXPECT_EQ(expected_pb.type(), report_pb.type());
    EXPECT_EQ(expected_pb.url(), report_pb.url());
    EXPECT_EQ(expected_pb.page_url(), report_pb.page_url());
    EXPECT_EQ(expected_pb.referrer_url(), report_pb.referrer_url());
    EXPECT_EQ(expected_pb.did_proceed(), report_pb.did_proceed());
    EXPECT_EQ(expected_pb.has_repeat_visit(), report_pb.has_repeat_visit());
    if (expected_pb.has_repeat_visit() && report_pb.has_repeat_visit()) {
      EXPECT_EQ(expected_pb.repeat_visit(), report_pb.repeat_visit());
    }

    ASSERT_EQ(expected_pb.resources_size(), report_pb.resources_size());
    // Sort the resources, to make the test deterministic
    std::vector<const ClientSafeBrowsingReportRequest::Resource*> resources;
    for (int i = 0; i < report_pb.resources_size(); ++i) {
      const ClientSafeBrowsingReportRequest::Resource& resource =
          report_pb.resources(i);
      resources.push_back(&resource);
    }
    std::sort(resources.begin(), resources.end(),
              &ThreatDetailsTest::ResourceLessThan);

    std::vector<const ClientSafeBrowsingReportRequest::Resource*> expected;
    for (int i = 0; i < report_pb.resources_size(); ++i) {
      const ClientSafeBrowsingReportRequest::Resource& resource =
          expected_pb.resources(i);
      expected.push_back(&resource);
    }
    std::sort(expected.begin(), expected.end(),
              &ThreatDetailsTest::ResourceLessThan);

    for (uint32_t i = 0; i < expected.size(); ++i) {
      VerifyResource(resources[i], expected[i]);
    }

    EXPECT_EQ(expected_pb.complete(), report_pb.complete());
  }

  void VerifyResource(
      const ClientSafeBrowsingReportRequest::Resource* resource,
      const ClientSafeBrowsingReportRequest::Resource* expected) {
    EXPECT_EQ(expected->id(), resource->id());
    EXPECT_EQ(expected->url(), resource->url());
    EXPECT_EQ(expected->parent_id(), resource->parent_id());
    ASSERT_EQ(expected->child_ids_size(), resource->child_ids_size());
    for (int i = 0; i < expected->child_ids_size(); i++) {
      EXPECT_EQ(expected->child_ids(i), resource->child_ids(i));
    }

    // Verify HTTP Responses
    if (expected->has_response()) {
      ASSERT_TRUE(resource->has_response());
      EXPECT_EQ(expected->response().firstline().code(),
                resource->response().firstline().code());

      ASSERT_EQ(expected->response().headers_size(),
                resource->response().headers_size());
      for (int i = 0; i < expected->response().headers_size(); ++i) {
        EXPECT_EQ(expected->response().headers(i).name(),
                  resource->response().headers(i).name());
        EXPECT_EQ(expected->response().headers(i).value(),
                  resource->response().headers(i).value());
      }

      EXPECT_EQ(expected->response().body(), resource->response().body());
      EXPECT_EQ(expected->response().bodylength(),
                resource->response().bodylength());
      EXPECT_EQ(expected->response().bodydigest(),
                resource->response().bodydigest());
    }

    // Verify IP:port pair
    EXPECT_EQ(expected->response().remote_ip(),
              resource->response().remote_ip());
  }

  // Adds a page to history.
  // The redirects is the redirect url chain leading to the url.
  void AddPageToHistory(const GURL& url, history::RedirectList* redirects) {
    // The last item of the redirect chain has to be the final url when adding
    // to history backend.
    redirects->push_back(url);
    history_service()->AddPage(url, base::Time::Now(),
                               reinterpret_cast<history::ContextID>(1), 0,
                               GURL(), *redirects, ui::PAGE_TRANSITION_TYPED,
                               history::SOURCE_BROWSED, false);
  }

  scoped_refptr<MockSafeBrowsingUIManager> ui_manager_;
};

// Tests creating a simple threat report of a malware URL.
TEST_F(ThreatDetailsTest, ThreatSubResource) {
  // Commit a load.
  content::WebContentsTester::For(web_contents())
      ->TestDidNavigateWithReferrer(
          web_contents()->GetMainFrame(), 0 /* nav_entry_id */,
          true /* did_create_new_entry */, GURL(kLandingURL),
          content::Referrer(GURL(kReferrerURL),
                            blink::WebReferrerPolicyDefault),
          ui::PAGE_TRANSITION_TYPED);

  UnsafeResource resource;
  InitResource(&resource, SB_THREAT_TYPE_URL_MALWARE, true /* is_subresource */,
               GURL(kThreatURL));

  scoped_refptr<ThreatDetailsWrap> report =
      new ThreatDetailsWrap(ui_manager_.get(), web_contents(), resource, NULL);

  std::string serialized = WaitForSerializedReport(
      report.get(), true /* did_proceed*/, 1 /* num_visit */);

  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_MALWARE);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  // Note that the referrer policy is not actually enacted here, since that's
  // done in Blink.
  expected.set_referrer_url(kReferrerURL);
  expected.set_did_proceed(true);
  expected.set_repeat_visit(true);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kThreatURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(2);
  pb_resource->set_url(kReferrerURL);

  VerifyResults(actual, expected);
}

// Tests creating a simple threat report of a phishing page where the
// subresource has a different original_url.
TEST_F(ThreatDetailsTest, ThreatSubResourceWithOriginalUrl) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));

  UnsafeResource resource;
  InitResource(&resource, SB_THREAT_TYPE_URL_PHISHING,
               true /* is_subresource */, GURL(kThreatURL));
  resource.original_url = GURL(kOriginalLandingURL);

  scoped_refptr<ThreatDetailsWrap> report =
      new ThreatDetailsWrap(ui_manager_.get(), web_contents(), resource, NULL);

  std::string serialized = WaitForSerializedReport(
      report.get(), false /* did_proceed*/, 1 /* num_visit */);

  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_PHISHING);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(false);
  expected.set_repeat_visit(true);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kOriginalLandingURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(2);
  pb_resource->set_url(kThreatURL);
  // The Resource for kThreatURL should have the Resource for
  // kOriginalLandingURL (with id 1) as parent.
  pb_resource->set_parent_id(1);

  VerifyResults(actual, expected);
}

// Tests creating a threat report of a UwS page with data from the renderer.
TEST_F(ThreatDetailsTest, ThreatDOMDetails) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));

  UnsafeResource resource;
  InitResource(&resource, SB_THREAT_TYPE_URL_UNWANTED,
               true /* is_subresource */, GURL(kThreatURL));

  scoped_refptr<ThreatDetailsWrap> report =
      new ThreatDetailsWrap(ui_manager_.get(), web_contents(), resource, NULL);

  // Send a message from the DOM, with 2 nodes, a parent and a child.
  std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node> params;
  SafeBrowsingHostMsg_ThreatDOMDetails_Node child_node;
  child_node.url = GURL(kDOMChildURL);
  child_node.tag_name = "iframe";
  child_node.parent = GURL(kDOMParentURL);
  params.push_back(child_node);
  SafeBrowsingHostMsg_ThreatDOMDetails_Node parent_node;
  parent_node.url = GURL(kDOMParentURL);
  parent_node.children.push_back(GURL(kDOMChildURL));
  params.push_back(parent_node);
  report->OnReceivedThreatDOMDetails(params);

  std::string serialized = WaitForSerializedReport(
      report.get(), false /* did_proceed*/, 0 /* num_visit */);
  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_UNWANTED);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(false);
  expected.set_repeat_visit(false);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kThreatURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(2);
  pb_resource->set_url(kDOMChildURL);
  pb_resource->set_parent_id(3);

  pb_resource = expected.add_resources();
  pb_resource->set_id(3);
  pb_resource->set_url(kDOMParentURL);
  pb_resource->add_child_ids(2);
  expected.set_complete(false);  // Since the cache was missing.

  VerifyResults(actual, expected);
}

// Tests creating a threat report of a malware page where there are redirect
// urls to an unsafe resource url.
TEST_F(ThreatDetailsTest, ThreatWithRedirectUrl) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));

  UnsafeResource resource;
  InitResource(&resource, SB_THREAT_TYPE_URL_MALWARE, true /* is_subresource */,
               GURL(kThreatURL));
  resource.original_url = GURL(kOriginalLandingURL);

  // add some redirect urls
  resource.redirect_urls.push_back(GURL(kFirstRedirectURL));
  resource.redirect_urls.push_back(GURL(kSecondRedirectURL));
  resource.redirect_urls.push_back(GURL(kThreatURL));

  scoped_refptr<ThreatDetailsWrap> report =
      new ThreatDetailsWrap(ui_manager_.get(), web_contents(), resource, NULL);

  std::string serialized = WaitForSerializedReport(
      report.get(), true /* did_proceed*/, 0 /* num_visit */);
  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_MALWARE);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(true);
  expected.set_repeat_visit(false);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kOriginalLandingURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(2);
  pb_resource->set_url(kThreatURL);
  pb_resource->set_parent_id(4);

  pb_resource = expected.add_resources();
  pb_resource->set_id(3);
  pb_resource->set_url(kFirstRedirectURL);
  pb_resource->set_parent_id(1);

  pb_resource = expected.add_resources();
  pb_resource->set_id(4);
  pb_resource->set_url(kSecondRedirectURL);
  pb_resource->set_parent_id(3);

  VerifyResults(actual, expected);
}

// Test collecting threat details for a blocked main frame load.
TEST_F(ThreatDetailsTest, ThreatOnMainPageLoadBlocked) {
  const char* kUnrelatedReferrerURL =
      "http://www.unrelatedreferrer.com/some/path";
  const char* kUnrelatedURL = "http://www.unrelated.com/some/path";

  // Load and commit an unrelated URL. The ThreatDetails should not use this
  // navigation entry.
  content::WebContentsTester::For(web_contents())
      ->TestDidNavigateWithReferrer(
          web_contents()->GetMainFrame(), 0 /* nav_entry_id */,
          true /* did_create_new_entry */, GURL(kUnrelatedURL),
          content::Referrer(GURL(kUnrelatedReferrerURL),
                            blink::WebReferrerPolicyDefault),
          ui::PAGE_TRANSITION_TYPED);

  // Start a pending load with a referrer.
  controller().LoadURL(GURL(kLandingURL),
                       content::Referrer(GURL(kReferrerURL),
                                         blink::WebReferrerPolicyDefault),
                       ui::PAGE_TRANSITION_TYPED, std::string());

  // Create UnsafeResource for the pending main page load.
  UnsafeResource resource;
  InitResource(&resource, SB_THREAT_TYPE_URL_MALWARE,
               false /* is_subresource */, GURL(kLandingURL));

  // Start ThreatDetails collection.
  scoped_refptr<ThreatDetailsWrap> report =
      new ThreatDetailsWrap(ui_manager_.get(), web_contents(), resource, NULL);

  // Simulate clicking don't proceed.
  controller().DiscardNonCommittedEntries();

  // Finish ThreatDetails collection.
  std::string serialized = WaitForSerializedReport(
      report.get(), false /* did_proceed*/, 1 /* num_visit */);

  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_MALWARE);
  expected.set_url(kLandingURL);
  expected.set_page_url(kLandingURL);
  // Note that the referrer policy is not actually enacted here, since that's
  // done in Blink.
  expected.set_referrer_url(kReferrerURL);
  expected.set_did_proceed(false);
  expected.set_repeat_visit(true);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kReferrerURL);

  VerifyResults(actual, expected);
}

// Tests that a pending load does not interfere with collecting threat details
// for the committed page.
TEST_F(ThreatDetailsTest, ThreatWithPendingLoad) {
  const char* kPendingReferrerURL = "http://www.pendingreferrer.com/some/path";
  const char* kPendingURL = "http://www.pending.com/some/path";

  // Load and commit the landing URL with a referrer.
  content::WebContentsTester::For(web_contents())
      ->TestDidNavigateWithReferrer(
          web_contents()->GetMainFrame(), 0 /* nav_entry_id */,
          true /* did_create_new_entry */, GURL(kLandingURL),
          content::Referrer(GURL(kReferrerURL),
                            blink::WebReferrerPolicyDefault),
          ui::PAGE_TRANSITION_TYPED);

  // Create UnsafeResource for fake sub-resource of landing page.
  UnsafeResource resource;
  InitResource(&resource, SB_THREAT_TYPE_URL_MALWARE, true /* is_subresource */,
               GURL(kThreatURL));

  // Start a pending load before creating ThreatDetails.
  controller().LoadURL(GURL(kPendingURL),
                       content::Referrer(GURL(kPendingReferrerURL),
                                         blink::WebReferrerPolicyDefault),
                       ui::PAGE_TRANSITION_TYPED, std::string());

  // Do ThreatDetails collection.
  scoped_refptr<ThreatDetailsWrap> report =
      new ThreatDetailsWrap(ui_manager_.get(), web_contents(), resource, NULL);
  std::string serialized = WaitForSerializedReport(
      report.get(), true /* did_proceed*/, 1 /* num_visit */);

  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_MALWARE);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  // Note that the referrer policy is not actually enacted here, since that's
  // done in Blink.
  expected.set_referrer_url(kReferrerURL);
  expected.set_did_proceed(true);
  expected.set_repeat_visit(true);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kThreatURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(2);
  pb_resource->set_url(kReferrerURL);

  VerifyResults(actual, expected);
}

TEST_F(ThreatDetailsTest, ThreatOnFreshTab) {
  // A fresh WebContents should not have any NavigationEntries yet. (See
  // https://crbug.com/524208.)
  EXPECT_EQ(nullptr, controller().GetLastCommittedEntry());
  EXPECT_EQ(nullptr, controller().GetPendingEntry());

  // Simulate a subresource malware hit (this could happen if the WebContents
  // was created with window.open, and had content injected into it).
  UnsafeResource resource;
  InitResource(&resource, SB_THREAT_TYPE_URL_MALWARE, true /* is_subresource */,
               GURL(kThreatURL));

  // Do ThreatDetails collection.
  scoped_refptr<ThreatDetailsWrap> report =
      new ThreatDetailsWrap(ui_manager_.get(), web_contents(), resource, NULL);
  std::string serialized = WaitForSerializedReport(
      report.get(), true /* did_proceed*/, 1 /* num_visit */);

  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_MALWARE);
  expected.set_url(kThreatURL);
  expected.set_did_proceed(true);
  expected.set_repeat_visit(true);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kThreatURL);

  VerifyResults(actual, expected);
}

// Tests the interaction with the HTTP cache.
TEST_F(ThreatDetailsTest, HTTPCache) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));

  UnsafeResource resource;
  InitResource(&resource, SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL,
               true /* is_subresource */, GURL(kThreatURL));

  scoped_refptr<ThreatDetailsWrap> report =
      new ThreatDetailsWrap(ui_manager_.get(), web_contents(), resource,
                            profile()->GetRequestContext());

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&FillCache,
                 base::RetainedRef(profile()->GetRequestContext())));

  // The cache collection starts after the IPC from the DOM is fired.
  std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node> params;
  report->OnReceivedThreatDOMDetails(params);

  // Let the cache callbacks complete.
  base::RunLoop().RunUntilIdle();

  DVLOG(1) << "Getting serialized report";
  std::string serialized = WaitForSerializedReport(
      report.get(), true /* did_proceed*/, -1 /* num_visit */);
  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::CLIENT_SIDE_PHISHING_URL);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(true);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  ClientSafeBrowsingReportRequest::HTTPResponse* pb_response =
      pb_resource->mutable_response();
  pb_response->mutable_firstline()->set_code(200);
  ClientSafeBrowsingReportRequest::HTTPHeader* pb_header =
      pb_response->add_headers();
  pb_header->set_name("Content-Type");
  pb_header->set_value("text/html");
  pb_header = pb_response->add_headers();
  pb_header->set_name("Content-Length");
  pb_header->set_value("1024");
  pb_header = pb_response->add_headers();
  pb_header->set_name("Set-Cookie");
  pb_header->set_value("");  // The cookie is dropped.
  pb_response->set_body(kLandingData);
  std::string landing_data(kLandingData);
  pb_response->set_bodylength(landing_data.size());
  pb_response->set_bodydigest(base::MD5String(landing_data));
  pb_response->set_remote_ip("1.2.3.4:80");

  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kThreatURL);
  pb_response = pb_resource->mutable_response();
  pb_response->mutable_firstline()->set_code(200);
  pb_header = pb_response->add_headers();
  pb_header->set_name("Content-Type");
  pb_header->set_value("image/jpeg");
  pb_header = pb_response->add_headers();
  pb_header->set_name("Some-Other-Header");
  pb_header->set_value("foo");
  pb_response->set_body(kThreatData);
  std::string threat_data(kThreatData);
  pb_response->set_bodylength(threat_data.size());
  pb_response->set_bodydigest(base::MD5String(threat_data));
  pb_response->set_remote_ip("1.2.3.4:80");
  expected.set_complete(true);

  VerifyResults(actual, expected);
}

// Test that only some fields of the HTTPS resource (eg: whitelisted headers)
// are reported.
TEST_F(ThreatDetailsTest, HttpsResourceSanitization) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));

  UnsafeResource resource;
  InitResource(&resource, SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL,
               true /* is_subresource */, GURL(kThreatURLHttps));

  scoped_refptr<ThreatDetailsWrap> report =
      new ThreatDetailsWrap(ui_manager_.get(), web_contents(), resource,
                            profile()->GetRequestContext());

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&FillCacheHttps,
                 base::RetainedRef(profile()->GetRequestContext())));

  // The cache collection starts after the IPC from the DOM is fired.
  std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node> params;
  report->OnReceivedThreatDOMDetails(params);

  // Let the cache callbacks complete.
  base::RunLoop().RunUntilIdle();

  DVLOG(1) << "Getting serialized report";
  std::string serialized = WaitForSerializedReport(
      report.get(), true /* did_proceed*/, -1 /* num_visit */);
  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::CLIENT_SIDE_PHISHING_URL);
  expected.set_url(kThreatURLHttps);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(true);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  ClientSafeBrowsingReportRequest::HTTPResponse* pb_response =
      pb_resource->mutable_response();
  pb_response->mutable_firstline()->set_code(200);
  ClientSafeBrowsingReportRequest::HTTPHeader* pb_header =
      pb_response->add_headers();
  pb_header->set_name("Content-Type");
  pb_header->set_value("text/html");
  pb_header = pb_response->add_headers();
  pb_header->set_name("Content-Length");
  pb_header->set_value("1024");
  pb_header = pb_response->add_headers();
  pb_header->set_name("Set-Cookie");
  pb_header->set_value("");  // The cookie is dropped.
  pb_response->set_body(kLandingData);
  std::string landing_data(kLandingData);
  pb_response->set_bodylength(landing_data.size());
  pb_response->set_bodydigest(base::MD5String(landing_data));
  pb_response->set_remote_ip("1.2.3.4:80");

  // The threat URL is HTTP so the request and response are cleared (except for
  // whitelisted headers and certain safe fields). Namely the firstline and body
  // are missing.
  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kThreatURLHttps);
  pb_response = pb_resource->mutable_response();
  pb_header = pb_response->add_headers();
  pb_header->set_name("Content-Type");
  pb_header->set_value("image/jpeg");
  std::string threat_data(kThreatData);
  pb_response->set_bodylength(threat_data.size());
  pb_response->set_bodydigest(base::MD5String(threat_data));
  pb_response->set_remote_ip("1.2.3.4:80");
  expected.set_complete(true);

  VerifyResults(actual, expected);
}

// Tests the interaction with the HTTP cache (where the cache is empty).
TEST_F(ThreatDetailsTest, HTTPCacheNoEntries) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));

  UnsafeResource resource;
  InitResource(&resource, SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL,
               true /* is_subresource */, GURL(kThreatURL));

  scoped_refptr<ThreatDetailsWrap> report =
      new ThreatDetailsWrap(ui_manager_.get(), web_contents(), resource,
                            profile()->GetRequestContext());

  // No call to FillCache

  // The cache collection starts after the IPC from the DOM is fired.
  std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node> params;
  report->OnReceivedThreatDOMDetails(params);

  // Let the cache callbacks complete.
  base::RunLoop().RunUntilIdle();

  DVLOG(1) << "Getting serialized report";
  std::string serialized = WaitForSerializedReport(
      report.get(), false /* did_proceed*/, -1 /* num_visit */);
  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::CLIENT_SIDE_MALWARE_URL);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(false);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kThreatURL);
  expected.set_complete(true);

  VerifyResults(actual, expected);
}

// Test getting redirects from history service.
TEST_F(ThreatDetailsTest, HistoryServiceUrls) {
  // Add content to history service.
  // There are two redirect urls before reacing malware url:
  // kFirstRedirectURL -> kSecondRedirectURL -> kThreatURL
  GURL baseurl(kThreatURL);
  history::RedirectList redirects;
  redirects.push_back(GURL(kFirstRedirectURL));
  redirects.push_back(GURL(kSecondRedirectURL));
  AddPageToHistory(baseurl, &redirects);
  // Wait for history service operation finished.
  profile()->BlockUntilHistoryProcessesPendingRequests();

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));

  UnsafeResource resource;
  InitResource(&resource, SB_THREAT_TYPE_URL_MALWARE, true /* is_subresource */,
               GURL(kThreatURL));
  scoped_refptr<ThreatDetailsWrap> report =
      new ThreatDetailsWrap(ui_manager_.get(), web_contents(), resource, NULL);

  // The redirects collection starts after the IPC from the DOM is fired.
  std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node> params;
  report->OnReceivedThreatDOMDetails(params);

  // Let the redirects callbacks complete.
  base::RunLoop().RunUntilIdle();

  std::string serialized = WaitForSerializedReport(
      report.get(), true /* did_proceed*/, 1 /* num_visit */);
  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_MALWARE);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(true);
  expected.set_repeat_visit(true);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_parent_id(2);
  pb_resource->set_url(kThreatURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(2);
  pb_resource->set_parent_id(3);
  pb_resource->set_url(kSecondRedirectURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(3);
  pb_resource->set_url(kFirstRedirectURL);

  VerifyResults(actual, expected);
}

}  // namespace safe_browsing
