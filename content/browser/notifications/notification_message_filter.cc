// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_message_filter.h"

#include "base/callback.h"
#include "content/browser/notifications/page_notification_delegate.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/common/platform_notification_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/desktop_notification_delegate.h"
#include "content/public/browser/platform_notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"

namespace content {

NotificationMessageFilter::NotificationMessageFilter(
    int process_id,
    PlatformNotificationContextImpl* notification_context,
    ResourceContext* resource_context,
    BrowserContext* browser_context)
    : BrowserMessageFilter(PlatformNotificationMsgStart),
      process_id_(process_id),
      notification_context_(notification_context),
      resource_context_(resource_context),
      browser_context_(browser_context) {}

NotificationMessageFilter::~NotificationMessageFilter() {}

void NotificationMessageFilter::DidCloseNotification(int notification_id) {
  close_closures_.erase(notification_id);
}

bool NotificationMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NotificationMessageFilter, message)
    IPC_MESSAGE_HANDLER(PlatformNotificationHostMsg_CheckPermission,
                        OnCheckNotificationPermission)
    IPC_MESSAGE_HANDLER(PlatformNotificationHostMsg_Show,
                        OnShowPlatformNotification)
    IPC_MESSAGE_HANDLER(PlatformNotificationHostMsg_ShowPersistent,
                        OnShowPersistentNotification)
    IPC_MESSAGE_HANDLER(PlatformNotificationHostMsg_GetNotifications,
                        OnGetNotifications)
    IPC_MESSAGE_HANDLER(PlatformNotificationHostMsg_Close,
                        OnClosePlatformNotification)
    IPC_MESSAGE_HANDLER(PlatformNotificationHostMsg_ClosePersistent,
                        OnClosePersistentNotification)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void NotificationMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, content::BrowserThread::ID* thread) {
  if (message.type() == PlatformNotificationHostMsg_Show::ID ||
      message.type() == PlatformNotificationHostMsg_ShowPersistent::ID ||
      message.type() == PlatformNotificationHostMsg_Close::ID ||
      message.type() == PlatformNotificationHostMsg_ClosePersistent::ID)
    *thread = BrowserThread::UI;
}

void NotificationMessageFilter::OnCheckNotificationPermission(
    const GURL& origin, blink::WebNotificationPermission* permission) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  PlatformNotificationService* service =
      GetContentClient()->browser()->GetPlatformNotificationService();
  if (service) {
    *permission = service->CheckPermissionOnIOThread(resource_context_,
                                                     origin,
                                                     process_id_);
  } else {
    *permission = blink::WebNotificationPermissionDenied;
  }
}

void NotificationMessageFilter::OnShowPlatformNotification(
    int notification_id,
    const GURL& origin,
    const SkBitmap& icon,
    const PlatformNotificationData& notification_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!RenderProcessHost::FromID(process_id_))
    return;

  scoped_ptr<DesktopNotificationDelegate> delegate(
      new PageNotificationDelegate(process_id_, notification_id));

  PlatformNotificationService* service =
      GetContentClient()->browser()->GetPlatformNotificationService();
  DCHECK(service);

  if (!VerifyNotificationPermissionGranted(service, origin))
    return;

  base::Closure close_closure;
  service->DisplayNotification(browser_context_,
                               origin,
                               icon,
                               notification_data,
                               delegate.Pass(),
                               &close_closure);

  if (!close_closure.is_null())
    close_closures_[notification_id] = close_closure;
}

void NotificationMessageFilter::OnShowPersistentNotification(
    int request_id,
    int64 service_worker_registration_id,
    const GURL& origin,
    const SkBitmap& icon,
    const PlatformNotificationData& notification_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!RenderProcessHost::FromID(process_id_))
    return;

  PlatformNotificationService* service =
      GetContentClient()->browser()->GetPlatformNotificationService();
  DCHECK(service);

  if (!VerifyNotificationPermissionGranted(service, origin))
    return;

  service->DisplayPersistentNotification(browser_context_,
                                         service_worker_registration_id, origin,
                                         icon, notification_data);

  // TODO(peter): Confirm display of the persistent notification after the
  // data has been stored using the |notification_context_|.
  Send(new PlatformNotificationMsg_DidShowPersistent(request_id,
                                                     true /* success */));
}

void NotificationMessageFilter::OnGetNotifications(
    int request_id,
    int64_t service_worker_registration_id,
    const GURL& origin,
    const std::string& filter_tag) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(peter): Implement retrieval of persistent Web Notifications from the
  // database. Reply with an empty vector until this has been implemented.
  // Tracked in https://crbug.com/442143.

  Send(new PlatformNotificationMsg_DidGetNotifications(
      request_id,
      std::vector<PersistentNotificationInfo>()));
}

void NotificationMessageFilter::OnClosePlatformNotification(
    int notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!RenderProcessHost::FromID(process_id_))
    return;

  if (!close_closures_.count(notification_id))
    return;

  close_closures_[notification_id].Run();
  close_closures_.erase(notification_id);
}

void NotificationMessageFilter::OnClosePersistentNotification(
    const GURL& origin,
    const std::string& persistent_notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!RenderProcessHost::FromID(process_id_))
    return;

  PlatformNotificationService* service =
      GetContentClient()->browser()->GetPlatformNotificationService();
  DCHECK(service);

  // TODO(peter): Use |service_worker_registration_id| and |origin| when feeding
  // the close event through the notification database.

  service->ClosePersistentNotification(browser_context_,
                                       persistent_notification_id);
}

bool NotificationMessageFilter::VerifyNotificationPermissionGranted(
    PlatformNotificationService* service,
    const GURL& origin) {
  blink::WebNotificationPermission permission =
      service->CheckPermissionOnUIThread(browser_context_, origin, process_id_);
  if (permission == blink::WebNotificationPermissionAllowed)
    return true;

  BadMessageReceived();
  return false;
}

}  // namespace content
