// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"

#include <string>
#include <vector>

#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_creator.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"

using base::StringPiece;
using base::TimeDelta;

namespace {

const char kChromeProxyHeader[] = "chrome-proxy";

const char kActionValueDelimiter = '=';

const char kChromeProxyLoFiDirective[] = "q=low";

const char kChromeProxyActionBlockOnce[] = "block-once";
const char kChromeProxyActionBlock[] = "block";
const char kChromeProxyActionBypass[] = "bypass";

// Actions for tamper detection fingerprints.
const char kChromeProxyActionFingerprintChromeProxy[]   = "fcp";
const char kChromeProxyActionFingerprintVia[]           = "fvia";
const char kChromeProxyActionFingerprintOtherHeaders[]  = "foh";
const char kChromeProxyActionFingerprintContentLength[] = "fcl";

const int kShortBypassMaxSeconds = 59;
const int kMediumBypassMaxSeconds = 300;

// Returns a random bypass duration between 1 and 5 minutes.
base::TimeDelta GetDefaultBypassDuration() {
  const int64 delta_ms =
      base::RandInt(base::TimeDelta::FromMinutes(1).InMilliseconds(),
                    base::TimeDelta::FromMinutes(5).InMilliseconds());
  return TimeDelta::FromMilliseconds(delta_ms);
}

}  // namespace

namespace data_reduction_proxy {

const char* chrome_proxy_header() {
  return kChromeProxyHeader;
}

const char* chrome_proxy_lo_fi_directive() {
  return kChromeProxyLoFiDirective;
}

bool GetDataReductionProxyActionValue(
    const net::HttpResponseHeaders* headers,
    const std::string& action_prefix,
    std::string* action_value) {
  DCHECK(headers);
  DCHECK(!action_prefix.empty());
  // A valid action does not include a trailing '='.
  DCHECK(action_prefix[action_prefix.size() - 1] != kActionValueDelimiter);
  void* iter = NULL;
  std::string value;
  std::string prefix = action_prefix + kActionValueDelimiter;

  while (headers->EnumerateHeader(&iter, kChromeProxyHeader, &value)) {
    if (value.size() > prefix.size()) {
      if (base::StartsWith(value, prefix,
                           base::CompareCase::INSENSITIVE_ASCII)) {
        if (action_value)
          *action_value = value.substr(prefix.size());
        return true;
      }
    }
  }
  return false;
}

bool ParseHeadersAndSetBypassDuration(const net::HttpResponseHeaders* headers,
                                      const std::string& action_prefix,
                                      base::TimeDelta* bypass_duration) {
  DCHECK(headers);
  DCHECK(!action_prefix.empty());
  // A valid action does not include a trailing '='.
  DCHECK(action_prefix[action_prefix.size() - 1] != kActionValueDelimiter);
  void* iter = NULL;
  std::string value;
  std::string prefix = action_prefix + kActionValueDelimiter;

  while (headers->EnumerateHeader(&iter, kChromeProxyHeader, &value)) {
    if (value.size() > prefix.size()) {
      if (base::StartsWith(value, prefix,
                           base::CompareCase::INSENSITIVE_ASCII)) {
        int64 seconds;
        if (!base::StringToInt64(
                StringPiece(value.begin() + prefix.size(), value.end()),
                &seconds) || seconds < 0) {
          continue;  // In case there is a well formed instruction.
        }
        if (seconds != 0) {
          *bypass_duration = TimeDelta::FromSeconds(seconds);
        } else {
          // Server deferred to us to choose a duration. Default to a random
          // duration between one and five minutes.
          *bypass_duration = GetDefaultBypassDuration();
        }
        return true;
      }
    }
  }
  return false;
}

bool ParseHeadersForBypassInfo(const net::HttpResponseHeaders* headers,
                               DataReductionProxyInfo* proxy_info) {
  DCHECK(proxy_info);

  // Support header of the form Chrome-Proxy: bypass|block=<duration>, where
  // <duration> is the number of seconds to wait before retrying
  // the proxy. If the duration is 0, then the default proxy retry delay
  // (specified in |ProxyList::UpdateRetryInfoOnFallback|) will be used.
  // 'bypass' instructs Chrome to bypass the currently connected data reduction
  // proxy, whereas 'block' instructs Chrome to bypass all available data
  // reduction proxies.

  // 'block' takes precedence over 'bypass' and 'block-once', so look for it
  // first.
  // TODO(bengr): Reduce checks for 'block' and 'bypass' to a single loop.
  if (ParseHeadersAndSetBypassDuration(
          headers, kChromeProxyActionBlock, &proxy_info->bypass_duration)) {
    proxy_info->bypass_all = true;
    proxy_info->mark_proxies_as_bad = true;
    proxy_info->bypass_action = BYPASS_ACTION_TYPE_BLOCK;
    return true;
  }

  // Next, look for 'bypass'.
  if (ParseHeadersAndSetBypassDuration(
          headers, kChromeProxyActionBypass, &proxy_info->bypass_duration)) {
    proxy_info->bypass_all = false;
    proxy_info->mark_proxies_as_bad = true;
    proxy_info->bypass_action = BYPASS_ACTION_TYPE_BYPASS;
    return true;
  }

  // Lastly, look for 'block-once'. 'block-once' instructs Chrome to retry the
  // current request (if it's idempotent), bypassing all available data
  // reduction proxies. Unlike 'block', 'block-once' does not cause data
  // reduction proxies to be bypassed for an extended period of time;
  // 'block-once' only affects the retry of the current request.
  if (headers->HasHeaderValue(kChromeProxyHeader,
                              kChromeProxyActionBlockOnce)) {
    proxy_info->bypass_all = true;
    proxy_info->mark_proxies_as_bad = false;
    proxy_info->bypass_duration = TimeDelta();
    proxy_info->bypass_action = BYPASS_ACTION_TYPE_BLOCK_ONCE;
    return true;
  }

  return false;
}

bool HasDataReductionProxyViaHeader(const net::HttpResponseHeaders* headers,
                                    bool* has_intermediary) {
  const size_t kVersionSize = 4;
  const char kDataReductionProxyViaValue[] = "Chrome-Compression-Proxy";
  size_t value_len = strlen(kDataReductionProxyViaValue);
  void* iter = NULL;
  std::string value;

  // Case-sensitive comparison of |value|. Assumes the received protocol and the
  // space following it are always |kVersionSize| characters. E.g.,
  // 'Via: 1.1 Chrome-Compression-Proxy'
  while (headers->EnumerateHeader(&iter, "via", &value)) {
    if (value.size() >= kVersionSize + value_len &&
        !value.compare(kVersionSize, value_len, kDataReductionProxyViaValue)) {
      if (has_intermediary)
        // We assume intermediary exists if there is another Via header after
        // the data reduction proxy's Via header.
        *has_intermediary = !(headers->EnumerateHeader(&iter, "via", &value));
      return true;
    }
  }

  return false;
}

DataReductionProxyBypassType GetDataReductionProxyBypassType(
    const net::HttpResponseHeaders* headers,
    DataReductionProxyInfo* data_reduction_proxy_info) {
  DCHECK(data_reduction_proxy_info);
  if (ParseHeadersForBypassInfo(headers, data_reduction_proxy_info)) {
    // A chrome-proxy response header is only present in a 502. For proper
    // reporting, this check must come before the 5xx checks below.
    if (!data_reduction_proxy_info->mark_proxies_as_bad)
      return BYPASS_EVENT_TYPE_CURRENT;

    const TimeDelta& duration = data_reduction_proxy_info->bypass_duration;
    if (duration <= TimeDelta::FromSeconds(kShortBypassMaxSeconds))
      return BYPASS_EVENT_TYPE_SHORT;
    if (duration <= TimeDelta::FromSeconds(kMediumBypassMaxSeconds))
      return BYPASS_EVENT_TYPE_MEDIUM;
    return BYPASS_EVENT_TYPE_LONG;
  }

  // If a bypass is triggered by any of the following cases, then the data
  // reduction proxy should be bypassed for a random duration between 1 and 5
  // minutes.
  data_reduction_proxy_info->mark_proxies_as_bad = true;
  data_reduction_proxy_info->bypass_duration = GetDefaultBypassDuration();

  // Fall back if a 500, 502 or 503 is returned.
  if (headers->response_code() == net::HTTP_INTERNAL_SERVER_ERROR)
    return BYPASS_EVENT_TYPE_STATUS_500_HTTP_INTERNAL_SERVER_ERROR;
  if (headers->response_code() == net::HTTP_BAD_GATEWAY)
    return BYPASS_EVENT_TYPE_STATUS_502_HTTP_BAD_GATEWAY;
  if (headers->response_code() == net::HTTP_SERVICE_UNAVAILABLE)
    return BYPASS_EVENT_TYPE_STATUS_503_HTTP_SERVICE_UNAVAILABLE;
  // TODO(kundaji): Bypass if Proxy-Authenticate header value cannot be
  // interpreted by data reduction proxy.
  if (headers->response_code() == net::HTTP_PROXY_AUTHENTICATION_REQUIRED &&
      !headers->HasHeader("Proxy-Authenticate")) {
    return BYPASS_EVENT_TYPE_MALFORMED_407;
  }
  if (!HasDataReductionProxyViaHeader(headers, NULL) &&
      (headers->response_code() != net::HTTP_NOT_MODIFIED)) {
    // A Via header might not be present in a 304. Since the goal of a 304
    // response is to minimize information transfer, a sender in general
    // should not generate representation metadata other than Cache-Control,
    // Content-Location, Date, ETag, Expires, and Vary.

    // The proxy Via header might also not be present in a 4xx response.
    // Separate this case from other responses that are missing the header.
    if (headers->response_code() >= net::HTTP_BAD_REQUEST &&
        headers->response_code() < net::HTTP_INTERNAL_SERVER_ERROR) {
      // At this point, any 4xx response that is missing the via header
      // indicates an issue that is scoped to only the current request, so only
      // bypass the data reduction proxy for a second.
      // TODO(sclittle): Change this to only bypass the current request once
      // that is fully supported, see http://crbug.com/418342.
      data_reduction_proxy_info->bypass_duration = TimeDelta::FromSeconds(1);
      return BYPASS_EVENT_TYPE_MISSING_VIA_HEADER_4XX;
    }
    return BYPASS_EVENT_TYPE_MISSING_VIA_HEADER_OTHER;
  }
  // There is no bypass event.
  return BYPASS_EVENT_TYPE_MAX;
}

bool GetDataReductionProxyActionFingerprintChromeProxy(
    const net::HttpResponseHeaders* headers,
    std::string* chrome_proxy_fingerprint) {
  return GetDataReductionProxyActionValue(
      headers,
      kChromeProxyActionFingerprintChromeProxy,
      chrome_proxy_fingerprint);
}

bool GetDataReductionProxyActionFingerprintVia(
    const net::HttpResponseHeaders* headers,
    std::string* via_fingerprint) {
  return GetDataReductionProxyActionValue(
      headers,
      kChromeProxyActionFingerprintVia,
      via_fingerprint);
}

bool GetDataReductionProxyActionFingerprintOtherHeaders(
    const net::HttpResponseHeaders* headers,
    std::string* other_headers_fingerprint) {
  return GetDataReductionProxyActionValue(
      headers,
      kChromeProxyActionFingerprintOtherHeaders,
      other_headers_fingerprint);
}

bool GetDataReductionProxyActionFingerprintContentLength(
    const net::HttpResponseHeaders* headers,
    std::string* content_length_fingerprint) {
  return GetDataReductionProxyActionValue(
      headers,
      kChromeProxyActionFingerprintContentLength,
      content_length_fingerprint);
}

void GetDataReductionProxyHeaderWithFingerprintRemoved(
    const net::HttpResponseHeaders* headers,
    std::vector<std::string>* values) {
  DCHECK(values);
  std::string chrome_proxy_fingerprint_prefix = std::string(
      kChromeProxyActionFingerprintChromeProxy) + kActionValueDelimiter;

  std::string value;
  void* iter = NULL;
  while (headers->EnumerateHeader(&iter, kChromeProxyHeader, &value)) {
    if (value.size() > chrome_proxy_fingerprint_prefix.size()) {
      if (base::StartsWith(value, chrome_proxy_fingerprint_prefix,
                           base::CompareCase::INSENSITIVE_ASCII)) {
        continue;
      }
    }
    values->push_back(value);
  }
}

}  // namespace data_reduction_proxy
