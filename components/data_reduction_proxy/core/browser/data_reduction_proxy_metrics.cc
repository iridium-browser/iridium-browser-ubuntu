// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"

#include "base/basictypes.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_info.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_server.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "url/url_constants.h"

namespace data_reduction_proxy {

DataReductionProxyRequestType GetDataReductionProxyRequestType(
    const net::URLRequest& request,
    const net::ProxyConfig& data_reduction_proxy_config,
    const DataReductionProxyConfig& config) {
  if (request.url().SchemeIs(url::kHttpsScheme))
    return HTTPS;
  if (!request.url().SchemeIs(url::kHttpScheme)) {
    NOTREACHED();
    return UNKNOWN_TYPE;
  }

  // Check for a Data Reduction Proxy via header before checking if proxies are
  // bypassed, to avoid misreporting cases where the Data Reduction Proxy was
  // bypassed between the request being sent out and the response coming in.
  if (request.response_info().headers.get() &&
      HasDataReductionProxyViaHeader(request.response_info().headers.get(),
                                     NULL)) {
    return VIA_DATA_REDUCTION_PROXY;
  }

  base::TimeDelta bypass_delay;
  if (config.AreDataReductionProxiesBypassed(
      request, data_reduction_proxy_config, &bypass_delay)) {
    if (bypass_delay > base::TimeDelta::FromSeconds(kLongBypassDelayInSeconds))
      return LONG_BYPASS;
    return SHORT_BYPASS;
  }

  // Treat bypasses that only apply to the individual request as SHORT_BYPASS.
  // This includes bypasses triggered by "Chrome-Proxy: block-once", bypasses
  // due to other proxies overriding the Data Reduction Proxy, and bypasses due
  // to local bypass rules.
  if ((request.load_flags() & net::LOAD_BYPASS_PROXY) ||
      (!request.proxy_server().IsEmpty() &&
       !config.IsDataReductionProxy(request.proxy_server(), NULL)) ||
      config.IsBypassedByDataReductionProxyLocalRules(
          request, data_reduction_proxy_config)) {
    return SHORT_BYPASS;
  }

  return UNKNOWN_TYPE;
}

int64 GetAdjustedOriginalContentLength(
    DataReductionProxyRequestType request_type,
    int64 original_content_length,
    int64 received_content_length) {
  // Since there was no indication of the original content length, presume
  // it is no different from the number of bytes read.
  if (original_content_length == -1 ||
      request_type != VIA_DATA_REDUCTION_PROXY) {
    return received_content_length;
  }
  return original_content_length;
}

}  // namespace data_reduction_proxy
