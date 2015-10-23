/*
 * libjingle
 * Copyright 2014 Google Inc.
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
 */

package org.webrtc;

import android.graphics.SurfaceTexture;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCodecList;
import android.media.MediaFormat;
import android.opengl.EGL14;
import android.opengl.EGLContext;
import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import android.os.Build;
import android.util.Log;
import android.view.Surface;

import java.nio.ByteBuffer;

// Java-side of peerconnection_jni.cc:MediaCodecVideoDecoder.
// This class is an implementation detail of the Java PeerConnection API.
// MediaCodec is thread-hostile so this class must be operated on a single
// thread.
public class MediaCodecVideoDecoder {
  // This class is constructed, operated, and destroyed by its C++ incarnation,
  // so the class and its methods have non-public visibility.  The API this
  // class exposes aims to mimic the webrtc::VideoDecoder API as closely as
  // possibly to minimize the amount of translation work necessary.

  private static final String TAG = "MediaCodecVideoDecoder";

  // Tracks webrtc::VideoCodecType.
  public enum VideoCodecType {
    VIDEO_CODEC_VP8,
    VIDEO_CODEC_VP9,
    VIDEO_CODEC_H264
  }

  private static final int DEQUEUE_INPUT_TIMEOUT = 500000;  // 500 ms timeout.
  private Thread mediaCodecThread;
  private MediaCodec mediaCodec;
  private ByteBuffer[] inputBuffers;
  private ByteBuffer[] outputBuffers;
  private static final String VP8_MIME_TYPE = "video/x-vnd.on2.vp8";
  private static final String H264_MIME_TYPE = "video/avc";
  // List of supported HW VP8 decoders.
  private static final String[] supportedVp8HwCodecPrefixes =
    {"OMX.qcom.", "OMX.Nvidia.", "OMX.Exynos.", "OMX.Intel." };
  // List of supported HW H.264 decoders.
  private static final String[] supportedH264HwCodecPrefixes =
    {"OMX.qcom.", "OMX.Intel." };
  // NV12 color format supported by QCOM codec, but not declared in MediaCodec -
  // see /hardware/qcom/media/mm-core/inc/OMX_QCOMExtns.h
  private static final int
    COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m = 0x7FA30C04;
  // Allowable color formats supported by codec - in order of preference.
  private static final int[] supportedColorList = {
    CodecCapabilities.COLOR_FormatYUV420Planar,
    CodecCapabilities.COLOR_FormatYUV420SemiPlanar,
    CodecCapabilities.COLOR_QCOM_FormatYUV420SemiPlanar,
    COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m
  };
  private int colorFormat;
  private int width;
  private int height;
  private int stride;
  private int sliceHeight;
  private boolean useSurface;
  private int textureID = -1;
  private SurfaceTexture surfaceTexture = null;
  private Surface surface = null;
  private EglBase eglBase;

  private MediaCodecVideoDecoder() { }

  // Helper struct for findVp8Decoder() below.
  private static class DecoderProperties {
    public DecoderProperties(String codecName, int colorFormat) {
      this.codecName = codecName;
      this.colorFormat = colorFormat;
    }
    public final String codecName; // OpenMax component name for VP8 codec.
    public final int colorFormat;  // Color format supported by codec.
  }

  private static DecoderProperties findDecoder(
      String mime, String[] supportedCodecPrefixes) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
      return null; // MediaCodec.setParameters is missing.
    }
    for (int i = 0; i < MediaCodecList.getCodecCount(); ++i) {
      MediaCodecInfo info = MediaCodecList.getCodecInfoAt(i);
      if (info.isEncoder()) {
        continue;
      }
      String name = null;
      for (String mimeType : info.getSupportedTypes()) {
        if (mimeType.equals(mime)) {
          name = info.getName();
          break;
        }
      }
      if (name == null) {
        continue;  // No HW support in this codec; try the next one.
      }
      Log.v(TAG, "Found candidate decoder " + name);

      // Check if this is supported decoder.
      boolean supportedCodec = false;
      for (String codecPrefix : supportedCodecPrefixes) {
        if (name.startsWith(codecPrefix)) {
          supportedCodec = true;
          break;
        }
      }
      if (!supportedCodec) {
        continue;
      }

      // Check if codec supports either yuv420 or nv12.
      CodecCapabilities capabilities =
          info.getCapabilitiesForType(mime);
      for (int colorFormat : capabilities.colorFormats) {
        Log.v(TAG, "   Color: 0x" + Integer.toHexString(colorFormat));
      }
      for (int supportedColorFormat : supportedColorList) {
        for (int codecColorFormat : capabilities.colorFormats) {
          if (codecColorFormat == supportedColorFormat) {
            // Found supported HW decoder.
            Log.d(TAG, "Found target decoder " + name +
                ". Color: 0x" + Integer.toHexString(codecColorFormat));
            return new DecoderProperties(name, codecColorFormat);
          }
        }
      }
    }
    return null;  // No HW decoder.
  }

  public static boolean isVp8HwSupported() {
    return findDecoder(VP8_MIME_TYPE, supportedVp8HwCodecPrefixes) != null;
  }

  public static boolean isH264HwSupported() {
    return findDecoder(H264_MIME_TYPE, supportedH264HwCodecPrefixes) != null;
  }

  private void checkOnMediaCodecThread() {
    if (mediaCodecThread.getId() != Thread.currentThread().getId()) {
      throw new RuntimeException(
          "MediaCodecVideoDecoder previously operated on " + mediaCodecThread +
          " but is now called on " + Thread.currentThread());
    }
  }

  private boolean initDecode(
      VideoCodecType type, int width, int height,
      boolean useSurface, EGLContext sharedContext) {
    if (mediaCodecThread != null) {
      throw new RuntimeException("Forgot to release()?");
    }
    if (useSurface && sharedContext == null) {
      throw new RuntimeException("No shared EGL context.");
    }
    String mime = null;
    String[] supportedCodecPrefixes = null;
    if (type == VideoCodecType.VIDEO_CODEC_VP8) {
      mime = VP8_MIME_TYPE;
      supportedCodecPrefixes = supportedVp8HwCodecPrefixes;
    } else if (type == VideoCodecType.VIDEO_CODEC_H264) {
      mime = H264_MIME_TYPE;
      supportedCodecPrefixes = supportedH264HwCodecPrefixes;
    } else {
      throw new RuntimeException("Non supported codec " + type);
    }
    DecoderProperties properties = findDecoder(mime, supportedCodecPrefixes);
    if (properties == null) {
      throw new RuntimeException("Cannot find HW decoder for " + type);
    }
    Log.d(TAG, "Java initDecode: " + type + " : "+ width + " x " + height +
        ". Color: 0x" + Integer.toHexString(properties.colorFormat) +
        ". Use Surface: " + useSurface);
    if (sharedContext != null) {
      Log.d(TAG, "Decoder shared EGL Context: " + sharedContext);
    }
    mediaCodecThread = Thread.currentThread();
    try {
      Surface decodeSurface = null;
      this.width = width;
      this.height = height;
      this.useSurface = useSurface;
      stride = width;
      sliceHeight = height;

      if (useSurface) {
        // Create shared EGL context.
        eglBase = new EglBase(sharedContext, EglBase.ConfigType.PIXEL_BUFFER);
        eglBase.createDummyPbufferSurface();
        eglBase.makeCurrent();

        // Create output surface
        int[] textures = new int[1];
        GLES20.glGenTextures(1, textures, 0);
        GlUtil.checkNoGLES2Error("glGenTextures");
        textureID = textures[0];
        GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, textureID);
        GlUtil.checkNoGLES2Error("glBindTexture mTextureID");

        GLES20.glTexParameterf(GLES11Ext.GL_TEXTURE_EXTERNAL_OES,
            GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_NEAREST);
        GLES20.glTexParameterf(GLES11Ext.GL_TEXTURE_EXTERNAL_OES,
            GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
        GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES,
            GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
        GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES,
            GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);
        GlUtil.checkNoGLES2Error("glTexParameter");
        Log.d(TAG, "Video decoder TextureID = " + textureID);
        surfaceTexture = new SurfaceTexture(textureID);
        surface = new Surface(surfaceTexture);
        decodeSurface = surface;
      }

      MediaFormat format = MediaFormat.createVideoFormat(mime, width, height);
      if (!useSurface) {
        format.setInteger(MediaFormat.KEY_COLOR_FORMAT, properties.colorFormat);
      }
      Log.d(TAG, "  Format: " + format);
      mediaCodec =
          MediaCodecVideoEncoder.createByCodecName(properties.codecName);
      if (mediaCodec == null) {
        return false;
      }
      mediaCodec.configure(format, decodeSurface, null, 0);
      mediaCodec.start();
      colorFormat = properties.colorFormat;
      outputBuffers = mediaCodec.getOutputBuffers();
      inputBuffers = mediaCodec.getInputBuffers();
      Log.d(TAG, "Input buffers: " + inputBuffers.length +
          ". Output buffers: " + outputBuffers.length);
      return true;
    } catch (IllegalStateException e) {
      Log.e(TAG, "initDecode failed", e);
      return false;
    }
  }

  private void release() {
    Log.d(TAG, "Java releaseDecoder");
    checkOnMediaCodecThread();
    try {
      mediaCodec.stop();
      mediaCodec.release();
    } catch (IllegalStateException e) {
      Log.e(TAG, "release failed", e);
    }
    mediaCodec = null;
    mediaCodecThread = null;
    if (useSurface) {
      surface.release();
      if (textureID >= 0) {
        int[] textures = new int[1];
        textures[0] = textureID;
        Log.d(TAG, "Delete video decoder TextureID " + textureID);
        GLES20.glDeleteTextures(1, textures, 0);
        GlUtil.checkNoGLES2Error("glDeleteTextures");
      }
      eglBase.release();
      eglBase = null;
    }
  }

  // Dequeue an input buffer and return its index, -1 if no input buffer is
  // available, or -2 if the codec is no longer operative.
  private int dequeueInputBuffer() {
    checkOnMediaCodecThread();
    try {
      return mediaCodec.dequeueInputBuffer(DEQUEUE_INPUT_TIMEOUT);
    } catch (IllegalStateException e) {
      Log.e(TAG, "dequeueIntputBuffer failed", e);
      return -2;
    }
  }

  private boolean queueInputBuffer(
      int inputBufferIndex, int size, long timestampUs) {
    checkOnMediaCodecThread();
    try {
      inputBuffers[inputBufferIndex].position(0);
      inputBuffers[inputBufferIndex].limit(size);
      mediaCodec.queueInputBuffer(inputBufferIndex, 0, size, timestampUs, 0);
      return true;
    }
    catch (IllegalStateException e) {
      Log.e(TAG, "decode failed", e);
      return false;
    }
  }

  // Helper struct for dequeueOutputBuffer() below.
  private static class DecoderOutputBufferInfo {
    public DecoderOutputBufferInfo(
        int index, int offset, int size, long presentationTimestampUs) {
      this.index = index;
      this.offset = offset;
      this.size = size;
      this.presentationTimestampUs = presentationTimestampUs;
    }

    private final int index;
    private final int offset;
    private final int size;
    private final long presentationTimestampUs;
  }

  // Dequeue and return an output buffer index, -1 if no output
  // buffer available or -2 if error happened.
  private DecoderOutputBufferInfo dequeueOutputBuffer(int dequeueTimeoutUs) {
    checkOnMediaCodecThread();
    try {
      MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
      int result = mediaCodec.dequeueOutputBuffer(info, dequeueTimeoutUs);
      while (result == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED ||
          result == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
        if (result == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
          outputBuffers = mediaCodec.getOutputBuffers();
          Log.d(TAG, "Decoder output buffers changed: " + outputBuffers.length);
        } else if (result == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
          MediaFormat format = mediaCodec.getOutputFormat();
          Log.d(TAG, "Decoder format changed: " + format.toString());
          width = format.getInteger(MediaFormat.KEY_WIDTH);
          height = format.getInteger(MediaFormat.KEY_HEIGHT);
          if (!useSurface && format.containsKey(MediaFormat.KEY_COLOR_FORMAT)) {
            colorFormat = format.getInteger(MediaFormat.KEY_COLOR_FORMAT);
            Log.d(TAG, "Color: 0x" + Integer.toHexString(colorFormat));
            // Check if new color space is supported.
            boolean validColorFormat = false;
            for (int supportedColorFormat : supportedColorList) {
              if (colorFormat == supportedColorFormat) {
                validColorFormat = true;
                break;
              }
            }
            if (!validColorFormat) {
              Log.e(TAG, "Non supported color format");
              return new DecoderOutputBufferInfo(-1, 0, 0, -1);
            }
          }
          if (format.containsKey("stride")) {
            stride = format.getInteger("stride");
          }
          if (format.containsKey("slice-height")) {
            sliceHeight = format.getInteger("slice-height");
          }
          Log.d(TAG, "Frame stride and slice height: "
              + stride + " x " + sliceHeight);
          stride = Math.max(width, stride);
          sliceHeight = Math.max(height, sliceHeight);
        }
        result = mediaCodec.dequeueOutputBuffer(info, dequeueTimeoutUs);
      }
      if (result >= 0) {
        return new DecoderOutputBufferInfo(result, info.offset, info.size,
            info.presentationTimeUs);
      }
      return null;
    } catch (IllegalStateException e) {
      Log.e(TAG, "dequeueOutputBuffer failed", e);
      return new DecoderOutputBufferInfo(-1, 0, 0, -1);
    }
  }

  // Release a dequeued output buffer back to the codec for re-use.  Return
  // false if the codec is no longer operable.
  private boolean releaseOutputBuffer(int index, boolean render) {
    checkOnMediaCodecThread();
    try {
      if (!useSurface) {
        render = false;
      }
      mediaCodec.releaseOutputBuffer(index, render);
      return true;
    } catch (IllegalStateException e) {
      Log.e(TAG, "releaseOutputBuffer failed", e);
      return false;
    }
  }
}
