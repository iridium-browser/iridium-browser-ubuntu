// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"

#include "net/http/http_network_session.h"
#include "net/log/net_log.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/next_proto.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Used only to verify that a wrapped network delegate gets called.
class CountingNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  CountingNetworkDelegate() : created_requests_(0) {
  }

  ~CountingNetworkDelegate() final {
  }

  int OnBeforeURLRequest(net::URLRequest* request,
                         const net::CompletionCallback& callback,
                         GURL* new_url) final {
    created_requests_++;
    return net::OK;
  }

  int created_requests() const {
    return created_requests_;
  }

 private:
  int created_requests_;
};

}  // namespace

namespace data_reduction_proxy {

class DataReductionProxyIODataTest : public testing::Test {
 public:
  void SetUp() override {
    RegisterSimpleProfilePrefs(prefs_.registry());
  }

  void RequestCallback(int err) {
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return loop_.task_runner();
  }

  net::TestDelegate* delegate() {
    return &delegate_;
  }

  const net::TestURLRequestContext& context() const {
    return context_;
  }

  net::NetLog* net_log() {
    return &net_log_;
  }

  PrefService* prefs() {
    return &prefs_;
  }

 private:
  base::MessageLoopForIO loop_;
  net::TestDelegate delegate_;
  net::TestURLRequestContext context_;
  net::NetLog net_log_;
  TestingPrefServiceSimple prefs_;
};

TEST_F(DataReductionProxyIODataTest, TestConstruction) {
  scoped_ptr<DataReductionProxyIOData> io_data(new DataReductionProxyIOData(
      Client::UNKNOWN, DataReductionProxyParams::kAllowed, net_log(),
      task_runner(), task_runner(), false /* enabled */,
      false /* enable_quic */, std::string() /* user_agent */));

  // Check that the SimpleURLRequestContextGetter uses vanilla HTTP.
  net::URLRequestContext* request_context =
      io_data->basic_url_request_context_getter_.get()->GetURLRequestContext();
  const net::HttpNetworkSession::Params* http_params =
      request_context->GetNetworkSessionParams();
  EXPECT_TRUE(http_params->use_alternative_services);
  EXPECT_FALSE(http_params->enable_quic);
  net::NextProtoVector expected_protos =
      net::NextProtosWithSpdyAndQuic(false, false);
  EXPECT_EQ(expected_protos.size(), http_params->next_protos.size());
  size_t proto_index = 0;
  for (const auto& proto : expected_protos)
    EXPECT_EQ(proto, http_params->next_protos[proto_index++]);

  // Check that io_data creates an interceptor. Such an interceptor is
  // thoroughly tested by DataReductionProxyInterceptoTest.
  scoped_ptr<net::URLRequestInterceptor> interceptor =
      io_data->CreateInterceptor();
  EXPECT_NE(nullptr, interceptor.get());

  // When creating a network delegate, expect that it properly wraps a
  // network delegate. Such a network delegate is thoroughly tested by
  // DataReductionProxyNetworkDelegateTest.
   scoped_ptr<net::URLRequest> fake_request =
       context().CreateRequest(
           GURL("http://www.foo.com/"), net::IDLE, delegate());
  CountingNetworkDelegate* wrapped_network_delegate =
      new CountingNetworkDelegate();
  scoped_ptr<DataReductionProxyNetworkDelegate> network_delegate =
      io_data->CreateNetworkDelegate(
          make_scoped_ptr(wrapped_network_delegate), false);
  network_delegate->NotifyBeforeURLRequest(
      fake_request.get(),
      base::Bind(&DataReductionProxyIODataTest::RequestCallback,
                 base::Unretained(this)), nullptr);
  EXPECT_EQ(1, wrapped_network_delegate->created_requests());
  EXPECT_NE(nullptr, io_data->bypass_stats());

  // Creating a second delegate with bypass statistics tracking should result
  // in usage stats being created.
  io_data->CreateNetworkDelegate(make_scoped_ptr(new CountingNetworkDelegate()),
                                 true);
  EXPECT_NE(nullptr, io_data->bypass_stats());

  io_data->ShutdownOnUIThread();
}

TEST_F(DataReductionProxyIODataTest, TestResetBadProxyListOnDisableDataSaver) {
  net::TestURLRequestContext context(false);
  scoped_ptr<DataReductionProxyTestContext> drp_test_context =
      DataReductionProxyTestContext::Builder()
          .WithParamsFlags(DataReductionProxyParams::kAllowed |
                           DataReductionProxyParams::kFallbackAllowed |
                           DataReductionProxyParams::kPromoAllowed)
          .WithURLRequestContext(&context)
          .WithTestConfigurator()
          .SkipSettingsInitialization()
          .Build();

  drp_test_context->pref_service()->SetBoolean(
      prefs::kDataReductionProxyEnabled, true);
  drp_test_context->InitSettings();
  DataReductionProxyIOData* io_data = drp_test_context->io_data();
  std::vector<net::ProxyServer> proxies;
  proxies.push_back(net::ProxyServer::FromURI("http://foo1.com",
                                              net::ProxyServer::SCHEME_HTTP));
  net::ProxyService* proxy_service =
      io_data->url_request_context_getter_->GetURLRequestContext()
          ->proxy_service();
  net::ProxyInfo proxy_info;
  proxy_info.UseNamedProxy("http://foo2.com");
  net::BoundNetLog bound_net_log;
  const net::ProxyRetryInfoMap& bad_proxy_list =
      proxy_service->proxy_retry_info();

  // Simulate network error to add proxies to the bad proxy list.
  proxy_service->MarkProxiesAsBadUntil(proxy_info, base::TimeDelta::FromDays(1),
                                       proxies, bound_net_log);
  base::RunLoop().RunUntilIdle();

  // Verify that there are 2 proxies in the bad proxies list.
  EXPECT_EQ(2UL, bad_proxy_list.size());

  // Turn Data Saver off.
  drp_test_context->settings()->SetDataReductionProxyEnabled(false);
  base::RunLoop().RunUntilIdle();

  // Verify that bad proxy list is empty.
  EXPECT_EQ(0UL, bad_proxy_list.size());
}

}  // namespace data_reduction_proxy
