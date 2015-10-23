// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CLIENT_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CLIENT_H_

#include <map>
#include <string>
#include <vector>

#include "base/id_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/service_worker_event_status.mojom.h"
#include "ipc/ipc_listener.h"
#include "mojo/application/public/interfaces/service_provider.mojom.h"
#include "third_party/WebKit/public/platform/WebGeofencingEventType.h"
#include "third_party/WebKit/public/platform/WebMessagePortChannel.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerError.h"
#include "third_party/WebKit/public/web/modules/serviceworker/WebServiceWorkerContextClient.h"
#include "v8/include/v8.h"

namespace base {
class SingleThreadTaskRunner;
class TaskRunner;
}

namespace blink {
struct WebCircularGeofencingRegion;
struct WebCrossOriginServiceWorkerClient;
class WebDataSource;
struct WebServiceWorkerClientQueryOptions;
class WebServiceWorkerContextProxy;
class WebServiceWorkerProvider;
struct WebSyncRegistration;
}

namespace IPC {
class Message;
}

namespace content {

struct NavigatorConnectClient;
struct PlatformNotificationData;
struct ServiceWorkerClientInfo;
class ServiceWorkerProviderContext;
class ServiceWorkerContextClient;
class ThreadSafeSender;
class WebServiceWorkerRegistrationImpl;

// This class provides access to/from an ServiceWorker's WorkerGlobalScope.
// Unless otherwise noted, all methods are called on the worker thread.
class ServiceWorkerContextClient
    : public blink::WebServiceWorkerContextClient {
 public:
  using SyncCallback = mojo::Callback<void(ServiceWorkerEventStatus)>;

  // Returns a thread-specific client instance.  This does NOT create a
  // new instance.
  static ServiceWorkerContextClient* ThreadSpecificInstance();

  // Called on the main thread.
  ServiceWorkerContextClient(int embedded_worker_id,
                             int64 service_worker_version_id,
                             const GURL& service_worker_scope,
                             const GURL& script_url,
                             int worker_devtools_agent_route_id);
  ~ServiceWorkerContextClient() override;

  void OnMessageReceived(int thread_id,
                         int embedded_worker_id,
                         const IPC::Message& message);

  // Called some time after the worker has started. Attempts to use the
  // ServiceRegistry to connect to services before this method is called are
  // queued up and will resolve after this method is called.
  void BindServiceRegistry(
      mojo::InterfaceRequest<mojo::ServiceProvider> services,
      mojo::ServiceProviderPtr exposed_services);

  // WebServiceWorkerContextClient overrides.
  virtual blink::WebURL scope() const;
  virtual void getClients(const blink::WebServiceWorkerClientQueryOptions&,
                          blink::WebServiceWorkerClientsCallbacks*);
  virtual void openWindow(const blink::WebURL&,
                          blink::WebServiceWorkerClientCallbacks*);
  virtual void setCachedMetadata(const blink::WebURL&,
                                 const char* data,
                                 size_t size);
  virtual void clearCachedMetadata(const blink::WebURL&);
  virtual void workerReadyForInspection();

  // Called on the main thread.
  virtual void workerContextFailedToStart();

  virtual void workerContextStarted(blink::WebServiceWorkerContextProxy* proxy);
  virtual void didEvaluateWorkerScript(bool success);
  virtual void didInitializeWorkerContext(v8::Local<v8::Context> context,
                                          const blink::WebURL& url);
  virtual void willDestroyWorkerContext();
  virtual void workerContextDestroyed();
  virtual void reportException(const blink::WebString& error_message,
                               int line_number,
                               int column_number,
                               const blink::WebString& source_url);
  virtual void reportConsoleMessage(int source,
                                    int level,
                                    const blink::WebString& message,
                                    int line_number,
                                    const blink::WebString& source_url);
  virtual void sendDevToolsMessage(int call_id,
                                   const blink::WebString& message,
                                   const blink::WebString& state);
  virtual void didHandleActivateEvent(int request_id,
                                      blink::WebServiceWorkerEventResult);
  virtual void didHandleInstallEvent(int request_id,
                                     blink::WebServiceWorkerEventResult result);
  virtual void didHandleFetchEvent(int request_id);
  virtual void didHandleFetchEvent(
      int request_id,
      const blink::WebServiceWorkerResponse& response);
  virtual void didHandleNotificationClickEvent(
      int request_id,
      blink::WebServiceWorkerEventResult result);
  virtual void didHandlePushEvent(int request_id,
                                  blink::WebServiceWorkerEventResult result);
  virtual void didHandleSyncEvent(int request_id,
                                  blink::WebServiceWorkerEventResult result);

  // Called on the main thread.
  virtual blink::WebServiceWorkerNetworkProvider*
      createServiceWorkerNetworkProvider(blink::WebDataSource* data_source);
  virtual blink::WebServiceWorkerProvider* createServiceWorkerProvider();

  virtual void postMessageToClient(
      const blink::WebString& uuid,
      const blink::WebString& message,
      blink::WebMessagePortChannelArray* channels);
  virtual void postMessageToCrossOriginClient(
      const blink::WebCrossOriginServiceWorkerClient& client,
      const blink::WebString& message,
      blink::WebMessagePortChannelArray* channels);
  virtual void focus(const blink::WebString& uuid,
                     blink::WebServiceWorkerClientCallbacks*);
  virtual void navigate(const blink::WebString& uuid,
                        const blink::WebURL&,
                        blink::WebServiceWorkerClientCallbacks*);
  virtual void skipWaiting(
      blink::WebServiceWorkerSkipWaitingCallbacks* callbacks);
  virtual void claim(blink::WebServiceWorkerClientsClaimCallbacks* callbacks);

  virtual void DispatchSyncEvent(const blink::WebSyncRegistration& registration,
                                 const SyncCallback& callback);

 private:
  struct WorkerContextData;

  // Get routing_id for sending message to the ServiceWorkerVersion
  // in the browser process.
  int GetRoutingID() const { return embedded_worker_id_; }

  void Send(IPC::Message* message);
  void SendWorkerStarted();
  void SetRegistrationInServiceWorkerGlobalScope();

  void OnActivateEvent(int request_id);
  void OnInstallEvent(int request_id);
  void OnFetchEvent(int request_id, const ServiceWorkerFetchRequest& request);
  void OnNotificationClickEvent(
      int request_id,
      int64_t persistent_notification_id,
      const PlatformNotificationData& notification_data,
      int action_index);
  void OnPushEvent(int request_id, const std::string& data);
  void OnGeofencingEvent(int request_id,
                         blink::WebGeofencingEventType event_type,
                         const std::string& region_id,
                         const blink::WebCircularGeofencingRegion& region);
  void OnPostMessage(
      const base::string16& message,
      const std::vector<TransferredMessagePort>& sent_message_ports,
      const std::vector<int>& new_routing_ids);
  void OnCrossOriginMessageToWorker(
      const NavigatorConnectClient& client,
      const base::string16& message,
      const std::vector<TransferredMessagePort>& sent_message_ports,
      const std::vector<int>& new_routing_ids);
  void OnDidGetClients(
      int request_id, const std::vector<ServiceWorkerClientInfo>& clients);
  void OnOpenWindowResponse(int request_id,
                            const ServiceWorkerClientInfo& client);
  void OnOpenWindowError(int request_id, const std::string& message);
  void OnFocusClientResponse(int request_id,
                             const ServiceWorkerClientInfo& client);
  void OnNavigateClientResponse(int request_id,
                                const ServiceWorkerClientInfo& client);
  void OnNavigateClientError(int request_id, const GURL& url);
  void OnDidSkipWaiting(int request_id);
  void OnDidClaimClients(int request_id);
  void OnClaimClientsError(int request_id,
                           blink::WebServiceWorkerError::ErrorType error_type,
                           const base::string16& message);
  void OnPing();

  base::WeakPtr<ServiceWorkerContextClient> GetWeakPtr();

  const int embedded_worker_id_;
  const int64 service_worker_version_id_;
  const GURL service_worker_scope_;
  const GURL script_url_;
  const int worker_devtools_agent_route_id_;
  scoped_refptr<ThreadSafeSender> sender_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  scoped_refptr<base::TaskRunner> worker_task_runner_;

  scoped_refptr<ServiceWorkerProviderContext> provider_context_;

  // Not owned; this object is destroyed when proxy_ becomes invalid.
  blink::WebServiceWorkerContextProxy* proxy_;

  // Used for incoming messages from the browser for which an outgoing response
  // back to the browser is expected, the id must be sent back with the
  // response.
  int current_request_id_;

  // Initialized on the worker thread in workerContextStarted and
  // destructed on the worker thread in willDestroyWorkerContext.
  scoped_ptr<WorkerContextData> context_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerContextClient);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CLIENT_H_
