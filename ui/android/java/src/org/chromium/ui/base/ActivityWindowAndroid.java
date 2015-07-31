// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.IntentSender.SendIntentException;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.PermissionInfo;
import android.os.Handler;
import android.preference.PreferenceManager;
import android.text.TextUtils;
import android.util.SparseArray;
import android.view.View;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.BuildInfo;
import org.chromium.ui.UiUtils;

import java.lang.ref.WeakReference;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * The class provides the WindowAndroid's implementation which requires
 * Activity Instance.
 * Only instantiate this class when you need the implemented features.
 */
public class ActivityWindowAndroid
        extends WindowAndroid
        implements ApplicationStatus.ActivityStateListener, View.OnLayoutChangeListener {
    // Constants used for intent request code bounding.
    private static final int REQUEST_CODE_PREFIX = 1000;
    private static final int REQUEST_CODE_RANGE_SIZE = 100;
    private static final String TAG = "ActivityWindowAndroid";

    private static final String PERMISSION_QUERIED_KEY_PREFIX = "HasRequestedAndroidPermission::";

    private final WeakReference<Activity> mActivityRef;
    private final Handler mHandler;
    private final SparseArray<PermissionCallback> mOutstandingPermissionRequests;

    private Method mRequestPermissionsMethod;

    private int mNextRequestCode = 0;

    /**
     * Creates an Activity-specific WindowAndroid with associated intent functionality.
     * TODO(jdduke): Remove this overload when all callsites have been updated to
     * indicate their activity state listening preference.
     * @param activity The activity associated with the WindowAndroid.
     */
    public ActivityWindowAndroid(Activity activity) {
        this(activity, true);
    }

    /**
     * Creates an Activity-specific WindowAndroid with associated intent functionality.
     * @param activity The activity associated with the WindowAndroid.
     * @param listenToActivityState Whether to listen to activity state changes.
     */
    public ActivityWindowAndroid(Activity activity, boolean listenToActivityState) {
        super(activity.getApplicationContext());
        mActivityRef = new WeakReference<Activity>(activity);
        mHandler = new Handler();
        mOutstandingPermissionRequests = new SparseArray<PermissionCallback>();
        if (listenToActivityState) {
            ApplicationStatus.registerStateListenerForActivity(this, activity);
        }
    }

    @Override
    protected void registerKeyboardVisibilityCallbacks() {
        Activity activity = mActivityRef.get();
        if (activity == null) return;
        activity.findViewById(android.R.id.content).addOnLayoutChangeListener(this);
    }

    @Override
    protected void unregisterKeyboardVisibilityCallbacks() {
        Activity activity = mActivityRef.get();
        if (activity == null) return;
        activity.findViewById(android.R.id.content).removeOnLayoutChangeListener(this);
    }

    @Override
    public int showCancelableIntent(
            PendingIntent intent, IntentCallback callback, Integer errorId) {
        Activity activity = mActivityRef.get();
        if (activity == null) return START_INTENT_FAILURE;

        int requestCode = generateNextRequestCode();

        try {
            activity.startIntentSenderForResult(
                    intent.getIntentSender(), requestCode, new Intent(), 0, 0, 0);
        } catch (SendIntentException e) {
            return START_INTENT_FAILURE;
        }

        storeCallbackData(requestCode, callback, errorId);
        return requestCode;
    }

    @Override
    public int showCancelableIntent(Intent intent, IntentCallback callback, Integer errorId) {
        Activity activity = mActivityRef.get();
        if (activity == null) return START_INTENT_FAILURE;

        int requestCode = generateNextRequestCode();

        try {
            activity.startActivityForResult(intent, requestCode);
        } catch (ActivityNotFoundException e) {
            return START_INTENT_FAILURE;
        }

        storeCallbackData(requestCode, callback, errorId);
        return requestCode;
    }

    @Override
    public void cancelIntent(int requestCode) {
        Activity activity = mActivityRef.get();
        if (activity == null) return;
        activity.finishActivity(requestCode);
    }

    /**
     * Responds to the intent result if the intent was created by the native window.
     * @param requestCode Request code of the requested intent.
     * @param resultCode Result code of the requested intent.
     * @param data The data returned by the intent.
     * @return Boolean value of whether the intent was started by the native window.
     */
    public boolean onActivityResult(int requestCode, int resultCode, Intent data) {
        IntentCallback callback = mOutstandingIntents.get(requestCode);
        mOutstandingIntents.delete(requestCode);
        String errorMessage = mIntentErrors.remove(requestCode);

        if (callback != null) {
            callback.onIntentCompleted(this, resultCode,
                    mApplicationContext.getContentResolver(), data);
            return true;
        } else {
            if (errorMessage != null) {
                showCallbackNonExistentError(errorMessage);
                return true;
            }
        }
        return false;
    }

    private String getHasRequestedPermissionKey(String permission) {
        String permissionQueriedKey = permission;
        try {
            // Runtime permissions are controlled at the group level.  So when determining whether
            // we have requested a particular permission before, we should check whether we
            // have requested any permission in that group as that mimics the logic in the Android
            // framework.
            //
            // e.g. Requesting first the permission ACCESS_FINE_LOCATION will result in Chrome
            //      treating ACCESS_COARSE_LOCATION as if it had already been requested as well.
            PermissionInfo permissionInfo = getApplicationContext().getPackageManager()
                    .getPermissionInfo(permission, PackageManager.GET_META_DATA);

            if (!TextUtils.isEmpty(permissionInfo.group)) {
                permissionQueriedKey = permissionInfo.group;
            }
        } catch (NameNotFoundException e) {
            // Unknown permission.  Default back to the permission name instead of the group.
        }

        return PERMISSION_QUERIED_KEY_PREFIX + permissionQueriedKey;
    }

    @Override
    public boolean canRequestPermission(String permission) {
        if (!BuildInfo.isMncOrLater()) return false;

        Activity activity = mActivityRef.get();
        if (activity == null) return false;

        // TODO(tedchoc): Child classes are currently required to determine whether we have
        //                previously requested the permission before but the user did not
        //                select "Never ask again".  Merge with this class when possible.

        // Check whether we have ever asked for this permission by checking whether we saved
        // a preference associated with it before.
        String permissionQueriedKey = getHasRequestedPermissionKey(permission);
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(activity);
        if (!prefs.getBoolean(permissionQueriedKey, false)) return true;

        return false;
    }

    @Override
    public void requestPermissions(
            final String[] permissions, final PermissionCallback callback) {
        // If the permission request was not sent successfully, just post a response to the
        // callback with whatever the current permission state is for all the requested
        // permissions.  The response is posted to keep the async behavior of this method
        // consistent.
        if (!requestPermissionsInternal(permissions, callback)) {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    int[] results = new int[permissions.length];
                    for (int i = 0; i < permissions.length; i++) {
                        results[i] = hasPermission(permissions[i])
                                ? PackageManager.PERMISSION_GRANTED
                                : PackageManager.PERMISSION_DENIED;
                    }
                    callback.onRequestPermissionsResult(permissions, results);
                }
            });
        } else {
            Activity activity = mActivityRef.get();
            SharedPreferences.Editor editor =
                    PreferenceManager.getDefaultSharedPreferences(activity).edit();
            for (int i = 0; i < permissions.length; i++) {
                editor.putBoolean(getHasRequestedPermissionKey(permissions[i]), true);
            }
            editor.apply();
        }
    }

    /**
     * Issues the permission request and returns whether it was sent successfully.
     */
    private boolean requestPermissionsInternal(String[] permissions, PermissionCallback callback) {
        // TODO(tedchoc): Remove the MNC check once the SDK version is bumped.
        if (!BuildInfo.isMncOrLater()) return false;

        // TODO(tedchoc): Remove the reflection aspect of this once a public M SDK is available.
        Activity activity = mActivityRef.get();
        if (activity == null) return false;

        if (mRequestPermissionsMethod == null) {
            try {
                mRequestPermissionsMethod = Activity.class.getMethod(
                        "requestPermissions", String[].class, int.class);
            } catch (NoSuchMethodException e) {
                return false;
            }
        }

        int requestCode = generateNextRequestCode();
        mOutstandingPermissionRequests.put(requestCode, callback);

        try {
            mRequestPermissionsMethod.invoke(activity, permissions, requestCode);
            return true;
        } catch (IllegalAccessException e) {
            mOutstandingPermissionRequests.delete(requestCode);
        } catch (IllegalArgumentException e) {
            mOutstandingPermissionRequests.delete(requestCode);
        } catch (InvocationTargetException e) {
            mOutstandingPermissionRequests.delete(requestCode);
        }

        return false;
    }

    /**
     * Responds to a pending permission result.
     * @param requestCode The unique code for the permission request.
     * @param permissions The list of permissions in the result.
     * @param grantResults Whether the permissions were granted.
     * @return Whether the permission request corresponding to a pending permission request.
     */
    public boolean onRequestPermissionsResult(int requestCode, String[] permissions,
            int[] grantResults) {
        PermissionCallback callback = mOutstandingPermissionRequests.get(requestCode);
        mOutstandingPermissionRequests.delete(requestCode);
        if (callback == null) return false;
        callback.onRequestPermissionsResult(permissions, grantResults);
        return true;
    }

    @Override
    public WeakReference<Activity> getActivity() {
        // Return a new WeakReference to prevent clients from releasing our internal WeakReference.
        return new WeakReference<Activity>(mActivityRef.get());
    }

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        if (newState == ActivityState.PAUSED) {
            onActivityPaused();
        } else if (newState == ActivityState.RESUMED) {
            onActivityResumed();
        }
    }

    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        keyboardVisibilityPossiblyChanged(UiUtils.isKeyboardShowing(mActivityRef.get(), v));
    }

    private int generateNextRequestCode() {
        int requestCode = REQUEST_CODE_PREFIX + mNextRequestCode;
        mNextRequestCode = (mNextRequestCode + 1) % REQUEST_CODE_RANGE_SIZE;
        return requestCode;
    }

    private void storeCallbackData(int requestCode, IntentCallback callback, Integer errorId) {
        mOutstandingIntents.put(requestCode, callback);
        mIntentErrors.put(
                requestCode, errorId == null ? null : mApplicationContext.getString(errorId));
    }
}
