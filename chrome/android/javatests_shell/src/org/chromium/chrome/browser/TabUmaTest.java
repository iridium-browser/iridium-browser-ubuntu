// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.shell.ChromeShellActivity;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Tests for Tab-related histogram collection.
 */
public class TabUmaTest extends ChromeShellTestBase {
    private static final String TEST_URL =
            TestHttpServerClient.getUrl("chrome/test/data/android/about.html");

    private ChromeShellActivity mActivity;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mActivity = launchChromeShellWithUrl(TEST_URL);
        assertTrue(waitForActiveShellToBeDoneLoading());

        // This is to fake a "tab show" event that should be happening
        // on browser startup, but isn't currently. This code should
        // eventually be taken out when this issue is fixed.
        final Tab tab = ThreadUtils.runOnUiThreadBlocking(new Callable<Tab>() {
            @Override
            public Tab call() {
                Tab bgTab = Tab.createTabForLazyLoad(mActivity, false, mActivity.getWindowAndroid(),
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND, Tab.INVALID_TAB_ID,
                        new LoadUrlParams(TEST_URL), mActivity.getTabModelSelector());
                bgTab.initialize(null, null, true);
                return bgTab;
            }
        });

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tab.show(TabSelectionType.FROM_NEW);
            }
        });
    }

    /**
     * Verify that Tab.StatusWhenSwitchedBackToForeground is correctly recording lazy loads.
     */
    @MediumTest
    @Feature({"Uma"})
    public void testTabStatusWhenSwitchedToLazyLoads()
            throws ExecutionException, InterruptedException {
        final Tab tab = ThreadUtils.runOnUiThreadBlocking(new Callable<Tab>() {
            @Override
            public Tab call() {
                Tab bgTab = Tab.createTabForLazyLoad(mActivity, false, mActivity.getWindowAndroid(),
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND, Tab.INVALID_TAB_ID,
                        new LoadUrlParams(TEST_URL), mActivity.getTabModelSelector());
                bgTab.initialize(null, null, true);
                return bgTab;
            }
        });

        String histogram = "Tab.StatusWhenSwitchedBackToForeground";
        HistogramDelta lazyLoadCount =
                new HistogramDelta(histogram, TabUma.TAB_STATUS_LAZY_LOAD_FOR_BG_TAB);
        assertEquals(0, lazyLoadCount.getDelta()); // Sanity check.

        // Show the tab and verify that one sample was recorded in the lazy load bucket.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tab.show(TabSelectionType.FROM_USER);
            }
        });
        assertEquals(1, lazyLoadCount.getDelta());

        // Show the tab again and verify that we didn't record another sample.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tab.show(TabSelectionType.FROM_USER);
            }
        });
        assertEquals(1, lazyLoadCount.getDelta());
    }

    /**
     * Verify that Tab.BackgroundLoadStatus is correctly recorded.
     */
    @MediumTest
    @Feature({"Uma"})
    public void testTabBackgroundLoadStatus() throws ExecutionException {
        String histogram = "Tab.BackgroundLoadStatus";
        HistogramDelta shownLoadCount =
                new HistogramDelta(histogram, TabUma.TAB_BACKGROUND_LOAD_SHOWN);
        HistogramDelta lostLoadCount =
                new HistogramDelta(histogram, TabUma.TAB_BACKGROUND_LOAD_LOST);
        HistogramDelta skippedLoadCount =
                new HistogramDelta(histogram, TabUma.TAB_BACKGROUND_LOAD_SKIPPED);
        assertEquals(0, shownLoadCount.getDelta());
        assertEquals(0, lostLoadCount.getDelta());
        assertEquals(0, skippedLoadCount.getDelta());

        // Test a live tab created in background and shown.
        final Tab liveBgTab = ThreadUtils.runOnUiThreadBlocking(new Callable<Tab>() {
            @Override
            public Tab call() {
                Tab bgTab = Tab.createLiveTab(Tab.INVALID_TAB_ID, mActivity, false,
                        mActivity.getWindowAndroid(), TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                        Tab.INVALID_TAB_ID, true, mActivity.getTabModelSelector());
                bgTab.initialize(null, null, true);
                bgTab.loadUrl(new LoadUrlParams(TEST_URL));
                bgTab.show(TabSelectionType.FROM_USER);
                return bgTab;
            }
        });
        assertEquals(1, shownLoadCount.getDelta());
        assertEquals(0, lostLoadCount.getDelta());
        assertEquals(0, skippedLoadCount.getDelta());

        // Test a live tab killed in background before shown.
        final Tab killedBgTab = ThreadUtils.runOnUiThreadBlocking(new Callable<Tab>() {
            @Override
            public Tab call() {
                Tab bgTab = Tab.createLiveTab(Tab.INVALID_TAB_ID, mActivity, false,
                        mActivity.getWindowAndroid(), TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                        Tab.INVALID_TAB_ID, true, mActivity.getTabModelSelector());
                bgTab.initialize(null, null, true);
                bgTab.loadUrl(new LoadUrlParams(TEST_URL));
                // Simulate the renderer being killed by the OS.
                bgTab.simulateRendererKilledForTesting(false);
                bgTab.show(TabSelectionType.FROM_USER);
                return bgTab;
            }
        });
        assertEquals(1, shownLoadCount.getDelta());
        assertEquals(1, lostLoadCount.getDelta());
        assertEquals(0, skippedLoadCount.getDelta());

        // Test a tab created in background but not loaded eagerly.
        final Tab frozenBgTab = ThreadUtils.runOnUiThreadBlocking(new Callable<Tab>() {
            @Override
            public Tab call() {
                Tab bgTab = Tab.createTabForLazyLoad(mActivity, false, mActivity.getWindowAndroid(),
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND, Tab.INVALID_TAB_ID,
                        new LoadUrlParams(TEST_URL), mActivity.getTabModelSelector());
                bgTab.initialize(null, null, true);
                bgTab.show(TabSelectionType.FROM_USER);
                return bgTab;
            }
        });
        assertEquals(1, shownLoadCount.getDelta());
        assertEquals(1, lostLoadCount.getDelta());
        assertEquals(1, skippedLoadCount.getDelta());

        // Show every tab again and make sure we didn't record more samples - this metric should be
        // recorded only on first display.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                liveBgTab.show(TabSelectionType.FROM_USER);
                killedBgTab.show(TabSelectionType.FROM_USER);
                frozenBgTab.show(TabSelectionType.FROM_USER);
            }
        });
        assertEquals(1, shownLoadCount.getDelta());
        assertEquals(1, lostLoadCount.getDelta());
        assertEquals(1, skippedLoadCount.getDelta());
    }
}
