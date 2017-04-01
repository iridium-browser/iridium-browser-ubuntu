// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_request_impl.h"

#include "build/build_config.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icons_public.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/android_theme_resources.h"
#endif

PermissionRequestImpl::PermissionRequestImpl(
    const GURL& request_origin,
    content::PermissionType permission_type,
    Profile* profile,
    bool has_gesture,
    const PermissionDecidedCallback& permission_decided_callback,
    const base::Closure delete_callback)
    : request_origin_(request_origin),
      permission_type_(permission_type),
      profile_(profile),
      has_gesture_(has_gesture),
      permission_decided_callback_(permission_decided_callback),
      delete_callback_(delete_callback),
      is_finished_(false),
      action_taken_(false) {}

PermissionRequestImpl::~PermissionRequestImpl() {
  DCHECK(is_finished_);
  if (!action_taken_) {
    PermissionUmaUtil::PermissionIgnored(permission_type_, GetGestureType(),
                                         request_origin_, profile_);
  }
}

PermissionRequest::IconId PermissionRequestImpl::GetIconId() const {
#if defined(OS_ANDROID)
  switch (permission_type_) {
    case content::PermissionType::GEOLOCATION:
      return IDR_ANDROID_INFOBAR_GEOLOCATION;
    case content::PermissionType::NOTIFICATIONS:
    case content::PermissionType::PUSH_MESSAGING:
      return IDR_ANDROID_INFOBAR_NOTIFICATIONS;
    case content::PermissionType::MIDI_SYSEX:
      return IDR_ANDROID_INFOBAR_MIDI;
    case content::PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      return IDR_ANDROID_INFOBAR_PROTECTED_MEDIA_IDENTIFIER;
    default:
      NOTREACHED();
      return IDR_ANDROID_INFOBAR_WARNING;
  }
#else
  switch (permission_type_) {
    case content::PermissionType::GEOLOCATION:
      return gfx::VectorIconId::LOCATION_ON;
    case content::PermissionType::NOTIFICATIONS:
    case content::PermissionType::PUSH_MESSAGING:
      return gfx::VectorIconId::NOTIFICATIONS;
#if defined(OS_CHROMEOS)
    // TODO(xhwang): fix this icon, see crrev.com/863263007
    case content::PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      return gfx::VectorIconId::PRODUCT;
#endif
    case content::PermissionType::MIDI_SYSEX:
      return gfx::VectorIconId::MIDI;
    case content::PermissionType::FLASH:
      return gfx::VectorIconId::EXTENSION;
    default:
      NOTREACHED();
      return gfx::VectorIconId::VECTOR_ICON_NONE;
  }
#endif
}

base::string16 PermissionRequestImpl::GetMessageTextFragment() const {
  int message_id;
  switch (permission_type_) {
    case content::PermissionType::GEOLOCATION:
      message_id = IDS_GEOLOCATION_INFOBAR_PERMISSION_FRAGMENT;
      break;
    case content::PermissionType::NOTIFICATIONS:
    case content::PermissionType::PUSH_MESSAGING:
      message_id = IDS_NOTIFICATION_PERMISSIONS_FRAGMENT;
      break;
    case content::PermissionType::MIDI_SYSEX:
      message_id = IDS_MIDI_SYSEX_PERMISSION_FRAGMENT;
      break;
#if defined(OS_CHROMEOS)
    case content::PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      message_id = IDS_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_FRAGMENT;
      break;
#endif
    case content::PermissionType::FLASH:
      message_id = IDS_FLASH_PERMISSION_FRAGMENT;
      break;
    default:
      NOTREACHED();
      return base::string16();
  }
  return l10n_util::GetStringUTF16(message_id);
}

GURL PermissionRequestImpl::GetOrigin() const {
  return request_origin_;
}

void PermissionRequestImpl::PermissionGranted() {
  RegisterActionTaken();
  permission_decided_callback_.Run(persist(), CONTENT_SETTING_ALLOW);
}

void PermissionRequestImpl::PermissionDenied() {
  RegisterActionTaken();
  permission_decided_callback_.Run(persist(), CONTENT_SETTING_BLOCK);
}

void PermissionRequestImpl::Cancelled() {
  RegisterActionTaken();
  permission_decided_callback_.Run(false, CONTENT_SETTING_DEFAULT);
}

void PermissionRequestImpl::RequestFinished() {
  is_finished_ = true;
  delete_callback_.Run();
}

bool PermissionRequestImpl::ShouldShowPersistenceToggle() const {
  return (permission_type_ == content::PermissionType::GEOLOCATION) &&
         PermissionUtil::ShouldShowPersistenceToggle();
}

PermissionRequestType PermissionRequestImpl::GetPermissionRequestType()
    const {
  return PermissionUtil::GetRequestType(permission_type_);
}

PermissionRequestGestureType PermissionRequestImpl::GetGestureType()
    const {
  return PermissionUtil::GetGestureType(has_gesture_);
}

ContentSettingsType PermissionRequestImpl::GetContentSettingsType() const {
  switch (permission_type_) {
    case content::PermissionType::GEOLOCATION:
      return CONTENT_SETTINGS_TYPE_GEOLOCATION;
    case content::PermissionType::PUSH_MESSAGING:
    case content::PermissionType::NOTIFICATIONS:
      return CONTENT_SETTINGS_TYPE_NOTIFICATIONS;
    case content::PermissionType::MIDI_SYSEX:
      return CONTENT_SETTINGS_TYPE_MIDI_SYSEX;
#if defined(OS_CHROMEOS)
    case content::PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      return CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER;
#endif
    case content::PermissionType::FLASH:
      return CONTENT_SETTINGS_TYPE_PLUGINS;
    default:
      NOTREACHED();
      return CONTENT_SETTINGS_TYPE_DEFAULT;
  }
}
