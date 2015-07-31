// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Intent;
import android.graphics.Bitmap;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.TextUtils;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.shell.ChromeShellActivity;
import org.chromium.chrome.shell.ChromeShellApplication;
import org.chromium.chrome.shell.ChromeShellApplicationObserver;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.Callable;

/**
 * Tests org.chromium.chrome.browser.ShortcutHelper and it's C++ counterpart.
 */
public class ShortcutHelperTest extends ChromeShellTestBase {
    private static final String WEBAPP_ACTION_NAME = "WEBAPP_ACTION";

    private static final String WEBAPP_TITLE = "Webapp shortcut";
    private static final String WEBAPP_HTML = UrlUtils.encodeHtmlDataUri(
            "<html><head>"
            + "<meta name=\"mobile-web-app-capable\" content=\"yes\" />"
            + "<title>" + WEBAPP_TITLE + "</title>"
            + "</head><body>Webapp capable</body></html>");
    private static final String EDITED_WEBAPP_TITLE = "Webapp shortcut edited";

    private static final String SECOND_WEBAPP_TITLE = "Webapp shortcut #2";
    private static final String SECOND_WEBAPP_HTML = UrlUtils.encodeHtmlDataUri(
            "<html><head>"
            + "<meta name=\"mobile-web-app-capable\" content=\"yes\" />"
            + "<title>" + SECOND_WEBAPP_TITLE + "</title>"
            + "</head><body>Webapp capable again</body></html>");

    private static final String NORMAL_TITLE = "Plain shortcut";
    private static final String NORMAL_HTML = UrlUtils.encodeHtmlDataUri(
            "<html>"
            + "<head><title>" + NORMAL_TITLE + "</title></head>"
            + "<body>Not Webapp capable</body></html>");

    private static final String META_APP_NAME_PAGE_TITLE = "Not the right title";
    private static final String META_APP_NAME_TITLE = "Web application-name";
    private static final String META_APP_NAME_HTML = UrlUtils.encodeHtmlDataUri(
            "<html><head>"
            + "<meta name=\"mobile-web-app-capable\" content=\"yes\" />"
            + "<meta name=\"application-name\" content=\"" + META_APP_NAME_TITLE + "\">"
            + "<title>" + META_APP_NAME_PAGE_TITLE + "</title>"
            + "</head><body>Webapp capable</body></html>");

    private static class TestObserver implements ChromeShellApplicationObserver {
        Intent mFiredIntent;

        @Override
        public boolean onSendBroadcast(Intent intent) {
            if (intent.hasExtra(Intent.EXTRA_SHORTCUT_NAME)) {
                // Stop a shortcut from really being added.
                mFiredIntent = intent;
                return false;
            }

            return true;
        }

        public void reset() {
            mFiredIntent = null;
        }
    }

    private ChromeShellActivity mActivity;
    private TestObserver mTestObserver;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        ShortcutHelper.setFullScreenAction(WEBAPP_ACTION_NAME);
        mActivity = launchChromeShellWithBlankPage();
        waitForActiveShellToBeDoneLoading();

        // Set up the observer.
        mTestObserver = new TestObserver();
        ChromeShellApplication application =
                (ChromeShellApplication) mActivity.getApplication();
        application.addObserver(mTestObserver);
    }

    @SmallTest
    @Feature("{Webapp}")
    public void testAddWebappShortcuts() throws InterruptedException {
        // Add a webapp shortcut and make sure the intent's parameters make sense.
        addShortcutToURL(WEBAPP_HTML, WEBAPP_TITLE, "");
        Intent firedIntent = mTestObserver.mFiredIntent;
        assertEquals(WEBAPP_TITLE, firedIntent.getStringExtra(Intent.EXTRA_SHORTCUT_NAME));

        Intent launchIntent = firedIntent.getParcelableExtra(Intent.EXTRA_SHORTCUT_INTENT);
        assertEquals(WEBAPP_HTML, launchIntent.getStringExtra(ShortcutHelper.EXTRA_URL));
        assertEquals(WEBAPP_ACTION_NAME, launchIntent.getAction());
        assertEquals(mActivity.getPackageName(), launchIntent.getPackage());

        // Add a second shortcut and make sure it matches the second webapp's parameters.
        mTestObserver.reset();
        addShortcutToURL(SECOND_WEBAPP_HTML, SECOND_WEBAPP_TITLE, "");
        Intent newFiredIntent = mTestObserver.mFiredIntent;
        assertEquals(SECOND_WEBAPP_TITLE,
                newFiredIntent.getStringExtra(Intent.EXTRA_SHORTCUT_NAME));

        Intent newLaunchIntent = newFiredIntent.getParcelableExtra(Intent.EXTRA_SHORTCUT_INTENT);
        assertEquals(SECOND_WEBAPP_HTML, newLaunchIntent.getStringExtra(ShortcutHelper.EXTRA_URL));
        assertEquals(WEBAPP_ACTION_NAME, newLaunchIntent.getAction());
        assertEquals(mActivity.getPackageName(), newLaunchIntent.getPackage());
    }

    @SmallTest
    @Feature("{Webapp}")
    public void testAddBookmarkShortcut() throws InterruptedException {
        addShortcutToURL(NORMAL_HTML, NORMAL_TITLE, "");

        // Make sure the intent's parameters make sense.
        Intent firedIntent = mTestObserver.mFiredIntent;
        assertEquals(NORMAL_TITLE, firedIntent.getStringExtra(Intent.EXTRA_SHORTCUT_NAME));

        Intent launchIntent = firedIntent.getParcelableExtra(Intent.EXTRA_SHORTCUT_INTENT);
        assertEquals(mActivity.getPackageName(), launchIntent.getPackage());
        assertEquals(Intent.ACTION_VIEW, launchIntent.getAction());
        assertEquals(NORMAL_HTML, launchIntent.getDataString());
    }

    @SmallTest
    @Feature("{Webapp}")
    public void testAddWebappShortcutsWithoutTitleEdit() throws InterruptedException {
        // Add a webapp shortcut using the page's title.
        addShortcutToURL(WEBAPP_HTML, WEBAPP_TITLE, "");
        Intent firedIntent = mTestObserver.mFiredIntent;
        assertEquals(WEBAPP_TITLE, firedIntent.getStringExtra(Intent.EXTRA_SHORTCUT_NAME));
    }

    @SmallTest
    @Feature("{Webapp}")
    public void testAddWebappShortcutsWithTitleEdit() throws InterruptedException {
        // Add a webapp shortcut with a custom title.
        addShortcutToURL(WEBAPP_HTML, WEBAPP_TITLE, EDITED_WEBAPP_TITLE);
        Intent firedIntent = mTestObserver.mFiredIntent;
        assertEquals(EDITED_WEBAPP_TITLE, firedIntent.getStringExtra(Intent.EXTRA_SHORTCUT_NAME));
    }

    @SmallTest
    @Feature("{Webapp}")
    public void testAddWebappShortcutsWithApplicationName() throws InterruptedException {
        addShortcutToURL(META_APP_NAME_HTML, META_APP_NAME_PAGE_TITLE, "");
        Intent firedIntent = mTestObserver.mFiredIntent;
        assertEquals(META_APP_NAME_TITLE, firedIntent.getStringExtra(Intent.EXTRA_SHORTCUT_NAME));
    }

    private void addShortcutToURL(String url, final String expectedPageTitle, final String title)
            throws InterruptedException {
        final Tab activeTab = getActivity().getActiveTab();
        TabLoadObserver observer = new TabLoadObserver(activeTab, url) {
            @Override
            public boolean isSatisfied() {
                // The page title is often updated over several iterations.  Wait until the right
                // one appears.
                return super.isSatisfied()
                        && TextUtils.equals(activeTab.getTitle(), expectedPageTitle);
            }
        };
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(observer));

        // Add the shortcut.
        Callable<ShortcutHelper> callable = new Callable<ShortcutHelper>() {
            @Override
            public ShortcutHelper call() {
                final ShortcutHelper helper = new ShortcutHelper(
                        mActivity.getApplicationContext(), mActivity.getActiveTab());
                // Calling initialize() isn't strictly required but it is testing this code path.
                helper.initialize(new ShortcutHelper.ShortcutHelperObserver() {
                    @Override
                    public void onTitleAvailable(String t) {
                    }

                    @Override
                    public void onIconAvailable(Bitmap icon) {
                        helper.addShortcut(title);
                    }
                });
                return helper;
            }
        };
        final ShortcutHelper helper = ThreadUtils.runOnUiThreadBlockingNoException(callable);

        // Make sure that the shortcut was added.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mTestObserver.mFiredIntent != null;
            }
        }));

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                helper.destroy();
            }
        });
    }
}
