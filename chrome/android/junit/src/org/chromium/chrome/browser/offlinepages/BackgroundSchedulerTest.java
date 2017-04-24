// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;

import com.google.android.gms.gcm.GcmNetworkManager;
import com.google.android.gms.gcm.Task;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.internal.ShadowExtractor;

import org.chromium.base.BaseChromiumApplication;
import org.chromium.base.test.util.Feature;

/**
 * Unit tests for BackgroundScheduler.
 */
@RunWith(OfflinePageTestRunner.class)
@Config(manifest = Config.NONE, application = BaseChromiumApplication.class,
        shadows = {ShadowGcmNetworkManager.class, ShadowGoogleApiAvailability.class})
public class BackgroundSchedulerTest {
    private Context mContext;
    private TriggerConditions mConditions1 = new TriggerConditions(
            true /* power */, 10 /* battery percentage */, false /* unmetered */);
    private TriggerConditions mConditions2 = new TriggerConditions(
            false /* power */, 0 /* battery percentage */, false /* unmetered */);
    private ShadowGcmNetworkManager mGcmNetworkManager;

    @Before
    public void setUp() throws Exception {
        mContext = RuntimeEnvironment.application;
        mGcmNetworkManager = (ShadowGcmNetworkManager) ShadowExtractor.extract(
                GcmNetworkManager.getInstance(mContext));
        mGcmNetworkManager.clear();
    }

    @Test
    @Feature({"OfflinePages"})
    public void testSchedule() {
        assertNull(mGcmNetworkManager.getScheduledTask());
        BackgroundScheduler.getInstance(mContext).schedule(mConditions1);
        // Check with gcmNetworkManagerShadow that schedule got called.
        assertNotNull(mGcmNetworkManager.getScheduledTask());

        // Verify details of the scheduled task.
        Task task = mGcmNetworkManager.getScheduledTask();
        assertEquals(OfflinePageUtils.TASK_TAG, task.getTag());
        long scheduledTimeMillis = TaskExtrasPacker.unpackTimeFromBundle(task.getExtras());
        assertTrue(scheduledTimeMillis > 0L);
        assertEquals(
                mConditions1, TaskExtrasPacker.unpackTriggerConditionsFromBundle(task.getExtras()));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testCancel() {
        assertNull(mGcmNetworkManager.getScheduledTask());
        BackgroundScheduler.getInstance(mContext).schedule(mConditions1);
        assertNotNull(mGcmNetworkManager.getScheduledTask());

        assertNull(mGcmNetworkManager.getCanceledTask());
        BackgroundScheduler.getInstance(mContext).cancel();
        assertNotNull(mGcmNetworkManager.getCanceledTask());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testReschedulOnUpgrade() {
        assertNull(mGcmNetworkManager.getScheduledTask());
        BackgroundScheduler.getInstance(mContext).rescheduleOfflinePagesTasksOnUpgrade();
        // Check with gcmNetworkManagerShadow that schedule got called.
        assertNotNull(mGcmNetworkManager.getScheduledTask());

        // Verify details of the scheduled task.
        Task task = mGcmNetworkManager.getScheduledTask();
        assertEquals(OfflinePageUtils.TASK_TAG, task.getTag());
        long scheduledTimeMillis = TaskExtrasPacker.unpackTimeFromBundle(task.getExtras());
        assertTrue(scheduledTimeMillis > 0L);
        assertEquals(
                mConditions2, TaskExtrasPacker.unpackTriggerConditionsFromBundle(task.getExtras()));
    }
}
