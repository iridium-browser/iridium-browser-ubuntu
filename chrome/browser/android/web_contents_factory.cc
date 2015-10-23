// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/web_contents_factory.h"

#include "base/android/jni_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/web_contents.h"
#include "jni/WebContentsFactory_jni.h"

static jobject CreateWebContents(
    JNIEnv* env, jclass clazz, jboolean incognito, jboolean initially_hidden) {
  Profile* profile = g_browser_process->profile_manager()->GetLastUsedProfile();
  if (incognito)
    profile = profile->GetOffTheRecordProfile();

  content::WebContents::CreateParams params(profile);
  params.initially_hidden = static_cast<bool>(initially_hidden);
  return content::WebContents::Create(params)->GetJavaWebContents().Release();
}

bool RegisterWebContentsFactory(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
