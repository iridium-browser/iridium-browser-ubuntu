// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.app.ActivityManager.RecentTaskInfo;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.text.TextUtils;
import android.util.Log;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.tabmodel.document.ActivityDelegate;

import java.util.ArrayList;
import java.util.List;

/**
 * Deals with Document-related API calls.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class DocumentUtils {
    public static final String TAG = "DocumentUtilities";

    /**
     * Update the Recents entry for the Activity.
     * @param activity Activity to change the entry for.
     * @param title Title to show on the card.
     * @param icon Icon to show on the card.
     * @param color Color to use for the card's bar.
     * @param useDefaultStatusBarColor Whether status bar should be set to default color.
     */
    public static void updateTaskDescription(Activity activity, String title, Bitmap icon,
            int color, boolean useDefaultStatusBarColor) {
        ApiCompatibilityUtils.setTaskDescription(activity, title, icon, color);
        int statusBarColor = useDefaultStatusBarColor
                ? Color.BLACK : BrandColorUtils.computeStatusBarColor(color);
        ApiCompatibilityUtils.setStatusBarColor(activity.getWindow(), statusBarColor);
    }

    /**
     * Finishes tasks other than the one with the given task ID that were started with the given
     * tabId, leaving a unique task to own a Tab with that particular ID.
     * @param tabId ID of the tab to remove duplicates for.
     * @param canonicalTaskId ID of the task will be the only one left with the ID.
     * @return Intent of one of the tasks that were finished.
     */
    public static Intent finishOtherTasksWithTabID(int tabId, int canonicalTaskId) {
        if (tabId == Tab.INVALID_TAB_ID || Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            return null;
        }

        Context context = ApplicationStatus.getApplicationContext();

        ActivityManager manager =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        List<ActivityManager.AppTask> tasksToFinish = new ArrayList<ActivityManager.AppTask>();
        for (ActivityManager.AppTask task : manager.getAppTasks()) {
            RecentTaskInfo taskInfo = getTaskInfoFromTask(task);
            if (taskInfo == null) continue;
            int taskId = taskInfo.id;

            Intent baseIntent = taskInfo.baseIntent;
            int otherTabId = ActivityDelegate.getTabIdFromIntent(baseIntent);

            if (otherTabId == tabId && (taskId == -1 || taskId != canonicalTaskId)) {
                tasksToFinish.add(task);
            }
        }
        return finishAndRemoveTasks(tasksToFinish);
    }

    /**
     * Finishes tasks other than the one with the given ID that were started with the given data
     * in the Intent, removing those tasks from Recents and leaving a unique task with the data.
     * @param data Passed in as part of the Intent's data when starting the Activity.
     * @param canonicalTaskId ID of the task will be the only one left with the ID.
     * @return Intent of one of the tasks that were finished.
     */
    public static Intent finishOtherTasksWithData(Uri data, int canonicalTaskId) {
        if (data == null || Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return null;

        String dataString = data.toString();
        Context context = ApplicationStatus.getApplicationContext();

        ActivityManager manager =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        List<ActivityManager.AppTask> tasksToFinish = new ArrayList<ActivityManager.AppTask>();
        for (ActivityManager.AppTask task : manager.getAppTasks()) {
            RecentTaskInfo taskInfo = getTaskInfoFromTask(task);
            if (taskInfo == null) continue;
            int taskId = taskInfo.id;

            Intent baseIntent = taskInfo.baseIntent;
            String taskData = baseIntent == null ? null : taskInfo.baseIntent.getDataString();

            if (TextUtils.equals(dataString, taskData)
                    && (taskId == -1 || taskId != canonicalTaskId)) {
                tasksToFinish.add(task);
            }
        }
        return finishAndRemoveTasks(tasksToFinish);
    }

    private static Intent finishAndRemoveTasks(List<ActivityManager.AppTask> tasksToFinish) {
        Intent removedIntent = null;
        for (ActivityManager.AppTask task : tasksToFinish) {
            Log.d(TAG, "Removing task with duplicated data: " + task);
            removedIntent = getBaseIntentFromTask(task);
            task.finishAndRemoveTask();
        }
        return removedIntent;
    }

    /**
     * Returns the RecentTaskInfo for the task, if the ActivityManager succeeds in finding the task.
     * @param task AppTask containing information about a task.
     * @return The RecentTaskInfo associated with the task, or null if it couldn't be found.
     */
    public static RecentTaskInfo getTaskInfoFromTask(AppTask task) {
        RecentTaskInfo info = null;
        try {
            info = task.getTaskInfo();
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Failed to retrieve task info: ", e);
        }
        return info;
    }

    /**
     * Returns the baseIntent of the RecentTaskInfo associated with the given task.
     * @param task Task to get the baseIntent for.
     * @return The baseIntent, or null if it couldn't be retrieved.
     */
    public static Intent getBaseIntentFromTask(AppTask task) {
        RecentTaskInfo info = getTaskInfoFromTask(task);
        return info == null ? null : info.baseIntent;
    }

    /**
     * Given an AppTask retrieves the task class name.
     * @param task The app task to use.
     * @param pm The package manager to use for resolving intent.
     * @return Fully qualified class name or null if we were not able to
     * determine it.
     */
    public static String getTaskClassName(AppTask task, PackageManager pm) {
        RecentTaskInfo info = getTaskInfoFromTask(task);
        if (info == null) return null;

        Intent baseIntent = info.baseIntent;
        if (baseIntent == null) {
            return null;
        } else if (baseIntent.getComponent() != null) {
            return baseIntent.getComponent().getClassName();
        } else {
            ResolveInfo resolveInfo = pm.resolveActivity(baseIntent, 0);
            if (resolveInfo == null) return null;
            return resolveInfo.activityInfo.name;
        }
    }
}
