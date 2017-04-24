// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/core/common/lofi_decider.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator_test_util.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_retry_info.h"
#include "net/proxy/proxy_server.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/socket_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace data_reduction_proxy {
namespace {

using TestNetworkDelegate = net::NetworkDelegateImpl;

const char kOtherProxy[] = "testproxy:17";

const char kTestURL[] = "http://www.google.com/";

#if defined(OS_ANDROID)
const Client kClient = Client::CHROME_ANDROID;
#elif defined(OS_IOS)
const Client kClient = Client::CHROME_IOS;
#elif defined(OS_MACOSX)
const Client kClient = Client::CHROME_MAC;
#elif defined(OS_CHROMEOS)
const Client kClient = Client::CHROME_CHROMEOS;
#elif defined(OS_LINUX)
const Client kClient = Client::CHROME_LINUX;
#elif defined(OS_WIN)
const Client kClient = Client::CHROME_WINDOWS;
#elif defined(OS_FREEBSD)
const Client kClient = Client::CHROME_FREEBSD;
#elif defined(OS_OPENBSD)
const Client kClient = Client::CHROME_OPENBSD;
#elif defined(OS_SOLARIS)
const Client kClient = Client::CHROME_SOLARIS;
#elif defined(OS_QNX)
const Client kClient = Client::CHROME_QNX;
#else
const Client kClient = Client::UNKNOWN;
#endif

class TestLoFiDecider : public LoFiDecider {
 public:
  TestLoFiDecider()
      : should_request_lofi_resource_(false),
        ignore_is_using_data_reduction_proxy_check_(false) {}
  ~TestLoFiDecider() override {}

  bool IsUsingLoFi(const net::URLRequest& request) const override {
    return should_request_lofi_resource_;
  }

  void SetIsUsingLoFi(bool should_request_lofi_resource) {
    should_request_lofi_resource_ = should_request_lofi_resource;
  }

  void MaybeSetAcceptTransformHeader(
      const net::URLRequest& request,
      bool is_previews_disabled,
      net::HttpRequestHeaders* headers) const override {
    if (should_request_lofi_resource_) {
      headers->SetHeader(chrome_proxy_accept_transform_header(),
                         empty_image_directive());
    }
  }

  bool IsSlowPagePreviewRequested(
      const net::HttpRequestHeaders& headers) const override {
    std::string header_value;
    if (headers.GetHeader(chrome_proxy_accept_transform_header(),
                          &header_value)) {
      return header_value == empty_image_directive();
    }
    return false;
  }

  bool IsLitePagePreviewRequested(
      const net::HttpRequestHeaders& headers) const override {
    std::string header_value;
    if (headers.GetHeader(chrome_proxy_accept_transform_header(),
                          &header_value)) {
      return header_value == lite_page_directive();
    }
    return false;
  }

  void RemoveAcceptTransformHeader(
      net::HttpRequestHeaders* headers) const override {
    if (ignore_is_using_data_reduction_proxy_check_)
      return;
    headers->RemoveHeader(chrome_proxy_accept_transform_header());
  }

  void MaybeSetIgnorePreviewsBlacklistDirective(
      net::HttpRequestHeaders* headers) const override {}

  bool ShouldRecordLoFiUMA(const net::URLRequest& request) const override {
    return should_request_lofi_resource_;
  }

  void ignore_is_using_data_reduction_proxy_check() {
    ignore_is_using_data_reduction_proxy_check_ = true;
  }

 private:
  bool should_request_lofi_resource_;
  bool ignore_is_using_data_reduction_proxy_check_;
};

class TestLoFiUIService : public LoFiUIService {
 public:
  TestLoFiUIService() : on_lofi_response_(false) {}
  ~TestLoFiUIService() override {}

  bool DidNotifyLoFiResponse() const { return on_lofi_response_; }

  void OnLoFiReponseReceived(const net::URLRequest& request) override {
    on_lofi_response_ = true;
  }

 private:
  bool on_lofi_response_;
};

class DataReductionProxyNetworkDelegateTest : public testing::Test {
 public:
  DataReductionProxyNetworkDelegateTest()
      : context_(true), context_storage_(&context_) {}

  void Init(bool use_secure_proxy, bool enable_brotli_globally) {
    net::ProxyServer proxy_server =
        use_secure_proxy
            ? net::ProxyServer::FromURI("https://origin.net:443",
                                        net::ProxyServer::SCHEME_HTTPS)
            : net::ProxyServer::FromURI("http://origin.net:80",
                                        net::ProxyServer::SCHEME_HTTP);

    proxy_service_ =
        net::ProxyService::CreateFixedFromPacResult(proxy_server.ToPacString());
    context_.set_proxy_service(proxy_service_.get());
    test_context_ = (DataReductionProxyTestContext::Builder()
                         .WithClient(kClient)
                         .WithMockClientSocketFactory(&mock_socket_factory_)
                         .WithURLRequestContext(&context_)
                         .WithProxiesForHttp({DataReductionProxyServer(
                             proxy_server, ProxyServer::UNSPECIFIED_TYPE)})
                         .Build());

    context_.set_client_socket_factory(&mock_socket_factory_);
    test_context_->AttachToURLRequestContext(&context_storage_);

    std::unique_ptr<TestLoFiDecider> lofi_decider(new TestLoFiDecider());
    lofi_decider_ = lofi_decider.get();
    test_context_->io_data()->set_lofi_decider(std::move(lofi_decider));

    std::unique_ptr<TestLoFiUIService> lofi_ui_service(new TestLoFiUIService());
    lofi_ui_service_ = lofi_ui_service.get();
    test_context_->io_data()->set_lofi_ui_service(std::move(lofi_ui_service));

    context_.set_enable_brotli(enable_brotli_globally);
    context_.Init();

    test_context_->EnableDataReductionProxyWithSecureProxyCheckSuccess();
  }

  static void VerifyHeaders(bool expected_data_reduction_proxy_used,
                            bool expected_lofi_used,
                            const net::HttpRequestHeaders& headers) {
    EXPECT_EQ(expected_data_reduction_proxy_used,
              headers.HasHeader(chrome_proxy_header()));
    std::string header_value;
    headers.GetHeader(chrome_proxy_accept_transform_header(), &header_value);
    EXPECT_EQ(expected_data_reduction_proxy_used && expected_lofi_used,
              header_value.find("empty-image") != std::string::npos);
  }

  void VerifyDidNotifyLoFiResponse(bool lofi_response) const {
    EXPECT_EQ(lofi_response, lofi_ui_service_->DidNotifyLoFiResponse());
  }

  void VerifyDataReductionProxyData(const net::URLRequest& request,
                                    bool data_reduction_proxy_used,
                                    bool lofi_used) {
    DataReductionProxyData* data = DataReductionProxyData::GetData(request);
    if (!data_reduction_proxy_used) {
      EXPECT_FALSE(data);
    } else {
      EXPECT_TRUE(data->used_data_reduction_proxy());
      EXPECT_EQ(lofi_used, data->lofi_requested());
    }
  }

  // Each line in |response_headers| should end with "\r\n" and not '\0', and
  // the last line should have a second "\r\n".
  // An empty |response_headers| is allowed. It works by making this look like
  // an HTTP/0.9 response, since HTTP/0.9 responses don't have headers.
  std::unique_ptr<net::URLRequest> FetchURLRequest(
      const GURL& url,
      net::HttpRequestHeaders* request_headers,
      const std::string& response_headers,
      int64_t response_content_length) {
    const std::string response_body(
        base::checked_cast<size_t>(response_content_length), ' ');

    net::MockRead reads[] = {net::MockRead(response_headers.c_str()),
                             net::MockRead(response_body.c_str()),
                             net::MockRead(net::SYNCHRONOUS, net::OK)};
    net::StaticSocketDataProvider socket(reads, arraysize(reads), nullptr, 0);
    mock_socket_factory_.AddSocketDataProvider(&socket);

    net::TestDelegate delegate;
    std::unique_ptr<net::URLRequest> request =
        context_.CreateRequest(url, net::IDLE, &delegate);
    if (request_headers)
      request->SetExtraRequestHeaders(*request_headers);

    request->Start();
    base::RunLoop().RunUntilIdle();
    return request;
  }

  // Fetches a single URL request, verifies the correctness of Accept-Encoding
  // header, and verifies that the response is cached only if |expect_cached|
  // is set to true. Each line in |response_headers| should end with "\r\n" and
  // not '\0', and the last line should have a second "\r\n". An empty
  // |response_headers| is allowed. It works by making this look like an
  // HTTP/0.9 response, since HTTP/0.9 responses don't have headers.
  void FetchURLRequestAndVerifyBrotli(net::HttpRequestHeaders* request_headers,
                                      const std::string& response_headers,
                                      bool expect_cached,
                                      bool expect_brotli) {
    GURL url(kTestURL);
    net::SSLSocketDataProvider ssl_socket_data_provider(net::ASYNC, net::OK);

    int response_body_size = 140;
    const std::string response_body(
        base::checked_cast<size_t>(response_body_size), ' ');

    ssl_socket_data_provider.next_proto = net::kProtoHTTP11;
    ssl_socket_data_provider.cert = net::ImportCertFromFile(
        net::GetTestCertsDirectory(), "unittest.selfsigned.der");
    mock_socket_factory_.AddSSLSocketDataProvider(&ssl_socket_data_provider);

    net::MockRead reads[] = {net::MockRead(response_headers.c_str()),
                             net::MockRead(response_body.c_str()),
                             net::MockRead(net::SYNCHRONOUS, net::OK)};

    if (io_data()->test_request_options()->GetHeaderValueForTesting().empty()) {
      // Force regeneration of Chrome-Proxy header.
      io_data()->test_request_options()->SetSecureSession("123");
    }
    EXPECT_FALSE(
        io_data()->test_request_options()->GetHeaderValueForTesting().empty());

    std::string host = GURL(kTestURL).host();
    std::string prefix_headers = std::string("GET ")
                                     .append(kTestURL)
                                     .append(
                                         " HTTP/1.1\r\n"
                                         "Host: ")
                                     .append(host)
                                     .append(
                                         "\r\n"
                                         "Proxy-Connection: keep-alive\r\n"
                                         "User-Agent:\r\n");

    std::string accept_language_header("Accept-Language: en-us,fr\r\n");

    // Brotli is included in accept-encoding header only if the request went
    // to the network (i.e., it was not a cached response), and if data
    // reduction ptroxy network delegate added Brotli to the header.
    std::string accept_encoding_header =
        expect_brotli && !expect_cached
            ? "Accept-Encoding: gzip, deflate, br\r\n"
            : "Accept-Encoding: gzip, deflate\r\n";

    std::string suffix_headers =
        std::string("Chrome-Proxy: ") +
        io_data()->test_request_options()->GetHeaderValueForTesting() +
        std::string("\r\n\r\n");

    std::string mock_write = prefix_headers + accept_language_header +
                             accept_encoding_header + suffix_headers;

    if (expect_cached || !expect_brotli) {
      // Order of headers is different if the headers were modified by data
      // reduction proxy network delegate.
      mock_write = prefix_headers + accept_encoding_header +
                   accept_language_header + suffix_headers;
    }

    net::MockWrite writes[] = {net::MockWrite(mock_write.c_str())};
    net::StaticSocketDataProvider socket(reads, arraysize(reads), writes,
                                         arraysize(writes));
    mock_socket_factory_.AddSocketDataProvider(&socket);

    net::TestDelegate delegate;
    std::unique_ptr<net::URLRequest> request =
        context_.CreateRequest(url, net::IDLE, &delegate);
    if (request_headers)
      request->SetExtraRequestHeaders(*request_headers);

    request->Start();
    base::RunLoop().RunUntilIdle();

    if (!expect_cached) {
      EXPECT_EQ(response_body_size,
                request->received_response_content_length());
      EXPECT_NE(0, request->GetTotalSentBytes());
      EXPECT_NE(0, request->GetTotalReceivedBytes());
      EXPECT_FALSE(request->was_cached());
      VerifyBrotliPresent(request.get(), expect_brotli);
    } else {
      EXPECT_TRUE(request->was_cached());
    }
  }

  void VerifyBrotliPresent(net::URLRequest* request, bool expect_brotli) {
    net::HttpRequestHeaders request_headers_sent;
    EXPECT_TRUE(request->GetFullRequestHeaders(&request_headers_sent));
    std::string accept_encoding_value;
    EXPECT_TRUE(request_headers_sent.GetHeader("Accept-Encoding",
                                               &accept_encoding_value));
    EXPECT_NE(std::string::npos, accept_encoding_value.find("gzip"));
    if (expect_brotli) {
      // Brotli should be the last entry in the Accept-Encoding header.
      EXPECT_EQ(accept_encoding_value.length() - 2,
                accept_encoding_value.find("br"));
    } else {
      EXPECT_EQ(std::string::npos, accept_encoding_value.find("br"));
    }
  }

  void DelegateStageDone(int result) {}

  void NotifyNetworkDelegate(net::URLRequest* request,
                             const net::ProxyInfo& data_reduction_proxy_info,
                             const net::ProxyRetryInfoMap& proxy_retry_info,
                             net::HttpRequestHeaders* headers) {
    network_delegate()->NotifyBeforeURLRequest(
        request,
        base::Bind(&DataReductionProxyNetworkDelegateTest::DelegateStageDone,
                   base::Unretained(this)),
        nullptr);
    network_delegate()->NotifyBeforeStartTransaction(
        request,
        base::Bind(&DataReductionProxyNetworkDelegateTest::DelegateStageDone,
                   base::Unretained(this)),
        headers);
    network_delegate()->NotifyBeforeSendHeaders(
        request, data_reduction_proxy_info, proxy_retry_info, headers);
  }

  net::MockClientSocketFactory* mock_socket_factory() {
    return &mock_socket_factory_;
  }

  net::TestURLRequestContext* context() { return &context_; }

  net::NetworkDelegate* network_delegate() const {
    return context_.network_delegate();
  }

  TestDataReductionProxyParams* params() const {
    return test_context_->config()->test_params();
  }

  TestDataReductionProxyConfig* config() const {
    return test_context_->config();
  }

  TestDataReductionProxyIOData* io_data() const {
    return test_context_->io_data();
  }

  TestLoFiDecider* lofi_decider() const { return lofi_decider_; }

 private:
  base::MessageLoopForIO message_loop_;
  net::MockClientSocketFactory mock_socket_factory_;
  std::unique_ptr<net::ProxyService> proxy_service_;
  net::TestURLRequestContext context_;
  net::URLRequestContextStorage context_storage_;

  TestLoFiDecider* lofi_decider_;
  TestLoFiUIService* lofi_ui_service_;
  std::unique_ptr<DataReductionProxyTestContext> test_context_;
};

TEST_F(DataReductionProxyNetworkDelegateTest, AuthenticationTest) {
  Init(false, false);
  std::unique_ptr<net::URLRequest> fake_request(
      FetchURLRequest(GURL(kTestURL), nullptr, std::string(), 0));

  net::ProxyInfo data_reduction_proxy_info;
  net::ProxyRetryInfoMap proxy_retry_info;
  std::string data_reduction_proxy;
  base::TrimString(params()->DefaultOrigin(), "/", &data_reduction_proxy);
  data_reduction_proxy_info.UseNamedProxy(data_reduction_proxy);

  net::HttpRequestHeaders headers;
  network_delegate()->NotifyBeforeSendHeaders(fake_request.get(),
                                              data_reduction_proxy_info,
                                              proxy_retry_info, &headers);

  EXPECT_TRUE(headers.HasHeader(chrome_proxy_header()));
  std::string header_value;
  headers.GetHeader(chrome_proxy_header(), &header_value);
  EXPECT_TRUE(header_value.find("ps=") != std::string::npos);
  EXPECT_TRUE(header_value.find("sid=") != std::string::npos);
}

TEST_F(DataReductionProxyNetworkDelegateTest, LoFiTransitions) {
  Init(false, false);
  // Enable Lo-Fi.
  const struct {
    bool lofi_switch_enabled;
    bool auto_lofi_enabled;
    bool is_data_reduction_proxy;
  } tests[] = {
      {
          // Lo-Fi enabled through switch and not using a Data Reduction Proxy.
          true, false, false,
      },
      {
          // Lo-Fi enabled through switch and using a Data Reduction Proxy.
          true, false, true,
      },
      {
          // Lo-Fi enabled through field trial and not using a Data Reduction
          // Proxy.
          false, true, false,
      },
      {
          // Lo-Fi enabled through field trial and using a Data Reduction Proxy.
          false, true, true,
      },
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    if (tests[i].lofi_switch_enabled) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyLoFi,
          switches::kDataReductionProxyLoFiValueAlwaysOn);
    }
    base::FieldTrialList field_trial_list(nullptr);
    if (tests[i].auto_lofi_enabled) {
      base::FieldTrialList::CreateFieldTrial(params::GetLoFiFieldTrialName(),
                                             "Enabled");
    }
    config()->SetNetworkProhibitivelySlow(tests[i].auto_lofi_enabled);
    io_data()->SetLoFiModeActiveOnMainFrame(false);

    net::ProxyInfo data_reduction_proxy_info;
    std::string proxy;
    if (tests[i].is_data_reduction_proxy)
      base::TrimString(params()->DefaultOrigin(), "/", &proxy);
    else
      base::TrimString(kOtherProxy, "/", &proxy);
    data_reduction_proxy_info.UseNamedProxy(proxy);

    {
      // Main frame loaded. Lo-Fi should be used.
      net::HttpRequestHeaders headers;
      net::ProxyRetryInfoMap proxy_retry_info;

      net::TestDelegate delegate;
      std::unique_ptr<net::URLRequest> fake_request =
          context()->CreateRequest(GURL(kTestURL), net::IDLE, &delegate);
      fake_request->SetLoadFlags(net::LOAD_MAIN_FRAME_DEPRECATED);
      lofi_decider()->SetIsUsingLoFi(
          config()->ShouldEnableLoFi(*fake_request.get()));
      NotifyNetworkDelegate(fake_request.get(), data_reduction_proxy_info,
                            proxy_retry_info, &headers);

      VerifyHeaders(tests[i].is_data_reduction_proxy, true, headers);
      VerifyDataReductionProxyData(
          *fake_request, tests[i].is_data_reduction_proxy,
          config()->ShouldEnableLoFi(*fake_request.get()));
    }

    {
      // Lo-Fi is already off. Lo-Fi should not be used.
      net::HttpRequestHeaders headers;
      net::ProxyRetryInfoMap proxy_retry_info;
      net::TestDelegate delegate;
      std::unique_ptr<net::URLRequest> fake_request =
          context()->CreateRequest(GURL(kTestURL), net::IDLE, &delegate);
      lofi_decider()->SetIsUsingLoFi(false);
      NotifyNetworkDelegate(fake_request.get(), data_reduction_proxy_info,
                            proxy_retry_info, &headers);
      VerifyHeaders(tests[i].is_data_reduction_proxy, false, headers);
      VerifyDataReductionProxyData(*fake_request,
                                   tests[i].is_data_reduction_proxy, false);
    }

    {
      // Lo-Fi is already on. Lo-Fi should be used.
      net::HttpRequestHeaders headers;
      net::ProxyRetryInfoMap proxy_retry_info;
      net::TestDelegate delegate;
      std::unique_ptr<net::URLRequest> fake_request =
          context()->CreateRequest(GURL(kTestURL), net::IDLE, &delegate);

      lofi_decider()->SetIsUsingLoFi(true);
      NotifyNetworkDelegate(fake_request.get(), data_reduction_proxy_info,
                            proxy_retry_info, &headers);
      VerifyHeaders(tests[i].is_data_reduction_proxy, true, headers);
      VerifyDataReductionProxyData(*fake_request,
                                   tests[i].is_data_reduction_proxy, true);
    }

    {
      // Main frame request with Lo-Fi off. Lo-Fi should not be used.
      // State of Lo-Fi should persist until next page load.
      net::HttpRequestHeaders headers;
      net::ProxyRetryInfoMap proxy_retry_info;
      net::TestDelegate delegate;
      std::unique_ptr<net::URLRequest> fake_request =
          context()->CreateRequest(GURL(kTestURL), net::IDLE, &delegate);
      fake_request->SetLoadFlags(net::LOAD_MAIN_FRAME_DEPRECATED);
      lofi_decider()->SetIsUsingLoFi(false);
      NotifyNetworkDelegate(fake_request.get(), data_reduction_proxy_info,
                            proxy_retry_info, &headers);
      VerifyHeaders(tests[i].is_data_reduction_proxy, false, headers);
      VerifyDataReductionProxyData(*fake_request,
                                   tests[i].is_data_reduction_proxy, false);
    }

    {
      // Lo-Fi is off. Lo-Fi is still not used.
      net::HttpRequestHeaders headers;
      net::ProxyRetryInfoMap proxy_retry_info;
      net::TestDelegate delegate;
      std::unique_ptr<net::URLRequest> fake_request =
          context()->CreateRequest(GURL(kTestURL), net::IDLE, &delegate);
      lofi_decider()->SetIsUsingLoFi(false);
      NotifyNetworkDelegate(fake_request.get(), data_reduction_proxy_info,
                            proxy_retry_info, &headers);
      VerifyHeaders(tests[i].is_data_reduction_proxy, false, headers);
      VerifyDataReductionProxyData(*fake_request,
                                   tests[i].is_data_reduction_proxy, false);
    }

    {
      // Main frame request. Lo-Fi should be used.
      net::HttpRequestHeaders headers;
      net::ProxyRetryInfoMap proxy_retry_info;
      net::TestDelegate delegate;
      std::unique_ptr<net::URLRequest> fake_request =
          context()->CreateRequest(GURL(kTestURL), net::IDLE, &delegate);
      fake_request->SetLoadFlags(net::LOAD_MAIN_FRAME_DEPRECATED);
      lofi_decider()->SetIsUsingLoFi(
          config()->ShouldEnableLoFi(*fake_request.get()));
      NotifyNetworkDelegate(fake_request.get(), data_reduction_proxy_info,
                            proxy_retry_info, &headers);
      VerifyDataReductionProxyData(
          *fake_request, tests[i].is_data_reduction_proxy,
          config()->ShouldEnableLoFi(*fake_request.get()));
    }
  }
}

TEST_F(DataReductionProxyNetworkDelegateTest, RequestDataConfigurations) {
  Init(false, false);
  const struct {
    bool lofi_on;
    bool used_data_reduction_proxy;
    bool main_frame;
  } tests[] = {
      // Lo-Fi off. Main Frame Request.
      {false, true, true},
      // Data reduction proxy not used. Main Frame Request.
      {false, false, true},
      // Data reduction proxy not used, Lo-Fi should not be used. Main Frame
      // Request.
      {true, false, true},
      // Lo-Fi on. Main Frame Request.
      {true, true, true},
      // Lo-Fi off. Not a Main Frame Request.
      {false, true, false},
      // Data reduction proxy not used. Not a Main Frame Request.
      {false, false, false},
      // Data reduction proxy not used, Lo-Fi should not be used. Not a Main
      // Frame Request.
      {true, false, false},
      // Lo-Fi on. Not a Main Frame Request.
      {true, true, false},
  };

  for (const auto& test : tests) {
    net::ProxyInfo data_reduction_proxy_info;
    std::string data_reduction_proxy;
    base::TrimString(params()->DefaultOrigin(), "/", &data_reduction_proxy);
    if (test.used_data_reduction_proxy)
      data_reduction_proxy_info.UseNamedProxy(data_reduction_proxy);
    else
      data_reduction_proxy_info.UseNamedProxy("port.of.other.proxy");
    // Main frame loaded. Lo-Fi should be used.
    net::HttpRequestHeaders headers;
    net::ProxyRetryInfoMap proxy_retry_info;

    net::TestNetworkQualityEstimator test_network_quality_estimator;
    test_network_quality_estimator.set_effective_connection_type(
        net::EFFECTIVE_CONNECTION_TYPE_OFFLINE);
    context()->set_network_quality_estimator(&test_network_quality_estimator);

    std::unique_ptr<net::URLRequest> request = context()->CreateRequest(
        GURL(kTestURL), net::RequestPriority::IDLE, nullptr);
    request->SetLoadFlags(test.main_frame ? net::LOAD_MAIN_FRAME_DEPRECATED
                                          : 0);
    lofi_decider()->SetIsUsingLoFi(test.lofi_on);
    io_data()->request_options()->SetSecureSession("fake-session");
    network_delegate()->NotifyBeforeSendHeaders(
        request.get(), data_reduction_proxy_info, proxy_retry_info, &headers);
    DataReductionProxyData* data =
        DataReductionProxyData::GetData(*request.get());
    if (!test.used_data_reduction_proxy) {
      EXPECT_FALSE(data);
    } else {
      EXPECT_TRUE(data);
      EXPECT_EQ(test.main_frame ? net::EFFECTIVE_CONNECTION_TYPE_OFFLINE
                                : net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
                data->effective_connection_type());
      EXPECT_TRUE(data->used_data_reduction_proxy());
      EXPECT_EQ(GURL(kTestURL), data->request_url());
      EXPECT_EQ("fake-session", data->session_key());
      EXPECT_EQ(test.lofi_on, data->lofi_requested());
    }
  }
}

TEST_F(DataReductionProxyNetworkDelegateTest,
       RequestDataHoldbackConfigurations) {
  Init(false, false);
  const struct {
    bool data_reduction_proxy_enabled;
    bool used_direct;
  } tests[] = {
      {
          false, true,
      },
      {
          false, false,
      },
      {
          true, false,
      },
      {
          true, true,
      },
  };
  base::FieldTrialList field_trial_list(nullptr);
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "DataCompressionProxyHoldback", "Enabled"));
  for (const auto& test : tests) {
    net::ProxyInfo data_reduction_proxy_info;
    if (test.used_direct)
      data_reduction_proxy_info.UseDirect();
    else
      data_reduction_proxy_info.UseNamedProxy("some.other.proxy");
    config()->UpdateConfigForTesting(test.data_reduction_proxy_enabled, true);
    std::unique_ptr<net::URLRequest> request = context()->CreateRequest(
        GURL(kTestURL), net::RequestPriority::IDLE, nullptr);
    request->set_method("GET");
    net::HttpRequestHeaders headers;
    net::ProxyRetryInfoMap proxy_retry_info;
    network_delegate()->NotifyBeforeSendHeaders(
        request.get(), data_reduction_proxy_info, proxy_retry_info, &headers);
    DataReductionProxyData* data =
        DataReductionProxyData::GetData(*request.get());
    if (!test.data_reduction_proxy_enabled || !test.used_direct) {
      EXPECT_FALSE(data);
    } else {
      EXPECT_TRUE(data);
      EXPECT_TRUE(data->used_data_reduction_proxy());
    }
  }
}

TEST_F(DataReductionProxyNetworkDelegateTest, RedirectRequestDataCleared) {
  Init(false, false);
  net::ProxyInfo data_reduction_proxy_info;
  std::string data_reduction_proxy;
  base::TrimString(params()->DefaultOrigin(), "/", &data_reduction_proxy);
  data_reduction_proxy_info.UseNamedProxy(data_reduction_proxy);

  // Main frame loaded. Lo-Fi should be used.
  net::HttpRequestHeaders headers;
  net::ProxyRetryInfoMap proxy_retry_info;

  net::TestNetworkQualityEstimator test_network_quality_estimator;
  test_network_quality_estimator.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_OFFLINE);
  context()->set_network_quality_estimator(&test_network_quality_estimator);

  std::unique_ptr<net::URLRequest> request = context()->CreateRequest(
      GURL(kTestURL), net::RequestPriority::IDLE, nullptr);
  request->SetLoadFlags(net::LOAD_MAIN_FRAME_DEPRECATED);
  lofi_decider()->SetIsUsingLoFi(true);
  io_data()->request_options()->SetSecureSession("fake-session");
  network_delegate()->NotifyBeforeSendHeaders(
      request.get(), data_reduction_proxy_info, proxy_retry_info, &headers);
  DataReductionProxyData* data =
      DataReductionProxyData::GetData(*request.get());

  EXPECT_TRUE(data);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_OFFLINE,
            data->effective_connection_type());
  EXPECT_TRUE(data->used_data_reduction_proxy());
  EXPECT_EQ(GURL(kTestURL), data->request_url());
  EXPECT_EQ("fake-session", data->session_key());
  EXPECT_TRUE(data->lofi_requested());

  data_reduction_proxy_info.UseNamedProxy("port.of.other.proxy");

  // Simulate a redirect even though the same URL is used. Should clear
  // DataReductionProxyData.
  network_delegate()->NotifyBeforeRedirect(request.get(), GURL(kTestURL));
  data = DataReductionProxyData::GetData(*request.get());
  EXPECT_FALSE(data);

  // Call NotifyBeforeSendHeaders again with different proxy info to check that
  // new data isn't added.
  network_delegate()->NotifyBeforeSendHeaders(
      request.get(), data_reduction_proxy_info, proxy_retry_info, &headers);
  data = DataReductionProxyData::GetData(*request.get());
  EXPECT_FALSE(data);
}

TEST_F(DataReductionProxyNetworkDelegateTest, NetHistograms) {
  Init(false, false);
  const std::string kReceivedValidOCLHistogramName =
      "Net.HttpContentLengthWithValidOCL";
  const std::string kOriginalValidOCLHistogramName =
      "Net.HttpOriginalContentLengthWithValidOCL";
  const std::string kDifferenceValidOCLHistogramName =
      "Net.HttpContentLengthDifferenceWithValidOCL";

  // Lo-Fi histograms.
  const std::string kReceivedValidOCLLoFiOnHistogramName =
      "Net.HttpContentLengthWithValidOCL.LoFiOn";
  const std::string kOriginalValidOCLLoFiOnHistogramName =
      "Net.HttpOriginalContentLengthWithValidOCL.LoFiOn";
  const std::string kDifferenceValidOCLLoFiOnHistogramName =
      "Net.HttpContentLengthDifferenceWithValidOCL.LoFiOn";

  const std::string kReceivedHistogramName = "Net.HttpContentLength";
  const std::string kOriginalHistogramName = "Net.HttpOriginalContentLength";
  const std::string kDifferenceHistogramName =
      "Net.HttpContentLengthDifference";
  const std::string kFreshnessLifetimeHistogramName =
      "Net.HttpContentFreshnessLifetime";
  const std::string kCacheableHistogramName = "Net.HttpContentLengthCacheable";
  const std::string kCacheable4HoursHistogramName =
      "Net.HttpContentLengthCacheable4Hours";
  const std::string kCacheable24HoursHistogramName =
      "Net.HttpContentLengthCacheable24Hours";
  const int64_t kResponseContentLength = 100;
  const int64_t kOriginalContentLength = 200;

  base::HistogramTester histogram_tester;

  std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
      "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "x-original-content-length: " +
      base::Int64ToString(kOriginalContentLength) + "\r\n\r\n";

  std::unique_ptr<net::URLRequest> fake_request(FetchURLRequest(
      GURL(kTestURL), nullptr, response_headers, kResponseContentLength));
  fake_request->SetLoadFlags(fake_request->load_flags() |
                             net::LOAD_MAIN_FRAME_DEPRECATED);

  base::TimeDelta freshness_lifetime =
      fake_request->response_info().headers->GetFreshnessLifetimes(
          fake_request->response_info().response_time).freshness;

  histogram_tester.ExpectUniqueSample(kReceivedValidOCLHistogramName,
                                      kResponseContentLength, 1);
  histogram_tester.ExpectUniqueSample(kOriginalValidOCLHistogramName,
                                      kOriginalContentLength, 1);
  histogram_tester.ExpectUniqueSample(
      kDifferenceValidOCLHistogramName,
      kOriginalContentLength - kResponseContentLength, 1);
  histogram_tester.ExpectUniqueSample(kReceivedHistogramName,
                                      kResponseContentLength, 1);
  histogram_tester.ExpectUniqueSample(kOriginalHistogramName,
                                      kOriginalContentLength, 1);
  histogram_tester.ExpectUniqueSample(
      kDifferenceHistogramName,
      kOriginalContentLength - kResponseContentLength, 1);
  histogram_tester.ExpectUniqueSample(kFreshnessLifetimeHistogramName,
                                      freshness_lifetime.InSeconds(), 1);
  histogram_tester.ExpectUniqueSample(kCacheableHistogramName,
                                      kResponseContentLength, 1);
  histogram_tester.ExpectUniqueSample(kCacheable4HoursHistogramName,
                                      kResponseContentLength, 1);
  histogram_tester.ExpectUniqueSample(kCacheable24HoursHistogramName,
                                      kResponseContentLength, 1);

  // Check Lo-Fi histograms.
  const struct {
    bool lofi_enabled_through_switch;
    bool auto_lofi_enabled;
    int expected_count;
  } tests[] = {
      {
          // Lo-Fi disabled.
          false, false, 0,
      },
      {
          // Auto Lo-Fi enabled.
          // This should populate Lo-Fi content length histogram.
          false, true, 1,
      },
      {
          // Lo-Fi enabled through switch.
          // This should populate Lo-Fi content length histogram.
          true, false, 1,
      },
      {
          // Lo-Fi enabled through switch and Auto Lo-Fi also enabled.
          // This should populate Lo-Fi content length histogram.
          true, true, 1,
      },
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    config()->ResetLoFiStatusForTest();
    config()->SetNetworkProhibitivelySlow(tests[i].auto_lofi_enabled);
    base::FieldTrialList field_trial_list(nullptr);
    if (tests[i].auto_lofi_enabled) {
      base::FieldTrialList::CreateFieldTrial(params::GetLoFiFieldTrialName(),
                                             "Enabled");
    }

    if (tests[i].lofi_enabled_through_switch) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyLoFi,
          switches::kDataReductionProxyLoFiValueAlwaysOn);
    }

    lofi_decider()->SetIsUsingLoFi(
        config()->ShouldEnableLoFi(*fake_request.get()));

    fake_request = (FetchURLRequest(GURL(kTestURL), nullptr, response_headers,
                                    kResponseContentLength));
    fake_request->SetLoadFlags(fake_request->load_flags() |
                               net::LOAD_MAIN_FRAME_DEPRECATED);

    // Histograms are accumulative, so get the sum of all the tests so far.
    int expected_count = 0;
    for (size_t j = 0; j <= i; ++j)
      expected_count += tests[j].expected_count;

    if (expected_count == 0) {
      histogram_tester.ExpectTotalCount(kReceivedValidOCLLoFiOnHistogramName,
                                        expected_count);
      histogram_tester.ExpectTotalCount(kOriginalValidOCLLoFiOnHistogramName,
                                        expected_count);
      histogram_tester.ExpectTotalCount(kDifferenceValidOCLLoFiOnHistogramName,
                                        expected_count);
    } else {
      histogram_tester.ExpectUniqueSample(kReceivedValidOCLLoFiOnHistogramName,
                                          kResponseContentLength,
                                          expected_count);
      histogram_tester.ExpectUniqueSample(kOriginalValidOCLLoFiOnHistogramName,
                                          kOriginalContentLength,
                                          expected_count);
      histogram_tester.ExpectUniqueSample(
          kDifferenceValidOCLLoFiOnHistogramName,
          kOriginalContentLength - kResponseContentLength, expected_count);
    }
  }
}

TEST_F(DataReductionProxyNetworkDelegateTest, OnCompletedInternalLoFi) {
  Init(false, false);
  // Enable Lo-Fi.
  const struct {
    bool lofi_response;
  } tests[] = {
      {false}, {true},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::string response_headers =
        "HTTP/1.1 200 OK\r\n"
        "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
        "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
        "Via: 1.1 Chrome-Compression-Proxy\r\n"
        "x-original-content-length: 200\r\n";

    if (tests[i].lofi_response)
      response_headers += "Chrome-Proxy-Content-Transform: empty-image\r\n";

    response_headers += "\r\n";
    FetchURLRequest(GURL(kTestURL), nullptr, response_headers, 140);

    VerifyDidNotifyLoFiResponse(tests[i].lofi_response);
  }
}

TEST_F(DataReductionProxyNetworkDelegateTest,
       TestLoFiTransformationTypeHistogram) {
  Init(false, false);
  const char kLoFiTransformationTypeHistogram[] =
      "DataReductionProxy.LoFi.TransformationType";
  base::HistogramTester histogram_tester;

  net::HttpRequestHeaders request_headers;
  request_headers.SetHeader("chrome-proxy-accept-transform", "lite-page");
  lofi_decider()->ignore_is_using_data_reduction_proxy_check();
  FetchURLRequest(GURL(kTestURL), &request_headers, std::string(), 140);
  histogram_tester.ExpectBucketCount(kLoFiTransformationTypeHistogram,
                                     NO_TRANSFORMATION_LITE_PAGE_REQUESTED, 1);

  std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Chrome-Proxy-Content-Transform: lite-page\r\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
      "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "x-original-content-length: 200\r\n";

  response_headers += "\r\n";
  FetchURLRequest(GURL(kTestURL), nullptr, response_headers, 140);

  histogram_tester.ExpectBucketCount(kLoFiTransformationTypeHistogram,
                                     LITE_PAGE, 1);
}

// Test that Brotli is not added to the accept-encoding header when it is
// disabled globally.
TEST_F(DataReductionProxyNetworkDelegateTest,
       BrotliAdvertisement_BrotliDisabled) {
  Init(true /* use_secure_proxy */, false /* enable_brotli_globally */);

  std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 140\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "x-original-content-length: 200\r\n"
      "Cache-Control: max-age=1200\r\n"
      "Vary: accept-encoding\r\n";
  response_headers += "\r\n";

  // Use secure sockets when fetching the request since Brotli is only enabled
  // for secure connections.
  FetchURLRequestAndVerifyBrotli(nullptr, response_headers, false, false);
}

// Test that Brotli is not added to the accept-encoding header when the request
// is fetched from an insecure proxy.
TEST_F(DataReductionProxyNetworkDelegateTest,
       BrotliAdvertisementInsecureProxy) {
  Init(false /* use_secure_proxy */, true /* enable_brotli_globally */);
  std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 140\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "x-original-content-length: 200\r\n"
      "Cache-Control: max-age=1200\r\n"
      "Vary: accept-encoding\r\n";
  response_headers += "\r\n";

  // Use secure sockets when fetching the request since Brotli is only enabled
  // for secure connections.
  std::unique_ptr<net::URLRequest> request(
      FetchURLRequest(GURL(kTestURL), nullptr, response_headers, 140));
  EXPECT_EQ(140, request->received_response_content_length());
  EXPECT_NE(0, request->GetTotalSentBytes());
  EXPECT_NE(0, request->GetTotalReceivedBytes());
  EXPECT_FALSE(request->was_cached());
  // Brotli should be added to Accept Encoding header only if secure proxy is in
  VerifyBrotliPresent(request.get(), false);
}

// Test that Brotli is not added to the accept-encoding header when it is
// disabled via data reduction proxy field trial.
TEST_F(DataReductionProxyNetworkDelegateTest,
       BrotliAdvertisementDisabledViaFieldTrial) {
  Init(true /* use_secure_proxy */, true /* enable_brotli_globally */);

  base::FieldTrialList field_trial_list(nullptr);
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "DataReductionProxyBrotliAcceptEncoding", "Disabled"));

  std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 140\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "x-original-content-length: 200\r\n"
      "Cache-Control: max-age=1200\r\n"
      "Vary: accept-encoding\r\n";
  response_headers += "\r\n";

  FetchURLRequestAndVerifyBrotli(nullptr, response_headers, false, false);
  FetchURLRequestAndVerifyBrotli(nullptr, response_headers, true, false);
}

// Test that Brotli is correctly added to the accept-encoding header when it is
// enabled globally.
TEST_F(DataReductionProxyNetworkDelegateTest, BrotliAdvertisement) {
  Init(true /* use_secure_proxy */, true /* enable_brotli_globally */);

  std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 140\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "x-original-content-length: 200\r\n"
      "Cache-Control: max-age=1200\r\n"
      "Vary: accept-encoding\r\n";
  response_headers += "\r\n";

  FetchURLRequestAndVerifyBrotli(nullptr, response_headers, false, true);
  FetchURLRequestAndVerifyBrotli(nullptr, response_headers, true, true);
}

}  // namespace

}  // namespace data_reduction_proxy
