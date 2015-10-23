// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.net.Uri;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TestWebContentsObserver;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

import java.util.concurrent.TimeoutException;

/**
 * The base class of the WebappActivity tests. It provides the common methods to access the activity
 * UI.  This particular test base only instantiates WebappActivity0.
 */
public abstract class WebappActivityTestBase extends ChromeActivityTestCaseBase<WebappActivity0> {
    static final String WEBAPP_ID = "webapp_id";

    TestWebContentsObserver mTestObserver;

    public WebappActivityTestBase() {
        super(WebappActivity0.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        // Default to a webapp that just loads about:blank to avoid a network load.  This results
        // in the URL bar showing since {@link UrlUtils} cannot parse this type of URL.
        Intent intent = new Intent(getInstrumentation().getTargetContext(), WebappActivity0.class);
        intent.setData(Uri.parse(WebappActivity.WEBAPP_SCHEME + "://" + WEBAPP_ID));
        intent.putExtra(ShortcutHelper.EXTRA_ID, WEBAPP_ID);
        intent.putExtra(ShortcutHelper.EXTRA_URL, "about:blank");
        setActivityIntent(intent);

        waitUntilIdle();

        // TODO(yfriedman): Change callers to be executed on the UI thread. Unfortunately this is
        // super convenient as the caller is nearly always on the test thread which is fine to
        // block and it's cumbersome to keep bouncing to the UI thread.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mTestObserver = new TestWebContentsObserver(
                        getActivity().getActivityTab().getWebContents());
            }
        });
    }

    /**
     * Waits until any loads in progress have completed.
     */
    protected void waitUntilIdle() {
        getInstrumentation().waitForIdleSync();
        try {
            assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return getActivity().getActivityTab() != null
                                && !getActivity().getActivityTab().isLoading();
                    }
                }));
        } catch (InterruptedException exception) {
            fail();
        }

        getInstrumentation().waitForIdleSync();
    }

    /**
     * Loads the URL in the WebappActivity and waits until it has been fully loaded.
     * @param url URL to load.
     */
    @Override
    public int loadUrl(final String url) throws IllegalArgumentException, InterruptedException {
        waitUntilIdle();

        final CallbackHelper startLoadingHelper = mTestObserver.getOnPageStartedHelper();
        final CallbackHelper finishLoadingHelper = mTestObserver.getOnPageFinishedHelper();

        int startLoadingCount = startLoadingHelper.getCallCount();
        int finishLoadingCount = finishLoadingHelper.getCallCount();

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                int pageTransition = PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR;
                getActivity().getActivityTab().loadUrl(new LoadUrlParams(url, pageTransition));
            }
        });

        try {
            startLoadingHelper.waitForCallback(startLoadingCount);
        } catch (TimeoutException e) {
            fail();
        }

        try {
            finishLoadingHelper.waitForCallback(finishLoadingCount);
        } catch (TimeoutException e) {
            fail();
        }
        return 0;
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        // Do nothing
    }
}
