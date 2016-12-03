// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/extension_media_access_handler.h"

#include <utility>

#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/media_stream_capture_indicator.h"
#include "chrome/browser/media/media_stream_device_permissions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"

namespace {

// This is a short-term solution to grant camera and/or microphone access to
// extensions:
// 1. Virtual keyboard extension.
// 2. Flutter gesture recognition extension.
// 3. TODO(smus): Airbender experiment 1.
// 4. TODO(smus): Airbender experiment 2.
// 5. Hotwording component extension.
// 6. XKB input method component extension.
// 7. M17n/T13n/CJK input method component extension.
// Once http://crbug.com/292856 is fixed, remove this whitelist.
bool IsMediaRequestWhitelistedForExtension(
    const extensions::Extension* extension) {
  return extension->id() == "mppnpdlheglhdfmldimlhpnegondlapf" ||
         extension->id() == "jokbpnebhdcladagohdnfgjcpejggllo" ||
         extension->id() == "clffjmdilanldobdnedchkdbofoimcgb" ||
         extension->id() == "nnckehldicaciogcbchegobnafnjkcne" ||
         extension->id() == "nbpagnldghgfoolbancepceaanlmhfmd" ||
         extension->id() == "jkghodnilhceideoidjikpgommlajknk" ||
         extension->id() == "gjaehgfemfahhmlgpdfknkhdnemmolop";
}

}  // namespace

ExtensionMediaAccessHandler::ExtensionMediaAccessHandler() {
}

ExtensionMediaAccessHandler::~ExtensionMediaAccessHandler() {
}

bool ExtensionMediaAccessHandler::SupportsStreamType(
    const content::MediaStreamType type,
    const extensions::Extension* extension) {
  return extension && (extension->is_platform_app() ||
                       IsMediaRequestWhitelistedForExtension(extension)) &&
         (type == content::MEDIA_DEVICE_AUDIO_CAPTURE ||
          type == content::MEDIA_DEVICE_VIDEO_CAPTURE);
}

bool ExtensionMediaAccessHandler::CheckMediaAccessPermission(
    content::WebContents* web_contents,
    const GURL& security_origin,
    content::MediaStreamType type,
    const extensions::Extension* extension) {
  return extension->permissions_data()->HasAPIPermission(
      type == content::MEDIA_DEVICE_AUDIO_CAPTURE
          ? extensions::APIPermission::kAudioCapture
          : extensions::APIPermission::kVideoCapture);
}

void ExtensionMediaAccessHandler::HandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension) {
  // TODO(vrk): This code is largely duplicated in
  // MediaStreamDevicesController::Accept(). Move this code into a shared method
  // between the two classes.

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  bool audio_allowed =
      request.audio_type == content::MEDIA_DEVICE_AUDIO_CAPTURE &&
      extension->permissions_data()->HasAPIPermission(
          extensions::APIPermission::kAudioCapture) &&
      GetDevicePolicy(profile, extension->url(), prefs::kAudioCaptureAllowed,
                      prefs::kAudioCaptureAllowedUrls) != ALWAYS_DENY;
  bool video_allowed =
      request.video_type == content::MEDIA_DEVICE_VIDEO_CAPTURE &&
      extension->permissions_data()->HasAPIPermission(
          extensions::APIPermission::kVideoCapture) &&
      GetDevicePolicy(profile, extension->url(), prefs::kVideoCaptureAllowed,
                      prefs::kVideoCaptureAllowedUrls) != ALWAYS_DENY;

  bool get_default_audio_device = audio_allowed;
  bool get_default_video_device = video_allowed;

  content::MediaStreamDevices devices;

  // Set an initial error result. If neither audio or video is allowed, we'll
  // never try to get any device below but will just create |ui| and return an
  // empty list with "invalid state" result. If at least one is allowed, we'll
  // try to get device(s), and if failure, we want to return "no hardware"
  // result.
  // TODO(grunell): The invalid state result should be changed to a new denied
  // result + a dcheck to ensure at least one of audio or video types is
  // capture.
  content::MediaStreamRequestResult result =
      (audio_allowed || video_allowed) ? content::MEDIA_DEVICE_NO_HARDWARE
                                       : content::MEDIA_DEVICE_INVALID_STATE;

  // Get the exact audio or video device if an id is specified.
  // We only set any error result here and before running the callback change
  // it to OK if we have any device.
  if (audio_allowed && !request.requested_audio_device_id.empty()) {
    const content::MediaStreamDevice* audio_device =
        MediaCaptureDevicesDispatcher::GetInstance()->GetRequestedAudioDevice(
            request.requested_audio_device_id);
    if (audio_device) {
      devices.push_back(*audio_device);
      get_default_audio_device = false;
    }
  }
  if (video_allowed && !request.requested_video_device_id.empty()) {
    const content::MediaStreamDevice* video_device =
        MediaCaptureDevicesDispatcher::GetInstance()->GetRequestedVideoDevice(
            request.requested_video_device_id);
    if (video_device) {
      devices.push_back(*video_device);
      get_default_video_device = false;
    }
  }

  // If either or both audio and video devices were requested but not
  // specified by id, get the default devices.
  if (get_default_audio_device || get_default_video_device) {
    MediaCaptureDevicesDispatcher::GetInstance()->GetDefaultDevicesForProfile(
        profile, get_default_audio_device, get_default_video_device, &devices);
  }

  std::unique_ptr<content::MediaStreamUI> ui;
  if (!devices.empty()) {
    result = content::MEDIA_DEVICE_OK;
    ui = MediaCaptureDevicesDispatcher::GetInstance()
             ->GetMediaStreamCaptureIndicator()
             ->RegisterMediaStream(web_contents, devices);
  }

  callback.Run(devices, result, std::move(ui));
}
