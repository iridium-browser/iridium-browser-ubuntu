// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_TEST_TOOLS_MOCK_QUIC_SERVER_SESSION_VISITOR_H_
#define NET_TOOLS_QUIC_TEST_TOOLS_MOCK_QUIC_SERVER_SESSION_VISITOR_H_

#include "net/tools/quic/quic_server_session.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {
namespace tools {
namespace test {

class MockQuicServerSessionVisitor : public QuicServerSessionVisitor {
 public:
  MockQuicServerSessionVisitor();
  virtual ~MockQuicServerSessionVisitor();
  MOCK_METHOD2(OnConnectionClosed,
               void(QuicConnectionId connection_id, QuicErrorCode error));
  MOCK_METHOD1(OnWriteBlocked,
               void(QuicBlockedWriterInterface* blocked_writer));
  MOCK_METHOD1(OnConnectionAddedToTimeWaitList,
               void(QuicConnectionId connection_id));
  MOCK_METHOD1(OnConnectionRemovedFromTimeWaitList,
               void(QuicConnectionId connection_id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockQuicServerSessionVisitor);
};

}  // namespace test
}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_TEST_TOOLS_MOCK_QUIC_SERVER_SESSION_VISITOR_H_
