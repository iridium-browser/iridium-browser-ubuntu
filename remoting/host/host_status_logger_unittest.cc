// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_status_logger.h"

#include "base/message_loop/message_loop.h"
#include "remoting/host/fake_host_status_monitor.h"
#include "remoting/signaling/mock_signal_strategy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/libjingle/xmllite/xmlelement.h"

using buzz::XmlElement;
using buzz::QName;
using testing::_;
using testing::DeleteArg;
using testing::InSequence;
using testing::Return;

namespace remoting {

namespace {

ACTION_P(QuitMainMessageLoop, message_loop) {
  message_loop->PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
}

const char kJabberClientNamespace[] = "jabber:client";
const char kChromotingNamespace[] = "google:remoting";
const char kTestBotJid[] = "remotingunittest@bot.talk.google.com";
const char kClientJid1[] = "client@domain.com/1234";
const char kClientJid2[] = "client@domain.com/5678";
const char kHostJid[] = "host@domain.com/1234";

bool IsLogEntryForConnection(XmlElement* node, const char* connection_type) {
  return (node->Name() == QName(kChromotingNamespace, "entry") &&
          node->Attr(QName(std::string(), "event-name")) == "session-state" &&
          node->Attr(QName(std::string(), "session-state")) == "connected" &&
          node->Attr(QName(std::string(), "role")) == "host" &&
          node->Attr(QName(std::string(), "mode")) == "me2me" &&
          node->Attr(QName(std::string(), "connection-type")) ==
              connection_type);
}

MATCHER_P(IsClientConnected, connection_type, "") {
  if (arg->Name() != QName(kJabberClientNamespace, "iq")) {
    return false;
  }
  buzz::XmlElement* log_stanza = arg->FirstChild()->AsElement();
  if (log_stanza->Name() != QName(kChromotingNamespace, "log")) {
    return false;
  }
  if (log_stanza->NextChild()) {
    return false;
  }
  buzz::XmlElement* log_entry = log_stanza->FirstChild()->AsElement();
  if (!IsLogEntryForConnection(log_entry, connection_type)) {
    return false;
  }
  if (log_entry->NextChild()) {
    return false;
  }
  return true;
}

MATCHER_P2(IsTwoClientsConnected, connection_type1, connection_type2, "") {
  if (arg->Name() != QName(kJabberClientNamespace, "iq")) {
    return false;
  }
  buzz::XmlElement* log_stanza = arg->FirstChild()->AsElement();
  if (log_stanza->Name() != QName(kChromotingNamespace, "log")) {
    return false;
  }
  if (log_stanza->NextChild()) {
    return false;
  }
  buzz::XmlElement* log_entry = log_stanza->FirstChild()->AsElement();
  if (!IsLogEntryForConnection(log_entry, connection_type1)) {
    return false;
  }
  log_entry = log_entry->NextChild()->AsElement();
  if (!IsLogEntryForConnection(log_entry, connection_type2)) {
    return false;
  }
  if (log_entry->NextChild()) {
    return false;
  }
  return true;
}

bool IsLogEntryForDisconnection(XmlElement* node) {
  return (node->Name() == QName(kChromotingNamespace, "entry") &&
          node->Attr(QName(std::string(), "event-name")) == "session-state" &&
          node->Attr(QName(std::string(), "session-state")) == "closed" &&
          node->Attr(QName(std::string(), "role")) == "host" &&
          node->Attr(QName(std::string(), "mode")) == "me2me");
}

MATCHER(IsClientDisconnected, "") {
  if (arg->Name() != QName(kJabberClientNamespace, "iq")) {
    return false;
  }
  buzz::XmlElement* log_stanza = arg->FirstChild()->AsElement();
  if (log_stanza->Name() !=QName(kChromotingNamespace, "log")) {
    return false;
  }
  if (log_stanza->NextChild()) {
    return false;
  }
  buzz::XmlElement* log_entry = log_stanza->FirstChild()->AsElement();
  if (!IsLogEntryForDisconnection(log_entry)) {
    return false;
  }
  if (log_entry->NextChild()) {
    return false;
  }
  return true;
}

}  // namespace

class HostStatusLoggerTest : public testing::Test {
 public:
  HostStatusLoggerTest() {}
  void SetUp() override {
    EXPECT_CALL(signal_strategy_, AddListener(_));
    host_status_logger_.reset(
        new HostStatusLogger(host_status_monitor_.AsWeakPtr(),
                             ServerLogEntry::ME2ME,
                             &signal_strategy_,
                             kTestBotJid));
    EXPECT_CALL(signal_strategy_, RemoveListener(_));
  }

 protected:
  base::MessageLoop message_loop_;
  MockSignalStrategy signal_strategy_;
  scoped_ptr<HostStatusLogger> host_status_logger_;
  FakeHostStatusMonitor host_status_monitor_;
};

TEST_F(HostStatusLoggerTest, SendNow) {
  {
    InSequence s;
    EXPECT_CALL(signal_strategy_, GetLocalJid())
        .WillRepeatedly(Return(kHostJid));
    EXPECT_CALL(signal_strategy_, AddListener(_));
    EXPECT_CALL(signal_strategy_, GetNextId());
    EXPECT_CALL(signal_strategy_, SendStanzaPtr(IsClientConnected("direct")))
        .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
    EXPECT_CALL(signal_strategy_, RemoveListener(_))
        .WillOnce(QuitMainMessageLoop(&message_loop_))
        .RetiresOnSaturation();
  }
  host_status_logger_->SetSignalingStateForTest(SignalStrategy::CONNECTED);
  protocol::TransportRoute route;
  route.type = protocol::TransportRoute::DIRECT;
  host_status_logger_->OnClientRouteChange(kClientJid1, "video", route);
  host_status_logger_->OnClientAuthenticated(kClientJid1);
  host_status_logger_->OnClientConnected(kClientJid1);
  host_status_logger_->SetSignalingStateForTest(
      SignalStrategy::DISCONNECTED);
  message_loop_.Run();
}

TEST_F(HostStatusLoggerTest, SendLater) {
  protocol::TransportRoute route;
  route.type = protocol::TransportRoute::DIRECT;
  host_status_logger_->OnClientRouteChange(kClientJid1, "video", route);
  host_status_logger_->OnClientAuthenticated(kClientJid1);
  host_status_logger_->OnClientConnected(kClientJid1);
  {
    InSequence s;
    EXPECT_CALL(signal_strategy_, GetLocalJid())
        .WillRepeatedly(Return(kHostJid));
    EXPECT_CALL(signal_strategy_, AddListener(_));
    EXPECT_CALL(signal_strategy_, GetNextId());
    EXPECT_CALL(signal_strategy_, SendStanzaPtr(IsClientConnected("direct")))
        .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
    EXPECT_CALL(signal_strategy_, RemoveListener(_))
        .WillOnce(QuitMainMessageLoop(&message_loop_))
        .RetiresOnSaturation();
  }
  host_status_logger_->SetSignalingStateForTest(SignalStrategy::CONNECTED);
  host_status_logger_->SetSignalingStateForTest(SignalStrategy::DISCONNECTED);
  message_loop_.Run();
}

TEST_F(HostStatusLoggerTest, SendTwoEntriesLater) {
  protocol::TransportRoute route1;
  route1.type = protocol::TransportRoute::DIRECT;
  host_status_logger_->OnClientRouteChange(kClientJid1, "video", route1);
  host_status_logger_->OnClientAuthenticated(kClientJid1);
  host_status_logger_->OnClientConnected(kClientJid1);
  protocol::TransportRoute route2;
  route2.type = protocol::TransportRoute::STUN;
  host_status_logger_->OnClientRouteChange(kClientJid2, "video", route2);
  host_status_logger_->OnClientAuthenticated(kClientJid2);
  host_status_logger_->OnClientConnected(kClientJid2);
  {
    InSequence s;
    EXPECT_CALL(signal_strategy_, GetLocalJid())
        .WillRepeatedly(Return(kHostJid));
    EXPECT_CALL(signal_strategy_, AddListener(_));
    EXPECT_CALL(signal_strategy_, GetNextId());
    EXPECT_CALL(signal_strategy_,
        SendStanzaPtr(IsTwoClientsConnected("direct", "stun")))
            .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
    EXPECT_CALL(signal_strategy_, RemoveListener(_))
        .WillOnce(QuitMainMessageLoop(&message_loop_))
        .RetiresOnSaturation();
  }
  host_status_logger_->SetSignalingStateForTest(SignalStrategy::CONNECTED);
  host_status_logger_->SetSignalingStateForTest(SignalStrategy::DISCONNECTED);
  message_loop_.Run();
}

TEST_F(HostStatusLoggerTest, HandleRouteChangeInUnusualOrder) {
  {
    InSequence s;
    EXPECT_CALL(signal_strategy_, GetLocalJid())
        .WillRepeatedly(Return(kHostJid));
    EXPECT_CALL(signal_strategy_, AddListener(_));
    EXPECT_CALL(signal_strategy_, GetNextId());
    EXPECT_CALL(signal_strategy_, SendStanzaPtr(IsClientConnected("direct")))
        .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
    EXPECT_CALL(signal_strategy_, GetNextId());
    EXPECT_CALL(signal_strategy_, SendStanzaPtr(IsClientDisconnected()))
        .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
    EXPECT_CALL(signal_strategy_, GetNextId());
    EXPECT_CALL(signal_strategy_, SendStanzaPtr(IsClientConnected("stun")))
        .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
    EXPECT_CALL(signal_strategy_, RemoveListener(_))
        .WillOnce(QuitMainMessageLoop(&message_loop_))
        .RetiresOnSaturation();
  }
  host_status_logger_->SetSignalingStateForTest(SignalStrategy::CONNECTED);
  protocol::TransportRoute route1;
  route1.type = protocol::TransportRoute::DIRECT;
  host_status_logger_->OnClientRouteChange(kClientJid1, "video", route1);
  host_status_logger_->OnClientAuthenticated(kClientJid1);
  host_status_logger_->OnClientConnected(kClientJid1);
  protocol::TransportRoute route2;
  route2.type = protocol::TransportRoute::STUN;
  host_status_logger_->OnClientRouteChange(kClientJid2, "video", route2);
  host_status_logger_->OnClientDisconnected(kClientJid1);
  host_status_logger_->OnClientAuthenticated(kClientJid2);
  host_status_logger_->OnClientConnected(kClientJid2);
  host_status_logger_->SetSignalingStateForTest(SignalStrategy::DISCONNECTED);
  message_loop_.Run();
}

}  // namespace remoting
