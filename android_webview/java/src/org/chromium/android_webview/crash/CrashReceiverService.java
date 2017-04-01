// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.crash;

import android.annotation.TargetApi;
import android.app.Service;
import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Binder;
import android.os.Build;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.components.minidump_uploader.CrashFileManager;

import java.io.File;
import java.io.IOException;
import java.util.List;

/**
 * Service that is responsible for receiving crash dumps from an application, for upload.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class CrashReceiverService extends Service {
    private static final String TAG = "CrashReceiverService";

    private static final String WEBVIEW_CRASH_DIR = "WebView_Crashes";
    private static final String WEBVIEW_TMP_CRASH_DIR = "WebView_Crashes_Tmp";

    private static final int MINIDUMP_UPLOADING_JOB_ID = 42;
    // Initial back-off time for upload-job, this is set to a fairly high number (30 minutes) to
    // increase the chance of performing uploads in batches if the initial upload fails.
    private static final int JOB_BACKOFF_TIME_IN_MS = 1000 * 60 * 30;
    // Back-off policy for upload-job.
    private static final int JOB_BACKOFF_POLICY = JobInfo.BACKOFF_POLICY_EXPONENTIAL;

    private Object mCopyingLock = new Object();
    private boolean mIsCopying = false;

    // same switch as kEnableCrashReporterForTesting in //base/base_switches.h
    static final String CRASH_UPLOADS_ENABLED_FOR_TESTING_SWITCH =
            "enable-crash-reporter-for-testing";

    @Override
    public void onCreate() {
        super.onCreate();
        SynchronizedWebViewCommandLine.initOnSeparateThread();
    }

    private final ICrashReceiverService.Stub mBinder = new ICrashReceiverService.Stub() {
        @Override
        public void transmitCrashes(ParcelFileDescriptor[] fileDescriptors) {
            // TODO(gsennton): replace this check with a check for Android Checkbox when we have
            // access to that value through GmsCore.
            if (!SynchronizedWebViewCommandLine.hasSwitch(
                        CRASH_UPLOADS_ENABLED_FOR_TESTING_SWITCH)) {
                Log.i(TAG, "Crash reporting is not enabled, bailing!");
                return;
            }
            int uid = Binder.getCallingUid();
            performMinidumpCopyingSerially(
                    CrashReceiverService.this, uid, fileDescriptors, true /* scheduleUploads */);
        }
    };

    /**
     * Copies minidumps in a synchronized way, waiting for any already started copying operations to
     * finish before copying the current dumps.
     * @param scheduleUploads whether to ask JobScheduler to schedule an upload-job (avoid this
     * during testing).
     */
    @VisibleForTesting
    public void performMinidumpCopyingSerially(Context context, int uid,
            ParcelFileDescriptor[] fileDescriptors, boolean scheduleUploads) {
        if (!waitUntilWeCanCopy()) {
            Log.e(TAG, "something went wrong when waiting to copy minidumps, bailing!");
            return;
        }

        try {
            boolean copySucceeded = copyMinidumps(context, uid, fileDescriptors);
            if (copySucceeded && scheduleUploads) {
                // Only schedule a new job if there actually are any files to upload.
                scheduleNewJobIfNoJobsActive();
            }
        } finally {
            synchronized (mCopyingLock) {
                mIsCopying = false;
                mCopyingLock.notifyAll();
            }
        }
    }

    /**
     * Wait until we are allowed to copy minidumps.
     * @return whether we are actually allowed to copy the files - if false we should just bail.
     */
    private boolean waitUntilWeCanCopy() {
        synchronized (mCopyingLock) {
            while (mIsCopying) {
                try {
                    mCopyingLock.wait();
                } catch (InterruptedException e) {
                    Log.e(TAG, "Was interrupted when waiting to copy minidumps", e);
                    return false;
                }
            }
            mIsCopying = true;
            return true;
        }
    }

    /**
     * @return the currently pending job with ID MINIDUMP_UPLOADING_JOB_ID, or null if no such job
     * exists.
     */
    private static JobInfo getCurrentPendingJob(JobScheduler jobScheduler) {
        List<JobInfo> pendingJobs = jobScheduler.getAllPendingJobs();
        for (JobInfo job : pendingJobs) {
            if (job.getId() == MINIDUMP_UPLOADING_JOB_ID) return job;
        }
        return null;
    }

    private void scheduleNewJobIfNoJobsActive() {
        JobScheduler jobScheduler = (JobScheduler) getSystemService(Context.JOB_SCHEDULER_SERVICE);
        if (getCurrentPendingJob(jobScheduler) != null) {
            return;
        }
        JobInfo newJob = new JobInfo
                .Builder(MINIDUMP_UPLOADING_JOB_ID /* jobId */,
                    new ComponentName(this, MinidumpUploadJobService.class))
                .setRequiredNetworkType(JobInfo.NETWORK_TYPE_UNMETERED)
                // Minimum delay when a job is retried (a retry will happen when there are minidumps
                // left after trying to upload all minidumps - this could e.g. happen if we add more
                // minidumps at the same time as uploading old ones).
                .setBackoffCriteria(JOB_BACKOFF_TIME_IN_MS, JOB_BACKOFF_POLICY)
                .build();
        if (jobScheduler.schedule(newJob) == JobScheduler.RESULT_FAILURE) {
            throw new RuntimeException("couldn't schedule " + newJob);
        }
    }

    /**
     * Copy minidumps from the {@param fileDescriptors} to the directory where WebView stores its
     * minidump files. {@param context} is used to look up the directory in which the files will be
     * saved.
     * @return whether any minidump was copied.
     */
    @VisibleForTesting
    public static boolean copyMinidumps(
            Context context, int uid, ParcelFileDescriptor[] fileDescriptors) {
        CrashFileManager crashFileManager = new CrashFileManager(createWebViewCrashDir(context));
        boolean copiedAnything = false;
        if (fileDescriptors != null) {
            for (ParcelFileDescriptor fd : fileDescriptors) {
                if (fd == null) continue;
                try {
                    File copiedFile = crashFileManager.copyMinidumpFromFD(fd.getFileDescriptor(),
                            getWebViewTmpCrashDir(context), uid);
                    if (copiedFile == null) {
                        Log.w(TAG, "failed to copy minidump from " + fd.toString());
                        // TODO(gsennton): add UMA metric to ensure we aren't losing too many
                        // minidumps here.
                    } else {
                        copiedAnything = true;
                    }
                } catch (IOException e) {
                    Log.w(TAG, "failed to copy minidump from " + fd.toString() + ": "
                            + e.getMessage());
                } finally {
                    deleteFilesInWebViewTmpDirIfExists(context);
                }
            }
        }
        return copiedAnything;
    }

    /**
     * Delete all files in the directory where temporary files from this Service are stored.
     */
    @VisibleForTesting
    public static void deleteFilesInWebViewTmpDirIfExists(Context context) {
        deleteFilesInDirIfExists(getWebViewTmpCrashDir(context));
    }

    private static void deleteFilesInDirIfExists(File directory) {
        if (directory.isDirectory()) {
            File[] files = directory.listFiles();
            if (files != null) {
                for (File file : files) {
                    if (!file.delete()) {
                        Log.w(TAG, "Couldn't delete file " + file.getAbsolutePath());
                    }
                }
            }
        }
    }

    /**
     * Create the directory in which WebView wlll store its minidumps.
     * WebView needs a crash directory different from Chrome's to ensure Chrome's and WebView's
     * minidump handling won't clash in cases where both Chrome and WebView are provided by the
     * same app (Monochrome).
     * @param context Android Context used to find a cache-directory where minidumps can be stored.
     * @return a reference to the created directory, or null if the creation failed.
     */
    @VisibleForTesting
    public static File createWebViewCrashDir(Context context) {
        File dir = getWebViewCrashDir(context);
        if (dir.isDirectory() || dir.mkdirs()) {
            return dir;
        }
        return null;
    }

    /**
     * Fetch the crash directory where WebView stores its minidumps.
     * @param context Android Context used to find a cache-directory where minidumps can be stored.
     * @return a File pointing to the crash directory.
     */
    @VisibleForTesting
    public static File getWebViewCrashDir(Context context) {
        return new File(context.getCacheDir(), WEBVIEW_CRASH_DIR);
    }

    /**
     * Directory where we store files temporarily when copying from an app process.
     * @param context Android Context used to find a cache-directory where minidumps can be stored.
     */
    @VisibleForTesting
    public static File getWebViewTmpCrashDir(Context context) {
        return new File(context.getCacheDir(), WEBVIEW_TMP_CRASH_DIR);
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }
}
