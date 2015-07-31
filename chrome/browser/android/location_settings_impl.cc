// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/location_settings_impl.h"

#include "base/android/jni_android.h"
#include "content/public/browser/web_contents.h"
#include "jni/LocationSettings_jni.h"

using base::android::AttachCurrentThread;

LocationSettingsImpl::LocationSettingsImpl() {}

LocationSettingsImpl::~LocationSettingsImpl() {}

bool LocationSettingsImpl::CanSitesRequestLocationPermission(
    content::WebContents* web_contents) {
  JNIEnv* env = AttachCurrentThread();
  return Java_LocationSettings_canSitesRequestLocationPermission(
      env, web_contents->GetJavaWebContents().obj());
}

// Register native methods

bool LocationSettingsImpl::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
