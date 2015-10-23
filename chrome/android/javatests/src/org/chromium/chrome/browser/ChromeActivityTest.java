// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Instrumentation tests for ChromeActivity.
 */
public class ChromeActivityTest extends ChromeTabbedActivityTestBase {
    private static final String FILE_PATH = "chrome/test/data/android/test.html";

    /**
     * Verifies that the front tab receives the hide() call when the activity is stopped (hidden);
     * and that it receives the show() call when the activity is started again. This is a regression
     * test for http://crbug.com/319804 .
     */
    @MediumTest
    public void testTabVisibility() {
        // Create two tabs - tab[0] in the foreground and tab[1] in the background.
        final Tab[] tabs = new Tab[2];
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Foreground tab.
                ChromeTabCreator tabCreator = getActivity().getCurrentTabCreator();
                tabs[0] = tabCreator.createNewTab(
                        new LoadUrlParams(TestHttpServerClient.getUrl(FILE_PATH)),
                                TabLaunchType.FROM_KEYBOARD, null);
                // Background tab.
                tabs[1] = tabCreator.createNewTab(
                        new LoadUrlParams(TestHttpServerClient.getUrl(FILE_PATH)),
                                TabLaunchType.FROM_LONGPRESS_BACKGROUND, null);
            }
        });

        // Verify that the front tab is in the 'visible' state.
        assertFalse(tabs[0].isHidden());
        assertTrue(tabs[1].isHidden());

        // Fake sending the activity to background.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().onPause();
            }
        });
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().onStop();
            }
        });
        // TODO(jdduke): Ensure the Tab's WebContents is hidden despite the
        // Tab itself remaining visible. This prevents visual artifacts during
        // activity transitions.
        assertFalse(tabs[0].isHidden());
        assertTrue(tabs[1].isHidden());

        // Fake bringing the activity back to foreground.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().onStart();
            }
        });
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().onResume();
            }
        });
        // Verify that the front tab is in the 'visible' state.
        assertFalse(tabs[0].isHidden());
        assertTrue(tabs[1].isHidden());
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}
