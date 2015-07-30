// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Activity;
import android.app.PendingIntent;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Process;
import android.util.Log;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityManager;
import android.widget.Toast;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.ui.VSyncMonitor;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;

/**
 * The window base class that has the minimum functionality.
 */
@JNINamespace("ui")
public class WindowAndroid {
    private static final String TAG = "WindowAndroid";

    @TargetApi(Build.VERSION_CODES.KITKAT)
    private class TouchExplorationMonitor {
        // Listener that tells us when touch exploration is enabled or disabled.
        private AccessibilityManager.TouchExplorationStateChangeListener mTouchExplorationListener;

        TouchExplorationMonitor() {
            mTouchExplorationListener =
                    new AccessibilityManager.TouchExplorationStateChangeListener() {
                public void onTouchExplorationStateChanged(boolean enabled) {
                    mIsTouchExplorationEnabled =
                            mAccessibilityManager.isTouchExplorationEnabled();
                    refreshWillNotDraw();
                }
            };
            mAccessibilityManager.addTouchExplorationStateChangeListener(mTouchExplorationListener);
        }

        void destroy() {
            mAccessibilityManager.removeTouchExplorationStateChangeListener(
                    mTouchExplorationListener);
        }
    }

    // Native pointer to the c++ WindowAndroid object.
    private long mNativeWindowAndroid = 0;
    private final VSyncMonitor mVSyncMonitor;

    // A string used as a key to store intent errors in a bundle
    static final String WINDOW_CALLBACK_ERRORS = "window_callback_errors";

    // Error code returned when an Intent fails to start an Activity.
    public static final int START_INTENT_FAILURE = -1;

    protected Context mApplicationContext;
    protected SparseArray<IntentCallback> mOutstandingIntents;

    // Ideally, this would be a SparseArray<String>, but there's no easy way to store a
    // SparseArray<String> in a bundle during saveInstanceState(). So we use a HashMap and suppress
    // the Android lint warning "UseSparseArrays".
    protected HashMap<Integer, String> mIntentErrors;

    // We track all animations over content and provide a drawing placeholder for them.
    private HashSet<Animator> mAnimationsOverContent = new HashSet<Animator>();
    private View mAnimationPlaceholderView;

    private ViewGroup mKeyboardAccessoryView;

    private boolean mIsKeyboardShowing = false;

    // System accessibility service.
    private final AccessibilityManager mAccessibilityManager;

    // Whether touch exploration is enabled.
    private boolean mIsTouchExplorationEnabled;

    // On KitKat and higher, a class that monitors the touch exploration state.
    private TouchExplorationMonitor mTouchExplorationMonitor;

    /**
     * An interface to notify listeners of changes in the soft keyboard's visibility.
     */
    public interface KeyboardVisibilityListener {
        public void keyboardVisibilityChanged(boolean isShowing);
    }
    private LinkedList<KeyboardVisibilityListener> mKeyboardVisibilityListeners =
            new LinkedList<KeyboardVisibilityListener>();

    private final VSyncMonitor.Listener mVSyncListener = new VSyncMonitor.Listener() {
        @Override
        public void onVSync(VSyncMonitor monitor, long vsyncTimeMicros) {
            if (mNativeWindowAndroid != 0) {
                nativeOnVSync(mNativeWindowAndroid,
                              vsyncTimeMicros,
                              mVSyncMonitor.getVSyncPeriodInMicroseconds());
            }
        }
    };

    /**
     * @return true if onVSync handler is executing.
     * @see org.chromium.ui.VSyncMonitor#isInsideVSync().
     */
    public boolean isInsideVSync() {
        return mVSyncMonitor.isInsideVSync();
    }

    /**
     * @param context The application context.
     */
    @SuppressLint("UseSparseArrays")
    public WindowAndroid(Context context) {
        assert context == context.getApplicationContext();
        mApplicationContext = context;
        mOutstandingIntents = new SparseArray<IntentCallback>();
        mIntentErrors = new HashMap<Integer, String>();
        mVSyncMonitor = new VSyncMonitor(context, mVSyncListener);
        mAccessibilityManager = (AccessibilityManager)
                context.getSystemService(Context.ACCESSIBILITY_SERVICE);
    }

    /**
     * Shows an intent and returns the results to the callback object.
     * @param intent   The PendingIntent that needs to be shown.
     * @param callback The object that will receive the results for the intent.
     * @param errorId  The ID of error string to be show if activity is paused before intent
     *                 results, or null if no message is required.
     * @return Whether the intent was shown.
     */
    public boolean showIntent(PendingIntent intent, IntentCallback callback, Integer errorId) {
        return showCancelableIntent(intent, callback, errorId) >= 0;
    }

    /**
     * Shows an intent and returns the results to the callback object.
     * @param intent   The intent that needs to be shown.
     * @param callback The object that will receive the results for the intent.
     * @param errorId  The ID of error string to be show if activity is paused before intent
     *                 results, or null if no message is required.
     * @return Whether the intent was shown.
     */
    public boolean showIntent(Intent intent, IntentCallback callback, Integer errorId) {
        return showCancelableIntent(intent, callback, errorId) >= 0;
    }

    /**
     * Shows an intent that could be canceled and returns the results to the callback object.
     * @param  intent   The PendingIntent that needs to be shown.
     * @param  callback The object that will receive the results for the intent.
     * @param  errorId  The ID of error string to be show if activity is paused before intent
     *                  results, or null if no message is required.
     * @return A non-negative request code that could be used for finishActivity, or
     *         START_INTENT_FAILURE if failed.
     */
    public int showCancelableIntent(
            PendingIntent intent, IntentCallback callback, Integer errorId) {
        Log.d(TAG, "Can't show intent as context is not an Activity: " + intent);
        return START_INTENT_FAILURE;
    }

    /**
     * Shows an intent that could be canceled and returns the results to the callback object.
     * @param  intent   The intent that needs to be showed.
     * @param  callback The object that will receive the results for the intent.
     * @param  errorId  The ID of error string to be show if activity is paused before intent
     *                  results, or null if no message is required.
     * @return A non-negative request code that could be used for finishActivity, or
     *         START_INTENT_FAILURE if failed.
     */
    public int showCancelableIntent(Intent intent, IntentCallback callback, Integer errorId) {
        Log.d(TAG, "Can't show intent as context is not an Activity: " + intent);
        return START_INTENT_FAILURE;
    }

    /**
     * Force finish another activity that you had previously started with showCancelableIntent.
     * @param requestCode The request code returned from showCancelableIntent.
     */
    public void cancelIntent(int requestCode) {
        Log.d(TAG, "Can't cancel intent as context is not an Activity: " + requestCode);
    }

    /**
     * Removes a callback from the list of pending intents, so that nothing happens if/when the
     * result for that intent is received.
     * @param callback The object that should have received the results
     * @return True if the callback was removed, false if it was not found.
    */
    public boolean removeIntentCallback(IntentCallback callback) {
        int requestCode = mOutstandingIntents.indexOfValue(callback);
        if (requestCode < 0) return false;
        mOutstandingIntents.remove(requestCode);
        mIntentErrors.remove(requestCode);
        return true;
    }

    /**
     * Determine whether access to a particular permission is granted.
     * @param permission The permission whose access is to be checked.
     * @return Whether access to the permission is granted.
     */
    public boolean hasPermission(String permission) {
        return mApplicationContext.checkPermission(permission, Process.myPid(), Process.myUid())
                == PackageManager.PERMISSION_GRANTED;
    }

    /**
     * Determine whether the specified permission can be requested.
     *
     * <p>
     * A permission can be requested in the following states:
     * 1.) Default un-granted state, permission can be requested
     * 2.) Permission previously requested but denied by the user, but the user did not select
     *     "Never ask again".
     *
     * @param permission The permission name.
     * @return Whether the requesting the permission is allowed.
     */
    public boolean canRequestPermission(String permission) {
        Log.w(TAG, "Cannot determine the request permission state as the context "
                + "is not an Activity");
        assert false : "Failed to determine the request permission state using a WindowAndroid "
                + "without an Activity";
        return false;
    }

    /**
     * Requests the specified permissions are granted for further use.
     * @param permissions The list of permissions to request access to.
     * @param callback The callback to be notified whether the permissions were granted.
     */
    public void requestPermissions(String[] permissions, PermissionCallback callback) {
        Log.w(TAG, "Cannot request permissions as the context is not an Activity");
        assert false : "Failed to request permissions using a WindowAndroid without an Activity";
    }

    /**
     * Determine whether access to file is granted.
     */
    public boolean hasFileAccess() {
        return true;
    }

    /**
     * Requests the access to files.
     * @param callback The callback to be notified whether access were granted.
     */
    public void requestFileAccess(final FileAccessCallback callback) {
        new Handler().post(new Runnable() {
            @Override
            public void run() {
                callback.onFileAccessResult(false);
            }
        });
    }

    /**
     * Displays an error message with a provided error message string.
     * @param error The error message string to be displayed.
     */
    public void showError(String error) {
        if (error != null) {
            Toast.makeText(mApplicationContext, error, Toast.LENGTH_SHORT).show();
        }
    }

    /**
     * Displays an error message from the given resource id.
     * @param resId The error message string's resource id.
     */
    public void showError(int resId) {
        showError(mApplicationContext.getString(resId));
    }

    /**
     * Displays an error message for a nonexistent callback.
     * @param error The error message string to be displayed.
     */
    protected void showCallbackNonExistentError(String error) {
        showError(error);
    }

    /**
     * Broadcasts the given intent to all interested BroadcastReceivers.
     */
    public void sendBroadcast(Intent intent) {
        mApplicationContext.sendBroadcast(intent);
    }

    /**
     * @return A reference to owning Activity.  The returned WeakReference will never be null, but
     *         the contained Activity can be null (either if it has been garbage collected or if
     *         this is in the context of a WebView that was not created using an Activity).
     */
    public WeakReference<Activity> getActivity() {
        return new WeakReference<Activity>(null);
    }

    /**
     * @return The application context for this activity.
     */
    public Context getApplicationContext() {
        return mApplicationContext;
    }

    /**
     * Saves the error messages that should be shown if any pending intents would return
     * after the application has been put onPause.
     * @param bundle The bundle to save the information in onPause
     */
    public void saveInstanceState(Bundle bundle) {
        bundle.putSerializable(WINDOW_CALLBACK_ERRORS, mIntentErrors);
    }

    /**
     * Restores the error messages that should be shown if any pending intents would return
     * after the application has been put onPause.
     * @param bundle The bundle to restore the information from onResume
     */
    public void restoreInstanceState(Bundle bundle) {
        if (bundle == null) return;

        Object errors = bundle.getSerializable(WINDOW_CALLBACK_ERRORS);
        if (errors instanceof HashMap) {
            @SuppressWarnings("unchecked")
            HashMap<Integer, String> intentErrors = (HashMap<Integer, String>) errors;
            mIntentErrors = intentErrors;
        }
    }

    /**
     * For window instances associated with an activity, notifies any listeners
     * that the activity has been paused.
     */
    protected void onActivityPaused() {
        if (mNativeWindowAndroid == 0) return;
        nativeOnActivityPaused(mNativeWindowAndroid);
    }

    /**
     * For window instances associated with an activity, notifies any listeners
     * that the activity has been paused.
     */
    protected void onActivityResumed() {
        if (mNativeWindowAndroid == 0) return;
        nativeOnActivityResumed(mNativeWindowAndroid);
    }

    @CalledByNative
    private void requestVSyncUpdate() {
        mVSyncMonitor.requestUpdate();
    }

    /**
     * An interface that intent callback objects have to implement.
     */
    public interface IntentCallback {
        /**
         * Handles the data returned by the requested intent.
         * @param window A window reference.
         * @param resultCode Result code of the requested intent.
         * @param contentResolver An instance of ContentResolver class for accessing returned data.
         * @param data The data returned by the intent.
         */
        void onIntentCompleted(WindowAndroid window, int resultCode,
                ContentResolver contentResolver, Intent data);
    }

    /**
     * Callback for permission requests.
     */
    public interface PermissionCallback {
        /**
         * Called upon completing a permission request.
         * @param permissions The list of permissions in the result.
         * @param grantResults Whether the permissions were granted.
         */
        void onRequestPermissionsResult(String[] permissions, int[] grantResults);
    }

    /**
     * Callback for file access requests.
     */
    public interface FileAccessCallback {
        /**
         * Called upon completing a file access request.
         * @param granted Whether file access is granted.
         */
        void onFileAccessResult(boolean granted);
    }

    /**
     * Tests that an activity is available to handle the passed in intent.
     * @param  intent The intent to check.
     * @return True if an activity is available to process this intent when started, meaning that
     *         Context.startActivity will not throw ActivityNotFoundException.
     */
    public boolean canResolveActivity(Intent intent) {
        return mApplicationContext.getPackageManager().resolveActivity(intent, 0) != null;
    }

    /**
     * Destroys the c++ WindowAndroid object if one has been created.
     */
    public void destroy() {
        if (mNativeWindowAndroid != 0) {
            nativeDestroy(mNativeWindowAndroid);
            mNativeWindowAndroid = 0;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            if (mTouchExplorationMonitor != null) mTouchExplorationMonitor.destroy();
        }
    }

    /**
     * Returns a pointer to the c++ AndroidWindow object and calls the initializer if
     * the object has not been previously initialized.
     * @return A pointer to the c++ AndroidWindow.
     */
    public long getNativePointer() {
        if (mNativeWindowAndroid == 0) {
            mNativeWindowAndroid = nativeInit();
        }
        return mNativeWindowAndroid;
    }

    /**
     * Set the animation placeholder view, which we set to 'draw' during animations, such that
     * animations aren't clipped by the SurfaceView 'hole'. This can be the SurfaceView itself
     * or a view directly on top of it. This could be extended to many views if we ever need it.
     */
    public void setAnimationPlaceholderView(View view) {
        mAnimationPlaceholderView = view;

        // The accessibility focus ring also gets clipped by the SurfaceView 'hole', so
        // make sure the animation placeholder view is in place if touch exploration is on.
        mIsTouchExplorationEnabled = mAccessibilityManager.isTouchExplorationEnabled();
        refreshWillNotDraw();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            mTouchExplorationMonitor = new TouchExplorationMonitor();
        }
    }

    /**
     * Sets the keyboard accessory view.
     * @param view This view sits at the bottom of the content area and pushes the content up rather
     *             than overlaying it. Currently used as a container for Autofill suggestions.
     */
    public void setKeyboardAccessoryView(ViewGroup view) {
        mKeyboardAccessoryView = view;
    }

    /**
     * {@see setKeyboardAccessoryView(ViewGroup)}.
     */
    public ViewGroup getKeyboardAccessoryView() {
        return mKeyboardAccessoryView;
    }

    protected void registerKeyboardVisibilityCallbacks() {
    }

    protected void unregisterKeyboardVisibilityCallbacks() {
    }

    /**
     * Adds a listener that is updated of keyboard visibility changes. This works as a best guess.
     * {@see UiUtils.isKeyboardShowing}
     */
    public void addKeyboardVisibilityListener(KeyboardVisibilityListener listener) {
        if (mKeyboardVisibilityListeners.isEmpty()) {
            registerKeyboardVisibilityCallbacks();
        }
        mKeyboardVisibilityListeners.add(listener);
    }

    /**
     * {@see addKeyboardVisibilityListener()}.
     */
    public void removeKeyboardVisibilityListener(KeyboardVisibilityListener listener) {
        mKeyboardVisibilityListeners.remove(listener);
        if (mKeyboardVisibilityListeners.isEmpty()) {
            unregisterKeyboardVisibilityCallbacks();
        }
    }

    /**
     * To be called when the keyboard visibility state might have changed. Informs listeners of the
     * state change IFF there actually was a change.
     * @param isShowing The current (guesstimated) state of the keyboard.
     */
    protected void keyboardVisibilityPossiblyChanged(boolean isShowing) {
        if (mIsKeyboardShowing == isShowing) return;
        mIsKeyboardShowing = isShowing;

        // Clone the list in case a listener tries to remove itself during the callback.
        LinkedList<KeyboardVisibilityListener> listeners =
                new LinkedList<KeyboardVisibilityListener>(mKeyboardVisibilityListeners);
        for (KeyboardVisibilityListener listener : listeners) {
            listener.keyboardVisibilityChanged(isShowing);
        }
    }

    /**
     * Start a post-layout animation on top of web content.
     *
     * By default, Android optimizes what it shows on top of SurfaceViews (saves power).
     * Effectively, layouts determine what gets drawn and post-layout animations outside
     * of this area may be 'clipped'. Using this method to start such animations will
     * ensure that nothing is clipped during the animation, and restore the optimal
     * state when the animation ends.
     */
    public void startAnimationOverContent(Animator animation) {
        // We may not need an animation placeholder (eg. Webview doesn't use SurfaceView)
        if (mAnimationPlaceholderView == null) return;
        if (animation.isStarted()) throw new IllegalArgumentException("Already started.");
        boolean added = mAnimationsOverContent.add(animation);
        if (!added) throw new IllegalArgumentException("Already Added.");

        // We start the animation in this method to help guarantee that we never get stuck in this
        // state or leak objects into the set. Starting the animation here should guarantee that we
        // get an onAnimationEnd callback, and remove this animation.
        animation.start();

        // When the first animation starts, make the placeholder 'draw' itself.
        refreshWillNotDraw();

        // When the last animation ends, remove the placeholder view,
        // returning to the default optimized state.
        animation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                animation.removeListener(this);
                mAnimationsOverContent.remove(animation);
                refreshWillNotDraw();
            }
        });
    }

    /**
     * Update whether the placeholder is 'drawn' based on whether an animation is running
     * or touch exploration is enabled - if either of those are true, we call
     * setWillNotDraw(false) to ensure that the animation is drawn over the SurfaceView,
     * and otherwise we call setWillNotDraw(true).
     */
    private void refreshWillNotDraw() {
        boolean willNotDraw = !mIsTouchExplorationEnabled && mAnimationsOverContent.isEmpty();
        if (mAnimationPlaceholderView.willNotDraw() != willNotDraw) {
            mAnimationPlaceholderView.setWillNotDraw(willNotDraw);
        }
    }

    private native long nativeInit();
    private native void nativeOnVSync(long nativeWindowAndroid,
                                      long vsyncTimeMicros,
                                      long vsyncPeriodMicros);
    private native void nativeOnActivityPaused(long nativeWindowAndroid);
    private native void nativeOnActivityResumed(long nativeWindowAndroid);
    private native void nativeDestroy(long nativeWindowAndroid);

}
