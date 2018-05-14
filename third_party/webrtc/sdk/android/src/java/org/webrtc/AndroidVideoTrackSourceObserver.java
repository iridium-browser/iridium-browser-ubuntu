/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

/** An implementation of CapturerObserver that forwards all calls from Java to the C layer. */
@JNINamespace("webrtc::jni")
class AndroidVideoTrackSourceObserver implements VideoCapturer.CapturerObserver {
  // Pointer to VideoTrackSourceProxy proxying AndroidVideoTrackSource.
  private final long nativeSource;

  public AndroidVideoTrackSourceObserver(long nativeSource) {
    this.nativeSource = nativeSource;
  }

  @Override
  public void onCapturerStarted(boolean success) {
    nativeCapturerStarted(nativeSource, success);
  }

  @Override
  public void onCapturerStopped() {
    nativeCapturerStopped(nativeSource);
  }

  @Override
  @SuppressWarnings("deprecation")
  public void onByteBufferFrameCaptured(
      byte[] data, int width, int height, int rotation, long timeStamp) {
    nativeOnByteBufferFrameCaptured(
        nativeSource, data, data.length, width, height, rotation, timeStamp);
  }

  @Override
  @SuppressWarnings("deprecation")
  public void onTextureFrameCaptured(int width, int height, int oesTextureId,
      float[] transformMatrix, int rotation, long timestamp) {
    nativeOnTextureFrameCaptured(
        nativeSource, width, height, oesTextureId, transformMatrix, rotation, timestamp);
  }

  @Override
  public void onFrameCaptured(VideoFrame frame) {
    nativeOnFrameCaptured(nativeSource, frame.getBuffer().getWidth(), frame.getBuffer().getHeight(),
        frame.getRotation(), frame.getTimestampNs(), frame.getBuffer());
  }

  private static native void nativeCapturerStarted(long source, boolean success);
  private static native void nativeCapturerStopped(long source);
  private static native void nativeOnByteBufferFrameCaptured(
      long source, byte[] data, int length, int width, int height, int rotation, long timeStamp);
  private static native void nativeOnTextureFrameCaptured(long source, int width, int height,
      int oesTextureId, float[] transformMatrix, int rotation, long timestamp);
  private static native void nativeOnFrameCaptured(
      long source, int width, int height, int rotation, long timestampNs, VideoFrame.Buffer frame);
}
