// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NTP_NEW_TAB_PAGE_PREFS_H_
#define CHROME_BROWSER_ANDROID_NTP_NEW_TAB_PAGE_PREFS_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"

class NewTabPagePrefs {
 public:
  explicit NewTabPagePrefs(Profile* profile);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  jboolean GetCurrentlyOpenTabsCollapsed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SetCurrentlyOpenTabsCollapsed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean is_collapsed);

  jboolean GetSnapshotDocumentCollapsed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SetSnapshotDocumentCollapsed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean is_collapsed);

  jboolean GetRecentlyClosedTabsCollapsed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SetRecentlyClosedTabsCollapsed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean is_collapsed);

  jboolean GetSyncPromoCollapsed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SetSyncPromoCollapsed(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             jboolean is_collapsed);

  jboolean GetForeignSessionCollapsed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& session_tag);
  void SetForeignSessionCollapsed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& session_tag,
      jboolean is_collapsed);

  static bool RegisterNewTabPagePrefs(JNIEnv* env);
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
 private:
  virtual ~NewTabPagePrefs();

  Profile* profile_;  // weak
  DISALLOW_COPY_AND_ASSIGN(NewTabPagePrefs);
};

#endif  // CHROME_BROWSER_ANDROID_NTP_NEW_TAB_PAGE_PREFS_H_
