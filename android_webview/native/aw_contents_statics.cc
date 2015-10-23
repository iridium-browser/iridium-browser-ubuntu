// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_contents_statics.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/net/aw_url_request_context_getter.h"
#include "android_webview/common/aw_version_info_values.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_constants.h"
#include "jni/AwContentsStatics_jni.h"
#include "net/cert/cert_database.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ScopedJavaGlobalRef;
using content::BrowserThread;

namespace android_webview {

namespace {

void ClientCertificatesCleared(ScopedJavaGlobalRef<jobject>* callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  Java_AwContentsStatics_clientCertificatesCleared(env, callback->obj());
}

void NotifyClientCertificatesChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::CertDatabase::GetInstance()->OnAndroidKeyStoreChanged();
}

}  // namespace

// static
void ClearClientCertPreferences(JNIEnv* env, jclass, jobject callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ScopedJavaGlobalRef<jobject>* j_callback = new ScopedJavaGlobalRef<jobject>();
  j_callback->Reset(env, callback);
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&NotifyClientCertificatesChanged),
      base::Bind(&ClientCertificatesCleared, base::Owned(j_callback)));
}

// static
void SetDataReductionProxyKey(JNIEnv* env, jclass, jstring key) {
  AwBrowserContext* browser_context = AwBrowserContext::GetDefault();
  DCHECK(browser_context);
  DCHECK(browser_context->GetRequestContext());
  // The following call to GetRequestContext() could possibly be the first such
  // call, which means AwURLRequestContextGetter::InitializeURLRequestContext
  // will be called on IO thread as a result.
  AwURLRequestContextGetter* aw_url_request_context_getter =
      static_cast<AwURLRequestContextGetter*>(
          browser_context->GetRequestContext());

  // This PostTask has to be called after GetRequestContext, because SetKeyOnIO
  // needs a valid DataReductionProxyRequestOptions object.
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&AwURLRequestContextGetter::SetKeyOnIO,
                                     aw_url_request_context_getter,
                                     ConvertJavaStringToUTF8(env, key)));
}

// static
void SetDataReductionProxyEnabled(JNIEnv* env, jclass, jboolean enabled) {
  AwBrowserContext::SetDataReductionProxyEnabled(enabled);
}

// static
jstring GetUnreachableWebDataUrl(JNIEnv* env, jclass) {
  return base::android::ConvertUTF8ToJavaString(
             env, content::kUnreachableWebDataURL).Release();
}

// static
void SetRecordFullDocument(JNIEnv* env, jclass, jboolean record_full_document) {
  content::SynchronousCompositor::SetRecordFullDocument(record_full_document);
}

// static
void SetLegacyCacheRemovalDelayForTest(JNIEnv*, jclass, jlong delay_ms) {
  AwBrowserContext::SetLegacyCacheRemovalDelayForTest(delay_ms);
}

// static
jstring GetProductVersion(JNIEnv* env, jclass) {
  return base::android::ConvertUTF8ToJavaString(env, PRODUCT_VERSION).Release();
}

bool RegisterAwContentsStatics(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android_webview
