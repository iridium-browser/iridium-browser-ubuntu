/*
 * libjingle
 * Copyright 2015 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef TALK_APP_WEBRTC_JAVA_JNI_ANDROIDVIDEOCAPTURER_JNI_H_
#define TALK_APP_WEBRTC_JAVA_JNI_ANDROIDVIDEOCAPTURER_JNI_H_

#include <string>

#include "talk/app/webrtc/androidvideocapturer.h"
#include "talk/app/webrtc/java/jni/jni_helpers.h"
#include "webrtc/base/asyncinvoker.h"
#include "webrtc/base/thread_checker.h"

namespace webrtc_jni {

// AndroidVideoCapturerJni implements AndroidVideoCapturerDelegate.
// The purpose of the delegate is to hide the JNI specifics from the C++ only
// AndroidVideoCapturer.
class AndroidVideoCapturerJni : public webrtc::AndroidVideoCapturerDelegate {
 public:
  static int SetAndroidObjects(JNIEnv* jni, jobject appliction_context);

  // Creates a new instance of AndroidVideoCapturerJni. Returns a nullptr if
  // it can't be created. This happens if |device_name| is invalid.
  static rtc::scoped_refptr<AndroidVideoCapturerJni> Create(
      JNIEnv* jni,
      jobject j_video_capture, // Instance of VideoCapturerAndroid
      jstring device_name); // Name of the camera to use.

  void Start(int width, int height, int framerate,
             webrtc::AndroidVideoCapturer* capturer) override;
  void Stop() override;

  void ReturnBuffer(int64 time_stamp) override;

  std::string GetSupportedFormats() override;

  // Called from VideoCapturerAndroid::NativeObserver on a Java thread.
  void OnCapturerStarted(bool success);
  void OnIncomingFrame(void* video_frame,
                       int length,
                       int width,
                       int height,
                       int rotation,
                       int64 time_stamp);
  void OnOutputFormatRequest(int width, int height, int fps);
protected:
  AndroidVideoCapturerJni(JNIEnv* jni, jobject j_video_capturer);
  ~AndroidVideoCapturerJni();

private:
  bool Init(jstring device_name);

  void OnCapturerStarted_w(bool success);
  void OnCapturerStopped_w();
  void OnIncomingFrame_w(void* video_frame,
                         int length,
                         int width,
                         int height,
                         int rotation,
                         int64 time_stamp);
  void OnOutputFormatRequest_w(int width, int height, int fps);
  void ReturnBuffer_w(int64 time_stamp);

  JNIEnv* jni();

  const ScopedGlobalRef<jobject> j_capturer_global_;
  const ScopedGlobalRef<jclass> j_video_capturer_class_;
  const ScopedGlobalRef<jclass> j_observer_class_;
  volatile bool valid_global_refs_;
  jobject j_frame_observer_;

  rtc::ThreadChecker thread_checker_;

  rtc::Thread* thread_;  // The thread where Start is called on.
  // |capturer| is a guaranteed to be a valid pointer between a call to
  // AndroidVideoCapturerDelegate::Start
  // until AndroidVideoCapturerDelegate::Stop.
  webrtc::AndroidVideoCapturer* capturer_;
  rtc::AsyncInvoker invoker_;

  static jobject application_context_;

  DISALLOW_COPY_AND_ASSIGN(AndroidVideoCapturerJni);
};

}  // namespace webrtc_jni

#endif  // TALK_APP_WEBRTC_JAVA_JNI_ANDROIDVIDEOCAPTURER_JNI_H_
