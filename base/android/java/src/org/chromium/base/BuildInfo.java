// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Build;
import android.text.TextUtils;
import android.util.Log;

import org.chromium.base.annotations.CalledByNative;

/**
 * BuildInfo is a utility class providing easy access to {@link PackageInfo}
 * information. This is primarly of use for accessesing package information
 * from native code.
 */
public class BuildInfo {
    private static final String TAG = "BuildInfo";
    private static final int MAX_FINGERPRINT_LENGTH = 128;

    /**
     * BuildInfo is a static utility class and therefore shouldn't be
     * instantiated.
     */
    private BuildInfo() {
    }

    @CalledByNative
    public static String getDevice() {
        return Build.DEVICE;
    }

    @CalledByNative
    public static String getBrand() {
        return Build.BRAND;
    }

    @CalledByNative
    public static String getAndroidBuildId() {
        return Build.ID;
    }

    /**
     * @return The build fingerprint for the current Android install.  The value is truncated to a
     *         128 characters as this is used for crash and UMA reporting, which should avoid huge
     *         strings.
     */
    @CalledByNative
    public static String getAndroidBuildFingerprint() {
        return Build.FINGERPRINT.substring(
                0, Math.min(Build.FINGERPRINT.length(), MAX_FINGERPRINT_LENGTH));
    }

    @CalledByNative
    public static String getDeviceManufacturer() {
        return Build.MANUFACTURER;
    }

    @CalledByNative
    public static String getDeviceModel() {
        return Build.MODEL;
    }

    @CalledByNative
    public static String getPackageVersionCode(Context context) {
        String msg = "versionCode not available.";
        try {
            PackageManager pm = context.getPackageManager();
            PackageInfo pi = pm.getPackageInfo(context.getPackageName(), 0);
            msg = "";
            if (pi.versionCode > 0) {
                msg = Integer.toString(pi.versionCode);
            }
        } catch (NameNotFoundException e) {
            Log.d(TAG, msg);
        }
        return msg;

    }

    @CalledByNative
    public static String getPackageVersionName(Context context) {
        String msg = "versionName not available";
        try {
            PackageManager pm = context.getPackageManager();
            PackageInfo pi = pm.getPackageInfo(context.getPackageName(), 0);
            msg = pi.versionName;
        } catch (NameNotFoundException e) {
            Log.d(TAG, msg);
        }
        return msg;
    }

    @CalledByNative
    public static String getPackageLabel(Context context) {
        try {
            PackageManager packageManager = context.getPackageManager();
            ApplicationInfo appInfo = packageManager.getApplicationInfo(context.getPackageName(),
                    PackageManager.GET_META_DATA);
            CharSequence label = packageManager.getApplicationLabel(appInfo);
            return  label != null ? label.toString() : "";
        } catch (NameNotFoundException e) {
            return "";
        }
    }

    @CalledByNative
    public static String getPackageName(Context context) {
        String packageName = context != null ? context.getPackageName() : null;
        return packageName != null ? packageName : "";
    }

    @CalledByNative
    public static String getBuildType() {
        return Build.TYPE;
    }

    @CalledByNative
    public static int getSdkInt() {
        return Build.VERSION.SDK_INT;
    }

    /**
     * @return Whether the Android build is M or later.
     */
    public static boolean isMncOrLater() {
        // TODO(bauerb): Update this once the SDK is updated.
        return Build.VERSION.SDK_INT > Build.VERSION_CODES.LOLLIPOP_MR1
                 || TextUtils.equals("MNC", Build.VERSION.CODENAME);
    }

    private static boolean isLanguageSplit(String splitName) {
        // Names look like "config.XX".
        return splitName.length() == 9 && splitName.startsWith("config.");
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    @CalledByNative
    public static boolean hasLanguageApkSplits(Context context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            return false;
        }
        PackageInfo packageInfo = PackageUtils.getOwnPackageInfo(context);
        if (packageInfo.splitNames != null) {
            for (int i = 0; i < packageInfo.splitNames.length; ++i) {
                if (isLanguageSplit(packageInfo.splitNames[i])) {
                    return true;
                }
            }
        }
        return false;
    }
}
