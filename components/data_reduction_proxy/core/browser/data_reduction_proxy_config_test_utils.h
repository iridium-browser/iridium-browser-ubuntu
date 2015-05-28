// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_TEST_UTILS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_TEST_UTILS_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "net/base/net_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class NetLog;
}

namespace data_reduction_proxy {

class DataReductionProxyConfigurator;
class DataReductionProxyEventStore;
class DataReductionProxyMutableConfigValues;
class TestDataReductionProxyParams;

// Test version of |DataReductionProxyConfig|, which uses an underlying
// |TestDataReductionProxyParams| to permit overriding of default values
// returning from |DataReductionProxyParams|, as well as exposing methods to
// change the underlying state.
class TestDataReductionProxyConfig : public DataReductionProxyConfig {
 public:
  // Creates a |TestDataReductionProxyConfig| with the provided |params_flags|.
  TestDataReductionProxyConfig(
      int params_flags,
      unsigned int params_definitions,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      net::NetLog* net_log,
      DataReductionProxyConfigurator* configurator,
      DataReductionProxyEventStore* event_store);

  // Creates a |TestDataReductionProxyConfig| with the provided |config_values|.
  // This permits any DataReductionProxyConfigValues to be used (such as
  // DataReductionProxyParams or DataReductionProxyMutableConfigValues).
  TestDataReductionProxyConfig(
      scoped_ptr<DataReductionProxyConfigValues> config_values,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      net::NetLog* net_log,
      DataReductionProxyConfigurator* configurator,
      DataReductionProxyEventStore* event_store);

  ~TestDataReductionProxyConfig() override;

  void GetNetworkList(net::NetworkInterfaceList* interfaces,
                      int policy) override;

  // If true, uses QUIC instead of SPDY to connect to proxies that use TLS.
  void EnableQuic(bool enable);

  // Allows tests to reset the params being used for configuration.
  void ResetParamFlagsForTest(int flags);

  // Retrieves the test params being used for the configuration.
  TestDataReductionProxyParams* test_params();

  // Retrieves the underlying config values.
  // TODO(jeremyim): Rationalize with test_params().
  DataReductionProxyConfigValues* config_values();

  // Allows tests to set the internal state.
  void SetStateForTest(bool enabled_by_user,
                       bool alternative_enabled_by_user,
                       bool restricted_by_carrier);

  net::NetworkInterfaceList* interfaces() {
    return network_interfaces_.get();
  }

 private:
  scoped_ptr<net::NetworkInterfaceList> network_interfaces_;
};

// A |TestDataReductionProxyConfig| which permits mocking of methods for
// testing.
class MockDataReductionProxyConfig : public TestDataReductionProxyConfig {
 public:
  // Creates a |MockDataReductionProxyConfig|.
  MockDataReductionProxyConfig(
      scoped_ptr<DataReductionProxyConfigValues> config_values,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      net::NetLog* net_log,
      DataReductionProxyConfigurator* configurator,
      DataReductionProxyEventStore* event_store);
  ~MockDataReductionProxyConfig();

  MOCK_METHOD1(RecordSecureProxyCheckFetchResult,
               void(SecureProxyCheckFetchResult result));
  MOCK_METHOD3(LogProxyState,
               void(bool enabled, bool restricted, bool at_startup));
  MOCK_METHOD3(SetProxyPrefs,
               void(bool enabled, bool alternative_enabled, bool at_startup));
  MOCK_CONST_METHOD2(IsDataReductionProxy,
                     bool(const net::HostPortPair& host_port_pair,
                          DataReductionProxyTypeInfo* proxy_info));
  MOCK_CONST_METHOD2(WasDataReductionProxyUsed,
                     bool(const net::URLRequest*,
                          DataReductionProxyTypeInfo* proxy_info));
  MOCK_CONST_METHOD1(ContainsDataReductionProxy,
                     bool(const net::ProxyConfig::ProxyRules& proxy_rules));
  MOCK_CONST_METHOD2(IsBypassedByDataReductionProxyLocalRules,
                     bool(const net::URLRequest& request,
                          const net::ProxyConfig& data_reduction_proxy_config));
  MOCK_CONST_METHOD3(AreDataReductionProxiesBypassed,
                     bool(const net::URLRequest& request,
                          const net::ProxyConfig& data_reduction_proxy_config,
                          base::TimeDelta* min_retry_delay));

  // UpdateConfigurator should always call LogProxyState exactly once.
  void UpdateConfigurator(bool enabled,
                          bool alternative_enabled,
                          bool restricted,
                          bool at_startup) override;

  // HandleSecureProxyCheckResponse should always call
  // RecordSecureProxyCheckFetchResult exactly once.
  void HandleSecureProxyCheckResponse(
      const std::string& response,
      const net::URLRequestStatus& status) override;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_TEST_UTILS_H_
