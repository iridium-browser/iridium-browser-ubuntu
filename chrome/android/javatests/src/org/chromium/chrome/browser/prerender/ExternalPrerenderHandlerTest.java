// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.prerender;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.graphics.Rect;
import android.os.Environment;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content.browser.test.NativeLibraryTestBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Callable;

/** Tests for {@link ExternalPrerenderHandler}. */
public class ExternalPrerenderHandlerTest extends NativeLibraryTestBase {
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE2 = "/chrome/test/data/android/about.html";
    private static final int PRERENDER_DELAY_MS = 500;

    private ExternalPrerenderHandler mExternalPrerenderHandler;
    private Profile mProfile;
    private String mTestPage;
    private String mTestPage2;
    private EmbeddedTestServer mTestServer;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        loadNativeLibraryAndInitBrowserProcess();
        mExternalPrerenderHandler = new ExternalPrerenderHandler();

        final Callable<Profile> profileCallable = new Callable<Profile>() {
            @Override
            public Profile call() throws Exception {
                return Profile.getLastUsedProfile();
            }
        };
        mProfile = ThreadUtils.runOnUiThreadBlocking(profileCallable);

        mTestServer = EmbeddedTestServer.createAndStartFileServer(
                getInstrumentation().getContext(), Environment.getExternalStorageDirectory());
        mTestPage = mTestServer.getURL(TEST_PAGE);
        mTestPage2 = mTestServer.getURL(TEST_PAGE2);
    }

    @Override
    protected void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mExternalPrerenderHandler.cancelCurrentPrerender();
            }
        });
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Prerender"})
    @SmallTest
    public void testAddPrerender() throws Exception {
        final WebContents webContents = ensureStartedPrerenderForUrl(mTestPage);
        ensureCompletedPrerenderForUrl(webContents, mTestPage);
    }

    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Prerender"})
    @SmallTest
    public void testAddAndCancelPrerender() throws Exception {
        final WebContents webContents = ensureStartedPrerenderForUrl(mTestPage);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mExternalPrerenderHandler.cancelCurrentPrerender();
                assertFalse(mExternalPrerenderHandler.hasPrerenderedUrl(
                        mProfile, mTestPage, webContents));
            }
        });
    }

    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Prerender"})
    @SmallTest
    public void testAddSeveralPrerenders() throws Exception {
        WebContents webContents = ensureStartedPrerenderForUrl(mTestPage);
        Thread.sleep(PRERENDER_DELAY_MS);
        final WebContents webContents2 = ensureStartedPrerenderForUrl(mTestPage2);

        // Make sure that the second one didn't remove the first one.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertTrue(mExternalPrerenderHandler.hasPrerenderedUrl(
                        mProfile, mTestPage2, webContents2));
            }
        });
        ensureCompletedPrerenderForUrl(webContents, mTestPage);
        ensureCompletedPrerenderForUrl(webContents2, mTestPage2);
    }

    private WebContents ensureStartedPrerenderForUrl(final String url) throws Exception {
        Callable<WebContents> addPrerenderCallable = new Callable<WebContents>() {
            @Override
            public WebContents call() {
                WebContents webContents =
                        mExternalPrerenderHandler.addPrerender(
                                mProfile, url, "", new Rect(), false);
                assertNotNull(webContents);
                assertTrue(mExternalPrerenderHandler.hasPrerenderedUrl(
                        mProfile, url, webContents));
                return webContents;
            }
        };
        return ThreadUtils.runOnUiThreadBlocking(addPrerenderCallable);
    }

    private void ensureCompletedPrerenderForUrl(final WebContents webContents, final String url)
            throws Exception {
        CriteriaHelper.pollUiThread(new Criteria("No Prerender") {
            @Override
            public boolean isSatisfied() {
                return ExternalPrerenderHandler.hasPrerenderedAndFinishedLoadingUrl(
                        mProfile, url, webContents);
            }
        });
    }
}
