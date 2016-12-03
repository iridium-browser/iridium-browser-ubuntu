// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_resource_throttle.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/test/test_browser_thread.h"
#include "ipc/ipc_message.h"
#include "net/base/request_priority.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using content::ResourceType;

namespace prerender {

namespace {

class TestPrerenderContents : public PrerenderContents {
 public:
  TestPrerenderContents(PrerenderManager* prerender_manager,
                        int child_id, int route_id)
      : PrerenderContents(prerender_manager, static_cast<Profile*>(NULL),
                          GURL(), content::Referrer(), ORIGIN_NONE),
        child_id_(child_id),
        route_id_(route_id) {
    PrerenderResourceThrottle::OverridePrerenderContentsForTesting(this);
  }

  ~TestPrerenderContents() override {
    if (final_status() == FINAL_STATUS_MAX)
      SetFinalStatus(FINAL_STATUS_USED);
    PrerenderResourceThrottle::OverridePrerenderContentsForTesting(NULL);
  }

  bool GetChildId(int* child_id) const override {
    *child_id = child_id_;
    return true;
  }

  bool GetRouteId(int* route_id) const override {
    *route_id = route_id_;
    return true;
  }

  void Start() {
    prerendering_has_started_ = true;
    NotifyPrerenderStart();
  }

  void Cancel() {
    Destroy(FINAL_STATUS_CANCELLED);
  }

  void Use() {
    PrepareForUse();
  }

 private:
  int child_id_;
  int route_id_;
};

class TestPrerenderManager : public PrerenderManager {
 public:
  TestPrerenderManager() : PrerenderManager(nullptr) {
    mutable_config().rate_limit_enabled = false;
  }

  // We never allocate our PrerenderContents in PrerenderManager, so we don't
  // ever want the default pending delete behaviour.
  void MoveEntryToPendingDelete(PrerenderContents* entry,
                                FinalStatus final_status) override {}
};

class DeferredRedirectDelegate : public net::URLRequest::Delegate,
                                 public content::ResourceController {
 public:
  DeferredRedirectDelegate()
      : throttle_(NULL),
        was_deferred_(false),
        cancel_called_(false),
        resume_called_(false) {
  }

  void SetThrottle(PrerenderResourceThrottle* throttle) {
    throttle_ = throttle;
    throttle_->set_controller_for_testing(this);
  }

  void Run() {
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  bool was_deferred() const { return was_deferred_; }
  bool cancel_called() const { return cancel_called_; }
  bool resume_called() const { return resume_called_; }

  // net::URLRequest::Delegate implementation:
  void OnReceivedRedirect(net::URLRequest* request,
                          const net::RedirectInfo& redirect_info,
                          bool* defer_redirect) override {
    // Defer the redirect either way.
    *defer_redirect = true;

    // Find out what the throttle would have done.
    throttle_->WillRedirectRequest(redirect_info, &was_deferred_);
    run_loop_->Quit();
  }
  void OnResponseStarted(net::URLRequest* request) override {}
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override {}

  // content::ResourceController implementation:
  void Cancel() override {
    EXPECT_FALSE(cancel_called_);
    EXPECT_FALSE(resume_called_);

    cancel_called_ = true;
    run_loop_->Quit();
  }
  void CancelAndIgnore() override { Cancel(); }
  void CancelWithError(int error_code) override { Cancel(); }
  void Resume() override {
    EXPECT_TRUE(was_deferred_);
    EXPECT_FALSE(cancel_called_);
    EXPECT_FALSE(resume_called_);

    resume_called_ = true;
    run_loop_->Quit();
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  PrerenderResourceThrottle* throttle_;
  bool was_deferred_;
  bool cancel_called_;
  bool resume_called_;

  DISALLOW_COPY_AND_ASSIGN(DeferredRedirectDelegate);
};

}  // namespace

class PrerenderResourceThrottleTest : public testing::Test {
 public:
  static const int kDefaultChildId = 0;
  static const int kDefaultRouteId = 100;

  PrerenderResourceThrottleTest() :
      ui_thread_(BrowserThread::UI, &message_loop_),
      io_thread_(BrowserThread::IO, &message_loop_),
      test_contents_(&prerender_manager_, kDefaultChildId, kDefaultRouteId) {
    chrome_browser_net::SetUrlRequestMocksEnabled(true);
  }

  ~PrerenderResourceThrottleTest() override {
    chrome_browser_net::SetUrlRequestMocksEnabled(false);

    // Cleanup work so the file IO tasks from URLRequestMockHTTPJob
    // are gone.
    content::BrowserThread::GetBlockingPool()->FlushForTesting();
    RunEvents();
  }

  TestPrerenderManager* prerender_manager() {
    return &prerender_manager_;
  }

  TestPrerenderContents* test_contents() {
    return &test_contents_;
  }

  // Runs any tasks queued on either thread.
  void RunEvents() { base::RunLoop().RunUntilIdle(); }

 private:
  base::MessageLoopForIO message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;

  TestPrerenderManager prerender_manager_;
  TestPrerenderContents test_contents_;
};

// Checks that deferred redirects are throttled and resumed correctly.
TEST_F(PrerenderResourceThrottleTest, RedirectResume) {
  test_contents()->Start();
  RunEvents();

  // Fake a request.
  net::TestURLRequestContext url_request_context;
  DeferredRedirectDelegate delegate;
  std::unique_ptr<net::URLRequest> request(url_request_context.CreateRequest(
      net::URLRequestMockHTTPJob::GetMockUrl("prerender/image-deferred.png"),
      net::DEFAULT_PRIORITY, &delegate));
  content::ResourceRequestInfo::AllocateForTesting(
      request.get(),
      content::RESOURCE_TYPE_IMAGE,
      NULL,
      kDefaultChildId,
      kDefaultRouteId,
      MSG_ROUTING_NONE,
      false,  // is_main_frame
      false,  // parent_is_main_frame
      true,   // allow_download
      true,   // is_async
      false); // is_using_lofi

  // Install a prerender throttle.
  PrerenderResourceThrottle throttle(request.get());
  delegate.SetThrottle(&throttle);

  // Start the request and wait for a redirect.
  request->Start();
  delegate.Run();
  EXPECT_TRUE(delegate.was_deferred());
  // This calls WillRedirectRequestOnUI().
  RunEvents();

  // Display the prerendered RenderView and wait for the throttle to
  // notice.
  test_contents()->Use();
  delegate.Run();
  EXPECT_TRUE(delegate.resume_called());
  EXPECT_FALSE(delegate.cancel_called());
}

// Checks that redirects in main frame loads are not deferred.
TEST_F(PrerenderResourceThrottleTest, RedirectMainFrame) {
  test_contents()->Start();
  RunEvents();

  // Fake a request.
  net::TestURLRequestContext url_request_context;
  DeferredRedirectDelegate delegate;
  std::unique_ptr<net::URLRequest> request(url_request_context.CreateRequest(
      net::URLRequestMockHTTPJob::GetMockUrl("prerender/image-deferred.png"),
      net::DEFAULT_PRIORITY, &delegate));
  content::ResourceRequestInfo::AllocateForTesting(
      request.get(),
      content::RESOURCE_TYPE_MAIN_FRAME,
      NULL,
      kDefaultChildId,
      kDefaultRouteId,
      MSG_ROUTING_NONE,
      true,   // is_main_frame
      false,  // parent_is_main_frame
      true,   // allow_download
      true,   // is_async
      false); // is_using_lofi

  // Install a prerender throttle.
  PrerenderResourceThrottle throttle(request.get());
  delegate.SetThrottle(&throttle);

  // Start the request and wait for a redirect. This time, it should
  // not be deferred.
  request->Start();
  delegate.Run();
  // This calls WillRedirectRequestOnUI().
  RunEvents();

  // Cleanup work so the prerender is gone.
  test_contents()->Cancel();
  RunEvents();
}

// Checks that attempting to defer a synchronous request aborts the
// prerender.
TEST_F(PrerenderResourceThrottleTest, RedirectSyncXHR) {
  test_contents()->Start();
  RunEvents();

  // Fake a request.
  net::TestURLRequestContext url_request_context;
  DeferredRedirectDelegate delegate;
  std::unique_ptr<net::URLRequest> request(url_request_context.CreateRequest(
      net::URLRequestMockHTTPJob::GetMockUrl("prerender/image-deferred.png"),
      net::DEFAULT_PRIORITY, &delegate));
  content::ResourceRequestInfo::AllocateForTesting(
      request.get(),
      content::RESOURCE_TYPE_XHR,
      NULL,
      kDefaultChildId,
      kDefaultRouteId,
      MSG_ROUTING_NONE,
      false,   // is_main_frame
      false,   // parent_is_main_frame
      true,    // allow_download
      false,   // is_async
      false);  // is_using_lofi

  // Install a prerender throttle.
  PrerenderResourceThrottle throttle(request.get());
  delegate.SetThrottle(&throttle);

  // Start the request and wait for a redirect.
  request->Start();
  delegate.Run();
  // This calls WillRedirectRequestOnUI().
  RunEvents();

  // We should have cancelled the prerender.
  EXPECT_EQ(FINAL_STATUS_BAD_DEFERRED_REDIRECT,
            test_contents()->final_status());

  // Cleanup work so the prerender is gone.
  test_contents()->Cancel();
  RunEvents();
}

}  // namespace prerender
