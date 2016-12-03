// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/download_overwrite_infobar.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/android/download/download_overwrite_infobar_delegate.h"
#include "jni/DownloadOverwriteInfoBar_jni.h"

using base::android::ScopedJavaLocalRef;
using chrome::android::DownloadOverwriteInfoBarDelegate;

// static
std::unique_ptr<infobars::InfoBar> DownloadOverwriteInfoBar::CreateInfoBar(
    std::unique_ptr<DownloadOverwriteInfoBarDelegate> delegate) {
  return base::WrapUnique(new DownloadOverwriteInfoBar(std::move(delegate)));
}

DownloadOverwriteInfoBar::~DownloadOverwriteInfoBar() {
}

DownloadOverwriteInfoBar::DownloadOverwriteInfoBar(
    std::unique_ptr<DownloadOverwriteInfoBarDelegate> delegate)
    : InfoBarAndroid(std::move(delegate)) {}

base::android::ScopedJavaLocalRef<jobject>
DownloadOverwriteInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  DownloadOverwriteInfoBarDelegate* delegate = GetDelegate();

  ScopedJavaLocalRef<jstring> j_file_name =
      base::android::ConvertUTF8ToJavaString(env, delegate->GetFileName());
  ScopedJavaLocalRef<jstring> j_dir_name =
      base::android::ConvertUTF8ToJavaString(env, delegate->GetDirName());
  ScopedJavaLocalRef<jstring> j_dir_full_path =
      base::android::ConvertUTF8ToJavaString(env, delegate->GetDirFullPath());
  base::android::ScopedJavaLocalRef<jobject> java_infobar(
      Java_DownloadOverwriteInfoBar_createInfoBar(env, j_file_name, j_dir_name,
                                                  j_dir_full_path));
  return java_infobar;
}

void DownloadOverwriteInfoBar::ProcessButton(int action) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.

  DownloadOverwriteInfoBarDelegate* delegate = GetDelegate();
  if (action == InfoBarAndroid::ACTION_OVERWRITE) {
    if (delegate->OverwriteExistingFile())
      RemoveSelf();
  } else if (action == InfoBarAndroid::ACTION_CREATE_NEW_FILE) {
    if (delegate->CreateNewFile())
      RemoveSelf();
  } else {
    CHECK(false);
  }
}

DownloadOverwriteInfoBarDelegate* DownloadOverwriteInfoBar::GetDelegate() {
  return static_cast<DownloadOverwriteInfoBarDelegate*>(delegate());
}
