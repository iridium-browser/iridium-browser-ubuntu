// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BROWSING_DATA_BROWSING_DATA_COUNTER_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_BROWSING_DATA_BROWSING_DATA_COUNTER_BRIDGE_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"

class Profile;

// This class is a wrapper for BrowsingDataCounter (C++ backend) to be used by
// ClearBrowsingDataFragment (Java UI).
class BrowsingDataCounterBridge {
 public:
  // Creates a BrowsingDataCounterBridge for a certain browsing data type.
  // The |data_type| is a value of the enum BrowsingDataType.
  BrowsingDataCounterBridge(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint data_type);

  ~BrowsingDataCounterBridge();

  // Called by the Java counterpart when it is getting garbage collected.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  static bool Register(JNIEnv* env);

 private:
  void onCounterFinished(
      std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result);

  base::android::ScopedJavaGlobalRef<jobject> jobject_;
  std::unique_ptr<browsing_data::BrowsingDataCounter> counter_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataCounterBridge);
};

#endif // CHROME_BROWSER_ANDROID_BROWSING_DATA_BROWSING_DATA_COUNTER_BRIDGE_H_
