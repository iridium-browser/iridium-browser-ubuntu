// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_client_config_parser.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_creator.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/proxy/proxy_server.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

#if defined(USE_GOOGLE_API_KEYS)
#include "google_apis/google_api_keys.h"
#endif

namespace data_reduction_proxy {

namespace {

// Key of the UMA DataReductionProxy.ConfigService.FetchResponseCode histogram.
const char kUMAConfigServiceFetchResponseCode[] =
    "DataReductionProxy.ConfigService.FetchResponseCode";

// Key of the UMA
// DataReductionProxy.ConfigService.FetchFailedAttemptsBeforeSuccess histogram.
const char kUMAConfigServiceFetchFailedAttemptsBeforeSuccess[] =
    "DataReductionProxy.ConfigService.FetchFailedAttemptsBeforeSuccess";

// Key of the UMA DataReductionProxy.ConfigService.FetchLatency histogram.
const char kUMAConfigServiceFetchLatency[] =
    "DataReductionProxy.ConfigService.FetchLatency";

// Key of the UMA DataReductionProxy.ConfigService.AuthExpired histogram.
const char kUMAConfigServiceAuthExpired[] =
    "DataReductionProxy.ConfigService.AuthExpired";

#if defined(USE_GOOGLE_API_KEYS)
// Used in all Data Reduction Proxy URLs to specify API Key.
const char kApiKeyName[] = "key";
#endif

// This is the default backoff policy used to communicate with the Data
// Reduction Proxy configuration service.
const net::BackoffEntry::Policy kDefaultBackoffPolicy = {
    0,               // num_errors_to_ignore
    1 * 1000,        // initial_delay_ms
    4,               // multiply_factor
    0.10,            // jitter_factor,
    30 * 60 * 1000,  // maximum_backoff_ms
    -1,              // entry_lifetime_ms
    true,            // always_use_initial_delay
};

// Default value for |minimum_refresh_interval_on_success_|. This is used if
// the value through client config field trial is unavailable.
const int64_t kMinDelayOnSuccessMilliseconds = 5 * 60 * 1000;  // 5 minutes

// Extracts the list of Data Reduction Proxy servers to use for HTTP requests.
std::vector<net::ProxyServer> GetProxiesForHTTP(
    const data_reduction_proxy::ProxyConfig& proxy_config) {
  std::vector<net::ProxyServer> proxies;
  for (const auto& server : proxy_config.http_proxy_servers()) {
    if (server.scheme() != ProxyServer_ProxyScheme_UNSPECIFIED) {
      proxies.push_back(net::ProxyServer(
          config_parser::SchemeFromProxyScheme(server.scheme()),
          net::HostPortPair(server.host(), server.port())));
    }
  }

  return proxies;
}

GURL AddApiKeyToUrl(const GURL& url) {
  GURL new_url = url;
#if defined(USE_GOOGLE_API_KEYS)
  std::string api_key = google_apis::GetAPIKey();
  if (google_apis::HasKeysConfigured() && !api_key.empty()) {
    new_url = net::AppendOrReplaceQueryParameter(url, kApiKeyName, api_key);
  }
#endif
  return net::AppendOrReplaceQueryParameter(new_url, "alt", "proto");
}

void RecordAuthExpiredHistogram(bool auth_expired) {
  UMA_HISTOGRAM_BOOLEAN(kUMAConfigServiceAuthExpired, auth_expired);
}

}  // namespace

const net::BackoffEntry::Policy& GetBackoffPolicy() {
  return kDefaultBackoffPolicy;
}

DataReductionProxyConfigServiceClient::DataReductionProxyConfigServiceClient(
    scoped_ptr<DataReductionProxyParams> params,
    const net::BackoffEntry::Policy& backoff_policy,
    DataReductionProxyRequestOptions* request_options,
    DataReductionProxyMutableConfigValues* config_values,
    DataReductionProxyConfig* config,
    DataReductionProxyEventCreator* event_creator,
    net::NetLog* net_log,
    ConfigStorer config_storer)
    : params_(params.Pass()),
      request_options_(request_options),
      config_values_(config_values),
      config_(config),
      event_creator_(event_creator),
      net_log_(net_log),
      config_storer_(config_storer),
      backoff_entry_(&backoff_policy),
      config_service_url_(AddApiKeyToUrl(params::GetConfigServiceURL())),
      enabled_(false),
      use_local_config_(!config_service_url_.is_valid()),
      remote_config_applied_(false),
      url_request_context_getter_(nullptr),
      previous_request_failed_authentication_(false),
      minimum_refresh_interval_on_success_(
          base::TimeDelta::FromMilliseconds(kMinDelayOnSuccessMilliseconds)) {
  DCHECK(request_options);
  DCHECK(config_values);
  DCHECK(config);
  DCHECK(event_creator);
  DCHECK(net_log);
  // Constructed on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

DataReductionProxyConfigServiceClient::
    ~DataReductionProxyConfigServiceClient() {
  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
}

base::TimeDelta
DataReductionProxyConfigServiceClient::CalculateNextConfigRefreshTime(
    bool fetch_succeeded,
    const base::Time& config_expiration,
    const base::Time& now,
    const base::TimeDelta& backoff_delay) const {
  DCHECK(backoff_delay >= base::TimeDelta());
  if (fetch_succeeded) {
    base::TimeDelta success_delay = config_expiration - now;
    return std::max(
        backoff_delay,
        std::max(success_delay, minimum_refresh_interval_on_success()));
  }

  return backoff_delay;
}

void DataReductionProxyConfigServiceClient::PopulateClientConfigParams() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::string field_trial = params::GetClientConfigFieldTrialName();

  uint64_t minimum_refresh_interval_on_success_milliseconds =
      kMinDelayOnSuccessMilliseconds;
  std::string variation_value = variations::GetVariationParamValue(
      field_trial, "minimum_refresh_interval_on_success_msec");
  if (!variation_value.empty() &&
      base::StringToUint64(variation_value,
                           &minimum_refresh_interval_on_success_milliseconds)) {
    minimum_refresh_interval_on_success_ = base::TimeDelta::FromMilliseconds(
        minimum_refresh_interval_on_success_milliseconds);
  }
}

void DataReductionProxyConfigServiceClient::InitializeOnIOThread(
    net::URLRequestContextGetter* url_request_context_getter) {
  DCHECK(url_request_context_getter);
  net::NetworkChangeNotifier::AddIPAddressObserver(this);
  url_request_context_getter_ = url_request_context_getter;
}

void DataReductionProxyConfigServiceClient::SetEnabled(bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (enabled)
    PopulateClientConfigParams();
  enabled_ = enabled;
}

void DataReductionProxyConfigServiceClient::RetrieveConfig() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!enabled_)
    return;

  bound_net_log_ = net::BoundNetLog::Make(
      net_log_, net::NetLog::SOURCE_DATA_REDUCTION_PROXY);
  // Strip off query string parameters
  GURL::Replacements replacements;
  replacements.ClearQuery();
  GURL base_config_service_url =
      config_service_url_.ReplaceComponents(replacements);
  event_creator_->BeginConfigRequest(bound_net_log_, base_config_service_url);
  config_fetch_start_time_ = base::Time::Now();
  if (use_local_config_) {
    ReadAndApplyStaticConfig();
    return;
  }

  RetrieveRemoteConfig();
}

void DataReductionProxyConfigServiceClient::ApplySerializedConfig(
    const std::string& config_value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (use_local_config_)
    return;

  if (remote_config_applied_)
    return;

  std::string decoded_config;
  if (base::Base64Decode(config_value, &decoded_config)) {
    ClientConfig config;
    if (config.ParseFromString(decoded_config))
      ParseAndApplyProxyConfig(config);
  }
}

bool DataReductionProxyConfigServiceClient::ShouldRetryDueToAuthFailure(
    const net::HttpResponseHeaders* response_headers,
    const net::HostPortPair& proxy_server) {
  DCHECK(response_headers);
  if (config_->IsDataReductionProxy(proxy_server, nullptr)) {
    if (response_headers->response_code() ==
        net::HTTP_PROXY_AUTHENTICATION_REQUIRED) {
      DCHECK(!use_local_config_);
      // The default backoff logic is to increment the failure count (and
      // increase the backoff time) with each response failure to the remote
      // config service, and to decrement the failure count (and decrease the
      // backoff time) with each response success. In the case where the
      // config service returns a success response (decrementing the failure
      // count) but the session key is continually invalid (as a response from
      // the Data Reduction Proxy and not the config service), the previous
      // response should be considered a failure in order to ensure the backoff
      // time continues to increase.
      if (previous_request_failed_authentication_)
        GetBackoffEntry()->InformOfRequest(false);

      // Record that a request resulted in an authentication failure.
      RecordAuthExpiredHistogram(true);
      previous_request_failed_authentication_ = true;
      InvalidateConfig();
      RetrieveConfig();
      return true;
    }

    previous_request_failed_authentication_ = false;
  }

  return false;
}

net::BackoffEntry* DataReductionProxyConfigServiceClient::GetBackoffEntry() {
  return &backoff_entry_;
}

base::TimeDelta
DataReductionProxyConfigServiceClient::minimum_refresh_interval_on_success()
    const {
  return minimum_refresh_interval_on_success_;
}

void DataReductionProxyConfigServiceClient::SetConfigRefreshTimer(
    const base::TimeDelta& delay) {
  DCHECK(delay >= base::TimeDelta());
  config_refresh_timer_.Stop();
  config_refresh_timer_.Start(
      FROM_HERE, delay, this,
      &DataReductionProxyConfigServiceClient::RetrieveConfig);
}

base::Time DataReductionProxyConfigServiceClient::Now() {
  return base::Time::Now();
}

std::string
DataReductionProxyConfigServiceClient::ConstructStaticResponse() const {
  std::string response;
  ClientConfig config;
  params_->PopulateConfigResponse(&config);
  request_options_->PopulateConfigResponse(&config);
  config.SerializeToString(&response);
  return response;
}

void DataReductionProxyConfigServiceClient::OnIPAddressChanged() {
  GetBackoffEntry()->Reset();
  RetrieveConfig();
}

void DataReductionProxyConfigServiceClient::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(source == fetcher_.get());
  net::URLRequestStatus status = source->GetStatus();
  std::string response;
  source->GetResponseAsString(&response);
  HandleResponse(response, status, source->GetResponseCode());
}

void DataReductionProxyConfigServiceClient::ReadAndApplyStaticConfig() {
  std::string static_response = ConstructStaticResponse();
  HandleResponse(static_response, net::URLRequestStatus(), net::HTTP_OK);
}

void DataReductionProxyConfigServiceClient::RetrieveRemoteConfig() {
  CreateClientConfigRequest request;
  std::string serialized_request;
  const std::string& session_key = request_options_->GetSecureSession();
  if (!session_key.empty())
    request.set_session_key(request_options_->GetSecureSession());
  request.SerializeToString(&serialized_request);
  scoped_ptr<net::URLFetcher> fetcher =
      GetURLFetcherForConfig(config_service_url_, serialized_request);
  if (!fetcher.get()) {
    HandleResponse(std::string(),
                   net::URLRequestStatus::FromError(net::ERR_ABORTED),
                   net::URLFetcher::RESPONSE_CODE_INVALID);
    return;
  }

  fetcher_ = fetcher.Pass();
  fetcher_->Start();
}

void DataReductionProxyConfigServiceClient::InvalidateConfig() {
  GetBackoffEntry()->InformOfRequest(false);
  if (use_local_config_)
    return;

  config_storer_.Run(std::string());
  request_options_->Invalidate();
  config_values_->Invalidate();
  config_->ReloadConfig();
}

scoped_ptr<net::URLFetcher>
DataReductionProxyConfigServiceClient::GetURLFetcherForConfig(
    const GURL& secure_proxy_check_url,
    const std::string& request_body) {
  scoped_ptr<net::URLFetcher> fetcher(net::URLFetcher::Create(
      secure_proxy_check_url, net::URLFetcher::POST, this));
  fetcher->SetLoadFlags(net::LOAD_BYPASS_PROXY);
  fetcher->SetUploadData("application/x-protobuf", request_body);
  DCHECK(url_request_context_getter_);
  fetcher->SetRequestContext(url_request_context_getter_);
  // Configure max retries to be at most kMaxRetries times for 5xx errors.
  static const int kMaxRetries = 5;
  fetcher->SetMaxRetriesOn5xx(kMaxRetries);
  fetcher->SetAutomaticallyRetryOnNetworkChanges(kMaxRetries);
  return fetcher.Pass();
}

void DataReductionProxyConfigServiceClient::HandleResponse(
    const std::string& config_data,
    const net::URLRequestStatus& status,
    int response_code) {
  ClientConfig config;
  bool succeeded = false;

  if (!use_local_config_) {
    UMA_HISTOGRAM_SPARSE_SLOWLY(kUMAConfigServiceFetchResponseCode,
                                response_code);
  }

  if (status.status() == net::URLRequestStatus::SUCCESS &&
      response_code == net::HTTP_OK && config.ParseFromString(config_data)) {
    succeeded = ParseAndApplyProxyConfig(config);
  }

  base::Time expiration_time;
  if (succeeded) {
    expiration_time = config_parser::TimestampToTime(config.refresh_time());
  }

  if (!use_local_config_ && succeeded) {
    base::TimeDelta configuration_fetch_latency =
        base::Time::Now() - config_fetch_start_time_;
    RecordAuthExpiredHistogram(false);
    UMA_HISTOGRAM_MEDIUM_TIMES(kUMAConfigServiceFetchLatency,
                               configuration_fetch_latency);
    UMA_HISTOGRAM_COUNTS_100(kUMAConfigServiceFetchFailedAttemptsBeforeSuccess,
                             GetBackoffEntry()->failure_count());
    std::string encoded_config;
    base::Base64Encode(config_data, &encoded_config);
    config_storer_.Run(encoded_config);
  }

  GetBackoffEntry()->InformOfRequest(succeeded);
  base::TimeDelta next_config_refresh_time =
      CalculateNextConfigRefreshTime(succeeded, expiration_time, Now(),
                                     GetBackoffEntry()->GetTimeUntilRelease());
  SetConfigRefreshTimer(next_config_refresh_time);
  event_creator_->EndConfigRequest(
      bound_net_log_, status.error(), response_code,
      GetBackoffEntry()->failure_count(), next_config_refresh_time);
}

bool DataReductionProxyConfigServiceClient::ParseAndApplyProxyConfig(
    const ClientConfig& config) {
  if (!config.has_proxy_config())
    return false;

  std::vector<net::ProxyServer> proxies =
      GetProxiesForHTTP(config.proxy_config());

  if (params_ && params::IsDevRolloutEnabled() && !use_local_config_) {
    // If dev rollout is enabled, proxies returned by client config API are
    // discarded.
    proxies.clear();
    proxies.push_back(net::ProxyServer::FromURI(params_->GetDefaultDevOrigin(),
                                                net::ProxyServer::SCHEME_HTTP));
    proxies.push_back(net::ProxyServer::FromURI(
        params_->GetDefaultDevFallbackOrigin(), net::ProxyServer::SCHEME_HTTP));
  }

  if (proxies.empty())
    return false;

  if (!use_local_config_) {
    request_options_->SetSecureSession(config.session_key());
    // If QUIC is enabled, the scheme of the first proxy (if it is HTTPS) would
    // be changed to QUIC.
    if (proxies[0].scheme() == net::ProxyServer::SCHEME_HTTPS && params_ &&
        params_->quic_enabled()) {
      proxies[0] = net::ProxyServer(net::ProxyServer::SCHEME_QUIC,
                                    proxies[0].host_port_pair());
      DCHECK_EQ(net::ProxyServer::SCHEME_QUIC, proxies[0].scheme());
    }
    config_values_->UpdateValues(proxies);
    config_->ReloadConfig();
    remote_config_applied_ = true;
    return true;
  }

  std::string session;
  std::string credentials;
  if (!DataReductionProxyRequestOptions::ParseLocalSessionKey(
          config.session_key(), &session, &credentials)) {
    return false;
  }

  request_options_->SetCredentials(session, credentials);
  config_values_->UpdateValues(proxies);
  config_->ReloadConfig();
  return true;
}

}  // namespace data_reduction_proxy
