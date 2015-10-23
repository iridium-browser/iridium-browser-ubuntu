// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/url_request_context_config.h"

#include "base/json/json_reader.h"
#include "base/values.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_utils.h"
#include "net/url_request/url_request_context_builder.h"

namespace cronet {

#define DEFINE_CONTEXT_CONFIG(x) const char REQUEST_CONTEXT_CONFIG_##x[] = #x;
#include "components/cronet/url_request_context_config_list.h"
#undef DEFINE_CONTEXT_CONFIG

URLRequestContextConfig::QuicHint::QuicHint() {
}

URLRequestContextConfig::QuicHint::~QuicHint() {
}

// static
void URLRequestContextConfig::QuicHint::RegisterJSONConverter(
    base::JSONValueConverter<URLRequestContextConfig::QuicHint>* converter) {
  converter->RegisterStringField(REQUEST_CONTEXT_CONFIG_QUIC_HINT_HOST,
                                 &URLRequestContextConfig::QuicHint::host);
  converter->RegisterIntField(
      REQUEST_CONTEXT_CONFIG_QUIC_HINT_PORT,
      &URLRequestContextConfig::QuicHint::port);
  converter->RegisterIntField(
      REQUEST_CONTEXT_CONFIG_QUIC_HINT_ALT_PORT,
      &URLRequestContextConfig::QuicHint::alternate_port);
}

URLRequestContextConfig::URLRequestContextConfig() {
}

URLRequestContextConfig::~URLRequestContextConfig() {
}

bool URLRequestContextConfig::LoadFromJSON(const std::string& config_string) {
  scoped_ptr<base::Value> config_value = base::JSONReader::Read(config_string);
  if (!config_value || !config_value->IsType(base::Value::TYPE_DICTIONARY)) {
    DLOG(ERROR) << "Bad JSON: " << config_string;
    return false;
  }

  base::JSONValueConverter<URLRequestContextConfig> converter;
  if (!converter.Convert(*config_value, this)) {
    DLOG(ERROR) << "Bad Config: " << config_value;
    return false;
  }
  return true;
}

void URLRequestContextConfig::ConfigureURLRequestContextBuilder(
    net::URLRequestContextBuilder* context_builder) {
  std::string config_cache;
  if (http_cache != REQUEST_CONTEXT_CONFIG_HTTP_CACHE_DISABLED) {
    net::URLRequestContextBuilder::HttpCacheParams cache_params;
    if (http_cache == REQUEST_CONTEXT_CONFIG_HTTP_CACHE_DISK &&
        !storage_path.empty()) {
      cache_params.type = net::URLRequestContextBuilder::HttpCacheParams::DISK;
      cache_params.path = base::FilePath(storage_path);
    } else {
      cache_params.type =
          net::URLRequestContextBuilder::HttpCacheParams::IN_MEMORY;
    }
    cache_params.max_size = http_cache_max_size;
    context_builder->EnableHttpCache(cache_params);
  } else {
    context_builder->DisableHttpCache();
  }
  context_builder->set_user_agent(user_agent);
  context_builder->SetSpdyAndQuicEnabled(enable_spdy, enable_quic);
  context_builder->set_quic_connection_options(
      net::QuicUtils::ParseQuicConnectionOptions(quic_connection_options));
  context_builder->set_sdch_enabled(enable_sdch);
#if defined(CRONET_TEST)
  // Enable insecure quic only if Cronet is built for testing.
  // TODO(xunjieli): Remove once crbug.com/514629 is fixed.
  context_builder->set_enable_insecure_quic(true);
#endif
  // TODO(mef): Use |config| to set cookies.
}

// static
void URLRequestContextConfig::RegisterJSONConverter(
    base::JSONValueConverter<URLRequestContextConfig>* converter) {
  converter->RegisterStringField(REQUEST_CONTEXT_CONFIG_USER_AGENT,
                                 &URLRequestContextConfig::user_agent);
  converter->RegisterStringField(REQUEST_CONTEXT_CONFIG_STORAGE_PATH,
                                 &URLRequestContextConfig::storage_path);
  converter->RegisterBoolField(REQUEST_CONTEXT_CONFIG_ENABLE_QUIC,
                               &URLRequestContextConfig::enable_quic);
  converter->RegisterBoolField(REQUEST_CONTEXT_CONFIG_ENABLE_SPDY,
                               &URLRequestContextConfig::enable_spdy);
  converter->RegisterBoolField(REQUEST_CONTEXT_CONFIG_ENABLE_SDCH,
                               &URLRequestContextConfig::enable_sdch);
  converter->RegisterStringField(REQUEST_CONTEXT_CONFIG_HTTP_CACHE,
                                 &URLRequestContextConfig::http_cache);
  converter->RegisterBoolField(REQUEST_CONTEXT_CONFIG_LOAD_DISABLE_CACHE,
                               &URLRequestContextConfig::load_disable_cache);
  converter->RegisterIntField(REQUEST_CONTEXT_CONFIG_HTTP_CACHE_MAX_SIZE,
                              &URLRequestContextConfig::http_cache_max_size);
  converter->RegisterRepeatedMessage(REQUEST_CONTEXT_CONFIG_QUIC_HINTS,
                                     &URLRequestContextConfig::quic_hints);
  converter->RegisterStringField(
      REQUEST_CONTEXT_CONFIG_QUIC_OPTIONS,
      &URLRequestContextConfig::quic_connection_options);
  converter->RegisterStringField(
      REQUEST_CONTEXT_CONFIG_DATA_REDUCTION_PRIMARY_PROXY,
      &URLRequestContextConfig::data_reduction_primary_proxy);
  converter->RegisterStringField(
      REQUEST_CONTEXT_CONFIG_DATA_REDUCTION_FALLBACK_PROXY,
      &URLRequestContextConfig::data_reduction_fallback_proxy);
  converter->RegisterStringField(
      REQUEST_CONTEXT_CONFIG_DATA_REDUCTION_SECURE_PROXY_CHECK_URL,
      &URLRequestContextConfig::data_reduction_secure_proxy_check_url);
  converter->RegisterStringField(
      REQUEST_CONTEXT_CONFIG_DATA_REDUCTION_PROXY_KEY,
      &URLRequestContextConfig::data_reduction_proxy_key);
}

}  // namespace cronet
