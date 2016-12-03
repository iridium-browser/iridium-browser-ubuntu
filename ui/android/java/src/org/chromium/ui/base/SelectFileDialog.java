// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.Manifest;
import android.annotation.TargetApi;
import android.app.Activity;
import android.content.ClipData;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Build;
import android.provider.MediaStore;
import android.text.TextUtils;
import android.util.Log;

import org.chromium.base.ContentUriUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.ui.R;
import org.chromium.ui.UiUtils;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * A dialog that is triggered from a file input field that allows a user to select a file based on
 * a set of accepted file types. The path of the selected file is passed to the native dialog.
 */
@JNINamespace("ui")
@MainDex
public class SelectFileDialog
        implements WindowAndroid.IntentCallback, WindowAndroid.PermissionCallback {
    private static final String TAG = "SelectFileDialog";
    private static final String IMAGE_TYPE = "image/";
    private static final String VIDEO_TYPE = "video/";
    private static final String AUDIO_TYPE = "audio/";
    private static final String ALL_IMAGE_TYPES = IMAGE_TYPE + "*";
    private static final String ALL_VIDEO_TYPES = VIDEO_TYPE + "*";
    private static final String ALL_AUDIO_TYPES = AUDIO_TYPE + "*";
    private static final String ANY_TYPES = "*/*";

    /**
     * If set, overrides the WindowAndroid passed in {@link selectFile()}.
     */
    private static WindowAndroid sOverrideWindowAndroid = null;

    private final long mNativeSelectFileDialog;
    private List<String> mFileTypes;
    private boolean mCapture;
    private boolean mAllowMultiple;
    private Uri mCameraOutputUri;
    private WindowAndroid mWindowAndroid;

    private boolean mSupportsImageCapture;
    private boolean mSupportsVideoCapture;
    private boolean mSupportsAudioCapture;

    private SelectFileDialog(long nativeSelectFileDialog) {
        mNativeSelectFileDialog = nativeSelectFileDialog;
    }

    /**
     * Overrides the WindowAndroid passed in {@link selectFile()}.
     */
    @VisibleForTesting
    public static void setWindowAndroidForTests(WindowAndroid window) {
        sOverrideWindowAndroid = window;
    }

    /**
     * Creates and starts an intent based on the passed fileTypes and capture value.
     * @param fileTypes MIME types requested (i.e. "image/*")
     * @param capture The capture value as described in http://www.w3.org/TR/html-media-capture/
     * @param multiple Whether it should be possible to select multiple files.
     * @param window The WindowAndroid that can show intents
     */
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    @CalledByNative
    private void selectFile(
            String[] fileTypes, boolean capture, boolean multiple, WindowAndroid window) {
        mFileTypes = new ArrayList<String>(Arrays.asList(fileTypes));
        mCapture = capture;
        mAllowMultiple = multiple;
        mWindowAndroid = (sOverrideWindowAndroid == null) ? window : sOverrideWindowAndroid;

        mSupportsImageCapture =
                mWindowAndroid.canResolveActivity(new Intent(MediaStore.ACTION_IMAGE_CAPTURE));
        mSupportsVideoCapture =
                mWindowAndroid.canResolveActivity(new Intent(MediaStore.ACTION_VIDEO_CAPTURE));
        mSupportsAudioCapture =
                mWindowAndroid.canResolveActivity(
                        new Intent(MediaStore.Audio.Media.RECORD_SOUND_ACTION));

        List<String> missingPermissions = new ArrayList<>();
        if (((mSupportsImageCapture && shouldShowImageTypes())
                || (mSupportsVideoCapture && shouldShowVideoTypes()))
                        && !window.hasPermission(Manifest.permission.CAMERA)) {
            missingPermissions.add(Manifest.permission.CAMERA);
        }
        if (mSupportsAudioCapture && shouldShowAudioTypes()
                && !window.hasPermission(Manifest.permission.RECORD_AUDIO)) {
            missingPermissions.add(Manifest.permission.RECORD_AUDIO);
        }

        if (missingPermissions.isEmpty()) {
            launchSelectFileIntent();
        } else {
            window.requestPermissions(
                    missingPermissions.toArray(new String[missingPermissions.size()]), this);
        }
    }

    /**
     * Called to launch an intent to allow user to select files.
     */
    private void launchSelectFileIntent() {
        boolean hasCameraPermission = mWindowAndroid.hasPermission(Manifest.permission.CAMERA);
        if (mSupportsImageCapture && hasCameraPermission) {
            // GetCameraIntentTask will call LaunchSelectFileWithCameraIntent later.
            new GetCameraIntentTask().executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        } else {
            launchSelectFileWithCameraIntent(hasCameraPermission, null);
        }
    }

    /**
     * Called to launch an intent to allow user to select files. If |camera| is null,
     * the select file dialog shouldn't include any files from the camera. Otherwise, user
     * is allowed to choose files from the camera.
     * @param hasCameraPermission Whether accessing camera is allowed.
     * @param camera Intent for selecting files from camera.
     */
    private void launchSelectFileWithCameraIntent(boolean hasCameraPermission, Intent camera) {
        Intent camcorder = null;
        if (mSupportsVideoCapture && hasCameraPermission) {
            camcorder = new Intent(MediaStore.ACTION_VIDEO_CAPTURE);
        }

        boolean hasAudioPermission =
                mWindowAndroid.hasPermission(Manifest.permission.RECORD_AUDIO);
        Intent soundRecorder = null;
        if (mSupportsAudioCapture && hasAudioPermission) {
            soundRecorder = new Intent(MediaStore.Audio.Media.RECORD_SOUND_ACTION);
        }

        // Quick check - if the |capture| parameter is set and |fileTypes| has the appropriate MIME
        // type, we should just launch the appropriate intent. Otherwise build up a chooser based
        // on the accept type and then display that to the user.
        if (captureCamera() && camera != null) {
            if (mWindowAndroid.showIntent(camera, this, R.string.low_memory_error)) return;
        } else if (captureCamcorder() && camcorder != null) {
            if (mWindowAndroid.showIntent(camcorder, this, R.string.low_memory_error)) return;
        } else if (captureMicrophone() && soundRecorder != null) {
            if (mWindowAndroid.showIntent(soundRecorder, this, R.string.low_memory_error)) return;
        }

        Intent getContentIntent = new Intent(Intent.ACTION_GET_CONTENT);
        getContentIntent.addCategory(Intent.CATEGORY_OPENABLE);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2 && mAllowMultiple) {
            getContentIntent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        }

        ArrayList<Intent> extraIntents = new ArrayList<Intent>();
        if (!noSpecificType()) {
            // Create a chooser based on the accept type that was specified in the webpage. Note
            // that if the web page specified multiple accept types, we will have built a generic
            // chooser above.
            if (shouldShowImageTypes()) {
                if (camera != null) extraIntents.add(camera);
                getContentIntent.setType(ALL_IMAGE_TYPES);
            } else if (shouldShowVideoTypes()) {
                if (camcorder != null) extraIntents.add(camcorder);
                getContentIntent.setType(ALL_VIDEO_TYPES);
            } else if (shouldShowAudioTypes()) {
                if (soundRecorder != null) extraIntents.add(soundRecorder);
                getContentIntent.setType(ALL_AUDIO_TYPES);
            }
        }

        if (extraIntents.isEmpty()) {
            // We couldn't resolve an accept type, so fallback to a generic chooser.
            getContentIntent.setType(ANY_TYPES);
            if (camera != null) extraIntents.add(camera);
            if (camcorder != null) extraIntents.add(camcorder);
            if (soundRecorder != null) extraIntents.add(soundRecorder);
        }

        Intent chooser = new Intent(Intent.ACTION_CHOOSER);
        if (!extraIntents.isEmpty()) {
            chooser.putExtra(Intent.EXTRA_INITIAL_INTENTS,
                    extraIntents.toArray(new Intent[] { }));
        }
        chooser.putExtra(Intent.EXTRA_INTENT, getContentIntent);

        if (!mWindowAndroid.showIntent(chooser, this, R.string.low_memory_error)) {
            onFileNotSelected();
        }
    }

    private class GetCameraIntentTask extends AsyncTask<Void, Void, Uri> {
        @Override
        public Uri doInBackground(Void...voids) {
            try {
                Context context = mWindowAndroid.getApplicationContext();
                return UiUtils.getUriForImageCaptureFile(context, getFileForImageCapture(context));
            } catch (IOException e) {
                Log.e(TAG, "Cannot retrieve content uri from file", e);
                return null;
            }
        }

        @Override
        protected void onPostExecute(Uri result) {
            mCameraOutputUri = result;
            if (mCameraOutputUri == null && captureCamera()) {
                onFileNotSelected();
                return;
            }

            Intent camera = new Intent(MediaStore.ACTION_IMAGE_CAPTURE);
            camera.setFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                    | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            camera.putExtra(MediaStore.EXTRA_OUTPUT, mCameraOutputUri);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
                camera.setClipData(ClipData.newUri(
                        mWindowAndroid.getApplicationContext().getContentResolver(),
                        UiUtils.IMAGE_FILE_PATH, mCameraOutputUri));
            }
            launchSelectFileWithCameraIntent(true, camera);
        }
    }

    /**
     * Get a file for the image capture operation. For devices with JB MR2 or
     * latter android versions, the file is put under IMAGE_FILE_PATH directory.
     * For ICS devices, the file is put under CAPTURE_IMAGE_DIRECTORY.
     *
     * @param context The application context.
     * @return file path for the captured image to be stored.
     */
    private File getFileForImageCapture(Context context) throws IOException {
        assert !ThreadUtils.runningOnUiThread();
        File photoFile = File.createTempFile(String.valueOf(System.currentTimeMillis()), ".jpg",
                UiUtils.getDirectoryForImageCapture(context));
        return photoFile;
    }

    /**
     * Callback method to handle the intent results and pass on the path to the native
     * SelectFileDialog.
     * @param window The window that has access to the application activity.
     * @param resultCode The result code whether the intent returned successfully.
     * @param contentResolver The content resolver used to extract the path of the selected file.
     * @param results The results of the requested intent.
     */
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    @Override
    public void onIntentCompleted(WindowAndroid window, int resultCode,
            ContentResolver contentResolver, Intent results) {
        if (resultCode != Activity.RESULT_OK) {
            onFileNotSelected();
            return;
        }

        if (results == null
                || (results.getData() == null
                           && (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR2
                                      || results.getClipData() == null))) {
            // If we have a successful return but no data, then assume this is the camera returning
            // the photo that we requested.
            // If the uri is a file, we need to convert it to the absolute path or otherwise
            // android cannot handle it correctly on some earlier versions.
            // http://crbug.com/423338.
            String path = ContentResolver.SCHEME_FILE.equals(mCameraOutputUri.getScheme())
                    ? mCameraOutputUri.getPath() : mCameraOutputUri.toString();
            nativeOnFileSelected(mNativeSelectFileDialog, path,
                    mCameraOutputUri.getLastPathSegment());
            // Broadcast to the media scanner that there's a new photo on the device so it will
            // show up right away in the gallery (rather than waiting until the next time the media
            // scanner runs).
            window.sendBroadcast(new Intent(
                    Intent.ACTION_MEDIA_SCANNER_SCAN_FILE, mCameraOutputUri));
            return;
        }

        // Path for when EXTRA_ALLOW_MULTIPLE Intent extra has been defined. Each of the selected
        // files will be shared as an entry on the Intent's ClipData. This functionality is only
        // available in Android JellyBean MR2 and higher.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2
                && results.getData() == null && results.getClipData() != null) {
            ClipData clipData = results.getClipData();

            int itemCount = clipData.getItemCount();
            if (itemCount == 0) {
                onFileNotSelected();
                return;
            }

            Uri[] filePathArray = new Uri[itemCount];
            for (int i = 0; i < itemCount; ++i) {
                filePathArray[i] = clipData.getItemAt(i).getUri();
            }
            GetDisplayNameTask task = new GetDisplayNameTask(contentResolver, true);
            task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR, filePathArray);
            return;
        }

        if (ContentResolver.SCHEME_FILE.equals(results.getData().getScheme())) {
            nativeOnFileSelected(
                    mNativeSelectFileDialog, results.getData().getSchemeSpecificPart(), "");
            return;
        }

        if (ContentResolver.SCHEME_CONTENT.equals(results.getScheme())) {
            GetDisplayNameTask task = new GetDisplayNameTask(contentResolver, false);
            task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR, results.getData());
            return;
        }

        onFileNotSelected();
        window.showError(R.string.opening_file_error);
    }

    @Override
    public void onRequestPermissionsResult(String[] permissions, int[] grantResults) {
        for (int i = 0; i < grantResults.length; i++) {
            if (grantResults[i] == PackageManager.PERMISSION_DENIED && mCapture) {
                onFileNotSelected();
                return;
            }
        }
        launchSelectFileIntent();
    }

    private void onFileNotSelected() {
        nativeOnFileNotSelected(mNativeSelectFileDialog);
    }

    private boolean noSpecificType() {
        // We use a single Intent to decide the type of the file chooser we display to the user,
        // which means we can only give it a single type. If there are multiple accept types
        // specified, we will fallback to a generic chooser (unless a capture parameter has been
        // specified, in which case we'll try to satisfy that first.
        return mFileTypes.size() != 1 || mFileTypes.contains(ANY_TYPES);
    }

    private boolean shouldShowTypes(String allTypes, String specificType) {
        if (noSpecificType() || mFileTypes.contains(allTypes)) return true;
        return acceptSpecificType(specificType);
    }

    private boolean shouldShowImageTypes() {
        return shouldShowTypes(ALL_IMAGE_TYPES, IMAGE_TYPE);
    }

    private boolean shouldShowVideoTypes() {
        return shouldShowTypes(ALL_VIDEO_TYPES, VIDEO_TYPE);
    }

    private boolean shouldShowAudioTypes() {
        return shouldShowTypes(ALL_AUDIO_TYPES, AUDIO_TYPE);
    }

    private boolean acceptsSpecificType(String type) {
        return mFileTypes.size() == 1 && TextUtils.equals(mFileTypes.get(0), type);
    }

    private boolean captureCamera() {
        return mCapture && acceptsSpecificType(ALL_IMAGE_TYPES);
    }

    private boolean captureCamcorder() {
        return mCapture && acceptsSpecificType(ALL_VIDEO_TYPES);
    }

    private boolean captureMicrophone() {
        return mCapture && acceptsSpecificType(ALL_AUDIO_TYPES);
    }

    private boolean acceptSpecificType(String accept) {
        for (String type : mFileTypes) {
            if (type.startsWith(accept)) {
                return true;
            }
        }
        return false;
    }

    private class GetDisplayNameTask extends AsyncTask<Uri, Void, String[]> {
        String[] mFilePaths;
        final ContentResolver mContentResolver;
        final boolean mIsMultiple;

        public GetDisplayNameTask(ContentResolver contentResolver, boolean isMultiple) {
            mContentResolver = contentResolver;
            mIsMultiple = isMultiple;
        }

        @Override
        protected String[] doInBackground(Uri...uris) {
            mFilePaths = new String[uris.length];
            String[] displayNames = new String[uris.length];
            try {
                for (int i = 0; i < uris.length; i++) {
                    mFilePaths[i] = uris[i].toString();
                    displayNames[i] = ContentUriUtils.getDisplayName(
                            uris[i], mContentResolver, MediaStore.MediaColumns.DISPLAY_NAME);
                }
            }  catch (SecurityException e) {
                // Some third party apps will present themselves as being able
                // to handle the ACTION_GET_CONTENT intent but then declare themselves
                // as exported=false (or more often omit the exported keyword in
                // the manifest which defaults to false after JB).
                // In those cases trying to access the contents raises a security exception
                // which we should not crash on. See crbug.com/382367 for details.
                Log.w(TAG, "Unable to extract results from the content provider");
                return null;
            }

            return displayNames;
        }

        @Override
        protected void onPostExecute(String[] result) {
            if (result == null) {
                onFileNotSelected();
                return;
            }
            if (mIsMultiple) {
                nativeOnMultipleFilesSelected(mNativeSelectFileDialog, mFilePaths, result);
            } else {
                nativeOnFileSelected(mNativeSelectFileDialog, mFilePaths[0], result[0]);
            }
        }
    }

    @CalledByNative
    private static SelectFileDialog create(long nativeSelectFileDialog) {
        return new SelectFileDialog(nativeSelectFileDialog);
    }

    private native void nativeOnFileSelected(long nativeSelectFileDialogImpl,
            String filePath, String displayName);
    private native void nativeOnMultipleFilesSelected(long nativeSelectFileDialogImpl,
            String[] filePathArray, String[] displayNameArray);
    private native void nativeOnFileNotSelected(long nativeSelectFileDialogImpl);
}
