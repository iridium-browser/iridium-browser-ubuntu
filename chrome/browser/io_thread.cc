// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/io_thread.h"

#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/leak_tracker.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/profiler/scoped_tracker.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread.h"
#include "base/threading/worker_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/async_dns_field_trial.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/connect_interceptor.h"
#include "chrome/browser/net/dns_probe_service.h"
#include "chrome/browser/net/pref_proxy_config_tracker.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/policy/core/common/policy_service.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "net/base/host_mapping_rules.h"
#include "net/base/net_util.h"
#include "net/base/sdch_manager.h"
#include "net/cert/cert_policy_enforcer.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/ct_known_logs.h"
#include "net/cert/ct_known_logs_static.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cert/multi_threaded_cert_verifier.h"
#include "net/cookies/cookie_store.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_filter.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_server_properties_impl.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_script_fetcher_impl.h"
#include "net/proxy/proxy_service.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_utils.h"
#include "net/socket/tcp_client_socket.h"
#include "net/spdy/spdy_session.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/file_protocol_handler.h"
#include "net/url_request/ftp_protocol_handler.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_throttler_manager.h"
#include "url/url_constants.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "policy/policy_constants.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/event_router_forwarder.h"
#endif

#if defined(USE_NSS) || defined(OS_IOS)
#include "net/ocsp/nss_ocsp.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/net/cert_verify_proc_chromeos.h"
#include "chromeos/network/host_resolver_impl_chromeos.h"
#endif

using content::BrowserThread;

class SafeBrowsingURLRequestContext;

// The IOThread object must outlive any tasks posted to the IO thread before the
// Quit task, so base::Bind() calls are not refcounted.

namespace {

const char kTCPFastOpenFieldTrialName[] = "TCPFastOpen";
const char kTCPFastOpenHttpsEnabledGroupName[] = "HttpsEnabled";

const char kQuicFieldTrialName[] = "QUIC";
const char kQuicFieldTrialEnabledGroupName[] = "Enabled";
const char kQuicFieldTrialHttpsEnabledGroupName[] = "HttpsEnabled";

// The SPDY trial composes two different trial plus control groups:
//  * A "holdback" group with SPDY disabled, and corresponding control
//  (SPDY/3.1). The primary purpose of the holdback group is to encourage site
//  operators to do feature detection rather than UA-sniffing. As such, this
//  trial runs continuously.
//  * A SPDY/4 experiment, for SPDY/4 (aka HTTP/2) vs SPDY/3.1 comparisons and
//  eventual SPDY/4 deployment.
const char kSpdyFieldTrialName[] = "SPDY";
const char kSpdyFieldTrialHoldbackGroupNamePrefix[] = "SpdyDisabled";
const char kSpdyFieldTrialSpdy31GroupNamePrefix[] = "Spdy31Enabled";
const char kSpdyFieldTrialSpdy4GroupNamePrefix[] = "Spdy4Enabled";
const char kSpdyFieldTrialParametrizedPrefix[] = "Parametrized";

// Field trial for Cache-Control: stale-while-revalidate directive.
const char kStaleWhileRevalidateFieldTrialName[] = "StaleWhileRevalidate";

#if defined(OS_MACOSX) && !defined(OS_IOS)
void ObserveKeychainEvents() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  net::CertDatabase::GetInstance()->SetMessageLoopForKeychainEvents();
}
#endif

// Used for the "system" URLRequestContext.
class SystemURLRequestContext : public net::URLRequestContext {
 public:
  SystemURLRequestContext() {
#if defined(USE_NSS) || defined(OS_IOS)
    net::SetURLRequestContextForNSSHttpIO(this);
#endif
  }

 private:
  ~SystemURLRequestContext() override {
    AssertNoURLRequests();
#if defined(USE_NSS) || defined(OS_IOS)
    net::SetURLRequestContextForNSSHttpIO(NULL);
#endif
  }
};

scoped_ptr<net::HostResolver> CreateGlobalHostResolver(net::NetLog* net_log) {
  TRACE_EVENT0("startup", "IOThread::CreateGlobalHostResolver");
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  net::HostResolver::Options options;

  // Use the retry attempts override from the command-line, if any.
  if (command_line.HasSwitch(switches::kHostResolverRetryAttempts)) {
    std::string s =
        command_line.GetSwitchValueASCII(switches::kHostResolverRetryAttempts);
    // Parse the switch (it should be a non-negative integer).
    int n;
    if (base::StringToInt(s, &n) && n >= 0) {
      options.max_retry_attempts = static_cast<size_t>(n);
    } else {
      LOG(ERROR) << "Invalid switch for host resolver retry attempts: " << s;
    }
  }

  scoped_ptr<net::HostResolver> global_host_resolver;
#if defined OS_CHROMEOS
  global_host_resolver =
      chromeos::HostResolverImplChromeOS::CreateSystemResolver(options,
                                                               net_log);
#else
  global_host_resolver =
      net::HostResolver::CreateSystemResolver(options, net_log);
#endif

  // Determine if we should disable IPv6 support.
  if (command_line.HasSwitch(switches::kEnableIPv6)) {
    // Disable IPv6 probing.
    global_host_resolver->SetDefaultAddressFamily(
        net::ADDRESS_FAMILY_UNSPECIFIED);
  } else if (command_line.HasSwitch(switches::kDisableIPv6)) {
    global_host_resolver->SetDefaultAddressFamily(net::ADDRESS_FAMILY_IPV4);
  }

  // If hostname remappings were specified on the command-line, layer these
  // rules on top of the real host resolver. This allows forwarding all requests
  // through a designated test server.
  if (!command_line.HasSwitch(switches::kHostResolverRules))
    return global_host_resolver.Pass();

  scoped_ptr<net::MappedHostResolver> remapped_resolver(
      new net::MappedHostResolver(global_host_resolver.Pass()));
  remapped_resolver->SetRulesFromString(
      command_line.GetSwitchValueASCII(switches::kHostResolverRules));
  return remapped_resolver.Pass();
}

// TODO(willchan): Remove proxy script fetcher context since it's not necessary
// now that I got rid of refcounting URLRequestContexts.
// See IOThread::Globals for details.
net::URLRequestContext*
ConstructProxyScriptFetcherContext(IOThread::Globals* globals,
                                   net::NetLog* net_log) {
  net::URLRequestContext* context = new net::URLRequestContext;
  context->set_net_log(net_log);
  context->set_host_resolver(globals->host_resolver.get());
  context->set_cert_verifier(globals->cert_verifier.get());
  context->set_transport_security_state(
      globals->transport_security_state.get());
  context->set_cert_transparency_verifier(
      globals->cert_transparency_verifier.get());
  context->set_http_auth_handler_factory(
      globals->http_auth_handler_factory.get());
  context->set_proxy_service(globals->proxy_script_fetcher_proxy_service.get());
  context->set_http_transaction_factory(
      globals->proxy_script_fetcher_http_transaction_factory.get());
  context->set_job_factory(
      globals->proxy_script_fetcher_url_request_job_factory.get());
  context->set_cookie_store(globals->system_cookie_store.get());
  context->set_channel_id_service(
      globals->system_channel_id_service.get());
  context->set_network_delegate(globals->system_network_delegate.get());
  context->set_http_user_agent_settings(
      globals->http_user_agent_settings.get());
  // TODO(rtenneti): We should probably use HttpServerPropertiesManager for the
  // system URLRequestContext too. There's no reason this should be tied to a
  // profile.
  return context;
}

net::URLRequestContext*
ConstructSystemRequestContext(IOThread::Globals* globals,
                              net::NetLog* net_log) {
  net::URLRequestContext* context = new SystemURLRequestContext;
  context->set_net_log(net_log);
  context->set_host_resolver(globals->host_resolver.get());
  context->set_cert_verifier(globals->cert_verifier.get());
  context->set_transport_security_state(
      globals->transport_security_state.get());
  context->set_cert_transparency_verifier(
      globals->cert_transparency_verifier.get());
  context->set_http_auth_handler_factory(
      globals->http_auth_handler_factory.get());
  context->set_proxy_service(globals->system_proxy_service.get());
  context->set_http_transaction_factory(
      globals->system_http_transaction_factory.get());
  context->set_job_factory(globals->system_url_request_job_factory.get());
  context->set_cookie_store(globals->system_cookie_store.get());
  context->set_channel_id_service(
      globals->system_channel_id_service.get());
  context->set_throttler_manager(globals->throttler_manager.get());
  context->set_network_delegate(globals->system_network_delegate.get());
  context->set_http_user_agent_settings(
      globals->http_user_agent_settings.get());
  return context;
}

int GetSwitchValueAsInt(const base::CommandLine& command_line,
                        const std::string& switch_name) {
  int value;
  if (!base::StringToInt(command_line.GetSwitchValueASCII(switch_name),
                         &value)) {
    return 0;
  }
  return value;
}

// Returns the value associated with |key| in |params| or "" if the
// key is not present in the map.
const std::string& GetVariationParam(
    const std::map<std::string, std::string>& params,
    const std::string& key) {
  std::map<std::string, std::string>::const_iterator it = params.find(key);
  if (it == params.end())
    return base::EmptyString();

  return it->second;
}

// Return true if stale-while-revalidate support should be enabled.
bool IsStaleWhileRevalidateEnabled(const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kEnableStaleWhileRevalidate))
    return true;
  const std::string group_name =
      base::FieldTrialList::FindFullName(kStaleWhileRevalidateFieldTrialName);
  return group_name == "Enabled";
}

bool IsCertificateTransparencyRequiredForEV(
    const base::CommandLine& command_line) {
  const std::string group_name =
      base::FieldTrialList::FindFullName("CTRequiredForEVTrial");
  if (command_line.HasSwitch(
        switches::kDisableCertificateTransparencyRequirementForEV))
    return false;

  return group_name == "RequirementEnforced";
}

// Parse kUseSpdy command line flag options, which may contain the following:
//
//   "off"                      : Disables SPDY support entirely.
//   "ssl"                      : Forces SPDY for all HTTPS requests.
//   "no-ssl"                   : Forces SPDY for all HTTP requests.
//   "no-ping"                  : Disables SPDY ping connection testing.
//   "exclude=<host>"           : Disables SPDY support for the host <host>.
//   "no-compress"              : Disables SPDY header compression.
//   "no-alt-protocols          : Disables alternate protocol support.
//   "force-alt-protocols       : Forces an alternate protocol of SPDY/3
//                                on port 443.
//   "single-domain"            : Forces all spdy traffic to a single domain.
//   "init-max-streams=<limit>" : Specifies the maximum number of concurrent
//                                streams for a SPDY session, unless the
//                                specifies a different value via SETTINGS.
void ConfigureSpdyGlobalsFromUseSpdyArgument(const std::string& mode,
                                             IOThread::Globals* globals) {
  static const char kOff[] = "off";
  static const char kSSL[] = "ssl";
  static const char kDisableSSL[] = "no-ssl";
  static const char kDisablePing[] = "no-ping";
  static const char kExclude[] = "exclude";  // Hosts to exclude
  static const char kDisableCompression[] = "no-compress";
  static const char kDisableAltProtocols[] = "no-alt-protocols";
  static const char kSingleDomain[] = "single-domain";

  static const char kInitialMaxConcurrentStreams[] = "init-max-streams";

  std::vector<std::string> spdy_options;
  base::SplitString(mode, ',', &spdy_options);

  for (const std::string& element : spdy_options) {
    std::vector<std::string> name_value;
    base::SplitString(element, '=', &name_value);
    const std::string& option =
        name_value.size() > 0 ? name_value[0] : std::string();
    const std::string value =
        name_value.size() > 1 ? name_value[1] : std::string();

    if (option == kOff) {
      net::HttpStreamFactory::set_spdy_enabled(false);
      continue;
    }
    if (option == kDisableSSL) {
      globals->spdy_default_protocol.set(net::kProtoSPDY31);
      globals->force_spdy_over_ssl.set(false);
      globals->force_spdy_always.set(true);
      continue;
    }
    if (option == kSSL) {
      globals->spdy_default_protocol.set(net::kProtoSPDY31);
      globals->force_spdy_over_ssl.set(true);
      globals->force_spdy_always.set(true);
      continue;
    }
    if (option == kDisablePing) {
      globals->enable_spdy_ping_based_connection_checking.set(false);
      continue;
    }
    if (option == kExclude) {
      globals->forced_spdy_exclusions.insert(
          net::HostPortPair::FromURL(GURL(value)));
      continue;
    }
    if (option == kDisableCompression) {
      globals->enable_spdy_compression.set(false);
      continue;
    }
    if (option == kDisableAltProtocols) {
      globals->use_alternate_protocols.set(false);
      continue;
    }
    if (option == kSingleDomain) {
      DVLOG(1) << "FORCING SINGLE DOMAIN";
      globals->force_spdy_single_domain.set(true);
      continue;
    }
    if (option == kInitialMaxConcurrentStreams) {
      int streams;
      if (base::StringToInt(value, &streams)) {
        globals->initial_max_spdy_concurrent_streams.set(streams);
        continue;
      }
    }
    LOG(DFATAL) << "Unrecognized spdy option: " << option;
  }
}

}  // namespace

class IOThread::LoggingNetworkChangeObserver
    : public net::NetworkChangeNotifier::IPAddressObserver,
      public net::NetworkChangeNotifier::ConnectionTypeObserver,
      public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  // |net_log| must remain valid throughout our lifetime.
  explicit LoggingNetworkChangeObserver(net::NetLog* net_log)
      : net_log_(net_log) {
    net::NetworkChangeNotifier::AddIPAddressObserver(this);
    net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
    net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  }

  ~LoggingNetworkChangeObserver() override {
    net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
    net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
    net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  }

  // NetworkChangeNotifier::IPAddressObserver implementation.
  void OnIPAddressChanged() override {
    VLOG(1) << "Observed a change to the network IP addresses";

    net_log_->AddGlobalEntry(net::NetLog::TYPE_NETWORK_IP_ADDRESSES_CHANGED);
  }

  // NetworkChangeNotifier::ConnectionTypeObserver implementation.
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override {
    std::string type_as_string =
        net::NetworkChangeNotifier::ConnectionTypeToString(type);

    VLOG(1) << "Observed a change to network connectivity state "
            << type_as_string;

    net_log_->AddGlobalEntry(
        net::NetLog::TYPE_NETWORK_CONNECTIVITY_CHANGED,
        net::NetLog::StringCallback("new_connection_type", &type_as_string));
  }

  // NetworkChangeNotifier::NetworkChangeObserver implementation.
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override {
    std::string type_as_string =
        net::NetworkChangeNotifier::ConnectionTypeToString(type);

    VLOG(1) << "Observed a network change to state " << type_as_string;

    net_log_->AddGlobalEntry(
        net::NetLog::TYPE_NETWORK_CHANGED,
        net::NetLog::StringCallback("new_connection_type", &type_as_string));
  }

 private:
  net::NetLog* net_log_;
  DISALLOW_COPY_AND_ASSIGN(LoggingNetworkChangeObserver);
};

class SystemURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  explicit SystemURLRequestContextGetter(IOThread* io_thread);

  // Implementation for net::UrlRequestContextGetter.
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

 protected:
  ~SystemURLRequestContextGetter() override;

 private:
  IOThread* const io_thread_;  // Weak pointer, owned by BrowserProcess.
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  base::debug::LeakTracker<SystemURLRequestContextGetter> leak_tracker_;
};

SystemURLRequestContextGetter::SystemURLRequestContextGetter(
    IOThread* io_thread)
    : io_thread_(io_thread),
      network_task_runner_(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)) {
}

SystemURLRequestContextGetter::~SystemURLRequestContextGetter() {}

net::URLRequestContext* SystemURLRequestContextGetter::GetURLRequestContext() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(io_thread_->globals()->system_request_context.get());

  return io_thread_->globals()->system_request_context.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
SystemURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

IOThread::Globals::
SystemRequestContextLeakChecker::SystemRequestContextLeakChecker(
    Globals* globals)
    : globals_(globals) {
  DCHECK(globals_);
}

IOThread::Globals::
SystemRequestContextLeakChecker::~SystemRequestContextLeakChecker() {
  if (globals_->system_request_context.get())
    globals_->system_request_context->AssertNoURLRequests();
}

IOThread::Globals::Globals()
    : system_request_context_leak_checker(this),
      ignore_certificate_errors(false),
      use_stale_while_revalidate(false),
      testing_fixed_http_port(0),
      testing_fixed_https_port(0),
      enable_user_alternate_protocol_ports(false) {
}

IOThread::Globals::~Globals() {}

// |local_state| is passed in explicitly in order to (1) reduce implicit
// dependencies and (2) make IOThread more flexible for testing.
IOThread::IOThread(
    PrefService* local_state,
    policy::PolicyService* policy_service,
    ChromeNetLog* net_log,
    extensions::EventRouterForwarder* extension_event_router_forwarder)
    : net_log_(net_log),
#if defined(ENABLE_EXTENSIONS)
      extension_event_router_forwarder_(extension_event_router_forwarder),
#endif
      globals_(NULL),
      is_spdy_disabled_by_policy_(false),
      is_quic_allowed_by_policy_(true),
      creation_time_(base::TimeTicks::Now()),
      weak_factory_(this) {
  auth_schemes_ = local_state->GetString(prefs::kAuthSchemes);
  negotiate_disable_cname_lookup_ = local_state->GetBoolean(
      prefs::kDisableAuthNegotiateCnameLookup);
  negotiate_enable_port_ = local_state->GetBoolean(
      prefs::kEnableAuthNegotiatePort);
  auth_server_whitelist_ = local_state->GetString(prefs::kAuthServerWhitelist);
  auth_delegate_whitelist_ = local_state->GetString(
      prefs::kAuthNegotiateDelegateWhitelist);
  gssapi_library_name_ = local_state->GetString(prefs::kGSSAPILibraryName);
  pref_proxy_config_tracker_.reset(
      ProxyServiceFactory::CreatePrefProxyConfigTrackerOfLocalState(
          local_state));
  ChromeNetworkDelegate::InitializePrefsOnUIThread(
      &system_enable_referrers_,
      NULL,
      NULL,
      NULL,
      NULL,
      local_state);
  ssl_config_service_manager_.reset(
      SSLConfigServiceManager::CreateDefaultManager(local_state));

  base::Value* dns_client_enabled_default = new base::FundamentalValue(
      chrome_browser_net::ConfigureAsyncDnsFieldTrial());
  local_state->SetDefaultPrefValue(prefs::kBuiltInDnsClientEnabled,
                                   dns_client_enabled_default);
  chrome_browser_net::LogAsyncDnsPrefSource(
      local_state->FindPreference(prefs::kBuiltInDnsClientEnabled));

  dns_client_enabled_.Init(prefs::kBuiltInDnsClientEnabled,
                           local_state,
                           base::Bind(&IOThread::UpdateDnsClientEnabled,
                                      base::Unretained(this)));
  dns_client_enabled_.MoveToThread(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));

  quick_check_enabled_.Init(prefs::kQuickCheckEnabled,
                            local_state);
  quick_check_enabled_.MoveToThread(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));

#if defined(ENABLE_CONFIGURATION_POLICY)
  is_spdy_disabled_by_policy_ = policy_service->GetPolicies(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string())).Get(
          policy::key::kDisableSpdy) != NULL;

  const base::Value* value = policy_service->GetPolicies(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
      std::string())).GetValue(policy::key::kQuicAllowed);
  if (value)
    value->GetAsBoolean(&is_quic_allowed_by_policy_);
#endif  // ENABLE_CONFIGURATION_POLICY

  BrowserThread::SetDelegate(BrowserThread::IO, this);
}

IOThread::~IOThread() {
  // This isn't needed for production code, but in tests, IOThread may
  // be multiply constructed.
  BrowserThread::SetDelegate(BrowserThread::IO, NULL);

  pref_proxy_config_tracker_->DetachFromPrefService();
  DCHECK(!globals_);
}

IOThread::Globals* IOThread::globals() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return globals_;
}

void IOThread::SetGlobalsForTesting(Globals* globals) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!globals || !globals_);
  globals_ = globals;
}

ChromeNetLog* IOThread::net_log() {
  return net_log_;
}

void IOThread::ChangedToOnTheRecord() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&IOThread::ChangedToOnTheRecordOnIOThread,
                 base::Unretained(this)));
}

net::URLRequestContextGetter* IOThread::system_url_request_context_getter() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!system_url_request_context_getter_.get()) {
    InitSystemRequestContext();
  }
  return system_url_request_context_getter_.get();
}

void IOThread::Init() {
  // Prefer to use InitAsync unless you need initialization to block
  // the UI thread
}

void IOThread::InitAsync() {
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION("466432 IOThread::InitAsync::Start"));
  TRACE_EVENT0("startup", "IOThread::InitAsync");
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

#if defined(USE_NSS) || defined(OS_IOS)
  net::SetMessageLoopForNSSHttpIO();
#endif

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile2(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::CommandLineForCurrentProcess"));
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  DCHECK(!globals_);
  globals_ = new Globals;

  // Add an observer that will emit network change events to the ChromeNetLog.
  // Assuming NetworkChangeNotifier dispatches in FIFO order, we should be
  // logging the network change before other IO thread consumers respond to it.
  network_change_observer_.reset(
      new LoggingNetworkChangeObserver(net_log_));

  // Setup the HistogramWatcher to run on the IO thread.
  net::NetworkChangeNotifier::InitHistogramWatcher();

#if defined(ENABLE_EXTENSIONS)
  globals_->extension_event_router_forwarder =
      extension_event_router_forwarder_;
#endif

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile3(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::ChromeNetworkDelegate"));
  scoped_ptr<ChromeNetworkDelegate> chrome_network_delegate(
      new ChromeNetworkDelegate(extension_event_router_forwarder(),
                                &system_enable_referrers_));

#if defined(ENABLE_EXTENSIONS)
  if (command_line.HasSwitch(switches::kDisableExtensionsHttpThrottling))
    chrome_network_delegate->NeverThrottleRequests();
#endif

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile4(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::CreateGlobalHostResolver"));
  globals_->system_network_delegate = chrome_network_delegate.Pass();
  globals_->host_resolver = CreateGlobalHostResolver(net_log_);
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile5(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::UpdateDnsClientEnabled::Start"));
  UpdateDnsClientEnabled();
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile6(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::UpdateDnsClientEnabled::End"));
#if defined(OS_CHROMEOS)
  // Creates a CertVerifyProc that doesn't allow any profile-provided certs.
  globals_->cert_verifier.reset(new net::MultiThreadedCertVerifier(
      new chromeos::CertVerifyProcChromeOS()));
#else
  globals_->cert_verifier.reset(new net::MultiThreadedCertVerifier(
      net::CertVerifyProc::CreateDefault()));
#endif

  globals_->transport_security_state.reset(new net::TransportSecurityState());

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile7(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::CreateMultiLogVerifier"));
  net::MultiLogCTVerifier* ct_verifier = new net::MultiLogCTVerifier();
  globals_->cert_transparency_verifier.reset(ct_verifier);

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile8(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::CreateLogVerifiers::Start"));
  // Add built-in logs
  ct_verifier->AddLogs(net::ct::CreateLogVerifiersForKnownLogs());
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile9(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::CreateLogVerifiers::End"));

  // Add logs from command line
  if (command_line.HasSwitch(switches::kCertificateTransparencyLog)) {
    std::string switch_value = command_line.GetSwitchValueASCII(
        switches::kCertificateTransparencyLog);
    std::vector<std::string> logs;
    base::SplitString(switch_value, ',', &logs);
    for (std::vector<std::string>::iterator it = logs.begin(); it != logs.end();
         ++it) {
      const std::string& curr_log = *it;
      size_t delim_pos = curr_log.find(":");
      CHECK(delim_pos != std::string::npos)
          << "CT log description not provided (switch format"
             " is 'description:base64_key')";
      std::string log_description(curr_log.substr(0, delim_pos));
      std::string ct_public_key_data;
      CHECK(base::Base64Decode(curr_log.substr(delim_pos + 1),
                               &ct_public_key_data))
          << "Unable to decode CT public key.";
      scoped_ptr<net::CTLogVerifier> external_log_verifier(
          net::CTLogVerifier::Create(ct_public_key_data, log_description));
      CHECK(external_log_verifier) << "Unable to parse CT public key.";
      VLOG(1) << "Adding log with description " << log_description;
      ct_verifier->AddLog(external_log_verifier.Pass());
    }
  }

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile10(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::CertPolicyEnforcer"));
  net::CertPolicyEnforcer* policy_enforcer = NULL;
  policy_enforcer = new net::CertPolicyEnforcer(
      IsCertificateTransparencyRequiredForEV(command_line));
  globals_->cert_policy_enforcer.reset(policy_enforcer);

  globals_->ssl_config_service = GetSSLConfigService();

  globals_->http_auth_handler_factory.reset(CreateDefaultAuthHandlerFactory(
      globals_->host_resolver.get()));
  globals_->http_server_properties.reset(new net::HttpServerPropertiesImpl());
  // For the ProxyScriptFetcher, we use a direct ProxyService.
  globals_->proxy_script_fetcher_proxy_service.reset(
      net::ProxyService::CreateDirectWithNetLog(net_log_));
  // In-memory cookie store.
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile11(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::CreateCookieStore::Start"));
  globals_->system_cookie_store =
        content::CreateCookieStore(content::CookieStoreConfig());
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile12(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::CreateCookieStore::End"));
  // In-memory channel ID store.
  globals_->system_channel_id_service.reset(
      new net::ChannelIDService(
          new net::DefaultChannelIDStore(NULL),
          base::WorkerPool::GetTaskRunner(true)));
  globals_->dns_probe_service.reset(new chrome_browser_net::DnsProbeService());
  globals_->host_mapping_rules.reset(new net::HostMappingRules());
  globals_->http_user_agent_settings.reset(
      new net::StaticHttpUserAgentSettings(std::string(), GetUserAgent()));
  if (command_line.HasSwitch(switches::kHostRules)) {
    TRACE_EVENT_BEGIN0("startup", "IOThread::InitAsync:SetRulesFromString");
    globals_->host_mapping_rules->SetRulesFromString(
        command_line.GetSwitchValueASCII(switches::kHostRules));
    TRACE_EVENT_END0("startup", "IOThread::InitAsync:SetRulesFromString");
  }
  if (command_line.HasSwitch(switches::kIgnoreCertificateErrors))
    globals_->ignore_certificate_errors = true;
  globals_->use_stale_while_revalidate =
      IsStaleWhileRevalidateEnabled(command_line);
  if (command_line.HasSwitch(switches::kTestingFixedHttpPort)) {
    globals_->testing_fixed_http_port =
        GetSwitchValueAsInt(command_line, switches::kTestingFixedHttpPort);
  }
  if (command_line.HasSwitch(switches::kTestingFixedHttpsPort)) {
    globals_->testing_fixed_https_port =
        GetSwitchValueAsInt(command_line, switches::kTestingFixedHttpsPort);
  }
  ConfigureQuic(command_line);
  if (command_line.HasSwitch(
          switches::kEnableUserAlternateProtocolPorts)) {
    globals_->enable_user_alternate_protocol_ports = true;
  }
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile13(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::InitializeNetworkOptions"));
  InitializeNetworkOptions(command_line);

  net::HttpNetworkSession::Params session_params;
  InitializeNetworkSessionParams(&session_params);
  session_params.net_log = net_log_;
  session_params.proxy_service =
      globals_->proxy_script_fetcher_proxy_service.get();

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile14(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::HttpNetorkSession::Start"));
  TRACE_EVENT_BEGIN0("startup", "IOThread::InitAsync:HttpNetworkSession");
  scoped_refptr<net::HttpNetworkSession> network_session(
      new net::HttpNetworkSession(session_params));
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile15(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::HttpNetorkSession::End"));
  globals_->proxy_script_fetcher_http_transaction_factory
      .reset(new net::HttpNetworkLayer(network_session.get()));
  TRACE_EVENT_END0("startup", "IOThread::InitAsync:HttpNetworkSession");
  scoped_ptr<net::URLRequestJobFactoryImpl> job_factory(
      new net::URLRequestJobFactoryImpl());

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile16(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::SetProtocolHandler"));
  job_factory->SetProtocolHandler(url::kDataScheme,
                                  new net::DataProtocolHandler());
  job_factory->SetProtocolHandler(
      url::kFileScheme,
      new net::FileProtocolHandler(
          content::BrowserThread::GetBlockingPool()->
              GetTaskRunnerWithShutdownBehavior(
                  base::SequencedWorkerPool::SKIP_ON_SHUTDOWN)));
#if !defined(DISABLE_FTP_SUPPORT)
  globals_->proxy_script_fetcher_ftp_transaction_factory.reset(
      new net::FtpNetworkLayer(globals_->host_resolver.get()));
  job_factory->SetProtocolHandler(
      url::kFtpScheme,
      new net::FtpProtocolHandler(
          globals_->proxy_script_fetcher_ftp_transaction_factory.get()));
#endif
  globals_->proxy_script_fetcher_url_request_job_factory = job_factory.Pass();

  globals_->throttler_manager.reset(new net::URLRequestThrottlerManager());
  globals_->throttler_manager->set_net_log(net_log_);
  // Always done in production, disabled only for unit tests.
  globals_->throttler_manager->set_enable_thread_checks(true);

  globals_->proxy_script_fetcher_context.reset(
      ConstructProxyScriptFetcherContext(globals_, net_log_));

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Start observing Keychain events. This needs to be done on the UI thread,
  // as Keychain services requires a CFRunLoop.
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&ObserveKeychainEvents));
#endif

  // InitSystemRequestContext turns right around and posts a task back
  // to the IO thread, so we can't let it run until we know the IO
  // thread has started.
  //
  // Note that since we are at BrowserThread::Init time, the UI thread
  // is blocked waiting for the thread to start.  Therefore, posting
  // this task to the main thread's message loop here is guaranteed to
  // get it onto the message loop while the IOThread object still
  // exists.  However, the message might not be processed on the UI
  // thread until after IOThread is gone, so use a weak pointer.
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&IOThread::InitSystemRequestContext,
                                     weak_factory_.GetWeakPtr()));
}

void IOThread::CleanUp() {
  base::debug::LeakTracker<SafeBrowsingURLRequestContext>::CheckForLeaks();

#if defined(USE_NSS) || defined(OS_IOS)
  net::ShutdownNSSHttpIO();
#endif

  system_url_request_context_getter_ = NULL;

  // Release objects that the net::URLRequestContext could have been pointing
  // to.

  // Shutdown the HistogramWatcher on the IO thread.
  net::NetworkChangeNotifier::ShutdownHistogramWatcher();

  // This must be reset before the ChromeNetLog is destroyed.
  network_change_observer_.reset();

  system_proxy_config_service_.reset();

  delete globals_;
  globals_ = NULL;

  base::debug::LeakTracker<SystemURLRequestContextGetter>::CheckForLeaks();
}

void IOThread::InitializeNetworkOptions(const base::CommandLine& command_line) {
  // Only handle use-spdy command line flags if "spdy.disabled" preference is
  // not disabled via policy.
  if (is_spdy_disabled_by_policy_) {
    base::FieldTrial* trial = base::FieldTrialList::Find(kSpdyFieldTrialName);
    if (trial)
      trial->Disable();
  } else {
    std::string group = base::FieldTrialList::FindFullName(kSpdyFieldTrialName);
    VariationParameters params;
    if (!variations::GetVariationParams(kSpdyFieldTrialName, &params)) {
      params.clear();
    }
    ConfigureSpdyGlobals(command_line, group, params, globals_);
  }

  ConfigureTCPFastOpen(command_line);
  ConfigureSdch();

  // TODO(rch): Make the client socket factory a per-network session
  // instance, constructed from a NetworkSession::Params, to allow us
  // to move this option to IOThread::Globals &
  // HttpNetworkSession::Params.
}

void IOThread::ConfigureTCPFastOpen(const base::CommandLine& command_line) {
  const std::string trial_group =
      base::FieldTrialList::FindFullName(kTCPFastOpenFieldTrialName);
  if (trial_group == kTCPFastOpenHttpsEnabledGroupName)
    globals_->enable_tcp_fast_open_for_ssl.set(true);
  bool always_enable_if_supported =
      command_line.HasSwitch(switches::kEnableTcpFastOpen);
  // Check for OS support of TCP FastOpen, and turn it on for all connections
  // if indicated by user.
  net::CheckSupportAndMaybeEnableTCPFastOpen(always_enable_if_supported);
}

void IOThread::ConfigureSdch() {
  // Check SDCH field trial.  Default is now that everything is enabled,
  // so provide options for disabling HTTPS or all of SDCH.
  const char kSdchFieldTrialName[] = "SDCH";
  const char kEnabledHttpOnlyGroupName[] = "EnabledHttpOnly";
  const char kDisabledAllGroupName[] = "DisabledAll";

  // Store in a string on return to keep underlying storage for
  // StringPiece stable.
  std::string sdch_trial_group_string =
      base::FieldTrialList::FindFullName(kSdchFieldTrialName);
  base::StringPiece sdch_trial_group(sdch_trial_group_string);
  if (sdch_trial_group.starts_with(kEnabledHttpOnlyGroupName)) {
    net::SdchManager::EnableSdchSupport(true);
    net::SdchManager::EnableSecureSchemeSupport(false);
  } else if (sdch_trial_group.starts_with(kDisabledAllGroupName)) {
    net::SdchManager::EnableSdchSupport(false);
  }
}

// static
void IOThread::ConfigureSpdyGlobals(
    const base::CommandLine& command_line,
    base::StringPiece spdy_trial_group,
    const VariationParameters& spdy_trial_params,
    IOThread::Globals* globals) {
  if (command_line.HasSwitch(switches::kTrustedSpdyProxy)) {
    globals->trusted_spdy_proxy.set(
        command_line.GetSwitchValueASCII(switches::kTrustedSpdyProxy));
  }
  if (command_line.HasSwitch(switches::kIgnoreUrlFetcherCertRequests))
    net::URLFetcher::SetIgnoreCertificateRequests(true);

  if (command_line.HasSwitch(switches::kUseSpdy)) {
    std::string spdy_mode =
        command_line.GetSwitchValueASCII(switches::kUseSpdy);
    ConfigureSpdyGlobalsFromUseSpdyArgument(spdy_mode, globals);
    return;
  }

  globals->next_protos.clear();
  globals->next_protos.push_back(net::kProtoHTTP11);
  bool enable_quic = false;
  globals->enable_quic.CopyToIfSet(&enable_quic);
  if (enable_quic) {
    globals->next_protos.push_back(net::kProtoQUIC1SPDY3);
  }

  if (command_line.HasSwitch(switches::kEnableSpdy4)) {
    globals->next_protos.push_back(net::kProtoSPDY31);
    globals->next_protos.push_back(net::kProtoSPDY4_14);
    globals->next_protos.push_back(net::kProtoSPDY4);
    globals->use_alternate_protocols.set(true);
    return;
  }
  if (command_line.HasSwitch(switches::kEnableNpnHttpOnly)) {
    globals->use_alternate_protocols.set(false);
    return;
  }

  // No SPDY command-line flags have been specified. Examine trial groups.
  if (spdy_trial_group.starts_with(kSpdyFieldTrialHoldbackGroupNamePrefix)) {
    net::HttpStreamFactory::set_spdy_enabled(false);
    return;
  }
  if (spdy_trial_group.starts_with(kSpdyFieldTrialSpdy31GroupNamePrefix)) {
    globals->next_protos.push_back(net::kProtoSPDY31);
    globals->use_alternate_protocols.set(true);
    return;
  }
  if (spdy_trial_group.starts_with(kSpdyFieldTrialSpdy4GroupNamePrefix)) {
    globals->next_protos.push_back(net::kProtoSPDY31);
    globals->next_protos.push_back(net::kProtoSPDY4_14);
    globals->next_protos.push_back(net::kProtoSPDY4);
    globals->use_alternate_protocols.set(true);
    return;
  }
  if (spdy_trial_group.starts_with(kSpdyFieldTrialParametrizedPrefix)) {
    bool spdy_enabled = false;
    if (LowerCaseEqualsASCII(
            GetVariationParam(spdy_trial_params, "enable_spdy31"), "true")) {
      globals->next_protos.push_back(net::kProtoSPDY31);
      spdy_enabled = true;
    }
    if (LowerCaseEqualsASCII(
            GetVariationParam(spdy_trial_params, "enable_http2_14"), "true")) {
      globals->next_protos.push_back(net::kProtoSPDY4_14);
      spdy_enabled = true;
    }
    if (LowerCaseEqualsASCII(
            GetVariationParam(spdy_trial_params, "enable_http2"), "true")) {
      globals->next_protos.push_back(net::kProtoSPDY4);
      spdy_enabled = true;
    }
    // TODO(bnc): HttpStreamFactory::spdy_enabled_ is redundant with
    // globals->next_protos, can it be eliminated?
    net::HttpStreamFactory::set_spdy_enabled(spdy_enabled);
    globals->use_alternate_protocols.set(true);
    return;
  }

  // By default, enable HTTP/2.
  globals->next_protos.push_back(net::kProtoSPDY31);
  globals->next_protos.push_back(net::kProtoSPDY4_14);
  globals->next_protos.push_back(net::kProtoSPDY4);
  globals->use_alternate_protocols.set(true);
}

// static
void IOThread::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kAuthSchemes,
                               "basic,digest,ntlm,negotiate,"
                               "spdyproxy");
  registry->RegisterBooleanPref(prefs::kDisableAuthNegotiateCnameLookup, false);
  registry->RegisterBooleanPref(prefs::kEnableAuthNegotiatePort, false);
  registry->RegisterStringPref(prefs::kAuthServerWhitelist, std::string());
  registry->RegisterStringPref(prefs::kAuthNegotiateDelegateWhitelist,
                               std::string());
  registry->RegisterStringPref(prefs::kGSSAPILibraryName, std::string());
  registry->RegisterStringPref(
      data_reduction_proxy::prefs::kDataReductionProxy, std::string());
  registry->RegisterBooleanPref(prefs::kEnableReferrers, true);
  data_reduction_proxy::RegisterPrefs(registry);
  registry->RegisterBooleanPref(prefs::kBuiltInDnsClientEnabled, true);
  registry->RegisterBooleanPref(prefs::kQuickCheckEnabled, true);
}

net::HttpAuthHandlerFactory* IOThread::CreateDefaultAuthHandlerFactory(
    net::HostResolver* resolver) {
  net::HttpAuthFilterWhitelist* auth_filter_default_credentials = NULL;
  if (!auth_server_whitelist_.empty()) {
    auth_filter_default_credentials =
        new net::HttpAuthFilterWhitelist(auth_server_whitelist_);
  }
  net::HttpAuthFilterWhitelist* auth_filter_delegate = NULL;
  if (!auth_delegate_whitelist_.empty()) {
    auth_filter_delegate =
        new net::HttpAuthFilterWhitelist(auth_delegate_whitelist_);
  }
  globals_->url_security_manager.reset(
      net::URLSecurityManager::Create(auth_filter_default_credentials,
                                      auth_filter_delegate));
  std::vector<std::string> supported_schemes;
  base::SplitString(auth_schemes_, ',', &supported_schemes);

  scoped_ptr<net::HttpAuthHandlerRegistryFactory> registry_factory(
      net::HttpAuthHandlerRegistryFactory::Create(
          supported_schemes, globals_->url_security_manager.get(),
          resolver, gssapi_library_name_, negotiate_disable_cname_lookup_,
          negotiate_enable_port_));
  return registry_factory.release();
}

void IOThread::ClearHostCache() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  net::HostCache* host_cache = globals_->host_resolver->GetHostCache();
  if (host_cache)
    host_cache->clear();
}

void IOThread::InitializeNetworkSessionParams(
    net::HttpNetworkSession::Params* params) {
  InitializeNetworkSessionParamsFromGlobals(*globals_, params);
}

// static
void IOThread::InitializeNetworkSessionParamsFromGlobals(
    const IOThread::Globals& globals,
    net::HttpNetworkSession::Params* params) {
  params->host_resolver = globals.host_resolver.get();
  params->cert_verifier = globals.cert_verifier.get();
  params->cert_policy_enforcer = globals.cert_policy_enforcer.get();
  params->channel_id_service = globals.system_channel_id_service.get();
  params->transport_security_state = globals.transport_security_state.get();
  params->ssl_config_service = globals.ssl_config_service.get();
  params->http_auth_handler_factory = globals.http_auth_handler_factory.get();
  params->http_server_properties =
      globals.http_server_properties->GetWeakPtr();
  params->network_delegate = globals.system_network_delegate.get();
  params->host_mapping_rules = globals.host_mapping_rules.get();
  params->ignore_certificate_errors = globals.ignore_certificate_errors;
  params->use_stale_while_revalidate = globals.use_stale_while_revalidate;
  params->testing_fixed_http_port = globals.testing_fixed_http_port;
  params->testing_fixed_https_port = globals.testing_fixed_https_port;
  globals.enable_tcp_fast_open_for_ssl.CopyToIfSet(
      &params->enable_tcp_fast_open_for_ssl);

  globals.initial_max_spdy_concurrent_streams.CopyToIfSet(
      &params->spdy_initial_max_concurrent_streams);
  globals.force_spdy_single_domain.CopyToIfSet(
      &params->force_spdy_single_domain);
  globals.enable_spdy_compression.CopyToIfSet(
      &params->enable_spdy_compression);
  globals.enable_spdy_ping_based_connection_checking.CopyToIfSet(
      &params->enable_spdy_ping_based_connection_checking);
  globals.spdy_default_protocol.CopyToIfSet(
      &params->spdy_default_protocol);
  params->next_protos = globals.next_protos;
  globals.trusted_spdy_proxy.CopyToIfSet(&params->trusted_spdy_proxy);
  globals.force_spdy_over_ssl.CopyToIfSet(&params->force_spdy_over_ssl);
  globals.force_spdy_always.CopyToIfSet(&params->force_spdy_always);
  params->forced_spdy_exclusions = globals.forced_spdy_exclusions;
  globals.use_alternate_protocols.CopyToIfSet(
      &params->use_alternate_protocols);
  globals.alternate_protocol_probability_threshold.CopyToIfSet(
      &params->alternate_protocol_probability_threshold);

  globals.enable_quic.CopyToIfSet(&params->enable_quic);
  globals.enable_quic_for_proxies.CopyToIfSet(&params->enable_quic_for_proxies);
  globals.quic_always_require_handshake_confirmation.CopyToIfSet(
      &params->quic_always_require_handshake_confirmation);
  globals.quic_disable_connection_pooling.CopyToIfSet(
      &params->quic_disable_connection_pooling);
  globals.quic_load_server_info_timeout_srtt_multiplier.CopyToIfSet(
      &params->quic_load_server_info_timeout_srtt_multiplier);
  globals.quic_enable_connection_racing.CopyToIfSet(
      &params->quic_enable_connection_racing);
  globals.quic_enable_non_blocking_io.CopyToIfSet(
      &params->quic_enable_non_blocking_io);
  globals.quic_disable_disk_cache.CopyToIfSet(
      &params->quic_disable_disk_cache);
  globals.quic_max_number_of_lossy_connections.CopyToIfSet(
      &params->quic_max_number_of_lossy_connections);
  globals.quic_packet_loss_threshold.CopyToIfSet(
      &params->quic_packet_loss_threshold);
  globals.quic_socket_receive_buffer_size.CopyToIfSet(
      &params->quic_socket_receive_buffer_size);
  globals.enable_quic_port_selection.CopyToIfSet(
      &params->enable_quic_port_selection);
  globals.quic_max_packet_length.CopyToIfSet(&params->quic_max_packet_length);
  globals.quic_user_agent_id.CopyToIfSet(&params->quic_user_agent_id);
  globals.quic_supported_versions.CopyToIfSet(
      &params->quic_supported_versions);
  params->quic_connection_options = globals.quic_connection_options;

  globals.origin_to_force_quic_on.CopyToIfSet(
      &params->origin_to_force_quic_on);
  params->enable_user_alternate_protocol_ports =
      globals.enable_user_alternate_protocol_ports;
}

base::TimeTicks IOThread::creation_time() const {
  return creation_time_;
}

net::SSLConfigService* IOThread::GetSSLConfigService() {
  return ssl_config_service_manager_->Get();
}

void IOThread::ChangedToOnTheRecordOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Clear the host cache to avoid showing entries from the OTR session
  // in about:net-internals.
  ClearHostCache();
}

void IOThread::InitSystemRequestContext() {
  if (system_url_request_context_getter_.get())
    return;
  // If we're in unit_tests, IOThread may not be run.
  if (!BrowserThread::IsMessageLoopValid(BrowserThread::IO))
    return;
  system_proxy_config_service_.reset(
      ProxyServiceFactory::CreateProxyConfigService(
          pref_proxy_config_tracker_.get()));
  system_url_request_context_getter_ =
      new SystemURLRequestContextGetter(this);
  // Safe to post an unretained this pointer, since IOThread is
  // guaranteed to outlive the IO BrowserThread.
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&IOThread::InitSystemRequestContextOnIOThread,
                 base::Unretained(this)));
}

void IOThread::InitSystemRequestContextOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!globals_->system_proxy_service.get());
  DCHECK(system_proxy_config_service_.get());

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  globals_->system_proxy_service.reset(
      ProxyServiceFactory::CreateProxyService(
          net_log_,
          globals_->proxy_script_fetcher_context.get(),
          globals_->system_network_delegate.get(),
          system_proxy_config_service_.release(),
          command_line,
          quick_check_enabled_.GetValue()));

  net::HttpNetworkSession::Params system_params;
  InitializeNetworkSessionParams(&system_params);
  system_params.net_log = net_log_;
  system_params.proxy_service = globals_->system_proxy_service.get();

  globals_->system_http_transaction_factory.reset(
      new net::HttpNetworkLayer(
          new net::HttpNetworkSession(system_params)));
  globals_->system_url_request_job_factory.reset(
      new net::URLRequestJobFactoryImpl());
  globals_->system_request_context.reset(
      ConstructSystemRequestContext(globals_, net_log_));
  globals_->system_request_context->set_ssl_config_service(
      globals_->ssl_config_service.get());
  globals_->system_request_context->set_http_server_properties(
      globals_->http_server_properties->GetWeakPtr());
}

void IOThread::UpdateDnsClientEnabled() {
  globals()->host_resolver->SetDnsClientEnabled(*dns_client_enabled_);
}

void IOThread::ConfigureQuic(const base::CommandLine& command_line) {
  // Always fetch the field trial group to ensure it is reported correctly.
  // The command line flags will be associated with a group that is reported
  // so long as trial is actually queried.
  std::string group =
      base::FieldTrialList::FindFullName(kQuicFieldTrialName);
  VariationParameters params;
  if (!variations::GetVariationParams(kQuicFieldTrialName, &params)) {
    params.clear();
  }

  ConfigureQuicGlobals(command_line, group, params, is_quic_allowed_by_policy_,
                       globals_);
}

// static
void IOThread::ConfigureQuicGlobals(
    const base::CommandLine& command_line,
    base::StringPiece quic_trial_group,
    const VariationParameters& quic_trial_params,
    bool quic_allowed_by_policy,
    IOThread::Globals* globals) {
  bool enable_quic = ShouldEnableQuic(command_line, quic_trial_group,
                                      quic_allowed_by_policy);
  globals->enable_quic.set(enable_quic);
  bool enable_quic_for_proxies = ShouldEnableQuicForProxies(
      command_line, quic_trial_group, quic_allowed_by_policy);
  globals->enable_quic_for_proxies.set(enable_quic_for_proxies);
  if (enable_quic) {
    globals->quic_always_require_handshake_confirmation.set(
        ShouldQuicAlwaysRequireHandshakeConfirmation(quic_trial_params));
    globals->quic_disable_connection_pooling.set(
        ShouldQuicDisableConnectionPooling(quic_trial_params));
    int receive_buffer_size = GetQuicSocketReceiveBufferSize(quic_trial_params);
    if (receive_buffer_size != 0) {
      globals->quic_socket_receive_buffer_size.set(receive_buffer_size);
    }
    float load_server_info_timeout_srtt_multiplier =
        GetQuicLoadServerInfoTimeoutSrttMultiplier(quic_trial_params);
    if (load_server_info_timeout_srtt_multiplier != 0) {
      globals->quic_load_server_info_timeout_srtt_multiplier.set(
          load_server_info_timeout_srtt_multiplier);
    }
    globals->quic_enable_connection_racing.set(
        ShouldQuicEnableConnectionRacing(quic_trial_params));
    globals->quic_enable_non_blocking_io.set(
        ShouldQuicEnableNonBlockingIO(quic_trial_params));
    globals->quic_disable_disk_cache.set(
        ShouldQuicDisableDiskCache(quic_trial_params));
    int max_number_of_lossy_connections = GetQuicMaxNumberOfLossyConnections(
        quic_trial_params);
    if (max_number_of_lossy_connections != 0) {
      globals->quic_max_number_of_lossy_connections.set(
          max_number_of_lossy_connections);
    }
    float packet_loss_threshold = GetQuicPacketLossThreshold(quic_trial_params);
    if (packet_loss_threshold != 0)
      globals->quic_packet_loss_threshold.set(packet_loss_threshold);
    globals->enable_quic_port_selection.set(
        ShouldEnableQuicPortSelection(command_line));
    globals->quic_connection_options =
        GetQuicConnectionOptions(command_line, quic_trial_params);
    if (ShouldEnableQuicPacing(command_line, quic_trial_params)) {
      globals->quic_connection_options.push_back(net::kPACE);
    }
  }

  size_t max_packet_length = GetQuicMaxPacketLength(command_line,
                                                    quic_trial_params);
  if (max_packet_length != 0) {
    globals->quic_max_packet_length.set(max_packet_length);
  }

  std::string quic_user_agent_id =
      chrome::VersionInfo::GetVersionStringModifier();
  if (!quic_user_agent_id.empty())
    quic_user_agent_id.push_back(' ');
  chrome::VersionInfo version_info;
  quic_user_agent_id.append(version_info.ProductNameAndVersionForUserAgent());
  globals->quic_user_agent_id.set(quic_user_agent_id);

  net::QuicVersion version = GetQuicVersion(command_line, quic_trial_params);
  if (version != net::QUIC_VERSION_UNSUPPORTED) {
    net::QuicVersionVector supported_versions;
    supported_versions.push_back(version);
    globals->quic_supported_versions.set(supported_versions);
  }

  double threshold =
      GetAlternateProtocolProbabilityThreshold(command_line, quic_trial_params);
  if (threshold >=0 && threshold <= 1) {
    globals->alternate_protocol_probability_threshold.set(threshold);
    globals->http_server_properties->SetAlternateProtocolProbabilityThreshold(
        threshold);
  }

  if (command_line.HasSwitch(switches::kOriginToForceQuicOn)) {
    net::HostPortPair quic_origin =
        net::HostPortPair::FromString(
            command_line.GetSwitchValueASCII(switches::kOriginToForceQuicOn));
    if (!quic_origin.IsEmpty()) {
      globals->origin_to_force_quic_on.set(quic_origin);
    }
  }
}

bool IOThread::ShouldEnableQuic(const base::CommandLine& command_line,
                                base::StringPiece quic_trial_group,
                                bool quic_allowed_by_policy) {
  if (command_line.HasSwitch(switches::kDisableQuic) || !quic_allowed_by_policy)
    return false;

  if (command_line.HasSwitch(switches::kEnableQuic))
    return true;

  return quic_trial_group.starts_with(kQuicFieldTrialEnabledGroupName) ||
      quic_trial_group.starts_with(kQuicFieldTrialHttpsEnabledGroupName);
}

// static
bool IOThread::ShouldEnableQuicForProxies(const base::CommandLine& command_line,
                                          base::StringPiece quic_trial_group,
                                          bool quic_allowed_by_policy) {
  return ShouldEnableQuic(
      command_line, quic_trial_group, quic_allowed_by_policy) ||
      ShouldEnableQuicForDataReductionProxy();
}

// static
bool IOThread::ShouldEnableQuicForDataReductionProxy() {
  const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kDisableQuic))
    return false;

  return data_reduction_proxy::DataReductionProxyParams::
      IsIncludedInQuicFieldTrial();
}

bool IOThread::ShouldEnableQuicPortSelection(
    const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kDisableQuicPortSelection))
    return false;

  if (command_line.HasSwitch(switches::kEnableQuicPortSelection))
    return true;

  return false;  // Default to disabling port selection on all channels.
}

bool IOThread::ShouldEnableQuicPacing(
    const base::CommandLine& command_line,
    const VariationParameters& quic_trial_params) {
  if (command_line.HasSwitch(switches::kEnableQuicPacing))
    return true;

  if (command_line.HasSwitch(switches::kDisableQuicPacing))
    return false;

  return LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "enable_pacing"),
      "true");
}

net::QuicTagVector IOThread::GetQuicConnectionOptions(
    const base::CommandLine& command_line,
    const VariationParameters& quic_trial_params) {
  if (command_line.HasSwitch(switches::kQuicConnectionOptions)) {
    return net::QuicUtils::ParseQuicConnectionOptions(
        command_line.GetSwitchValueASCII(switches::kQuicConnectionOptions));
  }

  VariationParameters::const_iterator it =
      quic_trial_params.find("connection_options");
  if (it == quic_trial_params.end()) {
    return net::QuicTagVector();
  }

  return net::QuicUtils::ParseQuicConnectionOptions(it->second);
}

// static
double IOThread::GetAlternateProtocolProbabilityThreshold(
    const base::CommandLine& command_line,
    const VariationParameters& quic_trial_params) {
  double value;
  if (command_line.HasSwitch(
          switches::kAlternateProtocolProbabilityThreshold)) {
    if (base::StringToDouble(
            command_line.GetSwitchValueASCII(
                switches::kAlternateProtocolProbabilityThreshold),
            &value)) {
      return value;
    }
  }
  if (command_line.HasSwitch(switches::kEnableQuic)) {
    return 0;
  }
  if (base::StringToDouble(
          GetVariationParam(quic_trial_params,
                            "alternate_protocol_probability_threshold"),
          &value)) {
    return value;
  }
  return -1;
}

// static
bool IOThread::ShouldQuicAlwaysRequireHandshakeConfirmation(
    const VariationParameters& quic_trial_params) {
  return LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params,
                        "always_require_handshake_confirmation"),
      "true");
}

// static
bool IOThread::ShouldQuicDisableConnectionPooling(
    const VariationParameters& quic_trial_params) {
  return LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "disable_connection_pooling"),
      "true");
}

// static
float IOThread::GetQuicLoadServerInfoTimeoutSrttMultiplier(
    const VariationParameters& quic_trial_params) {
  double value;
  if (base::StringToDouble(GetVariationParam(quic_trial_params,
                                             "load_server_info_time_to_srtt"),
                           &value)) {
    return (float)value;
  }
  return 0.0f;
}

// static
bool IOThread::ShouldQuicEnableConnectionRacing(
    const VariationParameters& quic_trial_params) {
  return LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "enable_connection_racing"),
      "true");
}

// static
bool IOThread::ShouldQuicEnableNonBlockingIO(
    const VariationParameters& quic_trial_params) {
  return LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "enable_non_blocking_io"),
      "true");
}

// static
bool IOThread::ShouldQuicDisableDiskCache(
    const VariationParameters& quic_trial_params) {
  return LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "disable_disk_cache"), "true");
}

// static
int IOThread::GetQuicMaxNumberOfLossyConnections(
    const VariationParameters& quic_trial_params) {
  int value;
  if (base::StringToInt(GetVariationParam(quic_trial_params,
                                          "max_number_of_lossy_connections"),
                        &value)) {
    return value;
  }
  return 0;
}

// static
float IOThread::GetQuicPacketLossThreshold(
    const VariationParameters& quic_trial_params) {
  double value;
  if (base::StringToDouble(GetVariationParam(quic_trial_params,
                                             "packet_loss_threshold"),
                           &value)) {
    return (float)value;
  }
  return 0.0f;
}

// static
int IOThread::GetQuicSocketReceiveBufferSize(
    const VariationParameters& quic_trial_params) {
  int value;
  if (base::StringToInt(GetVariationParam(quic_trial_params,
                                          "receive_buffer_size"),
                        &value)) {
    return value;
  }
  return 0;
}

// static
size_t IOThread::GetQuicMaxPacketLength(
    const base::CommandLine& command_line,
    const VariationParameters& quic_trial_params) {
  if (command_line.HasSwitch(switches::kQuicMaxPacketLength)) {
    unsigned value;
    if (!base::StringToUint(
            command_line.GetSwitchValueASCII(switches::kQuicMaxPacketLength),
            &value)) {
      return 0;
    }
    return value;
  }

  unsigned value;
  if (base::StringToUint(GetVariationParam(quic_trial_params,
                                           "max_packet_length"),
                         &value)) {
    return value;
  }
  return 0;
}

// static
net::QuicVersion IOThread::GetQuicVersion(
    const base::CommandLine& command_line,
    const VariationParameters& quic_trial_params) {
  if (command_line.HasSwitch(switches::kQuicVersion)) {
    return ParseQuicVersion(
        command_line.GetSwitchValueASCII(switches::kQuicVersion));
  }

  return ParseQuicVersion(GetVariationParam(quic_trial_params, "quic_version"));
}

// static
net::QuicVersion IOThread::ParseQuicVersion(const std::string& quic_version) {
  net::QuicVersionVector supported_versions = net::QuicSupportedVersions();
  for (size_t i = 0; i < supported_versions.size(); ++i) {
    net::QuicVersion version = supported_versions[i];
    if (net::QuicVersionToString(version) == quic_version) {
      return version;
    }
  }

  return net::QUIC_VERSION_UNSUPPORTED;
}
