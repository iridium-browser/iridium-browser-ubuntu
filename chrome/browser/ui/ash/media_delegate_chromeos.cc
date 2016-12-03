// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/media_delegate_chromeos.h"

#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/wm_shell.h"
#include "ash/content/shell_content_state.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/chromeos/extensions/media_player_api.h"
#include "chrome/browser/chromeos/extensions/media_player_event_router.h"
#include "chrome/browser/media/media_stream_capture_indicator.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/process_manager.h"

namespace {

void GetMediaCaptureState(
    const MediaStreamCaptureIndicator* indicator,
    content::WebContents* web_contents,
    int* media_state_out) {
  if (indicator->IsCapturingVideo(web_contents))
    *media_state_out |= ash::MEDIA_CAPTURE_VIDEO;
  if (indicator->IsCapturingAudio(web_contents))
    *media_state_out |= ash::MEDIA_CAPTURE_AUDIO;
}

void GetBrowserMediaCaptureState(
    const MediaStreamCaptureIndicator* indicator,
    const content::BrowserContext* context,
    int* media_state_out) {
  const BrowserList* desktop_list = BrowserList::GetInstance();

  for (BrowserList::BrowserVector::const_iterator iter = desktop_list->begin();
       iter != desktop_list->end();
       ++iter) {
    TabStripModel* tab_strip_model = (*iter)->tab_strip_model();
    for (int i = 0; i < tab_strip_model->count(); ++i) {
      content::WebContents* web_contents = tab_strip_model->GetWebContentsAt(i);
      if (web_contents->GetBrowserContext() != context)
        continue;
      GetMediaCaptureState(indicator, web_contents, media_state_out);
      if (*media_state_out == ash::MEDIA_CAPTURE_AUDIO_VIDEO)
        return;
    }
  }
}

void GetAppMediaCaptureState(
    const MediaStreamCaptureIndicator* indicator,
    content::BrowserContext* context,
    int* media_state_out) {
  const extensions::AppWindowRegistry::AppWindowList& apps =
      extensions::AppWindowRegistry::Get(context)->app_windows();
  for (extensions::AppWindowRegistry::AppWindowList::const_iterator iter =
           apps.begin();
       iter != apps.end();
       ++iter) {
    GetMediaCaptureState(indicator, (*iter)->web_contents(), media_state_out);
    if (*media_state_out == ash::MEDIA_CAPTURE_AUDIO_VIDEO)
      return;
  }
}

void GetExtensionMediaCaptureState(
    const MediaStreamCaptureIndicator* indicator,
    content::BrowserContext* context,
    int* media_state_out) {
  for (content::RenderFrameHost* host :
           extensions::ProcessManager::Get(context)->GetAllFrames()) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(host);
    // RFH may not have web contents.
    if (!web_contents)
      continue;
    GetMediaCaptureState(indicator, web_contents, media_state_out);
    if (*media_state_out == ash::MEDIA_CAPTURE_AUDIO_VIDEO)
      return;
  }
}

ash::MediaCaptureState GetMediaCaptureStateOfAllWebContents(
    content::BrowserContext* context) {
  if (!context)
    return ash::MEDIA_CAPTURE_NONE;

  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();

  int media_state = ash::MEDIA_CAPTURE_NONE;
  // Browser windows
  GetBrowserMediaCaptureState(indicator.get(), context, &media_state);
  if (media_state == ash::MEDIA_CAPTURE_AUDIO_VIDEO)
    return ash::MEDIA_CAPTURE_AUDIO_VIDEO;

  // App windows
  GetAppMediaCaptureState(indicator.get(), context, &media_state);
  if (media_state == ash::MEDIA_CAPTURE_AUDIO_VIDEO)
    return ash::MEDIA_CAPTURE_AUDIO_VIDEO;

  // Extensions
  GetExtensionMediaCaptureState(indicator.get(), context, &media_state);

  return static_cast<ash::MediaCaptureState>(media_state);
}

}  // namespace

MediaDelegateChromeOS::MediaDelegateChromeOS() : weak_ptr_factory_(this) {
  MediaCaptureDevicesDispatcher::GetInstance()->AddObserver(this);
}

MediaDelegateChromeOS::~MediaDelegateChromeOS() {
  MediaCaptureDevicesDispatcher::GetInstance()->RemoveObserver(this);
}

void MediaDelegateChromeOS::HandleMediaNextTrack() {
  extensions::MediaPlayerAPI::Get(ProfileManager::GetActiveUserProfile())
      ->media_player_event_router()
      ->NotifyNextTrack();
}

void MediaDelegateChromeOS::HandleMediaPlayPause() {
  extensions::MediaPlayerAPI::Get(ProfileManager::GetActiveUserProfile())
      ->media_player_event_router()
      ->NotifyTogglePlayState();
}

void MediaDelegateChromeOS::HandleMediaPrevTrack() {
  extensions::MediaPlayerAPI::Get(ProfileManager::GetActiveUserProfile())
      ->media_player_event_router()
      ->NotifyPrevTrack();
}

ash::MediaCaptureState MediaDelegateChromeOS::GetMediaCaptureState(
    ash::UserIndex index) {
  content::BrowserContext* context =
      ash::ShellContentState::GetInstance()->GetBrowserContextByIndex(index);
  return GetMediaCaptureStateOfAllWebContents(context);
}

void MediaDelegateChromeOS::OnRequestUpdate(
    int render_process_id,
    int render_frame_id,
    content::MediaStreamType stream_type,
    const content::MediaRequestState state) {
  base::MessageLoopForUI::current()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&MediaDelegateChromeOS::NotifyMediaCaptureChange,
                            weak_ptr_factory_.GetWeakPtr()));
}

void MediaDelegateChromeOS::NotifyMediaCaptureChange() {
  ash::WmShell::Get()->system_tray_notifier()->NotifyMediaCaptureChanged();
}
