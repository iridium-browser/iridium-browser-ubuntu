// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.text.TextUtils;
import android.util.Log;

import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.test.BaseActivityInstrumentationTestCase;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.test.util.ApplicationData;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.common.ContentSwitches;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.concurrent.Callable;

/**
 * Base test class for all ChromeShell based tests.
 */
@CommandLineFlags.Add(ContentSwitches.ENABLE_TEST_INTENTS)
public class ChromeShellTestBase extends BaseActivityInstrumentationTestCase<ChromeShellActivity> {
    /** The maximum time the waitForActiveShellToBeDoneLoading method will wait. */
    private static final long WAIT_FOR_ACTIVE_SHELL_LOADING_TIMEOUT = scaleTimeout(10000);
    private static final String TAG = "ChromeShellTestBase";

    public ChromeShellTestBase() {
        super(ChromeShellActivity.class);
    }

    protected static void startChromeBrowserProcessSync(final Context context) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                try {
                    ((ChromeShellApplication) context.getApplicationContext())
                            .startBrowserProcessesAndLoadLibrariesSync(true);
                } catch (ProcessInitException e) {
                    Log.e(TAG, "Unable to load native library.", e);
                }
            }
        });
    }

    /**
     * Starts the {@link ChromeShellActivity} and loads the given URL.
     */
    protected ChromeShellActivity launchChromeShellWithUrl(String url) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (url != null) intent.setData(Uri.parse(url));
        intent.setComponent(new ComponentName(getInstrumentation().getTargetContext(),
                ChromeShellActivity.class));
        setActivityIntent(intent);
        return getActivity();
    }

    /**
     * Starts the {@link ChromeShellActivity} and loads a blank page.
     */
    protected ChromeShellActivity launchChromeShellWithBlankPage() {
        return launchChromeShellWithUrl("about:blank");
    }

    /**
     * Waits for the Active shell to finish loading.  This times out after
     * WAIT_FOR_ACTIVE_SHELL_LOADING_TIMEOUT milliseconds and it shouldn't be used for long
     * loading pages. Instead it should be used more for test initialization. The proper way
     * to wait is to use a TestCallbackHelperContainer after the initial load is completed.
     * @return Whether or not the Shell was actually finished loading.
     * @throws InterruptedException
     */
    protected boolean waitForActiveShellToBeDoneLoading() throws InterruptedException {
        final ChromeShellActivity activity = getActivity();

        // Wait for the Content Shell to be initialized.
        return CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws Exception {
                        ChromeShellTab tab = activity.getActiveTab();
                        if (tab != null) {
                            return !tab.isLoading()
                                    && !TextUtils.isEmpty(tab.getContentViewCore()
                                            .getWebContents().getUrl());
                        } else {
                            return false;
                        }
                    }
                });
            }
        }, WAIT_FOR_ACTIVE_SHELL_LOADING_TIMEOUT, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Clear all files and folders in the ChromeShell's application directory except 'lib'.
     *
     * The 'cache' directory is recreated as an empty directory.
     *
     * @return Whether clearing the application data was successful.
     */
    protected boolean clearAppData() throws InterruptedException {
        return ApplicationData.clearAppData(getInstrumentation().getTargetContext());
    }

    /**
     * Navigates the currently active tab to {@code url} and waits for the page to finish loading.
     */
    public void loadUrl(final String url) throws InterruptedException {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                getActivity().getActiveTab().loadUrl(new LoadUrlParams(url));
            }
        });
        waitForActiveShellToBeDoneLoading();
    }

    /**
     * Navigates the currently active tab to a sanitized version of {@code url}.
     * @param url The potentially unsanitized URL to navigate to.
     */
    public void loadUrlWithSanitization(final String url) throws InterruptedException {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                getActivity().getActiveTab().loadUrlWithSanitization(url);
            }
        });
        waitForActiveShellToBeDoneLoading();
    }

    // TODO(aelias): This method needs to be removed once http://crbug.com/179511 is fixed.
    // Meanwhile, we have to wait if the page has the <meta viewport> tag.
    /**
     * Waits till the ContentViewCore receives the expected page scale factor
     * from the compositor and asserts that this happens.
     */
    protected void assertWaitForPageScaleFactorMatch(final float expectedScale)
            throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getActivity().getActiveTab().getContentViewCore().getScale()
                        == expectedScale;
            }
        }));
    }
}
