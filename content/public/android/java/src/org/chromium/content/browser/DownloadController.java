// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.ui.base.WindowAndroid.FileAccessCallback;

/**
 * Java counterpart of android DownloadController.
 *
 * Its a singleton class instantiated by the C++ DownloadController.
 */
@JNINamespace("content")
public class DownloadController {
    private static final String LOGTAG = "DownloadController";
    private static final DownloadController sInstance = new DownloadController();

    /**
     * Class for notifying the application that download has completed.
     */
    public interface DownloadNotificationService {
        /**
         * Notify the host application that a download is finished.
         * @param downloadInfo Information about the completed download.
         */
        void onDownloadCompleted(final DownloadInfo downloadInfo);

        /**
         * Notify the host application that a download is in progress.
         * @param downloadInfo Information about the in-progress download.
         */
        void onDownloadUpdated(final DownloadInfo downloadInfo);
    }

    private static DownloadNotificationService sDownloadNotificationService;

    @CalledByNative
    public static DownloadController getInstance() {
        return sInstance;
    }

    private DownloadController() {
        nativeInit();
    }

    private static ContentViewDownloadDelegate downloadDelegateFromView(ContentViewCore view) {
        return view.getDownloadDelegate();
    }

    public static void setDownloadNotificationService(DownloadNotificationService service) {
        sDownloadNotificationService = service;
    }

    /**
     * Notifies the download delegate of a new GET download and passes all the information
     * needed to download the file.
     *
     * The download delegate is expected to handle the download.
     */
    @CalledByNative
    public void newHttpGetDownload(ContentViewCore view, String url,
            String userAgent, String contentDisposition, String mimeType,
            String cookie, String referer, boolean hasUserGesture,
            String filename, long contentLength) {
        ContentViewDownloadDelegate downloadDelegate = downloadDelegateFromView(view);

        if (downloadDelegate != null) {
            DownloadInfo downloadInfo = new DownloadInfo.Builder()
                    .setUrl(url)
                    .setUserAgent(userAgent)
                    .setContentDisposition(contentDisposition)
                    .setMimeType(mimeType)
                    .setCookie(cookie)
                    .setReferer(referer)
                    .setHasUserGesture(hasUserGesture)
                    .setFileName(filename)
                    .setContentLength(contentLength)
                    .setIsGETRequest(true)
                    .build();
            downloadDelegate.requestHttpGetDownload(downloadInfo);
        }
    }

    /**
     * Notifies the download delegate that a new download has started. This can
     * be either a POST download or a GET download with authentication.
     * @param view ContentViewCore associated with the download item.
     * @param filename File name of the downloaded file.
     * @param mimeType Mime of the downloaded item.
     */
    @CalledByNative
    public void onDownloadStarted(ContentViewCore view, String filename, String mimeType) {
        ContentViewDownloadDelegate downloadDelegate = downloadDelegateFromView(view);

        if (downloadDelegate != null) {
            downloadDelegate.onDownloadStarted(filename, mimeType);
        }
    }

    /**
     * Notifies the download delegate that a download completed and passes along info about the
     * download. This can be either a POST download or a GET download with authentication.
     */
    @CalledByNative
    public void onDownloadCompleted(Context context, String url, String mimeType,
            String filename, String path, long contentLength, boolean successful, int downloadId,
            boolean hasUserGesture) {
        if (sDownloadNotificationService != null) {
            DownloadInfo downloadInfo = new DownloadInfo.Builder()
                    .setUrl(url)
                    .setMimeType(mimeType)
                    .setFileName(filename)
                    .setFilePath(path)
                    .setContentLength(contentLength)
                    .setIsSuccessful(successful)
                    .setDescription(filename)
                    .setDownloadId(downloadId)
                    .setHasDownloadId(true)
                    .setHasUserGesture(hasUserGesture)
                    .build();
            sDownloadNotificationService.onDownloadCompleted(downloadInfo);
        }
    }

    /**
     * Notifies the download delegate about progress of a download. Downloads that use Chrome
     * network stack use custom notification to display the progress of downloads.
     */
    @CalledByNative
    public void onDownloadUpdated(Context context, String url, String mimeType,
            String filename, String path, long contentLength, boolean successful, int downloadId,
            int percentCompleted, long timeRemainingInMs, boolean hasUserGesture) {
        if (sDownloadNotificationService != null) {
            DownloadInfo downloadInfo = new DownloadInfo.Builder()
                    .setUrl(url)
                    .setMimeType(mimeType)
                    .setFileName(filename)
                    .setFilePath(path)
                    .setContentLength(contentLength)
                    .setIsSuccessful(successful)
                    .setDescription(filename)
                    .setDownloadId(downloadId)
                    .setHasDownloadId(true)
                    .setPercentCompleted(percentCompleted)
                    .setTimeRemainingInMillis(timeRemainingInMs)
                    .setHasUserGesture(hasUserGesture)
                    .build();
            sDownloadNotificationService.onDownloadUpdated(downloadInfo);
        }
    }

    /**
     * Notifies the download delegate that a dangerous download started.
     */
    @CalledByNative
    public void onDangerousDownload(ContentViewCore view, String filename,
            int downloadId) {
        ContentViewDownloadDelegate downloadDelegate = downloadDelegateFromView(view);
        if (downloadDelegate != null) {
            downloadDelegate.onDangerousDownload(filename, downloadId);
        }
    }

    /**
     * Returns whether file access is allowed.
     *
     * @param view The ContentViewCore to access file system.
     * @return true if allowed, or false otherwise.
     */
    @CalledByNative
    private boolean hasFileAccess(ContentViewCore view) {
        return view.getWindowAndroid().hasFileAccess();
    }

    /**
     * Called to prompt user with the file access permission.
     *
     * @param view The ContentViewCore to access file system.
     * @param callbackId The native callback function pointer.
     */
    @CalledByNative
    private void requestFileAccess(ContentViewCore view, final long callbackId) {
        FileAccessCallback callback = new FileAccessCallback() {
            @Override
            public void onFileAccessResult(boolean granted) {
                nativeOnRequestFileAccessResult(callbackId, granted);
            }
        };
        view.getWindowAndroid().requestFileAccess(callback);
    }

    // native methods
    private native void nativeInit();
    private native void nativeOnRequestFileAccessResult(long callbackId, boolean granted);
}
