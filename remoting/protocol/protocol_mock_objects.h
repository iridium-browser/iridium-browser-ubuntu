// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PROTOCOL_MOCK_OBJECTS_H_
#define REMOTING_PROTOCOL_PROTOCOL_MOCK_OBJECTS_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/values.h"
#include "net/base/ip_endpoint.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/pairing_registry.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/session_manager.h"
#include "remoting/protocol/transport.h"
#include "remoting/protocol/video_stub.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {
namespace protocol {

class MockConnectionToClientEventHandler
    : public ConnectionToClient::EventHandler {
 public:
  MockConnectionToClientEventHandler();
  ~MockConnectionToClientEventHandler() override;

  MOCK_METHOD1(OnConnectionAuthenticating,
               void(ConnectionToClient* connection));
  MOCK_METHOD1(OnConnectionAuthenticated, void(ConnectionToClient* connection));
  MOCK_METHOD1(CreateVideoStreams, void(ConnectionToClient* connection));
  MOCK_METHOD1(OnConnectionChannelsConnected,
               void(ConnectionToClient* connection));
  MOCK_METHOD2(OnConnectionClosed,
               void(ConnectionToClient* connection, ErrorCode error));
  MOCK_METHOD1(OnCreateVideoEncoder,
               void(std::unique_ptr<VideoEncoder>* encoder));
  MOCK_METHOD2(OnInputEventReceived,
               void(ConnectionToClient* connection, int64_t timestamp));
  MOCK_METHOD3(OnRouteChange,
               void(ConnectionToClient* connection,
                    const std::string& channel_name,
                    const TransportRoute& route));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockConnectionToClientEventHandler);
};

class MockClipboardStub : public ClipboardStub {
 public:
  MockClipboardStub();
  ~MockClipboardStub() override;

  MOCK_METHOD1(InjectClipboardEvent, void(const ClipboardEvent& event));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClipboardStub);
};

class MockCursorShapeChangeCallback {
 public:
  MockCursorShapeChangeCallback();
  virtual ~MockCursorShapeChangeCallback();

  MOCK_METHOD1(CursorShapeChangedPtr, void(CursorShapeInfo* info));
  void CursorShapeChanged(std::unique_ptr<CursorShapeInfo> info);

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCursorShapeChangeCallback);
};

class MockInputStub : public InputStub {
 public:
  MockInputStub();
  ~MockInputStub() override;

  MOCK_METHOD1(InjectKeyEvent, void(const KeyEvent& event));
  MOCK_METHOD1(InjectTextEvent, void(const TextEvent& event));
  MOCK_METHOD1(InjectMouseEvent, void(const MouseEvent& event));
  MOCK_METHOD1(InjectTouchEvent, void(const TouchEvent& event));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInputStub);
};

class MockHostStub : public HostStub {
 public:
  MockHostStub();
  ~MockHostStub() override;

  MOCK_METHOD1(NotifyClientResolution,
               void(const ClientResolution& resolution));
  MOCK_METHOD1(ControlVideo, void(const VideoControl& video_control));
  MOCK_METHOD1(ControlAudio, void(const AudioControl& audio_control));
  MOCK_METHOD1(SetCapabilities, void(const Capabilities& capabilities));
  MOCK_METHOD1(RequestPairing, void(const PairingRequest& pairing_request));
  MOCK_METHOD1(DeliverClientMessage, void(const ExtensionMessage& message));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockHostStub);
};

class MockClientStub : public ClientStub {
 public:
  MockClientStub();
  ~MockClientStub() override;

  // ClientStub mock implementation.
  MOCK_METHOD1(SetCapabilities, void(const Capabilities& capabilities));
  MOCK_METHOD1(SetPairingResponse,
               void(const PairingResponse& pairing_response));
  MOCK_METHOD1(DeliverHostMessage, void(const ExtensionMessage& message));
  MOCK_METHOD1(SetVideoLayout, void(const VideoLayout& layout));

  // ClipboardStub mock implementation.
  MOCK_METHOD1(InjectClipboardEvent, void(const ClipboardEvent& event));

  // CursorShapeStub mock implementation.
  MOCK_METHOD1(SetCursorShape, void(const CursorShapeInfo& cursor_shape));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClientStub);
};

class MockCursorShapeStub : public CursorShapeStub {
 public:
  MockCursorShapeStub();
  ~MockCursorShapeStub() override;

  MOCK_METHOD1(SetCursorShape, void(const CursorShapeInfo& cursor_shape));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCursorShapeStub);
};

class MockVideoStub : public VideoStub {
 public:
  MockVideoStub();
  ~MockVideoStub() override;

  MOCK_METHOD2(ProcessVideoPacketPtr,
               void(const VideoPacket* video_packet,
                    const base::Closure& done));
  void ProcessVideoPacket(std::unique_ptr<VideoPacket> video_packet,
                          const base::Closure& done) override {
    ProcessVideoPacketPtr(video_packet.get(), done);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoStub);
};

class MockSession : public Session {
 public:
  MockSession();
  ~MockSession() override;

  MOCK_METHOD1(SetEventHandler, void(Session::EventHandler* event_handler));
  MOCK_METHOD0(error, ErrorCode());
  MOCK_METHOD1(SetTransport, void(Transport*));
  MOCK_METHOD0(jid, const std::string&());
  MOCK_METHOD0(config, const SessionConfig&());
  MOCK_METHOD1(Close, void(ErrorCode error));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSession);
};

class MockSessionManager : public SessionManager {
 public:
  MockSessionManager();
  ~MockSessionManager() override;

  MOCK_METHOD1(AcceptIncoming, void(const IncomingSessionCallback&));
  void set_protocol_config(
      std::unique_ptr<CandidateSessionConfig> config) override {}
  MOCK_METHOD2(ConnectPtr,
               Session*(const std::string& host_jid,
                        Authenticator* authenticator));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD1(set_authenticator_factory_ptr,
               void(AuthenticatorFactory* factory));
  std::unique_ptr<Session> Connect(
      const std::string& host_jid,
      std::unique_ptr<Authenticator> authenticator) override {
    return base::WrapUnique(ConnectPtr(host_jid, authenticator.get()));
  }
  void set_authenticator_factory(
      std::unique_ptr<AuthenticatorFactory> authenticator_factory) override {
    set_authenticator_factory_ptr(authenticator_factory.release());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSessionManager);
};

// Simple delegate that caches information on paired clients in memory.
class MockPairingRegistryDelegate : public PairingRegistry::Delegate {
 public:
  MockPairingRegistryDelegate();
  ~MockPairingRegistryDelegate() override;

  // PairingRegistry::Delegate implementation.
  std::unique_ptr<base::ListValue> LoadAll() override;
  bool DeleteAll() override;
  protocol::PairingRegistry::Pairing Load(
      const std::string& client_id) override;
  bool Save(const protocol::PairingRegistry::Pairing& pairing) override;
  bool Delete(const std::string& client_id) override;

 private:
  typedef std::map<std::string, protocol::PairingRegistry::Pairing> Pairings;
  Pairings pairings_;
};

class SynchronousPairingRegistry : public PairingRegistry {
 public:
  explicit SynchronousPairingRegistry(std::unique_ptr<Delegate> delegate);

 protected:
  ~SynchronousPairingRegistry() override;

  // Runs tasks synchronously instead of posting them to |task_runner|.
  void PostTask(const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
                const tracked_objects::Location& from_here,
                const base::Closure& task) override;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PROTOCOL_MOCK_OBJECTS_H_
