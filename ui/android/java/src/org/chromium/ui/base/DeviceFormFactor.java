// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.Context;
import android.os.Build;
import android.util.DisplayMetrics;
import android.view.WindowManager;

import org.chromium.base.annotations.CalledByNative;

/**
 * UI utilities for accessing form factor information.
 */
public class DeviceFormFactor {

    /**
     * The minimum width that would classify the device as a tablet or a large tablet.
     */
    public static final int MINIMUM_TABLET_WIDTH_DP = 600;
    private static final int MINIMUM_LARGE_TABLET_WIDTH_DP = 720;

    private static Boolean sIsTablet = null;
    private static Boolean sIsLargeTablet = null;
    private static Integer sMinimumTabletWidthPx = null;
    private static Float sDensity = null;

    /**
     * @param context {@link Context} used to get the Application Context.
     * @return Whether the app should treat the device as a tablet for layout. This method is not
     *                 affected by Android N multi-window.
     */
    @CalledByNative
    public static boolean isTablet(Context context) {
        if (sIsTablet == null) {
            sIsTablet = getSmallestDeviceWidthDp(context) >= MINIMUM_TABLET_WIDTH_DP;
        }
        return sIsTablet;
    }

    /**
     * @param context {@link Context} used to get the Application Context.
     * @return True if the app should treat the device as a large (> 720dp) tablet for layout. This
     *         method is not affected by Android N multi-window.
     */
    public static boolean isLargeTablet(Context context) {
        if (sIsLargeTablet == null) {
            sIsLargeTablet = getSmallestDeviceWidthDp(context) >= MINIMUM_LARGE_TABLET_WIDTH_DP;
        }
        return sIsLargeTablet;
    }

    /**
     * Calculates the minimum device width in dp. This method is not affected by Android N
     * multi-window.
     *
     * @param context {@link Context} used to get the Application Context.
     * @return The smaller of device width and height in dp.
     */
    public static int getSmallestDeviceWidthDp(Context context) {
        assert context.getApplicationContext() != null;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            DisplayMetrics metrics = new DisplayMetrics();
            // The Application Context must be used instead of the regular Context, because
            // in Android N multi-window calling Display.getRealMetrics() using the regular Context
            // returns the size of the current screen rather than the device.
            ((WindowManager) context.getApplicationContext().getSystemService(
                    Context.WINDOW_SERVICE)).getDefaultDisplay().getRealMetrics(metrics);
            return Math.round(Math.min(metrics.heightPixels / metrics.density,
                    metrics.widthPixels / metrics.density));
        } else {
            // Display.getRealMetrics() is only available in API level 17+, so
            // Configuration.smallestScreenWidthDp is used instead. Proir to the introduction of
            // multi-window in Android N, smallestScreenWidthDp was the same as the minimum size
            // in getRealMetrics().
            return context.getResources().getConfiguration().smallestScreenWidthDp;
        }
    }

    /**
     * @param context {@link Context} used to get the display density.
     * @return The minimum width in px at which the device should be treated like a tablet for
     *         layout.
     */
    public static int getMinimumTabletWidthPx(Context context) {
        if (sMinimumTabletWidthPx == null) {
            sMinimumTabletWidthPx = Math.round(MINIMUM_TABLET_WIDTH_DP
                    * context.getResources().getDisplayMetrics().density);
        }
        return sMinimumTabletWidthPx;
    }

    /**
     * Resets all cached values if the display density has changed.
     */
    public static void resetValuesIfNeeded(Context context) {
        float currentDensity = context.getResources().getDisplayMetrics().density;
        if (sDensity != null && sDensity != currentDensity) {
            sIsTablet = null;
            sIsLargeTablet = null;
            sMinimumTabletWidthPx = null;
        }
        sDensity = currentDensity;
    }
}
