// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;

import com.google.android.gms.gcm.GcmNetworkManager;
import com.google.android.gms.gcm.Task;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.BaseChromiumApplication;
import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.SysUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeBackgroundServiceWaiter;
import org.chromium.net.ConnectionType;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.internal.ShadowExtractor;

/**
 * Unit tests for BackgroundOfflinerTask.
 */
@RunWith(OfflinePageTestRunner.class)
@Config(manifest = Config.NONE,
        application = BaseChromiumApplication.class,
        shadows = { ShadowGcmNetworkManager.class })
public class BackgroundOfflinerTaskTest {
    private static final boolean REQUIRE_POWER = true;
    private static final boolean REQUIRE_UNMETERED = true;
    private static final boolean POWER_CONNECTED = true;
    private static final int MINIMUM_BATTERY_LEVEL = 33;
    private static final String IS_LOW_END_DEVICE_SWITCH =
            "--" + BaseSwitches.ENABLE_LOW_END_DEVICE_MODE;

    @Mock
    private OfflinePageUtils mOfflinePageUtils;

    private Bundle mTaskExtras;
    private long mTestTime;
    private StubBackgroundSchedulerProcessor mStubBackgroundSchedulerProcessor;
    private TriggerConditions mTriggerConditions =
            new TriggerConditions(!REQUIRE_POWER, MINIMUM_BATTERY_LEVEL, REQUIRE_UNMETERED);
    private DeviceConditions mDeviceConditions = new DeviceConditions(
            !POWER_CONNECTED, MINIMUM_BATTERY_LEVEL + 5, ConnectionType.CONNECTION_3G);
    private Activity mTestActivity;

    private Context mContext;
    private ShadowGcmNetworkManager mGcmNetworkManager;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        when(mOfflinePageUtils.getDeviceConditionsImpl(any(Context.class)))
                .thenReturn(mDeviceConditions);

        // Build a bundle with trigger conditions.
        mTaskExtras = new Bundle();
        TaskExtrasPacker.packTimeInBundle(mTaskExtras);
        TaskExtrasPacker.packTriggerConditionsInBundle(mTaskExtras, mTriggerConditions);

        OfflinePageUtils.setInstanceForTesting(mOfflinePageUtils);
        mStubBackgroundSchedulerProcessor = new StubBackgroundSchedulerProcessor();
        RecordHistogram.disableForTests();
        mContext =  RuntimeEnvironment.application;
        mGcmNetworkManager = (ShadowGcmNetworkManager) ShadowExtractor.extract(
                GcmNetworkManager.getInstance(mContext));
        mGcmNetworkManager.clear();

        // Run tests as a low-end device.
        CommandLine.init(new String[] {"testcommand", IS_LOW_END_DEVICE_SWITCH});

        // Set up single, stopped Activity.
        ApplicationStatus.destroyForJUnitTests();
        mTestActivity = new Activity();
        ApplicationStatus.onStateChangeForTesting(mTestActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mTestActivity, ActivityState.STOPPED);
    }

    @After
    public void tearDown() throws Exception {
        // Clean up static state for subsequent Robolectric tests.
        CommandLine.reset();
        SysUtils.reset();
        ApplicationStatus.destroyForJUnitTests();
    }

    @Test
    @Feature({"OfflinePages"})
    public void testIncomingTask() {
        BackgroundOfflinerTask task =
                new BackgroundOfflinerTask(mStubBackgroundSchedulerProcessor);
        ChromeBackgroundServiceWaiter waiter = new ChromeBackgroundServiceWaiter(1000);
        task.processBackgroundRequests(mTaskExtras, mDeviceConditions, waiter);

        // Check with ShadowBackgroundBackgroundSchedulerProcessor that it started processing.
        assertTrue(mStubBackgroundSchedulerProcessor.getDidStartProcessing());
        assertSame(mDeviceConditions, mStubBackgroundSchedulerProcessor.getDeviceConditions());

        // Call the callback and then waiter should not block.
        mStubBackgroundSchedulerProcessor.callback();
        waiter.startWaiting();
    }

    @Test
    @Feature({"OfflinePages"})
    public void testIncomingTaskNotStarted() {
        mStubBackgroundSchedulerProcessor.setFailToStart(true);
        BackgroundOfflinerTask task = new BackgroundOfflinerTask(mStubBackgroundSchedulerProcessor);
        ChromeBackgroundServiceWaiter waiter = new ChromeBackgroundServiceWaiter(1000);
        task.processBackgroundRequests(mTaskExtras, mDeviceConditions, waiter);

        // Check with ShadowBackgroundBackgroundSchedulerProcessor that it did not start.
        assertFalse(mStubBackgroundSchedulerProcessor.getDidStartProcessing());

        // The waiter should not block if startProcessing returned false.
        waiter.startWaiting();
    }

    @Test
    @Feature({"OfflinePages"})
    public void testStartBackgroundRequests() {
        BackgroundOfflinerTask task = new BackgroundOfflinerTask(mStubBackgroundSchedulerProcessor);
        ChromeBackgroundServiceWaiter waiter = new ChromeBackgroundServiceWaiter(1);
        assertNull("Nothing scheduled", mGcmNetworkManager.getScheduledTask());
        assertTrue(task.startBackgroundRequests(
                RuntimeEnvironment.application, mTaskExtras, waiter));

        // Check that the backup task was scheduled.
        Task gcmTask = mGcmNetworkManager.getScheduledTask();
        assertNotNull("Backup task scheduled", gcmTask);
        assertEquals(mTriggerConditions,
                TaskExtrasPacker.unpackTriggerConditionsFromBundle(gcmTask.getExtras()));

        // Check with ShadowBackgroundBackgroundSchedulerProcessor that startProcessing got called.
        assertTrue(mStubBackgroundSchedulerProcessor.getDidStartProcessing());
        assertSame(mDeviceConditions, mStubBackgroundSchedulerProcessor.getDeviceConditions());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testStartBackgroundRequestsForLowBatteryLevel() {
        DeviceConditions deviceConditionsLowBattery = new DeviceConditions(
                !POWER_CONNECTED, MINIMUM_BATTERY_LEVEL - 1, ConnectionType.CONNECTION_WIFI);
        when(mOfflinePageUtils.getDeviceConditionsImpl(any(Context.class)))
                .thenReturn(deviceConditionsLowBattery);
        BackgroundOfflinerTask task = new BackgroundOfflinerTask(mStubBackgroundSchedulerProcessor);
        ChromeBackgroundServiceWaiter waiter = new ChromeBackgroundServiceWaiter(1);
        assertNull("Nothing scheduled", mGcmNetworkManager.getScheduledTask());
        assertFalse(task.startBackgroundRequests(
                RuntimeEnvironment.application, mTaskExtras, waiter));

        // Check that the backup task was scheduled.
        Task gcmTask = mGcmNetworkManager.getScheduledTask();
        assertNotNull("Backup task scheduled", gcmTask);
        assertEquals(mTriggerConditions,
                TaskExtrasPacker.unpackTriggerConditionsFromBundle(gcmTask.getExtras()));

        // Check that startProcessing was NOT called.
        assertFalse(mStubBackgroundSchedulerProcessor.getDidStartProcessing());

        // Now verify low battery level but with power connected will start processing.
        DeviceConditions deviceConditionsPowerConnected = new DeviceConditions(
                POWER_CONNECTED, MINIMUM_BATTERY_LEVEL - 1, ConnectionType.CONNECTION_WIFI);
        when(mOfflinePageUtils.getDeviceConditionsImpl(any(Context.class)))
                .thenReturn(deviceConditionsPowerConnected);
        BackgroundOfflinerTask task2 =
                new BackgroundOfflinerTask(mStubBackgroundSchedulerProcessor);
        ChromeBackgroundServiceWaiter waiter2 = new ChromeBackgroundServiceWaiter(1);
        assertTrue(task2.startBackgroundRequests(
                RuntimeEnvironment.application, mTaskExtras, waiter2));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testStartBackgroundRequestsForRunningActivityOnLowEndDevice() {
        BackgroundOfflinerTask task = new BackgroundOfflinerTask(mStubBackgroundSchedulerProcessor);
        ChromeBackgroundServiceWaiter waiter = new ChromeBackgroundServiceWaiter(1);
        assertNull("Nothing scheduled", mGcmNetworkManager.getScheduledTask());

        // Transition the test Activity to a running state.
        ApplicationStatus.onStateChangeForTesting(mTestActivity, ActivityState.STARTED);

        assertFalse(task.startBackgroundRequests(
                RuntimeEnvironment.application, mTaskExtras, waiter));

        // Check that the backup task was scheduled.
        Task gcmTask = mGcmNetworkManager.getScheduledTask();
        assertNotNull("Backup task scheduled", gcmTask);
        assertEquals(mTriggerConditions,
                TaskExtrasPacker.unpackTriggerConditionsFromBundle(gcmTask.getExtras()));

        // Check that startProcessing was NOT called.
        assertFalse(mStubBackgroundSchedulerProcessor.getDidStartProcessing());

        // Now verify will start processing when Activity is stopped.
        ApplicationStatus.onStateChangeForTesting(mTestActivity, ActivityState.STOPPED);
        BackgroundOfflinerTask task2 =
                new BackgroundOfflinerTask(mStubBackgroundSchedulerProcessor);
        ChromeBackgroundServiceWaiter waiter2 = new ChromeBackgroundServiceWaiter(1);
        assertTrue(task2.startBackgroundRequests(
                RuntimeEnvironment.application, mTaskExtras, waiter2));
    }
}
