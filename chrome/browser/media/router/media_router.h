// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/media/router/media_route.h"
#include "chrome/browser/media/router/media_route_id.h"
#include "chrome/browser/media/router/media_sink.h"
#include "chrome/browser/media/router/media_source.h"

namespace media_router {

class MediaRoutesObserver;
class MediaSinksObserver;

// Type of callback used in |RequestRoute()|. Callback is invoked when the
// route request either succeeded or failed.
// The first argument is the route created. If the route request failed, this
// will be a nullptr.
// The second argument is the error string, which will be nonempty if the route
// request failed.
using MediaRouteResponseCallback =
    base::Callback<void(scoped_ptr<MediaRoute>, const std::string&)>;

// An interface for handling resources related to media routing.
// Responsible for registering observers for receiving sink availability
// updates, handling route requests/responses, and operating on routes (e.g.
// posting messages or closing).
class MediaRouter {
 public:
  virtual ~MediaRouter() {}

  // Requests a media route from |source| to |sink_id|.
  // |callback| is invoked with a response indicating success or failure.
  virtual void RequestRoute(const MediaSourceId& source,
                            const MediaSinkId& sink_id,
                            const MediaRouteResponseCallback& callback) = 0;

  // Closes the media route specified by |route_id|.
  virtual void CloseRoute(const MediaRouteId& route_id) = 0;

  // Posts |message| to a MediaSink connected via MediaRoute with |route_id|.
  // TODO(imcheng): Support additional data types: Blob, ArrayBuffer,
  // ArrayBufferView.
  virtual void PostMessage(const MediaRouteId& route_id,
                           const std::string& message) = 0;

  // Receives updates from a MediaRouter instance.
  class Delegate {
   public:
    // Called when there is a message from a route.
    // |route_id|: The route ID.
    // |message|: The message.
    virtual void OnMessage(const MediaRouteId& route_id,
                           const std::string& message) = 0;
  };

 protected:
  friend class MediaSinksObserver;
  friend class MediaRoutesObserver;

  // The following APIs are called by MediaSinksObserver/MediaRoutesObserver
  // and implementations of MediaRouter only.

  // Registers |observer| with this MediaRouter. |observer| specifies a media
  // source and will receive updates with media sinks that are compatible with
  // that source. The initial update may happen synchronously.
  // NOTE: This class does not assume ownership of |observer|. Callers must
  // manage |observer| and make sure |UnregisterObserver()| is called
  // before the observer is destroyed.
  // Returns true if registration succeeded.
  // It is invalid to request the same observer more than once and will result
  // in undefined behavior.
  // If the MRPM Host is not available, the registration request will fail
  // immediately.
  virtual bool RegisterMediaSinksObserver(MediaSinksObserver* observer) = 0;

  // Removes a previously added MediaSinksObserver. |observer| will stop
  // receiving further updates.
  virtual void UnregisterMediaSinksObserver(MediaSinksObserver* observer) = 0;

  // Adds a MediaRoutesObserver to listen for updates on MediaRoutes.
  // The initial update may happen synchronously.
  // MediaRouter does not own |observer|. |RemoveMediaRoutesObserver| should
  // be called before |observer| is destroyed.
  virtual bool RegisterMediaRoutesObserver(MediaRoutesObserver* observer) = 0;

  // Removes a previously added MediaRoutesObserver. |observer| will stop
  // receiving further updates.
  virtual void UnregisterMediaRoutesObserver(MediaRoutesObserver* observer) = 0;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_H_
