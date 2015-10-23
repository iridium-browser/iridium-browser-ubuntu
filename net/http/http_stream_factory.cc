// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "net/base/host_mapping_rules.h"
#include "net/base/host_port_pair.h"
#include "net/base/port_util.h"
#include "net/http/http_network_session.h"
#include "net/spdy/spdy_alt_svc_wire_format.h"
#include "url/gurl.h"

namespace net {

// WARNING: If you modify or add any static flags, you must keep them in sync
// with |ResetStaticSettingsToInit|. This is critical for unit test isolation.

// static
bool HttpStreamFactory::spdy_enabled_ = true;

HttpStreamFactory::~HttpStreamFactory() {}

// static
void HttpStreamFactory::ResetStaticSettingsToInit() {
  spdy_enabled_ = true;
}

void HttpStreamFactory::ProcessAlternativeService(
    const base::WeakPtr<HttpServerProperties>& http_server_properties,
    base::StringPiece alternative_service_str,
    const HostPortPair& http_host_port_pair,
    const HttpNetworkSession& session) {
  SpdyAltSvcWireFormat::AlternativeServiceVector alternative_service_vector;
  if (!SpdyAltSvcWireFormat::ParseHeaderFieldValue(
          alternative_service_str, &alternative_service_vector)) {
    return;
  }

  // Convert SpdyAltSvcWireFormat::AlternativeService entries
  // to net::AlternativeServiceInfo.
  AlternativeServiceInfoVector alternative_service_info_vector;
  for (const SpdyAltSvcWireFormat::AlternativeService&
           alternative_service_entry : alternative_service_vector) {
    AlternateProtocol protocol =
        AlternateProtocolFromString(alternative_service_entry.protocol_id);
    if (!IsAlternateProtocolValid(protocol) ||
        !session.IsProtocolEnabled(protocol) ||
        !IsPortValid(alternative_service_entry.port)) {
      continue;
    }
    AlternativeService alternative_service(protocol,
                                           alternative_service_entry.host,
                                           alternative_service_entry.port);
    base::Time expiration =
        base::Time::Now() +
        base::TimeDelta::FromSeconds(alternative_service_entry.max_age);
    AlternativeServiceInfo alternative_service_info(
        alternative_service, alternative_service_entry.p, expiration);
    alternative_service_info_vector.push_back(alternative_service_info);
  }

  http_server_properties->SetAlternativeServices(
      RewriteHost(http_host_port_pair), alternative_service_info_vector);
}

void HttpStreamFactory::ProcessAlternateProtocol(
    const base::WeakPtr<HttpServerProperties>& http_server_properties,
    const std::vector<std::string>& alternate_protocol_values,
    const HostPortPair& http_host_port_pair,
    const HttpNetworkSession& session) {
  AlternateProtocol protocol = UNINITIALIZED_ALTERNATE_PROTOCOL;
  int port = 0;
  double probability = 1;
  bool is_valid = true;
  for (size_t i = 0; i < alternate_protocol_values.size(); ++i) {
    base::StringPiece alternate_protocol_str = alternate_protocol_values[i];
    if (base::StartsWith(alternate_protocol_str, "p=",
                         base::CompareCase::SENSITIVE)) {
      if (!base::StringToDouble(alternate_protocol_str.substr(2).as_string(),
                                &probability) ||
          probability < 0 || probability > 1) {
        DVLOG(1) << kAlternateProtocolHeader
                 << " header has unrecognizable probability: "
                 << alternate_protocol_values[i];
        is_valid = false;
        break;
      }
      continue;
    }

    std::vector<base::StringPiece> port_protocol_vector =
        base::SplitStringPiece(alternate_protocol_str, ":",
                               base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (port_protocol_vector.size() != 2) {
      DVLOG(1) << kAlternateProtocolHeader
               << " header has too many tokens: "
               << alternate_protocol_str;
      is_valid = false;
      break;
    }

    if (!base::StringToInt(port_protocol_vector[0], &port) ||
        port == 0 || !IsPortValid(port)) {
      DVLOG(1) << kAlternateProtocolHeader
               << " header has unrecognizable port: "
               << port_protocol_vector[0];
      is_valid = false;
      break;
    }

    protocol = AlternateProtocolFromString(port_protocol_vector[1].as_string());

    if (IsAlternateProtocolValid(protocol) &&
        !session.IsProtocolEnabled(protocol)) {
      DVLOG(1) << kAlternateProtocolHeader
               << " header has unrecognized protocol: "
               << port_protocol_vector[1];
      is_valid = false;
      break;
    }
  }

  if (!is_valid || protocol == UNINITIALIZED_ALTERNATE_PROTOCOL) {
    http_server_properties->ClearAlternativeServices(http_host_port_pair);
    return;
  }

  http_server_properties->SetAlternativeService(
      RewriteHost(http_host_port_pair),
      AlternativeService(protocol, "", static_cast<uint16>(port)), probability,
      base::Time::Now() + base::TimeDelta::FromDays(1));
}

GURL HttpStreamFactory::ApplyHostMappingRules(const GURL& url,
                                              HostPortPair* endpoint) {
  const HostMappingRules* mapping_rules = GetHostMappingRules();
  if (mapping_rules && mapping_rules->RewriteHost(endpoint)) {
    url::Replacements<char> replacements;
    const std::string port_str = base::UintToString(endpoint->port());
    replacements.SetPort(port_str.c_str(), url::Component(0, port_str.size()));
    replacements.SetHost(endpoint->host().c_str(),
                         url::Component(0, endpoint->host().size()));
    return url.ReplaceComponents(replacements);
  }
  return url;
}

HttpStreamFactory::HttpStreamFactory() {}

HostPortPair HttpStreamFactory::RewriteHost(HostPortPair host_port_pair) {
  const HostMappingRules* mapping_rules = GetHostMappingRules();
  if (mapping_rules)
    mapping_rules->RewriteHost(&host_port_pair);
  return host_port_pair;
}

}  // namespace net
