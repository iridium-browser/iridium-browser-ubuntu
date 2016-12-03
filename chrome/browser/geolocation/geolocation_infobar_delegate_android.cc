// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_infobar_delegate_android.h"

#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "grit/generated_resources.h"

// static
infobars::InfoBar* GeolocationInfoBarDelegateAndroid::Create(
    InfoBarService* infobar_service,
    const GURL& requesting_frame,
    bool user_gesture,
    Profile* profile,
    const PermissionSetCallback& callback) {
  return infobar_service->AddInfoBar(
      CreatePermissionInfoBar(std::unique_ptr<PermissionInfoBarDelegate>(
          new GeolocationInfoBarDelegateAndroid(requesting_frame, user_gesture,
                                                profile, callback))));
}

GeolocationInfoBarDelegateAndroid::GeolocationInfoBarDelegateAndroid(
    const GURL& requesting_frame,
    bool user_gesture,
    Profile* profile,
    const PermissionSetCallback& callback)
    : PermissionInfoBarDelegate(requesting_frame,
                                content::PermissionType::GEOLOCATION,
                                CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                user_gesture,
                                profile,
                                callback) {}

GeolocationInfoBarDelegateAndroid::~GeolocationInfoBarDelegateAndroid() {}

infobars::InfoBarDelegate::InfoBarIdentifier
GeolocationInfoBarDelegateAndroid::GetIdentifier() const {
  return GEOLOCATION_INFOBAR_DELEGATE_ANDROID;
}

int GeolocationInfoBarDelegateAndroid::GetIconId() const {
  return IDR_ANDROID_INFOBAR_GEOLOCATION;
}

int GeolocationInfoBarDelegateAndroid::GetMessageResourceId() const {
  return IDS_GEOLOCATION_INFOBAR_QUESTION;
}
