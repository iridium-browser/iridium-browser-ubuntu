// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/presentation/presentation_service_impl.h"

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/stl_util.h"
#include "content/browser/presentation/presentation_type_converters.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/presentation_connection_message.h"
#include "content/public/common/presentation_constants.h"

namespace content {

namespace {

const int kInvalidRequestSessionId = -1;

int GetNextRequestSessionId() {
  static int next_request_session_id = 0;
  return ++next_request_session_id;
}

// Converts a PresentationConnectionMessage |input| to a ConnectionMessage.
// |input|: The message to convert.
// |pass_ownership|: If true, function may reuse strings or buffers from
//     |input| without copying. |input| can be freely modified.
blink::mojom::ConnectionMessagePtr ToMojoConnectionMessage(
    content::PresentationConnectionMessage* input,
    bool pass_ownership) {
  DCHECK(input);
  blink::mojom::ConnectionMessagePtr output(
      blink::mojom::ConnectionMessage::New());
  if (input->is_binary()) {
    // binary data
    DCHECK(input->data);
    output->type = blink::mojom::PresentationMessageType::BINARY;
    if (pass_ownership) {
      output->data = std::move(*(input->data));
    } else {
      output->data = *(input->data);
    }
  } else {
    // string message
    output->type = blink::mojom::PresentationMessageType::TEXT;
    if (pass_ownership) {
      output->message = std::move(input->message);
    } else {
      output->message = input->message;
    }
  }
  return output;
}

std::unique_ptr<PresentationConnectionMessage> GetPresentationConnectionMessage(
    blink::mojom::ConnectionMessagePtr input) {
  std::unique_ptr<content::PresentationConnectionMessage> output;
  if (input.is_null())
    return output;

  switch (input->type) {
    case blink::mojom::PresentationMessageType::TEXT: {
      // Return nullptr PresentationConnectionMessage if invalid (unset
      // |message|,
      // set |data|, or size too large).
      if (input->data || !input->message ||
          input->message->size() >
              content::kMaxPresentationConnectionMessageSize)
        return output;

      output.reset(
          new PresentationConnectionMessage(PresentationMessageType::TEXT));
      output->message = std::move(input->message.value());
      return output;
    }
    case blink::mojom::PresentationMessageType::BINARY: {
      // Return nullptr PresentationConnectionMessage if invalid (unset |data|,
      // set
      // |message|, or size too large).
      if (!input->data || input->message ||
          input->data->size() > content::kMaxPresentationConnectionMessageSize)
        return output;

      output.reset(
          new PresentationConnectionMessage(PresentationMessageType::BINARY));
      output->data.reset(
          new std::vector<uint8_t>(std::move(input->data.value())));
      return output;
    }
  }

  NOTREACHED() << "Invalid presentation message type " << input->type;
  return output;
}

void InvokeNewSessionCallbackWithError(
    const PresentationServiceImpl::NewSessionCallback& callback) {
  callback.Run(blink::mojom::PresentationSessionInfoPtr(),
               blink::mojom::PresentationError::From(PresentationError(
                   PRESENTATION_ERROR_UNKNOWN, "Internal error")));
}

}  // namespace

PresentationServiceImpl::PresentationServiceImpl(
    RenderFrameHost* render_frame_host,
    WebContents* web_contents,
    ControllerPresentationServiceDelegate* controller_delegate,
    ReceiverPresentationServiceDelegate* receiver_delegate)
    : WebContentsObserver(web_contents),
      controller_delegate_(controller_delegate),
      receiver_delegate_(receiver_delegate),
      start_session_request_id_(kInvalidRequestSessionId),
      weak_factory_(this) {
  DCHECK(render_frame_host);
  DCHECK(web_contents);
  CHECK(render_frame_host->IsRenderFrameLive());

  render_process_id_ = render_frame_host->GetProcess()->GetID();
  render_frame_id_ = render_frame_host->GetRoutingID();
  DVLOG(2) << "PresentationServiceImpl: "
           << render_process_id_ << ", " << render_frame_id_;

  if (auto* delegate = GetPresentationServiceDelegate())
    delegate->AddObserver(render_process_id_, render_frame_id_, this);
}

PresentationServiceImpl::~PresentationServiceImpl() {
  DVLOG(2) << __FUNCTION__ << ": " << render_process_id_ << ", "
           << render_frame_id_;

  if (auto* delegate = GetPresentationServiceDelegate())
    delegate->RemoveObserver(render_process_id_, render_frame_id_);
}

// static
void PresentationServiceImpl::CreateMojoService(
    RenderFrameHost* render_frame_host,
    mojo::InterfaceRequest<blink::mojom::PresentationService> request) {
  DVLOG(2) << "CreateMojoService";
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  auto* browser = GetContentClient()->browser();
  auto* receiver_delegate =
      browser->GetReceiverPresentationServiceDelegate(web_contents);

  // In current implementation, web_contents can be controller or receiver
  // but not both.
  auto* controller_delegate =
      receiver_delegate
          ? nullptr
          : browser->GetControllerPresentationServiceDelegate(web_contents);

  // This object will be deleted when the RenderFrameHost is about to be
  // deleted (RenderFrameDeleted).
  PresentationServiceImpl* impl = new PresentationServiceImpl(
      render_frame_host, web_contents, controller_delegate, receiver_delegate);
  impl->Bind(std::move(request));
}

void PresentationServiceImpl::Bind(
    mojo::InterfaceRequest<blink::mojom::PresentationService> request) {
  binding_.reset(new mojo::Binding<blink::mojom::PresentationService>(
      this, std::move(request)));
}

void PresentationServiceImpl::SetClient(
    blink::mojom::PresentationServiceClientPtr client) {
  DCHECK(!client_.get());
  // TODO(imcheng): Set ErrorHandler to listen for errors.
  client_ = std::move(client);

  if (receiver_delegate_) {
    receiver_delegate_->RegisterReceiverConnectionAvailableCallback(
        base::Bind(&PresentationServiceImpl::OnReceiverConnectionAvailable,
                   weak_factory_.GetWeakPtr()));
  }
}

void PresentationServiceImpl::ListenForScreenAvailability(const GURL& url) {
  DVLOG(2) << "ListenForScreenAvailability " << url.spec();
  if (!controller_delegate_) {
    client_->OnScreenAvailabilityUpdated(url, false);
    return;
  }

  if (screen_availability_listeners_.count(url))
    return;

  std::unique_ptr<ScreenAvailabilityListenerImpl> listener(
      new ScreenAvailabilityListenerImpl(url, this));
  if (controller_delegate_->AddScreenAvailabilityListener(
          render_process_id_, render_frame_id_, listener.get())) {
    screen_availability_listeners_[url] = std::move(listener);
  } else {
    DVLOG(1) << "AddScreenAvailabilityListener failed. Ignoring request.";
  }
}

void PresentationServiceImpl::StopListeningForScreenAvailability(
    const GURL& url) {
  DVLOG(2) << "StopListeningForScreenAvailability " << url.spec();
  if (!controller_delegate_)
    return;

  auto listener_it = screen_availability_listeners_.find(url);
  if (listener_it == screen_availability_listeners_.end())
    return;

  controller_delegate_->RemoveScreenAvailabilityListener(
      render_process_id_, render_frame_id_, listener_it->second.get());
  screen_availability_listeners_.erase(listener_it);
}

void PresentationServiceImpl::StartSession(
    const std::vector<GURL>& presentation_urls,
    const NewSessionCallback& callback) {
  DVLOG(2) << "StartSession";
  if (!controller_delegate_) {
    callback.Run(
        blink::mojom::PresentationSessionInfoPtr(),
        blink::mojom::PresentationError::From(PresentationError(
            PRESENTATION_ERROR_NO_AVAILABLE_SCREENS, "No screens found.")));
    return;
  }

  // There is a StartSession request in progress. To avoid queueing up
  // requests, the incoming request is rejected.
  if (start_session_request_id_ != kInvalidRequestSessionId) {
    InvokeNewSessionCallbackWithError(callback);
    return;
  }

  start_session_request_id_ = GetNextRequestSessionId();
  pending_start_session_cb_.reset(new NewSessionCallbackWrapper(callback));
  controller_delegate_->StartSession(
      render_process_id_, render_frame_id_, presentation_urls,
      base::Bind(&PresentationServiceImpl::OnStartSessionSucceeded,
                 weak_factory_.GetWeakPtr(), start_session_request_id_),
      base::Bind(&PresentationServiceImpl::OnStartSessionError,
                 weak_factory_.GetWeakPtr(), start_session_request_id_));
}

void PresentationServiceImpl::JoinSession(
    const std::vector<GURL>& presentation_urls,
    const base::Optional<std::string>& presentation_id,
    const NewSessionCallback& callback) {
  DVLOG(2) << "JoinSession";
  if (!controller_delegate_) {
    callback.Run(blink::mojom::PresentationSessionInfoPtr(),
                 blink::mojom::PresentationError::From(PresentationError(
                     PRESENTATION_ERROR_NO_PRESENTATION_FOUND,
                     "Error joining route: No matching route")));
    return;
  }

  int request_session_id = RegisterJoinSessionCallback(callback);
  if (request_session_id == kInvalidRequestSessionId) {
    InvokeNewSessionCallbackWithError(callback);
    return;
  }
  controller_delegate_->JoinSession(
      render_process_id_, render_frame_id_, presentation_urls,
      presentation_id.value_or(std::string()),
      base::Bind(&PresentationServiceImpl::OnJoinSessionSucceeded,
                 weak_factory_.GetWeakPtr(), request_session_id),
      base::Bind(&PresentationServiceImpl::OnJoinSessionError,
                 weak_factory_.GetWeakPtr(), request_session_id));
}

int PresentationServiceImpl::RegisterJoinSessionCallback(
    const NewSessionCallback& callback) {
  if (pending_join_session_cbs_.size() >= kMaxNumQueuedSessionRequests)
    return kInvalidRequestSessionId;

  int request_id = GetNextRequestSessionId();
  pending_join_session_cbs_[request_id].reset(
      new NewSessionCallbackWrapper(callback));
  return request_id;
}

void PresentationServiceImpl::ListenForConnectionStateChange(
    const PresentationSessionInfo& connection) {
  // NOTE: Blink will automatically transition the connection's state to
  // 'connected'.
  if (controller_delegate_) {
    controller_delegate_->ListenForConnectionStateChange(
        render_process_id_, render_frame_id_, connection,
        base::Bind(&PresentationServiceImpl::OnConnectionStateChanged,
                   weak_factory_.GetWeakPtr(), connection));
  }
}

void PresentationServiceImpl::OnStartSessionSucceeded(
    int request_session_id,
    const PresentationSessionInfo& session_info) {
  if (request_session_id != start_session_request_id_)
    return;

  CHECK(pending_start_session_cb_.get());
  pending_start_session_cb_->Run(
      blink::mojom::PresentationSessionInfo::From(session_info),
      blink::mojom::PresentationErrorPtr());
  ListenForConnectionStateChange(session_info);
  pending_start_session_cb_.reset();
  start_session_request_id_ = kInvalidRequestSessionId;
}

void PresentationServiceImpl::OnStartSessionError(
    int request_session_id,
    const PresentationError& error) {
  if (request_session_id != start_session_request_id_)
    return;

  CHECK(pending_start_session_cb_.get());
  pending_start_session_cb_->Run(blink::mojom::PresentationSessionInfoPtr(),
                                 blink::mojom::PresentationError::From(error));
  pending_start_session_cb_.reset();
  start_session_request_id_ = kInvalidRequestSessionId;
}

void PresentationServiceImpl::OnJoinSessionSucceeded(
    int request_session_id,
    const PresentationSessionInfo& session_info) {
  if (RunAndEraseJoinSessionMojoCallback(
          request_session_id,
          blink::mojom::PresentationSessionInfo::From(session_info),
          blink::mojom::PresentationErrorPtr())) {
    ListenForConnectionStateChange(session_info);
  }
}

void PresentationServiceImpl::OnJoinSessionError(
    int request_session_id,
    const PresentationError& error) {
  RunAndEraseJoinSessionMojoCallback(
      request_session_id, blink::mojom::PresentationSessionInfoPtr(),
      blink::mojom::PresentationError::From(error));
}

bool PresentationServiceImpl::RunAndEraseJoinSessionMojoCallback(
    int request_session_id,
    blink::mojom::PresentationSessionInfoPtr session,
    blink::mojom::PresentationErrorPtr error) {
  auto it = pending_join_session_cbs_.find(request_session_id);
  if (it == pending_join_session_cbs_.end())
    return false;

  DCHECK(it->second.get());
  it->second->Run(std::move(session), std::move(error));
  pending_join_session_cbs_.erase(it);
  return true;
}

void PresentationServiceImpl::SetDefaultPresentationUrls(
    const std::vector<GURL>& presentation_urls) {
  DVLOG(2) << "SetDefaultPresentationUrls";
  if (!controller_delegate_)
    return;

  if (default_presentation_urls_ == presentation_urls)
    return;

  default_presentation_urls_ = presentation_urls;
  controller_delegate_->SetDefaultPresentationUrls(
      render_process_id_, render_frame_id_, presentation_urls,
      base::Bind(&PresentationServiceImpl::OnDefaultPresentationStarted,
                 weak_factory_.GetWeakPtr()));
}

void PresentationServiceImpl::SendConnectionMessage(
    blink::mojom::PresentationSessionInfoPtr session,
    blink::mojom::ConnectionMessagePtr connection_message,
    const SendConnectionMessageCallback& callback) {
  DVLOG(2) << "SendConnectionMessage"
           << " [id]: " << session->id;
  DCHECK(!connection_message.is_null());
  // send_message_callback_ should be null by now, otherwise resetting of
  // send_message_callback_ with new callback will drop the old callback.
  if (!controller_delegate_ || send_message_callback_) {
    callback.Run(false);
    return;
  }

  send_message_callback_.reset(new SendConnectionMessageCallback(callback));
  controller_delegate_->SendMessage(
      render_process_id_, render_frame_id_,
      session.To<PresentationSessionInfo>(),
      GetPresentationConnectionMessage(std::move(connection_message)),
      base::Bind(&PresentationServiceImpl::OnSendMessageCallback,
                 weak_factory_.GetWeakPtr()));
}

void PresentationServiceImpl::OnSendMessageCallback(bool sent) {
  // It is possible that Reset() is invoked before receiving this callback.
  // So, always check send_message_callback_ for non-null.
  if (send_message_callback_) {
    send_message_callback_->Run(sent);
    send_message_callback_.reset();
  }
}

void PresentationServiceImpl::CloseConnection(
    const GURL& presentation_url,
    const std::string& presentation_id) {
  DVLOG(2) << "CloseConnection " << presentation_id;
  if (controller_delegate_)
    controller_delegate_->CloseConnection(render_process_id_, render_frame_id_,
                                          presentation_id);
}

void PresentationServiceImpl::Terminate(const GURL& presentation_url,
                                        const std::string& presentation_id) {
  DVLOG(2) << "Terminate " << presentation_id;
  if (controller_delegate_)
    controller_delegate_->Terminate(render_process_id_, render_frame_id_,
                                    presentation_id);
}

void PresentationServiceImpl::OnConnectionStateChanged(
    const PresentationSessionInfo& connection,
    const PresentationConnectionStateChangeInfo& info) {
  DVLOG(2) << "PresentationServiceImpl::OnConnectionStateChanged "
           << "[presentation_id]: " << connection.presentation_id
           << " [state]: " << info.state;
  DCHECK(client_.get());
  if (info.state == PRESENTATION_CONNECTION_STATE_CLOSED) {
    client_->OnConnectionClosed(
        blink::mojom::PresentationSessionInfo::From(connection),
        content::PresentationConnectionCloseReasonToMojo(info.close_reason),
        info.message);
  } else {
    client_->OnConnectionStateChanged(
        blink::mojom::PresentationSessionInfo::From(connection),
        PresentationConnectionStateToMojo(info.state));
  }
}

bool PresentationServiceImpl::FrameMatches(
    content::RenderFrameHost* render_frame_host) const {
  if (!render_frame_host)
    return false;

  return render_frame_host->GetProcess()->GetID() == render_process_id_ &&
         render_frame_host->GetRoutingID() == render_frame_id_;
}

PresentationServiceDelegate*
PresentationServiceImpl::GetPresentationServiceDelegate() {
  return receiver_delegate_
             ? static_cast<PresentationServiceDelegate*>(receiver_delegate_)
             : static_cast<PresentationServiceDelegate*>(controller_delegate_);
}

void PresentationServiceImpl::ListenForConnectionMessages(
    blink::mojom::PresentationSessionInfoPtr session) {
  DVLOG(2) << "ListenForConnectionMessages";
  if (!controller_delegate_)
    return;

  PresentationSessionInfo session_info(session.To<PresentationSessionInfo>());
  controller_delegate_->ListenForConnectionMessages(
      render_process_id_, render_frame_id_, session_info,
      base::Bind(&PresentationServiceImpl::OnConnectionMessages,
                 weak_factory_.GetWeakPtr(), session_info));
}

void PresentationServiceImpl::SetPresentationConnection(
    blink::mojom::PresentationSessionInfoPtr session,
    blink::mojom::PresentationConnectionPtr controller_connection_ptr,
    blink::mojom::PresentationConnectionRequest receiver_connection_request) {
  DVLOG(2) << "SetPresentationConnection";

  if (!controller_delegate_)
    return;

  PresentationSessionInfo session_info(session.To<PresentationSessionInfo>());
  controller_delegate_->ConnectToOffscreenPresentation(
      render_process_id_, render_frame_id_, session_info,
      std::move(controller_connection_ptr),
      std::move(receiver_connection_request));
}

void PresentationServiceImpl::OnConnectionMessages(
    const PresentationSessionInfo& session,
    const std::vector<std::unique_ptr<PresentationConnectionMessage>>& messages,
    bool pass_ownership) {
  DCHECK(client_);

  DVLOG(2) << "OnConnectionMessages"
           << " [id]: " << session.presentation_id;
  std::vector<blink::mojom::ConnectionMessagePtr> mojo_messages(
      messages.size());
  std::transform(
      messages.begin(), messages.end(), mojo_messages.begin(),
      [pass_ownership](
          const std::unique_ptr<PresentationConnectionMessage>& message) {
        return ToMojoConnectionMessage(message.get(), pass_ownership);
      });

  client_->OnConnectionMessagesReceived(
      blink::mojom::PresentationSessionInfo::From(session),
      std::move(mojo_messages));
}

void PresentationServiceImpl::OnReceiverConnectionAvailable(
    const content::PresentationSessionInfo& session_info,
    PresentationConnectionPtr controller_connection_ptr,
    PresentationConnectionRequest receiver_connection_request) {
  DVLOG(2) << "PresentationServiceImpl::OnReceiverConnectionAvailable";

  client_->OnReceiverConnectionAvailable(
      blink::mojom::PresentationSessionInfo::From(session_info),
      std::move(controller_connection_ptr),
      std::move(receiver_connection_request));
}

void PresentationServiceImpl::DidNavigateAnyFrame(
    content::RenderFrameHost* render_frame_host,
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  DVLOG(2) << "PresentationServiceImpl::DidNavigateAnyFrame";
  if (!FrameMatches(render_frame_host))
    return;

  std::string prev_url_host = details.previous_url.host();
  std::string curr_url_host = params.url.host();

  // If a frame navigation is in-page (e.g. navigating to a fragment in
  // same page) then we do not unregister listeners.
  DVLOG(2) << "DidNavigateAnyFrame: "
           << "prev host: " << prev_url_host << ", curr host: " << curr_url_host
           << ", details.is_in_page: " << details.is_in_page;
  if (details.is_in_page)
    return;

  // Reset if the frame actually navigated.
  Reset();
}

void PresentationServiceImpl::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  DVLOG(2) << "PresentationServiceImpl::RenderFrameDeleted";
  if (!FrameMatches(render_frame_host))
    return;

  // RenderFrameDeleted means the associated RFH is going to be deleted soon.
  // This object should also be deleted.
  Reset();
  delete this;
}

void PresentationServiceImpl::WebContentsDestroyed() {
  LOG(ERROR) << "PresentationServiceImpl is being deleted in "
             << "WebContentsDestroyed()! This shouldn't happen since it "
             << "should've been deleted during RenderFrameDeleted().";
  Reset();
  delete this;
}

void PresentationServiceImpl::Reset() {
  DVLOG(2) << "PresentationServiceImpl::Reset";

  if (auto* delegate = GetPresentationServiceDelegate())
    delegate->Reset(render_process_id_, render_frame_id_);

  default_presentation_urls_.clear();

  screen_availability_listeners_.clear();

  start_session_request_id_ = kInvalidRequestSessionId;
  pending_start_session_cb_.reset();

  pending_join_session_cbs_.clear();

  if (on_connection_messages_callback_.get()) {
    on_connection_messages_callback_->Run(
        std::vector<blink::mojom::ConnectionMessagePtr>());
    on_connection_messages_callback_.reset();
  }

  if (send_message_callback_) {
    // Run the callback with false, indicating the renderer to stop sending
    // the requests and invalidate all pending requests.
    send_message_callback_->Run(false);
    send_message_callback_.reset();
  }
}

void PresentationServiceImpl::OnDelegateDestroyed() {
  DVLOG(2) << "PresentationServiceImpl::OnDelegateDestroyed";
  controller_delegate_ = nullptr;
  receiver_delegate_ = nullptr;
  Reset();
}

void PresentationServiceImpl::OnDefaultPresentationStarted(
    const PresentationSessionInfo& connection) {
  DCHECK(client_.get());
  client_->OnDefaultSessionStarted(
      blink::mojom::PresentationSessionInfo::From(connection));
  ListenForConnectionStateChange(connection);
}

PresentationServiceImpl::ScreenAvailabilityListenerImpl::
    ScreenAvailabilityListenerImpl(const GURL& availability_url,
                                   PresentationServiceImpl* service)
    : availability_url_(availability_url), service_(service) {
  DCHECK(service_);
  DCHECK(service_->client_.get());
}

PresentationServiceImpl::ScreenAvailabilityListenerImpl::
~ScreenAvailabilityListenerImpl() {
}

GURL PresentationServiceImpl::ScreenAvailabilityListenerImpl::
    GetAvailabilityUrl() const {
  return availability_url_;
}

void PresentationServiceImpl::ScreenAvailabilityListenerImpl
::OnScreenAvailabilityChanged(bool available) {
  service_->client_->OnScreenAvailabilityUpdated(availability_url_, available);
}

void PresentationServiceImpl::ScreenAvailabilityListenerImpl
::OnScreenAvailabilityNotSupported() {
  service_->client_->OnScreenAvailabilityNotSupported(availability_url_);
}

PresentationServiceImpl::NewSessionCallbackWrapper
::NewSessionCallbackWrapper(const NewSessionCallback& callback)
    : callback_(callback) {
}

PresentationServiceImpl::NewSessionCallbackWrapper
::~NewSessionCallbackWrapper() {
  if (!callback_.is_null())
    InvokeNewSessionCallbackWithError(callback_);
}

void PresentationServiceImpl::NewSessionCallbackWrapper::Run(
    blink::mojom::PresentationSessionInfoPtr session,
    blink::mojom::PresentationErrorPtr error) {
  DCHECK(!callback_.is_null());
  callback_.Run(std::move(session), std::move(error));
  callback_.Reset();
}

}  // namespace content
