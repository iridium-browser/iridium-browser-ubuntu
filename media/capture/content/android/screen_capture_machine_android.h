// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_CONTENT_ANDROID_SCREEN_CAPTURE_MACHINE_ANDROID_H_
#define MEDIA_CAPTURE_CONTENT_ANDROID_SCREEN_CAPTURE_MACHINE_ANDROID_H_

#include <jni.h>
#include <memory>

#include "base/android/scoped_java_ref.h"
#include "media/capture/content/screen_capture_device_core.h"

namespace media {

// ScreenCaptureMachineAndroid captures 32bit RGB or YUV420 triplanar.
class ScreenCaptureMachineAndroid : public media::VideoCaptureMachine {
 public:
  ScreenCaptureMachineAndroid();
  ~ScreenCaptureMachineAndroid() override;

  static bool RegisterScreenCaptureMachine(JNIEnv* env);
  static base::android::ScopedJavaLocalRef<jobject>
  createScreenCaptureMachineAndroid(jlong nativeScreenCaptureMachineAndroid);

  // Implement org.chromium.media.ScreenCapture.nativeOnRGBAFrameAvailable.
  void OnRGBAFrameAvailable(JNIEnv* env,
                            jobject obj,
                            jobject buf,
                            jint row_stride,
                            jint left,
                            jint top,
                            jint width,
                            jint height,
                            jlong timestamp);
  // Implement org.chromium.media.ScreenCapture.nativeOnI420FrameAvailable.
  void OnI420FrameAvailable(JNIEnv* env,
                            jobject obj,
                            jobject y_buffer,
                            jint y_stride,
                            jobject u_buffer,
                            jobject v_buffer,
                            jint uv_row_stride,
                            jint uv_pixel_stride,
                            jint left,
                            jint top,
                            jint width,
                            jint height,
                            jlong timestamp);

  // Implement org.chromium.media.ScreenCapture.nativeOnActivityResult.
  void OnActivityResult(JNIEnv* env, jobject obj, jboolean result);

  // VideoCaptureMachine overrides.
  void Start(const scoped_refptr<media::ThreadSafeCaptureOracle>& oracle_proxy,
             const media::VideoCaptureParams& params,
             const base::Callback<void(bool)> callback) override;
  void Stop(const base::Closure& callback) override;
  void MaybeCaptureForRefresh() override;

 private:
  // Makes all the decisions about which frames to copy, and how.
  scoped_refptr<media::ThreadSafeCaptureOracle> oracle_proxy_;

  // Cache the last frame for possible refreshing.
  scoped_refptr<VideoFrame> lastFrame_;

  // Java VideoCaptureAndroid instance.
  base::android::ScopedJavaLocalRef<jobject> j_capture_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCaptureMachineAndroid);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_CONTENT_ANDROID_SCREEN_CAPTURE_MACHINE_ANDROID_H_
