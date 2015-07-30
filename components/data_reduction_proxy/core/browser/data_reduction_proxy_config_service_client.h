// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_SERVICE_CLIENT_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_SERVICE_CLIENT_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/backoff_entry.h"
#include "net/base/network_change_notifier.h"
#include "net/log/net_log.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace base {
class CommandLine;
}

namespace net {
class URLFetcher;
class URLRequestContextGetter;
class URLRequestStatus;
}

namespace data_reduction_proxy {

class ClientConfig;
class DataReductionProxyConfig;
class DataReductionProxyEventCreator;
class DataReductionProxyMutableConfigValues;
class DataReductionProxyParams;
class DataReductionProxyRequestOptions;
class DataReductionProxyService;

// Retrieves the default net::BackoffEntry::Policy for the Data Reduction Proxy
// configuration service client.
const net::BackoffEntry::Policy& GetBackoffPolicy();

// Retrieves the Data Reduction Proxy configuration from a remote service. This
// object lives on the IO thread.
// TODO(jeremyim): Rename the class to DataReductionProxyConfigGetter(?).
class DataReductionProxyConfigServiceClient
    : public net::NetworkChangeNotifier::IPAddressObserver,
      public net::URLFetcherDelegate {
 public:
  // Returns the URL from which the Data Reduction Proxy configuration should
  // be retrieved.
  static GURL GetConfigServiceURL(const base::CommandLine& command_line);

  // The caller must ensure that all parameters remain alive for the lifetime of
  // the |DataReductionProxyConfigClient|, with the exception of |params|
  // which this instance will own.
  DataReductionProxyConfigServiceClient(
      scoped_ptr<DataReductionProxyParams> params,
      const net::BackoffEntry::Policy& backoff_policy,
      DataReductionProxyRequestOptions* request_options,
      DataReductionProxyMutableConfigValues* config_values,
      DataReductionProxyConfig* config,
      DataReductionProxyEventCreator* event_creator,
      net::NetLog* net_log);

  ~DataReductionProxyConfigServiceClient() override;

  // Performs initialization on the IO thread.
  void InitializeOnIOThread(
      net::URLRequestContextGetter* url_request_context_getter);

  // Request the retrieval of the Data Reduction Proxy configuration. This
  // operation takes place asynchronously.
  void RetrieveConfig();

 protected:
  // Retrieves the backoff entry object being used to throttle request failures.
  // Virtual for testing.
  virtual net::BackoffEntry* GetBackoffEntry();

  // Returns the current time.
  // Virtual for testing.
  virtual base::Time Now();

  // Sets a timer to determine when to next refresh the Data Reduction Proxy
  // configuration.
  void SetConfigRefreshTimer(const base::TimeDelta& delay);

  // Constructs a synthetic response based on |params_|.
  std::string ConstructStaticResponse() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigServiceClientTest,
                           TestConstructStaticResponse);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigServiceClientTest,
                           OnIPAddressChange);
  friend class TestDataReductionProxyConfigServiceClient;

  // Override of net::NetworkChangeNotifier::IPAddressObserver.
  void OnIPAddressChanged() override;

  // Override of net::URLFetcherDelegate.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Retrieves the Data Reduction Proxy configuration from |params_|.
  void ReadAndApplyStaticConfig();

  // Retrieves the Data Reduction Proxy configuration from a remote service.
  void RetrieveRemoteConfig();

  // Returns a fetcher to retrieve the Data Reduction Proxy configuration.
  // |secure_proxy_check_url| is the url from which to retrieve the config.
  // |request_body| is the request body sent to the configuration service.
  scoped_ptr<net::URLFetcher> GetURLFetcherForConfig(
      const GURL& secure_proxy_check_url,
      const std::string& request_body);

  // Handles the response from the remote Data Reduction Proxy configuration
  // service. |response| is the response body, |status| is the
  // |net::URLRequestStatus| of the response, and response_code is the HTTP
  // response code (if available).
  void HandleResponse(const std::string& response,
                      const net::URLRequestStatus& status,
                      int response_code);

  // Parses out the proxy configuration portion of |config| and applies it to
  // |config_| and |request_options_|.
  bool ParseAndApplyProxyConfig(const ClientConfig& config);

  // Contains the static configuration data to use.
  scoped_ptr<DataReductionProxyParams> params_;

  // The caller must ensure that the |request_options_| outlives this instance.
  DataReductionProxyRequestOptions* request_options_;

  // The caller must ensure that the |config_values_| outlives this instance.
  DataReductionProxyMutableConfigValues* config_values_;

  // The caller must ensure that the |config_| outlives this instance.
  DataReductionProxyConfig* config_;

  // The caller must ensure that the |event_creator_| outlives this instance.
  DataReductionProxyEventCreator* event_creator_;

  // The caller must ensure that the |net_log_| outlives this instance.
  net::NetLog* net_log_;

  // Used to calculate the backoff time on request failures.
  net::BackoffEntry backoff_entry_;

  // The URL for retrieving the Data Reduction Proxy configuration.
  GURL config_service_url_;

  // Whether to use |params_| to obtain the Data Reduction Proxy configuration
  // or the remote server specified by |config_service_url_|.
  // TODO(jeremyim): Remove this as part of bug 479282.
  bool use_local_config_;

  // Used for setting up |fetcher_|.
  net::URLRequestContextGetter* url_request_context_getter_;

  // An event that fires when it is time to refresh the Data Reduction Proxy
  // configuration.
  base::OneShotTimer<DataReductionProxyConfigServiceClient>
      config_refresh_timer_;

  // A |net::URLFetcher| to retrieve the Data Reduction Proxy configuration.
  scoped_ptr<net::URLFetcher> fetcher_;

  // Used to correlate the start and end of requests.
  net::BoundNetLog bound_net_log_;

  // Used to determine the latency in retrieving the Data Reduction Proxy
  // configuration.
  base::Time config_fetch_start_time_;

  // Enforce usage on the IO thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyConfigServiceClient);
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_SERVICE_CLIENT_H_
