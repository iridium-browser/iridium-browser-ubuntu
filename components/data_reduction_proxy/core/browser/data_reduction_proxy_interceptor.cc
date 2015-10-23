// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_interceptor.h"

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_protocol.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_creator.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_http_job.h"
#include "net/url_request/url_request_job_manager.h"
#include "url/url_constants.h"

namespace data_reduction_proxy {

DataReductionProxyInterceptor::DataReductionProxyInterceptor(
    DataReductionProxyConfig* config,
    DataReductionProxyConfigServiceClient* config_service_client,
    DataReductionProxyBypassStats* stats,
    DataReductionProxyEventCreator* event_creator)
    : bypass_stats_(stats),
      config_service_client_(config_service_client),
      event_creator_(event_creator),
      bypass_protocol_(new DataReductionProxyBypassProtocol(config)) {
  DCHECK(event_creator_);
}

DataReductionProxyInterceptor::~DataReductionProxyInterceptor() {
}

net::URLRequestJob* DataReductionProxyInterceptor::MaybeInterceptRequest(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  return nullptr;
}

net::URLRequestJob* DataReductionProxyInterceptor::MaybeInterceptRedirect(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const GURL& location) const {
  return MaybeInterceptResponseOrRedirect(request, network_delegate);
}

net::URLRequestJob* DataReductionProxyInterceptor::MaybeInterceptResponse(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  return MaybeInterceptResponseOrRedirect(request, network_delegate);
}

net::URLRequestJob*
DataReductionProxyInterceptor::MaybeInterceptResponseOrRedirect(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  DCHECK(request);
  if (request->response_info().was_cached)
    return nullptr;
  bool should_retry = false;
  // Consider retrying due to an authentication failure from the Data Reduction
  // Proxy server when using the config service.
  if (config_service_client_ != nullptr) {
    const net::HttpResponseHeaders* response_headers =
        request->response_info().headers.get();
    if (response_headers) {
      should_retry = config_service_client_->ShouldRetryDueToAuthFailure(
          response_headers, request->proxy_server());
    }
  }

  // Consider retrying due errors stemming from the Data Reduction Proxy
  // protocol in the response headers.
  if (!should_retry) {
    DataReductionProxyInfo data_reduction_proxy_info;
    DataReductionProxyBypassType bypass_type = BYPASS_EVENT_TYPE_MAX;
    should_retry = bypass_protocol_->MaybeBypassProxyAndPrepareToRetry(
        request, &bypass_type, &data_reduction_proxy_info);
    if (bypass_stats_ && bypass_type != BYPASS_EVENT_TYPE_MAX)
      bypass_stats_->SetBypassType(bypass_type);

    MaybeAddBypassEvent(request, data_reduction_proxy_info, bypass_type,
                        should_retry);
  }

  if (!should_retry)
    return nullptr;
  // Returning non-NULL has the effect of restarting the request with the
  // supplied job.
  DCHECK(request->url().SchemeIs(url::kHttpScheme));
  return net::URLRequestJobManager::GetInstance()->CreateJob(
      request, network_delegate);
}

void DataReductionProxyInterceptor::MaybeAddBypassEvent(
    net::URLRequest* request,
    const DataReductionProxyInfo& data_reduction_proxy_info,
    DataReductionProxyBypassType bypass_type,
    bool should_retry) const {
  if (data_reduction_proxy_info.bypass_action != BYPASS_ACTION_TYPE_NONE) {
    event_creator_->AddBypassActionEvent(
        request->net_log(), data_reduction_proxy_info.bypass_action,
        request->method(), request->url(), should_retry,
        data_reduction_proxy_info.bypass_duration);
  } else if (bypass_type != BYPASS_EVENT_TYPE_MAX) {
    event_creator_->AddBypassTypeEvent(
        request->net_log(), bypass_type, request->method(), request->url(),
        should_retry, data_reduction_proxy_info.bypass_duration);
  }
}

}  // namespace data_reduction_proxy
