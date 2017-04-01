// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_LOGO_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_LOGO_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/url_request/url_fetcher_delegate.h"

class LogoService;

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

// The C++ counterpart to LogoBridge.java. Enables Java code to access the
// default search provider's logo.
class LogoBridge : public net::URLFetcherDelegate {
 public:
  explicit LogoBridge(jobject j_profile);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void GetCurrentLogo(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_logo_observer);

  void GetAnimatedLogo(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       const base::android::JavaParamRef<jobject>& j_callback,
                       const base::android::JavaParamRef<jstring>& j_url);

  // net::URLFetcherDelegate:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

 private:
  ~LogoBridge() override;

  // Clears and resets the URLFetcher for animated logo.
  void ClearFetcher();

  LogoService* logo_service_;

  // The URLFetcher currently fetching the animated logo. NULL when not
  // fetching.
  std::unique_ptr<net::URLFetcher> fetcher_;

  // The timestamp for the last time the animated logo started downloading.
  base::TimeTicks animated_logo_download_start_time_;

  // The URLRequestContextGetter used to download the animated logo.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  base::android::ScopedJavaGlobalRef<jobject> j_callback_;

  base::WeakPtrFactory<LogoBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LogoBridge);
};

bool RegisterLogoBridge(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_LOGO_BRIDGE_H_
