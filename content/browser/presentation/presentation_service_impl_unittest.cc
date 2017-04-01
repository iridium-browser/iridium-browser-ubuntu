// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/presentation/presentation_service_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "content/public/common/presentation_constants.h"
#include "content/public/common/presentation_session.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::ByRef;
using ::testing::Eq;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;

namespace content {

namespace {

// Matches Mojo structs.
MATCHER_P(Equals, expected, "") {
  return expected.Equals(arg);
}

// Matches blink::mojom::PresentationSessionInfo passed by reference.
MATCHER_P(SessionInfoEquals, expected, "") {
  blink::mojom::PresentationSessionInfo& expected_value = expected;
  return expected_value.Equals(arg);
}

// Matches content::PresentationSessionInfo passed by reference.
MATCHER_P(ContentSessionInfoEquals, expected, "") {
  const content::PresentationSessionInfo& expected_value = expected;
  return expected_value.presentation_url == arg.presentation_url &&
         expected_value.presentation_id == arg.presentation_id;
}

const char kPresentationId[] = "presentationId";
const char kPresentationUrl1[] = "http://foo.com/index.html";
const char kPresentationUrl2[] = "http://example.com/index.html";
const char kPresentationUrl3[] = "http://example.net/index.html";

void DoNothing(blink::mojom::PresentationSessionInfoPtr info,
               blink::mojom::PresentationErrorPtr error) {}

}  // namespace

class MockPresentationServiceDelegate
    : public ControllerPresentationServiceDelegate {
 public:
  MOCK_METHOD3(AddObserver,
               void(int render_process_id,
                    int render_frame_id,
                    PresentationServiceDelegate::Observer* observer));
  MOCK_METHOD2(RemoveObserver,
      void(int render_process_id, int render_frame_id));

  bool AddScreenAvailabilityListener(
      int render_process_id,
      int routing_id,
      PresentationScreenAvailabilityListener* listener) override {
    if (!screen_availability_listening_supported_)
      listener->OnScreenAvailabilityNotSupported();

    return AddScreenAvailabilityListener();
  }
  MOCK_METHOD0(AddScreenAvailabilityListener, bool());

  MOCK_METHOD3(RemoveScreenAvailabilityListener,
      void(int render_process_id,
           int routing_id,
           PresentationScreenAvailabilityListener* listener));
  MOCK_METHOD2(Reset,
      void(int render_process_id,
           int routing_id));
  MOCK_METHOD4(SetDefaultPresentationUrls,
               void(int render_process_id,
                    int routing_id,
                    const std::vector<GURL>& default_presentation_urls,
                    const PresentationSessionStartedCallback& callback));
  MOCK_METHOD5(StartSession,
               void(int render_process_id,
                    int render_frame_id,
                    const std::vector<GURL>& presentation_urls,
                    const PresentationSessionStartedCallback& success_cb,
                    const PresentationSessionErrorCallback& error_cb));
  MOCK_METHOD6(JoinSession,
               void(int render_process_id,
                    int render_frame_id,
                    const std::vector<GURL>& presentation_urls,
                    const std::string& presentation_id,
                    const PresentationSessionStartedCallback& success_cb,
                    const PresentationSessionErrorCallback& error_cb));
  MOCK_METHOD3(CloseConnection,
               void(int render_process_id,
                    int render_frame_id,
                    const std::string& presentation_id));
  MOCK_METHOD3(Terminate,
               void(int render_process_id,
                    int render_frame_id,
                    const std::string& presentation_id));
  MOCK_METHOD4(ListenForConnectionMessages,
               void(int render_process_id,
                    int render_frame_id,
                    const content::PresentationSessionInfo& session,
                    const PresentationConnectionMessageCallback& message_cb));
  MOCK_METHOD5(SendMessageRawPtr,
               void(int render_process_id,
                    int render_frame_id,
                    const content::PresentationSessionInfo& session,
                    PresentationConnectionMessage* message_request,
                    const SendMessageCallback& send_message_cb));
  void SendMessage(
      int render_process_id,
      int render_frame_id,
      const content::PresentationSessionInfo& session,
      std::unique_ptr<PresentationConnectionMessage> message_request,
      const SendMessageCallback& send_message_cb) override {
    SendMessageRawPtr(render_process_id, render_frame_id, session,
                      message_request.release(), send_message_cb);
  }
  MOCK_METHOD4(ListenForConnectionStateChange,
               void(int render_process_id,
                    int render_frame_id,
                    const content::PresentationSessionInfo& connection,
                    const content::PresentationConnectionStateChangedCallback&
                        state_changed_cb));

  void ConnectToOffscreenPresentation(
      int render_process_id,
      int render_frame_id,
      const content::PresentationSessionInfo& session,
      PresentationConnectionPtr controller_conn_ptr,
      PresentationConnectionRequest receiver_conn_request) override {
    RegisterOffscreenPresentationConnectionRaw(
        render_process_id, render_frame_id, session, controller_conn_ptr.get());
  }

  MOCK_METHOD4(RegisterOffscreenPresentationConnectionRaw,
               void(int render_process_id,
                    int render_frame_id,
                    const content::PresentationSessionInfo& session,
                    blink::mojom::PresentationConnection* connection));

  void set_screen_availability_listening_supported(bool value) {
    screen_availability_listening_supported_ = value;
  }

 private:
  bool screen_availability_listening_supported_ = true;
};

class MockReceiverPresentationServiceDelegate
    : public ReceiverPresentationServiceDelegate {
 public:
  MOCK_METHOD3(AddObserver,
               void(int render_process_id,
                    int render_frame_id,
                    PresentationServiceDelegate::Observer* observer));
  MOCK_METHOD2(RemoveObserver,
               void(int render_process_id, int render_frame_id));
  MOCK_METHOD2(Reset, void(int render_process_id, int routing_id));
  MOCK_METHOD1(RegisterReceiverConnectionAvailableCallback,
               void(const content::ReceiverConnectionAvailableCallback&));
};

class MockPresentationConnection : public blink::mojom::PresentationConnection {
 public:
  void OnMessage(blink::mojom::ConnectionMessagePtr message,
                 const base::Callback<void(bool)>& send_message_cb) override {
    OnConnectionMessageReceived(*message);
  }
  MOCK_METHOD1(OnConnectionMessageReceived,
               void(const blink::mojom::ConnectionMessage& message));
  MOCK_METHOD1(DidChangeState,
               void(blink::mojom::PresentationConnectionState state));
};

class MockPresentationServiceClient
    : public blink::mojom::PresentationServiceClient {
 public:
  MOCK_METHOD2(OnScreenAvailabilityUpdated,
               void(const GURL& url, bool available));
  void OnConnectionStateChanged(
      blink::mojom::PresentationSessionInfoPtr connection,
      blink::mojom::PresentationConnectionState new_state) override {
    OnConnectionStateChanged(*connection, new_state);
  }
  MOCK_METHOD2(OnConnectionStateChanged,
               void(const blink::mojom::PresentationSessionInfo& connection,
                    blink::mojom::PresentationConnectionState new_state));

  void OnConnectionClosed(
      blink::mojom::PresentationSessionInfoPtr connection,
      blink::mojom::PresentationConnectionCloseReason reason,
      const std::string& message) override {
    OnConnectionClosed(*connection, reason, message);
  }
  MOCK_METHOD3(OnConnectionClosed,
               void(const blink::mojom::PresentationSessionInfo& connection,
                    blink::mojom::PresentationConnectionCloseReason reason,
                    const std::string& message));

  MOCK_METHOD1(OnScreenAvailabilityNotSupported, void(const GURL& url));

  void OnConnectionMessagesReceived(
      blink::mojom::PresentationSessionInfoPtr session_info,
      std::vector<blink::mojom::ConnectionMessagePtr> messages) override {
    messages_received_ = std::move(messages);
    MessagesReceived();
  }
  MOCK_METHOD0(MessagesReceived, void());

  void OnDefaultSessionStarted(
      blink::mojom::PresentationSessionInfoPtr session_info) override {
    OnDefaultSessionStarted(*session_info);
  }
  MOCK_METHOD1(OnDefaultSessionStarted,
               void(const blink::mojom::PresentationSessionInfo& session_info));

  void OnReceiverConnectionAvailable(
      blink::mojom::PresentationSessionInfoPtr session_info,
      blink::mojom::PresentationConnectionPtr controller_conn_ptr,
      blink::mojom::PresentationConnectionRequest receiver_conn_request)
      override {
    OnReceiverConnectionAvailable(*session_info);
  }
  MOCK_METHOD1(OnReceiverConnectionAvailable,
               void(const blink::mojom::PresentationSessionInfo& session_info));

  std::vector<blink::mojom::ConnectionMessagePtr> messages_received_;
};

class PresentationServiceImplTest : public RenderViewHostImplTestHarness {
 public:
  PresentationServiceImplTest()
      : presentation_url1_(GURL(kPresentationUrl1)),
        presentation_url2_(GURL(kPresentationUrl2)),
        presentation_url3_(GURL(kPresentationUrl3)) {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    auto request = mojo::MakeRequest(&service_ptr_);
    EXPECT_CALL(mock_delegate_, AddObserver(_, _, _)).Times(1);
    TestRenderFrameHost* render_frame_host = contents()->GetMainFrame();
    render_frame_host->InitializeRenderFrameIfNeeded();
    service_impl_.reset(new PresentationServiceImpl(
        render_frame_host, contents(), &mock_delegate_, nullptr));
    service_impl_->Bind(std::move(request));

    blink::mojom::PresentationServiceClientPtr client_ptr;
    client_binding_.reset(
        new mojo::Binding<blink::mojom::PresentationServiceClient>(
            &mock_client_, mojo::MakeRequest(&client_ptr)));
    service_impl_->SetClient(std::move(client_ptr));

    presentation_urls_.push_back(presentation_url1_);
    presentation_urls_.push_back(presentation_url2_);
  }

  void TearDown() override {
    service_ptr_.reset();
    if (service_impl_.get()) {
      EXPECT_CALL(mock_delegate_, RemoveObserver(_, _)).Times(1);
      service_impl_.reset();
    }
    RenderViewHostImplTestHarness::TearDown();
  }

  void ListenForScreenAvailabilityAndWait(const GURL& url,
                                          bool delegate_success) {
    base::RunLoop run_loop;
    // This will call to |service_impl_| via mojo. Process the message
    // using RunLoop.
    // The callback shouldn't be invoked since there is no availability
    // result yet.
    EXPECT_CALL(mock_delegate_, AddScreenAvailabilityListener())
        .WillOnce(DoAll(
            InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
            Return(delegate_success)));
    service_ptr_->ListenForScreenAvailability(url);
    run_loop.Run();

    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_delegate_));
  }

  void RunLoopFor(base::TimeDelta duration) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), duration);
    run_loop.Run();
  }

  void SaveQuitClosureAndRunLoop() {
    base::RunLoop run_loop;
    run_loop_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    run_loop_quit_closure_.Reset();
  }

  void SimulateScreenAvailabilityChangeAndWait(const GURL& url,
                                               bool available) {
    auto listener_it = service_impl_->screen_availability_listeners_.find(url);
    ASSERT_TRUE(listener_it->second);

    base::RunLoop run_loop;
    EXPECT_CALL(mock_client_, OnScreenAvailabilityUpdated(url, available))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    listener_it->second->OnScreenAvailabilityChanged(available);
    run_loop.Run();
  }

  void ExpectReset() {
    EXPECT_CALL(mock_delegate_, Reset(_, _)).Times(1);
  }

  void ExpectCleanState() {
    EXPECT_TRUE(service_impl_->default_presentation_urls_.empty());
    EXPECT_EQ(
        service_impl_->screen_availability_listeners_.find(presentation_url1_),
        service_impl_->screen_availability_listeners_.end());
    EXPECT_FALSE(service_impl_->on_connection_messages_callback_.get());
  }

  void ExpectNewSessionCallbackSuccess(
      blink::mojom::PresentationSessionInfoPtr info,
      blink::mojom::PresentationErrorPtr error) {
    EXPECT_FALSE(info.is_null());
    EXPECT_TRUE(error.is_null());
    if (!run_loop_quit_closure_.is_null())
      run_loop_quit_closure_.Run();
  }

  void ExpectNewSessionCallbackError(
      blink::mojom::PresentationSessionInfoPtr info,
      blink::mojom::PresentationErrorPtr error) {
    EXPECT_TRUE(info.is_null());
    EXPECT_FALSE(error.is_null());
    if (!run_loop_quit_closure_.is_null())
      run_loop_quit_closure_.Run();
  }

  void ExpectConnectionMessages(
      const std::vector<blink::mojom::ConnectionMessagePtr>& expected_msgs,
      const std::vector<blink::mojom::ConnectionMessagePtr>& actual_msgs) {
    EXPECT_EQ(expected_msgs.size(), actual_msgs.size());
    for (size_t i = 0; i < actual_msgs.size(); ++i)
      EXPECT_TRUE(expected_msgs[i].Equals(actual_msgs[i]));
  }

  void ExpectSendConnectionMessageCallback(bool success) {
    EXPECT_TRUE(success);
    EXPECT_FALSE(service_impl_->send_message_callback_);
    if (!run_loop_quit_closure_.is_null())
      run_loop_quit_closure_.Run();
  }

  void RunListenForConnectionMessages(const std::string& text_msg,
                                      const std::vector<uint8_t>& binary_data,
                                      bool pass_ownership) {
    std::vector<blink::mojom::ConnectionMessagePtr> expected_msgs(2);
    expected_msgs[0] = blink::mojom::ConnectionMessage::New();
    expected_msgs[0]->type = blink::mojom::PresentationMessageType::TEXT;
    expected_msgs[0]->message = text_msg;
    expected_msgs[1] = blink::mojom::ConnectionMessage::New();
    expected_msgs[1]->type = blink::mojom::PresentationMessageType::BINARY;
    expected_msgs[1]->data = binary_data;

    blink::mojom::PresentationSessionInfoPtr session(
        blink::mojom::PresentationSessionInfo::New());
    session->url = presentation_url1_;
    session->id = kPresentationId;

    PresentationConnectionMessageCallback message_cb;
    {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_delegate_, ListenForConnectionMessages(_, _, _, _))
        .WillOnce(DoAll(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
                        SaveArg<3>(&message_cb)));
    service_ptr_->ListenForConnectionMessages(std::move(session));
    run_loop.Run();
    }

    std::vector<std::unique_ptr<PresentationConnectionMessage>> messages;
    std::unique_ptr<content::PresentationConnectionMessage> message;
    message.reset(new content::PresentationConnectionMessage(
        PresentationMessageType::TEXT));
    message->message = text_msg;
    messages.push_back(std::move(message));
    message.reset(new content::PresentationConnectionMessage(
        PresentationMessageType::BINARY));
    message->data.reset(new std::vector<uint8_t>(binary_data));
    messages.push_back(std::move(message));

    std::vector<blink::mojom::ConnectionMessagePtr> actual_msgs;
    {
      base::RunLoop run_loop;
      EXPECT_CALL(mock_client_, MessagesReceived())
          .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
      message_cb.Run(std::move(messages), pass_ownership);
      run_loop.Run();
    }
    ExpectConnectionMessages(expected_msgs, mock_client_.messages_received_);
  }

  MockPresentationServiceDelegate mock_delegate_;
  MockReceiverPresentationServiceDelegate mock_receiver_delegate_;

  std::unique_ptr<PresentationServiceImpl> service_impl_;
  mojo::InterfacePtr<blink::mojom::PresentationService> service_ptr_;

  MockPresentationServiceClient mock_client_;
  std::unique_ptr<mojo::Binding<blink::mojom::PresentationServiceClient>>
      client_binding_;

  base::Closure run_loop_quit_closure_;

  GURL presentation_url1_;
  GURL presentation_url2_;
  GURL presentation_url3_;
  std::vector<GURL> presentation_urls_;
};

TEST_F(PresentationServiceImplTest, ListenForScreenAvailability) {
  ListenForScreenAvailabilityAndWait(presentation_url1_, true);

  SimulateScreenAvailabilityChangeAndWait(presentation_url1_, true);
  SimulateScreenAvailabilityChangeAndWait(presentation_url1_, false);
  SimulateScreenAvailabilityChangeAndWait(presentation_url1_, true);
}

TEST_F(PresentationServiceImplTest, Reset) {
  ListenForScreenAvailabilityAndWait(presentation_url1_, true);

  ExpectReset();
  service_impl_->Reset();
  ExpectCleanState();
}

TEST_F(PresentationServiceImplTest, DidNavigateThisFrame) {
  ListenForScreenAvailabilityAndWait(presentation_url1_, true);

  ExpectReset();
  service_impl_->DidNavigateAnyFrame(
      contents()->GetMainFrame(),
      content::LoadCommittedDetails(),
      content::FrameNavigateParams());
  ExpectCleanState();
}

TEST_F(PresentationServiceImplTest, DidNavigateOtherFrame) {
  ListenForScreenAvailabilityAndWait(presentation_url1_, true);

  // TODO(imcheng): How to get a different RenderFrameHost?
  service_impl_->DidNavigateAnyFrame(
      nullptr,
      content::LoadCommittedDetails(),
      content::FrameNavigateParams());

  // Availability is reported and callback is invoked since it was not
  // removed.
  SimulateScreenAvailabilityChangeAndWait(presentation_url1_, true);
}

TEST_F(PresentationServiceImplTest, ThisRenderFrameDeleted) {
  ListenForScreenAvailabilityAndWait(presentation_url1_, true);

  ExpectReset();

  // Since the frame matched the service, |service_impl_| will be deleted.
  PresentationServiceImpl* service = service_impl_.release();
  EXPECT_CALL(mock_delegate_, RemoveObserver(_, _)).Times(1);
  service->RenderFrameDeleted(contents()->GetMainFrame());
}

TEST_F(PresentationServiceImplTest, OtherRenderFrameDeleted) {
  ListenForScreenAvailabilityAndWait(presentation_url1_, true);

  // TODO(imcheng): How to get a different RenderFrameHost?
  service_impl_->RenderFrameDeleted(nullptr);

  // Availability is reported and callback should be invoked since listener
  // has not been deleted.
  SimulateScreenAvailabilityChangeAndWait(presentation_url1_, true);
}

TEST_F(PresentationServiceImplTest, DelegateFails) {
  ListenForScreenAvailabilityAndWait(presentation_url1_, false);
  ASSERT_EQ(
      service_impl_->screen_availability_listeners_.find(presentation_url1_),
      service_impl_->screen_availability_listeners_.end());
}

TEST_F(PresentationServiceImplTest, SetDefaultPresentationUrls) {
  EXPECT_CALL(mock_delegate_,
              SetDefaultPresentationUrls(_, _, presentation_urls_, _))
      .Times(1);

  service_impl_->SetDefaultPresentationUrls(presentation_urls_);

  // Sets different DPUs.
  std::vector<GURL> more_urls = presentation_urls_;
  more_urls.push_back(presentation_url3_);

  content::PresentationSessionStartedCallback callback;
  EXPECT_CALL(mock_delegate_, SetDefaultPresentationUrls(_, _, more_urls, _))
      .WillOnce(SaveArg<3>(&callback));
  service_impl_->SetDefaultPresentationUrls(more_urls);

  blink::mojom::PresentationSessionInfo session_info;
  session_info.url = presentation_url2_;
  session_info.id = kPresentationId;

  base::RunLoop run_loop;
  EXPECT_CALL(mock_client_,
              OnDefaultSessionStarted(SessionInfoEquals(ByRef(session_info))))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(mock_delegate_, ListenForConnectionStateChange(_, _, _, _));
  callback.Run(
      content::PresentationSessionInfo(presentation_url2_, kPresentationId));
  run_loop.Run();
}

TEST_F(PresentationServiceImplTest,
       ListenForConnectionStateChange) {
  content::PresentationSessionInfo connection(presentation_url1_,
                                              kPresentationId);
  content::PresentationConnectionStateChangedCallback state_changed_cb;
  // Trigger state change. It should be propagated back up to |mock_client_|.
  blink::mojom::PresentationSessionInfo presentation_connection;
  presentation_connection.url = presentation_url1_;
  presentation_connection.id = kPresentationId;

  EXPECT_CALL(mock_delegate_, ListenForConnectionStateChange(_, _, _, _))
      .WillOnce(SaveArg<3>(&state_changed_cb));
  service_impl_->ListenForConnectionStateChange(connection);

  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_client_,
                OnConnectionStateChanged(
                    SessionInfoEquals(ByRef(presentation_connection)),
                    blink::mojom::PresentationConnectionState::TERMINATED))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    state_changed_cb.Run(PresentationConnectionStateChangeInfo(
        PRESENTATION_CONNECTION_STATE_TERMINATED));
    run_loop.Run();
  }
}

TEST_F(PresentationServiceImplTest, ListenForConnectionClose) {
  content::PresentationSessionInfo connection(presentation_url1_,
                                              kPresentationId);
  content::PresentationConnectionStateChangedCallback state_changed_cb;
  EXPECT_CALL(mock_delegate_, ListenForConnectionStateChange(_, _, _, _))
      .WillOnce(SaveArg<3>(&state_changed_cb));
  service_impl_->ListenForConnectionStateChange(connection);

  // Trigger connection close. It should be propagated back up to
  // |mock_client_|.
  blink::mojom::PresentationSessionInfo presentation_connection;
  presentation_connection.url = presentation_url1_;
  presentation_connection.id = kPresentationId;
  {
    base::RunLoop run_loop;
    PresentationConnectionStateChangeInfo closed_info(
        PRESENTATION_CONNECTION_STATE_CLOSED);
    closed_info.close_reason = PRESENTATION_CONNECTION_CLOSE_REASON_WENT_AWAY;
    closed_info.message = "Foo";

    EXPECT_CALL(
        mock_client_,
        OnConnectionClosed(
            SessionInfoEquals(ByRef(presentation_connection)),
            blink::mojom::PresentationConnectionCloseReason::WENT_AWAY, "Foo"))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    state_changed_cb.Run(closed_info);
    run_loop.Run();
  }
}

TEST_F(PresentationServiceImplTest, SetSameDefaultPresentationUrls) {
  EXPECT_CALL(mock_delegate_,
              SetDefaultPresentationUrls(_, _, presentation_urls_, _))
      .Times(1);
  service_impl_->SetDefaultPresentationUrls(presentation_urls_);
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_delegate_));

  // Same URLs as before; no-ops.
  service_impl_->SetDefaultPresentationUrls(presentation_urls_);
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_delegate_));
}

TEST_F(PresentationServiceImplTest, StartSessionSuccess) {
  service_ptr_->StartSession(
      presentation_urls_,
      base::Bind(&PresentationServiceImplTest::ExpectNewSessionCallbackSuccess,
                 base::Unretained(this)));
  base::RunLoop run_loop;
  base::Callback<void(const PresentationSessionInfo&)> success_cb;
  EXPECT_CALL(mock_delegate_, StartSession(_, _, presentation_urls_, _, _))
      .WillOnce(DoAll(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
                      SaveArg<3>(&success_cb)));
  run_loop.Run();

  EXPECT_CALL(mock_delegate_, ListenForConnectionStateChange(_, _, _, _))
      .Times(1);
  success_cb.Run(PresentationSessionInfo(presentation_url1_, kPresentationId));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, StartSessionError) {
  service_ptr_->StartSession(
      presentation_urls_,
      base::Bind(&PresentationServiceImplTest::ExpectNewSessionCallbackError,
                 base::Unretained(this)));
  base::RunLoop run_loop;
  base::Callback<void(const PresentationError&)> error_cb;
  EXPECT_CALL(mock_delegate_, StartSession(_, _, presentation_urls_, _, _))
      .WillOnce(DoAll(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
                      SaveArg<4>(&error_cb)));
  run_loop.Run();
  error_cb.Run(PresentationError(PRESENTATION_ERROR_UNKNOWN, "Error message"));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, JoinSessionSuccess) {
  service_ptr_->JoinSession(
      presentation_urls_, base::Optional<std::string>(kPresentationId),
      base::Bind(&PresentationServiceImplTest::ExpectNewSessionCallbackSuccess,
                 base::Unretained(this)));
  base::RunLoop run_loop;
  base::Callback<void(const PresentationSessionInfo&)> success_cb;
  EXPECT_CALL(mock_delegate_,
              JoinSession(_, _, presentation_urls_, kPresentationId, _, _))
      .WillOnce(DoAll(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
                      SaveArg<4>(&success_cb)));
  run_loop.Run();

  EXPECT_CALL(mock_delegate_, ListenForConnectionStateChange(_, _, _, _))
      .Times(1);
  success_cb.Run(PresentationSessionInfo(presentation_url1_, kPresentationId));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, JoinSessionError) {
  service_ptr_->JoinSession(
      presentation_urls_, base::Optional<std::string>(kPresentationId),
      base::Bind(&PresentationServiceImplTest::ExpectNewSessionCallbackError,
                 base::Unretained(this)));
  base::RunLoop run_loop;
  base::Callback<void(const PresentationError&)> error_cb;
  EXPECT_CALL(mock_delegate_,
              JoinSession(_, _, presentation_urls_, kPresentationId, _, _))
      .WillOnce(DoAll(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
                      SaveArg<5>(&error_cb)));
  run_loop.Run();
  error_cb.Run(PresentationError(PRESENTATION_ERROR_UNKNOWN, "Error message"));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, CloseConnection) {
  service_ptr_->CloseConnection(presentation_url1_, kPresentationId);

  base::RunLoop run_loop;
  EXPECT_CALL(mock_delegate_, CloseConnection(_, _, Eq(kPresentationId)))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  run_loop.Run();
}

TEST_F(PresentationServiceImplTest, Terminate) {
  service_ptr_->Terminate(presentation_url1_, kPresentationId);
  base::RunLoop run_loop;
  EXPECT_CALL(mock_delegate_, Terminate(_, _, Eq(kPresentationId)))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  run_loop.Run();
}

TEST_F(PresentationServiceImplTest, ListenForConnectionMessagesPassed) {
  std::string text_msg("123");
  std::vector<uint8_t> binary_data(3, '\1');
  RunListenForConnectionMessages(text_msg, binary_data, true);
}

TEST_F(PresentationServiceImplTest, ListenForConnectionMessagesCopied) {
  std::string text_msg("123");
  std::vector<uint8_t> binary_data(3, '\1');
  RunListenForConnectionMessages(text_msg, binary_data, false);
}

TEST_F(PresentationServiceImplTest, ListenForConnectionMessagesWithEmptyMsg) {
  std::string text_msg("");
  std::vector<uint8_t> binary_data;
  RunListenForConnectionMessages(text_msg, binary_data, false);
}

TEST_F(PresentationServiceImplTest, SetPresentationConnection) {
  blink::mojom::PresentationSessionInfoPtr session(
      blink::mojom::PresentationSessionInfo::New());
  session->url = presentation_url1_;
  session->id = kPresentationId;

  blink::mojom::PresentationConnectionPtr connection;
  MockPresentationConnection mock_presentation_connection;
  mojo::Binding<blink::mojom::PresentationConnection> connection_binding(
      &mock_presentation_connection, mojo::MakeRequest(&connection));
  blink::mojom::PresentationConnectionPtr receiver_connection;
  auto request = mojo::MakeRequest(&receiver_connection);

  content::PresentationSessionInfo expected(presentation_url1_,
                                            kPresentationId);
  EXPECT_CALL(mock_delegate_,
              RegisterOffscreenPresentationConnectionRaw(
                  _, _, ContentSessionInfoEquals(ByRef(expected)), _));

  service_impl_->SetPresentationConnection(
      std::move(session), std::move(connection), std::move(request));
}

TEST_F(PresentationServiceImplTest, ReceiverPresentationServiceDelegate) {
  MockReceiverPresentationServiceDelegate mock_receiver_delegate;

  PresentationServiceImpl service_impl(contents()->GetMainFrame(), contents(),
                                       nullptr, &mock_receiver_delegate);

  ReceiverConnectionAvailableCallback callback;
  EXPECT_CALL(mock_receiver_delegate,
              RegisterReceiverConnectionAvailableCallback(_))
      .WillOnce(SaveArg<0>(&callback));

  blink::mojom::PresentationServiceClientPtr client_ptr;
  client_binding_.reset(
      new mojo::Binding<blink::mojom::PresentationServiceClient>(
          &mock_client_, mojo::MakeRequest(&client_ptr)));
  service_impl.controller_delegate_ = nullptr;
  service_impl.SetClient(std::move(client_ptr));
  EXPECT_FALSE(callback.is_null());

  // NO-OP for ControllerPresentationServiceDelegate API functions
  EXPECT_CALL(mock_delegate_, ListenForConnectionMessages(_, _, _, _)).Times(0);

  blink::mojom::PresentationSessionInfoPtr session(
      blink::mojom::PresentationSessionInfo::New());
  session->url = GURL(kPresentationUrl1);
  session->id = kPresentationId;

  service_impl.ListenForConnectionMessages(std::move(session));
}

TEST_F(PresentationServiceImplTest, StartSessionInProgress) {
  EXPECT_CALL(mock_delegate_, StartSession(_, _, presentation_urls_, _, _))
      .Times(1);
  service_ptr_->StartSession(presentation_urls_, base::Bind(&DoNothing));

  // This request should fail immediately, since there is already a StartSession
  // in progress.
  service_ptr_->StartSession(
      presentation_urls_,
      base::Bind(&PresentationServiceImplTest::ExpectNewSessionCallbackError,
                 base::Unretained(this)));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, SendStringMessage) {
  std::string message("Test presentation session message");

  blink::mojom::PresentationSessionInfoPtr session(
      blink::mojom::PresentationSessionInfo::New());
  session->url = presentation_url1_;
  session->id = kPresentationId;
  blink::mojom::ConnectionMessagePtr message_request(
      blink::mojom::ConnectionMessage::New());
  message_request->type = blink::mojom::PresentationMessageType::TEXT;
  message_request->message = message;
  service_ptr_->SendConnectionMessage(
      std::move(session), std::move(message_request),
      base::Bind(
          &PresentationServiceImplTest::ExpectSendConnectionMessageCallback,
          base::Unretained(this)));

  base::RunLoop run_loop;
  base::Callback<void(bool)> send_message_cb;
  PresentationConnectionMessage* test_message = nullptr;
  EXPECT_CALL(mock_delegate_, SendMessageRawPtr(_, _, _, _, _))
      .WillOnce(DoAll(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
                      SaveArg<3>(&test_message), SaveArg<4>(&send_message_cb)));
  run_loop.Run();

  // Make sure |test_message| gets deleted.
  std::unique_ptr<PresentationConnectionMessage> scoped_test_message(
      test_message);
  EXPECT_TRUE(test_message);
  EXPECT_FALSE(test_message->is_binary());
  EXPECT_LE(test_message->message.size(),
            kMaxPresentationConnectionMessageSize);
  EXPECT_EQ(message, test_message->message);
  ASSERT_FALSE(test_message->data);
  send_message_cb.Run(true);
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, SendArrayBuffer) {
  // Test Array buffer data.
  const uint8_t buffer[] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48};
  std::vector<uint8_t> data;
  data.assign(buffer, buffer + sizeof(buffer));

  blink::mojom::PresentationSessionInfoPtr session(
      blink::mojom::PresentationSessionInfo::New());
  session->url = presentation_url1_;
  session->id = kPresentationId;
  blink::mojom::ConnectionMessagePtr message_request(
      blink::mojom::ConnectionMessage::New());
  message_request->type = blink::mojom::PresentationMessageType::BINARY;
  message_request->data = data;
  service_ptr_->SendConnectionMessage(
      std::move(session), std::move(message_request),
      base::Bind(
          &PresentationServiceImplTest::ExpectSendConnectionMessageCallback,
          base::Unretained(this)));

  base::RunLoop run_loop;
  base::Callback<void(bool)> send_message_cb;
  PresentationConnectionMessage* test_message = nullptr;
  EXPECT_CALL(mock_delegate_, SendMessageRawPtr(_, _, _, _, _))
      .WillOnce(DoAll(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
                      SaveArg<3>(&test_message), SaveArg<4>(&send_message_cb)));
  run_loop.Run();

  // Make sure |test_message| gets deleted.
  std::unique_ptr<PresentationConnectionMessage> scoped_test_message(
      test_message);
  EXPECT_TRUE(test_message);
  EXPECT_TRUE(test_message->is_binary());
  EXPECT_EQ(PresentationMessageType::BINARY, test_message->type);
  EXPECT_TRUE(test_message->message.empty());
  ASSERT_TRUE(test_message->data);
  EXPECT_EQ(data.size(), test_message->data->size());
  EXPECT_LE(test_message->data->size(), kMaxPresentationConnectionMessageSize);
  EXPECT_EQ(0, memcmp(buffer, &(*test_message->data)[0], sizeof(buffer)));
  send_message_cb.Run(true);
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, SendArrayBufferWithExceedingLimit) {
  // Create buffer with size exceeding the limit.
  // Use same size as in content::kMaxPresentationConnectionMessageSize.
  const size_t kMaxBufferSizeInBytes = 64 * 1024;  // 64 KB.
  uint8_t buffer[kMaxBufferSizeInBytes + 1];
  memset(buffer, 0, kMaxBufferSizeInBytes+1);
  std::vector<uint8_t> data;
  data.assign(buffer, buffer + sizeof(buffer));

  blink::mojom::PresentationSessionInfoPtr session(
      blink::mojom::PresentationSessionInfo::New());
  session->url = presentation_url1_;
  session->id = kPresentationId;
  blink::mojom::ConnectionMessagePtr message_request(
      blink::mojom::ConnectionMessage::New());
  message_request->type = blink::mojom::PresentationMessageType::BINARY;
  message_request->data = data;
  service_ptr_->SendConnectionMessage(
      std::move(session), std::move(message_request),
      base::Bind(
          &PresentationServiceImplTest::ExpectSendConnectionMessageCallback,
          base::Unretained(this)));

  base::RunLoop run_loop;
  base::Callback<void(bool)> send_message_cb;
  PresentationConnectionMessage* test_message = nullptr;
  EXPECT_CALL(mock_delegate_, SendMessageRawPtr(_, _, _, _, _))
      .WillOnce(DoAll(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
                      SaveArg<3>(&test_message), SaveArg<4>(&send_message_cb)));
  run_loop.Run();

  EXPECT_FALSE(test_message);
  send_message_cb.Run(true);
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, SendBlobData) {
  const uint8_t buffer[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
  std::vector<uint8_t> data;
  data.assign(buffer, buffer + sizeof(buffer));

  blink::mojom::PresentationSessionInfoPtr session(
      blink::mojom::PresentationSessionInfo::New());
  session->url = presentation_url1_;
  session->id = kPresentationId;
  blink::mojom::ConnectionMessagePtr message_request(
      blink::mojom::ConnectionMessage::New());
  message_request->type = blink::mojom::PresentationMessageType::BINARY;
  message_request->data = data;
  service_ptr_->SendConnectionMessage(
      std::move(session), std::move(message_request),
      base::Bind(
          &PresentationServiceImplTest::ExpectSendConnectionMessageCallback,
          base::Unretained(this)));

  base::RunLoop run_loop;
  base::Callback<void(bool)> send_message_cb;
  PresentationConnectionMessage* test_message = nullptr;
  EXPECT_CALL(mock_delegate_, SendMessageRawPtr(_, _, _, _, _))
      .WillOnce(DoAll(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
                      SaveArg<3>(&test_message), SaveArg<4>(&send_message_cb)));
  run_loop.Run();

  // Make sure |test_message| gets deleted.
  std::unique_ptr<PresentationConnectionMessage> scoped_test_message(
      test_message);
  EXPECT_TRUE(test_message);
  EXPECT_TRUE(test_message->is_binary());
  EXPECT_EQ(PresentationMessageType::BINARY, test_message->type);
  EXPECT_TRUE(test_message->message.empty());
  ASSERT_TRUE(test_message->data);
  EXPECT_EQ(data.size(), test_message->data->size());
  EXPECT_LE(test_message->data->size(), kMaxPresentationConnectionMessageSize);
  EXPECT_EQ(0, memcmp(buffer, &(*test_message->data)[0], sizeof(buffer)));
  send_message_cb.Run(true);
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, MaxPendingJoinSessionRequests) {
  const char* presentation_url = "http://fooUrl%d";
  const char* presentation_id = "presentationId%d";
  int num_requests = PresentationServiceImpl::kMaxNumQueuedSessionRequests;
  int i = 0;
  EXPECT_CALL(mock_delegate_, JoinSession(_, _, _, _, _, _))
      .Times(num_requests);
  for (; i < num_requests; ++i) {
    std::vector<GURL> urls = {GURL(base::StringPrintf(presentation_url, i))};
    service_ptr_->JoinSession(urls, base::StringPrintf(presentation_id, i),
                              base::Bind(&DoNothing));
  }

  std::vector<GURL> urls = {GURL(base::StringPrintf(presentation_url, i))};
  // Exceeded maximum queue size, should invoke mojo callback with error.
  service_ptr_->JoinSession(
      urls, base::StringPrintf(presentation_id, i),
      base::Bind(&PresentationServiceImplTest::ExpectNewSessionCallbackError,
                 base::Unretained(this)));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, ScreenAvailabilityNotSupported) {
  mock_delegate_.set_screen_availability_listening_supported(false);
  base::RunLoop run_loop;
  EXPECT_CALL(mock_client_,
              OnScreenAvailabilityNotSupported(presentation_url1_))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  ListenForScreenAvailabilityAndWait(presentation_url1_, false);
  run_loop.Run();
}

}  // namespace content
