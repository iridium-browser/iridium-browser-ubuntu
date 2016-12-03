// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_QUIC_SIMPLE_DISPATCHER_H_
#define NET_TOOLS_QUIC_QUIC_SIMPLE_DISPATCHER_H_

#include "net/tools/quic/quic_dispatcher.h"

namespace net {

class QuicSimpleDispatcher : public QuicDispatcher {
 public:
  QuicSimpleDispatcher(
      const QuicConfig& config,
      const QuicCryptoServerConfig* crypto_config,
      QuicVersionManager* version_manager,
      std::unique_ptr<QuicConnectionHelperInterface> helper,
      std::unique_ptr<QuicServerSessionBase::Helper> session_helper,
      std::unique_ptr<QuicAlarmFactory> alarm_factory);

  ~QuicSimpleDispatcher() override;

 protected:
  QuicServerSessionBase* CreateQuicSession(
      QuicConnectionId connection_id,
      const IPEndPoint& client_address) override;
};

}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_SIMPLE_DISPATCHER_H_
