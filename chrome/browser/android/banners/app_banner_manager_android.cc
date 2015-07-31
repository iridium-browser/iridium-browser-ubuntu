// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/banners/app_banner_manager_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "chrome/browser/android/banners/app_banner_data_fetcher_android.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "jni/AppBannerManager_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;

namespace {
const char kPlayPlatform[] = "play";
}  // anonymous namespace

namespace banners {

AppBannerManagerAndroid::AppBannerManagerAndroid(JNIEnv* env,
                                                 jobject obj,
                                                 int icon_size)
    : AppBannerManager(icon_size),
      weak_java_banner_view_manager_(env, obj) {
}

AppBannerManagerAndroid::~AppBannerManagerAndroid() {
}

void AppBannerManagerAndroid::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void AppBannerManagerAndroid::ReplaceWebContents(JNIEnv* env,
                                                 jobject obj,
                                                 jobject jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  AppBannerManager::ReplaceWebContents(web_contents);
}

bool AppBannerManagerAndroid::HandleNonWebApp(const std::string& platform,
                                              const GURL& url,
                                              const std::string& id) {
  if (!CheckPlatformAndId(platform, id))
    return false;

  banners::TrackDisplayEvent(DISPLAY_EVENT_BANNER_REQUESTED);

  // Send the info to the Java side to get info about the app.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = weak_java_banner_view_manager_.get(env);
  if (jobj.is_null())
    return false;

  ScopedJavaLocalRef<jstring> jurl(
      ConvertUTF8ToJavaString(env, data_fetcher()->validated_url().spec()));
  ScopedJavaLocalRef<jstring> jpackage(
      ConvertUTF8ToJavaString(env, id));
  Java_AppBannerManager_fetchAppDetails(
      env, jobj.obj(), jurl.obj(), jpackage.obj(), ideal_icon_size());
  return true;
}

bool AppBannerManagerAndroid::CheckPlatformAndId(const std::string& platform,
                                                 const std::string& id) {
  if (platform != kPlayPlatform) {
    banners::OutputDeveloperDebugMessage(
        web_contents(), platform + banners::kIgnoredNotSupportedOnAndroid);
    return false;
  }
  if (id.empty()) {
    banners::OutputDeveloperDebugMessage(web_contents(), banners::kIgnoredNoId);
    return false;
  }
  return true;
}

bool AppBannerManagerAndroid::CheckFetcherMatchesContents() {
  if (!web_contents()) {
    return false;
  }
  if (!data_fetcher() ||
      data_fetcher()->validated_url() != web_contents()->GetURL()) {
    banners::OutputDeveloperNotShownMessage(
        web_contents(), banners::kUserNavigatedBeforeBannerShown);
    return false;
  }
  return true;
}

AppBannerDataFetcher* AppBannerManagerAndroid::CreateAppBannerDataFetcher(
    base::WeakPtr<Delegate> weak_delegate,
    const int ideal_icon_size) {
  return new AppBannerDataFetcherAndroid(web_contents(), weak_delegate,
                                         ideal_icon_size);
}

bool AppBannerManagerAndroid::OnAppDetailsRetrieved(JNIEnv* env,
                                                    jobject obj,
                                                    jobject japp_data,
                                                    jstring japp_title,
                                                    jstring japp_package,
                                                    jstring jicon_url) {
  if (!CheckFetcherMatchesContents())
    return false;

  base::android::ScopedJavaLocalRef<jobject> native_app_data;
  native_app_data.Reset(env, japp_data);
  GURL image_url = GURL(ConvertJavaStringToUTF8(env, jicon_url));

  AppBannerDataFetcherAndroid* android_fetcher =
      static_cast<AppBannerDataFetcherAndroid*>(data_fetcher().get());
  return android_fetcher->ContinueFetching(
      ConvertJavaStringToUTF16(env, japp_title),
      ConvertJavaStringToUTF8(env, japp_package),
      native_app_data, image_url);
}

bool AppBannerManagerAndroid::IsFetcherActive(JNIEnv* env, jobject obj) {
  return AppBannerManager::IsFetcherActive();
}

// static
bool AppBannerManagerAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

jlong Init(JNIEnv* env, jobject obj, jint icon_size) {
  AppBannerManagerAndroid* manager =
      new AppBannerManagerAndroid(env, obj, icon_size);
  return reinterpret_cast<intptr_t>(manager);
}

void SetTimeDeltaForTesting(JNIEnv* env, jclass clazz, jint days) {
  AppBannerDataFetcher::SetTimeDeltaForTesting(days);
}

void DisableSecureSchemeCheckForTesting(JNIEnv* env, jclass clazz) {
  AppBannerManager::DisableSecureSchemeCheckForTesting();
}

jboolean IsEnabled(JNIEnv* env, jclass clazz) {
  return AppBannerManager::IsEnabled();
}

}  // namespace banners
