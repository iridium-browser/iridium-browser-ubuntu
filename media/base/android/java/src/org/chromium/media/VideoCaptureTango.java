// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.content.Context;
import android.graphics.ImageFormat;

import org.chromium.base.Log;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;

/**
 * This class extends the VideoCapture base class for manipulating a Tango
 * device's cameras, namely the associated Depth (z-Buffer), Fisheye and back-
 * facing 4MP video capture devices. These devices are differentiated via the
 * |id| passed on constructor, according to the index correspondence in
 * |s_CAM_PARAMS|; all devices |id| are index 0 towards the parent VideoCapture.
 **/
@SuppressWarnings("deprecation")
public class VideoCaptureTango extends VideoCaptureCamera {

    private static class CamParams {
        final int mId;
        final String mName;
        final int mWidth;
        final int mHeight;

        CamParams(int id, String name, int width, int height) {
            mId = id;
            mName = name;
            mWidth = width;
            mHeight = height;
        }
    }

    private ByteBuffer mFrameBuffer = null;
    private final int mTangoCameraId;
    // The indexes must coincide with |CAM_PARAMS| defined below.
    private static final int DEPTH_CAMERA_ID = 0;
    private static final int FISHEYE_CAMERA_ID = 1;
    private static final int FOURMP_CAMERA_ID = 2;
    private static final CamParams CAM_PARAMS[] = {
            new CamParams(DEPTH_CAMERA_ID, "depth", 320, 240),
            new CamParams(FISHEYE_CAMERA_ID, "fisheye", 640, 480),
            new CamParams(FOURMP_CAMERA_ID, "4MP", 1280, 720) };

    // SuperFrame size definitions. Note that total size is the amount of lines
    // multiplied by 3/2 due to Chroma components following.
    private static final int SF_WIDTH = 1280;
    private static final int SF_HEIGHT = 1168;
    private static final int SF_FULL_HEIGHT = SF_HEIGHT * 3 / 2;
    private static final int SF_LINES_HEADER = 16;
    private static final int SF_LINES_FISHEYE = 240;
    private static final int SF_LINES_RESERVED = 80;  // Spec says 96.
    private static final int SF_LINES_DEPTH = 60;
    private static final int SF_LINES_DEPTH_PADDED = 112;  // Spec says 96.
    private static final int SF_LINES_BIGIMAGE = 720;
    private static final int SF_OFFSET_4MP_CHROMA = 112;

    private static final byte CHROMA_ZERO_LEVEL = 127;
    private static final String TAG = "cr.media";

    static int numberOfCameras() {
        return CAM_PARAMS.length;
    }

    static int getCaptureApiType(int index) {
        if (index >= CAM_PARAMS.length) {
            return CaptureApiType.API1;
        }
        return CaptureApiType.TANGO;
    }

    static String getName(int index) {
        if (index >= CAM_PARAMS.length) return "";
        return CAM_PARAMS[index].mName;
    }

    static VideoCaptureFormat[] getDeviceSupportedFormats(int id) {
        ArrayList<VideoCaptureFormat> formatList = new ArrayList<VideoCaptureFormat>();
        if (id == DEPTH_CAMERA_ID) {
            formatList.add(new VideoCaptureFormat(320, 180, 5, ImageFormat.YV12));
        } else if (id == FISHEYE_CAMERA_ID) {
            formatList.add(new VideoCaptureFormat(640, 480, 30, ImageFormat.YV12));
        } else if (id == FOURMP_CAMERA_ID) {
            formatList.add(new VideoCaptureFormat(1280, 720, 20, ImageFormat.YV12));
        }
        return formatList.toArray(new VideoCaptureFormat[formatList.size()]);
    }

    VideoCaptureTango(Context context,
                      int id,
                      long nativeVideoCaptureDeviceAndroid) {
        // All Tango cameras are like the back facing one for the generic VideoCapture code.
        super(context, 0, nativeVideoCaptureDeviceAndroid);
        mTangoCameraId = id;
    }

    @Override
    protected void setCaptureParameters(
            int width,
            int height,
            int frameRate,
            android.hardware.Camera.Parameters cameraParameters) {
        mCaptureFormat = new VideoCaptureFormat(CAM_PARAMS[mTangoCameraId].mWidth,
                                           CAM_PARAMS[mTangoCameraId].mHeight,
                                           frameRate,
                                           ImageFormat.YV12);
        // Connect Tango SuperFrame mode. Available sf modes are "all",
        // "big-rgb", "small-rgb", "depth", "ir".
        cameraParameters.set("sf-mode", "all");
    }

    @Override
    protected void allocateBuffers() {
        mFrameBuffer = ByteBuffer.allocateDirect(
                mCaptureFormat.mWidth * mCaptureFormat.mHeight * 3 / 2);
        // Prefill Chroma to their zero-equivalent for the cameras that only
        // provide Luma component.
        Arrays.fill(mFrameBuffer.array(), CHROMA_ZERO_LEVEL);
    }

    @Override
    protected void setPreviewCallback(android.hardware.Camera.PreviewCallback cb) {
        mCamera.setPreviewCallback(cb);
    }

    @Override
    public void onPreviewFrame(byte[] data, android.hardware.Camera camera) {
        mPreviewBufferLock.lock();
        try {
            if (!mIsRunning) return;
            if (data.length == SF_WIDTH * SF_FULL_HEIGHT) {
                if (mTangoCameraId == DEPTH_CAMERA_ID) {
                    int sizeY = SF_WIDTH * SF_LINES_DEPTH;
                    int startY =
                            SF_WIDTH * (SF_LINES_HEADER + SF_LINES_FISHEYE + SF_LINES_RESERVED);
                    // Depth is composed of 16b samples in which only 12b are
                    // used. Throw away lowest 4 resolution bits. Android
                    // platforms are big endian, LSB in lowest address. In this
                    // case Chroma components are unused. No need to write them
                    // explicitly since they're filled to 128 on creation.
                    byte depthsample;
                    for (int j = startY; j < startY + 2 * sizeY; j += 2) {
                        depthsample = (byte) ((data[j + 1] << 4) | ((data[j] & 0xF0) >> 4));
                        mFrameBuffer.put(depthsample);
                    }
                    for (int j = 0; j < mCaptureFormat.mWidth * mCaptureFormat.mHeight - sizeY;
                            ++j) {
                        mFrameBuffer.put((byte) 0);
                    }
                } else if (mTangoCameraId == FISHEYE_CAMERA_ID) {
                    int sizeY = SF_WIDTH * SF_LINES_FISHEYE;
                    int startY = SF_WIDTH * SF_LINES_HEADER;
                    // Fisheye is black and white so Chroma components are unused. No need to write
                    // them explicitly since they're filled to 128 on creation.
                    ByteBuffer.wrap(data, startY, sizeY).get(mFrameBuffer.array(), 0, sizeY);
                } else if (mTangoCameraId == FOURMP_CAMERA_ID) {
                    int startY = SF_WIDTH * (SF_LINES_HEADER + SF_LINES_FISHEYE
                                    + SF_LINES_RESERVED + SF_LINES_DEPTH_PADDED);
                    int sizeY = SF_WIDTH * SF_LINES_BIGIMAGE;

                    // The spec is completely inaccurate on the location, sizes
                    // and format of these channels.
                    int startU = SF_WIDTH * (SF_HEIGHT + SF_OFFSET_4MP_CHROMA);
                    int sizeU = SF_WIDTH * SF_LINES_BIGIMAGE / 4;
                    int startV = (SF_WIDTH * SF_HEIGHT * 5 / 4) + SF_WIDTH * SF_OFFSET_4MP_CHROMA;
                    int sizeV = SF_WIDTH * SF_LINES_BIGIMAGE / 4;

                    // Equivalent to the following |for| loop but much faster:
                    // for (int i = START; i < START + SIZE; ++i)
                    //     mFrameBuffer.put(data[i]);
                    ByteBuffer.wrap(data, startY, sizeY)
                              .get(mFrameBuffer.array(), 0, sizeY);
                    ByteBuffer.wrap(data, startU, sizeU)
                              .get(mFrameBuffer.array(), sizeY, sizeU);
                    ByteBuffer.wrap(data, startV, sizeV)
                              .get(mFrameBuffer.array(), sizeY + sizeU, sizeV);
                } else {
                    Log.e(TAG, "Unknown camera, #id: %d", mTangoCameraId);
                    return;
                }
                mFrameBuffer.rewind();  // Important!
                nativeOnFrameAvailable(mNativeVideoCaptureDeviceAndroid,
                                       mFrameBuffer.array(),
                                       mFrameBuffer.capacity(),
                                       getCameraRotation());
            }
        } finally {
            mPreviewBufferLock.unlock();
        }
    }
}
