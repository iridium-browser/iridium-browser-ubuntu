// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/context_menu_helper.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/android/download_controller_android.h"
#include "content/public/common/context_menu_params.h"
#include "jni/ContextMenuHelper_jni.h"
#include "jni/ContextMenuParams_jni.h"
#include "ui/gfx/geometry/point.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaLocalRef;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ContextMenuHelper);

ContextMenuHelper::ContextMenuHelper(content::WebContents* web_contents)
    : web_contents_(web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(
      env,
      Java_ContextMenuHelper_create(env, reinterpret_cast<long>(this)).obj());
  DCHECK(!java_obj_.is_null());
}

ContextMenuHelper::~ContextMenuHelper() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextMenuHelper_destroy(env, java_obj_.obj());
}

void ContextMenuHelper::ShowContextMenu(
    const content::ContextMenuParams& params) {
  content::ContentViewCore* content_view_core =
      content::ContentViewCore::FromWebContents(web_contents_);

  if (!content_view_core)
    return;

  base::android::ScopedJavaLocalRef<jobject> jcontent_view_core(
      content_view_core->GetJavaObject());

  if (jcontent_view_core.is_null())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  context_menu_params_ = params;
  Java_ContextMenuHelper_showContextMenu(
      env,
      java_obj_.obj(),
      jcontent_view_core.obj(),
      ContextMenuHelper::CreateJavaContextMenuParams(params).obj());
}

void ContextMenuHelper::SetPopulator(jobject jpopulator) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextMenuHelper_setPopulator(env, java_obj_.obj(), jpopulator);
}

base::android::ScopedJavaLocalRef<jobject>
ContextMenuHelper::CreateJavaContextMenuParams(
    const content::ContextMenuParams& params) {
  GURL sanitizedReferrer = (params.frame_url.is_empty() ?
      params.page_url : params.frame_url).GetAsReferrer();

  std::map<std::string, std::string>::const_iterator it =
      params.properties.find(data_reduction_proxy::chrome_proxy_header());
  bool image_was_fetched_lo_fi =
      it != params.properties.end() &&
      it->second == data_reduction_proxy::chrome_proxy_lo_fi_directive();

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> jmenu_info =
      ContextMenuParamsAndroid::Java_ContextMenuParams_create(
          env,
          params.media_type,
          ConvertUTF8ToJavaString(env, params.page_url.spec()).obj(),
          ConvertUTF8ToJavaString(env, params.link_url.spec()).obj(),
          ConvertUTF16ToJavaString(env, params.link_text).obj(),
          ConvertUTF8ToJavaString(env, params.unfiltered_link_url.spec()).obj(),
          ConvertUTF8ToJavaString(env, params.src_url.spec()).obj(),
          ConvertUTF16ToJavaString(env, params.title_text).obj(),
          image_was_fetched_lo_fi,
          ConvertUTF8ToJavaString(env, sanitizedReferrer.spec()).obj(),
          params.referrer_policy);

  return jmenu_info;
}

void ContextMenuHelper::OnStartDownload(JNIEnv* env,
                                        jobject obj,
                                        jboolean jis_link,
                                        jstring jheaders) {
  std::string headers(ConvertJavaStringToUTF8(env, jheaders));
  content::DownloadControllerAndroid::Get()->StartContextMenuDownload(
      context_menu_params_,
      web_contents_,
      jis_link,
      headers);
}

bool RegisterContextMenuHelper(JNIEnv* env) {
  return RegisterNativesImpl(env) &&
         ContextMenuParamsAndroid::RegisterNativesImpl(env);
}
