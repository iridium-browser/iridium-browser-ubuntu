// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "mojo/edk/embedder/embedder.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "device/bluetooth/android/bluetooth_jni_registrar.h"
#include "device/geolocation/android/geolocation_jni_registrar.h"
#include "device/usb/android/usb_jni_registrar.h"
#endif

int main(int argc, char** argv) {
#if defined(OS_ANDROID)
  device::android::RegisterBluetoothJni(base::android::AttachCurrentThread());
  device::android::RegisterGeolocationJni(base::android::AttachCurrentThread());
  device::android::RegisterUsbJni(base::android::AttachCurrentThread());
#endif

  base::TestSuite test_suite(argc, argv);

  mojo::edk::Init();
  return base::LaunchUnitTests(
      argc,
      argv,
      base::Bind(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
