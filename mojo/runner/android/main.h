// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_RUNNER_ANDROID_MAIN_H_
#define MOJO_RUNNER_ANDROID_MAIN_H_

#include <jni.h>

namespace mojo {
namespace runner {

bool RegisterShellMain(JNIEnv* env);

}  // namespace runner
}  // namespace mojo

#endif  // MOJO_RUNNER_ANDROID_MAIN_H_
