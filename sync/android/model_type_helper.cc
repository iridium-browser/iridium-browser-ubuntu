// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/android/model_type_helper.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "jni/ModelTypeHelper_jni.h"
#include "sync/internal_api/public/base/model_type.h"

namespace syncer {

static jstring ModelTypeToNotificationType(JNIEnv* env,
                                           jclass clazz,
                                           jint model_type_int) {
  std::string model_type_string;
  ModelType model_type = static_cast<ModelType>(model_type_int);
  if (!RealModelTypeToNotificationType(model_type, &model_type_string)) {
    NOTREACHED() << "No string representation of model type " << model_type;
  }
  return base::android::ConvertUTF8ToJavaString(env, model_type_string)
      .Release();
}

bool RegisterModelTypeHelperJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace syncer
