// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/test_tools/mock_quic_time_wait_list_manager.h"

using testing::_;
using testing::AnyNumber;
using testing::Invoke;

namespace net {
namespace tools {
namespace test {

MockTimeWaitListManager::MockTimeWaitListManager(
    QuicPacketWriter* writer,
    QuicServerSessionVisitor* visitor,
    QuicConnectionHelperInterface* helper)
    : QuicTimeWaitListManager(writer,
                              visitor,
                              helper,
                              QuicSupportedVersions()) {
  // Though AddConnectionIdToTimeWait is mocked, we want to retain its
  // functionality.
  EXPECT_CALL(*this, AddConnectionIdToTimeWait(_, _, _, _))
      .Times(testing::AnyNumber());
  ON_CALL(*this, AddConnectionIdToTimeWait(_, _, _, _))
      .WillByDefault(
          Invoke(this, &MockTimeWaitListManager::
                           QuicTimeWaitListManager_AddConnectionIdToTimeWait));
}

MockTimeWaitListManager::~MockTimeWaitListManager() {}

}  // namespace test
}  // namespace tools
}  // namespace net
