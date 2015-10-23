// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_CLIENT_CONFIG_RESPONSE_PARSER_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_CLIENT_CONFIG_RESPONSE_PARSER_H_

#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "net/proxy/proxy_server.h"

namespace base {
class Time;
}

namespace data_reduction_proxy {

namespace config_parser {

// Returns the |net::ProxyServer::Scheme| for a ProxyServer_ProxyScheme.
net::ProxyServer::Scheme SchemeFromProxyScheme(
    ProxyServer_ProxyScheme proxy_scheme);

// Returns the ProxyServer_ProxyScheme for a |net::ProxyServer::Scheme|.
ProxyServer_ProxyScheme ProxySchemeFromScheme(net::ProxyServer::Scheme scheme);

// Returns the |Timestamp| representation of |time|.
void TimetoTimestamp(const base::Time& time, Timestamp* timestamp);

// Returns the |base::Time| representation of |timestamp|.
base::Time TimestampToTime(const Timestamp& timestamp);

}  // namespace config_parser

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_CLIENT_CONFIG_RESPONSE_PARSER_H_
