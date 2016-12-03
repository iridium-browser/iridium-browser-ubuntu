// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_CREATE_PRESENTATION_CONNECTION_REQUEST_H_
#define CHROME_BROWSER_MEDIA_ROUTER_CREATE_PRESENTATION_CONNECTION_REQUEST_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/media/router/media_route.h"
#include "chrome/browser/media/router/media_source.h"
#include "chrome/browser/media/router/presentation_request.h"
#include "chrome/browser/media/router/render_frame_host_id.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "url/gurl.h"

namespace content {
struct PresentationError;
struct PresentationSessionInfo;
}  // namespace content

namespace media_router {

class RouteRequestResult;

// Holds parameters for creating a presentation session.
// A request object is created by presentation_service_delegate_impl when it
// gets create-session request. The object is then passed to and owned by the
// MediaRouterUI. |success_cb| will be invoked when create-session
// succeeds, or |error_cb| will be invoked when create-session fails or
// the UI closes.
class CreatePresentationConnectionRequest {
 public:
  using PresentationSessionSuccessCallback =
      base::Callback<void(const content::PresentationSessionInfo&,
                          const MediaRoute::Id&)>;
  using PresentationSessionErrorCallback =
      content::PresentationSessionErrorCallback;
  // |presentation_url|: The presentation URL of the request. Must be a valid
  //                     URL.
  // |frame_url|: The URL of the frame that initiated the presentation request.
  // |success_cb|: Callback to invoke when the request succeeds. Must be valid.
  // |erorr_cb|: Callback to invoke when the request fails. Must be valid.
  CreatePresentationConnectionRequest(
      const RenderFrameHostId& render_frame_host_id,
      const std::string& presentation_url,
      const GURL& frame_url,
      const PresentationSessionSuccessCallback& success_cb,
      const PresentationSessionErrorCallback& error_cb);
  ~CreatePresentationConnectionRequest();

  const PresentationRequest& presentation_request() const {
    return presentation_request_;
  }

  // Invokes |success_cb_| or |error_cb_| with the given arguments.
  // These functions can only be invoked once per instance. It is an error
  // to invoke these functions more than once.
  void InvokeSuccessCallback(const std::string& presentation_id,
                             const MediaRoute::Id& route_id);
  void InvokeErrorCallback(const content::PresentationError& error);

  // Handle route creation/joining response by invoking the right callback.
  static void HandleRouteResponse(
      std::unique_ptr<CreatePresentationConnectionRequest> presentation_request,
      const RouteRequestResult& result);

 private:
  const PresentationRequest presentation_request_;
  PresentationSessionSuccessCallback success_cb_;
  PresentationSessionErrorCallback error_cb_;
  bool cb_invoked_;

  DISALLOW_COPY_AND_ASSIGN(CreatePresentationConnectionRequest);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_CREATE_PRESENTATION_CONNECTION_REQUEST_H_
