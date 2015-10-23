// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class CancelAfterFirstReadURLRequestDelegate : public net::TestDelegate {
 public:
  CancelAfterFirstReadURLRequestDelegate() {}

  ~CancelAfterFirstReadURLRequestDelegate() override {}

  void OnResponseStarted(net::URLRequest* request) override {
    // net::TestDelegate will start the first read.
    TestDelegate::OnResponseStarted(request);
    request->Cancel();
  }

  void OnReadCompleted(net::URLRequest* request, int bytes_read) override {
    // Read should have been cancelled.
    EXPECT_EQ(-1, bytes_read);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CancelAfterFirstReadURLRequestDelegate);
};

class UrlDataManagerBackendTest : public testing::Test {
 public:
  UrlDataManagerBackendTest() {
    // URLRequestJobFactory takes ownership of the passed in ProtocolHandler.
    url_request_job_factory_.SetProtocolHandler(
        "chrome",
        URLDataManagerBackend::CreateProtocolHandler(
            &resource_context_, false, nullptr, nullptr));
    url_request_context_.set_job_factory(&url_request_job_factory_);
  }

  scoped_ptr<net::URLRequest> CreateRequest(net::URLRequest::Delegate* delegate,
                                            const char* origin) {
    scoped_ptr<net::URLRequest> request = url_request_context_.CreateRequest(
        GURL("chrome://resources/polymer/v1_0/polymer/polymer-extracted.js"),
        net::HIGHEST, delegate);
    request->SetExtraRequestHeaderByName("Origin", origin, true);
    return request.Pass();
  }

 protected:
  TestBrowserThreadBundle thread_bundle_;
  MockResourceContext resource_context_;
  net::URLRequestJobFactoryImpl url_request_job_factory_;
  net::URLRequestContext url_request_context_;
  net::TestDelegate delegate_;
};

TEST_F(UrlDataManagerBackendTest, AccessControlAllowOriginChromeUrl) {
  scoped_ptr<net::URLRequest> request(
      CreateRequest(&delegate_, "chrome://webui"));
  request->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request->response_headers()->HasHeaderValue(
      "Access-Control-Allow-Origin", "chrome://webui"));
}

TEST_F(UrlDataManagerBackendTest, AccessControlAllowOriginNonChromeUrl) {
  scoped_ptr<net::URLRequest> request(
      CreateRequest(&delegate_, "http://www.example.com"));
  request->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request->response_headers()->HasHeaderValue(
      "Access-Control-Allow-Origin", "null"));
}

// Check that the URLRequest isn't passed headers after cancellation.
TEST_F(UrlDataManagerBackendTest, CancelBeforeResponseStarts) {
  scoped_ptr<net::URLRequest> request(
      CreateRequest(&delegate_, "chrome://webui"));
  request->Start();
  request->Cancel();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::URLRequestStatus::CANCELED, request->status().status());
  EXPECT_EQ(1, delegate_.response_started_count());
}

// Check that the URLRequest isn't passed data after cancellation.
TEST_F(UrlDataManagerBackendTest, CancelAfterFirstReadStarted) {
  CancelAfterFirstReadURLRequestDelegate cancel_delegate;
  scoped_ptr<net::URLRequest> request(
      CreateRequest(&cancel_delegate, "chrome://webui"));
  request->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::URLRequestStatus::CANCELED, request->status().status());
  EXPECT_EQ(1, cancel_delegate.response_started_count());
  EXPECT_EQ("", cancel_delegate.data_received());
}

}  // namespace content
