// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/security_state_model_android.h"

#include "base/logging.h"
#include "chrome/browser/ssl/chrome_security_state_model_client.h"
#include "content/public/browser/web_contents.h"
#include "jni/SecurityStateModel_jni.h"

using base::android::JavaParamRef;

// static
bool RegisterSecurityStateModelAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
jint GetSecurityLevelForWebContents(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    const JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  ChromeSecurityStateModelClient::CreateForWebContents(web_contents);
  ChromeSecurityStateModelClient* model_client =
      ChromeSecurityStateModelClient::FromWebContents(web_contents);
  DCHECK(model_client);
  return model_client->GetSecurityInfo().security_level;
}

// static
jboolean IsDeprecatedSHA1Present(JNIEnv* env,
                                 const JavaParamRef<jclass>& jcaller,
                                 const JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  ChromeSecurityStateModelClient::CreateForWebContents(web_contents);
  ChromeSecurityStateModelClient* model_client =
      ChromeSecurityStateModelClient::FromWebContents(web_contents);
  DCHECK(model_client);
  return model_client->GetSecurityInfo().sha1_deprecation_status !=
         security_state::SecurityStateModel::NO_DEPRECATED_SHA1;
}

// static
jboolean IsPassiveMixedContentPresent(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    const JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  ChromeSecurityStateModelClient::CreateForWebContents(web_contents);
  ChromeSecurityStateModelClient* model_client =
      ChromeSecurityStateModelClient::FromWebContents(web_contents);
  DCHECK(model_client);
  return model_client->GetSecurityInfo().mixed_content_status ==
             security_state::SecurityStateModel::CONTENT_STATUS_DISPLAYED ||
         model_client->GetSecurityInfo().mixed_content_status ==
             security_state::SecurityStateModel::
                 CONTENT_STATUS_DISPLAYED_AND_RAN;
}
