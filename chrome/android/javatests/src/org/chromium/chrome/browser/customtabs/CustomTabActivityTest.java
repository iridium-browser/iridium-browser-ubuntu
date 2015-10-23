// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Application;
import android.app.Instrumentation;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.support.customtabs.CustomTabsIntent;
import android.support.customtabs.ICustomTabsCallback;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.ImageButton;

import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.document.BrandColorUtils;
import org.chromium.chrome.browser.document.DocumentActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.content.browser.BrowserStartupController;
import org.chromium.content.browser.BrowserStartupController.StartupCallback;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.ArrayList;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Instrumentation tests for app menu, context menu, and toolbar of a {@link CustomTabActivity}.
 */
public class CustomTabActivityTest extends CustomTabActivityTestBase {

    /**
     * An empty {@link BroadcastReceiver} that exists only to make the PendingIntent to carry an
     * explicit intent. Otherwise the framework will not send it after {@link PendingIntent#send()}.
     */
    public static class DummyBroadcastReceiver extends BroadcastReceiver {
        // The url has to be copied from the instrumentation class, because a BroadcastReceiver is
        // deployed as a different package, and it cannot get access to data from the
        // instrumentation package.
        private static final String TEST_PAGE_COPY = TestHttpServerClient.getUrl(
                "chrome/test/data/android/google.html");

        @Override
        public void onReceive(Context context, Intent intent) {
            // Note: even if this assertion fails, the test might still pass, because
            // BroadcastReceiver is not treated as part of the instrumentation test.
            assertEquals(TEST_PAGE_COPY, intent.getDataString());
        }
    }

    private static final int MAX_MENU_CUSTOM_ITEMS = 5;
    private static final int NUM_CHROME_MENU_ITEMS = 3;
    private static final String
            TEST_ACTION = "org.chromium.chrome.browser.customtabs.TEST_PENDING_INTENT_SENT";
    private static final String TEST_PAGE = TestHttpServerClient.getUrl(
            "chrome/test/data/android/google.html");
    private static final String TEST_PAGE_2 = TestHttpServerClient.getUrl(
            "chrome/test/data/android/test.html");
    private static final String TEST_MENU_TITLE = "testMenuTitle";

    private CustomTabActivity mActivity;

    @Override
    protected void startActivityCompletely(Intent intent) {
        super.startActivityCompletely(intent);
        mActivity = getActivity();
    }

    /**
     * @see CustomTabsTestUtils#createMinimalCustomTabIntent(Context, String, IBinder).
     */
    private Intent createMinimalCustomTabIntent() {
        return CustomTabsTestUtils.createMinimalCustomTabIntent(
                getInstrumentation().getTargetContext(), TEST_PAGE, null);
    }

    /**
     * Add a bundle specifying a custom menu entry.
     * @param intent The intent to modify.
     * @return The pending intent associated with the menu entry.
     */
    private PendingIntent addMenuEntriesToIntent(Intent intent, int numEntries) {
        Intent menuIntent = new Intent();
        menuIntent.setClass(getInstrumentation().getContext(), DummyBroadcastReceiver.class);
        menuIntent.setAction(TEST_ACTION);
        PendingIntent pi = PendingIntent.getBroadcast(getInstrumentation().getTargetContext(), 0,
                menuIntent, 0);
        ArrayList<Bundle> menuItems = new ArrayList<Bundle>();
        for (int i = 0; i < numEntries; i++) {
            Bundle bundle = new Bundle();
            bundle.putString(CustomTabsIntent.KEY_MENU_ITEM_TITLE, TEST_MENU_TITLE);
            bundle.putParcelable(CustomTabsIntent.KEY_PENDING_INTENT, pi);
            menuItems.add(bundle);
        }
        intent.putParcelableArrayListExtra(CustomTabsIntent.EXTRA_MENU_ITEMS, menuItems);
        return pi;
    }

    private void addToolbarColorToIntent(Intent intent, int color) {
        intent.putExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR, color);
    }

    /**
     * Adds an action button to the custom tab toolbar.
     * @return The {@link PendingIntent} that will be triggered when the action button is clicked.
     */
    private PendingIntent addActionButtonToIntent(Intent intent, Bitmap icon, String description) {
        Intent actionIntent = new Intent();
        actionIntent.setClass(getInstrumentation().getContext(), DummyBroadcastReceiver.class);
        actionIntent.setAction(TEST_ACTION);

        Bundle bundle = new Bundle();
        bundle.putParcelable(CustomTabsIntent.KEY_ICON, icon);
        bundle.putString(CustomTabsIntent.KEY_DESCRIPTION, description);
        PendingIntent pi = PendingIntent.getBroadcast(getInstrumentation().getTargetContext(), 0,
                actionIntent, 0);
        bundle.putParcelable(CustomTabsIntent.KEY_PENDING_INTENT, pi);

        intent.putExtra(CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE, bundle);
        return pi;
    }

    private void openAppMenuAndAssertMenuShown() throws InterruptedException {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mActivity.onMenuOrKeyboardAction(R.id.show_menu, false);
            }
        });

        assertTrue("App menu was not shown", CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mActivity.getAppMenuHandler().isAppMenuShowing();
            }
        }));
    }

    /**
     * Test the entries in the context menu shown when long clicking an image.
     */
    @SmallTest
    public void testContextMenuEntriesForImage() throws InterruptedException, TimeoutException {
        startCustomTabActivityWithIntent(createMinimalCustomTabIntent());

        final int expectedMenuSize = 4;
        Menu menu = ContextMenuUtils.openContextMenu(this, getActivity().getActivityTab(), "logo");
        assertEquals(expectedMenuSize, menu.size());
        assertNotNull(menu.findItem(R.id.contextmenu_save_image));
        assertNotNull(menu.findItem(R.id.contextmenu_open_image));
        assertNotNull(menu.findItem(R.id.contextmenu_copy_image));
        assertNotNull(menu.findItem(R.id.contextmenu_copy_image_url));
    }

    /**
     * Test the entries in the context menu shown when long clicking an link.
     */
    @SmallTest
    public void testContextMenuEntriesForLink() throws InterruptedException, TimeoutException {
        startCustomTabActivityWithIntent(createMinimalCustomTabIntent());

        final int expectedMenuSize = 3;
        Menu menu = ContextMenuUtils.openContextMenu(this, getActivity().getActivityTab(),
                "aboutLink");
        assertEquals(expectedMenuSize, menu.size());
        assertNotNull(menu.findItem(R.id.contextmenu_copy_link_address_text));
        assertNotNull(menu.findItem(R.id.contextmenu_copy_link_text));
        assertNotNull(menu.findItem(R.id.contextmenu_save_link_as));
    }

    /**
     * Test the entries in the app menu.
     */
    @SmallTest
    public void testCustomTabAppMenu() throws InterruptedException {
        Intent intent = createMinimalCustomTabIntent();
        int numMenuEntries = 1;
        addMenuEntriesToIntent(intent, numMenuEntries);
        startCustomTabActivityWithIntent(intent);

        openAppMenuAndAssertMenuShown();
        final int expectedMenuSize = numMenuEntries + NUM_CHROME_MENU_ITEMS;
        Menu menu = getActivity().getAppMenuHandler().getAppMenuForTest().getMenuForTest();
        assertNotNull("App menu is not initialized: ", menu);
        assertEquals(expectedMenuSize, menu.size());
        assertNotNull(menu.findItem(R.id.forward_menu_id));
        assertNotNull(menu.findItem(R.id.info_menu_id));
        assertNotNull(menu.findItem(R.id.reload_menu_id));
        assertNotNull(menu.findItem(R.id.find_in_page_id));
        assertNotNull(menu.findItem(R.id.open_in_chrome_id));
    }

    /**
     * Test that only up to 5 entries are added to the custom menu.
     */
    @SmallTest
    public void testCustomTabMaxMenuItems() throws InterruptedException {
        Intent intent = createMinimalCustomTabIntent();
        int numMenuEntries = 7;
        addMenuEntriesToIntent(intent, numMenuEntries);
        startCustomTabActivityWithIntent(intent);

        openAppMenuAndAssertMenuShown();
        int expectedMenuSize = MAX_MENU_CUSTOM_ITEMS + NUM_CHROME_MENU_ITEMS;
        Menu menu = getActivity().getAppMenuHandler().getAppMenuForTest().getMenuForTest();
        assertNotNull("App menu is not initialized: ", menu);
        assertEquals(expectedMenuSize, menu.size());
    }

    /**
     * Test whether the custom menu is correctly shown and clicking it sends the right
     * {@link PendingIntent}.
     */
    @SmallTest
    public void testCustomMenuEntry() throws InterruptedException {
        Intent intent = createMinimalCustomTabIntent();
        final PendingIntent pi = addMenuEntriesToIntent(intent, 1);
        startCustomTabActivityWithIntent(intent);

        final OnFinishedForTest onFinished = new OnFinishedForTest(pi);
        mActivity.getIntentDataProvider().setPendingIntentOnFinishedForTesting(onFinished);

        openAppMenuAndAssertMenuShown();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                MenuItem item = mActivity.getAppMenuPropertiesDelegate().getMenuItemForTitle(
                        TEST_MENU_TITLE);
                assertNotNull(item);
                assertTrue(mActivity.onOptionsItemSelected(item));
            }
        });

        assertTrue("Pending Intent was not sent.", CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return onFinished.isSent();
            }
        }));
    }

    /**
     * Test whether clicking "Open in Chrome" takes us to a chrome normal tab, loading the same url.
     */
    @SmallTest
    public void testOpenInChrome() throws InterruptedException {
        startCustomTabActivityWithIntent(createMinimalCustomTabIntent());

        boolean isDocumentMode = FeatureUtilities.isDocumentMode(
                getInstrumentation().getTargetContext());
        String activityName;
        if (isDocumentMode) {
            activityName = DocumentActivity.class.getName();
        } else {
            activityName = ChromeTabbedActivity.class.getName();
        }
        Instrumentation.ActivityMonitor monitor = getInstrumentation().addMonitor(activityName,
                null, false);

        openAppMenuAndAssertMenuShown();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mActivity.onMenuOrKeyboardAction(R.id.open_in_chrome_id, false);
            }
        });

        final ChromeActivity chromeActivity = (ChromeActivity) monitor
                .waitForActivityWithTimeout(ACTIVITY_START_TIMEOUT_MS);
        assertNotNull("A normal chrome activity did not start.", chromeActivity);
        assertTrue("The normal tab was not initiated correctly.",
                CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        Tab tab = chromeActivity.getActivityTab();
                        return tab != null && tab.getUrl().equals(TEST_PAGE);
                    }
                }));
    }

    /**
     * Test whether the color of the toolbar is correctly customized. For L or later releases,
     * status bar color is also tested.
     */
    @SmallTest
    public void testToolbarColor() throws InterruptedException {
        Intent intent = createMinimalCustomTabIntent();
        final int expectedColor = Color.RED;
        addToolbarColorToIntent(intent, expectedColor);
        startCustomTabActivityWithIntent(intent);

        View toolbarView = getActivity().findViewById(R.id.toolbar);
        assertTrue("A custom tab toolbar is never shown", toolbarView instanceof CustomTabToolbar);
        CustomTabToolbar toolbar = (CustomTabToolbar) toolbarView;
        assertEquals(expectedColor, toolbar.getBackground().getColor());
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.LOLLIPOP) {
            assertEquals(BrandColorUtils.computeStatusBarColor(expectedColor),
                    getActivity().getWindow().getStatusBarColor());
        }
    }

    /**
     * Test if an action button is shown with correct image and size, and clicking it sends the
     * correct {@link PendingIntent}.
     */
    @SmallTest
    public void testActionButton() throws InterruptedException {
        final int iconHeightDp = 48;
        final int iconWidthDp = 96;
        Resources testRes = getInstrumentation().getTargetContext().getResources();
        float density = testRes.getDisplayMetrics().density;
        Bitmap expectedIcon = Bitmap.createBitmap((int) (iconWidthDp * density),
                (int) (iconHeightDp * density), Bitmap.Config.ARGB_8888);

        Intent intent = createMinimalCustomTabIntent();
        final PendingIntent pi = addActionButtonToIntent(intent, expectedIcon, "Good test");
        startCustomTabActivityWithIntent(intent);

        final OnFinishedForTest onFinished = new OnFinishedForTest(pi);
        mActivity.getIntentDataProvider().setPendingIntentOnFinishedForTesting(onFinished);

        View toolbarView = getActivity().findViewById(R.id.toolbar);
        assertTrue("A custom tab toolbar is never shown", toolbarView instanceof CustomTabToolbar);
        CustomTabToolbar toolbar = (CustomTabToolbar) toolbarView;
        final ImageButton actionButton = toolbar.getCustomActionButtonForTest();

        assertNotNull(actionButton);
        assertNotNull(actionButton.getDrawable());
        assertTrue("Action button's background is not a BitmapDrawable.",
                actionButton.getDrawable() instanceof BitmapDrawable);

        assertTrue("Action button does not have the correct bitmap.",
                expectedIcon.sameAs(((BitmapDrawable) actionButton.getDrawable()).getBitmap()));

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                actionButton.performClick();
            }
        });

        assertTrue("Pending Intent was not sent.", CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return onFinished.isSent();
            }
        }));
    }

    /**
     * Test the case that the action button should not be shown, given a bitmap with unacceptable
     * height/width ratio.
     */
    @SmallTest
    public void testActionButtonBadRatio() throws InterruptedException {
        final int iconHeightDp = 20;
        final int iconWidthDp = 60;
        Resources testRes = getInstrumentation().getTargetContext().getResources();
        float density = testRes.getDisplayMetrics().density;
        Bitmap expectedIcon = Bitmap.createBitmap((int) (iconWidthDp * density),
                (int) (iconHeightDp * density), Bitmap.Config.ARGB_8888);

        Intent intent = createMinimalCustomTabIntent();
        addActionButtonToIntent(intent, expectedIcon, "Good test");
        startCustomTabActivityWithIntent(intent);

        View toolbarView = getActivity().findViewById(R.id.toolbar);
        assertTrue("A custom tab toolbar is never shown", toolbarView instanceof CustomTabToolbar);
        CustomTabToolbar toolbar = (CustomTabToolbar) toolbarView;
        final ImageButton actionButton = toolbar.getCustomActionButtonForTest();

        assertNotNull(actionButton);
        assertTrue("Action button should not be shown",
                View.VISIBLE != actionButton.getVisibility());

        CustomTabIntentDataProvider dataProvider = mActivity.getIntentDataProvider();
        assertNull(dataProvider.getActionButtonParams());
    }

    @SmallTest
    public void testLaunchWithSession() throws InterruptedException {
        IBinder session = warmUpAndLaunchUrlWithSession();
        assertEquals(mActivity.getIntentDataProvider().getSession(), session);
    }

    @SmallTest
    public void testLoadNewUrlWithSession() throws InterruptedException {
        final IBinder session = warmUpAndLaunchUrlWithSession();
        final Context context = getInstrumentation().getTargetContext();
        assertEquals(mActivity.getIntentDataProvider().getSession(), session);
        assertFalse("CustomTabContentHandler handled intent with wrong session",
                ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws Exception {
                        return CustomTabActivity.handleInActiveContentIfNeeded(
                                CustomTabsTestUtils.createMinimalCustomTabIntent(context,
                                        TEST_PAGE_2,
                                        CustomTabsTestUtils.newDummyCallback().asBinder()));
                    }
                }));
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return mActivity.getActivityTab().getUrl().equals(TEST_PAGE);
                    }
        }));
        assertTrue("CustomTabContentHandler can't handle intent with same session",
                ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws Exception {
                        return CustomTabActivity.handleInActiveContentIfNeeded(
                            CustomTabsTestUtils.createMinimalCustomTabIntent(context,
                                    TEST_PAGE_2, session));
                    }
                }));
        final Tab tab = mActivity.getActivityTab();
        final CallbackHelper pageLoadFinishedHelper = new CallbackHelper();
        tab.addObserver(new EmptyTabObserver() {
            @Override
            public void onPageLoadFinished(Tab tab) {
                pageLoadFinishedHelper.notifyCalled();
            }
        });
        try {
            pageLoadFinishedHelper.waitForCallback(0);
        } catch (TimeoutException e) {
            fail();
        }
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return mActivity.getActivityTab().getUrl().equals(TEST_PAGE_2);
                    }
        }));
    }

    @SmallTest
    public void testReferrerAddedAutomatically() throws InterruptedException {
        final IBinder session = warmUpAndLaunchUrlWithSession();
        assertEquals(mActivity.getIntentDataProvider().getSession(), session);
        final Context context = getInstrumentation().getTargetContext().getApplicationContext();
        CustomTabsConnection connection = CustomTabsConnection.getInstance((Application) context);
        String packageName = context.getPackageName();
        final String referrer =
                IntentHandler.constructValidReferrerForAuthority(packageName).getUrl();
        assertEquals(referrer, connection.getReferrerForSession(session).getUrl());

        final Tab tab = mActivity.getActivityTab();
        final CallbackHelper pageLoadFinishedHelper = new CallbackHelper();
        tab.addObserver(new EmptyTabObserver() {
            @Override
            public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                assertEquals(referrer, params.getReferrer().getUrl());
            }

            @Override
            public void onPageLoadFinished(Tab tab) {
                pageLoadFinishedHelper.notifyCalled();
            }
        });
        assertTrue("CustomTabContentHandler can't handle intent with same session",
                ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws Exception {
                        return CustomTabActivity.handleInActiveContentIfNeeded(
                            CustomTabsTestUtils.createMinimalCustomTabIntent(context,
                                    TEST_PAGE_2, session));
                    }
                }));
        try {
            pageLoadFinishedHelper.waitForCallback(0);
        } catch (TimeoutException e) {
            fail();
        }
    }

    private IBinder warmUpAndLaunchUrlWithSession() throws InterruptedException {
        final Context context = getInstrumentation().getTargetContext().getApplicationContext();
        CustomTabsConnection connection = CustomTabsConnection.getInstance((Application) context);
        ICustomTabsCallback callback = CustomTabsTestUtils.newDummyCallback();
        final CallbackHelper startupCallbackHelper = new CallbackHelper();
        connection.warmup(0);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                BrowserStartupController.get(context, LibraryProcessType.PROCESS_BROWSER)
                        .addStartupCompletedObserver(new StartupCallback() {
                            @Override
                            public void onSuccess(boolean alreadyStarted) {
                                startupCallbackHelper.notifyCalled();
                            }

                            @Override
                            public void onFailure() {
                                fail();
                            }
                        });
            }
        });

        try {
            startupCallbackHelper.waitForCallback(0);
        } catch (TimeoutException e) {
            fail();
        }
        connection.newSession(callback);
        startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                context, TEST_PAGE, callback.asBinder()));
        return callback.asBinder();
    }

    /**
     * A helper class to monitor sending status of a {@link PendingIntent}.
     */
    private static class OnFinishedForTest implements PendingIntent.OnFinished {

        private PendingIntent mPi;
        private AtomicBoolean mIsSent = new AtomicBoolean();
        private String mUri;

        /**
         * Create an instance of {@link OnFinishedForTest}, testing the given {@link PendingIntent}.
         */
        public OnFinishedForTest(PendingIntent pi) {
            mPi = pi;
        }

        /**
         * @return Whether {@link PendingIntent#send()} has been successfully triggered and the sent
         *         intent carries the correct Uri as data.
         */
        public boolean isSent() {
            return mIsSent.get() && TEST_PAGE.equals(mUri);
        }

        @Override
        public void onSendFinished(PendingIntent pendingIntent, Intent intent, int resultCode,
                String resultData, Bundle resultExtras) {
            if (pendingIntent.equals(mPi)) {
                mUri = intent.getDataString();
                mIsSent.set(true);
            }
        }
    }
}
