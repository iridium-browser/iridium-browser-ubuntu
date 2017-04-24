/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ServiceWorkerGlobalScopeClient_h
#define ServiceWorkerGlobalScopeClient_h

#include "core/dom/MessagePort.h"
#include "core/workers/WorkerClients.h"
#include "modules/ModulesExport.h"
#include "public/platform/WebMessagePortChannel.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerClientsClaimCallbacks.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerClientsInfo.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerEventResult.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerSkipWaitingCallbacks.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include <memory>

namespace blink {

struct WebServiceWorkerClientQueryOptions;
class ExecutionContext;
class WebServiceWorkerResponse;
class WebURL;
class WorkerClients;

// See WebServiceWorkerContextClient for documentation for the methods in this
// class.
class MODULES_EXPORT ServiceWorkerGlobalScopeClient
    : public Supplement<WorkerClients> {
  WTF_MAKE_NONCOPYABLE(ServiceWorkerGlobalScopeClient);
  DISALLOW_NEW();

 public:
  virtual ~ServiceWorkerGlobalScopeClient() {}

  // Called from ServiceWorkerClients.
  virtual void getClient(const WebString&,
                         std::unique_ptr<WebServiceWorkerClientCallbacks>) = 0;
  virtual void getClients(
      const WebServiceWorkerClientQueryOptions&,
      std::unique_ptr<WebServiceWorkerClientsCallbacks>) = 0;
  virtual void openWindow(const WebURL&,
                          std::unique_ptr<WebServiceWorkerClientCallbacks>) = 0;
  virtual void setCachedMetadata(const WebURL&, const char*, size_t) = 0;
  virtual void clearCachedMetadata(const WebURL&) = 0;

  virtual WebURL scope() const = 0;

  virtual void didHandleActivateEvent(int eventID,
                                      WebServiceWorkerEventResult,
                                      double eventDispatchTime) = 0;
  virtual void didHandleExtendableMessageEvent(int eventID,
                                               WebServiceWorkerEventResult,
                                               double eventDispatchTime) = 0;
  // Calling respondToFetchEvent without response means no response was
  // provided by the service worker in the fetch events, so fallback to native.
  virtual void respondToFetchEvent(int fetchEventID,
                                   double eventDispatchTime) = 0;
  virtual void respondToFetchEvent(int fetchEventID,
                                   const WebServiceWorkerResponse&,
                                   double eventDispatchTime) = 0;
  virtual void didHandleFetchEvent(int fetchEventID,
                                   WebServiceWorkerEventResult,
                                   double eventDispatchTime) = 0;
  virtual void didHandleInstallEvent(int installEventID,
                                     WebServiceWorkerEventResult,
                                     double eventDispatchTime) = 0;
  virtual void didHandleNotificationClickEvent(int eventID,
                                               WebServiceWorkerEventResult,
                                               double eventDispatchTime) = 0;
  virtual void didHandleNotificationCloseEvent(int eventID,
                                               WebServiceWorkerEventResult,
                                               double eventDispatchTime) = 0;
  virtual void didHandlePushEvent(int pushEventID,
                                  WebServiceWorkerEventResult,
                                  double eventDispatchTime) = 0;
  virtual void didHandleSyncEvent(int syncEventID,
                                  WebServiceWorkerEventResult,
                                  double eventDispatchTime) = 0;
  virtual void didHandlePaymentRequestEvent(int paymentRequestEventID,
                                            WebServiceWorkerEventResult,
                                            double eventDispatchTime) = 0;
  virtual void postMessageToClient(const WebString& clientUUID,
                                   const WebString& message,
                                   WebMessagePortChannelArray) = 0;
  virtual void skipWaiting(
      std::unique_ptr<WebServiceWorkerSkipWaitingCallbacks>) = 0;
  virtual void claim(
      std::unique_ptr<WebServiceWorkerClientsClaimCallbacks>) = 0;
  virtual void focus(const WebString& clientUUID,
                     std::unique_ptr<WebServiceWorkerClientCallbacks>) = 0;
  virtual void navigate(const WebString& clientUUID,
                        const WebURL&,
                        std::unique_ptr<WebServiceWorkerClientCallbacks>) = 0;
  virtual void registerForeignFetchScopes(
      const WebVector<WebURL>& subScopes,
      const WebVector<WebSecurityOrigin>&) = 0;

  static const char* supplementName();
  static ServiceWorkerGlobalScopeClient* from(ExecutionContext*);

 protected:
  ServiceWorkerGlobalScopeClient() {}
};

MODULES_EXPORT void provideServiceWorkerGlobalScopeClientToWorker(
    WorkerClients*,
    ServiceWorkerGlobalScopeClient*);

}  // namespace blink

#endif  // ServiceWorkerGlobalScopeClient_h
