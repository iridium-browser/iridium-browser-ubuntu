// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/android/ui_base_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "ui/base/clipboard/clipboard_android.h"
#include "ui/base/device_form_factor_android.h"
#include "ui/base/l10n/l10n_util_android.h"
#include "ui/base/resource/resource_bundle_android.h"
#include "ui/base/touch/touch_device.h"

namespace ui {
namespace android {

static base::android::RegistrationMethod kUiRegisteredMethods[] = {
  { "Clipboard", RegisterClipboardAndroid },
  { "DeviceFormFactor", RegisterDeviceFormFactorAndroid },
  { "LocalizationUtils", l10n_util::RegisterLocalizationUtil },
  { "ResourceBundle", RegisterResourceBundleAndroid },
  { "TouchDevice", RegisterTouchDeviceAndroid },
};

bool RegisterJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kUiRegisteredMethods,
                               arraysize(kUiRegisteredMethods));
}

}  // namespace android
}  // namespace ui
