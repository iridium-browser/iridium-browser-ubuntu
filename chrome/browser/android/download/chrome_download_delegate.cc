// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/chrome_download_delegate.h"

#include <jni.h>

#include <string>
#include <type_traits>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "chrome/browser/android/download/android_download_manager_overwrite_infobar_delegate.h"
#include "chrome/browser/android/download/download_controller_base.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/permissions/permission_update_infobar_delegate_android.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "jni/ChromeDownloadDelegate_jni.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::WebContents;

// Gets the download warning text for the given file name.
static ScopedJavaLocalRef<jstring> GetDownloadWarningText(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& filename) {
  return base::android::ConvertUTF8ToJavaString(
      env, l10n_util::GetStringFUTF8(
               IDS_PROMPT_DANGEROUS_DOWNLOAD,
               base::android::ConvertJavaStringToUTF16(env, filename)));
}

// Returns true if a file name is dangerous, or false otherwise.
static jboolean IsDownloadDangerous(JNIEnv* env,
                                    const JavaParamRef<jclass>& clazz,
                                    const JavaParamRef<jstring>& filename) {
  base::FilePath path(base::android::ConvertJavaStringToUTF8(env, filename));
  return safe_browsing::FileTypePolicies::GetInstance()->GetFileDangerLevel(
             path) != safe_browsing::DownloadFileType::NOT_DANGEROUS;
}

// Called when a dangerous download is validated.
static void DangerousDownloadValidated(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& tab,
    const JavaParamRef<jstring>& jdownload_guid,
    jboolean accept) {
  std::string download_guid =
      base::android::ConvertJavaStringToUTF8(env, jdownload_guid);
  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, tab);
  DownloadControllerBase::Get()->DangerousDownloadValidated(
      tab_android->web_contents(), download_guid, accept);
}

// static
bool ChromeDownloadDelegate::EnqueueDownloadManagerRequest(
    jobject chrome_download_delegate,
    bool overwrite,
    jobject download_info) {
  JNIEnv* env = base::android::AttachCurrentThread();

  return Java_ChromeDownloadDelegate_enqueueDownloadManagerRequestFromNative(
      env, chrome_download_delegate, overwrite, download_info);
}

// Called when we need to interrupt download and ask users whether to overwrite
// an existing file.
static void LaunchDownloadOverwriteInfoBar(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& delegate,
    const JavaParamRef<jobject>& tab,
    const JavaParamRef<jobject>& download_info,
    const JavaParamRef<jstring>& jfile_name,
    const JavaParamRef<jstring>& jdir_name,
    const JavaParamRef<jstring>& jdir_full_path) {
  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, tab);

  std::string file_name =
      base::android::ConvertJavaStringToUTF8(env, jfile_name);
  std::string dir_name = base::android::ConvertJavaStringToUTF8(env, jdir_name);
  std::string dir_full_path =
      base::android::ConvertJavaStringToUTF8(env, jdir_full_path);

  chrome::android::AndroidDownloadManagerOverwriteInfoBarDelegate::Create(
      InfoBarService::FromWebContents(tab_android->web_contents()), file_name,
      dir_name, dir_full_path, delegate, download_info);
}

static void LaunchPermissionUpdateInfoBar(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& tab,
    const JavaParamRef<jstring>& jpermission,
    jlong callback_id) {
  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, tab);

  std::string permission =
      base::android::ConvertJavaStringToUTF8(env, jpermission);

  // Convert java long long int to c++ pointer, take ownership.
  static_assert(
      std::is_same<
          DownloadControllerBase::AcquireFileAccessPermissionCallback,
          base::Callback<void(bool)>>::value,
      "Callback types don't match!");
  std::unique_ptr<base::Callback<void(bool)>> cb(
      reinterpret_cast<base::Callback<void(bool)>*>(callback_id));

  std::vector<std::string> permissions;
  permissions.push_back(permission);

  PermissionUpdateInfoBarDelegate::Create(
      tab_android->web_contents(),
      permissions,
      IDS_MISSING_STORAGE_PERMISSION_DOWNLOAD_EDUCATION_TEXT,
      *cb);
}

ChromeDownloadDelegate::ChromeDownloadDelegate(
    WebContents* web_contents) {}

ChromeDownloadDelegate::~ChromeDownloadDelegate() {
   JNIEnv* env = base::android::AttachCurrentThread();
   env->DeleteGlobalRef(java_ref_);
}

void ChromeDownloadDelegate::SetJavaRef(JNIEnv* env, jobject jobj) {
  java_ref_ = env->NewGlobalRef(jobj);
}

void ChromeDownloadDelegate::RequestHTTPGetDownload(
    const std::string& url,
    const std::string& user_agent,
    const std::string& content_disposition,
    const std::string& mime_type,
    const std::string& cookie,
    const std::string& referer,
    const base::string16& file_name,
    int64_t content_length,
    bool has_user_gesture,
    bool must_download) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jurl =
      ConvertUTF8ToJavaString(env, url);
  ScopedJavaLocalRef<jstring> juser_agent =
      ConvertUTF8ToJavaString(env, user_agent);
  ScopedJavaLocalRef<jstring> jcontent_disposition =
      ConvertUTF8ToJavaString(env, content_disposition);
  ScopedJavaLocalRef<jstring> jmime_type =
      ConvertUTF8ToJavaString(env, mime_type);
  ScopedJavaLocalRef<jstring> jcookie =
      ConvertUTF8ToJavaString(env, cookie);
  ScopedJavaLocalRef<jstring> jreferer =
      ConvertUTF8ToJavaString(env, referer);

  // net::GetSuggestedFilename will fallback to "download" as filename.
  ScopedJavaLocalRef<jstring> jfilename =
      base::android::ConvertUTF16ToJavaString(env, file_name);
  Java_ChromeDownloadDelegate_requestHttpGetDownload(
      env, java_ref_, jurl, juser_agent, jcontent_disposition, jmime_type,
      jcookie, jreferer, has_user_gesture, jfilename, content_length,
      must_download);
}

void ChromeDownloadDelegate::OnDownloadStarted(const std::string& filename,
                                               const std::string& mime_type) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jfilename = ConvertUTF8ToJavaString(
      env, filename);
  ScopedJavaLocalRef<jstring> jmime_type =
      ConvertUTF8ToJavaString(env, mime_type);
  Java_ChromeDownloadDelegate_onDownloadStarted(env, java_ref_, jfilename,
                                                jmime_type);
}

void ChromeDownloadDelegate::OnDangerousDownload(const std::string& filename,
                                                 const std::string& guid) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jfilename = ConvertUTF8ToJavaString(
      env, filename);
  ScopedJavaLocalRef<jstring> jguid = ConvertUTF8ToJavaString(env, guid);
  Java_ChromeDownloadDelegate_onDangerousDownload(env, java_ref_, jfilename,
                                                  jguid);
}

void ChromeDownloadDelegate::RequestFileAccess(intptr_t callback_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ChromeDownloadDelegate_requestFileAccess(
      env, java_ref_, callback_id);
}

void Init(JNIEnv* env,
          const JavaParamRef<jobject>& obj,
          const JavaParamRef<jobject>& jweb_contents) {
  auto* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  ChromeDownloadDelegate::CreateForWebContents(web_contents);
  ChromeDownloadDelegate::FromWebContents(web_contents)->SetJavaRef(env, obj);
}

bool RegisterChromeDownloadDelegate(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ChromeDownloadDelegate);
