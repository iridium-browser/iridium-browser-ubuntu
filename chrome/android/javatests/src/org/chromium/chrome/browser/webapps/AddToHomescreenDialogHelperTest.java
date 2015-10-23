// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.TextUtils;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.Callable;

/**
 * Tests org.chromium.chrome.browser.webapps.AddToHomescreenDialogHelper and it's C++ counterpart.
 */
public class AddToHomescreenDialogHelperTest extends ChromeActivityTestCaseBase<ChromeActivity> {
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

    private static class TestShortcutHelperDelegate extends ShortcutHelper.Delegate {
        public Intent mBroadcastedIntent;

        @Override
        public void sendBroadcast(Context context, Intent intent) {
            mBroadcastedIntent = intent;
        }

        @Override
        public String getFullscreenAction() {
            return WEBAPP_ACTION_NAME;
        }

        public void clearBroadcastedIntent() {
            mBroadcastedIntent = null;
        }
    }

    private ChromeActivity mActivity;
    private TestShortcutHelperDelegate mShortcutHelperDelegate;

    public AddToHomescreenDialogHelperTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mShortcutHelperDelegate = new TestShortcutHelperDelegate();
        ShortcutHelper.setDelegateForTests(mShortcutHelperDelegate);
        mActivity = getActivity();
    }

    @SmallTest
    @Feature("{Webapp}")
    public void testAddWebappShortcuts() throws InterruptedException {
        // Add a webapp shortcut and make sure the intent's parameters make sense.
        addShortcutToURL(WEBAPP_HTML, WEBAPP_TITLE, "");
        Intent firedIntent = mShortcutHelperDelegate.mBroadcastedIntent;
        assertEquals(WEBAPP_TITLE, firedIntent.getStringExtra(Intent.EXTRA_SHORTCUT_NAME));

        Intent launchIntent = firedIntent.getParcelableExtra(Intent.EXTRA_SHORTCUT_INTENT);
        assertEquals(WEBAPP_HTML, launchIntent.getStringExtra(ShortcutHelper.EXTRA_URL));
        assertEquals(WEBAPP_ACTION_NAME, launchIntent.getAction());
        assertEquals(mActivity.getPackageName(), launchIntent.getPackage());

        // Add a second shortcut and make sure it matches the second webapp's parameters.
        mShortcutHelperDelegate.clearBroadcastedIntent();
        addShortcutToURL(SECOND_WEBAPP_HTML, SECOND_WEBAPP_TITLE, "");
        Intent newFiredIntent = mShortcutHelperDelegate.mBroadcastedIntent;
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
        Intent firedIntent = mShortcutHelperDelegate.mBroadcastedIntent;
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
        Intent firedIntent = mShortcutHelperDelegate.mBroadcastedIntent;
        assertEquals(WEBAPP_TITLE, firedIntent.getStringExtra(Intent.EXTRA_SHORTCUT_NAME));
    }

    @SmallTest
    @Feature("{Webapp}")
    public void testAddWebappShortcutsWithTitleEdit() throws InterruptedException {
        // Add a webapp shortcut with a custom title.
        addShortcutToURL(WEBAPP_HTML, WEBAPP_TITLE, EDITED_WEBAPP_TITLE);
        Intent firedIntent = mShortcutHelperDelegate.mBroadcastedIntent;
        assertEquals(EDITED_WEBAPP_TITLE, firedIntent.getStringExtra(Intent.EXTRA_SHORTCUT_NAME));
    }

    @SmallTest
    @Feature("{Webapp}")
    public void testAddWebappShortcutsWithApplicationName() throws InterruptedException {
        addShortcutToURL(META_APP_NAME_HTML, META_APP_NAME_PAGE_TITLE, "");
        Intent firedIntent = mShortcutHelperDelegate.mBroadcastedIntent;
        assertEquals(META_APP_NAME_TITLE, firedIntent.getStringExtra(Intent.EXTRA_SHORTCUT_NAME));
    }

    private void addShortcutToURL(String url, final String expectedPageTitle, final String title)
            throws InterruptedException {
        final Tab activeTab = mActivity.getActivityTab();
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
        Callable<AddToHomescreenDialogHelper> callable =
                new Callable<AddToHomescreenDialogHelper>() {
            @Override
            public AddToHomescreenDialogHelper call() {
                final AddToHomescreenDialogHelper helper = new AddToHomescreenDialogHelper(
                        mActivity.getApplicationContext(), mActivity.getActivityTab());
                // Calling initialize() isn't strictly required but it is testing this code path.
                helper.initialize(new AddToHomescreenDialogHelper.Observer() {
                    @Override
                    public void onUserTitleAvailable(String t) {
                    }

                    @Override
                    public void onIconAvailable(Bitmap icon) {
                        helper.addShortcut(title);
                    }
                });
                return helper;
            }
        };
        final AddToHomescreenDialogHelper helper =
                ThreadUtils.runOnUiThreadBlockingNoException(callable);

        // Make sure that the shortcut was added.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mShortcutHelperDelegate.mBroadcastedIntent != null;
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
