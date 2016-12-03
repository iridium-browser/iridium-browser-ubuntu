// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/save_password_infobar.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "jni/SavePasswordInfoBar_jni.h"

using base::android::JavaParamRef;

SavePasswordInfoBar::SavePasswordInfoBar(
    std::unique_ptr<SavePasswordInfoBarDelegate> delegate)
    : ConfirmInfoBar(std::move(delegate)) {}

SavePasswordInfoBar::~SavePasswordInfoBar() {
}

base::android::ScopedJavaLocalRef<jobject>
SavePasswordInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  using base::android::ConvertUTF16ToJavaString;
  using base::android::ScopedJavaLocalRef;
  SavePasswordInfoBarDelegate* save_password_delegate =
      static_cast<SavePasswordInfoBarDelegate*>(delegate());
  ScopedJavaLocalRef<jstring> ok_button_text = ConvertUTF16ToJavaString(
      env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_OK));
  ScopedJavaLocalRef<jstring> cancel_button_text = ConvertUTF16ToJavaString(
      env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_CANCEL));
  ScopedJavaLocalRef<jstring> message_text = ConvertUTF16ToJavaString(
      env, save_password_delegate->GetMessageText());
  ScopedJavaLocalRef<jstring> first_run_experience_message =
      ConvertUTF16ToJavaString(
          env, save_password_delegate->GetFirstRunExperienceMessage());

  return Java_SavePasswordInfoBar_show(
      env, GetEnumeratedIconId(), message_text,
      save_password_delegate->message_link_range().start(),
      save_password_delegate->message_link_range().end(), ok_button_text,
      cancel_button_text, first_run_experience_message);
}

void SavePasswordInfoBar::OnLinkClicked(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  GetDelegate()->LinkClicked(NEW_FOREGROUND_TAB);
}

std::unique_ptr<infobars::InfoBar> CreateSavePasswordInfoBar(
    std::unique_ptr<SavePasswordInfoBarDelegate> delegate) {
  return base::WrapUnique(new SavePasswordInfoBar(std::move(delegate)));
}
