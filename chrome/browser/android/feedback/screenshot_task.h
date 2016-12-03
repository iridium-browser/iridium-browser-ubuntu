// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEEDBACK_SCREENSHOT_TASK_H_
#define CHROME_BROWSER_ANDROID_FEEDBACK_SCREENSHOT_TASK_H_

#include "base/android/jni_android.h"

namespace chrome {
namespace android {

bool RegisterScreenshotTask(JNIEnv* env);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_FEEDBACK_SCREENSHOT_TASK_H_
