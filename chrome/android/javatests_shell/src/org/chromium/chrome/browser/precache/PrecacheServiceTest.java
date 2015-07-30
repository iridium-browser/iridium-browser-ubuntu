// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.precache;

import android.content.Intent;
import android.test.ServiceTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.components.precache.MockDeviceState;

/**
 * Tests of {@link PrecacheService}.
 */
public class PrecacheServiceTest extends ServiceTestCase<MockPrecacheService> {

    /**
     * Mock of the {@link PrecacheLauncher}.
     */
    static class MockPrecacheLauncher extends PrecacheLauncher {

        private MockPrecacheService mService;

        public void setService(MockPrecacheService service) {
            mService = service;
        }

        @Override
        public void destroy() {}

        @Override
        public void start() {}

        @Override
        public void cancel() {}

        @Override
        protected void onPrecacheCompleted() {
            mService.handlePrecacheCompleted();
        }
    }

    private final MockPrecacheLauncher mPrecacheLauncher = new MockPrecacheLauncher();

    public PrecacheServiceTest() {
        super(MockPrecacheService.class);
    }

    @Override
    protected void setupService() {
        super.setupService();
        mPrecacheLauncher.setService(getService());
        getService().setPrecacheLauncher(mPrecacheLauncher);
    }

    private void startAndChangeDeviceState(MockPrecacheService service, boolean powerIsConnected,
            boolean isInteractive, boolean wifiIsAvailable) {
        AdvancedMockContext context = new AdvancedMockContext();
        getService().setDeviceState(new MockDeviceState(0, true, false, true));
        assertFalse("Precaching should not be in progress initially", service.isPrecaching());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Intent intent = new Intent(getService(), MockPrecacheService.class);
                intent.setAction(PrecacheService.ACTION_START_PRECACHE);
                startService(intent);
            }
        });
        assertTrue("Precaching should be in progress after start", service.isPrecaching());
        getService().setDeviceState(
                new MockDeviceState(0, powerIsConnected, isInteractive, wifiIsAvailable));
        service.getDeviceStateReceiver().onReceive(context, new Intent());
    }

    /** Tests that disconnecting power stops a precache. */
    @SmallTest
    @Feature({"Precache"})
    public void testPrecacheWhenPowerDisconnects() {
        setupService();
        startAndChangeDeviceState(getService(), false, false, true);
        assertFalse("Precaching should not be in progress when power is disconnected",
                getService().isPrecaching());

    }

    /** Tests that the device becoming interactive stops a precache. */
    @SmallTest
    @Feature({"Precache"})
    public void testPrecacheWhenDeviceBecomesInteractive() {
        setupService();
        startAndChangeDeviceState(getService(), true, true, true);
        assertFalse("Precaching should not be in progress when the device is interactive",
                getService().isPrecaching());
    }

    /** Tests that going off of Wi-Fi stops a precache. */
    @SmallTest
    @Feature({"Precache"})
    public void testPrecacheWhenNoLongerWifi() {
        setupService();
        startAndChangeDeviceState(getService(), true, true, true);
        assertFalse("Precaching should not be in progress when the network is not Wi-Fi",
                getService().isPrecaching());
    }

    /** Tests that precaching continues if prerequisites are still met. */
    @SmallTest
    @Feature({"Precache"})
    public void testPrecacheWhenPrerequisitesStillMet() {
        setupService();
        startAndChangeDeviceState(getService(), true, false, true);
        assertTrue("Precaching should be in progress", getService().isPrecaching());
    }

    /**
     * Tests that precaching continues until completion while the prerequisite device states
     * (Wifi is in use, power is connected, and the device is non-interactive) continue to be met.
     */
    @SmallTest
    @Feature({"Precache"})
    public void testPrecacheCompleted() {
        setupService();
        startAndChangeDeviceState(getService(), true, false, true);
        assertTrue("Precaching should be in progress", getService().isPrecaching());
        mPrecacheLauncher.onPrecacheCompleted();
        assertFalse("Precaching should not be in progress after completion",
                getService().isPrecaching());
    }
}
