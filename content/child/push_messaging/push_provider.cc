// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/push_messaging/push_provider.h"

#include <memory>
#include <utility>

#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/threading/thread_local.h"
#include "content/child/push_messaging/push_dispatcher.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/push_messaging_messages.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/push_subscription_options.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushSubscription.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushSubscriptionOptions.h"

namespace content {
namespace {

int CurrentWorkerId() {
  return WorkerThread::GetCurrentId();
}

// Returns the id of the given |service_worker_registration|, which
// is only available on the implementation of the interface.
int64_t GetServiceWorkerRegistrationId(
    blink::WebServiceWorkerRegistration* service_worker_registration) {
  return static_cast<WebServiceWorkerRegistrationImpl*>(
             service_worker_registration)
      ->registrationId();
}

}  // namespace

blink::WebPushError PushRegistrationStatusToWebPushError(
    PushRegistrationStatus status) {
  blink::WebPushError::ErrorType error_type =
      blink::WebPushError::ErrorTypeAbort;
  switch (status) {
    case PUSH_REGISTRATION_STATUS_PERMISSION_DENIED:
      error_type = blink::WebPushError::ErrorTypeNotAllowed;
      break;
    case PUSH_REGISTRATION_STATUS_SENDER_ID_MISMATCH:
      error_type = blink::WebPushError::ErrorTypeInvalidState;
      break;
    case PUSH_REGISTRATION_STATUS_SUCCESS_FROM_PUSH_SERVICE:
    case PUSH_REGISTRATION_STATUS_NO_SERVICE_WORKER:
    case PUSH_REGISTRATION_STATUS_SERVICE_NOT_AVAILABLE:
    case PUSH_REGISTRATION_STATUS_LIMIT_REACHED:
    case PUSH_REGISTRATION_STATUS_SERVICE_ERROR:
    case PUSH_REGISTRATION_STATUS_NO_SENDER_ID:
    case PUSH_REGISTRATION_STATUS_STORAGE_ERROR:
    case PUSH_REGISTRATION_STATUS_SUCCESS_FROM_CACHE:
    case PUSH_REGISTRATION_STATUS_NETWORK_ERROR:
    case PUSH_REGISTRATION_STATUS_INCOGNITO_PERMISSION_DENIED:
    case PUSH_REGISTRATION_STATUS_PUBLIC_KEY_UNAVAILABLE:
    case PUSH_REGISTRATION_STATUS_MANIFEST_EMPTY_OR_MISSING:
      error_type = blink::WebPushError::ErrorTypeAbort;
      break;
  }
  return blink::WebPushError(
      error_type,
      blink::WebString::fromUTF8(PushRegistrationStatusToString(status)));
}

static base::LazyInstance<base::ThreadLocalPointer<PushProvider>>::Leaky
    g_push_provider_tls = LAZY_INSTANCE_INITIALIZER;

PushProvider::PushProvider(ThreadSafeSender* thread_safe_sender,
                           PushDispatcher* push_dispatcher)
    : thread_safe_sender_(thread_safe_sender),
      push_dispatcher_(push_dispatcher) {
  g_push_provider_tls.Pointer()->Set(this);
}

PushProvider::~PushProvider() {
  g_push_provider_tls.Pointer()->Set(nullptr);
}

PushProvider* PushProvider::ThreadSpecificInstance(
    ThreadSafeSender* thread_safe_sender,
    PushDispatcher* push_dispatcher) {
  if (g_push_provider_tls.Pointer()->Get())
    return g_push_provider_tls.Pointer()->Get();

  PushProvider* provider =
      new PushProvider(thread_safe_sender, push_dispatcher);
  if (CurrentWorkerId())
    WorkerThread::AddObserver(provider);
  return provider;
}

void PushProvider::WillStopCurrentWorkerThread() {
  delete this;
}

void PushProvider::subscribe(
    blink::WebServiceWorkerRegistration* service_worker_registration,
    const blink::WebPushSubscriptionOptions& options,
    std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  int request_id = push_dispatcher_->GenerateRequestId(CurrentWorkerId());
  subscription_callbacks_.AddWithID(std::move(callbacks), request_id);
  int64_t service_worker_registration_id =
      GetServiceWorkerRegistrationId(service_worker_registration);
  PushSubscriptionOptions content_options;
  content_options.user_visible_only = options.userVisibleOnly;

  // Just treat the server key as a string of bytes and pass it to the push
  // service.
  content_options.sender_info = options.applicationServerKey.latin1();
  thread_safe_sender_->Send(new PushMessagingHostMsg_Subscribe(
      ChildProcessHost::kInvalidUniqueID, request_id,
      service_worker_registration_id, content_options));
}

void PushProvider::unsubscribe(
    blink::WebServiceWorkerRegistration* service_worker_registration,
    std::unique_ptr<blink::WebPushUnsubscribeCallbacks> callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);

  int request_id = push_dispatcher_->GenerateRequestId(CurrentWorkerId());
  unsubscribe_callbacks_.AddWithID(std::move(callbacks), request_id);
  int64_t service_worker_registration_id =
      GetServiceWorkerRegistrationId(service_worker_registration);
  thread_safe_sender_->Send(new PushMessagingHostMsg_Unsubscribe(
      request_id, service_worker_registration_id));
}

void PushProvider::getSubscription(
    blink::WebServiceWorkerRegistration* service_worker_registration,
    std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  int request_id = push_dispatcher_->GenerateRequestId(CurrentWorkerId());
  subscription_callbacks_.AddWithID(std::move(callbacks), request_id);
  int64_t service_worker_registration_id =
      GetServiceWorkerRegistrationId(service_worker_registration);
  thread_safe_sender_->Send(new PushMessagingHostMsg_GetSubscription(
      request_id, service_worker_registration_id));
}

void PushProvider::getPermissionStatus(
    blink::WebServiceWorkerRegistration* service_worker_registration,
    const blink::WebPushSubscriptionOptions& options,
    std::unique_ptr<blink::WebPushPermissionStatusCallbacks> callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  int request_id = push_dispatcher_->GenerateRequestId(CurrentWorkerId());
  permission_status_callbacks_.AddWithID(std::move(callbacks), request_id);
  int64_t service_worker_registration_id =
      GetServiceWorkerRegistrationId(service_worker_registration);
  thread_safe_sender_->Send(new PushMessagingHostMsg_GetPermissionStatus(
      request_id, service_worker_registration_id, options.userVisibleOnly));
}

bool PushProvider::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PushProvider, message)
    IPC_MESSAGE_HANDLER(PushMessagingMsg_SubscribeFromWorkerSuccess,
                        OnSubscribeFromWorkerSuccess);
    IPC_MESSAGE_HANDLER(PushMessagingMsg_SubscribeFromWorkerError,
                        OnSubscribeFromWorkerError);
    IPC_MESSAGE_HANDLER(PushMessagingMsg_UnsubscribeSuccess,
                        OnUnsubscribeSuccess);
    IPC_MESSAGE_HANDLER(PushMessagingMsg_UnsubscribeError, OnUnsubscribeError);
    IPC_MESSAGE_HANDLER(PushMessagingMsg_GetSubscriptionSuccess,
                        OnGetSubscriptionSuccess);
    IPC_MESSAGE_HANDLER(PushMessagingMsg_GetSubscriptionError,
                        OnGetSubscriptionError);
    IPC_MESSAGE_HANDLER(PushMessagingMsg_GetPermissionStatusSuccess,
                        OnGetPermissionStatusSuccess);
    IPC_MESSAGE_HANDLER(PushMessagingMsg_GetPermissionStatusError,
                        OnGetPermissionStatusError);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void PushProvider::OnSubscribeFromWorkerSuccess(
    int request_id,
    const GURL& endpoint,
    const PushSubscriptionOptions& options,
    const std::vector<uint8_t>& p256dh,
    const std::vector<uint8_t>& auth) {
  blink::WebPushSubscriptionCallbacks* callbacks =
      subscription_callbacks_.Lookup(request_id);
  if (!callbacks)
    return;

  callbacks->onSuccess(base::MakeUnique<blink::WebPushSubscription>(
      endpoint, options.user_visible_only,
      blink::WebString::fromLatin1(options.sender_info), p256dh, auth));

  subscription_callbacks_.Remove(request_id);
}

void PushProvider::OnSubscribeFromWorkerError(int request_id,
                                              PushRegistrationStatus status) {
  blink::WebPushSubscriptionCallbacks* callbacks =
      subscription_callbacks_.Lookup(request_id);
  if (!callbacks)
    return;

  callbacks->onError(PushRegistrationStatusToWebPushError(status));

  subscription_callbacks_.Remove(request_id);
}

void PushProvider::OnUnsubscribeSuccess(int request_id, bool did_unsubscribe) {
  blink::WebPushUnsubscribeCallbacks* callbacks =
      unsubscribe_callbacks_.Lookup(request_id);
  if (!callbacks)
    return;

  callbacks->onSuccess(did_unsubscribe);

  unsubscribe_callbacks_.Remove(request_id);
}

void PushProvider::OnUnsubscribeError(int request_id,
                                      blink::WebPushError::ErrorType error_type,
                                      const std::string& error_message) {
  blink::WebPushUnsubscribeCallbacks* callbacks =
      unsubscribe_callbacks_.Lookup(request_id);
  if (!callbacks)
    return;

  callbacks->onError(blink::WebPushError(
      error_type, blink::WebString::fromUTF8(error_message)));

  unsubscribe_callbacks_.Remove(request_id);
}

void PushProvider::OnGetSubscriptionSuccess(
    int request_id,
    const GURL& endpoint,
    const PushSubscriptionOptions& options,
    const std::vector<uint8_t>& p256dh,
    const std::vector<uint8_t>& auth) {
  blink::WebPushSubscriptionCallbacks* callbacks =
      subscription_callbacks_.Lookup(request_id);
  if (!callbacks)
    return;

  callbacks->onSuccess(base::MakeUnique<blink::WebPushSubscription>(
      endpoint, options.user_visible_only,
      blink::WebString::fromLatin1(options.sender_info), p256dh, auth));

  subscription_callbacks_.Remove(request_id);
}

void PushProvider::OnGetSubscriptionError(int request_id,
                                          PushGetRegistrationStatus status) {
  blink::WebPushSubscriptionCallbacks* callbacks =
      subscription_callbacks_.Lookup(request_id);
  if (!callbacks)
    return;

  // We are only expecting an error if we can't find a registration.
  callbacks->onSuccess(nullptr);

  subscription_callbacks_.Remove(request_id);
}

void PushProvider::OnGetPermissionStatusSuccess(
    int request_id,
    blink::WebPushPermissionStatus status) {
  blink::WebPushPermissionStatusCallbacks* callbacks =
      permission_status_callbacks_.Lookup(request_id);
  if (!callbacks)
    return;

  callbacks->onSuccess(status);

  permission_status_callbacks_.Remove(request_id);
}

void PushProvider::OnGetPermissionStatusError(
    int request_id,
    blink::WebPushError::ErrorType error) {
  blink::WebPushPermissionStatusCallbacks* callbacks =
      permission_status_callbacks_.Lookup(request_id);
  if (!callbacks)
    return;

  std::string error_message;
  if (error == blink::WebPushError::ErrorTypeNotSupported) {
    error_message =
        "Push subscriptions that don't enable userVisibleOnly are not "
        "supported.";
  }

  callbacks->onError(
      blink::WebPushError(error, blink::WebString::fromUTF8(error_message)));

  permission_status_callbacks_.Remove(request_id);
}

}  // namespace content
