// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/android/component_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/macros.h"
#include "components/invalidation/impl/invalidation_service_android.h"

namespace invalidation {

namespace android {

static base::android::RegistrationMethod kInvalidationRegisteredMethods[] = {
    {"InvalidationService",
        invalidation::InvalidationServiceAndroid::RegisterJni},
};

bool RegisterInvalidationJni(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env,
      kInvalidationRegisteredMethods,
      arraysize(kInvalidationRegisteredMethods));
}

}  // namespace android

}  // namespace invalidation
