// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/permission_bubble_media_access_handler.h"

#include "base/metrics/field_trial.h"
#include "chrome/browser/media/media_permission.h"
#include "chrome/browser/media/media_stream_device_permissions.h"
#include "chrome/browser/media/media_stream_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;

struct PermissionBubbleMediaAccessHandler::PendingAccessRequest {
  PendingAccessRequest(const content::MediaStreamRequest& request,
                       const content::MediaResponseCallback& callback)
      : request(request), callback(callback) {}
  ~PendingAccessRequest() {}

  // TODO(gbillock): make the MediaStreamDevicesController owned by
  // this object when we're using bubbles.
  content::MediaStreamRequest request;
  content::MediaResponseCallback callback;
};

PermissionBubbleMediaAccessHandler::PermissionBubbleMediaAccessHandler() {
  // PermissionBubbleMediaAccessHandler should be created on UI thread.
  // Otherwise, it will not receive
  // content::NOTIFICATION_WEB_CONTENTS_DESTROYED, and that will result in
  // possible use after free.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  notifications_registrar_.Add(this,
                               content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                               content::NotificationService::AllSources());
}

PermissionBubbleMediaAccessHandler::~PermissionBubbleMediaAccessHandler() {
}

bool PermissionBubbleMediaAccessHandler::SupportsStreamType(
    const content::MediaStreamType type,
    const extensions::Extension* extension) {
  return type == content::MEDIA_DEVICE_VIDEO_CAPTURE ||
         type == content::MEDIA_DEVICE_AUDIO_CAPTURE;
}

bool PermissionBubbleMediaAccessHandler::CheckMediaAccessPermission(
    content::WebContents* web_contents,
    const GURL& security_origin,
    content::MediaStreamType type,
    const extensions::Extension* extension) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  ContentSettingsType content_settings_type =
      type == content::MEDIA_DEVICE_AUDIO_CAPTURE
          ? CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC
          : CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA;
  MediaPermission permission(content_settings_type,
                             content::MEDIA_DEVICE_ACCESS, security_origin,
                             profile);
  content::MediaStreamRequestResult unused;
  if (permission.GetPermissionStatus(&unused) == CONTENT_SETTING_ALLOW)
    return true;

  return false;
}

void PermissionBubbleMediaAccessHandler::HandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RequestsQueue& queue = pending_requests_[web_contents];
  queue.push_back(PendingAccessRequest(request, callback));

  // If this is the only request then show the infobar.
  if (queue.size() == 1)
    ProcessQueuedAccessRequest(web_contents);
}

void PermissionBubbleMediaAccessHandler::ProcessQueuedAccessRequest(
    content::WebContents* web_contents) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::map<content::WebContents*, RequestsQueue>::iterator it =
      pending_requests_.find(web_contents);

  if (it == pending_requests_.end() || it->second.empty()) {
    // Don't do anything if the tab was closed.
    return;
  }

  DCHECK(!it->second.empty());

  if (PermissionBubbleManager::Enabled()) {
    scoped_ptr<MediaStreamDevicesController> controller(
        new MediaStreamDevicesController(
            web_contents, it->second.front().request,
            base::Bind(
                &PermissionBubbleMediaAccessHandler::OnAccessRequestResponse,
                base::Unretained(this), web_contents)));
    if (!controller->IsAskingForAudio() && !controller->IsAskingForVideo())
      return;
    PermissionBubbleManager* bubble_manager =
        PermissionBubbleManager::FromWebContents(web_contents);
    if (bubble_manager)
      bubble_manager->AddRequest(controller.release());
    return;
  }

  // TODO(gbillock): delete this block and the MediaStreamInfoBarDelegate
  // when we've transitioned to bubbles. (crbug/337458)
  MediaStreamInfoBarDelegate::Create(
      web_contents, it->second.front().request,
      base::Bind(&PermissionBubbleMediaAccessHandler::OnAccessRequestResponse,
                 base::Unretained(this), web_contents));
}

void PermissionBubbleMediaAccessHandler::UpdateMediaRequestState(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    content::MediaStreamType stream_type,
    content::MediaRequestState state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (state != content::MEDIA_REQUEST_STATE_CLOSING)
    return;

  bool found = false;
  for (RequestsQueues::iterator rqs_it = pending_requests_.begin();
       rqs_it != pending_requests_.end(); ++rqs_it) {
    RequestsQueue& queue = rqs_it->second;
    for (RequestsQueue::iterator it = queue.begin(); it != queue.end(); ++it) {
      if (it->request.render_process_id == render_process_id &&
          it->request.render_frame_id == render_frame_id &&
          it->request.page_request_id == page_request_id) {
        queue.erase(it);
        found = true;
        break;
      }
    }
    if (found)
      break;
  }
}

void PermissionBubbleMediaAccessHandler::OnAccessRequestResponse(
    content::WebContents* web_contents,
    const content::MediaStreamDevices& devices,
    content::MediaStreamRequestResult result,
    scoped_ptr<content::MediaStreamUI> ui) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::map<content::WebContents*, RequestsQueue>::iterator it =
      pending_requests_.find(web_contents);
  if (it == pending_requests_.end()) {
    // WebContents has been destroyed. Don't need to do anything.
    return;
  }

  RequestsQueue& queue(it->second);
  if (queue.empty())
    return;

  content::MediaResponseCallback callback = queue.front().callback;
  queue.pop_front();

  if (!queue.empty()) {
    // Post a task to process next queued request. It has to be done
    // asynchronously to make sure that calling infobar is not destroyed until
    // after this function returns.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &PermissionBubbleMediaAccessHandler::ProcessQueuedAccessRequest,
            base::Unretained(this), web_contents));
  }

  callback.Run(devices, result, ui.Pass());
}

void PermissionBubbleMediaAccessHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (type == content::NOTIFICATION_WEB_CONTENTS_DESTROYED) {
    content::WebContents* web_contents =
        content::Source<content::WebContents>(source).ptr();
    pending_requests_.erase(web_contents);
  }
}
