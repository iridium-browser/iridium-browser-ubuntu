/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/videodecoderfactorywrapper.h"

#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder.h"
#include "common_types.h"  // NOLINT(build/include)
#include "rtc_base/logging.h"
#include "sdk/android/generated_video_jni/jni/VideoDecoderFactory_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/wrappednativecodec.h"

namespace webrtc {
namespace jni {

VideoDecoderFactoryWrapper::VideoDecoderFactoryWrapper(
    JNIEnv* jni,
    const JavaRef<jobject>& decoder_factory)
    : decoder_factory_(jni, decoder_factory) {}
VideoDecoderFactoryWrapper::~VideoDecoderFactoryWrapper() = default;

std::unique_ptr<VideoDecoder> VideoDecoderFactoryWrapper::CreateVideoDecoder(
    const SdpVideoFormat& format) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedJavaLocalRef<jobject> decoder = Java_VideoDecoderFactory_createDecoder(
      jni, decoder_factory_, NativeToJavaString(jni, format.name));
  if (!decoder.obj())
    return nullptr;
  return JavaToNativeVideoDecoder(jni, decoder);
}

std::vector<SdpVideoFormat> VideoDecoderFactoryWrapper::GetSupportedFormats()
    const {
  // TODO(andersc): VideoDecoderFactory.java does not have this method.
  return std::vector<SdpVideoFormat>();
}

}  // namespace jni
}  // namespace webrtc
