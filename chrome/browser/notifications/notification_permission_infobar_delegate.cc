// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_permission_infobar_delegate.h"

#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/grit/generated_resources.h"

NotificationPermissionInfoBarDelegate::NotificationPermissionInfoBarDelegate(
    const content::PermissionType& permission_type,
    const GURL& requesting_frame,
    bool user_gesture,
    Profile* profile,
    const PermissionSetCallback& callback)
    : PermissionInfoBarDelegate(requesting_frame,
                                permission_type,
                                CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                user_gesture,
                                profile,
                                callback) {
  DCHECK(permission_type == content::PermissionType::NOTIFICATIONS ||
         permission_type == content::PermissionType::PUSH_MESSAGING);
}

NotificationPermissionInfoBarDelegate::~NotificationPermissionInfoBarDelegate()
    {}

infobars::InfoBarDelegate::InfoBarIdentifier
NotificationPermissionInfoBarDelegate::GetIdentifier() const {
  return NOTIFICATION_PERMISSION_INFOBAR_DELEGATE;
}

int NotificationPermissionInfoBarDelegate::GetIconId() const {
  return IDR_ANDROID_INFOBAR_NOTIFICATIONS;
}

int NotificationPermissionInfoBarDelegate::GetMessageResourceId() const {
  return IDS_NOTIFICATION_PERMISSIONS;
}
