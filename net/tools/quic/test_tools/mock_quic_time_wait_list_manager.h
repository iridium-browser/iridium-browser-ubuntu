// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common utilities for Quic tests

#ifndef NET_TOOLS_QUIC_TEST_TOOLS_MOCK_QUIC_TIME_WAIT_LIST_MANAGER_H_
#define NET_TOOLS_QUIC_TEST_TOOLS_MOCK_QUIC_TIME_WAIT_LIST_MANAGER_H_

#include "net/tools/quic/quic_time_wait_list_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {
namespace tools {
namespace test {

class MockTimeWaitListManager : public QuicTimeWaitListManager {
 public:
  MockTimeWaitListManager(QuicPacketWriter* writer,
                          QuicServerSessionVisitor* visitor,
                          QuicConnectionHelperInterface* helper);
  ~MockTimeWaitListManager() override;

  MOCK_METHOD4(AddConnectionIdToTimeWait,
               void(QuicConnectionId connection_id,
                    QuicVersion version,
                    bool connection_rejected_statelessly,
                    QuicEncryptedPacket* close_packet));

  void QuicTimeWaitListManager_AddConnectionIdToTimeWait(
      QuicConnectionId connection_id,
      QuicVersion version,
      bool connection_rejected_statelessly,
      QuicEncryptedPacket* close_packet) {
    QuicTimeWaitListManager::AddConnectionIdToTimeWait(
        connection_id, version, connection_rejected_statelessly, close_packet);
  }

  MOCK_METHOD5(ProcessPacket,
               void(const IPEndPoint& server_address,
                    const IPEndPoint& client_address,
                    QuicConnectionId connection_id,
                    QuicPacketSequenceNumber sequence_number,
                    const QuicEncryptedPacket& packet));
};

}  // namespace test
}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_TEST_TOOLS_MOCK_QUIC_TIME_WAIT_LIST_MANAGER_H_
