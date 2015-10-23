// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/pref_proxy_config_tracker_impl.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "net/proxy/proxy_list.h"
#include "net/proxy/proxy_server.h"

using content::BrowserThread;

namespace {

// Determine if |proxy| is of the form "*.googlezip.net".
bool IsGooglezipDataReductionProxy(const net::ProxyServer& proxy) {
  return proxy.is_valid() && !proxy.is_direct() &&
         base::EndsWith(proxy.host_port_pair().host(), ".googlezip.net",
                        base::CompareCase::SENSITIVE);
}

// Removes any Data Reduction Proxies like *.googlezip.net from |proxy_list|.
// Returns the number of proxies that were removed from |proxy_list|.
size_t RemoveGooglezipDataReductionProxiesFromList(net::ProxyList* proxy_list) {
  bool found_googlezip_proxy = false;
  for (const net::ProxyServer& proxy : proxy_list->GetAll()) {
    if (IsGooglezipDataReductionProxy(proxy)) {
      found_googlezip_proxy = true;
      break;
    }
  }
  if (!found_googlezip_proxy)
    return 0;

  size_t num_removed_proxies = 0;
  net::ProxyList replacement_list;
  for (const net::ProxyServer& proxy : proxy_list->GetAll()) {
    if (!IsGooglezipDataReductionProxy(proxy))
      replacement_list.AddProxyServer(proxy);
    else
      ++num_removed_proxies;
  }

  if (replacement_list.IsEmpty())
    replacement_list.AddProxyServer(net::ProxyServer::Direct());
  *proxy_list = replacement_list;
  return num_removed_proxies;
}

// Remove any Data Reduction Proxies like *.googlezip.net from |proxy_rules|.
// This is to prevent a Data Reduction Proxy from being activated in an
// unsupported way, such as from a proxy pref, which could cause Chrome to use
// the Data Reduction Proxy without adding any of the necessary authentication
// headers or applying the Data Reduction Proxy bypass logic. See
// http://crbug.com/476610.
// TODO(sclittle): This method should be removed once the UMA indicates that
// *.googlezip.net proxies are no longer present in the |proxy_rules|.
void RemoveGooglezipDataReductionProxies(
    net::ProxyConfig::ProxyRules* proxy_rules) {
  size_t num_removed_proxies =
      RemoveGooglezipDataReductionProxiesFromList(
          &proxy_rules->fallback_proxies) +
      RemoveGooglezipDataReductionProxiesFromList(
          &proxy_rules->proxies_for_ftp) +
      RemoveGooglezipDataReductionProxiesFromList(
          &proxy_rules->proxies_for_http) +
      RemoveGooglezipDataReductionProxiesFromList(
          &proxy_rules->proxies_for_https) +
      RemoveGooglezipDataReductionProxiesFromList(&proxy_rules->single_proxies);

  UMA_HISTOGRAM_COUNTS_100("Net.PrefProxyConfig.GooglezipProxyRemovalCount",
                           num_removed_proxies);
}

}  // namespace

//============================= ChromeProxyConfigService =======================

ChromeProxyConfigService::ChromeProxyConfigService(
    net::ProxyConfigService* base_service)
    : base_service_(base_service),
      pref_config_state_(ProxyPrefs::CONFIG_UNSET),
      pref_config_read_pending_(true),
      registered_observer_(false) {
}

ChromeProxyConfigService::~ChromeProxyConfigService() {
  if (registered_observer_ && base_service_.get())
    base_service_->RemoveObserver(this);
}

void ChromeProxyConfigService::AddObserver(
    net::ProxyConfigService::Observer* observer) {
  RegisterObserver();
  observers_.AddObserver(observer);
}

void ChromeProxyConfigService::RemoveObserver(
    net::ProxyConfigService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

net::ProxyConfigService::ConfigAvailability
    ChromeProxyConfigService::GetLatestProxyConfig(net::ProxyConfig* config) {
  RegisterObserver();

  if (pref_config_read_pending_)
    return net::ProxyConfigService::CONFIG_PENDING;

  // Ask the base service if available.
  net::ProxyConfig system_config;
  ConfigAvailability system_availability =
      net::ProxyConfigService::CONFIG_UNSET;
  if (base_service_.get())
    system_availability = base_service_->GetLatestProxyConfig(&system_config);

  ProxyPrefs::ConfigState config_state;
  return PrefProxyConfigTrackerImpl::GetEffectiveProxyConfig(
      pref_config_state_, pref_config_,
      system_availability, system_config, false,
      &config_state, config);
}

void ChromeProxyConfigService::OnLazyPoll() {
  if (base_service_.get())
    base_service_->OnLazyPoll();
}

void ChromeProxyConfigService::UpdateProxyConfig(
    ProxyPrefs::ConfigState config_state,
    const net::ProxyConfig& config) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  pref_config_read_pending_ = false;
  pref_config_state_ = config_state;
  pref_config_ = config;

  if (!observers_.might_have_observers())
    return;

  // Evaluate the proxy configuration. If GetLatestProxyConfig returns
  // CONFIG_PENDING, we are using the system proxy service, but it doesn't have
  // a valid configuration yet. Once it is ready, OnProxyConfigChanged() will be
  // called and broadcast the proxy configuration.
  // Note: If a switch between a preference proxy configuration and the system
  // proxy configuration occurs an unnecessary notification might get send if
  // the two configurations agree. This case should be rare however, so we don't
  // handle that case specially.
  net::ProxyConfig new_config;
  ConfigAvailability availability = GetLatestProxyConfig(&new_config);
  if (availability != CONFIG_PENDING) {
    FOR_EACH_OBSERVER(net::ProxyConfigService::Observer, observers_,
                      OnProxyConfigChanged(new_config, availability));
  }
}

void ChromeProxyConfigService::OnProxyConfigChanged(
    const net::ProxyConfig& config,
    ConfigAvailability availability) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Check whether there is a proxy configuration defined by preferences. In
  // this case that proxy configuration takes precedence and the change event
  // from the delegate proxy service can be disregarded.
  if (!PrefProxyConfigTrackerImpl::PrefPrecedes(pref_config_state_)) {
    net::ProxyConfig actual_config;
    availability = GetLatestProxyConfig(&actual_config);
    FOR_EACH_OBSERVER(net::ProxyConfigService::Observer, observers_,
                      OnProxyConfigChanged(actual_config, availability));
  }
}

void ChromeProxyConfigService::RegisterObserver() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!registered_observer_ && base_service_.get()) {
    base_service_->AddObserver(this);
    registered_observer_ = true;
  }
}

//========================= PrefProxyConfigTrackerImpl =========================

PrefProxyConfigTrackerImpl::PrefProxyConfigTrackerImpl(
    PrefService* pref_service)
    : pref_service_(pref_service),
      chrome_proxy_config_service_(NULL),
      update_pending_(true) {
  config_state_ = ReadPrefConfig(pref_service_, &pref_config_);
  proxy_prefs_.Init(pref_service);
  proxy_prefs_.Add(prefs::kProxy,
                   base::Bind(&PrefProxyConfigTrackerImpl::OnProxyPrefChanged,
                              base::Unretained(this)));
}

PrefProxyConfigTrackerImpl::~PrefProxyConfigTrackerImpl() {
  DCHECK(pref_service_ == NULL);
}

scoped_ptr<net::ProxyConfigService>
PrefProxyConfigTrackerImpl::CreateTrackingProxyConfigService(
    scoped_ptr<net::ProxyConfigService> base_service) {
  chrome_proxy_config_service_ =
      new ChromeProxyConfigService(base_service.release());
  VLOG(1) << this << ": set chrome proxy config service to "
          << chrome_proxy_config_service_;
  if (chrome_proxy_config_service_ && update_pending_)
    OnProxyConfigChanged(config_state_, pref_config_);

  return scoped_ptr<net::ProxyConfigService>(chrome_proxy_config_service_);
}

void PrefProxyConfigTrackerImpl::DetachFromPrefService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Stop notifications.
  proxy_prefs_.RemoveAll();
  pref_service_ = NULL;
  chrome_proxy_config_service_ = NULL;
}

// static
bool PrefProxyConfigTrackerImpl::PrefPrecedes(
    ProxyPrefs::ConfigState config_state) {
  return config_state == ProxyPrefs::CONFIG_POLICY ||
         config_state == ProxyPrefs::CONFIG_EXTENSION ||
         config_state == ProxyPrefs::CONFIG_OTHER_PRECEDE;
}

// static
net::ProxyConfigService::ConfigAvailability
    PrefProxyConfigTrackerImpl::GetEffectiveProxyConfig(
        ProxyPrefs::ConfigState pref_state,
        const net::ProxyConfig& pref_config,
        net::ProxyConfigService::ConfigAvailability system_availability,
        const net::ProxyConfig& system_config,
        bool ignore_fallback_config,
        ProxyPrefs::ConfigState* effective_config_state,
        net::ProxyConfig* effective_config) {
  net::ProxyConfigService::ConfigAvailability rv;
  *effective_config_state = pref_state;

  if (PrefPrecedes(pref_state)) {
    *effective_config = pref_config;
    rv = net::ProxyConfigService::CONFIG_VALID;
  } else if (system_availability == net::ProxyConfigService::CONFIG_UNSET) {
    // If there's no system proxy config, fall back to prefs or default.
    if (pref_state == ProxyPrefs::CONFIG_FALLBACK && !ignore_fallback_config)
      *effective_config = pref_config;
    else
      *effective_config = net::ProxyConfig::CreateDirect();
    rv = net::ProxyConfigService::CONFIG_VALID;
  } else {
    *effective_config_state = ProxyPrefs::CONFIG_SYSTEM;
    *effective_config = system_config;
    rv = system_availability;
  }

  // Remove any Data Reduction Proxies like *.googlezip.net from the proxy
  // config rules, since specifying a DRP in the proxy rules is not a supported
  // means of activating the DRP, and could cause requests to be sent to the DRP
  // without the appropriate authentication headers and without using any of the
  // DRP bypass logic. This prevents the Data Reduction Proxy from being
  // improperly activated via the proxy pref.
  // TODO(sclittle): This is a temporary fix for http://crbug.com/476610, and
  // should be removed once that bug is fixed and verified.
  if (rv == net::ProxyConfigService::CONFIG_VALID)
    RemoveGooglezipDataReductionProxies(&effective_config->proxy_rules());

  return rv;
}

// static
void PrefProxyConfigTrackerImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  base::DictionaryValue* default_settings =
      ProxyConfigDictionary::CreateSystem();
  registry->RegisterDictionaryPref(prefs::kProxy, default_settings);
}

// static
void PrefProxyConfigTrackerImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* pref_service) {
  base::DictionaryValue* default_settings =
      ProxyConfigDictionary::CreateSystem();
  pref_service->RegisterDictionaryPref(prefs::kProxy, default_settings);
}

// static
ProxyPrefs::ConfigState PrefProxyConfigTrackerImpl::ReadPrefConfig(
    const PrefService* pref_service,
    net::ProxyConfig* config) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Clear the configuration and source.
  *config = net::ProxyConfig();
  ProxyPrefs::ConfigState config_state = ProxyPrefs::CONFIG_UNSET;

  const PrefService::Preference* pref =
      pref_service->FindPreference(prefs::kProxy);
  DCHECK(pref);

  const base::DictionaryValue* dict =
      pref_service->GetDictionary(prefs::kProxy);
  DCHECK(dict);
  ProxyConfigDictionary proxy_dict(dict);

  if (PrefConfigToNetConfig(proxy_dict, config)) {
    if (!pref->IsUserModifiable() || pref->HasUserSetting()) {
      if (pref->IsManaged())
        config_state = ProxyPrefs::CONFIG_POLICY;
      else if (pref->IsExtensionControlled())
        config_state = ProxyPrefs::CONFIG_EXTENSION;
      else
        config_state = ProxyPrefs::CONFIG_OTHER_PRECEDE;
    } else  {
      config_state = ProxyPrefs::CONFIG_FALLBACK;
    }
  }

  return config_state;
}

ProxyPrefs::ConfigState PrefProxyConfigTrackerImpl::GetProxyConfig(
    net::ProxyConfig* config) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (config_state_ != ProxyPrefs::CONFIG_UNSET)
    *config = pref_config_;
  return config_state_;
}

void PrefProxyConfigTrackerImpl::OnProxyConfigChanged(
    ProxyPrefs::ConfigState config_state,
    const net::ProxyConfig& config) {
  if (!chrome_proxy_config_service_) {
    VLOG(1) << "No chrome proxy config service to push to UpdateProxyConfig";
    update_pending_ = true;
    return;
  }
  update_pending_ = !BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ChromeProxyConfigService::UpdateProxyConfig,
                 base::Unretained(chrome_proxy_config_service_),
                 config_state, config));
  VLOG(1) << this << (update_pending_ ? ": Error" : ": Done")
          << " pushing proxy to UpdateProxyConfig";
}

bool PrefProxyConfigTrackerImpl::PrefConfigToNetConfig(
    const ProxyConfigDictionary& proxy_dict,
    net::ProxyConfig* config) {
  ProxyPrefs::ProxyMode mode;
  if (!proxy_dict.GetMode(&mode)) {
    // Fall back to system settings if the mode preference is invalid.
    return false;
  }

  switch (mode) {
    case ProxyPrefs::MODE_SYSTEM:
      // Use system settings.
      return false;
    case ProxyPrefs::MODE_DIRECT:
      // Ignore all the other proxy config preferences if the use of a proxy
      // has been explicitly disabled.
      return true;
    case ProxyPrefs::MODE_AUTO_DETECT:
      config->set_auto_detect(true);
      return true;
    case ProxyPrefs::MODE_PAC_SCRIPT: {
      std::string proxy_pac;
      if (!proxy_dict.GetPacUrl(&proxy_pac)) {
        LOG(ERROR) << "Proxy settings request PAC script but do not specify "
                   << "its URL. Falling back to direct connection.";
        return true;
      }
      GURL proxy_pac_url(proxy_pac);
      if (!proxy_pac_url.is_valid()) {
        LOG(ERROR) << "Invalid proxy PAC url: " << proxy_pac;
        return true;
      }
      config->set_pac_url(proxy_pac_url);
      bool pac_mandatory = false;
      proxy_dict.GetPacMandatory(&pac_mandatory);
      config->set_pac_mandatory(pac_mandatory);
      return true;
    }
    case ProxyPrefs::MODE_FIXED_SERVERS: {
      std::string proxy_server;
      if (!proxy_dict.GetProxyServer(&proxy_server)) {
        LOG(ERROR) << "Proxy settings request fixed proxy servers but do not "
                   << "specify their URLs. Falling back to direct connection.";
        return true;
      }
      config->proxy_rules().ParseFromString(proxy_server);

      std::string proxy_bypass;
      if (proxy_dict.GetBypassList(&proxy_bypass)) {
        config->proxy_rules().bypass_rules.ParseFromString(proxy_bypass);
      }
      return true;
    }
    case ProxyPrefs::kModeCount: {
      // Fall through to NOTREACHED().
    }
  }
  NOTREACHED() << "Unknown proxy mode, falling back to system settings.";
  return false;
}

void PrefProxyConfigTrackerImpl::OnProxyPrefChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  net::ProxyConfig new_config;
  ProxyPrefs::ConfigState config_state = ReadPrefConfig(pref_service_,
                                                        &new_config);
  if (config_state_ != config_state ||
      (config_state_ != ProxyPrefs::CONFIG_UNSET &&
       !pref_config_.Equals(new_config))) {
    config_state_ = config_state;
    if (config_state_ != ProxyPrefs::CONFIG_UNSET)
      pref_config_ = new_config;
    update_pending_ = true;
  }
  if (update_pending_)
    OnProxyConfigChanged(config_state, new_config);
}
