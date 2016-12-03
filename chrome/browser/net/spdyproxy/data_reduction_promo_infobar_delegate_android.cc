// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_promo_infobar_delegate_android.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/web_contents.h"
#include "jni/DataReductionPromoInfoBarDelegate_jni.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::JavaParamRef;

// static
void DataReductionPromoInfoBarDelegateAndroid::Create(
    content::WebContents* web_contents) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  infobar_service->AddInfoBar(
      DataReductionPromoInfoBarDelegateAndroid::CreateInfoBar(
          infobar_service,
          base::MakeUnique<DataReductionPromoInfoBarDelegateAndroid>()));
}

DataReductionPromoInfoBarDelegateAndroid::
    DataReductionPromoInfoBarDelegateAndroid() {}

DataReductionPromoInfoBarDelegateAndroid::
    ~DataReductionPromoInfoBarDelegateAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DataReductionPromoInfoBarDelegate_onNativeDestroyed(env);
}

// static
bool DataReductionPromoInfoBarDelegateAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
void DataReductionPromoInfoBarDelegateAndroid::Launch(
    JNIEnv* env, jclass, jobject jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  Create(web_contents);
}

base::android::ScopedJavaLocalRef<jobject>
DataReductionPromoInfoBarDelegateAndroid::CreateRenderInfoBar(JNIEnv* env) {
  return Java_DataReductionPromoInfoBarDelegate_showPromoInfoBar(env);
}

infobars::InfoBarDelegate::InfoBarIdentifier
DataReductionPromoInfoBarDelegateAndroid::GetIdentifier() const {
  return DATA_REDUCTION_PROMO_INFOBAR_DELEGATE_ANDROID;
}

base::string16 DataReductionPromoInfoBarDelegateAndroid::GetMessageText()
    const {
  // Message is set in DataReductionPromoInfoBar.java.
  return base::string16();
}

bool DataReductionPromoInfoBarDelegateAndroid::Accept() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DataReductionPromoInfoBarDelegate_accept(env);
  return true;
}

// JNI for DataReductionPromoInfoBarDelegate.
void Launch(JNIEnv* env,
            const JavaParamRef<jclass>& clazz,
            const JavaParamRef<jobject>& jweb_contents) {
  DataReductionPromoInfoBarDelegateAndroid::Launch(env, clazz, jweb_contents);
}
