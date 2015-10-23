// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/cronet_data_reduction_proxy.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/pref_service_factory.h"
#include "base/single_thread_task_runner.h"
#include "components/cronet/android/cronet_in_memory_pref_store.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/browser/data_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_interceptor.h"

namespace cronet {
namespace {

scoped_ptr<PrefService> CreatePrefService() {
  scoped_refptr<PrefRegistrySimple> pref_registry(new PrefRegistrySimple());
  data_reduction_proxy::RegisterSimpleProfilePrefs(pref_registry.get());
  base::PrefServiceFactory pref_service_factory;
  pref_service_factory.set_user_prefs(
      make_scoped_refptr(new CronetInMemoryPrefStore()));
  scoped_ptr<PrefService> pref_service =
      pref_service_factory.Create(pref_registry.get()).Pass();
  pref_registry = nullptr;
  return pref_service.Pass();
}

// TODO(bengr): Apply test configurations directly, instead of via the
// command line.
void AddOptionsToCommandLine(const std::string& primary_proxy,
                             const std::string& fallback_proxy,
                             const std::string& secure_proxy_check_url,
                             base::CommandLine* command_line) {
  DCHECK((primary_proxy.empty() && fallback_proxy.empty() &&
      secure_proxy_check_url.empty()) ||
          (!primary_proxy.empty() && !fallback_proxy.empty() &&
              !secure_proxy_check_url.empty()));
  if (primary_proxy.empty())
    return;
  command_line->AppendSwitchASCII(
      data_reduction_proxy::switches::kDataReductionProxy, primary_proxy);
  command_line->AppendSwitchASCII(
      data_reduction_proxy::switches::kDataReductionProxyFallback,
      fallback_proxy);
  command_line->AppendSwitchASCII(
      data_reduction_proxy::switches::kDataReductionProxySecureProxyCheckURL,
      secure_proxy_check_url);
}

}  // namespace

CronetDataReductionProxy::CronetDataReductionProxy(
    const std::string& key,
    const std::string& primary_proxy,
    const std::string& fallback_proxy,
    const std::string& secure_proxy_check_url,
    const std::string& user_agent,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    net::NetLog* net_log)
    : task_runner_(task_runner) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  AddOptionsToCommandLine(primary_proxy, fallback_proxy, secure_proxy_check_url,
                          base::CommandLine::ForCurrentProcess());
  prefs_ = CreatePrefService();
  // In Cronet, the Data Reduction Proxy's UI classes are Created on Cronet's
  // network thread.
  settings_.reset(
      new data_reduction_proxy::DataReductionProxySettings());
  io_data_.reset(
      new data_reduction_proxy::DataReductionProxyIOData(
          data_reduction_proxy::Client::CRONET_ANDROID,
          data_reduction_proxy::DataReductionProxyParams::kAllowed |
              data_reduction_proxy::DataReductionProxyParams::kFallbackAllowed,
          net_log, task_runner, task_runner, false, false, user_agent));
  io_data_->request_options()->SetKeyOnIO(key);
}

CronetDataReductionProxy::~CronetDataReductionProxy() {
  io_data_->ShutdownOnUIThread();
}

scoped_ptr<net::NetworkDelegate>
CronetDataReductionProxy::CreateNetworkDelegate(
    scoped_ptr<net::NetworkDelegate> wrapped_network_delegate) {
  return io_data_->CreateNetworkDelegate(wrapped_network_delegate.Pass(),
                                         false /* No bypass UMA */ );
}

scoped_ptr<net::URLRequestInterceptor>
CronetDataReductionProxy::CreateInterceptor() {
  return io_data_->CreateInterceptor();
}

void CronetDataReductionProxy::Init(bool enable,
                                    net::URLRequestContext* context) {
  url_request_context_getter_ =
      new net::TrivialURLRequestContextGetter(
          context, task_runner_);
  scoped_ptr<data_reduction_proxy::DataReductionProxyService>
      data_reduction_proxy_service(
          new data_reduction_proxy::DataReductionProxyService(
              settings_.get(), prefs_.get(),
              url_request_context_getter_.get(),
              make_scoped_ptr(new data_reduction_proxy::DataStore()),
              task_runner_, task_runner_, task_runner_, base::TimeDelta()));
  io_data_->SetDataReductionProxyService(
      data_reduction_proxy_service->GetWeakPtr());
  settings_->InitDataReductionProxySettings(
      prefs_.get(), io_data_.get(), data_reduction_proxy_service.Pass());
  settings_->SetDataReductionProxyEnabled(enable);
  settings_->MaybeActivateDataReductionProxy(true);
}

}  // namespace cronet
