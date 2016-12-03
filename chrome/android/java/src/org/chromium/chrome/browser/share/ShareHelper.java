// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.ClipData;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Build;
import android.support.annotation.Nullable;
import android.support.v7.app.AlertDialog;
import android.text.TextUtils;
import android.util.Pair;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.ui.UiUtils;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * A helper class that helps to start an intent to share titles and URLs.
 */
public class ShareHelper {

    private static final String TAG = "share";

    /** The task ID of the activity that triggered the share action. */
    public static final String EXTRA_TASK_ID = "org.chromium.chrome.extra.TASK_ID";

    private static final String JPEG_EXTENSION = ".jpg";
    private static final String PACKAGE_NAME_KEY = "last_shared_package_name";
    private static final String CLASS_NAME_KEY = "last_shared_class_name";
    private static final String EXTRA_SHARE_SCREENSHOT_AS_STREAM = "share_screenshot_as_stream";
    private static final long COMPONENT_INFO_READ_TIMEOUT_IN_MS = 1000;

    /**
     * Directory name for shared images.
     *
     * Named "screenshot" for historical reasons as we only initially shared screenshot images.
     */
    private static final String SHARE_IMAGES_DIRECTORY_NAME = "screenshot";

    private ShareHelper() {}

    @SuppressFBWarnings("NP_NULL_ON_SOME_PATH_FROM_RETURN_VALUE")
    private static void deleteShareImageFiles(File file) {
        if (!file.exists()) return;
        if (file.isDirectory()) {
            for (File f : file.listFiles()) deleteShareImageFiles(f);
        }
        if (!file.delete()) {
            Log.w(TAG, "Failed to delete share image file: %s", file.getAbsolutePath());
        }
    }

    /**
     * Callback interface for when a target is chosen.
     */
    public static interface TargetChosenCallback {
        /**
         * Called when the user chooses a target in the share dialog.
         *
         * Note that if the user cancels the share dialog, this callback is never called.
         */
        public void onTargetChosen(ComponentName chosenComponent);
    }

    /**
     * Receiver to record the chosen component when sharing an Intent.
     */
    static class TargetChosenReceiver extends BroadcastReceiver {
        private static final String EXTRA_RECEIVER_TOKEN = "receiver_token";
        private static final Object LOCK = new Object();

        private static String sTargetChosenReceiveAction;
        private static TargetChosenReceiver sLastRegisteredReceiver;

        private final boolean mSaveLastUsed;
        @Nullable private final TargetChosenCallback mCallback;

        private TargetChosenReceiver(boolean saveLastUsed,
                                     @Nullable TargetChosenCallback callback) {
            mSaveLastUsed = saveLastUsed;
            mCallback = callback;
        }

        static boolean isSupported() {
            return Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP_MR1;
        }

        @TargetApi(Build.VERSION_CODES.LOLLIPOP_MR1)
        static void sendChooserIntent(boolean saveLastUsed, Activity activity,
                                      Intent sharingIntent,
                                      @Nullable TargetChosenCallback callback) {
            synchronized (LOCK) {
                if (sTargetChosenReceiveAction == null) {
                    sTargetChosenReceiveAction = activity.getPackageName() + "/"
                            + TargetChosenReceiver.class.getName() + "_ACTION";
                }
                Context context = activity.getApplicationContext();
                if (sLastRegisteredReceiver != null) {
                    context.unregisterReceiver(sLastRegisteredReceiver);
                }
                sLastRegisteredReceiver = new TargetChosenReceiver(saveLastUsed, callback);
                context.registerReceiver(
                        sLastRegisteredReceiver, new IntentFilter(sTargetChosenReceiveAction));
            }

            Intent intent = new Intent(sTargetChosenReceiveAction);
            intent.setPackage(activity.getPackageName());
            intent.putExtra(EXTRA_RECEIVER_TOKEN, sLastRegisteredReceiver.hashCode());
            final PendingIntent pendingIntent = PendingIntent.getBroadcast(activity, 0, intent,
                    PendingIntent.FLAG_CANCEL_CURRENT | PendingIntent.FLAG_ONE_SHOT);
            Intent chooserIntent = Intent.createChooser(sharingIntent,
                    activity.getString(R.string.share_link_chooser_title),
                    pendingIntent.getIntentSender());
            activity.startActivity(chooserIntent);
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            synchronized (LOCK) {
                if (sLastRegisteredReceiver != this) return;
                context.getApplicationContext().unregisterReceiver(sLastRegisteredReceiver);
                sLastRegisteredReceiver = null;
            }
            if (!intent.hasExtra(EXTRA_RECEIVER_TOKEN)
                    || intent.getIntExtra(EXTRA_RECEIVER_TOKEN, 0) != this.hashCode()) {
                return;
            }

            ComponentName target = intent.getParcelableExtra(Intent.EXTRA_CHOSEN_COMPONENT);
            if (mCallback != null) {
                mCallback.onTargetChosen(target);
            }
            if (mSaveLastUsed && target != null) {
                setLastShareComponentName(target);
            }
        }
    }

    /**
     * Clears all shared image files.
     */
    public static void clearSharedImages(final Context context) {
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... params) {
                try {
                    File imagePath = UiUtils.getDirectoryForImageCapture(context);
                    deleteShareImageFiles(new File(imagePath, SHARE_IMAGES_DIRECTORY_NAME));
                } catch (IOException ie) {
                    // Ignore exception.
                }
                return null;
            }
        }.execute();
    }

    /**
     * Creates and shows a share intent picker dialog or starts a share intent directly with the
     * activity that was most recently used to share based on shareDirectly value.
     *
     * This function will save |screenshot| under {app's root}/files/images/screenshot (or
     * /sdcard/DCIM/browser-images/screenshot if ADK is lower than JB MR2).
     * Cleaning up doesn't happen automatically, and so an app should call clearSharedScreenshots()
     * explicitly when needed.
     *
     * @param shareDirectly Whether it should share directly with the activity that was most
     *                      recently used to share.
     * @param saveLastUsed Whether to save the chosen activity for future direct sharing.
     * @param activity Activity that is used to access package manager.
     * @param title Title of the page to be shared.
     * @param text Text to be shared. If both |text| and |url| are supplied, they are concatenated
     *             with a space.
     * @param url URL of the page to be shared.
     * @param offlineUri URI to the offline MHTML file to be shared.
     * @param screenshot Screenshot of the page to be shared.
     * @param callback Optional callback to be called when user makes a choice. Will not be called
     *                 if receiving a response when the user makes a choice is not supported (on
     *                 older Android versions).
     */
    public static void share(boolean shareDirectly, boolean saveLastUsed, Activity activity,
            String title, String text, String url, @Nullable Uri offlineUri, Bitmap screenshot,
            @Nullable TargetChosenCallback callback) {
        if (shareDirectly) {
            shareWithLastUsed(activity, title, text, url, offlineUri, screenshot);
        } else if (TargetChosenReceiver.isSupported()) {
            makeIntentAndShare(saveLastUsed, activity, title, text, url, offlineUri, screenshot,
                    null, callback);
        } else {
            showShareDialog(
                    saveLastUsed, activity, title, text, url, offlineUri, screenshot, callback);
        }
    }

    /**
     * Trigger the share action for the given image data.
     * @param activity The activity used to trigger the share action.
     * @param jpegImageData The image data to be shared in jpeg format.
     */
    public static void shareImage(final Activity activity, final byte[] jpegImageData) {
        if (jpegImageData.length == 0) {
            Log.w(TAG, "Share failed -- Received image contains no data.");
            return;
        }

        new AsyncTask<Void, Void, File>() {
            @Override
            protected File doInBackground(Void... params) {
                FileOutputStream fOut = null;
                try {
                    File path = new File(UiUtils.getDirectoryForImageCapture(activity),
                            SHARE_IMAGES_DIRECTORY_NAME);
                    if (path.exists() || path.mkdir()) {
                        File saveFile = File.createTempFile(
                                String.valueOf(System.currentTimeMillis()), JPEG_EXTENSION, path);
                        fOut = new FileOutputStream(saveFile);
                        fOut.write(jpegImageData);
                        fOut.flush();
                        fOut.close();

                        return saveFile;
                    } else {
                        Log.w(TAG, "Share failed -- Unable to create share image directory.");
                    }
                } catch (IOException ie) {
                    if (fOut != null) {
                        try {
                            fOut.close();
                        } catch (IOException e) {
                            // Ignore exception.
                        }
                    }
                }

                return null;
            }

            @Override
            protected void onPostExecute(File saveFile) {
                if (saveFile == null) return;

                if (ApplicationStatus.getStateForApplication()
                        != ApplicationState.HAS_DESTROYED_ACTIVITIES) {
                    Uri imageUri = UiUtils.getUriForImageCaptureFile(activity, saveFile);

                    Intent chooserIntent = Intent.createChooser(getShareImageIntent(imageUri),
                            activity.getString(R.string.share_link_chooser_title));
                    activity.startActivity(chooserIntent);
                }
            }
        }.execute();
    }

    /**
     * Creates and shows a share intent picker dialog.
     *
     * @param saveLastUsed Whether to save the chosen activity for future direct sharing.
     * @param activity Activity that is used to access package manager.
     * @param title Title of the page to be shared.
     * @param text Text to be shared. If both |text| and |url| are supplied, they are concatenated
     *             with a space.
     * @param url URL of the page to be shared.
     * @oaram Uri URI of the offline page to be shared.
     * @param screenshot Screenshot of the page to be shared.
     * @param callback Optional callback to be called when user makes a choice. Will not be called
     *                 if receiving a response when the user makes a choice is not supported (on
     *                 older Android versions).
     */
    private static void showShareDialog(final boolean saveLastUsed, final Activity activity,
            final String title, final String text, final String url, final Uri offlineUri,
            final Bitmap screenshot, @Nullable final TargetChosenCallback callback) {
        Intent intent = getShareIntent(activity, title, text, url, null, null);
        PackageManager manager = activity.getPackageManager();
        List<ResolveInfo> resolveInfoList = manager.queryIntentActivities(intent, 0);
        assert resolveInfoList.size() > 0;
        if (resolveInfoList.size() == 0) return;
        Collections.sort(resolveInfoList, new ResolveInfo.DisplayNameComparator(manager));

        final ShareDialogAdapter adapter =
                new ShareDialogAdapter(activity, manager, resolveInfoList);
        AlertDialog.Builder builder = new AlertDialog.Builder(activity, R.style.AlertDialogTheme);
        builder.setTitle(activity.getString(R.string.share_link_chooser_title));
        builder.setAdapter(adapter, null);

        final AlertDialog dialog = builder.create();
        dialog.show();
        dialog.getListView().setOnItemClickListener(new OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                ResolveInfo info = adapter.getItem(position);
                ActivityInfo ai = info.activityInfo;
                ComponentName component =
                        new ComponentName(ai.applicationInfo.packageName, ai.name);
                if (callback != null) callback.onTargetChosen(component);
                if (saveLastUsed) setLastShareComponentName(component);
                makeIntentAndShare(
                        false, activity, title, text, url, offlineUri, screenshot, component, null);
                dialog.dismiss();
            }
        });
    }

    /**
     * Starts a share intent with the activity that was most recently used to share.
     * If there is no most recently used activity, it does nothing.
     * @param activity Activity that is used to start the share intent.
     * @param title Title of the page to be shared.
     * @param text Text to be shared. If both |text| and |url| are supplied, they are concatenated
     *             with a space.
     * @param url URL of the page to be shared.
     * @oaram Uri URI of the offline page to be shared.
     * @param screenshot Screenshot of the page to be shared.
     */
    private static void shareWithLastUsed(Activity activity, String title, String text, String url,
            Uri offlineUri, Bitmap screenshot) {
        ComponentName component = getLastShareComponentName();
        if (component == null) return;
        makeIntentAndShare(
                false, activity, title, text, url, offlineUri, screenshot, component, null);
    }

    private static void shareIntent(boolean saveLastUsed, Activity activity, Intent sharingIntent,
                                    @Nullable TargetChosenCallback callback) {
        if (sharingIntent.getComponent() != null) {
            // If a component was specified, there should not also be a callback.
            assert callback == null;
            activity.startActivity(sharingIntent);
        } else {
            assert TargetChosenReceiver.isSupported();
            TargetChosenReceiver.sendChooserIntent(saveLastUsed, activity, sharingIntent, callback);
        }
    }

    private static void makeIntentAndShare(final boolean saveLastUsed, final Activity activity,
            final String title, final String text, final String url, final Uri offlineUri,
            final Bitmap screenshot, final ComponentName component,
            @Nullable final TargetChosenCallback callback) {
        if (screenshot == null) {
            Intent intent = getDirectShareIntentForComponent(
                    activity, title, text, url, offlineUri, null, component);
            shareIntent(saveLastUsed, activity, intent, callback);
        } else {
            new AsyncTask<Void, Void, File>() {
                @Override
                protected File doInBackground(Void... params) {
                    FileOutputStream fOut = null;
                    try {
                        File path = new File(UiUtils.getDirectoryForImageCapture(activity),
                                SHARE_IMAGES_DIRECTORY_NAME);
                        if (path.exists() || path.mkdir()) {
                            File saveFile = File.createTempFile(
                                    String.valueOf(System.currentTimeMillis()),
                                    JPEG_EXTENSION, path);
                            fOut = new FileOutputStream(saveFile);
                            screenshot.compress(Bitmap.CompressFormat.JPEG, 85, fOut);
                            fOut.flush();
                            fOut.close();

                            return saveFile;
                        }
                    } catch (IOException ie) {
                        if (fOut != null) {
                            try {
                                fOut.close();
                            } catch (IOException e) {
                                // Ignore exception.
                            }
                        }
                    }

                    return null;
                }

                @Override
                protected void onPostExecute(File saveFile) {
                    if (ApplicationStatus.getStateForApplication()
                            != ApplicationState.HAS_DESTROYED_ACTIVITIES) {
                        Uri screenshotUri = saveFile == null
                                ? null : UiUtils.getUriForImageCaptureFile(activity, saveFile);
                        shareIntent(saveLastUsed, activity,
                                getDirectShareIntentForComponent(activity, title, text, url,
                                            offlineUri, screenshotUri, component),
                                callback);
                    }
                }
            }.execute();
        }
    }

    /**
     * Set the icon and the title for the menu item used for direct share.
     *
     * @param activity Activity that is used to access the package manager.
     * @param item The menu item that is used for direct share
     */
    public static void configureDirectShareMenuItem(Activity activity, MenuItem item) {
        Drawable directShareIcon = null;
        CharSequence directShareTitle = null;

        final ComponentName component = getLastShareComponentName();
        boolean isComponentValid = false;
        if (component != null) {
            Intent intent = getShareIntent(activity, "", "", "", null, null);
            intent.setPackage(component.getPackageName());
            PackageManager manager = activity.getPackageManager();
            List<ResolveInfo> resolveInfoList = manager.queryIntentActivities(intent, 0);
            for (ResolveInfo info : resolveInfoList) {
                ActivityInfo ai = info.activityInfo;
                if (component.equals(new ComponentName(ai.applicationInfo.packageName, ai.name))) {
                    isComponentValid = true;
                    break;
                }
            }
        }
        if (isComponentValid) {
            boolean retrieved = false;
            try {
                final PackageManager pm = activity.getPackageManager();
                AsyncTask<Void, Void, Pair<Drawable, CharSequence>> task =
                        new AsyncTask<Void, Void, Pair<Drawable, CharSequence>>() {
                            @Override
                            protected Pair<Drawable, CharSequence> doInBackground(Void... params) {
                                Drawable directShareIcon = null;
                                CharSequence directShareTitle = null;
                                try {
                                    directShareIcon = pm.getActivityIcon(component);
                                    ApplicationInfo ai =
                                            pm.getApplicationInfo(component.getPackageName(), 0);
                                    directShareTitle = pm.getApplicationLabel(ai);
                                } catch (NameNotFoundException exception) {
                                    // Use the default null values.
                                }
                                return new Pair<Drawable, CharSequence>(
                                        directShareIcon, directShareTitle);
                            }
                        };
                task.execute();
                Pair<Drawable, CharSequence> result =
                        task.get(COMPONENT_INFO_READ_TIMEOUT_IN_MS, TimeUnit.MILLISECONDS);
                directShareIcon = result.first;
                directShareTitle = result.second;
                retrieved = true;
            } catch (InterruptedException ie) {
                // Use the default null values.
            } catch (ExecutionException ee) {
                // Use the default null values.
            } catch (TimeoutException te) {
                // Use the default null values.
            }
            RecordHistogram.recordBooleanHistogram(
                    "Android.IsLastSharedAppInfoRetrieved", retrieved);
        }

        item.setIcon(directShareIcon);
        if (directShareTitle != null) {
            item.setTitle(activity.getString(R.string.accessibility_menu_share_via,
                    directShareTitle));
        }
    }

    @VisibleForTesting
    public static Intent getShareIntent(Activity activity, String title, String text, String url,
            Uri offlineUri, Uri screenshotUri) {
        if (!TextUtils.isEmpty(url)) {
            url = DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(url);
            if (!TextUtils.isEmpty(text)) {
                // Concatenate text and URL with a space.
                text = text + " " + url;
            } else {
                text = url;
            }
        }

        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.addFlags(ApiCompatibilityUtils.getActivityNewDocumentFlag());
        intent.putExtra(Intent.EXTRA_SUBJECT, title);
        intent.putExtra(Intent.EXTRA_TEXT, text);
        intent.putExtra(EXTRA_TASK_ID, activity.getTaskId());

        if (screenshotUri != null) {
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        }
        if (screenshotUri != null) {
            // To give read access to an Intent target, we need to put |screenshotUri| in clipData
            // because adding Intent.FLAG_GRANT_READ_URI_PERMISSION doesn't work for
            // EXTRA_SHARE_SCREENSHOT_AS_STREAM.
            intent.setClipData(ClipData.newRawUri("", screenshotUri));
            intent.putExtra(EXTRA_SHARE_SCREENSHOT_AS_STREAM, screenshotUri);
        }
        if (offlineUri == null) {
            intent.setType("text/plain");
        } else {
            intent.setType("multipart/related");
            intent.putExtra(Intent.EXTRA_STREAM, offlineUri);
        }
        return intent;
    }

    private static Intent getShareImageIntent(Uri imageUri) {
        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.addFlags(ApiCompatibilityUtils.getActivityNewDocumentFlag());
        intent.setType("image/jpeg");
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        intent.putExtra(Intent.EXTRA_STREAM, imageUri);
        return intent;
    }

    private static Intent getDirectShareIntentForComponent(Activity activity, String title,
            String text, String url, Uri offlineUri, Uri screenshotUri, ComponentName component) {
        Intent intent = getShareIntent(activity, title, text, url, offlineUri, screenshotUri);
        intent.addFlags(Intent.FLAG_ACTIVITY_FORWARD_RESULT
                | Intent.FLAG_ACTIVITY_PREVIOUS_IS_TOP);
        intent.setComponent(component);
        return intent;
    }

    private static ComponentName getLastShareComponentName() {
        SharedPreferences preferences = ContextUtils.getAppSharedPreferences();
        String packageName = preferences.getString(PACKAGE_NAME_KEY, null);
        String className = preferences.getString(CLASS_NAME_KEY, null);
        if (packageName == null || className == null) return null;
        return new ComponentName(packageName, className);
    }

    private static void setLastShareComponentName(ComponentName component) {
        SharedPreferences preferences = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = preferences.edit();
        editor.putString(PACKAGE_NAME_KEY, component.getPackageName());
        editor.putString(CLASS_NAME_KEY, component.getClassName());
        editor.apply();
    }
}
