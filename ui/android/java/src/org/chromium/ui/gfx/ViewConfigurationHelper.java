// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.gfx;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.util.TypedValue;
import android.view.ViewConfiguration;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.ui.R;

/**
 * This class facilitates access to ViewConfiguration-related properties, also
 * providing native-code notifications when such properties have changed.
 *
 */
@JNINamespace("gfx")
public class ViewConfigurationHelper {

    // Fallback constants when resource lookup fails, see
    // ui/android/java/res/values/dimens.xml.
    private static final float MIN_SCALING_SPAN_MM = 27.0f;
    private static final float MIN_SCALING_TOUCH_MAJOR_DIP = 48.0f;

    private final Context mAppContext;
    private ViewConfiguration mViewConfiguration;
    private float mDensity;

    private ViewConfigurationHelper(Context context) {
        mAppContext = context.getApplicationContext();
        mViewConfiguration = ViewConfiguration.get(mAppContext);
        mDensity = mAppContext.getResources().getDisplayMetrics().density;
        assert mDensity > 0;
    }

    private void registerListener() {
        mAppContext.registerComponentCallbacks(
                new ComponentCallbacks() {
                    @Override
                    public void onConfigurationChanged(Configuration configuration) {
                        updateNativeViewConfigurationIfNecessary();
                    }

                    @Override
                    public void onLowMemory() {
                    }
                });
    }

    private void updateNativeViewConfigurationIfNecessary() {
        ViewConfiguration configuration = ViewConfiguration.get(mAppContext);
        if (mViewConfiguration == configuration) {
            // The density should remain the same as long as the ViewConfiguration remains the same.
            assert mDensity == mAppContext.getResources().getDisplayMetrics().density;
            return;
        }

        mViewConfiguration = configuration;
        mDensity = mAppContext.getResources().getDisplayMetrics().density;
        assert mDensity > 0;
        nativeUpdateSharedViewConfiguration(getMaximumFlingVelocity(), getMinimumFlingVelocity(),
                getTouchSlop(), getDoubleTapSlop(), getMinScalingSpan(), getMinScalingTouchMajor());
    }

    @CalledByNative
    private static int getDoubleTapTimeout() {
        return ViewConfiguration.getDoubleTapTimeout();
    }

    @CalledByNative
    private static int getLongPressTimeout() {
        return ViewConfiguration.getLongPressTimeout();
    }

    @CalledByNative
    private static int getTapTimeout() {
        return ViewConfiguration.getTapTimeout();
    }

    @CalledByNative
    private static float getScrollFriction() {
        return ViewConfiguration.getScrollFriction();
    }

    @CalledByNative
    private float getMaximumFlingVelocity() {
        return toDips(mViewConfiguration.getScaledMaximumFlingVelocity());
    }

    @CalledByNative
    private float getMinimumFlingVelocity() {
        return toDips(mViewConfiguration.getScaledMinimumFlingVelocity());
    }

    @CalledByNative
    private float getTouchSlop() {
        return toDips(mViewConfiguration.getScaledTouchSlop());
    }

    @CalledByNative
    private float getDoubleTapSlop() {
        return toDips(mViewConfiguration.getScaledDoubleTapSlop());
    }

    @CalledByNative
    private float getMinScalingSpan() {
        return toDips(getScaledMinScalingSpan());
    }

    @CalledByNative
    private float getMinScalingTouchMajor() {
        return toDips(getScaledMinScalingTouchMajor());
    }

    private int getScaledMinScalingSpan() {
        final Resources res = mAppContext.getResources();
        int id = res.getIdentifier("config_minScalingSpan", "dimen", "android");
        // Fall back to a sensible default if the internal identifier does not exist.
        if (id == 0) id = R.dimen.config_min_scaling_span;
        try {
            return res.getDimensionPixelSize(id);
        } catch (Resources.NotFoundException e) {
            assert false : "MinScalingSpan resource lookup failed.";
            return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_MM, MIN_SCALING_SPAN_MM,
                    res.getDisplayMetrics());
        }
    }

    private int getScaledMinScalingTouchMajor() {
        final Resources res = mAppContext.getResources();
        int id = res.getIdentifier("config_minScalingTouchMajor", "dimen", "android");
        // Fall back to a sensible default if the internal identifier does not exist.
        if (id == 0) id = R.dimen.config_min_scaling_touch_major;
        try {
            return res.getDimensionPixelSize(id);
        } catch (Resources.NotFoundException e) {
            assert false : "MinScalingTouchMajor resource lookup failed.";
            return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                    MIN_SCALING_TOUCH_MAJOR_DIP, res.getDisplayMetrics());
        }
    }

    /**
     * @return the unscaled pixel quantity in DIPs.
     */
    private float toDips(int pixels) {
        return pixels / mDensity;
    }

    @CalledByNative
    private static ViewConfigurationHelper createWithListener(Context context) {
        ViewConfigurationHelper viewConfigurationHelper = new ViewConfigurationHelper(context);
        viewConfigurationHelper.registerListener();
        return viewConfigurationHelper;
    }

    private native void nativeUpdateSharedViewConfiguration(float maximumFlingVelocity,
            float minimumFlingVelocity, float touchSlop, float doubleTapSlop, float minScalingSpan,
            float minScalingTouchMajor);
}
