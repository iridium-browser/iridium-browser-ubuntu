// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_jni_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "content/public/browser/web_contents.h"
#include "jni/TabModelJniBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using content::WebContents;

namespace {

static Profile* FindProfile(jboolean is_incognito) {
  if (g_browser_process == NULL ||
      g_browser_process->profile_manager() == NULL) {
    LOG(ERROR) << "Browser process or profile manager not initialized";
    return NULL;
  }
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (is_incognito)
    return profile->GetOffTheRecordProfile();
  return profile;
}

}  // namespace

TabModelJniBridge::TabModelJniBridge(JNIEnv* env,
                                     jobject jobj,
                                     bool is_incognito)
    : TabModel(FindProfile(is_incognito)),
      java_object_(env, env->NewWeakGlobalRef(jobj)) {
  TabModelList::AddTabModel(this);
}

void TabModelJniBridge::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

ScopedJavaLocalRef<jobject> TabModelJniBridge::GetProfileAndroid(JNIEnv* env,
                                                                 jobject obj) {
  ProfileAndroid* profile_android = ProfileAndroid::FromProfile(GetProfile());
  if (!profile_android)
    return ScopedJavaLocalRef<jobject>();
  return profile_android->GetJavaObject();
}

void TabModelJniBridge::TabAddedToModel(JNIEnv* env,
                                        jobject obj,
                                        jobject jtab) {
  TabAndroid* tab = TabAndroid::GetNativeTab(env, jtab);

  // Tab#initialize() should have been called by now otherwise we can't push
  // the window id.
  DCHECK(tab);

  tab->SetWindowSessionID(GetSessionId());
}

int TabModelJniBridge::GetTabCount() const {
  JNIEnv* env = AttachCurrentThread();
  return Java_TabModelJniBridge_getCount(env, java_object_.get(env).obj());
}

int TabModelJniBridge::GetActiveIndex() const {
  JNIEnv* env = AttachCurrentThread();
  return Java_TabModelJniBridge_index(env, java_object_.get(env).obj());
}

void TabModelJniBridge::CreateTab(WebContents* web_contents,
                                  int parent_tab_id) {
  JNIEnv* env = AttachCurrentThread();
  Java_TabModelJniBridge_createTabWithWebContents(
      env, java_object_.get(env).obj(),
      web_contents->GetBrowserContext()->IsOffTheRecord(),
      web_contents->GetJavaWebContents().obj(),
      parent_tab_id);
}

WebContents* TabModelJniBridge::GetWebContentsAt(int index) const {
  TabAndroid* tab = GetTabAt(index);
  return tab == NULL ? NULL : tab->web_contents();
}

TabAndroid* TabModelJniBridge::GetTabAt(int index) const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jtab =
      Java_TabModelJniBridge_getTabAt(env,
                                      java_object_.get(env).obj(),
                                      index);

  return jtab.is_null() ?
      NULL : TabAndroid::GetNativeTab(env, jtab.obj());
}

void TabModelJniBridge::SetActiveIndex(int index) {
  JNIEnv* env = AttachCurrentThread();
  Java_TabModelJniBridge_setIndex(env, java_object_.get(env).obj(), index);
}

void TabModelJniBridge::CloseTabAt(int index) {
  JNIEnv* env = AttachCurrentThread();
  Java_TabModelJniBridge_closeTabAt(env,
                                    java_object_.get(env).obj(),
                                    index);
}

WebContents* TabModelJniBridge::CreateNewTabForDevTools(
    const GURL& url) {
  // TODO(dfalcantara): Change the Java side so that it creates and returns the
  //                    WebContents, which we can load the URL on and return.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jurl = ConvertUTF8ToJavaString(env, url.spec());
  ScopedJavaLocalRef<jobject> obj =
      Java_TabModelJniBridge_createNewTabForDevTools(
          env,
          java_object_.get(env).obj(),
          jurl.obj());
  if (obj.is_null()) {
    VLOG(0) << "Failed to create java tab";
    return NULL;
  }
  TabAndroid* tab = TabAndroid::GetNativeTab(env, obj.obj());
  if (!tab) {
    VLOG(0) << "Failed to create java tab";
    return NULL;
  }
  return tab->web_contents();
}

bool TabModelJniBridge::IsSessionRestoreInProgress() const {
  JNIEnv* env = AttachCurrentThread();
  return Java_TabModelJniBridge_isSessionRestoreInProgress(
      env, java_object_.get(env).obj());
}

void TabModelJniBridge::BroadcastSessionRestoreComplete(JNIEnv* env,
                                                        jobject obj) {
  TabModel::BroadcastSessionRestoreComplete();
}

inline static base::TimeDelta GetTimeDelta(jlong ms) {
  return base::TimeDelta::FromMilliseconds(static_cast<int64>(ms));
}

void LogFromCloseMetric(JNIEnv* env,
                        jclass jcaller,
                        jlong ms,
                        jboolean perceived) {
  if (perceived) {
    UMA_HISTOGRAM_TIMES("Tabs.SwitchFromCloseLatency_Perceived",
                        GetTimeDelta(ms));
  } else {
    UMA_HISTOGRAM_TIMES("Tabs.SwitchFromCloseLatency_Actual",
                        GetTimeDelta(ms));
  }
}

void LogFromExitMetric(JNIEnv* env,
                       jclass jcaller,
                       jlong ms,
                       jboolean perceived) {
  if (perceived) {
    UMA_HISTOGRAM_TIMES("Tabs.SwitchFromExitLatency_Perceived",
                        GetTimeDelta(ms));
  } else {
    UMA_HISTOGRAM_TIMES("Tabs.SwitchFromExitLatency_Actual",
                        GetTimeDelta(ms));
  }
}

void LogFromNewMetric(JNIEnv* env,
                      jclass jcaller,
                      jlong ms,
                      jboolean perceived) {
  if (perceived) {
    UMA_HISTOGRAM_TIMES("Tabs.SwitchFromNewLatency_Perceived",
                        GetTimeDelta(ms));
  } else {
    UMA_HISTOGRAM_TIMES("Tabs.SwitchFromNewLatency_Actual",
                        GetTimeDelta(ms));
  }
}

void LogFromUserMetric(JNIEnv* env,
                       jclass jcaller,
                       jlong ms,
                       jboolean perceived) {
  if (perceived) {
    UMA_HISTOGRAM_TIMES("Tabs.SwitchFromUserLatency_Perceived",
                        GetTimeDelta(ms));
  } else {
    UMA_HISTOGRAM_TIMES("Tabs.SwitchFromUserLatency_Actual",
                        GetTimeDelta(ms));
  }
}

TabModelJniBridge::~TabModelJniBridge() {
  TabModelList::RemoveTabModel(this);
}

bool TabModelJniBridge::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static jlong Init(JNIEnv* env, jobject obj, jboolean is_incognito) {
  TabModel* tab_model = new TabModelJniBridge(env, obj, is_incognito);
  return reinterpret_cast<intptr_t>(tab_model);
}
