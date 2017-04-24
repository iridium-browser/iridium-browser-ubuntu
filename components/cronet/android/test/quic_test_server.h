// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_TEST_QUIC_TEST_SERVER_H_
#define COMPONENTS_CRONET_ANDROID_TEST_QUIC_TEST_SERVER_H_

#include <jni.h>

namespace cronet {

bool RegisterQuicTestServer(JNIEnv* env);

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_TEST_QUIC_TEST_SERVER_H_
