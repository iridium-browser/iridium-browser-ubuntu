// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_http_job.h"

#include <cstddef>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "net/base/auth.h"
#include "net/base/request_priority.h"
#include "net/base/test_data_directory.h"
#include "net/cookies/cookie_store_test_helpers.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_transaction_test_util.h"
#include "net/socket/socket_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "net/websockets/websocket_handshake_stream_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

using ::testing::Return;

// Inherit from URLRequestHttpJob to expose the priority and some
// other hidden functions.
class TestURLRequestHttpJob : public URLRequestHttpJob {
 public:
  explicit TestURLRequestHttpJob(URLRequest* request)
      : URLRequestHttpJob(request, request->context()->network_delegate(),
                          request->context()->http_user_agent_settings()) {}

  using URLRequestHttpJob::SetPriority;
  using URLRequestHttpJob::Start;
  using URLRequestHttpJob::Kill;
  using URLRequestHttpJob::priority;

 protected:
  ~TestURLRequestHttpJob() override {}
};

class URLRequestHttpJobTest : public ::testing::Test {
 protected:
  URLRequestHttpJobTest()
      : req_(context_.CreateRequest(GURL("http://www.example.com"),
                                    DEFAULT_PRIORITY,
                                    &delegate_)) {
    context_.set_http_transaction_factory(&network_layer_);
  }

  bool TransactionAcceptsSdchEncoding() {
    base::WeakPtr<MockNetworkTransaction> transaction(
        network_layer_.last_transaction());
    EXPECT_TRUE(transaction);
    if (!transaction) return false;

    const HttpRequestInfo* request_info = transaction->request();
    EXPECT_TRUE(request_info);
    if (!request_info) return false;

    std::string encoding_headers;
    bool get_success = request_info->extra_headers.GetHeader(
        "Accept-Encoding", &encoding_headers);
    EXPECT_TRUE(get_success);
    if (!get_success) return false;

    // This check isn't wrapped with EXPECT* macros because different
    // results from this function may be expected in different tests.
    for (const std::string& token :
         base::SplitString(encoding_headers, ", ", base::KEEP_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY)) {
      if (base::EqualsCaseInsensitiveASCII(token, "sdch"))
        return true;
    }
    return false;
  }

  void EnableSdch() {
    context_.SetSdchManager(scoped_ptr<SdchManager>(new SdchManager).Pass());
  }

  MockNetworkLayer network_layer_;
  TestURLRequestContext context_;
  TestDelegate delegate_;
  scoped_ptr<URLRequest> req_;
};

class URLRequestHttpJobWithMockSocketsTest : public ::testing::Test {
 protected:
  URLRequestHttpJobWithMockSocketsTest()
      : context_(new TestURLRequestContext(true)) {
    context_->set_client_socket_factory(&socket_factory_);
    context_->set_network_delegate(&network_delegate_);
    context_->set_backoff_manager(&manager_);
    context_->Init();
  }

  MockClientSocketFactory socket_factory_;
  TestNetworkDelegate network_delegate_;
  URLRequestBackoffManager manager_;
  scoped_ptr<TestURLRequestContext> context_;
};

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestContentLengthSuccessfulRequest) {
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};

  StaticSocketDataProvider socket_data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  scoped_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate)
          .Pass();

  request->Start();
  ASSERT_TRUE(request->is_pending());
  base::RunLoop().Run();

  EXPECT_TRUE(request->status().is_success());
  EXPECT_EQ(12, request->received_response_content_length());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest, TestContentLengthAbortedRequest) {
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 20\r\n\r\n"),
                      MockRead("Test Content"),
                      MockRead(net::SYNCHRONOUS, net::ERR_FAILED)};

  StaticSocketDataProvider socket_data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  scoped_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate)
          .Pass();

  request->Start();
  ASSERT_TRUE(request->is_pending());
  base::RunLoop().Run();

  EXPECT_EQ(URLRequestStatus::FAILED, request->status().status());
  EXPECT_EQ(12, request->received_response_content_length());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestContentLengthCancelledRequest) {
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 20\r\n\r\n"),
                      MockRead("Test Content"),
                      MockRead(net::SYNCHRONOUS, net::ERR_IO_PENDING)};

  StaticSocketDataProvider socket_data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  scoped_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate)
          .Pass();

  request->Start();
  base::RunLoop().RunUntilIdle();
  request->Cancel();
  base::RunLoop().Run();

  EXPECT_EQ(URLRequestStatus::CANCELED, request->status().status());
  EXPECT_EQ(12, request->received_response_content_length());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest, BackoffHeader) {
  MockWrite writes[] = {MockWrite(
      "GET / HTTP/1.1\r\n"
      "Host: www.example.com\r\n"
      "Connection: keep-alive\r\n"
      "User-Agent:\r\n"
      "Accept-Encoding: gzip, deflate\r\n"
      "Accept-Language: en-us,fr\r\n\r\n")};

  MockRead reads[] = {MockRead(
                          "HTTP/1.1 200 OK\r\n"
                          "Backoff: 3600\r\n"
                          "Content-Length: 9\r\n\r\n"),
                      MockRead("test.html")};

  net::SSLSocketDataProvider ssl_socket_data_provider(net::ASYNC, net::OK);
  ssl_socket_data_provider.SetNextProto(kProtoHTTP11);
  ssl_socket_data_provider.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "unittest.selfsigned.der");
  socket_factory_.AddSSLSocketDataProvider(&ssl_socket_data_provider);

  StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                       arraysize(writes));
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate1;
  scoped_ptr<URLRequest> request1 =
      context_->CreateRequest(GURL("https://www.example.com"), DEFAULT_PRIORITY,
                              &delegate1).Pass();

  request1->Start();
  ASSERT_TRUE(request1->is_pending());
  base::RunLoop().Run();

  EXPECT_TRUE(request1->status().is_success());
  EXPECT_EQ("test.html", delegate1.data_received());
  EXPECT_EQ(1, delegate1.received_before_network_start_count());
  EXPECT_EQ(1, manager_.GetNumberOfEntriesForTests());

  // Issue another request, and backoff logic should apply.
  TestDelegate delegate2;
  scoped_ptr<URLRequest> request2 =
      context_->CreateRequest(GURL("https://www.example.com"), DEFAULT_PRIORITY,
                              &delegate2).Pass();

  request2->Start();
  ASSERT_TRUE(request2->is_pending());
  base::RunLoop().Run();

  EXPECT_FALSE(request2->status().is_success());
  EXPECT_EQ(ERR_TEMPORARY_BACKOFF, request2->status().error());
  EXPECT_EQ(0, delegate2.received_before_network_start_count());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest, BackoffHeaderNotSecure) {
  MockWrite writes[] = {MockWrite(
      "GET / HTTP/1.1\r\n"
      "Host: www.example.com\r\n"
      "Connection: keep-alive\r\n"
      "User-Agent:\r\n"
      "Accept-Encoding: gzip, deflate\r\n"
      "Accept-Language: en-us,fr\r\n\r\n")};
  MockRead reads[] = {MockRead(
                          "HTTP/1.1 200 OK\r\n"
                          "Backoff: 3600\r\n"
                          "Content-Length: 9\r\n\r\n"),
                      MockRead("test.html")};

  StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                       arraysize(writes));
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  scoped_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate).Pass();

  request->Start();
  ASSERT_TRUE(request->is_pending());
  base::RunLoop().Run();

  EXPECT_TRUE(request->status().is_success());
  EXPECT_EQ("test.html", delegate.data_received());
  EXPECT_EQ(1, delegate.received_before_network_start_count());
  // Backoff logic does not apply to plain HTTP request.
  EXPECT_EQ(0, manager_.GetNumberOfEntriesForTests());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest, BackoffHeaderCachedResponse) {
  MockWrite writes[] = {MockWrite(
      "GET / HTTP/1.1\r\n"
      "Host: www.example.com\r\n"
      "Connection: keep-alive\r\n"
      "User-Agent:\r\n"
      "Accept-Encoding: gzip, deflate\r\n"
      "Accept-Language: en-us,fr\r\n\r\n")};
  MockRead reads[] = {MockRead(
                          "HTTP/1.1 200 OK\r\n"
                          "Backoff: 3600\r\n"
                          "Cache-Control: max-age=120\r\n"
                          "Content-Length: 9\r\n\r\n"),
                      MockRead("test.html")};

  net::SSLSocketDataProvider ssl_socket_data_provider(net::ASYNC, net::OK);
  ssl_socket_data_provider.SetNextProto(kProtoHTTP11);
  ssl_socket_data_provider.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "unittest.selfsigned.der");
  socket_factory_.AddSSLSocketDataProvider(&ssl_socket_data_provider);

  StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                       arraysize(writes));
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate1;
  scoped_ptr<URLRequest> request1 =
      context_->CreateRequest(GURL("https://www.example.com"), DEFAULT_PRIORITY,
                              &delegate1).Pass();

  request1->Start();
  ASSERT_TRUE(request1->is_pending());
  base::RunLoop().Run();

  EXPECT_TRUE(request1->status().is_success());
  EXPECT_EQ("test.html", delegate1.data_received());
  EXPECT_EQ(1, delegate1.received_before_network_start_count());
  EXPECT_EQ(1, manager_.GetNumberOfEntriesForTests());

  // Backoff logic does not apply to a second request, since it is fetched
  // from cache.
  TestDelegate delegate2;
  scoped_ptr<URLRequest> request2 =
      context_->CreateRequest(GURL("https://www.example.com"), DEFAULT_PRIORITY,
                              &delegate2).Pass();

  request2->Start();
  ASSERT_TRUE(request2->is_pending());
  base::RunLoop().Run();
  EXPECT_TRUE(request2->was_cached());
  EXPECT_TRUE(request2->status().is_success());
  EXPECT_EQ(0, delegate2.received_before_network_start_count());
}

TEST_F(URLRequestHttpJobTest, TestCancelWhileReadingCookies) {
  context_.set_cookie_store(new DelayedCookieMonster());

  TestDelegate delegate;
  scoped_ptr<URLRequest> request =
      context_.CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                             &delegate)
          .Pass();

  request->Start();
  request->Cancel();
  base::RunLoop().Run();

  DCHECK_EQ(0, delegate.received_before_network_start_count());
  EXPECT_EQ(URLRequestStatus::CANCELED, request->status().status());
}

// Make sure that SetPriority actually sets the URLRequestHttpJob's
// priority, both before and after start.
TEST_F(URLRequestHttpJobTest, SetPriorityBasic) {
  scoped_refptr<TestURLRequestHttpJob> job(
      new TestURLRequestHttpJob(req_.get()));
  EXPECT_EQ(DEFAULT_PRIORITY, job->priority());

  job->SetPriority(LOWEST);
  EXPECT_EQ(LOWEST, job->priority());

  job->SetPriority(LOW);
  EXPECT_EQ(LOW, job->priority());

  job->Start();
  EXPECT_EQ(LOW, job->priority());

  job->SetPriority(MEDIUM);
  EXPECT_EQ(MEDIUM, job->priority());
}

// Make sure that URLRequestHttpJob passes on its priority to its
// transaction on start.
TEST_F(URLRequestHttpJobTest, SetTransactionPriorityOnStart) {
  scoped_refptr<TestURLRequestHttpJob> job(
      new TestURLRequestHttpJob(req_.get()));
  job->SetPriority(LOW);

  EXPECT_FALSE(network_layer_.last_transaction());

  job->Start();

  ASSERT_TRUE(network_layer_.last_transaction());
  EXPECT_EQ(LOW, network_layer_.last_transaction()->priority());
}

// Make sure that URLRequestHttpJob passes on its priority updates to
// its transaction.
TEST_F(URLRequestHttpJobTest, SetTransactionPriority) {
  scoped_refptr<TestURLRequestHttpJob> job(
      new TestURLRequestHttpJob(req_.get()));
  job->SetPriority(LOW);
  job->Start();
  ASSERT_TRUE(network_layer_.last_transaction());
  EXPECT_EQ(LOW, network_layer_.last_transaction()->priority());

  job->SetPriority(HIGHEST);
  EXPECT_EQ(HIGHEST, network_layer_.last_transaction()->priority());
}

// Confirm we do advertise SDCH encoding in the case of a GET.
TEST_F(URLRequestHttpJobTest, SdchAdvertisementGet) {
  EnableSdch();
  req_->set_method("GET");  // Redundant with default.
  scoped_refptr<TestURLRequestHttpJob> job(
      new TestURLRequestHttpJob(req_.get()));
  job->Start();
  EXPECT_TRUE(TransactionAcceptsSdchEncoding());
}

// Confirm we don't advertise SDCH encoding in the case of a POST.
TEST_F(URLRequestHttpJobTest, SdchAdvertisementPost) {
  EnableSdch();
  req_->set_method("POST");
  scoped_refptr<TestURLRequestHttpJob> job(
      new TestURLRequestHttpJob(req_.get()));
  job->Start();
  EXPECT_FALSE(TransactionAcceptsSdchEncoding());
}

// This base class just serves to set up some things before the TestURLRequest
// constructor is called.
class URLRequestHttpJobWebSocketTestBase : public ::testing::Test {
 protected:
  URLRequestHttpJobWebSocketTestBase() : socket_data_(nullptr, 0, nullptr, 0),
                                         context_(true) {
    // A Network Delegate is required for the WebSocketHandshakeStreamBase
    // object to be passed on to the HttpNetworkTransaction.
    context_.set_network_delegate(&network_delegate_);

    // Attempting to create real ClientSocketHandles is not going to work out so
    // well. Set up a fake socket factory.
    socket_factory_.AddSocketDataProvider(&socket_data_);
    context_.set_client_socket_factory(&socket_factory_);
    context_.Init();
  }

  StaticSocketDataProvider socket_data_;
  TestNetworkDelegate network_delegate_;
  MockClientSocketFactory socket_factory_;
  TestURLRequestContext context_;
};

class URLRequestHttpJobWebSocketTest
    : public URLRequestHttpJobWebSocketTestBase {
 protected:
  URLRequestHttpJobWebSocketTest()
      : req_(context_.CreateRequest(GURL("ws://www.example.com"),
                                    DEFAULT_PRIORITY,
                                    &delegate_)) {
    // The TestNetworkDelegate expects a call to NotifyBeforeURLRequest before
    // anything else happens.
    GURL url("ws://localhost/");
    TestCompletionCallback dummy;
    network_delegate_.NotifyBeforeURLRequest(
        req_.get(), dummy.callback(), &url);
  }

  TestDelegate delegate_;
  scoped_ptr<URLRequest> req_;
};

class MockCreateHelper : public WebSocketHandshakeStreamBase::CreateHelper {
 public:
  // GoogleMock does not appear to play nicely with move-only types like
  // scoped_ptr, so this forwarding method acts as a workaround.
  WebSocketHandshakeStreamBase* CreateBasicStream(
      scoped_ptr<ClientSocketHandle> connection,
      bool using_proxy) override {
    // Discard the arguments since we don't need them anyway.
    return CreateBasicStreamMock();
  }

  MOCK_METHOD0(CreateBasicStreamMock,
               WebSocketHandshakeStreamBase*());

  MOCK_METHOD2(CreateSpdyStream,
               WebSocketHandshakeStreamBase*(const base::WeakPtr<SpdySession>&,
                                             bool));
};

class FakeWebSocketHandshakeStream : public WebSocketHandshakeStreamBase {
 public:
  FakeWebSocketHandshakeStream() : initialize_stream_was_called_(false) {}

  bool initialize_stream_was_called() const {
    return initialize_stream_was_called_;
  }

  // Fake implementation of HttpStreamBase methods.
  int InitializeStream(const HttpRequestInfo* request_info,
                       RequestPriority priority,
                       const BoundNetLog& net_log,
                       const CompletionCallback& callback) override {
    initialize_stream_was_called_ = true;
    return ERR_IO_PENDING;
  }

  int SendRequest(const HttpRequestHeaders& request_headers,
                  HttpResponseInfo* response,
                  const CompletionCallback& callback) override {
    return ERR_IO_PENDING;
  }

  int ReadResponseHeaders(const CompletionCallback& callback) override {
    return ERR_IO_PENDING;
  }

  int ReadResponseBody(IOBuffer* buf,
                       int buf_len,
                       const CompletionCallback& callback) override {
    return ERR_IO_PENDING;
  }

  void Close(bool not_reusable) override {}

  bool IsResponseBodyComplete() const override { return false; }

  bool CanFindEndOfResponse() const override { return false; }

  bool IsConnectionReused() const override { return false; }
  void SetConnectionReused() override {}

  bool IsConnectionReusable() const override { return false; }

  int64 GetTotalReceivedBytes() const override { return 0; }

  bool GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const override {
    return false;
  }

  void GetSSLInfo(SSLInfo* ssl_info) override {}

  void GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info) override {}

  bool IsSpdyHttpStream() const override { return false; }

  void Drain(HttpNetworkSession* session) override {}

  void SetPriority(RequestPriority priority) override {}

  UploadProgress GetUploadProgress() const override {
    return UploadProgress();
  }

  HttpStream* RenewStreamForAuth() override { return nullptr; }

  // Fake implementation of WebSocketHandshakeStreamBase method(s)
  scoped_ptr<WebSocketStream> Upgrade() override {
    return scoped_ptr<WebSocketStream>();
  }

 private:
  bool initialize_stream_was_called_;
};

TEST_F(URLRequestHttpJobWebSocketTest, RejectedWithoutCreateHelper) {
  scoped_refptr<TestURLRequestHttpJob> job(
      new TestURLRequestHttpJob(req_.get()));
  job->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(URLRequestStatus::FAILED, req_->status().status());
  EXPECT_EQ(ERR_DISALLOWED_URL_SCHEME, req_->status().error());
}

TEST_F(URLRequestHttpJobWebSocketTest, CreateHelperPassedThrough) {
  scoped_refptr<TestURLRequestHttpJob> job(
      new TestURLRequestHttpJob(req_.get()));
  scoped_ptr<MockCreateHelper> create_helper(
      new ::testing::StrictMock<MockCreateHelper>());
  FakeWebSocketHandshakeStream* fake_handshake_stream(
      new FakeWebSocketHandshakeStream);
  // Ownership of fake_handshake_stream is transferred when CreateBasicStream()
  // is called.
  EXPECT_CALL(*create_helper, CreateBasicStreamMock())
      .WillOnce(Return(fake_handshake_stream));
  req_->SetUserData(WebSocketHandshakeStreamBase::CreateHelper::DataKey(),
                    create_helper.release());
  req_->SetLoadFlags(LOAD_DISABLE_CACHE);
  job->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(URLRequestStatus::IO_PENDING, req_->status().status());
  EXPECT_TRUE(fake_handshake_stream->initialize_stream_was_called());
}

}  // namespace

}  // namespace net
