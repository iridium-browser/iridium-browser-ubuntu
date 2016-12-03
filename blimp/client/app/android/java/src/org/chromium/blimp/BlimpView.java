// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Point;
import android.os.Build;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blimp.session.BlimpClientSession;
import org.chromium.ui.UiUtils;

/**
 * A {@link View} that will visually represent the Blimp rendered content.  This {@link View} starts
 * a native compositor.
 */
@JNINamespace("blimp::client")
public class BlimpView
        extends SurfaceView implements SurfaceHolder.Callback, View.OnLayoutChangeListener {
    private long mNativeBlimpViewPtr;

    /**
     * Builds a new {@link BlimpView}.
     * @param context A {@link Context} instance.
     * @param attrs   An {@link AttributeSet} instance.
     */
    public BlimpView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setFocusable(true);
        setFocusableInTouchMode(true);
        addOnLayoutChangeListener(this);
    }

    /**
     * Starts up rendering for this {@link View}.  This will start up the native compositor and will
     * display it's contents.
     * @param blimpClientSession The {@link BlimpClientSession} that contains the content-lite
     *                           features required by the native components of the compositor.
     */
    public void initializeRenderer(BlimpClientSession blimpClientSession) {
        assert mNativeBlimpViewPtr == 0;

        WindowManager windowManager =
                (WindowManager) getContext().getSystemService(Context.WINDOW_SERVICE);
        Point displaySize = new Point();
        windowManager.getDefaultDisplay().getSize(displaySize);
        Point physicalSize = new Point();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            windowManager.getDefaultDisplay().getRealSize(physicalSize);
        }
        float deviceScaleFactor = getContext().getResources().getDisplayMetrics().density;
        mNativeBlimpViewPtr = nativeInit(blimpClientSession, physicalSize.x, physicalSize.y,
                displaySize.x, displaySize.y, deviceScaleFactor);
        getHolder().addCallback(this);
        setBackgroundColor(Color.WHITE);
        setVisibility(VISIBLE);
    }

    /**
     * Stops rendering for this {@link View} and destroys all internal state.  This {@link View}
     * should not be used after this.
     */
    public void destroyRenderer() {
        getHolder().removeCallback(this);
        if (mNativeBlimpViewPtr != 0) {
            nativeDestroy(mNativeBlimpViewPtr);
            mNativeBlimpViewPtr = 0;
        }
    }

    // View.OnLayoutChangeListener implementation.
    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        if (mNativeBlimpViewPtr == 0) return;
        nativeOnContentAreaSizeChanged(mNativeBlimpViewPtr, right - left, bottom - top,
                getContext().getResources().getDisplayMetrics().density);
    }

    // View overrides.
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (mNativeBlimpViewPtr == 0) return false;

        int eventAction = event.getActionMasked();

        // Close the IME. It might be open for typing URL into toolbar.
        // TODO(shaktisahu): Detect if the IME was open and return immediately (crbug/606977)
        UiUtils.hideKeyboard(this);

        if (!isValidTouchEventActionForNative(eventAction)) return false;

        int pointerCount = event.getPointerCount();

        float[] touchMajor = {event.getTouchMajor(), pointerCount > 1 ? event.getTouchMajor(1) : 0};
        float[] touchMinor = {event.getTouchMinor(), pointerCount > 1 ? event.getTouchMinor(1) : 0};

        for (int i = 0; i < 2; i++) {
            if (touchMajor[i] < touchMinor[i]) {
                float tmp = touchMajor[i];
                touchMajor[i] = touchMinor[i];
                touchMinor[i] = tmp;
            }
        }

        boolean consumed = nativeOnTouchEvent(mNativeBlimpViewPtr, event,
                event.getEventTime(), eventAction,
                pointerCount, event.getHistorySize(), event.getActionIndex(),
                event.getX(), event.getY(),
                pointerCount > 1 ? event.getX(1) : 0,
                pointerCount > 1 ? event.getY(1) : 0,
                event.getPointerId(0), pointerCount > 1 ? event.getPointerId(1) : -1,
                touchMajor[0], touchMajor[1],
                touchMinor[0], touchMinor[1],
                event.getOrientation(), pointerCount > 1 ? event.getOrientation(1) : 0,
                event.getAxisValue(MotionEvent.AXIS_TILT),
                pointerCount > 1 ? event.getAxisValue(MotionEvent.AXIS_TILT, 1) : 0,
                event.getRawX(), event.getRawY(),
                event.getToolType(0),
                pointerCount > 1 ? event.getToolType(1) : MotionEvent.TOOL_TYPE_UNKNOWN,
                event.getButtonState(),
                event.getMetaState());

        return consumed;
    }

    // SurfaceView overrides.
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        setZOrderMediaOverlay(true);
        setVisibility(GONE);
    }

    // SurfaceHolder.Callback2 interface.
    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        if (mNativeBlimpViewPtr == 0) return;
        nativeOnSurfaceChanged(mNativeBlimpViewPtr, format, width, height, holder.getSurface());
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        if (mNativeBlimpViewPtr == 0) return;
        nativeOnSurfaceCreated(mNativeBlimpViewPtr);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        if (mNativeBlimpViewPtr == 0) return;
        nativeOnSurfaceDestroyed(mNativeBlimpViewPtr);
    }

    private static boolean isValidTouchEventActionForNative(int eventAction) {
        // Only these actions have any effect on gesture detection.  Other
        // actions have no corresponding WebTouchEvent type and may confuse the
        // touch pipline, so we ignore them entirely.
        return eventAction == MotionEvent.ACTION_DOWN
                || eventAction == MotionEvent.ACTION_UP
                || eventAction == MotionEvent.ACTION_CANCEL
                || eventAction == MotionEvent.ACTION_MOVE
                || eventAction == MotionEvent.ACTION_POINTER_DOWN
                || eventAction == MotionEvent.ACTION_POINTER_UP;
    }

    @CalledByNative
    public void onSwapBuffersCompleted() {
        if (getBackground() == null) return;

        setBackgroundResource(0);
    }

    // Native Methods
    private native long nativeInit(BlimpClientSession blimpClientSession, int physicalWidth,
            int physicalHeight, int displayWidth, int displayHeight, float dpToPixel);
    private native void nativeDestroy(long nativeBlimpView);
    private native void nativeOnContentAreaSizeChanged(
            long nativeBlimpView, int width, int height, float dpToPx);
    private native void nativeOnSurfaceChanged(
            long nativeBlimpView, int format, int width, int height, Surface surface);
    private native void nativeOnSurfaceCreated(long nativeBlimpView);
    private native void nativeOnSurfaceDestroyed(long nativeBlimpView);
    private native boolean nativeOnTouchEvent(
            long nativeBlimpView, MotionEvent event,
            long timeMs, int action, int pointerCount, int historySize, int actionIndex,
            float x0, float y0, float x1, float y1,
            int pointerId0, int pointerId1,
            float touchMajor0, float touchMajor1,
            float touchMinor0, float touchMinor1,
            float orientation0, float orientation1,
            float tilt0, float tilt1,
            float rawX, float rawY,
            int androidToolType0, int androidToolType1,
            int androidButtonState, int androidMetaState);
}
