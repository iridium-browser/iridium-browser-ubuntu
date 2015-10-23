// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/rlz/revenue_stats.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data_android.h"
#include "jni/RevenueStats_jni.h"
#include "url/gurl.h"

namespace chrome {
namespace android {

// Register native methods
bool RegisterRevenueStats(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static void SetSearchClient(JNIEnv* env, jclass clazz, jstring jclient) {
  SearchTermsDataAndroid::search_client_.Get() =
      base::android::ConvertJavaStringToUTF8(env, jclient);
}

static void SetRlzParameterValue(JNIEnv* env, jclass clazz, jstring jrlz) {
  SearchTermsDataAndroid::rlz_parameter_value_.Get() =
      base::android::ConvertJavaStringToUTF16(env, jrlz);
}

}  // namespace android
}  // namespace chrome
