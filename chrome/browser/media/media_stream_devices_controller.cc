// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_stream_devices_controller.h"

#include "base/metrics/histogram.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/media_stream_capture_indicator.h"
#include "chrome/browser/media/media_stream_device_permissions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/media_stream_request.h"
#include "extensions/common/constants.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace {

bool HasAvailableDevicesForRequest(const content::MediaStreamRequest& request) {
  const content::MediaStreamDevices* audio_devices =
      request.audio_type == content::MEDIA_DEVICE_AUDIO_CAPTURE ?
          &MediaCaptureDevicesDispatcher::GetInstance()
              ->GetAudioCaptureDevices() :
          NULL;

  const content::MediaStreamDevices* video_devices =
      request.video_type == content::MEDIA_DEVICE_VIDEO_CAPTURE ?
          &MediaCaptureDevicesDispatcher::GetInstance()
              ->GetVideoCaptureDevices() :
          NULL;

  // Check if we're being asked for audio and/or video and that either of those
  // lists is empty.  If they are, we do not have devices available for the
  // request.
  // TODO(tommi): It's kind of strange to have this here since if we fail this
  // test, there'll be a UI shown that indicates to the user that access to
  // non-existing audio/video devices has been denied.  The user won't have
  // any way to change that but there will be a UI shown which indicates that
  // access is blocked.
  if ((audio_devices != NULL && audio_devices->empty()) ||
      (video_devices != NULL && video_devices->empty())) {
    return false;
  }

  // Note: we check requested_[audio|video]_device_id before dereferencing
  // [audio|video]_devices.  If the requested device id is non-empty, then
  // the corresponding device list must not be NULL.

  if (!request.requested_audio_device_id.empty() &&
      !audio_devices->FindById(request.requested_audio_device_id)) {
    return false;
  }

  if (!request.requested_video_device_id.empty() &&
      !video_devices->FindById(request.requested_video_device_id)) {
    return false;
  }

  return true;
}

enum DevicePermissionActions {
  kAllowHttps = 0,
  kAllowHttp,
  kDeny,
  kCancel,
  kPermissionActionsMax  // Must always be last!
};

// This is a wrapper around the call to
// TabSpecificContentSettings::OnMediaStreamPermissionSet, precomputing the
// information from |request_permissions| to a form which is understood by
// TabSpecificContentSettings.
void OnMediaStreamPermissionSet(
    TabSpecificContentSettings* content_settings,
    content::WebContents* web_contents,
    const GURL& request_origin,
    const MediaStreamDevicesController::MediaStreamTypeSettingsMap&
        request_permissions) {
  TabSpecificContentSettings::MicrophoneCameraState microphone_camera_state =
      TabSpecificContentSettings::MICROPHONE_CAMERA_NOT_ACCESSED;
  std::string selected_audio_device;
  std::string selected_video_device;
  std::string requested_audio_device;
  std::string requested_video_device;

  PrefService* prefs = Profile::FromBrowserContext(
      web_contents->GetBrowserContext())->GetPrefs();
  auto it = request_permissions.find(content::MEDIA_DEVICE_AUDIO_CAPTURE);
  if (it != request_permissions.end()) {
    requested_audio_device = it->second.requested_device_id;
    selected_audio_device = requested_audio_device.empty() ?
            prefs->GetString(prefs::kDefaultAudioCaptureDevice) :
            requested_audio_device;
    DCHECK_NE(MediaStreamDevicesController::MEDIA_NONE, it->second.permission);
    bool mic_allowed =
        it->second.permission == MediaStreamDevicesController::MEDIA_ALLOWED;
    microphone_camera_state |=
        TabSpecificContentSettings::MICROPHONE_ACCESSED |
        (mic_allowed ? 0 : TabSpecificContentSettings::MICROPHONE_BLOCKED);
  }

  it = request_permissions.find(content::MEDIA_DEVICE_VIDEO_CAPTURE);
  if (it != request_permissions.end()) {
    requested_video_device = it->second.requested_device_id;
    selected_video_device = requested_video_device.empty() ?
            prefs->GetString(prefs::kDefaultVideoCaptureDevice) :
            requested_video_device;
    DCHECK_NE(MediaStreamDevicesController::MEDIA_NONE, it->second.permission);
    bool cam_allowed =
        it->second.permission == MediaStreamDevicesController::MEDIA_ALLOWED;
    microphone_camera_state |=
        TabSpecificContentSettings::CAMERA_ACCESSED |
        (cam_allowed ? 0 : TabSpecificContentSettings::CAMERA_BLOCKED);
  }

  content_settings->OnMediaStreamPermissionSet(request_origin,
                                               microphone_camera_state,
                                               selected_audio_device,
                                               selected_video_device,
                                               requested_audio_device,
                                               requested_video_device);
}

}  // namespace

MediaStreamDevicesController::MediaStreamTypeSettings::MediaStreamTypeSettings(
    Permission permission, const std::string& requested_device_id):
    permission(permission), requested_device_id(requested_device_id) {}

MediaStreamDevicesController::MediaStreamTypeSettings::
    MediaStreamTypeSettings(): permission(MEDIA_NONE) {}

MediaStreamDevicesController::MediaStreamTypeSettings::
    ~MediaStreamTypeSettings() {}

MediaStreamDevicesController::MediaStreamDevicesController(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback)
    : web_contents_(web_contents),
      request_(request),
      callback_(callback) {
  profile_ = Profile::FromBrowserContext(web_contents->GetBrowserContext());
  content_settings_ = TabSpecificContentSettings::FromWebContents(web_contents);

  // For MEDIA_OPEN_DEVICE requests (Pepper) we always request both webcam
  // and microphone to avoid popping two infobars.
  // We start with setting the requested media type to allowed or blocked
  // depending on the policy. If not blocked by policy it may be blocked later
  // in the two remaining filtering steps (by user setting or by user when
  // clicking the infobar).
  // TODO(grunell): It's not the nicest solution to let the MEDIA_OPEN_DEVICE
  // case take a ride on the MEDIA_DEVICE_*_CAPTURE permission. Should be fixed.
  if (request.audio_type == content::MEDIA_DEVICE_AUDIO_CAPTURE ||
      request.request_type == content::MEDIA_OPEN_DEVICE) {
    if (GetDevicePolicy(profile_,
                        request_.security_origin,
                        prefs::kAudioCaptureAllowed,
                        prefs::kAudioCaptureAllowedUrls) == ALWAYS_DENY) {
      request_permissions_.insert(std::make_pair(
          content::MEDIA_DEVICE_AUDIO_CAPTURE,
          MediaStreamTypeSettings(MEDIA_BLOCKED_BY_POLICY,
                                  request.requested_audio_device_id)));
    } else {
      request_permissions_.insert(std::make_pair(
          content::MEDIA_DEVICE_AUDIO_CAPTURE,
          MediaStreamTypeSettings(MEDIA_ALLOWED,
                                  request.requested_audio_device_id)));
    }
  }
  if (request.video_type == content::MEDIA_DEVICE_VIDEO_CAPTURE ||
      request.request_type == content::MEDIA_OPEN_DEVICE) {
    if (GetDevicePolicy(profile_,
                        request_.security_origin,
                        prefs::kVideoCaptureAllowed,
                        prefs::kVideoCaptureAllowedUrls) == ALWAYS_DENY) {
      request_permissions_.insert(std::make_pair(
          content::MEDIA_DEVICE_VIDEO_CAPTURE,
          MediaStreamTypeSettings(MEDIA_BLOCKED_BY_POLICY,
                                  request.requested_video_device_id)));
    } else {
      request_permissions_.insert(std::make_pair(
          content::MEDIA_DEVICE_VIDEO_CAPTURE,
          MediaStreamTypeSettings(MEDIA_ALLOWED,
                                  request.requested_video_device_id)));
    }
  }
}

MediaStreamDevicesController::~MediaStreamDevicesController() {
  if (!callback_.is_null()) {
    callback_.Run(content::MediaStreamDevices(),
                  content::MEDIA_DEVICE_FAILED_DUE_TO_SHUTDOWN,
                  scoped_ptr<content::MediaStreamUI>());
  }
}

// static
void MediaStreamDevicesController::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* prefs) {
  prefs->RegisterBooleanPref(prefs::kVideoCaptureAllowed, true);
  prefs->RegisterBooleanPref(prefs::kAudioCaptureAllowed, true);
  prefs->RegisterListPref(prefs::kVideoCaptureAllowedUrls);
  prefs->RegisterListPref(prefs::kAudioCaptureAllowedUrls);
}

// TODO(gbillock): rename? doesn't actually dismiss. More of a 'check profile
// and system for compatibility' thing.
bool MediaStreamDevicesController::DismissInfoBarAndTakeActionOnSettings() {
  // Tab capture is allowed for extensions only and infobar is not shown for
  // extensions.
  if (request_.audio_type == content::MEDIA_TAB_AUDIO_CAPTURE ||
      request_.video_type == content::MEDIA_TAB_VIDEO_CAPTURE) {
    Deny(false, content::MEDIA_DEVICE_INVALID_STATE);
    return true;
  }

  // Deny the request if the security origin is empty, this happens with
  // file access without |--allow-file-access-from-files| flag.
  if (request_.security_origin.is_empty()) {
    Deny(false, content::MEDIA_DEVICE_INVALID_SECURITY_ORIGIN);
    return true;
  }

  // Deny the request if there is no device attached to the OS of the
  // requested type. If both audio and video is requested, both types must be
  // available.
  if (!HasAvailableDevicesForRequest(request_)) {
    Deny(false, content::MEDIA_DEVICE_NO_HARDWARE);
    return true;
  }

  // Check if any allow exception has been made for this request.
  if (IsRequestAllowedByDefault()) {
    Accept(false);
    return true;
  }

  // Filter any parts of the request that have been blocked by default and deny
  // it if nothing is left to accept.
  if (FilterBlockedByDefaultDevices() == 0) {
    Deny(false, content::MEDIA_DEVICE_PERMISSION_DENIED);
    return true;
  }

  // Check if the media default setting is set to block.
  if (IsDefaultMediaAccessBlocked()) {
    Deny(false, content::MEDIA_DEVICE_PERMISSION_DENIED);
    return true;
  }

  // Show the infobar.
  return false;
}

bool MediaStreamDevicesController::HasAudio() const {
  return IsDeviceAudioCaptureRequestedAndAllowed();
}

bool MediaStreamDevicesController::HasVideo() const {
  return IsDeviceVideoCaptureRequestedAndAllowed();
}

const std::string& MediaStreamDevicesController::GetSecurityOriginSpec() const {
  return request_.security_origin.spec();
}

void MediaStreamDevicesController::Accept(bool update_content_setting) {
  NotifyUIRequestAccepted();

  // Get the default devices for the request.
  content::MediaStreamDevices devices;
  bool audio_allowed = IsDeviceAudioCaptureRequestedAndAllowed();
  bool video_allowed = IsDeviceVideoCaptureRequestedAndAllowed();
  if (audio_allowed || video_allowed) {
    switch (request_.request_type) {
      case content::MEDIA_OPEN_DEVICE: {
        const content::MediaStreamDevice* device = NULL;
        // For open device request, when requested device_id is empty, pick
        // the first available of the given type. If requested device_id is
        // not empty, return the desired device if it's available. Otherwise,
        // return no device.
        if (audio_allowed &&
            request_.audio_type == content::MEDIA_DEVICE_AUDIO_CAPTURE) {
          if (!request_.requested_audio_device_id.empty()) {
            device = MediaCaptureDevicesDispatcher::GetInstance()->
                GetRequestedAudioDevice(request_.requested_audio_device_id);
          } else {
            device = MediaCaptureDevicesDispatcher::GetInstance()->
                GetFirstAvailableAudioDevice();
          }
        } else if (video_allowed &&
            request_.video_type == content::MEDIA_DEVICE_VIDEO_CAPTURE) {
          // Pepper API opens only one device at a time.
          if (!request_.requested_video_device_id.empty()) {
            device = MediaCaptureDevicesDispatcher::GetInstance()->
                GetRequestedVideoDevice(request_.requested_video_device_id);
          } else {
            device = MediaCaptureDevicesDispatcher::GetInstance()->
                GetFirstAvailableVideoDevice();
          }
        }
        if (device)
          devices.push_back(*device);
        break;
      }
      case content::MEDIA_GENERATE_STREAM: {
        bool get_default_audio_device = audio_allowed;
        bool get_default_video_device = video_allowed;

        // Get the exact audio or video device if an id is specified.
        if (audio_allowed && !request_.requested_audio_device_id.empty()) {
          const content::MediaStreamDevice* audio_device =
              MediaCaptureDevicesDispatcher::GetInstance()->
                  GetRequestedAudioDevice(request_.requested_audio_device_id);
          if (audio_device) {
            devices.push_back(*audio_device);
            get_default_audio_device = false;
          }
        }
        if (video_allowed && !request_.requested_video_device_id.empty()) {
          const content::MediaStreamDevice* video_device =
              MediaCaptureDevicesDispatcher::GetInstance()->
                  GetRequestedVideoDevice(request_.requested_video_device_id);
          if (video_device) {
            devices.push_back(*video_device);
            get_default_video_device = false;
          }
        }

        // If either or both audio and video devices were requested but not
        // specified by id, get the default devices.
        if (get_default_audio_device || get_default_video_device) {
          MediaCaptureDevicesDispatcher::GetInstance()->
              GetDefaultDevicesForProfile(profile_,
                                          get_default_audio_device,
                                          get_default_video_device,
                                          &devices);
        }
        break;
      }
      case content::MEDIA_DEVICE_ACCESS: {
        // Get the default devices for the request.
        MediaCaptureDevicesDispatcher::GetInstance()->
            GetDefaultDevicesForProfile(profile_,
                                        audio_allowed,
                                        video_allowed,
                                        &devices);
        break;
      }
      case content::MEDIA_ENUMERATE_DEVICES: {
        // Do nothing.
        NOTREACHED();
        break;
      }
    }  // switch

    // TODO(raymes): We currently set the content permission for non-https
    // websites for Pepper requests as well. This is temporary and should be
    // removed.
    if (update_content_setting) {
      if ((IsSchemeSecure() && !devices.empty()) ||
          request_.request_type == content::MEDIA_OPEN_DEVICE) {
        StorePermission(true);
      }
    }

    if (audio_allowed) {
      profile_->GetHostContentSettingsMap()->UpdateLastUsageByPattern(
          ContentSettingsPattern::FromURLNoWildcard(request_.security_origin),
          ContentSettingsPattern::Wildcard(),
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
    }
    if (video_allowed) {
      profile_->GetHostContentSettingsMap()->UpdateLastUsageByPattern(
          ContentSettingsPattern::FromURLNoWildcard(request_.security_origin),
          ContentSettingsPattern::Wildcard(),
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
    }
  }

  scoped_ptr<content::MediaStreamUI> ui;
  if (!devices.empty()) {
    ui = MediaCaptureDevicesDispatcher::GetInstance()->
        GetMediaStreamCaptureIndicator()->RegisterMediaStream(
            web_contents_, devices);
  }
  content::MediaResponseCallback cb = callback_;
  callback_.Reset();
  cb.Run(devices,
         devices.empty() ?
             content::MEDIA_DEVICE_NO_HARDWARE : content::MEDIA_DEVICE_OK,
         ui.Pass());
}

void MediaStreamDevicesController::Deny(
    bool update_content_setting,
    content::MediaStreamRequestResult result) {
  DLOG(WARNING) << "MediaStreamDevicesController::Deny: " << result;
  NotifyUIRequestDenied();

  if (update_content_setting) {
    // Store sticky permissions if |update_content_setting|.
    CHECK_EQ(content::MEDIA_DEVICE_PERMISSION_DENIED, result);
    StorePermission(false);
  }

  content::MediaResponseCallback cb = callback_;
  callback_.Reset();
  cb.Run(content::MediaStreamDevices(),
         result,
         scoped_ptr<content::MediaStreamUI>());
}

int MediaStreamDevicesController::GetIconID() const {
  if (HasVideo())
    return IDR_INFOBAR_MEDIA_STREAM_CAMERA;

  return IDR_INFOBAR_MEDIA_STREAM_MIC;
}

base::string16 MediaStreamDevicesController::GetMessageText() const {
  int message_id = IDS_MEDIA_CAPTURE_AUDIO_AND_VIDEO;
  if (!HasAudio())
    message_id = IDS_MEDIA_CAPTURE_VIDEO_ONLY;
  else if (!HasVideo())
    message_id = IDS_MEDIA_CAPTURE_AUDIO_ONLY;
  return l10n_util::GetStringFUTF16(
      message_id, base::UTF8ToUTF16(GetSecurityOriginSpec()));
}

base::string16 MediaStreamDevicesController::GetMessageTextFragment() const {
  int message_id = IDS_MEDIA_CAPTURE_AUDIO_AND_VIDEO_PERMISSION_FRAGMENT;
  if (!HasAudio())
    message_id = IDS_MEDIA_CAPTURE_VIDEO_ONLY_PERMISSION_FRAGMENT;
  else if (!HasVideo())
    message_id = IDS_MEDIA_CAPTURE_AUDIO_ONLY_PERMISSION_FRAGMENT;
  return l10n_util::GetStringUTF16(message_id);
}

bool MediaStreamDevicesController::HasUserGesture() const {
  return request_.user_gesture;
}

GURL MediaStreamDevicesController::GetRequestingHostname() const {
  return request_.security_origin;
}

void MediaStreamDevicesController::PermissionGranted() {
  GURL origin(GetSecurityOriginSpec());
  if (origin.SchemeIsSecure()) {
    UMA_HISTOGRAM_ENUMERATION("Media.DevicePermissionActions",
                              kAllowHttps, kPermissionActionsMax);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Media.DevicePermissionActions",
                              kAllowHttp, kPermissionActionsMax);
  }
  Accept(true);
}

void MediaStreamDevicesController::PermissionDenied() {
  UMA_HISTOGRAM_ENUMERATION("Media.DevicePermissionActions",
                            kDeny, kPermissionActionsMax);
  Deny(true, content::MEDIA_DEVICE_PERMISSION_DENIED);
}

void MediaStreamDevicesController::Cancelled() {
  UMA_HISTOGRAM_ENUMERATION("Media.DevicePermissionActions",
                            kCancel, kPermissionActionsMax);
  Deny(false, content::MEDIA_DEVICE_PERMISSION_DISMISSED);
}

void MediaStreamDevicesController::RequestFinished() {
  delete this;
}

bool MediaStreamDevicesController::IsRequestAllowedByDefault() const {
  // The request from internal objects like chrome://URLs is always allowed.
  if (CheckAllowAllMediaStreamContentForOrigin(
          profile_, request_.security_origin,
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC) &&
      CheckAllowAllMediaStreamContentForOrigin(
          profile_, request_.security_origin,
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA)) {
    return true;
  }

  struct {
    bool has_capability;
    const char* policy_name;
    const char* list_policy_name;
    ContentSettingsType settings_type;
  } device_checks[] = {
    { IsDeviceAudioCaptureRequestedAndAllowed(), prefs::kAudioCaptureAllowed,
      prefs::kAudioCaptureAllowedUrls, CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC },
    { IsDeviceVideoCaptureRequestedAndAllowed(), prefs::kVideoCaptureAllowed,
      prefs::kVideoCaptureAllowedUrls,
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA },
  };

  for (size_t i = 0; i < arraysize(device_checks); ++i) {
    if (!device_checks[i].has_capability)
      continue;

    MediaStreamDevicePolicy policy =
        GetDevicePolicy(profile_,
                        request_.security_origin,
                        device_checks[i].policy_name,
                        device_checks[i].list_policy_name);

    if (policy == ALWAYS_DENY)
      return false;

    if (policy == POLICY_NOT_SET) {
      // Only load content settings from secure origins unless it is a
      // content::MEDIA_OPEN_DEVICE (Pepper) request.
      if (!IsSchemeSecure() &&
          request_.request_type != content::MEDIA_OPEN_DEVICE) {
        return false;
      }
      if (profile_->GetHostContentSettingsMap()->GetContentSetting(
              request_.security_origin,
              request_.security_origin,
              device_checks[i].settings_type,
              content_settings::ResourceIdentifier()) !=
          CONTENT_SETTING_ALLOW) {
        return false;
      }
    }
    // If we get here, then either policy is set to ALWAYS_ALLOW or the content
    // settings allow the request by default.
  }

  return true;
}

int MediaStreamDevicesController::FilterBlockedByDefaultDevices() {
  int requested_devices = 0;

  if (IsDeviceAudioCaptureRequestedAndAllowed()) {
    if (profile_->GetHostContentSettingsMap()->GetContentSetting(
            request_.security_origin,
            request_.security_origin,
            CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
            content_settings::ResourceIdentifier()) ==
        CONTENT_SETTING_BLOCK) {
      request_permissions_[content::MEDIA_DEVICE_AUDIO_CAPTURE].permission =
          MEDIA_BLOCKED_BY_USER_SETTING;
    } else {
      ++requested_devices;
    }
  }

  if (IsDeviceVideoCaptureRequestedAndAllowed()) {
    if (profile_->GetHostContentSettingsMap()->GetContentSetting(
            request_.security_origin,
            request_.security_origin,
            CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
            content_settings::ResourceIdentifier()) ==
        CONTENT_SETTING_BLOCK) {
      request_permissions_[content::MEDIA_DEVICE_VIDEO_CAPTURE].permission =
          MEDIA_BLOCKED_BY_USER_SETTING;
    } else {
      ++requested_devices;
    }
  }

  return requested_devices;
}

bool MediaStreamDevicesController::IsDefaultMediaAccessBlocked() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return (profile_->GetHostContentSettingsMap()->GetDefaultContentSetting(
              CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, NULL)
          == CONTENT_SETTING_BLOCK &&
          profile_->GetHostContentSettingsMap()->GetDefaultContentSetting(
              CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, NULL)
          == CONTENT_SETTING_BLOCK);
}

bool MediaStreamDevicesController::IsSchemeSecure() const {
  return request_.security_origin.SchemeIsSecure() ||
      request_.security_origin.SchemeIs(extensions::kExtensionScheme);
}

void MediaStreamDevicesController::StorePermission(bool allowed) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromURLNoWildcard(request_.security_origin);
  // Check the pattern is valid or not. When the request is from a file access,
  // no exception will be made.
  if (!primary_pattern.IsValid())
    return;

  ContentSetting content_setting = allowed ?
      CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
  if (request_permissions_.find(content::MEDIA_DEVICE_AUDIO_CAPTURE) !=
      request_permissions_.end()) {
    profile_->GetHostContentSettingsMap()->SetContentSetting(
        primary_pattern,
        ContentSettingsPattern::Wildcard(),
        CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
        std::string(),
        content_setting);
  }
  if (request_permissions_.find(content::MEDIA_DEVICE_VIDEO_CAPTURE) !=
      request_permissions_.end()) {
    profile_->GetHostContentSettingsMap()->SetContentSetting(
        primary_pattern,
        ContentSettingsPattern::Wildcard(),
        CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
        std::string(),
        content_setting);
  }
}

void MediaStreamDevicesController::NotifyUIRequestAccepted() const {
  if (!content_settings_)
    return;

  OnMediaStreamPermissionSet(content_settings_,
                             web_contents_,
                             request_.security_origin,
                             request_permissions_);
}

void MediaStreamDevicesController::NotifyUIRequestDenied() {
  if (!content_settings_)
    return;

  if (IsDeviceAudioCaptureRequestedAndAllowed()) {
    request_permissions_[content::MEDIA_DEVICE_AUDIO_CAPTURE].permission =
        MEDIA_BLOCKED_BY_USER;
  }
  if (IsDeviceVideoCaptureRequestedAndAllowed()) {
    request_permissions_[content::MEDIA_DEVICE_VIDEO_CAPTURE].permission =
        MEDIA_BLOCKED_BY_USER;
  }

  OnMediaStreamPermissionSet(content_settings_,
                             web_contents_,
                             request_.security_origin,
                             request_permissions_);
}

bool MediaStreamDevicesController::IsDeviceAudioCaptureRequestedAndAllowed()
    const {
  MediaStreamTypeSettingsMap::const_iterator it =
      request_permissions_.find(content::MEDIA_DEVICE_AUDIO_CAPTURE);
  return (it != request_permissions_.end() && IsCaptureDeviceRequestAllowed() &&
          it->second.permission == MEDIA_ALLOWED);
}

bool MediaStreamDevicesController::IsDeviceVideoCaptureRequestedAndAllowed()
    const {
  MediaStreamTypeSettingsMap::const_iterator it =
      request_permissions_.find(content::MEDIA_DEVICE_VIDEO_CAPTURE);
  return (it != request_permissions_.end() && IsCaptureDeviceRequestAllowed() &&
          it->second.permission == MEDIA_ALLOWED);
}

bool MediaStreamDevicesController::IsCaptureDeviceRequestAllowed() const {
#if defined(OS_ANDROID)
  // Don't approve device requests if the tab was hidden.
  // TODO(qinmin): Add a test for this. http://crbug.com/396869.
  return web_contents_->GetRenderWidgetHostView()->IsShowing();
#endif
  return true;
}
