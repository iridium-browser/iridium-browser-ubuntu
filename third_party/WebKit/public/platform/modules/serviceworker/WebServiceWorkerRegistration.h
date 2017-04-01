// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerRegistration_h
#define WebServiceWorkerRegistration_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebURL.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerError.h"
#include <memory>

namespace blink {

class WebServiceWorkerProvider;
class WebServiceWorkerRegistrationProxy;
struct WebNavigationPreloadState;

// The interface of the registration representation in the embedder. The
// embedder implements this interface and passes its handle
// (WebServiceWorkerRegistration::Handle) to Blink. Blink accesses the
// implementation via the handle to update or unregister the registration.
class WebServiceWorkerRegistration {
 public:
  virtual ~WebServiceWorkerRegistration() {}

  using WebServiceWorkerUpdateCallbacks =
      WebCallbacks<void, const WebServiceWorkerError&>;
  using WebServiceWorkerUnregistrationCallbacks =
      WebCallbacks<bool, const WebServiceWorkerError&>;
  using WebEnableNavigationPreloadCallbacks =
      WebCallbacks<void, const WebServiceWorkerError&>;
  using WebGetNavigationPreloadStateCallbacks =
      WebCallbacks<const WebNavigationPreloadState&,
                   const WebServiceWorkerError&>;
  using WebSetNavigationPreloadHeaderCallbacks =
      WebCallbacks<void, const WebServiceWorkerError&>;

  // The handle interface that retains a reference to the implementation of
  // WebServiceWorkerRegistration in the embedder and is owned by
  // ServiceWorkerRegistration object in Blink. The embedder must keep the
  // registration representation while Blink is owning this handle.
  class Handle {
   public:
    virtual ~Handle() {}
    virtual WebServiceWorkerRegistration* registration() { return nullptr; }
  };

  virtual void setProxy(WebServiceWorkerRegistrationProxy*) {}
  virtual WebServiceWorkerRegistrationProxy* proxy() { return nullptr; }
  virtual void proxyStopped() {}

  virtual WebURL scope() const { return WebURL(); }
  virtual int64_t registrationId() const = 0;
  virtual void update(WebServiceWorkerProvider*,
                      std::unique_ptr<WebServiceWorkerUpdateCallbacks>) {}
  virtual void unregister(
      WebServiceWorkerProvider*,
      std::unique_ptr<WebServiceWorkerUnregistrationCallbacks>) {}

  virtual void enableNavigationPreload(
      bool enable,
      WebServiceWorkerProvider*,
      std::unique_ptr<WebEnableNavigationPreloadCallbacks>) {}
  virtual void getNavigationPreloadState(
      WebServiceWorkerProvider*,
      std::unique_ptr<WebGetNavigationPreloadStateCallbacks>) {}
  virtual void setNavigationPreloadHeader(
      const WebString& value,
      WebServiceWorkerProvider*,
      std::unique_ptr<WebSetNavigationPreloadHeaderCallbacks>) {}
};

}  // namespace blink

#endif  // WebServiceWorkerRegistration_h
