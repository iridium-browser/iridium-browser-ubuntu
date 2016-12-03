// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_BRIDGE_H_

#include <stdint.h>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/supports_user_data.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_model.h"

namespace content {
class BrowserContext;
}

namespace offline_pages {
namespace android {

/**
 * Bridge between C++ and Java for exposing native implementation of offline
 * pages model in managed code.
 */
class OfflinePageBridge : public OfflinePageModel::Observer,
                          public base::SupportsUserData::Data {
 public:
  static base::android::ScopedJavaLocalRef<jobject> ConvertToJavaOfflinePage(
      JNIEnv* env,
      const OfflinePageItem& offline_page);

  OfflinePageBridge(JNIEnv* env,
                    content::BrowserContext* browser_context,
                    OfflinePageModel* offline_page_model);
  ~OfflinePageBridge() override;

  // OfflinePageModel::Observer implementation.
  void OfflinePageModelLoaded(OfflinePageModel* model) override;
  void OfflinePageModelChanged(OfflinePageModel* model) override;
  void OfflinePageDeleted(int64_t offline_id,
                          const ClientId& client_id) override;

  void HasPages(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                const base::android::JavaParamRef<jstring>& name_space,
                const base::android::JavaParamRef<jobject>& j_callback_obj);

  void CheckPagesExistOffline(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobjectArray>& j_urls_array,
      const base::android::JavaParamRef<jobject>& j_callback_obj);

  void GetAllPages(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   const base::android::JavaParamRef<jobject>& j_result_obj,
                   const base::android::JavaParamRef<jobject>& j_callback_obj);

  base::android::ScopedJavaLocalRef<jlongArray> GetOfflineIdsForClientId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& j_namespace,
      const base::android::JavaParamRef<jstring>& j_client_id);

  base::android::ScopedJavaLocalRef<jobject> GetPageByOfflineId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong offline_id);

  void SelectPageForOnlineUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& j_online_url,
      int tab_id,
      const base::android::JavaParamRef<jobject>& j_callback_obj);

  void GetPageByOfflineUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& j_offline_url,
      const base::android::JavaParamRef<jobject>& j_callback_obj);

  void SavePage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_callback_obj,
      const base::android::JavaParamRef<jobject>& j_web_contents,
      const base::android::JavaParamRef<jstring>& j_namespace,
      const base::android::JavaParamRef<jstring>& j_client_id);

  void SavePageLater(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     const base::android::JavaParamRef<jstring>& url,
                     const base::android::JavaParamRef<jstring>& j_namespace,
                     const base::android::JavaParamRef<jstring>& j_client_id,
                     jboolean user_requested);

  void DeletePages(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_callback_obj,
      const base::android::JavaParamRef<jlongArray>& j_offline_ids_array);

  void CheckMetadataConsistency(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jstring> GetOfflinePageHeaderForReload(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_web_contents);

  void GetRequestsInQueue(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_callback_obj);

  void RemoveRequestsFromQueue(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jlongArray>& j_request_ids_array,
      const base::android::JavaParamRef<jobject>& j_callback_obj);

  base::android::ScopedJavaGlobalRef<jobject> java_ref() { return java_ref_; }

 private:
  void NotifyIfDoneLoading() const;

  base::android::ScopedJavaLocalRef<jobject> CreateClientId(
      JNIEnv* env,
      const ClientId& clientId) const;

  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
  // Not owned.
  content::BrowserContext* browser_context_;
  // Not owned.
  OfflinePageModel* offline_page_model_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageBridge);
};

bool RegisterOfflinePageBridge(JNIEnv* env);

}  // namespace android
}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_BRIDGE_H_

