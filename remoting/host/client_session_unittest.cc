// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/test_simple_task_runner.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/constants.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/client_session.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/fake_desktop_capturer.h"
#include "remoting/host/fake_host_extension.h"
#include "remoting/host/fake_mouse_cursor_monitor.h"
#include "remoting/host/host_extension.h"
#include "remoting/host/host_extension_session.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/test_event_matchers.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock_mutant.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer_mock_objects.h"

namespace remoting {

using protocol::MockClientStub;
using protocol::MockConnectionToClient;
using protocol::MockHostStub;
using protocol::MockInputStub;
using protocol::MockSession;
using protocol::MockVideoStub;
using protocol::SessionConfig;
using protocol::test::EqualsClipboardEvent;
using protocol::test::EqualsMouseButtonEvent;
using protocol::test::EqualsMouseMoveEvent;
using protocol::test::EqualsKeyEvent;

using testing::_;
using testing::AnyNumber;
using testing::AtMost;
using testing::AtLeast;
using testing::CreateFunctor;
using testing::DeleteArg;
using testing::DoAll;
using testing::Expectation;
using testing::Invoke;
using testing::Return;
using testing::ReturnRef;
using testing::Sequence;
using testing::StrEq;
using testing::StrictMock;

namespace {

const char kDefaultTestCapability[] = "default";

ACTION_P2(InjectClipboardEvent, connection, event) {
  connection->clipboard_stub()->InjectClipboardEvent(event);
}

ACTION_P2(InjectKeyEvent, connection, event) {
  connection->input_stub()->InjectKeyEvent(event);
}

ACTION_P2(InjectMouseEvent, connection, event) {
  connection->input_stub()->InjectMouseEvent(event);
}

ACTION_P2(LocalMouseMoved, client_session, event) {
  client_session->OnLocalMouseMoved(
      webrtc::DesktopVector(event.x(), event.y()));
}

ACTION_P2(SetGnubbyAuthHandlerForTesting, client_session, gnubby_auth_handler) {
  client_session->SetGnubbyAuthHandlerForTesting(gnubby_auth_handler);
}

ACTION_P2(DeliverClientMessage, client_session, message) {
  client_session->DeliverClientMessage(message);
}

ACTION_P2(SetCapabilities, client_session, capabilities) {
  protocol::Capabilities capabilities_message;
  capabilities_message.set_capabilities(capabilities);
  client_session->SetCapabilities(capabilities_message);
}

// Matches a |protocol::Capabilities| argument against a list of capabilities
// formatted as a space-separated string.
MATCHER_P(EqCapabilities, expected_capabilities, "") {
  if (!arg.has_capabilities())
    return false;

  std::vector<std::string> words_args = base::SplitString(
      arg.capabilities(), " ", base::KEEP_WHITESPACE,
      base::SPLIT_WANT_NONEMPTY);
  std::vector<std::string> words_expected = base::SplitString(
      expected_capabilities, " ", base::KEEP_WHITESPACE,
      base::SPLIT_WANT_NONEMPTY);
  std::sort(words_args.begin(), words_args.end());
  std::sort(words_expected.begin(), words_expected.end());
  return words_args == words_expected;
}

}  // namespace

class ClientSessionTest : public testing::Test {
 public:
  ClientSessionTest() : client_jid_("user@domain/rest-of-jid") {}

  void SetUp() override;
  void TearDown() override;

  // Creates the client session.
  void CreateClientSession();

  // Disconnects the client session.
  void DisconnectClientSession();

  // Stops and releases the ClientSession, allowing the MessageLoop to quit.
  void StopClientSession();

 protected:
  // Creates a DesktopEnvironment with a fake webrtc::DesktopCapturer, to mock
  // DesktopEnvironmentFactory::Create().
  DesktopEnvironment* CreateDesktopEnvironment();

  // Returns |input_injector_| created and initialized by SetUp(), to mock
  // DesktopEnvironment::CreateInputInjector().
  InputInjector* CreateInputInjector();

  // Creates a fake webrtc::DesktopCapturer, to mock
  // DesktopEnvironment::CreateVideoCapturer().
  webrtc::DesktopCapturer* CreateVideoCapturer();

  // Creates a MockMouseCursorMonitor, to mock
  // DesktopEnvironment::CreateMouseCursorMonitor
  webrtc::MouseCursorMonitor* CreateMouseCursorMonitor();

  // Notifies the client session that the client connection has been
  // authenticated and channels have been connected. This effectively enables
  // the input pipe line and starts video capturing.
  void ConnectClientSession();

  // Creates expectation that simulates client supporting same capabilities as
  // host.
  void SetMatchCapabilitiesExpectation();

  // Creates expectations to send an extension message and to disconnect
  // afterwards.
  void SetSendMessageAndDisconnectExpectation(const std::string& message_type);

  // Message loop that will process all ClientSession tasks.
  base::MessageLoop message_loop_;

  // AutoThreadTaskRunner on which |client_session_| will be run.
  scoped_refptr<AutoThreadTaskRunner> task_runner_;

  // Used to run |message_loop_| after each test, until no objects remain that
  // require it.
  base::RunLoop run_loop_;

  // HostExtensions to pass when creating the ClientSession. Caller retains
  // ownership of the HostExtensions themselves.
  std::vector<HostExtension*> extensions_;

  // ClientSession instance under test.
  scoped_ptr<ClientSession> client_session_;

  // ClientSession::EventHandler mock for use in tests.
  MockClientSessionEventHandler session_event_handler_;

  // Storage for values to be returned by the protocol::Session mock.
  scoped_ptr<SessionConfig> session_config_;
  const std::string client_jid_;

  // Stubs returned to |client_session_| components by |connection_|.
  MockClientStub client_stub_;
  MockVideoStub video_stub_;

  // DesktopEnvironment owns |input_injector_|, but input injection tests need
  // to express expectations on it.
  scoped_ptr<MockInputInjector> input_injector_;

  // ClientSession owns |connection_| but tests need it to inject fake events.
  MockConnectionToClient* connection_;

  scoped_ptr<MockDesktopEnvironmentFactory> desktop_environment_factory_;
};

void ClientSessionTest::SetUp() {
  // Arrange to run |message_loop_| until no components depend on it.
  task_runner_ = new AutoThreadTaskRunner(
      message_loop_.task_runner(), run_loop_.QuitClosure());

  desktop_environment_factory_.reset(new MockDesktopEnvironmentFactory());
  EXPECT_CALL(*desktop_environment_factory_, CreatePtr())
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this,
                             &ClientSessionTest::CreateDesktopEnvironment));
  EXPECT_CALL(*desktop_environment_factory_, SupportsAudioCapture())
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));

  input_injector_.reset(new MockInputInjector());

  session_config_ = SessionConfig::ForTest();
}

void ClientSessionTest::TearDown() {
  // Clear out |task_runner_| reference so the loop can quit, and run it until
  // it does.
  task_runner_ = nullptr;
  run_loop_.Run();
}

void ClientSessionTest::CreateClientSession() {
  // Mock protocol::Session APIs called directly by ClientSession.
  protocol::MockSession* session = new MockSession();
  EXPECT_CALL(*session, config()).WillRepeatedly(ReturnRef(*session_config_));
  EXPECT_CALL(*session, jid()).WillRepeatedly(ReturnRef(client_jid_));
  EXPECT_CALL(*session, SetEventHandler(_));

  // Mock protocol::ConnectionToClient APIs called directly by ClientSession.
  // HostStub is not touched by ClientSession, so we can safely pass nullptr.
  scoped_ptr<MockConnectionToClient> connection(
      new MockConnectionToClient(session, nullptr));
  EXPECT_CALL(*connection, session()).WillRepeatedly(Return(session));
  EXPECT_CALL(*connection, client_stub())
      .WillRepeatedly(Return(&client_stub_));
  EXPECT_CALL(*connection, video_stub()).WillRepeatedly(Return(&video_stub_));
  EXPECT_CALL(*connection, Disconnect());
  connection_ = connection.get();

  client_session_.reset(new ClientSession(
      &session_event_handler_,
      task_runner_,  // Audio thread.
      task_runner_,  // Input thread.
      task_runner_,  // Capture thread.
      task_runner_,  // Encode thread.
      task_runner_,  // Network thread.
      task_runner_,  // UI thread.
      connection.Pass(),
      desktop_environment_factory_.get(),
      base::TimeDelta(),
      nullptr,
      extensions_));
}

void ClientSessionTest::DisconnectClientSession() {
  client_session_->DisconnectSession();
  // MockSession won't trigger OnConnectionClosed, so fake it.
  client_session_->OnConnectionClosed(client_session_->connection(),
                                      protocol::OK);
}

void ClientSessionTest::StopClientSession() {
  client_session_.reset();

  desktop_environment_factory_.reset();
}

DesktopEnvironment* ClientSessionTest::CreateDesktopEnvironment() {
  MockDesktopEnvironment* desktop_environment = new MockDesktopEnvironment();
  EXPECT_CALL(*desktop_environment, CreateAudioCapturerPtr())
      .Times(0);
  EXPECT_CALL(*desktop_environment, CreateInputInjectorPtr())
      .WillOnce(Invoke(this, &ClientSessionTest::CreateInputInjector));
  EXPECT_CALL(*desktop_environment, CreateScreenControlsPtr())
      .Times(AtMost(1));
  EXPECT_CALL(*desktop_environment, CreateVideoCapturerPtr())
      .WillRepeatedly(Invoke(this, &ClientSessionTest::CreateVideoCapturer));
  EXPECT_CALL(*desktop_environment, CreateMouseCursorMonitorPtr())
      .WillRepeatedly(
          Invoke(this, &ClientSessionTest::CreateMouseCursorMonitor));
  EXPECT_CALL(*desktop_environment, GetCapabilities())
      .Times(AtMost(1))
      .WillOnce(Return(kDefaultTestCapability));
  EXPECT_CALL(*desktop_environment, SetCapabilities(_))
      .Times(AtMost(1));

  return desktop_environment;
}

InputInjector* ClientSessionTest::CreateInputInjector() {
  EXPECT_TRUE(input_injector_);
  return input_injector_.release();
}

webrtc::DesktopCapturer* ClientSessionTest::CreateVideoCapturer() {
  return new FakeDesktopCapturer();
}

webrtc::MouseCursorMonitor* ClientSessionTest::CreateMouseCursorMonitor() {
  return new FakeMouseCursorMonitor();
}

void ClientSessionTest::ConnectClientSession() {
  // Stubs should be set only after connection is authenticated.
  EXPECT_FALSE(connection_->clipboard_stub());
  EXPECT_FALSE(connection_->input_stub());

  client_session_->OnConnectionAuthenticated(client_session_->connection());

  EXPECT_TRUE(connection_->clipboard_stub());
  EXPECT_TRUE(connection_->input_stub());

  client_session_->OnConnectionChannelsConnected(client_session_->connection());
}

void ClientSessionTest::SetMatchCapabilitiesExpectation() {
  // Set the client to report the same capabilities as the host.
  EXPECT_CALL(client_stub_, SetCapabilities(_))
      .Times(AtMost(1))
      .WillOnce(Invoke(client_session_.get(), &ClientSession::SetCapabilities));
}

void ClientSessionTest::SetSendMessageAndDisconnectExpectation(
    const std::string& message_type) {
  protocol::ExtensionMessage message;
  message.set_type(message_type);
  message.set_data("data");

  Expectation authenticated =
      EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_))
          .WillOnce(Return(true));
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_))
      .After(authenticated)
      .WillOnce(DoAll(
          DeliverClientMessage(client_session_.get(), message),
          InvokeWithoutArgs(this, &ClientSessionTest::DisconnectClientSession),
          InvokeWithoutArgs(this, &ClientSessionTest::StopClientSession)));
}

TEST_F(ClientSessionTest, ClipboardStubFilter) {
  CreateClientSession();

  EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*input_injector_, StartPtr(_));
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_));

  // Wait for the first video packet to be captured to make sure that
  // the injected input will go though. Otherwise mouse events will be blocked
  // by the mouse clamping filter.
  base::RunLoop run_loop;
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .Times(AtLeast(1))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  {
    EXPECT_CALL(*input_injector_, InjectClipboardEvent(EqualsClipboardEvent(
                                      kMimeTypeTextUtf8, "a")));
    EXPECT_CALL(*input_injector_, InjectKeyEvent(EqualsKeyEvent(1, true)));
    EXPECT_CALL(*input_injector_, InjectKeyEvent(EqualsKeyEvent(1, false)));
    EXPECT_CALL(*input_injector_,
                InjectMouseEvent(EqualsMouseMoveEvent(100, 101)));

    EXPECT_CALL(*input_injector_, InjectClipboardEvent(EqualsClipboardEvent(
                                      kMimeTypeTextUtf8, "c")));
    EXPECT_CALL(*input_injector_, InjectKeyEvent(EqualsKeyEvent(3, true)));
    EXPECT_CALL(*input_injector_,
                InjectMouseEvent(EqualsMouseMoveEvent(300, 301)));
    EXPECT_CALL(*input_injector_, InjectKeyEvent(EqualsKeyEvent(3, false)));
  }

  ConnectClientSession();

  // Wait for the first frame.
  run_loop.Run();

  // Inject test events that are expected to be injected.
  protocol::ClipboardEvent clipboard_event;
  clipboard_event.set_mime_type(kMimeTypeTextUtf8);
  clipboard_event.set_data("a");
  connection_->clipboard_stub()->InjectClipboardEvent(clipboard_event);

  protocol::KeyEvent key_event;
  key_event.set_pressed(true);
  key_event.set_usb_keycode(1);
  connection_->input_stub()->InjectKeyEvent(key_event);

  protocol::MouseEvent mouse_event;
  mouse_event.set_x(100);
  mouse_event.set_y(101);
  connection_->input_stub()->InjectMouseEvent(mouse_event);

  base::RunLoop().RunUntilIdle();

  // Disable input.
  client_session_->SetDisableInputs(true);

  // These event shouldn't get though to the input injector.
  clipboard_event.set_data("b");
  connection_->clipboard_stub()->InjectClipboardEvent(clipboard_event);

  key_event.set_pressed(true);
  key_event.set_usb_keycode(2);
  connection_->input_stub()->InjectKeyEvent(key_event);
  key_event.set_pressed(false);
  key_event.set_usb_keycode(2);
  connection_->input_stub()->InjectKeyEvent(key_event);

  mouse_event.set_x(200);
  mouse_event.set_y(201);
  connection_->input_stub()->InjectMouseEvent(mouse_event);

  base::RunLoop().RunUntilIdle();

  // Enable input again.
  client_session_->SetDisableInputs(false);

  clipboard_event.set_data("c");
  connection_->clipboard_stub()->InjectClipboardEvent(clipboard_event);
  base::RunLoop().RunUntilIdle();

  key_event.set_pressed(true);
  key_event.set_usb_keycode(3);
  connection_->input_stub()->InjectKeyEvent(key_event);

  mouse_event.set_x(300);
  mouse_event.set_y(301);
  connection_->input_stub()->InjectMouseEvent(mouse_event);

  client_session_->DisconnectSession();
  client_session_->OnConnectionClosed(connection_, protocol::OK);
  client_session_.reset();
}

TEST_F(ClientSessionTest, LocalInputTest) {
  CreateClientSession();

  protocol::MouseEvent mouse_event1;
  mouse_event1.set_x(100);
  mouse_event1.set_y(101);
  protocol::MouseEvent mouse_event2;
  mouse_event2.set_x(200);
  mouse_event2.set_y(201);
  protocol::MouseEvent mouse_event3;
  mouse_event3.set_x(300);
  mouse_event3.set_y(301);

  Expectation authenticated =
      EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_))
          .WillOnce(Return(true));
  EXPECT_CALL(*input_injector_, StartPtr(_))
      .After(authenticated);
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_))
      .After(authenticated);

  // Wait for the first video packet to be captured to make sure that
  // the injected input will go though. Otherwise mouse events will be blocked
  // by the mouse clamping filter.
  Sequence s;
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .InSequence(s)
      .After(authenticated)
      .WillOnce(DoAll(
          // This event should get through to the input stub.
          InjectMouseEvent(connection_, mouse_event1),
#if !defined(OS_WIN)
          // The OS echoes the injected event back.
          LocalMouseMoved(client_session_.get(), mouse_event1),
#endif  // !defined(OS_WIN)
          // This one should get throught as well.
          InjectMouseEvent(connection_, mouse_event2),
          // Now this is a genuine local event.
          LocalMouseMoved(client_session_.get(), mouse_event1),
          // This one should be blocked because of the previous  local input
          // event.
          InjectMouseEvent(connection_, mouse_event3),
          // TODO(jamiewalch): Verify that remote inputs are re-enabled
          // eventually (via dependency injection, not sleep!)
          InvokeWithoutArgs(this, &ClientSessionTest::DisconnectClientSession),
          InvokeWithoutArgs(this, &ClientSessionTest::StopClientSession)));
  EXPECT_CALL(*input_injector_,
              InjectMouseEvent(EqualsMouseMoveEvent(100, 101))).InSequence(s);
  EXPECT_CALL(*input_injector_,
              InjectMouseEvent(EqualsMouseMoveEvent(200, 201))).InSequence(s);
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_)).InSequence(s);

  ConnectClientSession();
}

TEST_F(ClientSessionTest, RestoreEventState) {
  CreateClientSession();

  protocol::KeyEvent key1;
  key1.set_pressed(true);
  key1.set_usb_keycode(1);

  protocol::KeyEvent key2;
  key2.set_pressed(true);
  key2.set_usb_keycode(2);

  protocol::MouseEvent mousedown;
  mousedown.set_button(protocol::MouseEvent::BUTTON_LEFT);
  mousedown.set_button_down(true);

  Expectation authenticated =
      EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_))
          .WillOnce(Return(true));
  EXPECT_CALL(*input_injector_, StartPtr(_)).After(authenticated);
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_))
      .After(authenticated);

  // Wait for the first video packet to be captured to make sure that
  // the injected input will go though. Otherwise mouse events will be blocked
  // by the mouse clamping filter.
  Sequence s;
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .InSequence(s)
      .After(authenticated)
      .WillOnce(DoAll(
          InjectKeyEvent(connection_, key1), InjectKeyEvent(connection_, key2),
          InjectMouseEvent(connection_, mousedown),
          InvokeWithoutArgs(this, &ClientSessionTest::DisconnectClientSession),
          InvokeWithoutArgs(this, &ClientSessionTest::StopClientSession)));
  EXPECT_CALL(*input_injector_, InjectKeyEvent(EqualsKeyEvent(1, true)))
      .InSequence(s);
  EXPECT_CALL(*input_injector_, InjectKeyEvent(EqualsKeyEvent(2, true)))
      .InSequence(s);
  EXPECT_CALL(*input_injector_, InjectMouseEvent(EqualsMouseButtonEvent(
      protocol::MouseEvent::BUTTON_LEFT, true)))
      .InSequence(s);
  EXPECT_CALL(*input_injector_, InjectKeyEvent(EqualsKeyEvent(1, false)))
      .InSequence(s);
  EXPECT_CALL(*input_injector_, InjectKeyEvent(EqualsKeyEvent(2, false)))
      .InSequence(s);
  EXPECT_CALL(*input_injector_, InjectMouseEvent(EqualsMouseButtonEvent(
      protocol::MouseEvent::BUTTON_LEFT, false)))
      .InSequence(s);
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_)).InSequence(s);

  ConnectClientSession();
}

TEST_F(ClientSessionTest, ClampMouseEvents) {
  CreateClientSession();

  Expectation authenticated =
      EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_))
          .WillOnce(Return(true));
  EXPECT_CALL(*input_injector_, StartPtr(_)).After(authenticated);
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_))
      .After(authenticated);
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_)).After(authenticated);

  Expectation connected = authenticated;

  int input_x[3] = { -999, 100, 999 };
  int expected_x[3] = { 0, 100, FakeDesktopCapturer::kWidth - 1 };
  int input_y[3] = { -999, 50, 999 };
  int expected_y[3] = { 0, 50, FakeDesktopCapturer::kHeight - 1 };

  protocol::MouseEvent expected_event;
  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 3; i++) {
      protocol::MouseEvent injected_event;
      injected_event.set_x(input_x[i]);
      injected_event.set_y(input_y[j]);

      if (i == 0 && j == 0) {
        // Inject the 1st event once a video packet has been received.
        connected =
            EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
                .After(connected)
                .WillOnce(InjectMouseEvent(connection_, injected_event));
      } else {
        // Every next event is injected once the previous event has been
        // received.
        connected =
            EXPECT_CALL(*input_injector_,
                        InjectMouseEvent(EqualsMouseMoveEvent(
                            expected_event.x(), expected_event.y())))
                .After(connected)
                .WillOnce(InjectMouseEvent(connection_, injected_event));
      }

      expected_event.set_x(expected_x[i]);
      expected_event.set_y(expected_y[j]);
    }
  }

  // Shutdown the connection once the last event has been received.
  EXPECT_CALL(*input_injector_, InjectMouseEvent(EqualsMouseMoveEvent(
                                    expected_event.x(), expected_event.y())))
      .After(connected)
      .WillOnce(DoAll(
          InvokeWithoutArgs(this, &ClientSessionTest::DisconnectClientSession),
          InvokeWithoutArgs(this, &ClientSessionTest::StopClientSession)));

  ConnectClientSession();
}

TEST_F(ClientSessionTest, NoGnubbyAuth) {
  CreateClientSession();

  protocol::ExtensionMessage message;
  message.set_type("gnubby-auth");
  message.set_data("test");

  Expectation authenticated =
      EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_))
          .WillOnce(Return(true));
  EXPECT_CALL(*input_injector_, StartPtr(_)).After(authenticated);
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_))
      .After(authenticated)
      .WillOnce(DoAll(
          DeliverClientMessage(client_session_.get(), message),
          InvokeWithoutArgs(this, &ClientSessionTest::DisconnectClientSession),
          InvokeWithoutArgs(this, &ClientSessionTest::StopClientSession)));
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_));

  ConnectClientSession();
}

TEST_F(ClientSessionTest, EnableGnubbyAuth) {
  CreateClientSession();

  // Lifetime controlled by object under test.
  MockGnubbyAuthHandler* gnubby_auth_handler = new MockGnubbyAuthHandler();

  protocol::ExtensionMessage message;
  message.set_type("gnubby-auth");
  message.set_data("test");

  Expectation authenticated =
      EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_))
          .WillOnce(Return(true));
  EXPECT_CALL(*input_injector_, StartPtr(_)).After(authenticated);
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_))
      .After(authenticated)
      .WillOnce(DoAll(
          SetGnubbyAuthHandlerForTesting(client_session_.get(),
                                         gnubby_auth_handler),
          DeliverClientMessage(client_session_.get(), message),
          InvokeWithoutArgs(this, &ClientSessionTest::DisconnectClientSession),
          InvokeWithoutArgs(this, &ClientSessionTest::StopClientSession)));
  EXPECT_CALL(*gnubby_auth_handler, DeliverClientMessage(_));
  EXPECT_CALL(session_event_handler_, OnSessionClosed(_));

  ConnectClientSession();
}

// Verifies that the client's video pipeline can be reset mid-session.
TEST_F(ClientSessionTest, ResetVideoPipeline) {
  CreateClientSession();

  EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_))
      .WillOnce(Return(true));

  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .WillOnce(DoAll(
          InvokeWithoutArgs(this, &ClientSessionTest::DisconnectClientSession),
          InvokeWithoutArgs(this, &ClientSessionTest::StopClientSession)));

  ConnectClientSession();

  client_session_->ResetVideoPipeline();
}

// Verifies that clients can have extensions registered, resulting in the
// correct capabilities being reported, and messages delivered correctly.
// The extension system is tested more extensively in the
// HostExtensionSessionManager unit-tests.
TEST_F(ClientSessionTest, Extensions) {
  // Configure fake extensions for testing.
  FakeExtension extension1("ext1", "cap1");
  extensions_.push_back(&extension1);
  FakeExtension extension2("ext2", "");
  extensions_.push_back(&extension2);
  FakeExtension extension3("ext3", "cap3");
  extensions_.push_back(&extension3);

  // Set the second extension to request to modify the video pipeline.
  extension2.set_steal_video_capturer(true);

  CreateClientSession();

  Expectation authenticated =
      EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_))
          .WillOnce(Return(true));

  // Verify that the ClientSession reports the correct capabilities, and mimic
  // the client reporting an overlapping set of capabilities.
  EXPECT_CALL(client_stub_,
              SetCapabilities(EqCapabilities("cap1 cap3 default")))
      .After(authenticated)
      .WillOnce(SetCapabilities(client_session_.get(), "cap1 cap4 default"));

  // Verify that the correct extension messages are delivered, and dropped.
  protocol::ExtensionMessage message1;
  message1.set_type("ext1");
  message1.set_data("data");
  protocol::ExtensionMessage message3;
  message3.set_type("ext3");
  message3.set_data("data");
  protocol::ExtensionMessage message4;
  message4.set_type("ext4");
  message4.set_data("data");
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_))
      .WillOnce(DoAll(
          DeliverClientMessage(client_session_.get(), message1),
          DeliverClientMessage(client_session_.get(), message3),
          DeliverClientMessage(client_session_.get(), message4),
          InvokeWithoutArgs(this, &ClientSessionTest::DisconnectClientSession),
          InvokeWithoutArgs(this, &ClientSessionTest::StopClientSession)));

  // Simulate the ClientSession connect and extension negotiation.
  ConnectClientSession();
  base::RunLoop().RunUntilIdle();

  // ext1 was instantiated and sent a message, and did not wrap anything.
  EXPECT_TRUE(extension1.was_instantiated());
  EXPECT_TRUE(extension1.has_handled_message());
  EXPECT_FALSE(extension1.has_wrapped_video_encoder());

  // ext2 was instantiated but not sent a message, and wrapped video encoder.
  EXPECT_TRUE(extension2.was_instantiated());
  EXPECT_FALSE(extension2.has_handled_message());
  EXPECT_TRUE(extension2.has_wrapped_video_encoder());

  // ext3 was sent a message but not instantiated.
  EXPECT_FALSE(extension3.was_instantiated());
}

// Verifies that an extension can "steal" the video capture, in which case no
// VideoFramePump is instantiated.
TEST_F(ClientSessionTest, StealVideoCapturer) {
  FakeExtension extension("ext1", "cap1");
  extensions_.push_back(&extension);

  CreateClientSession();

  SetMatchCapabilitiesExpectation();

  EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_))
      .WillOnce(Return(true));

  ConnectClientSession();

  base::RunLoop().RunUntilIdle();

  extension.set_steal_video_capturer(true);
  client_session_->ResetVideoPipeline();

  base::RunLoop().RunUntilIdle();

  // Verify that video control messages received while there is no video
  // scheduler active won't crash things.
  protocol::VideoControl video_control;
  video_control.set_enable(false);
  video_control.set_lossless_encode(true);
  video_control.set_lossless_color(true);
  client_session_->ControlVideo(video_control);

  // TODO(wez): Find a way to verify that the ClientSession never captures any
  // frames in this case.

  DisconnectClientSession();
  StopClientSession();

  // ext1 was instantiated and wrapped the video capturer.
  EXPECT_TRUE(extension.was_instantiated());
  EXPECT_TRUE(extension.has_wrapped_video_capturer());
}

}  // namespace remoting
