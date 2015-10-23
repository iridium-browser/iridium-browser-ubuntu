// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_config_values.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_creator.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_quality_estimator.h"
#include "net/proxy/proxy_server.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

using base::FieldTrialList;

namespace {

const char kEnabled[] = "Enabled";
const char kControl[] = "Control";

// Values of the UMA DataReductionProxy.NetworkChangeEvents histograms.
// This enum must remain synchronized with the enum of the same
// name in metrics/histograms/histograms.xml.
enum DataReductionProxyNetworkChangeEvent {
  IP_CHANGED = 0,         // The client IP address changed.
  DISABLED_ON_VPN = 1,    // The proxy is disabled because a VPN is running.
  CHANGE_EVENT_COUNT = 2  // This must always be last.
};

// Key of the UMA DataReductionProxy.ProbeURL histogram.
const char kUMAProxyProbeURL[] = "DataReductionProxy.ProbeURL";

// Key of the UMA DataReductionProxy.ProbeURLNetError histogram.
const char kUMAProxyProbeURLNetError[] = "DataReductionProxy.ProbeURLNetError";

// Key of the UMA DataReductionProxy.SecureProxyCheck.Latency histogram.
const char kUMAProxySecureProxyCheckLatency[] =
    "DataReductionProxy.SecureProxyCheck.Latency";

// Record a network change event.
void RecordNetworkChangeEvent(DataReductionProxyNetworkChangeEvent event) {
  UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.NetworkChangeEvents", event,
                            CHANGE_EVENT_COUNT);
}

// Looks for an instance of |host_port_pair| in |proxy_list|, and returns true
// if found. Also sets |index| to the index at which the matching address was
// found.
bool FindProxyInList(const std::vector<net::ProxyServer>& proxy_list,
                     const net::HostPortPair& host_port_pair,
                     int* index) {
  for (size_t proxy_index = 0; proxy_index < proxy_list.size(); ++proxy_index) {
    const net::ProxyServer& proxy = proxy_list[proxy_index];
    if (proxy.is_valid() && proxy.host_port_pair().Equals(host_port_pair)) {
      *index = proxy_index;
      return true;
    }
  }
  return false;
}

// Following UMA is plotted to measure how frequently Lo-Fi state changes.
// Too frequent changes are undesirable.
void RecordAutoLoFiRequestHeaderStateChange(
    net::NetworkChangeNotifier::ConnectionType connection_type,
    bool previous_header_low,
    bool current_header_low) {
  // Auto Lo-Fi request header state changes.
  // Possible Lo-Fi header directives are empty ("") and low ("q=low").
  // This enum must remain synchronized with the enum of the same name in
  // metrics/histograms/histograms.xml.
  enum AutoLoFiRequestHeaderState {
    AUTO_LOFI_REQUEST_HEADER_STATE_EMPTY_TO_EMPTY = 0,
    AUTO_LOFI_REQUEST_HEADER_STATE_EMPTY_TO_LOW = 1,
    AUTO_LOFI_REQUEST_HEADER_STATE_LOW_TO_EMPTY = 2,
    AUTO_LOFI_REQUEST_HEADER_STATE_LOW_TO_LOW = 3,
    AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY
  };

  AutoLoFiRequestHeaderState state;
  if (!previous_header_low) {
    if (current_header_low)
      state = AUTO_LOFI_REQUEST_HEADER_STATE_EMPTY_TO_LOW;
    else
      state = AUTO_LOFI_REQUEST_HEADER_STATE_EMPTY_TO_EMPTY;
  } else {
    if (current_header_low) {
      // Low to low in useful in checking how many consecutive page loads
      // are done with Lo-Fi enabled.
      state = AUTO_LOFI_REQUEST_HEADER_STATE_LOW_TO_LOW;
    } else {
      state = AUTO_LOFI_REQUEST_HEADER_STATE_LOW_TO_EMPTY;
    }
  }

  switch (connection_type) {
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.AutoLoFiRequestHeaderState.Unknown", state,
          AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.AutoLoFiRequestHeaderState.Ethernet", state,
          AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.AutoLoFiRequestHeaderState.WiFi", state,
          AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_2G:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.AutoLoFiRequestHeaderState.2G", state,
          AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_3G:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.AutoLoFiRequestHeaderState.3G", state,
          AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_4G:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.AutoLoFiRequestHeaderState.4G", state,
          AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_NONE:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.AutoLoFiRequestHeaderState.None", state,
          AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.AutoLoFiRequestHeaderState.Bluetooth", state,
          AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY);
      break;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace

namespace data_reduction_proxy {

// Checks if the secure proxy is allowed by the carrier by sending a probe.
class SecureProxyChecker : public net::URLFetcherDelegate {
 public:
  SecureProxyChecker(const scoped_refptr<net::URLRequestContextGetter>&
                         url_request_context_getter)
      : url_request_context_getter_(url_request_context_getter) {}

  void OnURLFetchComplete(const net::URLFetcher* source) override {
    DCHECK_EQ(source, fetcher_.get());
    net::URLRequestStatus status = source->GetStatus();

    std::string response;
    source->GetResponseAsString(&response);

    base::TimeDelta secure_proxy_check_latency =
        base::Time::Now() - secure_proxy_check_start_time_;
    if (secure_proxy_check_latency >= base::TimeDelta()) {
      UMA_HISTOGRAM_MEDIUM_TIMES(kUMAProxySecureProxyCheckLatency,
                                 secure_proxy_check_latency);
    }

    fetcher_callback_.Run(response, status, source->GetResponseCode());
  }

  void CheckIfSecureProxyIsAllowed(const GURL& secure_proxy_check_url,
                                   FetcherResponseCallback fetcher_callback) {
    fetcher_ = net::URLFetcher::Create(secure_proxy_check_url,
                                       net::URLFetcher::GET, this);
    fetcher_->SetLoadFlags(net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_PROXY);
    fetcher_->SetRequestContext(url_request_context_getter_.get());
    // Configure max retries to be at most kMaxRetries times for 5xx errors.
    static const int kMaxRetries = 5;
    fetcher_->SetMaxRetriesOn5xx(kMaxRetries);
    fetcher_->SetAutomaticallyRetryOnNetworkChanges(kMaxRetries);
    // The secure proxy check should not be redirected. Since the secure proxy
    // check will inevitably fail if it gets redirected somewhere else (e.g. by
    // a captive portal), short circuit that by giving up on the secure proxy
    // check if it gets redirected.
    fetcher_->SetStopOnRedirect(true);

    fetcher_callback_ = fetcher_callback;

    secure_proxy_check_start_time_ = base::Time::Now();
    fetcher_->Start();
  }

  ~SecureProxyChecker() override {}

 private:
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  // The URLFetcher being used for the secure proxy check.
  scoped_ptr<net::URLFetcher> fetcher_;
  FetcherResponseCallback fetcher_callback_;

  // Used to determine the latency in performing the Data Reduction Proxy secure
  // proxy check.
  base::Time secure_proxy_check_start_time_;

  DISALLOW_COPY_AND_ASSIGN(SecureProxyChecker);
};

DataReductionProxyConfig::DataReductionProxyConfig(
    net::NetLog* net_log,
    scoped_ptr<DataReductionProxyConfigValues> config_values,
    DataReductionProxyConfigurator* configurator,
    DataReductionProxyEventCreator* event_creator)
    : secure_proxy_allowed_(params::ShouldUseSecureProxyByDefault()),
      disabled_on_vpn_(false),
      unreachable_(false),
      enabled_by_user_(false),
      config_values_(config_values.Pass()),
      net_log_(net_log),
      configurator_(configurator),
      event_creator_(event_creator),
      auto_lofi_minimum_rtt_(base::TimeDelta::Max()),
      auto_lofi_maximum_kbps_(0),
      auto_lofi_hysteresis_(base::TimeDelta::Max()),
      network_quality_last_updated_(base::TimeTicks()),
      network_prohibitively_slow_(false),
      connection_type_(net::NetworkChangeNotifier::GetConnectionType()),
      lofi_status_(LOFI_STATUS_TEMPORARILY_OFF),
      last_main_frame_request_(base::TimeTicks::Now()),
      network_quality_at_last_main_frame_request_(
          NETWORK_QUALITY_AT_LAST_MAIN_FRAME_REQUEST_UNKNOWN) {
  DCHECK(configurator);
  DCHECK(event_creator);
  if (params::IsLoFiDisabledViaFlags())
    SetLoFiModeOff();
  // Constructed on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

DataReductionProxyConfig::~DataReductionProxyConfig() {
  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
}

void DataReductionProxyConfig::InitializeOnIOThread(const scoped_refptr<
    net::URLRequestContextGetter>& url_request_context_getter) {
  secure_proxy_checker_.reset(
      new SecureProxyChecker(url_request_context_getter));

  if (!config_values_->allowed())
    return;

  PopulateAutoLoFiParams();
  AddDefaultProxyBypassRules();
  net::NetworkChangeNotifier::AddIPAddressObserver(this);
}

void DataReductionProxyConfig::ReloadConfig() {
  DCHECK(thread_checker_.CalledOnValidThread());
  UpdateConfigurator(enabled_by_user_, secure_proxy_allowed_);
}

bool DataReductionProxyConfig::WasDataReductionProxyUsed(
    const net::URLRequest* request,
    DataReductionProxyTypeInfo* proxy_info) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(request);
  return IsDataReductionProxy(request->proxy_server(), proxy_info);
}

bool DataReductionProxyConfig::IsDataReductionProxy(
    const net::HostPortPair& host_port_pair,
    DataReductionProxyTypeInfo* proxy_info) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  int proxy_index = 0;
  if (FindProxyInList(config_values_->proxies_for_http(), host_port_pair,
                      &proxy_index)) {
    if (proxy_info) {
      const std::vector<net::ProxyServer>& proxy_list =
          config_values_->proxies_for_http();
      proxy_info->proxy_servers = std::vector<net::ProxyServer>(
          proxy_list.begin() + proxy_index, proxy_list.end());
      proxy_info->is_fallback = (proxy_index != 0);
    }
    return true;
  }

  if (FindProxyInList(config_values_->proxies_for_https(), host_port_pair,
                      &proxy_index)) {
    if (proxy_info) {
      const std::vector<net::ProxyServer>& proxy_list =
          config_values_->proxies_for_https();
      proxy_info->proxy_servers = std::vector<net::ProxyServer>(
          proxy_list.begin() + proxy_index, proxy_list.end());
      proxy_info->is_fallback = (proxy_index != 0);
      proxy_info->is_ssl = true;
    }
    return true;
  }

  return false;
}

bool DataReductionProxyConfig::IsBypassedByDataReductionProxyLocalRules(
    const net::URLRequest& request,
    const net::ProxyConfig& data_reduction_proxy_config) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(request.context());
  DCHECK(request.context()->proxy_service());
  net::ProxyInfo result;
  data_reduction_proxy_config.proxy_rules().Apply(
      request.url(), &result);
  if (!result.proxy_server().is_valid())
    return true;
  if (result.proxy_server().is_direct())
    return true;
  return !IsDataReductionProxy(result.proxy_server().host_port_pair(), NULL);
}

bool DataReductionProxyConfig::AreDataReductionProxiesBypassed(
    const net::URLRequest& request,
    const net::ProxyConfig& data_reduction_proxy_config,
    base::TimeDelta* min_retry_delay) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (request.context() != NULL &&
      request.context()->proxy_service() != NULL) {
    return AreProxiesBypassed(
        request.context()->proxy_service()->proxy_retry_info(),
        data_reduction_proxy_config.proxy_rules(),
        request.url().SchemeIsCryptographic(), min_retry_delay);
  }

  return false;
}

bool DataReductionProxyConfig::AreProxiesBypassed(
    const net::ProxyRetryInfoMap& retry_map,
    const net::ProxyConfig::ProxyRules& proxy_rules,
    bool is_https,
    base::TimeDelta* min_retry_delay) const {
  // Data reduction proxy config is TYPE_PROXY_PER_SCHEME.
  if (proxy_rules.type != net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME)
    return false;

  const net::ProxyList* proxies = is_https ?
      proxy_rules.MapUrlSchemeToProxyList(url::kHttpsScheme) :
      proxy_rules.MapUrlSchemeToProxyList(url::kHttpScheme);

  if (!proxies)
    return false;

  base::TimeDelta min_delay = base::TimeDelta::Max();
  bool bypassed = false;

  for (const net::ProxyServer proxy : proxies->GetAll()) {
    if (!proxy.is_valid() || proxy.is_direct())
      continue;

    base::TimeDelta delay;
    if (IsDataReductionProxy(proxy.host_port_pair(), NULL)) {
      if (!IsProxyBypassed(retry_map, proxy, &delay))
        return false;
      if (delay < min_delay)
        min_delay = delay;
      bypassed = true;
    }
  }

  if (min_retry_delay && bypassed)
    *min_retry_delay = min_delay;

  return bypassed;
}

bool DataReductionProxyConfig::IsNetworkQualityProhibitivelySlow(
    const net::NetworkQualityEstimator* network_quality_estimator) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsIncludedInLoFiEnabledFieldTrial() ||
         IsIncludedInLoFiControlFieldTrial() ||
         params::IsLoFiSlowConnectionsOnlyViaFlags());

  if (!network_quality_estimator)
    return false;

  // True iff network type changed since the last call to
  // IsNetworkQualityProhibitivelySlow(). This call happens only on main frame
  // requests.
  bool network_type_changed = false;
  if (net::NetworkChangeNotifier::GetConnectionType() != connection_type_) {
    connection_type_ = net::NetworkChangeNotifier::GetConnectionType();
    network_type_changed = true;
  }

  // Initialize to fastest RTT and fastest bandwidth.
  base::TimeDelta rtt = base::TimeDelta();
  int32_t kbps = INT32_MAX;

  bool is_network_quality_available =
      network_quality_estimator->GetRTTEstimate(&rtt) &&
      network_quality_estimator->GetDownlinkThroughputKbpsEstimate(&kbps);

  // True only if the network is currently estimated to be slower than the
  // defined thresholds.
  bool is_network_currently_slow = false;

  if (is_network_quality_available) {
    // Network is slow if either the downlink bandwidth is too low or the RTT is
    // too high.
    is_network_currently_slow =
        kbps < auto_lofi_maximum_kbps_ || rtt > auto_lofi_minimum_rtt_;

    network_quality_at_last_main_frame_request_ =
        is_network_currently_slow
            ? NETWORK_QUALITY_AT_LAST_MAIN_FRAME_REQUEST_SLOW
            : NETWORK_QUALITY_AT_LAST_MAIN_FRAME_REQUEST_NOT_SLOW;
  }

  // Return the cached entry if the last update was within the hysteresis
  // duration and if the connection type has not changed.
  if (!network_type_changed && !network_quality_last_updated_.is_null() &&
      base::TimeTicks::Now() - network_quality_last_updated_ <=
          auto_lofi_hysteresis_) {
    return network_prohibitively_slow_;
  }

  network_quality_last_updated_ = base::TimeTicks::Now();

  if (!is_network_quality_available)
    return false;

  network_prohibitively_slow_ = is_network_currently_slow;
  return network_prohibitively_slow_;
}

bool DataReductionProxyConfig::IsIncludedInLoFiEnabledFieldTrial() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return FieldTrialList::FindFullName(params::GetLoFiFieldTrialName()) ==
         kEnabled;
}

bool DataReductionProxyConfig::IsIncludedInLoFiControlFieldTrial() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return FieldTrialList::FindFullName(params::GetLoFiFieldTrialName()) ==
         kControl;
}

LoFiStatus DataReductionProxyConfig::GetLoFiStatus() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return lofi_status_;
}

// static
bool DataReductionProxyConfig::ShouldUseLoFiHeaderForRequests(
    LoFiStatus lofi_status) {
  switch (lofi_status) {
    case LOFI_STATUS_OFF:
    case LOFI_STATUS_TEMPORARILY_OFF:
    case LOFI_STATUS_ACTIVE_CONTROL:
    case LOFI_STATUS_INACTIVE_CONTROL:
    case LOFI_STATUS_INACTIVE:
      return false;
    // Lo-Fi header can be used only if Lo-Fi is not temporarily off and either
    // the user has enabled Lo-Fi through flags, or session is in Lo-Fi enabled
    // group with network quality prohibitively slow.
    case LOFI_STATUS_ACTIVE_FROM_FLAGS:
    case LOFI_STATUS_ACTIVE:
      return true;
    default:
      NOTREACHED() << lofi_status;
  }
  return false;
}

bool DataReductionProxyConfig::ShouldUseLoFiHeaderForRequests() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return ShouldUseLoFiHeaderForRequests(lofi_status_);
}

void DataReductionProxyConfig::PopulateAutoLoFiParams() {
  std::string field_trial = params::GetLoFiFieldTrialName();

  if (params::IsLoFiSlowConnectionsOnlyViaFlags()) {
    // Default parameters to use.
    auto_lofi_minimum_rtt_ = base::TimeDelta::FromMilliseconds(2000);
    auto_lofi_maximum_kbps_ = 0;
    auto_lofi_hysteresis_ = base::TimeDelta::FromSeconds(60);
    field_trial = params::GetLoFiFlagFieldTrialName();
  }

  if (!IsIncludedInLoFiControlFieldTrial() &&
      !IsIncludedInLoFiEnabledFieldTrial() &&
      !params::IsLoFiSlowConnectionsOnlyViaFlags()) {
    return;
  }

  uint64_t auto_lofi_minimum_rtt_msec;
  std::string variation_value =
      variations::GetVariationParamValue(field_trial, "rtt_msec");
  if (!variation_value.empty() &&
      base::StringToUint64(variation_value, &auto_lofi_minimum_rtt_msec)) {
    auto_lofi_minimum_rtt_ =
        base::TimeDelta::FromMilliseconds(auto_lofi_minimum_rtt_msec);
  }
  DCHECK_GE(auto_lofi_minimum_rtt_, base::TimeDelta());

  int32_t auto_lofi_maximum_kbps;
  variation_value = variations::GetVariationParamValue(field_trial, "kbps");
  if (!variation_value.empty() &&
      base::StringToInt(variation_value, &auto_lofi_maximum_kbps)) {
    auto_lofi_maximum_kbps_ = auto_lofi_maximum_kbps;
  }
  DCHECK_GE(auto_lofi_maximum_kbps_, 0);

  uint32_t auto_lofi_hysteresis_period_seconds;
  variation_value = variations::GetVariationParamValue(
      field_trial, "hysteresis_period_seconds");
  if (!variation_value.empty() &&
      base::StringToUint(variation_value,
                         &auto_lofi_hysteresis_period_seconds)) {
    auto_lofi_hysteresis_ =
        base::TimeDelta::FromSeconds(auto_lofi_hysteresis_period_seconds);
  }
  DCHECK_GE(auto_lofi_hysteresis_, base::TimeDelta());
}

bool DataReductionProxyConfig::IsProxyBypassed(
    const net::ProxyRetryInfoMap& retry_map,
    const net::ProxyServer& proxy_server,
    base::TimeDelta* retry_delay) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  net::ProxyRetryInfoMap::const_iterator found =
      retry_map.find(proxy_server.ToURI());

  if (found == retry_map.end() ||
      found->second.bad_until < base::TimeTicks::Now()) {
    return false;
  }

  if (retry_delay)
     *retry_delay = found->second.current_delay;

  return true;
}

bool DataReductionProxyConfig::ContainsDataReductionProxy(
    const net::ProxyConfig::ProxyRules& proxy_rules) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Data Reduction Proxy configurations are always TYPE_PROXY_PER_SCHEME.
  if (proxy_rules.type != net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME)
    return false;

  const net::ProxyList* https_proxy_list =
      proxy_rules.MapUrlSchemeToProxyList("https");
  if (https_proxy_list && !https_proxy_list->IsEmpty() &&
      // Sufficient to check only the first proxy.
      IsDataReductionProxy(https_proxy_list->Get().host_port_pair(), NULL)) {
    return true;
  }

  const net::ProxyList* http_proxy_list =
      proxy_rules.MapUrlSchemeToProxyList("http");
  if (http_proxy_list && !http_proxy_list->IsEmpty() &&
      // Sufficient to check only the first proxy.
      IsDataReductionProxy(http_proxy_list->Get().host_port_pair(), NULL)) {
    return true;
  }

  return false;
}

bool DataReductionProxyConfig::UsingHTTPTunnel(
    const net::HostPortPair& proxy_server) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return config_values_->UsingHTTPTunnel(proxy_server);
}

// Returns true if the Data Reduction Proxy configuration may be used.
bool DataReductionProxyConfig::allowed() const {
  return config_values_->allowed();
}

// Returns true if the Data Reduction Proxy promo may be shown. This is not
// tied to whether the Data Reduction Proxy is enabled.
bool DataReductionProxyConfig::promo_allowed() const {
  return config_values_->promo_allowed();
}

void DataReductionProxyConfig::SetProxyConfig(bool enabled, bool at_startup) {
  DCHECK(thread_checker_.CalledOnValidThread());
  enabled_by_user_ = enabled;
  UpdateConfigurator(enabled_by_user_, secure_proxy_allowed_);

  // Check if the proxy has been restricted explicitly by the carrier.
  if (enabled) {
    // It is safe to use base::Unretained here, since it gets executed
    // synchronously on the IO thread, and |this| outlives
    // |secure_proxy_checker_|.
    SecureProxyCheck(
        config_values_->secure_proxy_check_url(),
        base::Bind(&DataReductionProxyConfig::HandleSecureProxyCheckResponse,
                   base::Unretained(this)));
  }
}

void DataReductionProxyConfig::UpdateConfigurator(bool enabled,
                                                  bool secure_proxy_allowed) {
  DCHECK(configurator_);
  std::vector<net::ProxyServer> proxies_for_http =
      config_values_->proxies_for_http();
  std::vector<net::ProxyServer> proxies_for_https =
      config_values_->proxies_for_https();
  if (enabled && !disabled_on_vpn_ && !config_values_->holdback() &&
      (!proxies_for_http.empty() || !proxies_for_https.empty())) {
    configurator_->Enable(!secure_proxy_allowed, proxies_for_http,
                          proxies_for_https);
  } else {
    configurator_->Disable();
  }
}

void DataReductionProxyConfig::HandleSecureProxyCheckResponse(
    const std::string& response,
    const net::URLRequestStatus& status,
    int http_response_code) {
  bool success_response = ("OK" == response.substr(0, 2));
  if (event_creator_)
    event_creator_->EndSecureProxyCheck(bound_net_log_, status.error(),
                                        http_response_code, success_response);

  if (status.status() == net::URLRequestStatus::FAILED) {
    if (status.error() == net::ERR_INTERNET_DISCONNECTED) {
      RecordSecureProxyCheckFetchResult(INTERNET_DISCONNECTED);
      return;
    }
    // TODO(bengr): Remove once we understand the reasons secure proxy checks
    // are failing. Secure proxy check errors are either due to fetcher-level
    // errors or modified responses. This only tracks the former.
    UMA_HISTOGRAM_SPARSE_SLOWLY(kUMAProxyProbeURLNetError,
                                std::abs(status.error()));
  }

  if (success_response) {
    DVLOG(1) << "The data reduction proxy is unrestricted.";

    if (enabled_by_user_) {
      if (!secure_proxy_allowed_) {
        secure_proxy_allowed_ = true;
        // The user enabled the proxy, but sometime previously in the session,
        // the network operator had blocked the secure proxy check and
        // restricted the user. The current network doesn't block the secure
        // proxy check, so don't restrict the proxy configurations.
        ReloadConfig();
        RecordSecureProxyCheckFetchResult(SUCCEEDED_PROXY_ENABLED);
      } else {
        RecordSecureProxyCheckFetchResult(SUCCEEDED_PROXY_ALREADY_ENABLED);
      }
    }
    secure_proxy_allowed_ = true;
    return;
  }
  DVLOG(1) << "The data reduction proxy is restricted to the configured "
           << "fallback proxy.";
  if (enabled_by_user_) {
    if (secure_proxy_allowed_) {
      // Restrict the proxy.
      secure_proxy_allowed_ = false;
      ReloadConfig();
      RecordSecureProxyCheckFetchResult(FAILED_PROXY_DISABLED);
    } else {
      RecordSecureProxyCheckFetchResult(FAILED_PROXY_ALREADY_DISABLED);
    }
  }
  secure_proxy_allowed_ = false;
}

void DataReductionProxyConfig::OnIPAddressChanged() {
  if (enabled_by_user_) {
    DCHECK(config_values_->allowed());
    RecordNetworkChangeEvent(IP_CHANGED);
    if (MaybeDisableIfVPN())
      return;

    bool should_use_secure_proxy = params::ShouldUseSecureProxyByDefault();
    if (!should_use_secure_proxy && secure_proxy_allowed_) {
      secure_proxy_allowed_ = false;
      RecordSecureProxyCheckFetchResult(PROXY_DISABLED_BEFORE_CHECK);
      ReloadConfig();
    }

    // It is safe to use base::Unretained here, since it gets executed
    // synchronously on the IO thread, and |this| outlives
    // |secure_proxy_checker_|.
    SecureProxyCheck(
        config_values_->secure_proxy_check_url(),
        base::Bind(&DataReductionProxyConfig::HandleSecureProxyCheckResponse,
                   base::Unretained(this)));
  }
}

void DataReductionProxyConfig::AddDefaultProxyBypassRules() {
  // localhost
  DCHECK(configurator_);
  configurator_->AddHostPatternToBypass("<local>");
  // RFC6890 loopback addresses.
  // TODO(tbansal): Remove this once crbug/446705 is fixed.
  configurator_->AddHostPatternToBypass("127.0.0.0/8");

  // RFC6890 current network (only valid as source address).
  configurator_->AddHostPatternToBypass("0.0.0.0/8");

  // RFC1918 private addresses.
  configurator_->AddHostPatternToBypass("10.0.0.0/8");
  configurator_->AddHostPatternToBypass("172.16.0.0/12");
  configurator_->AddHostPatternToBypass("192.168.0.0/16");

  // RFC3513 unspecified address.
  configurator_->AddHostPatternToBypass("::/128");

  // RFC4193 private addresses.
  configurator_->AddHostPatternToBypass("fc00::/7");
  // IPV6 probe addresses.
  configurator_->AddHostPatternToBypass("*-ds.metric.gstatic.com");
  configurator_->AddHostPatternToBypass("*-v4.metric.gstatic.com");
}

void DataReductionProxyConfig::RecordSecureProxyCheckFetchResult(
    SecureProxyCheckFetchResult result) {
  UMA_HISTOGRAM_ENUMERATION(kUMAProxyProbeURL, result,
                            SECURE_PROXY_CHECK_FETCH_RESULT_COUNT);
}

void DataReductionProxyConfig::SecureProxyCheck(
    const GURL& secure_proxy_check_url,
    FetcherResponseCallback fetcher_callback) {
  bound_net_log_ = net::BoundNetLog::Make(
      net_log_, net::NetLog::SOURCE_DATA_REDUCTION_PROXY);
  if (event_creator_) {
    event_creator_->BeginSecureProxyCheck(
        bound_net_log_, config_values_->secure_proxy_check_url());
  }

  secure_proxy_checker_->CheckIfSecureProxyIsAllowed(secure_proxy_check_url,
                                                     fetcher_callback);
}

void DataReductionProxyConfig::SetLoFiModeOff() {
  DCHECK(thread_checker_.CalledOnValidThread());
  lofi_status_ = LOFI_STATUS_OFF;
}

void DataReductionProxyConfig::RecordAutoLoFiAccuracyRate(
    const net::NetworkQualityEstimator* network_quality_estimator) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(network_quality_estimator);
  DCHECK(IsIncludedInLoFiEnabledFieldTrial());
  DCHECK_NE(network_quality_at_last_main_frame_request_,
            NETWORK_QUALITY_AT_LAST_MAIN_FRAME_REQUEST_UNKNOWN);

  base::TimeDelta rtt_since_last_page_load;
  if (!network_quality_estimator->GetRecentMedianRTT(
          last_main_frame_request_, &rtt_since_last_page_load)) {
    return;
  }
  int32_t downstream_throughput_kbps;
  if (!network_quality_estimator->GetRecentMedianDownlinkThroughputKbps(
          last_main_frame_request_, &downstream_throughput_kbps)) {
    return;
  }

  // Values of Auto Lo-Fi accuracy.
  // This enum must remain synchronized with the enum of the same name in
  // metrics/histograms/histograms.xml.
  enum AutoLoFiAccuracy {
    AUTO_LOFI_ACCURACY_ESTIMATED_SLOW_ACTUAL_SLOW = 0,
    AUTO_LOFI_ACCURACY_ESTIMATED_SLOW_ACTUAL_NOT_SLOW = 1,
    AUTO_LOFI_ACCURACY_ESTIMATED_NOT_SLOW_ACTUAL_SLOW = 2,
    AUTO_LOFI_ACCURACY_ESTIMATED_NOT_SLOW_ACTUAL_NOT_SLOW = 3,
    AUTO_LOFI_ACCURACY_INDEX_BOUNDARY
  };

  bool should_have_used_lofi =
      rtt_since_last_page_load > auto_lofi_minimum_rtt_ ||
      downstream_throughput_kbps < auto_lofi_maximum_kbps_;

  AutoLoFiAccuracy accuracy = AUTO_LOFI_ACCURACY_INDEX_BOUNDARY;

  if (should_have_used_lofi) {
    if (network_quality_at_last_main_frame_request_ ==
        NETWORK_QUALITY_AT_LAST_MAIN_FRAME_REQUEST_SLOW) {
      accuracy = AUTO_LOFI_ACCURACY_ESTIMATED_SLOW_ACTUAL_SLOW;
    } else if (network_quality_at_last_main_frame_request_ ==
               NETWORK_QUALITY_AT_LAST_MAIN_FRAME_REQUEST_NOT_SLOW) {
      accuracy = AUTO_LOFI_ACCURACY_ESTIMATED_NOT_SLOW_ACTUAL_SLOW;
    } else {
      NOTREACHED();
    }
  } else {
    if (network_quality_at_last_main_frame_request_ ==
        NETWORK_QUALITY_AT_LAST_MAIN_FRAME_REQUEST_SLOW) {
      accuracy = AUTO_LOFI_ACCURACY_ESTIMATED_SLOW_ACTUAL_NOT_SLOW;
    } else if (network_quality_at_last_main_frame_request_ ==
               NETWORK_QUALITY_AT_LAST_MAIN_FRAME_REQUEST_NOT_SLOW) {
      accuracy = AUTO_LOFI_ACCURACY_ESTIMATED_NOT_SLOW_ACTUAL_NOT_SLOW;
    } else {
      NOTREACHED();
    }
  }

  switch (connection_type_) {
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
      UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.AutoLoFiAccuracy.Unknown",
                                accuracy, AUTO_LOFI_ACCURACY_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
      UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.AutoLoFiAccuracy.Ethernet",
                                accuracy, AUTO_LOFI_ACCURACY_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
      UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.AutoLoFiAccuracy.WiFi",
                                accuracy, AUTO_LOFI_ACCURACY_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_2G:
      UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.AutoLoFiAccuracy.2G",
                                accuracy, AUTO_LOFI_ACCURACY_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_3G:
      UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.AutoLoFiAccuracy.3G",
                                accuracy, AUTO_LOFI_ACCURACY_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_4G:
      UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.AutoLoFiAccuracy.4G",
                                accuracy, AUTO_LOFI_ACCURACY_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_NONE:
      UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.AutoLoFiAccuracy.None",
                                accuracy, AUTO_LOFI_ACCURACY_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.AutoLoFiAccuracy.Bluetooth",
                                accuracy, AUTO_LOFI_ACCURACY_INDEX_BOUNDARY);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void DataReductionProxyConfig::UpdateLoFiStatusOnMainFrameRequest(
    bool user_temporarily_disabled_lofi,
    const net::NetworkQualityEstimator* network_quality_estimator) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Record Lo-Fi accuracy rate only if the session is Lo-Fi enabled
  // field trial, and the user has not enabled Lo-Fi on slow connections
  // via flags.
  if (network_quality_estimator &&
      network_quality_at_last_main_frame_request_ !=
          NETWORK_QUALITY_AT_LAST_MAIN_FRAME_REQUEST_UNKNOWN &&
      IsIncludedInLoFiEnabledFieldTrial() &&
      !params::IsLoFiSlowConnectionsOnlyViaFlags()) {
    RecordAutoLoFiAccuracyRate(network_quality_estimator);
  }
  last_main_frame_request_ = base::TimeTicks::Now();
  network_quality_at_last_main_frame_request_ =
      NETWORK_QUALITY_AT_LAST_MAIN_FRAME_REQUEST_UNKNOWN;

  // If Lo-Fi has been permanently turned off, its status can't change.
  if (lofi_status_ == LOFI_STATUS_OFF)
    return;

  // If the user has temporarily disabled Lo-Fi on a main frame request, it will
  // remain disabled until next main frame request.
  if (user_temporarily_disabled_lofi) {
    switch (lofi_status_) {
      // Turn off Lo-Fi temporarily (until next main frame request) if it was
      // enabled from flags or because the session is in Lo-Fi enabled group.
      case LOFI_STATUS_ACTIVE_FROM_FLAGS:
      case LOFI_STATUS_ACTIVE:
      case LOFI_STATUS_INACTIVE:
        lofi_status_ = LOFI_STATUS_TEMPORARILY_OFF;
        return;
      // Lo-Fi is already temporarily off, so no need to change state.
      case LOFI_STATUS_TEMPORARILY_OFF:
      // If the current session does not have Lo-Fi switch, is not in Auto Lo-Fi
      // enabled group and is in Auto Lo-Fi control group, then we do not need
      // to temporarily disable Lo-Fi because it would never be used.
      case LOFI_STATUS_ACTIVE_CONTROL:
      case LOFI_STATUS_INACTIVE_CONTROL:
        return;

      default:
        NOTREACHED() << "Unexpected Lo-Fi status = " << lofi_status_;
    }
  }

  if (params::IsLoFiAlwaysOnViaFlags()) {
    lofi_status_ = LOFI_STATUS_ACTIVE_FROM_FLAGS;
    return;
  }

  if (params::IsLoFiCellularOnlyViaFlags()) {
    if (net::NetworkChangeNotifier::IsConnectionCellular(
            net::NetworkChangeNotifier::GetConnectionType())) {
      lofi_status_ = LOFI_STATUS_ACTIVE_FROM_FLAGS;
      return;
    }
    lofi_status_ = LOFI_STATUS_TEMPORARILY_OFF;
    return;
  }

  // Store the previous state of Lo-Fi, so that change in Lo-Fi status can be
  // recorded properly. This is not needed for the control group, because it
  // is only used to report changes in request headers, and the request headers
  // are never modified in the control group.
  LoFiStatus previous_lofi_status = lofi_status_;

  if (params::IsLoFiSlowConnectionsOnlyViaFlags() ||
      IsIncludedInLoFiEnabledFieldTrial()) {
    lofi_status_ = IsNetworkQualityProhibitivelySlow(network_quality_estimator)
                       ? LOFI_STATUS_ACTIVE
                       : LOFI_STATUS_INACTIVE;
    RecordAutoLoFiRequestHeaderStateChange(
        connection_type_, ShouldUseLoFiHeaderForRequests(previous_lofi_status),
        ShouldUseLoFiHeaderForRequests(lofi_status_));
    return;
  }

  if (IsIncludedInLoFiControlFieldTrial()) {
    lofi_status_ = IsNetworkQualityProhibitivelySlow(network_quality_estimator)
                       ? LOFI_STATUS_ACTIVE_CONTROL
                       : LOFI_STATUS_INACTIVE_CONTROL;
    return;
  }

  // If Lo-Fi is not enabled through command line and the user is not in
  // Lo-Fi field trials, we set Lo-Fi to permanent off.
  lofi_status_ = LOFI_STATUS_OFF;
}

void DataReductionProxyConfig::GetNetworkList(
    net::NetworkInterfaceList* interfaces,
    int policy) {
  net::GetNetworkList(interfaces, policy);
}

bool DataReductionProxyConfig::MaybeDisableIfVPN() {
  if (params::IsIncludedInUseDataSaverOnVPNFieldTrial()) {
    return false;
  }
  net::NetworkInterfaceList network_interfaces;
  GetNetworkList(&network_interfaces, 0);
  // VPNs use a "tun" interface, so the presence of a "tun" interface indicates
  // a VPN is in use. This logic only works on Android and Linux platforms.
  // Data Saver will not be disabled on any other platform on VPN.
  const std::string vpn_interface_name_prefix = "tun";
  for (size_t i = 0; i < network_interfaces.size(); ++i) {
    if (base::StartsWith(network_interfaces[i].name, vpn_interface_name_prefix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      disabled_on_vpn_ = true;
      ReloadConfig();
      RecordNetworkChangeEvent(DISABLED_ON_VPN);
      return true;
    }
  }
  if (disabled_on_vpn_) {
    disabled_on_vpn_ = false;
    ReloadConfig();
  }
  return false;
}

}  // namespace data_reduction_proxy
