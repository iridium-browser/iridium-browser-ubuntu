// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_util.h"

#include "build/build_config.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/permission_type.h"

using content::PermissionType;

std::size_t PermissionTypeHash::operator()(
    const content::PermissionType& type) const {
  return static_cast<size_t>(type);
}

// The returned strings must match the RAPPOR metrics in rappor.xml,
// and any Field Trial configs for the Permissions kill switch e.g.
// Permissions.Action.Geolocation etc..
std::string PermissionUtil::GetPermissionString(
    content::PermissionType permission) {
  switch (permission) {
    case content::PermissionType::GEOLOCATION:
      return "Geolocation";
    case content::PermissionType::NOTIFICATIONS:
      return "Notifications";
    case content::PermissionType::MIDI_SYSEX:
      return "MidiSysEx";
    case content::PermissionType::PUSH_MESSAGING:
      return "PushMessaging";
    case content::PermissionType::DURABLE_STORAGE:
      return "DurableStorage";
    case content::PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      return "ProtectedMediaIdentifier";
    case content::PermissionType::AUDIO_CAPTURE:
      return "AudioCapture";
    case content::PermissionType::VIDEO_CAPTURE:
      return "VideoCapture";
    case content::PermissionType::MIDI:
      return "Midi";
    case content::PermissionType::BACKGROUND_SYNC:
      return "BackgroundSync";
    case content::PermissionType::FLASH:
      return "Flash";
    case content::PermissionType::NUM:
      break;
  }
  NOTREACHED();
  return std::string();
}

PermissionRequestType PermissionUtil::GetRequestType(
    content::PermissionType type) {
  switch (type) {
    case content::PermissionType::GEOLOCATION:
      return PermissionRequestType::PERMISSION_GEOLOCATION;
    case content::PermissionType::NOTIFICATIONS:
      return PermissionRequestType::PERMISSION_NOTIFICATIONS;
    case content::PermissionType::MIDI_SYSEX:
      return PermissionRequestType::PERMISSION_MIDI_SYSEX;
    case content::PermissionType::PUSH_MESSAGING:
      return PermissionRequestType::PERMISSION_PUSH_MESSAGING;
    case content::PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      return PermissionRequestType::PERMISSION_PROTECTED_MEDIA_IDENTIFIER;
    case content::PermissionType::FLASH:
      return PermissionRequestType::PERMISSION_FLASH;
    default:
      NOTREACHED();
      return PermissionRequestType::UNKNOWN;
  }
}

PermissionRequestGestureType PermissionUtil::GetGestureType(bool user_gesture) {
  return user_gesture ? PermissionRequestGestureType::GESTURE
                      : PermissionRequestGestureType::NO_GESTURE;
}

bool PermissionUtil::GetPermissionType(ContentSettingsType type,
                                       PermissionType* out) {
  if (type == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
    *out = PermissionType::GEOLOCATION;
  } else if (type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    *out = PermissionType::NOTIFICATIONS;
  } else if (type == CONTENT_SETTINGS_TYPE_MIDI_SYSEX) {
    *out = PermissionType::MIDI_SYSEX;
  } else if (type == CONTENT_SETTINGS_TYPE_DURABLE_STORAGE) {
    *out = PermissionType::DURABLE_STORAGE;
  } else if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) {
    *out = PermissionType::VIDEO_CAPTURE;
  } else if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC) {
    *out = PermissionType::AUDIO_CAPTURE;
  } else if (type == CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC) {
    *out = PermissionType::BACKGROUND_SYNC;
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  } else if (type == CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER) {
    *out = PermissionType::PROTECTED_MEDIA_IDENTIFIER;
#endif
  } else {
    return false;
  }
  return true;
}

bool PermissionUtil::ShouldShowPersistenceToggle() {
  return base::FeatureList::IsEnabled(
      features::kDisplayPersistenceToggleInPermissionPrompts);
}

PermissionUtil::ScopedRevocationReporter::ScopedRevocationReporter(
    Profile* profile,
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    PermissionSourceUI source_ui)
    : profile_(profile),
      primary_url_(primary_url),
      secondary_url_(secondary_url),
      content_type_(content_type),
      source_ui_(source_ui) {
  if (!primary_url_.is_valid() ||
      (!secondary_url_.is_valid() && !secondary_url_.is_empty())) {
    is_initially_allowed_ = false;
    return;
  }
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  ContentSetting initial_content_setting = settings_map->GetContentSetting(
      primary_url_, secondary_url_, content_type_, std::string());
  is_initially_allowed_ = initial_content_setting == CONTENT_SETTING_ALLOW;
}

PermissionUtil::ScopedRevocationReporter::ScopedRevocationReporter(
    Profile* profile,
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    PermissionSourceUI source_ui)
    : ScopedRevocationReporter(
          profile,
          GURL(primary_pattern.ToString()),
          GURL((secondary_pattern == ContentSettingsPattern::Wildcard())
                   ? primary_pattern.ToString()
                   : secondary_pattern.ToString()),
          content_type,
          source_ui) {}

PermissionUtil::ScopedRevocationReporter::~ScopedRevocationReporter() {
  if (!is_initially_allowed_)
    return;
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  ContentSetting final_content_setting = settings_map->GetContentSetting(
      primary_url_, secondary_url_, content_type_, std::string());
  if (final_content_setting != CONTENT_SETTING_ALLOW) {
    PermissionType permission_type;
    if (PermissionUtil::GetPermissionType(content_type_, &permission_type)) {
      PermissionUmaUtil::PermissionRevoked(permission_type, source_ui_,
                                           primary_url_, profile_);
    }
  }
}
