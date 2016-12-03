// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_auth_handler.h"

#include <cstdint>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/security_key/fake_security_key_ipc_client.h"
#include "remoting/host/security_key/fake_security_key_ipc_server.h"
#include "remoting/host/security_key/security_key_ipc_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const int kConnectionId1 = 1;
const int kConnectionId2 = 2;
}  // namespace

namespace remoting {

class SecurityKeyAuthHandlerWinTest : public testing::Test {
 public:
  SecurityKeyAuthHandlerWinTest();
  ~SecurityKeyAuthHandlerWinTest() override;

  // Passed to the object used for testing to be called back to signal
  // completion of an IPC channel state change or reception of an IPC message.
  void OperationComplete();

 protected:
  // Waits until the current |run_loop_| instance is signaled, then resets it.
  void WaitForOperationComplete();

  // Used as a callback given to the object under test, expected to be called
  // back when a security key request is received by it.
  void SendMessageToClient(int connection_id, const std::string& data);

  // Creates a new security key connection on the object under test.
  void CreateSecurityKeyConnection(const std::string& channel_name);

  // Sets |desktop_session_id_| to the id for the current Windows session.
  void InitializeDesktopSessionId();

  // Uses |fake_ipc_client| to connect to the initial IPC server channel, it
  // then validates internal state of the object under test and closes the
  // connection based on |close_connection|.
  void EstablishInitialIpcConnection(FakeSecurityKeyIpcClient* fake_ipc_client,
                                     int expected_connection_id,
                                     const std::string& channel_name,
                                     bool close_connection);

  // Sends a security key response message using |fake_ipc_server| and
  // validates the state of the object under test.
  void SendRequestToSecurityKeyAuthHandler(
      const base::WeakPtr<FakeSecurityKeyIpcServer>& fake_ipc_server,
      int connection_id,
      const std::string& request_payload);

  // Sends a security key response message to |fake_ipc_server| and validates
  // the state of the object under test.
  void SendResponseViaSecurityKeyAuthHandler(
      const base::WeakPtr<FakeSecurityKeyIpcServer>& fake_ipc_server,
      int connection_id,
      const std::string& response_payload);

  // Closes a security key session IPC channel and validates state.
  void CloseSecurityKeySessionIpcChannel(
      const base::WeakPtr<FakeSecurityKeyIpcServer>& fake_ipc_server,
      int connection_id);

  // Returns a unique IPC channel name which prevents conflicts when running
  // tests concurrently.
  std::string GetUniqueTestChannelName();

  // IPC tests require a valid MessageLoop to run.
  base::MessageLoopForIO message_loop_;

  // Used to allow |message_loop_| to run during tests.  The instance is reset
  // after each stage of the tests has been completed.
  std::unique_ptr<base::RunLoop> run_loop_;

  // The object under test.
  std::unique_ptr<SecurityKeyAuthHandler> auth_handler_;

  // Set as the default factory to create SecurityKeyIpcServerFactory
  // instances, this class will track each objects creation and allow the tests
  // to access it and use it for driving tests and validating state.
  FakeSecurityKeyIpcServerFactory ipc_server_factory_;

  // Used to validate the object under test uses the correct ID when
  // communicating over the IPC channel.
  int last_connection_id_received_ = -1;

  // Used to validate that IPC connections are only allowed from a specific
  // Windows session.
  DWORD desktop_session_id_ = UINT32_MAX;

  // Stores the contents of the last IPC message received for validation.
  std::string last_message_received_;

 private:
  testing::NiceMock<MockClientSessionDetails> mock_client_session_details_;

  DISALLOW_COPY_AND_ASSIGN(SecurityKeyAuthHandlerWinTest);
};

SecurityKeyAuthHandlerWinTest::SecurityKeyAuthHandlerWinTest()
    : run_loop_(new base::RunLoop()) {
  auth_handler_ = remoting::SecurityKeyAuthHandler::Create(
      &mock_client_session_details_,
      base::Bind(&SecurityKeyAuthHandlerWinTest::SendMessageToClient,
                 base::Unretained(this)),
      /*file_task_runner=*/nullptr);
}

SecurityKeyAuthHandlerWinTest::~SecurityKeyAuthHandlerWinTest() {}

void SecurityKeyAuthHandlerWinTest::OperationComplete() {
  run_loop_->Quit();
}

void SecurityKeyAuthHandlerWinTest::WaitForOperationComplete() {
  run_loop_->Run();
  run_loop_.reset(new base::RunLoop());
}

void SecurityKeyAuthHandlerWinTest::SendMessageToClient(
    int connection_id,
    const std::string& data) {
  last_connection_id_received_ = connection_id;
  last_message_received_ = data;
  OperationComplete();
}

void SecurityKeyAuthHandlerWinTest::CreateSecurityKeyConnection(
    const std::string& channel_name) {
  ASSERT_EQ(0u, auth_handler_->GetActiveConnectionCountForTest());

  remoting::SetSecurityKeyIpcChannelNameForTest(channel_name);

  // Create a new SecurityKey IPC Server connection.
  auth_handler_->CreateSecurityKeyConnection();
  ASSERT_TRUE(IPC::Channel::IsNamedServerInitialized(channel_name));

  InitializeDesktopSessionId();
}

void SecurityKeyAuthHandlerWinTest::InitializeDesktopSessionId() {
  ASSERT_TRUE(
      ProcessIdToSessionId(GetCurrentProcessId(), &desktop_session_id_));

  ON_CALL(mock_client_session_details_, desktop_session_id())
      .WillByDefault(testing::Return(desktop_session_id_));
}

void SecurityKeyAuthHandlerWinTest::EstablishInitialIpcConnection(
    FakeSecurityKeyIpcClient* fake_ipc_client,
    int expected_connection_id,
    const std::string& channel_name,
    bool close_connection) {
  size_t expected_connection_count =
      auth_handler_->GetActiveConnectionCountForTest() + 1;

  ASSERT_TRUE(fake_ipc_client->ConnectViaIpc(channel_name));
  // Client and Server will each signal us once when OnChannelConenect() is
  // called so we wait on complete twice.  The order in which each is signaled
  // is not important.
  WaitForOperationComplete();
  WaitForOperationComplete();

  // Verify the connection details have been passed to the client.
  std::string new_channel_name = fake_ipc_client->last_message_received();
  ASSERT_FALSE(new_channel_name.empty());

  // Verify the internal state of the SecurityKeyAuthHandler is correct.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(expected_connection_id));
  ASSERT_EQ(expected_connection_count,
            auth_handler_->GetActiveConnectionCountForTest());

  if (close_connection) {
    fake_ipc_client->CloseIpcConnection();
    WaitForOperationComplete();
  }
}

void SecurityKeyAuthHandlerWinTest::SendRequestToSecurityKeyAuthHandler(
    const base::WeakPtr<FakeSecurityKeyIpcServer>& fake_ipc_server,
    int connection_id,
    const std::string& request_payload) {
  size_t expected_connection_count =
      auth_handler_->GetActiveConnectionCountForTest();
  // Send a security key request using the fake IPC server.
  fake_ipc_server->SendRequest(request_payload);
  WaitForOperationComplete();

  // Verify the FakeSecurityKeyIpcServer instance was not destroyed.
  ASSERT_TRUE(fake_ipc_server.get());

  // Verify the request was received.
  ASSERT_EQ(connection_id, last_connection_id_received_);
  ASSERT_EQ(request_payload, last_message_received_);

  // Verify the internal state of the SecurityKeyAuthHandler is still correct.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(connection_id));
  ASSERT_EQ(expected_connection_count,
            auth_handler_->GetActiveConnectionCountForTest());
}

void SecurityKeyAuthHandlerWinTest::SendResponseViaSecurityKeyAuthHandler(
    const base::WeakPtr<FakeSecurityKeyIpcServer>& fake_ipc_server,
    int connection_id,
    const std::string& response_payload) {
  size_t expected_connection_count =
      auth_handler_->GetActiveConnectionCountForTest();

  // Send a security key response using the new IPC channel.
  auth_handler_->SendClientResponse(connection_id, response_payload);
  WaitForOperationComplete();

  // Verify the security key response was received.
  ASSERT_EQ(response_payload, fake_ipc_server->last_message_received());

  // Verify the internal state of the SecurityKeyAuthHandler is still correct.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(connection_id));
  ASSERT_EQ(expected_connection_count,
            auth_handler_->GetActiveConnectionCountForTest());
}

void SecurityKeyAuthHandlerWinTest::CloseSecurityKeySessionIpcChannel(
    const base::WeakPtr<FakeSecurityKeyIpcServer>& fake_ipc_server,
    int connection_id) {
  size_t expected_connection_count =
      auth_handler_->GetActiveConnectionCountForTest() - 1;

  fake_ipc_server->CloseChannel();

  // Verify the internal state has been updated.
  ASSERT_FALSE(auth_handler_->IsValidConnectionId(connection_id));
  ASSERT_EQ(expected_connection_count,
            auth_handler_->GetActiveConnectionCountForTest());

  // Verify the FakeSecurityKeyIpcServer instance was destroyed.
  ASSERT_FALSE(fake_ipc_server.get());
}

std::string SecurityKeyAuthHandlerWinTest::GetUniqueTestChannelName() {
  std::string channel_name("Uber_Awesome_Super_Mega_Test_Channel.");
  channel_name.append(IPC::Channel::GenerateUniqueRandomChannelID());

  return channel_name;
}

TEST_F(SecurityKeyAuthHandlerWinTest, HandleSingleSecurityKeyRequest) {
  std::string channel_name(GetUniqueTestChannelName());
  CreateSecurityKeyConnection(channel_name);

  // Create a fake client and connect to the IPC server channel.
  FakeSecurityKeyIpcClient fake_ipc_client(
      base::Bind(&SecurityKeyAuthHandlerWinTest::OperationComplete,
                 base::Unretained(this)));
  EstablishInitialIpcConnection(&fake_ipc_client, kConnectionId1, channel_name,
                                /*close_connection=*/true);

  // Connect to the private IPC server channel created for this client.
  std::string new_channel_name = fake_ipc_client.last_message_received();

  // Retrieve the IPC server instance created when the client connected.
  base::WeakPtr<FakeSecurityKeyIpcServer> fake_ipc_server =
      ipc_server_factory_.GetIpcServerObject(kConnectionId1);
  ASSERT_TRUE(fake_ipc_server.get());
  ASSERT_EQ(new_channel_name, fake_ipc_server->channel_name());

  fake_ipc_server->set_send_response_callback(
      base::Bind(&SecurityKeyAuthHandlerWinTest::OperationComplete,
                 base::Unretained(this)));

  // Send a security key request using the fake IPC server.
  SendRequestToSecurityKeyAuthHandler(fake_ipc_server, kConnectionId1,
                                      "0123456789");

  // Send a security key response using the new IPC channel.
  SendResponseViaSecurityKeyAuthHandler(fake_ipc_server, kConnectionId1,
                                        "9876543210");

  CloseSecurityKeySessionIpcChannel(fake_ipc_server, kConnectionId1);
}

TEST_F(SecurityKeyAuthHandlerWinTest, HandleConcurrentSecurityKeyRequests) {
  std::string channel_name(GetUniqueTestChannelName());
  CreateSecurityKeyConnection(channel_name);

  // Create fake clients and connect each to the IPC server channel.
  FakeSecurityKeyIpcClient fake_ipc_client_1(
      base::Bind(&SecurityKeyAuthHandlerWinTest::OperationComplete,
                 base::Unretained(this)));
  FakeSecurityKeyIpcClient fake_ipc_client_2(
      base::Bind(&SecurityKeyAuthHandlerWinTest::OperationComplete,
                 base::Unretained(this)));

  EstablishInitialIpcConnection(&fake_ipc_client_1, kConnectionId1,
                                channel_name,
                                /*close_connection=*/true);
  EstablishInitialIpcConnection(&fake_ipc_client_2, kConnectionId2,
                                channel_name,
                                /*close_connection=*/true);

  // Verify the connection details have been passed to the client.
  std::string channel_name_1 = fake_ipc_client_1.last_message_received();
  std::string channel_name_2 = fake_ipc_client_2.last_message_received();
  ASSERT_NE(channel_name_1, channel_name_2);

  base::WeakPtr<FakeSecurityKeyIpcServer> fake_ipc_server_1 =
      ipc_server_factory_.GetIpcServerObject(kConnectionId1);
  ASSERT_TRUE(fake_ipc_server_1.get());
  ASSERT_EQ(channel_name_1, fake_ipc_server_1->channel_name());

  base::WeakPtr<FakeSecurityKeyIpcServer> fake_ipc_server_2 =
      ipc_server_factory_.GetIpcServerObject(kConnectionId2);
  ASSERT_TRUE(fake_ipc_server_2.get());
  ASSERT_EQ(channel_name_2, fake_ipc_server_2->channel_name());

  fake_ipc_server_1->set_send_response_callback(
      base::Bind(&SecurityKeyAuthHandlerWinTest::OperationComplete,
                 base::Unretained(this)));
  fake_ipc_server_2->set_send_response_callback(
      base::Bind(&SecurityKeyAuthHandlerWinTest::OperationComplete,
                 base::Unretained(this)));

  // Connect and send a security key request using the first IPC channel.
  SendRequestToSecurityKeyAuthHandler(fake_ipc_server_1, kConnectionId1,
                                      "aaaaaaaaaa");

  // Send a security key request using the second IPC channel.
  SendRequestToSecurityKeyAuthHandler(fake_ipc_server_1, kConnectionId1,
                                      "bbbbbbbbbb");

  // Send a security key response using the first IPC channel.
  SendResponseViaSecurityKeyAuthHandler(fake_ipc_server_2, kConnectionId2,
                                        "cccccccccc");

  // Send a security key response using the second IPC channel.
  SendResponseViaSecurityKeyAuthHandler(fake_ipc_server_2, kConnectionId2,
                                        "dddddddddd");

  // Close the IPC channels.
  CloseSecurityKeySessionIpcChannel(fake_ipc_server_1, kConnectionId1);
  CloseSecurityKeySessionIpcChannel(fake_ipc_server_2, kConnectionId2);
}

TEST_F(SecurityKeyAuthHandlerWinTest, HandleSequentialSecurityKeyRequests) {
  std::string channel_name(GetUniqueTestChannelName());
  CreateSecurityKeyConnection(channel_name);

  // Create fake clients to connect to the IPC server channel.
  FakeSecurityKeyIpcClient fake_ipc_client_1(
      base::Bind(&SecurityKeyAuthHandlerWinTest::OperationComplete,
                 base::Unretained(this)));

  EstablishInitialIpcConnection(&fake_ipc_client_1, kConnectionId1,
                                channel_name,
                                /*close_connection=*/true);

  // Verify the connection details have been passed to the client.
  std::string channel_name_1 = fake_ipc_client_1.last_message_received();

  base::WeakPtr<FakeSecurityKeyIpcServer> fake_ipc_server_1 =
      ipc_server_factory_.GetIpcServerObject(kConnectionId1);
  ASSERT_TRUE(fake_ipc_server_1.get());
  ASSERT_EQ(channel_name_1, fake_ipc_server_1->channel_name());

  fake_ipc_server_1->set_send_response_callback(
      base::Bind(&SecurityKeyAuthHandlerWinTest::OperationComplete,
                 base::Unretained(this)));

  // Send a security key request using the first IPC channel.
  SendRequestToSecurityKeyAuthHandler(fake_ipc_server_1, kConnectionId1,
                                      "aaaaaaaaaa");

  // Send a security key response using the first IPC channel.
  SendResponseViaSecurityKeyAuthHandler(fake_ipc_server_1, kConnectionId1,
                                        "cccccccccc");

  // Close the IPC channel.
  CloseSecurityKeySessionIpcChannel(fake_ipc_server_1, kConnectionId1);

  // Now connect with a second client.
  FakeSecurityKeyIpcClient fake_ipc_client_2(
      base::Bind(&SecurityKeyAuthHandlerWinTest::OperationComplete,
                 base::Unretained(this)));
  EstablishInitialIpcConnection(&fake_ipc_client_2, kConnectionId2,
                                channel_name,
                                /*close_connection=*/true);

  std::string channel_name_2 = fake_ipc_client_2.last_message_received();
  ASSERT_NE(channel_name_1, channel_name_2);

  base::WeakPtr<FakeSecurityKeyIpcServer> fake_ipc_server_2 =
      ipc_server_factory_.GetIpcServerObject(kConnectionId2);
  ASSERT_TRUE(fake_ipc_server_2.get());
  ASSERT_EQ(channel_name_2, fake_ipc_server_2->channel_name());

  fake_ipc_server_2->set_send_response_callback(
      base::Bind(&SecurityKeyAuthHandlerWinTest::OperationComplete,
                 base::Unretained(this)));

  // Send a security key request using the second IPC channel.
  SendRequestToSecurityKeyAuthHandler(fake_ipc_server_2, kConnectionId2,
                                      "bbbbbbbbbb");

  // Send a security key response using the second IPC channel.
  SendResponseViaSecurityKeyAuthHandler(fake_ipc_server_2, kConnectionId2,
                                        "dddddddddd");

  // Close the IPC channel.
  CloseSecurityKeySessionIpcChannel(fake_ipc_server_2, kConnectionId2);
}

TEST_F(SecurityKeyAuthHandlerWinTest,
       ClientNeverDisconnectsFromInitialIpcChannel) {
  const int kLowConnectionTimeoutInMs = 25;
  auth_handler_->SetRequestTimeoutForTest(
      base::TimeDelta::FromMilliseconds(kLowConnectionTimeoutInMs));

  std::string channel_name(GetUniqueTestChannelName());
  CreateSecurityKeyConnection(channel_name);

  // Create a fake client and connect to the IPC server channel.
  FakeSecurityKeyIpcClient fake_ipc_client(
      base::Bind(&SecurityKeyAuthHandlerWinTest::OperationComplete,
                 base::Unretained(this)));
  EstablishInitialIpcConnection(&fake_ipc_client, kConnectionId1, channel_name,
                                /*close_connection=*/false);

  // Don't close the channel here, instead wait for the SecurityKeyAuthHandler
  // to close the connection due to the timeout.
  WaitForOperationComplete();

  // Verify the connection that was set up still exists.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(kConnectionId1));
  ASSERT_EQ(1u, auth_handler_->GetActiveConnectionCountForTest());

  // Attempt to connect again after the error.
  EstablishInitialIpcConnection(&fake_ipc_client, kConnectionId2, channel_name,
                                /*close_connection=*/true);
}

TEST_F(SecurityKeyAuthHandlerWinTest, HandleSecurityKeyRequestTimeout) {
  std::string channel_name(GetUniqueTestChannelName());
  CreateSecurityKeyConnection(channel_name);

  // Create a fake client and connect to the IPC server channel.
  FakeSecurityKeyIpcClient fake_ipc_client(
      base::Bind(&SecurityKeyAuthHandlerWinTest::OperationComplete,
                 base::Unretained(this)));
  EstablishInitialIpcConnection(&fake_ipc_client, kConnectionId1, channel_name,
                                /*close_connection=*/true);

  // Connect to the private IPC server channel created for this client.
  std::string new_channel_name = fake_ipc_client.last_message_received();

  // Retrieve the IPC server instance created when the client connected.
  base::WeakPtr<FakeSecurityKeyIpcServer> fake_ipc_server =
      ipc_server_factory_.GetIpcServerObject(kConnectionId1);
  ASSERT_TRUE(fake_ipc_server.get());
  ASSERT_EQ(new_channel_name, fake_ipc_server->channel_name());

  fake_ipc_server->set_send_response_callback(
      base::Bind(&SecurityKeyAuthHandlerWinTest::OperationComplete,
                 base::Unretained(this)));

  // Simulate a timeout and verify the IPC server is cleaned up.
  CloseSecurityKeySessionIpcChannel(fake_ipc_server, kConnectionId1);

  // Attempt to connect again after the error.
  EstablishInitialIpcConnection(&fake_ipc_client, kConnectionId2, channel_name,
                                /*close_connection=*/true);
}

TEST_F(SecurityKeyAuthHandlerWinTest, HandleSecurityKeyErrorResponse) {
  std::string channel_name(GetUniqueTestChannelName());
  CreateSecurityKeyConnection(channel_name);

  // Create a fake client and connect to the IPC server channel.
  FakeSecurityKeyIpcClient fake_ipc_client(
      base::Bind(&SecurityKeyAuthHandlerWinTest::OperationComplete,
                 base::Unretained(this)));
  EstablishInitialIpcConnection(&fake_ipc_client, kConnectionId1, channel_name,
                                /*close_connection=*/true);

  // Connect to the private IPC server channel created for this client.
  std::string new_channel_name = fake_ipc_client.last_message_received();

  // Retrieve the IPC server instance created when the client connected.
  base::WeakPtr<FakeSecurityKeyIpcServer> fake_ipc_server =
      ipc_server_factory_.GetIpcServerObject(kConnectionId1);
  ASSERT_TRUE(fake_ipc_server.get());
  ASSERT_EQ(new_channel_name, fake_ipc_server->channel_name());

  fake_ipc_server->set_send_response_callback(
      base::Bind(&SecurityKeyAuthHandlerWinTest::OperationComplete,
                 base::Unretained(this)));

  // Send a security key request using the fake IPC server.
  SendRequestToSecurityKeyAuthHandler(fake_ipc_server, kConnectionId1,
                                      "0123456789");

  // Simulate a security key error from the client.
  auth_handler_->SendErrorAndCloseConnection(kConnectionId1);
  // Wait for the ipc server channel to be torn down.
  WaitForOperationComplete();

  // Verify the connection was cleaned up.
  ASSERT_FALSE(fake_ipc_server.get());
  ASSERT_FALSE(auth_handler_->IsValidConnectionId(kConnectionId1));
  ASSERT_EQ(0u, auth_handler_->GetActiveConnectionCountForTest());

  // Attempt to connect again after the error.
  EstablishInitialIpcConnection(&fake_ipc_client, kConnectionId2, channel_name,
                                /*close_connection=*/true);
}

TEST_F(SecurityKeyAuthHandlerWinTest, IpcConnectionFailsFromInvalidSession) {
  std::string channel_name(GetUniqueTestChannelName());
  CreateSecurityKeyConnection(channel_name);

  // Set the current session id to a 'different' session.
  desktop_session_id_ += 1;

  // Create a fake client and connect to the IPC server channel.
  FakeSecurityKeyIpcClient fake_ipc_client(
      base::Bind(&SecurityKeyAuthHandlerWinTest::OperationComplete,
                 base::Unretained(this)));
  ASSERT_TRUE(fake_ipc_client.ConnectViaIpc(channel_name));
  // Wait for the error callback to be signaled.
  WaitForOperationComplete();

  // Verify the connection was not set up.
  ASSERT_FALSE(auth_handler_->IsValidConnectionId(kConnectionId1));
  ASSERT_EQ(0u, auth_handler_->GetActiveConnectionCountForTest());
}

}  // namespace remoting
