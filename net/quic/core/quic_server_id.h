// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SERVER_ID_H_
#define NET_QUIC_QUIC_SERVER_ID_H_

#include <stdint.h>

#include <string>

#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/base/privacy_mode.h"

namespace net {

// The id used to identify sessions. Includes the hostname, port, scheme and
// privacy_mode.
class NET_EXPORT_PRIVATE QuicServerId {
 public:
  QuicServerId();
  QuicServerId(const HostPortPair& host_port_pair, PrivacyMode privacy_mode);
  QuicServerId(const std::string& host, uint16_t port);
  QuicServerId(const std::string& host,
               uint16_t port,
               PrivacyMode privacy_mode);
  ~QuicServerId();

  // Needed to be an element of std::set.
  bool operator<(const QuicServerId& other) const;
  bool operator==(const QuicServerId& other) const;

  // Creates a QuicServerId from a string formatted in same manner as
  // ToString().
  static QuicServerId FromString(const std::string& str);

  // ToString() will convert the QuicServerId to "scheme:hostname:port" or
  // "scheme:hostname:port/private". "scheme" will be "https".
  std::string ToString() const;

  // Used in Chromium, but not in the server.
  const HostPortPair& host_port_pair() const { return host_port_pair_; }

  const std::string& host() const { return host_port_pair_.host(); }

  uint16_t port() const { return host_port_pair_.port(); }

  PrivacyMode privacy_mode() const { return privacy_mode_; }

 private:
  HostPortPair host_port_pair_;
  PrivacyMode privacy_mode_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SERVER_ID_H_
