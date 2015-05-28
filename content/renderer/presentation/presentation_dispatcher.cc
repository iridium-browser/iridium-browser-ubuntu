// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/presentation/presentation_dispatcher.h"

#include "base/logging.h"
#include "content/common/presentation/presentation_service.mojom.h"
#include "content/public/common/service_registry.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/presentation/presentation_session_client.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationController.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationError.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "url/gurl.h"

namespace {

blink::WebPresentationError::ErrorType GetWebPresentationErrorTypeFromMojo(
    presentation::PresentationErrorType mojoErrorType) {
  switch (mojoErrorType) {
    case presentation::PRESENTATION_ERROR_TYPE_NO_AVAILABLE_SCREENS:
      return blink::WebPresentationError::ErrorTypeNoAvailableScreens;
    case presentation::PRESENTATION_ERROR_TYPE_SESSION_REQUEST_CANCELLED:
      return blink::WebPresentationError::ErrorTypeSessionRequestCancelled;
    case presentation::PRESENTATION_ERROR_TYPE_NO_PRESENTATION_FOUND:
      return blink::WebPresentationError::ErrorTypeNoPresentationFound;
    case presentation::PRESENTATION_ERROR_TYPE_UNKNOWN:
    default:
      return blink::WebPresentationError::ErrorTypeUnknown;
  }
}

blink::WebPresentationSessionState GetWebPresentationSessionStateFromMojo(
        presentation::PresentationSessionState mojoSessionState) {
  switch (mojoSessionState) {
    case presentation::PRESENTATION_SESSION_STATE_CONNECTED:
      return blink::WebPresentationSessionState::Connected;
    case presentation::PRESENTATION_SESSION_STATE_DISCONNECTED:
      return blink::WebPresentationSessionState::Disconnected;
  }

  NOTREACHED();
  return blink::WebPresentationSessionState::Disconnected;
}

GURL GetPresentationURLFromFrame(content::RenderFrame* frame) {
  DCHECK(frame);

  GURL url(frame->GetWebFrame()->document().defaultPresentationURL());
  return url.is_valid() ? url : GURL();
}

}  // namespace

namespace content {

PresentationDispatcher::PresentationDispatcher(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      controller_(nullptr) {
}

PresentationDispatcher::~PresentationDispatcher() {
  // Controller should be destroyed before the dispatcher when frame is
  // destroyed.
  DCHECK(!controller_);
}

void PresentationDispatcher::setController(
    blink::WebPresentationController* controller) {
  // There shouldn't be any swapping from one non-null controller to another.
  DCHECK(controller != controller_ && (!controller || !controller_));
  controller_ = controller;
  // The controller is set to null when the frame is about to be detached.
  // Nothing is listening for screen availability anymore but the Mojo service
  // will know about the frame being detached anyway.
}

void PresentationDispatcher::updateAvailableChangeWatched(bool watched) {
  GURL presentation_url(GetPresentationURLFromFrame(render_frame()));
  DoUpdateAvailableChangeWatched(presentation_url.spec(), watched);
}

void PresentationDispatcher::DoUpdateAvailableChangeWatched(
    const std::string& presentation_url, bool watched) {
  ConnectToPresentationServiceIfNeeded();
  if (watched) {
    presentation_service_->ListenForScreenAvailability(
        presentation_url,
        base::Bind(&PresentationDispatcher::OnScreenAvailabilityChanged,
                 base::Unretained(this)));
  } else {
    presentation_service_->RemoveScreenAvailabilityListener(presentation_url);
  }
}

void PresentationDispatcher::startSession(
    const blink::WebString& presentationUrl,
    const blink::WebString& presentationId,
    blink::WebPresentationSessionClientCallbacks* callback) {
  DCHECK(callback);
  ConnectToPresentationServiceIfNeeded();

  // The dispatcher owns the service so |this| will be valid when
  // OnSessionCreated() is called. |callback| needs to be alive and also needs
  // to be destroyed so we transfer its ownership to the mojo callback.
  presentation_service_->StartSession(
      presentationUrl.utf8(),
      presentationId.utf8(),
      base::Bind(&PresentationDispatcher::OnSessionCreated,
          base::Unretained(this),
          base::Owned(callback)));
}

void PresentationDispatcher::joinSession(
    const blink::WebString& presentationUrl,
    const blink::WebString& presentationId,
    blink::WebPresentationSessionClientCallbacks* callback) {
  DCHECK(callback);
  ConnectToPresentationServiceIfNeeded();

  // The dispatcher owns the service so |this| will be valid when
  // OnSessionCreated() is called. |callback| needs to be alive and also needs
  // to be destroyed so we transfer its ownership to the mojo callback.
  presentation_service_->JoinSession(
      presentationUrl.utf8(),
      presentationId.utf8(),
      base::Bind(&PresentationDispatcher::OnSessionCreated,
          base::Unretained(this),
          base::Owned(callback)));
}

void PresentationDispatcher::closeSession(
    const blink::WebString& presentationUrl,
    const blink::WebString& presentationId) {
  ConnectToPresentationServiceIfNeeded();

  presentation_service_->CloseSession(
      presentationUrl.utf8(),
      presentationId.utf8());
}

void PresentationDispatcher::DidChangeDefaultPresentation() {
  GURL presentation_url(GetPresentationURLFromFrame(render_frame()));

  ConnectToPresentationServiceIfNeeded();
  presentation_service_->SetDefaultPresentationURL(
      presentation_url.spec(), mojo::String());
}

void PresentationDispatcher::OnScreenAvailabilityChanged(
    const std::string& presentation_url, bool available) {
  if (!controller_)
    return;

  // Reset the callback to get the next event.
  DoUpdateAvailableChangeWatched(presentation_url,
                                 controller_->isAvailableChangeWatched());

  controller_->didChangeAvailability(available);
}

void PresentationDispatcher::OnDefaultSessionStarted(
    presentation::PresentationSessionInfoPtr session_info) {
  if (!controller_)
    return;

  // Reset the callback to get the next event.
  presentation_service_->ListenForDefaultSessionStart(base::Bind(
      &PresentationDispatcher::OnDefaultSessionStarted,
      base::Unretained(this)));

  DCHECK(!session_info.is_null());
  controller_->didStartDefaultSession(
      new PresentationSessionClient(session_info.Pass()));
}

void PresentationDispatcher::OnSessionCreated(
    blink::WebPresentationSessionClientCallbacks* callback,
    presentation::PresentationSessionInfoPtr session_info,
    presentation::PresentationErrorPtr error) {
  DCHECK(callback);
  if (!error.is_null()) {
    DCHECK(session_info.is_null());
    callback->onError(new blink::WebPresentationError(
        GetWebPresentationErrorTypeFromMojo(error->error_type),
        blink::WebString::fromUTF8(error->message)));
    return;
  }

  DCHECK(!session_info.is_null());
  callback->onSuccess(new PresentationSessionClient(session_info.Pass()));
}

void PresentationDispatcher::OnSessionStateChange(
    presentation::PresentationSessionInfoPtr session_info,
    presentation::PresentationSessionState session_state) {
  if (!controller_)
    return;

  presentation_service_->ListenForSessionStateChange(base::Bind(
      &PresentationDispatcher::OnSessionStateChange,
      base::Unretained(this)));

  DCHECK(!session_info.is_null());
  controller_->didChangeSessionState(
      new PresentationSessionClient(session_info.Pass()),
      GetWebPresentationSessionStateFromMojo(session_state));
}

void PresentationDispatcher::ConnectToPresentationServiceIfNeeded() {
  if (presentation_service_.get())
    return;

  render_frame()->GetServiceRegistry()->ConnectToRemoteService(
      &presentation_service_);
  // TODO(imcheng): Uncomment these once they are implemented on the browser
  // side. (crbug.com/459006)
  /*
  presentation_service_->ListenForDefaultSessionStart(base::Bind(
      &PresentationDispatcher::OnDefaultSessionStarted,
      base::Unretained(this)));
  presentation_service_->ListenForSessionStateChange(base::Bind(
      &PresentationDispatcher::OnSessionStateChange,
      base::Unretained(this)));
  */
}

}  // namespace content
