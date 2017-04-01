// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPresentationClient_h
#define WebPresentationClient_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebCommon.h"

#include <memory>

namespace blink {

class WebPresentationAvailabilityObserver;
class WebPresentationController;
struct WebPresentationError;
class WebPresentationReceiver;
struct WebPresentationSessionInfo;
class WebString;
class WebURL;
template <typename T>
class WebVector;

// If session was created, callback's onSuccess() is invoked with the
// information about the presentation session created by the embedder.
// Otherwise, onError() is invoked with the error code and message.
using WebPresentationConnectionCallback =
    WebCallbacks<const WebPresentationSessionInfo&,
                 const WebPresentationError&>;

// Callback for .getAvailability().
using WebPresentationAvailabilityCallbacks =
    WebCallbacks<bool, const WebPresentationError&>;

// The implementation the embedder has to provide for the Presentation API to
// work.
class WebPresentationClient {
 public:
  virtual ~WebPresentationClient() {}

  // Passes the Blink-side delegate to the embedder.
  virtual void setController(WebPresentationController*) = 0;

  // Passes the Blink-side delegate to the embedder.
  virtual void setReceiver(WebPresentationReceiver*) = 0;

  // Called when the frame requests to start a new session.
  virtual void startSession(
      const WebVector<WebURL>& presentationUrls,
      std::unique_ptr<WebPresentationConnectionCallback>) = 0;

  // Called when the frame requests to join an existing session.
  virtual void joinSession(
      const WebVector<WebURL>& presentationUrls,
      const WebString& presentationId,
      std::unique_ptr<WebPresentationConnectionCallback>) = 0;

  // Called when the frame requests to send String message to an existing
  // session.
  virtual void sendString(const WebURL& presentationUrl,
                          const WebString& presentationId,
                          const WebString& message) = 0;

  // Called when the frame requests to send ArrayBuffer/View data to an existing
  // session.  Embedder copies the |data| and the ownership is not transferred.
  virtual void sendArrayBuffer(const WebURL& presentationUrl,
                               const WebString& presentationId,
                               const uint8_t* data,
                               size_t length) = 0;

  // Called when the frame requests to send Blob data to an existing session.
  // Embedder copies the |data| and the ownership is not transferred.
  virtual void sendBlobData(const WebURL& presentationUrl,
                            const WebString& presentationId,
                            const uint8_t* data,
                            size_t length) = 0;

  // Called when the frame requests to close an existing session.
  virtual void closeSession(const WebURL& presentationUrl,
                            const WebString& presentationId) = 0;

  // Called when the frame requests to terminate an existing session.
  virtual void terminateSession(const WebURL& presentationUrl,
                                const WebString& presentationId) = 0;

  // Called when the frame wants to know the availability of a presentation
  // display for |availabilityUrl|.
  virtual void getAvailability(
      const WebVector<WebURL>& availabilityUrls,
      std::unique_ptr<WebPresentationAvailabilityCallbacks>) = 0;

  // Start listening to changes in presentation displays availability. The
  // observer will be notified in case of a change. The observer is
  // respensible to call stopListening() before being destroyed.
  virtual void startListening(WebPresentationAvailabilityObserver*) = 0;

  // Stop listening to changes in presentation displays availability. The
  // observer will no longer be notified in case of a change.
  virtual void stopListening(WebPresentationAvailabilityObserver*) = 0;

  // Called when a defaultRequest has been set. It sends the url associated
  // with it for the embedder.
  virtual void setDefaultPresentationUrls(const WebVector<WebURL>&) = 0;
};

}  // namespace blink

#endif  // WebPresentationClient_h
