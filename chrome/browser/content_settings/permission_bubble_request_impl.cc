// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/permission_bubble_request_impl.h"

#include "chrome/browser/content_settings/permission_context_base.h"
#include "chrome/browser/content_settings/permission_context_uma_util.h"
#include "chrome/grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

PermissionBubbleRequestImpl::PermissionBubbleRequestImpl(
    const GURL& request_origin,
    bool user_gesture,
    ContentSettingsType type,
    const std::string& display_languages,
    const PermissionDecidedCallback& permission_decided_callback,
    const base::Closure delete_callback)
    : request_origin_(request_origin),
      user_gesture_(user_gesture),
      type_(type),
      display_languages_(display_languages),
      permission_decided_callback_(permission_decided_callback),
      delete_callback_(delete_callback),
      is_finished_(false),
      action_taken_(false) {
}

PermissionBubbleRequestImpl::~PermissionBubbleRequestImpl() {
  DCHECK(is_finished_);
  if (!action_taken_)
    PermissionContextUmaUtil::PermissionIgnored(type_, request_origin_);
}

int PermissionBubbleRequestImpl::GetIconID() const {
  int icon_id;
  switch (type_) {
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      icon_id = IDR_INFOBAR_GEOLOCATION;
      break;
#if defined(ENABLE_NOTIFICATIONS)
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      icon_id = IDR_INFOBAR_DESKTOP_NOTIFICATIONS;
      break;
#endif
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      icon_id = IDR_ALLOWED_MIDI_SYSEX;
      break;
    case CONTENT_SETTINGS_TYPE_PUSH_MESSAGING:
      icon_id = IDR_INFOBAR_WARNING;
      break;
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
    case CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER:
      icon_id = IDR_INFOBAR_PROTECTED_MEDIA_IDENTIFIER;
      break;
#endif
    default:
      NOTREACHED();
      return IDR_INFOBAR_WARNING;
  }
  return icon_id;
}

base::string16 PermissionBubbleRequestImpl::GetMessageText() const {
  int message_id;
  switch (type_) {
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      message_id = IDS_GEOLOCATION_INFOBAR_QUESTION;
      break;
#if defined(ENABLE_NOTIFICATIONS)
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      message_id = IDS_NOTIFICATION_PERMISSIONS;
      break;
#endif
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      message_id = IDS_MIDI_SYSEX_INFOBAR_QUESTION;
      break;
    case CONTENT_SETTINGS_TYPE_PUSH_MESSAGING:
      message_id = IDS_PUSH_MESSAGES_PERMISSION_QUESTION;
      break;
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
    case CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER:
      message_id = IDS_PROTECTED_MEDIA_IDENTIFIER_INFOBAR_QUESTION;
      break;
#endif
    default:
      NOTREACHED();
      return base::string16();
  }
  return l10n_util::GetStringFUTF16(
      message_id,
      net::FormatUrl(request_origin_, display_languages_,
                     net::kFormatUrlOmitUsernamePassword |
                     net::kFormatUrlOmitTrailingSlashOnBareHostname,
                     net::UnescapeRule::SPACES, NULL, NULL, NULL));
}

base::string16 PermissionBubbleRequestImpl::GetMessageTextFragment() const {
  int message_id;
  switch (type_) {
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      message_id = IDS_GEOLOCATION_INFOBAR_PERMISSION_FRAGMENT;
      break;
#if defined(ENABLE_NOTIFICATIONS)
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      message_id = IDS_NOTIFICATION_PERMISSIONS_FRAGMENT;
      break;
#endif
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      message_id = IDS_MIDI_SYSEX_PERMISSION_FRAGMENT;
      break;
    case CONTENT_SETTINGS_TYPE_PUSH_MESSAGING:
      message_id = IDS_PUSH_MESSAGES_BUBBLE_FRAGMENT;
      break;
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
    case CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER:
      message_id = IDS_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_FRAGMENT;
      break;
#endif
    default:
      NOTREACHED();
      return base::string16();
  }
  return l10n_util::GetStringUTF16(message_id);
}

bool PermissionBubbleRequestImpl::HasUserGesture() const {
  return user_gesture_;
}

GURL PermissionBubbleRequestImpl::GetRequestingHostname() const {
  return request_origin_;
}

void PermissionBubbleRequestImpl::PermissionGranted() {
  RegisterActionTaken();
  permission_decided_callback_.Run(true, CONTENT_SETTING_ALLOW);
}

void PermissionBubbleRequestImpl::PermissionDenied() {
  RegisterActionTaken();
  permission_decided_callback_.Run(true, CONTENT_SETTING_BLOCK);
}

void PermissionBubbleRequestImpl::Cancelled() {
  RegisterActionTaken();
  permission_decided_callback_.Run(false, CONTENT_SETTING_DEFAULT);
}

void PermissionBubbleRequestImpl::RequestFinished() {
  is_finished_ = true;
  delete_callback_.Run();
}
