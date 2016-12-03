// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/url_loader_factory_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "content/browser/loader/mojo_async_resource_handler.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/test_url_loader_client.h"
#include "content/browser/loader_delegate_impl.h"
#include "content/common/resource_request.h"
#include "content/common/resource_request_completion_status.h"
#include "content/common/url_loader.mojom.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/url_request/url_request_filter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

class RejectingResourceDispatcherHostDelegate final
    : public ResourceDispatcherHostDelegate {
 public:
  RejectingResourceDispatcherHostDelegate() {}
  bool ShouldBeginRequest(const std::string& method,
                          const GURL& url,
                          ResourceType resource_type,
                          ResourceContext* resource_context) override {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(RejectingResourceDispatcherHostDelegate);
};

// The test parameter is the number of bytes allocated for the buffer in the
// data pipe, for testing the case where the allocated size is smaller than the
// size the mime sniffer *implicitly* requires.
class URLLoaderFactoryImplTest : public ::testing::TestWithParam<size_t> {
 public:
  URLLoaderFactoryImplTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        browser_context_(new TestBrowserContext()),
        resource_message_filter_(new ResourceMessageFilter(
            0,
            0,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            base::Bind(&URLLoaderFactoryImplTest::GetContexts,
                       base::Unretained(this)))) {
    MojoAsyncResourceHandler::SetAllocationSizeForTesting(GetParam());
    rdh_.SetLoaderDelegate(&loader_deleate_);

    URLLoaderFactoryImpl::Create(resource_message_filter_,
                                 mojo::GetProxy(&factory_));

    // Calling this function creates a request context.
    browser_context_->GetResourceContext()->GetRequestContext();
    base::RunLoop().RunUntilIdle();
  }

  ~URLLoaderFactoryImplTest() override {
    rdh_.SetDelegate(nullptr);
    net::URLRequestFilter::GetInstance()->ClearHandlers();

    rdh_.CancelRequestsForProcess(resource_message_filter_->child_id());
    base::RunLoop().RunUntilIdle();
    MojoAsyncResourceHandler::SetAllocationSizeForTesting(
        MojoAsyncResourceHandler::kDefaultAllocationSize);
  }

  void GetContexts(ResourceType resource_type,
                   ResourceContext** resource_context,
                   net::URLRequestContext** request_context) {
    *resource_context = browser_context_->GetResourceContext();
    *request_context =
        browser_context_->GetResourceContext()->GetRequestContext();
  }

  TestBrowserThreadBundle thread_bundle_;
  LoaderDelegateImpl loader_deleate_;
  ResourceDispatcherHostImpl rdh_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  scoped_refptr<ResourceMessageFilter> resource_message_filter_;
  mojom::URLLoaderFactoryPtr factory_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryImplTest);
};

TEST_P(URLLoaderFactoryImplTest, GetResponse) {
  mojom::URLLoaderPtr loader;
  base::FilePath root;
  PathService::Get(DIR_TEST_DATA, &root);
  net::URLRequestMockHTTPJob::AddUrlHandlers(root,
                                             BrowserThread::GetBlockingPool());
  ResourceRequest request;
  TestURLLoaderClient client;
  // Assume the file contents is small enough to be stored in the data pipe.
  request.url = net::URLRequestMockHTTPJob::GetMockUrl("hello.html");
  request.method = "GET";
  request.is_main_frame = true;
  factory_->CreateLoaderAndStart(mojo::GetProxy(&loader), 1, request,
                                 client.CreateInterfacePtrAndBind());

  ASSERT_FALSE(client.has_received_response());
  ASSERT_FALSE(client.response_body().is_valid());
  ASSERT_FALSE(client.has_received_completion());

  client.RunUntilResponseReceived();
  ASSERT_FALSE(client.has_received_completion());
  ASSERT_FALSE(client.has_received_completion());

  client.RunUntilResponseBodyArrived();
  ASSERT_TRUE(client.response_body().is_valid());
  ASSERT_FALSE(client.has_received_completion());

  client.RunUntilComplete();

  EXPECT_EQ(200, client.response_head().headers->response_code());
  std::string content_type;
  client.response_head().headers->GetNormalizedHeader("content-type",
                                                      &content_type);
  EXPECT_EQ("text/html", content_type);
  EXPECT_EQ(0, client.completion_status().error_code);

  std::string contents;
  while (true) {
    char buffer[16];
    uint32_t read_size = sizeof(buffer);
    MojoResult r = mojo::ReadDataRaw(client.response_body(), buffer, &read_size,
                                     MOJO_READ_DATA_FLAG_NONE);
    if (r == MOJO_RESULT_FAILED_PRECONDITION)
      break;
    if (r == MOJO_RESULT_SHOULD_WAIT)
      continue;
    ASSERT_EQ(MOJO_RESULT_OK, r);
    contents += std::string(buffer, read_size);
  }
  std::string expected;
  base::ReadFileToString(
      root.Append(base::FilePath(FILE_PATH_LITERAL("hello.html"))), &expected);
  EXPECT_EQ(expected, contents);
}

TEST_P(URLLoaderFactoryImplTest, GetFailedResponse) {
  mojom::URLLoaderPtr loader;
  ResourceRequest request;
  TestURLLoaderClient client;
  net::URLRequestFailedJob::AddUrlHandler();
  request.url = net::URLRequestFailedJob::GetMockHttpUrlWithFailurePhase(
      net::URLRequestFailedJob::START, net::ERR_TIMED_OUT);
  request.method = "GET";
  factory_->CreateLoaderAndStart(mojo::GetProxy(&loader), 1, request,
                                 client.CreateInterfacePtrAndBind());

  client.RunUntilComplete();
  ASSERT_FALSE(client.has_received_response());
  ASSERT_FALSE(client.response_body().is_valid());

  EXPECT_EQ(net::ERR_TIMED_OUT, client.completion_status().error_code);
}

// This test tests a case where resource loading is cancelled before started.
TEST_P(URLLoaderFactoryImplTest, InvalidURL) {
  mojom::URLLoaderPtr loader;
  ResourceRequest request;
  TestURLLoaderClient client;
  request.url = GURL();
  request.method = "GET";
  ASSERT_FALSE(request.url.is_valid());
  factory_->CreateLoaderAndStart(mojo::GetProxy(&loader), 1, request,
                                 client.CreateInterfacePtrAndBind());

  client.RunUntilComplete();
  ASSERT_FALSE(client.has_received_response());
  ASSERT_FALSE(client.response_body().is_valid());

  EXPECT_EQ(net::ERR_ABORTED, client.completion_status().error_code);
}

// This test tests a case where resource loading is cancelled before started.
TEST_P(URLLoaderFactoryImplTest, ShouldNotRequestURL) {
  mojom::URLLoaderPtr loader;
  RejectingResourceDispatcherHostDelegate rdh_delegate;
  rdh_.SetDelegate(&rdh_delegate);
  ResourceRequest request;
  TestURLLoaderClient client;
  request.url = GURL("http://localhost/");
  request.method = "GET";
  factory_->CreateLoaderAndStart(mojo::GetProxy(&loader), 1, request,
                                 client.CreateInterfacePtrAndBind());

  client.RunUntilComplete();
  rdh_.SetDelegate(nullptr);

  ASSERT_FALSE(client.has_received_response());
  ASSERT_FALSE(client.response_body().is_valid());

  EXPECT_EQ(net::ERR_ABORTED, client.completion_status().error_code);
}

INSTANTIATE_TEST_CASE_P(URLLoaderFactoryImplTest,
                        URLLoaderFactoryImplTest,
                        ::testing::Values(128, 32 * 1024));

}  // namespace

}  // namespace content
