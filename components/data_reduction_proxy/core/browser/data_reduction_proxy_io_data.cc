// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_protocol.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_experiments_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_interceptor.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_creator.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_storage_delegate.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "net/log/net_log.h"
#include "net/url_request/http_user_agent_settings.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"

namespace data_reduction_proxy {

// A |net::URLRequestContextGetter| which uses only vanilla HTTP/HTTPS for
// performing requests. This is used by the secure proxy check to prevent the
// use of SPDY and QUIC which may be used by the primary request contexts.
class BasicHTTPURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  BasicHTTPURLRequestContextGetter(
      const std::string& user_agent,
      const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner);

  // Overridden from net::URLRequestContextGetter:
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

 private:
  ~BasicHTTPURLRequestContextGetter() override;

  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  scoped_ptr<net::HttpUserAgentSettings> user_agent_settings_;
  scoped_ptr<net::URLRequestContext> url_request_context_;

  DISALLOW_COPY_AND_ASSIGN(BasicHTTPURLRequestContextGetter);
};

BasicHTTPURLRequestContextGetter::BasicHTTPURLRequestContextGetter(
    const std::string& user_agent,
    const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner)
    : network_task_runner_(network_task_runner),
      user_agent_settings_(
          new net::StaticHttpUserAgentSettings(std::string(), user_agent)) {
}

net::URLRequestContext*
BasicHTTPURLRequestContextGetter::GetURLRequestContext() {
  if (!url_request_context_) {
    net::URLRequestContextBuilder builder;
    builder.set_proxy_service(net::ProxyService::CreateDirect());
    builder.SetSpdyAndQuicEnabled(false, false);
    url_request_context_.reset(builder.Build());
  }

  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
BasicHTTPURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

BasicHTTPURLRequestContextGetter::~BasicHTTPURLRequestContextGetter() {
}

DataReductionProxyIOData::DataReductionProxyIOData(
    const Client& client,
    int param_flags,
    net::NetLog* net_log,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    bool enabled,
    bool enable_quic,
    const std::string& user_agent)
    : client_(client),
      net_log_(net_log),
      io_task_runner_(io_task_runner),
      ui_task_runner_(ui_task_runner),
      enabled_(enabled),
      url_request_context_getter_(nullptr),
      basic_url_request_context_getter_(
          new BasicHTTPURLRequestContextGetter(user_agent, io_task_runner)),
      weak_factory_(this) {
  DCHECK(net_log);
  DCHECK(io_task_runner_);
  DCHECK(ui_task_runner_);
  scoped_ptr<DataReductionProxyParams> params(
      new DataReductionProxyParams(param_flags));
  params->EnableQuic(enable_quic);
  event_creator_.reset(new DataReductionProxyEventCreator(this));
  configurator_.reset(
      new DataReductionProxyConfigurator(net_log, event_creator_.get()));
  bool use_config_client = DataReductionProxyParams::IsConfigClientEnabled();
  DataReductionProxyMutableConfigValues* raw_mutable_config = nullptr;
  if (use_config_client) {
    scoped_ptr<DataReductionProxyMutableConfigValues> mutable_config =
        DataReductionProxyMutableConfigValues::CreateFromParams(params.get());
    raw_mutable_config = mutable_config.get();
    config_.reset(new DataReductionProxyConfig(net_log, mutable_config.Pass(),
                                               configurator_.get(),
                                               event_creator_.get()));
  } else {
    config_.reset(new DataReductionProxyConfig(
        net_log, params.Pass(), configurator_.get(), event_creator_.get()));
  }

  // It is safe to use base::Unretained here, since it gets executed
  // synchronously on the IO thread, and |this| outlives the caller (since the
  // caller is owned by |this|.
  bypass_stats_.reset(new DataReductionProxyBypassStats(
      config_.get(), base::Bind(&DataReductionProxyIOData::SetUnreachable,
                                base::Unretained(this))));
  request_options_.reset(
      new DataReductionProxyRequestOptions(client_, config_.get()));
  request_options_->Init();
  if (use_config_client) {
    config_client_.reset(new DataReductionProxyConfigServiceClient(
        params.Pass(), GetBackoffPolicy(), request_options_.get(),
        raw_mutable_config, config_.get(), event_creator_.get(), net_log_));
  }

  proxy_delegate_.reset(
      new DataReductionProxyDelegate(request_options_.get(), config_.get()));
  // It is safe to use base::Unretained here, since it gets executed
  // synchronously on the IO thread, and |this| outlives the caller (since the
  // caller is owned by |this|.
  experiments_stats_.reset(new DataReductionProxyExperimentsStats(base::Bind(
      &DataReductionProxyIOData::SetInt64Pref, base::Unretained(this))));
 }

 DataReductionProxyIOData::DataReductionProxyIOData()
     : client_(Client::UNKNOWN),
       net_log_(nullptr),
       url_request_context_getter_(nullptr),
       weak_factory_(this) {
}

DataReductionProxyIOData::~DataReductionProxyIOData() {
}

void DataReductionProxyIOData::ShutdownOnUIThread() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
}

void DataReductionProxyIOData::SetDataReductionProxyService(
    base::WeakPtr<DataReductionProxyService> data_reduction_proxy_service) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  service_ = data_reduction_proxy_service;
  url_request_context_getter_ = service_->url_request_context_getter();
  // Using base::Unretained is safe here, unless the browser is being shut down
  // before the Initialize task can be executed. The task is only created as
  // part of class initialization.
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DataReductionProxyIOData::InitializeOnIOThread,
                 base::Unretained(this)));
}

void DataReductionProxyIOData::InitializeOnIOThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  config_->InitializeOnIOThread(basic_url_request_context_getter_.get());
  if (config_client_.get())
    config_client_->InitializeOnIOThread(url_request_context_getter_);
  experiments_stats_->InitializeOnIOThread();
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DataReductionProxyService::SetIOData,
                 service_, weak_factory_.GetWeakPtr()));
}

bool DataReductionProxyIOData::IsEnabled() const {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  return enabled_;
}

void DataReductionProxyIOData::RetrieveConfig() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (config_client_)
    config_client_->RetrieveConfig();
}

scoped_ptr<net::URLRequestInterceptor>
DataReductionProxyIOData::CreateInterceptor() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  return make_scoped_ptr(new DataReductionProxyInterceptor(
      config_.get(), bypass_stats_.get(), event_creator_.get()));
}

scoped_ptr<DataReductionProxyNetworkDelegate>
DataReductionProxyIOData::CreateNetworkDelegate(
    scoped_ptr<net::NetworkDelegate> wrapped_network_delegate,
    bool track_proxy_bypass_statistics) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  scoped_ptr<DataReductionProxyNetworkDelegate> network_delegate(
      new DataReductionProxyNetworkDelegate(
          wrapped_network_delegate.Pass(), config_.get(),
          request_options_.get(), configurator_.get(),
          experiments_stats_.get()));
  if (track_proxy_bypass_statistics)
    network_delegate->InitIODataAndUMA(this, bypass_stats_.get());
  return network_delegate.Pass();
}

// TODO(kundaji): Rename this method to something more descriptive.
// Bug http://crbug/488190.
void DataReductionProxyIOData::SetProxyPrefs(bool enabled,
                                             bool alternative_enabled,
                                             bool at_startup) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK(url_request_context_getter_->GetURLRequestContext()->proxy_service());
  enabled_ = enabled;
  config_->SetProxyConfig(enabled, alternative_enabled, at_startup);

  // If Data Saver is disabled, reset data reduction proxy state.
  if (!enabled) {
    net::ProxyService* proxy_service =
        url_request_context_getter_->GetURLRequestContext()->proxy_service();
    proxy_service->ClearBadProxiesCache();
    bypass_stats_->ClearRequestCounts();
    bypass_stats_->NotifyUnavailabilityIfChanged();
  }
}

void DataReductionProxyIOData::UpdateContentLengths(
    int64 received_content_length,
    int64 original_content_length,
    bool data_reduction_proxy_enabled,
    DataReductionProxyRequestType request_type) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DataReductionProxyService::UpdateContentLengths,
                 service_, received_content_length, original_content_length,
                 data_reduction_proxy_enabled, request_type));
}

void DataReductionProxyIOData::AddEvent(scoped_ptr<base::Value> event) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE, base::Bind(&DataReductionProxyService::AddEvent, service_,
                            base::Passed(&event)));
}

void DataReductionProxyIOData::AddEnabledEvent(scoped_ptr<base::Value> event,
                                               bool enabled) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE, base::Bind(&DataReductionProxyService::AddEnabledEvent,
                            service_, base::Passed(&event), enabled));
}

void DataReductionProxyIOData::AddEventAndSecureProxyCheckState(
    scoped_ptr<base::Value> event,
    SecureProxyCheckState state) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DataReductionProxyService::AddEventAndSecureProxyCheckState,
                 service_, base::Passed(&event), state));
}

void DataReductionProxyIOData::AddAndSetLastBypassEvent(
    scoped_ptr<base::Value> event,
    int64 expiration_ticks) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DataReductionProxyService::AddAndSetLastBypassEvent, service_,
                 base::Passed(&event), expiration_ticks));
}

void DataReductionProxyIOData::SetUnreachable(bool unreachable) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DataReductionProxyService::SetUnreachable,
                 service_, unreachable));
}

void DataReductionProxyIOData::SetInt64Pref(const std::string& pref_path,
                                            int64 value) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE, base::Bind(&DataReductionProxyService::SetInt64Pref, service_,
                            pref_path, value));
}

}  // namespace data_reduction_proxy
