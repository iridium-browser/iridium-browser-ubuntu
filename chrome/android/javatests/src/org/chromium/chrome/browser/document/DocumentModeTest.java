// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.PendingIntent;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.test.suitebuilder.annotation.MediumTest;
import android.view.View;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModelSelector;
import org.chromium.chrome.browser.tabmodel.document.OffTheRecordDocumentTabModel;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.DisableInTabbedMode;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.LoadUrlParams;

import java.lang.ref.WeakReference;
import java.util.List;

/**
 * General tests for how Document mode Activities interact with each other.
 */
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
@DisableInTabbedMode
public class DocumentModeTest extends DocumentModeTestBase {
    /**
     * Confirm that you can't start ChromeTabbedActivity while the user is running in Document mode.
     */
    @MediumTest
    public void testDontStartTabbedActivityInDocumentMode() throws Exception {
        launchThreeTabs();

        // Try launching a ChromeTabbedActivity.
        startTabbedActivity(URL_1);

        // ApplicationStatus should note that the ChromeTabbedActivity isn't running anymore.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                List<WeakReference<Activity>> activities = ApplicationStatus.getRunningActivities();
                for (WeakReference<Activity> activity : activities) {
                    if (activity.get() instanceof ChromeTabbedActivity) return false;
                }
                return true;
            }
        }));
    }

    /**
     * Confirm that firing an Intent without a properly formatted document://ID?url scheme causes
     * the DocumentActivity to finish itself and hopefully not flat crash (though that'd be better
     * than letting the user live in both tabbed and document mode simultaneously crbug.com/445136).
     */
    @MediumTest
    public void testFireInvalidIntent() throws Exception {
        launchThreeTabs();

        final DocumentTabModelSelector selector =
                ChromeApplication.getDocumentTabModelSelector();
        final Activity lastTrackedActivity = ApplicationStatus.getLastTrackedFocusedActivity();

        // Send the user home, then fire an Intent with invalid data.
        ApplicationTestUtils.fireHomeScreenIntent(mContext);
        Intent intent = new Intent(lastTrackedActivity.getIntent());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        intent.setData(Uri.parse("toteslegitscheme://"));
        mContext.startActivity(intent);

        // A DocumentActivity gets started, but it should immediately call finishAndRemoveTask()
        // because of the broken Intent.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
                return activity != lastTrackedActivity;
            }
        }));

        // We shouldn't record that a new Tab exists.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return selector.getCurrentModel().getCount() == 3
                        && selector.getTotalTabCount() == 3;
            }
        }));
    }

    /**
     * Confirm that firing an Intent for a document that has an ID for an already existing Tab kills
     * the original.
     */
    @MediumTest
    public void testDuplicateTabIDsKillsOldActivities() throws Exception {
        launchThreeTabs();

        final DocumentTabModelSelector selector =
                ChromeApplication.getDocumentTabModelSelector();
        final int lastTabId = selector.getCurrentTabId();
        final Activity lastTrackedActivity = ApplicationStatus.getLastTrackedFocusedActivity();

        // Send the user home, then fire an Intent with an old Tab ID and a new URL.
        ApplicationTestUtils.fireHomeScreenIntent(mContext);
        Intent intent = new Intent(lastTrackedActivity.getIntent());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        intent.setData(Uri.parse("document://" + lastTabId + "?" + URL_4));
        mContext.startActivity(intent);

        // Funnily enough, Android doesn't differentiate between URIs with different queries when
        // refocusing Activities based on the Intent data.  This means we can't do a check to see
        // that the new Activity appears with URL_4 -- we just get a new instance of URL_3.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
                if (!(activity instanceof ChromeActivity)) return false;

                ChromeActivity chromeActivity = (ChromeActivity) activity;
                Tab tab = chromeActivity.getActivityTab();
                return tab != null && lastTrackedActivity != activity
                        && !selector.isIncognitoSelected()
                        && lastTabId == selector.getCurrentTabId();
            }
        }));

        // Although we get a new DocumentActivity, the old one with the same tab ID gets killed.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return selector.getCurrentModel().getCount() == 3
                    && selector.getTotalTabCount() == 3;
            }
        }));
    }

    /**
     * Confirm that firing a View Intent with a null URL acts like a Main Intent.
     */
    @MediumTest
    public void testRelaunchLatestTabWithInvalidViewIntent() throws Exception {
        launchThreeTabs();

        final DocumentTabModelSelector selector =
                ChromeApplication.getDocumentTabModelSelector();
        final int lastTabId = selector.getCurrentTabId();

        final Activity lastTrackedActivity = ApplicationStatus.getLastTrackedFocusedActivity();

        // Send Chrome to the background, then bring it back.
        ApplicationTestUtils.fireHomeScreenIntent(mContext);
        Intent intent = new Intent(Intent.ACTION_VIEW, null);
        intent.setClassName(mContext, ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(intent);
        ApplicationTestUtils.waitUntilChromeInForeground();

        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return lastTrackedActivity == ApplicationStatus.getLastTrackedFocusedActivity()
                        && !selector.isIncognitoSelected()
                        && lastTabId == selector.getCurrentTabId();
            }
        }));

        assertEquals(3, selector.getCurrentModel().getCount());
        assertEquals(3, selector.getTotalTabCount());
    }

    /**
     * Confirm that clicking the Chrome icon (e.g. firing an Intent with ACTION_MAIN) brings back
     * the last viewed Tab.
     */
    @MediumTest
    public void testRelaunchLatestTab() throws Exception {
        launchThreeTabs();

        final DocumentTabModelSelector selector =
                ChromeApplication.getDocumentTabModelSelector();
        final int lastTabId = selector.getCurrentTabId();

        // Send Chrome to the background, then bring it back.
        ApplicationTestUtils.fireHomeScreenIntent(mContext);
        ApplicationTestUtils.launchChrome(mContext);

        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !selector.isIncognitoSelected() && lastTabId == selector.getCurrentTabId();
            }
        }));

        assertEquals(3, selector.getCurrentModel().getCount());
        assertEquals(3, selector.getTotalTabCount());
    }

    /**
     * Confirm that setting the index brings the correct tab forward.
     */
    @MediumTest
    public void testSetIndex() throws Exception {
        int[] tabIds = launchThreeTabs();

        final DocumentTabModelSelector selector =
                ChromeApplication.getDocumentTabModelSelector();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                TabModelUtils.setIndex(selector.getCurrentModel(), 0);
            }
        });

        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !selector.isIncognitoSelected() && selector.getCurrentModel().index() == 0;
            }
        }));

        assertEquals(3, selector.getCurrentModel().getCount());
        assertEquals(3, selector.getTotalTabCount());
        assertEquals(tabIds[0], selector.getCurrentTab().getId());
    }

    /** Check that Intents that request reusing Tabs are honored. */
    @MediumTest
    public void testReuseIntent() throws Exception {
        // Create a tab, then send the user back to the Home screen.
        int tabId = launchViaViewIntent(false, URL_1, "Page 1");
        assertTrue(ChromeApplication.isDocumentTabModelSelectorInitializedForTests());
        final DocumentTabModelSelector selector =
                ChromeApplication.getDocumentTabModelSelector();
        assertEquals(1, selector.getModel(false).getCount());
        ApplicationTestUtils.fireHomeScreenIntent(mContext);

        // Fire an Intent to reuse the same tab as before.
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(URL_1));
                intent.setClass(mContext, ChromeLauncherActivity.class);
                intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                intent.putExtra(ShortcutHelper.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
                mContext.startActivity(intent);
            }
        };
        ActivityUtils.waitForActivity(getInstrumentation(), ChromeLauncherActivity.class, runnable);
        ApplicationTestUtils.waitUntilChromeInForeground();
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                return lastActivity instanceof DocumentActivity;
            }
        }));
        assertEquals(tabId, selector.getCurrentTabId());
        assertFalse(selector.isIncognitoSelected());

        // Create another tab.
        final int secondTabId = launchViaViewIntent(false, URL_2, "Page 2");
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return selector.getModel(false).getCount() == 2
                        && selector.getCurrentTabId() == secondTabId;
            }
        }));
    }

    /**
     * Tests both ways of launching Incognito tabs: via an Intent, and via
     * {@ref ChromeLauncherActivity#launchDocumentInstance()}.
     */
    @MediumTest
    public void testIncognitoLaunches() throws Exception {
        assertFalse(ChromeApplication.isDocumentTabModelSelectorInitializedForTests());

        // Make sure that an untrusted Intent can't launch an IncognitoDocumentActivity.
        Instrumentation.ActivityMonitor monitor = getInstrumentation().addMonitor(
                DocumentActivity.class.getName(), null, false);
        assertFalse(ChromeApplication.isDocumentTabModelSelectorInitializedForTests());
        assertEquals(0, ApplicationStatus.getRunningActivities().size());
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(URL_1));
                intent.setClass(mContext, ChromeLauncherActivity.class);
                intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
                mContext.startActivity(intent);
            }
        };
        ActivityUtils.waitForActivity(getInstrumentation(), ChromeLauncherActivity.class, runnable);
        DocumentActivity doc = (DocumentActivity) monitor.waitForActivityWithTimeout(1000);
        assertNull(doc);

        // Create an Incognito tab via an Intent extra.
        final int firstId = launchViaViewIntent(true, URL_2, "Page 2");
        assertTrue(ChromeApplication.isDocumentTabModelSelectorInitializedForTests());
        final DocumentTabModelSelector selector =
                ChromeApplication.getDocumentTabModelSelector();
        final TabModel incognitoModel = selector.getModel(true);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return firstId == selector.getCurrentTabId() && selector.getTotalTabCount() == 1;
            }
        }));
        assertEquals(incognitoModel, selector.getCurrentModel());

        // Make sure the URL isn't in the Intent of the first IncognitoDocumentActivity.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ApplicationStatus.getLastTrackedFocusedActivity()
                        instanceof IncognitoDocumentActivity;
            }
        }));
        assertNull("URL is in the Incognito Intent", IntentHandler.getUrlFromIntent(
                ApplicationStatus.getLastTrackedFocusedActivity().getIntent()));

        // Launch via ChromeLauncherActivity.launchInstance().
        final int secondId = launchViaLaunchDocumentInstance(true, URL_3, "Page 3");
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return secondId == selector.getCurrentTabId() && selector.getTotalTabCount() == 2;
            }
        }));
        assertTrue(selector.isIncognitoSelected());
        assertEquals(incognitoModel, selector.getCurrentModel());
        assertEquals(secondId, TabModelUtils.getCurrentTabId(incognitoModel));

        // Make sure the URL isn't in the Intent of the second IncognitoDocumentActivity.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ApplicationStatus.getLastTrackedFocusedActivity()
                        instanceof IncognitoDocumentActivity;
            }
        }));
        assertNull("URL is in the Incognito Intent", IntentHandler.getUrlFromIntent(
                ApplicationStatus.getLastTrackedFocusedActivity().getIntent()));
    }

    /**
     * Tests that opening an Incognito tab via a context menu while in Incognito mode opens the tab
     * in the background.
     */
    @MediumTest
    public void testIncognitoOpensInBackgroundFromIncognito() throws Exception {
        // Create an Incognito tab via an Intent extra.
        assertFalse(ChromeApplication.isDocumentTabModelSelectorInitializedForTests());
        final int firstId = launchViaLaunchDocumentInstance(true, HREF_LINK, "href link page");
        assertTrue(ChromeApplication.isDocumentTabModelSelectorInitializedForTests());
        final DocumentTabModelSelector selector =
                ChromeApplication.getDocumentTabModelSelector();
        final TabModel incognitoModel = selector.getModel(true);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return firstId == selector.getCurrentTabId() && selector.getTotalTabCount() == 1;
            }
        }));
        assertEquals(incognitoModel, selector.getCurrentModel());

        Activity firstActivity = ApplicationStatus.getLastTrackedFocusedActivity();
        assertTrue(firstActivity instanceof IncognitoDocumentActivity);

        // The context menu for links in Incognito mode lacks an "Open in new Incognito tab" option.
        // Instead, the regular "Open in new tab" option opens a new incognito tab.
        openLinkInNewTabViaContextMenu(false, true);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return firstId == selector.getCurrentTabId() && selector.getTotalTabCount() == 2;
            }
        }));
        assertEquals(incognitoModel, selector.getCurrentModel());
        assertEquals(firstActivity, ApplicationStatus.getLastTrackedFocusedActivity());
    }

    /**
     * Confirm that the incognito tabs and TabModel are destroyed when the "close all" notification
     * Intent is fired.
     */
    @MediumTest
    public void testIncognitoNotificationClosesTabs() throws Exception {
        final int regularId = launchViaLaunchDocumentInstance(false, URL_1, "Page 1");
        final DocumentTabModelSelector selector =
                ChromeApplication.getDocumentTabModelSelector();
        assertFalse(selector.isIncognitoSelected());

        final int incognitoId = launchViaLaunchDocumentInstance(true, URL_2, "Page 2");
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return selector.isIncognitoSelected() && selector.getCurrentTabId() == incognitoId;
            }
        }));
        assertEquals(0, selector.getCurrentModel().index());
        assertEquals(1, selector.getCurrentModel().getCount());

        PendingIntent closeAllIntent =
                ChromeLauncherActivity.getRemoveAllIncognitoTabsIntent(mContext);
        closeAllIntent.send();

        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return selector.getCurrentTabId() == regularId;
            }
        }));
        OffTheRecordDocumentTabModel tabModel =
                (OffTheRecordDocumentTabModel) selector.getModel(true);
        assertFalse(selector.isIncognitoSelected());
        assertFalse(tabModel.isDocumentTabModelImplCreated());
    }

    /**
     * Tests that Incognito tabs are opened in the foreground when spawned from a regular tab.
     */
    @MediumTest
    public void testIncognitoOpensInForegroundViaLinkContextMenu() throws Exception {
        launchViaLaunchDocumentInstance(false, HREF_LINK, "href link page");

        // Save the current tab info.
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        assertTrue(activity instanceof DocumentActivity);
        assertFalse(activity instanceof IncognitoDocumentActivity);
        final DocumentActivity regularActivity = (DocumentActivity) activity;

        final DocumentTabModelSelector selector =
                ChromeApplication.getDocumentTabModelSelector();
        final TabModel regularTabModel = selector.getModel(false);
        final TabModel incognitoTabModel = selector.getModel(true);
        final int regularTabId = selector.getCurrentTabId();

        // Open a link in incognito via the context menu.
        openLinkInNewTabViaContextMenu(true, true);
        final DocumentActivity secondActivity =
                (DocumentActivity) ApplicationStatus.getLastTrackedFocusedActivity();
        final int secondTabId = selector.getCurrentTabId();
        assertEquals("Wrong regular tab count", 1, regularTabModel.getCount());
        assertEquals("Wrong incognito tab count", 1, incognitoTabModel.getCount());
        assertNotSame("Wrong tab selected", regularTabId, selector.getCurrentTabId());
        assertTrue("Wrong model selected", selector.isIncognitoSelected());
        assertEquals("Wrong tab index selected", 0, selector.getCurrentModel().index());
        assertNotSame("Wrong Activity in foreground", regularActivity, secondActivity);
        assertTrue("Foreground Activity isn't Incognito",
                secondActivity instanceof IncognitoDocumentActivity);

        // Re-open the other tab.
        TabModelUtils.setIndex(regularTabModel, 0);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !selector.isIncognitoSelected()
                        && selector.getCurrentTabId() == regularTabId;
            }
        }));

        // Try to open a new Incognito Tab in the background using the TabModelSelector directly.
        // Should open it in the foreground, instead.
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                LoadUrlParams params = new LoadUrlParams(URL_1);
                secondActivity.getTabModelSelector().openNewTab(
                        params, TabModel.TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                        regularActivity.getActivityTab(), true);
            }
        };
        DocumentActivity thirdActivity = ActivityUtils.waitForActivity(getInstrumentation(),
                IncognitoDocumentActivity.class, runnable);
        waitForFullLoad(thirdActivity, "Page 1");
        assertEquals("Wrong regular tab count", 1, regularTabModel.getCount());
        assertEquals("Wrong incognito tab count", 2, incognitoTabModel.getCount());
        assertNotSame("Wrong tab selected", regularTabId, selector.getCurrentTabId());
        assertNotSame("Wrong tab selected", secondTabId, selector.getCurrentTabId());
        assertTrue("Wrong model selected", selector.isIncognitoSelected());
        assertEquals("Wrong tab index selected", 1, selector.getCurrentModel().index());
        assertNotSame("Wrong Activity in foreground", regularActivity, thirdActivity);
        assertNotSame("Wrong Activity in foreground", secondActivity, thirdActivity);
        assertTrue("Foreground Activity isn't Incognito",
                thirdActivity instanceof IncognitoDocumentActivity);
    }

    /**
     * Tests that tab ID is properly set when tabs change.
     */
    @MediumTest
    public void testLastTabIdUpdates() throws Exception {
        launchViaLaunchDocumentInstance(false, HREF_LINK, "href link page");

        final DocumentActivity firstActivity =
                (DocumentActivity) ApplicationStatus.getLastTrackedFocusedActivity();

        // Save the current tab ID.
        final DocumentTabModelSelector selector =
                ChromeApplication.getDocumentTabModelSelector();
        final TabModel tabModel = selector.getModel(false);
        final int firstTabId = selector.getCurrentTabId();
        final int firstTabIndex = tabModel.index();

        openLinkInBackgroundTab();

        // Do a plain click to make the link open in a new foreground Document.
        Runnable fgTrigger = new Runnable() {
            @Override
            public void run() {
                ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                    @Override
                    public void run() {
                        View view = firstActivity.findViewById(android.R.id.content).getRootView();
                        TouchCommon.singleClickView(view);
                    }
                });
            }
        };

        final DocumentActivity thirdActivity = ActivityUtils.waitForActivity(
                    getInstrumentation(), DocumentActivity.class, fgTrigger);
        waitForFullLoad(thirdActivity, "Page 4");
        assertEquals("Wrong number of tabs", 3, tabModel.getCount());
        assertNotSame("Wrong tab selected", firstTabIndex, tabModel.index());
        assertNotSame("Wrong tab ID in foreground", firstTabId, selector.getCurrentTabId());
        assertNotSame("Wrong Activity in foreground",
                firstActivity, ApplicationStatus.getLastTrackedFocusedActivity());
        assertEquals("URL is not in the Intent",
                URL_4, IntentHandler.getUrlFromIntent(thirdActivity.getIntent()));
    }

    /**
     * Tests that the page loads fine when a new page is opened via:
     * <a href="" target="_blank" rel="noreferrer">
     */
    @MediumTest
    public void testTargetBlank() throws Exception {
        Intent lastIntent = performNewWindowTest(
                HREF_NO_REFERRER_LINK, "href no referrer link page", false);
        assertEquals("URL is not in the Intent", URL_4, IntentHandler.getUrlFromIntent(lastIntent));
    }

    /**
     * Tests that tabs opened via window.open() load properly and with the URL in the Intent.
     * Tabs opened this way have their WebContents paused while the new Activity that will host
     * the WebContents starts asynchronously.
     */
    @MediumTest
    public void testWindowOpen() throws Exception {
        Intent lastIntent = performNewWindowTest(ONCLICK_LINK, "window.open page", true);
        assertEquals("URL is not in the Intent",
                URL_4, IntentHandler.getUrlFromIntent(lastIntent));
    }

    /**
     * Tests that the page loads fine when a new page is opened via window.open() and the opener is
     * set to null immediately afterward.
     */
    @MediumTest
    public void testWindowOpenWithOpenerSuppressed() throws Exception {
        Intent lastIntent = performNewWindowTest(
                ONCLICK_NO_REFERRER_LINK, "window.open page, opener set to null", true);
        assertEquals("Intent wasn't fired with about:blank",
                "about:blank", IntentHandler.getUrlFromIntent(lastIntent));
    }

    private Intent performNewWindowTest(String url, String title, boolean checkWindowOpenSuccess)
            throws Exception {
        launchViaLaunchDocumentInstance(false, url, title);

        final DocumentActivity firstActivity =
                (DocumentActivity) ApplicationStatus.getLastTrackedFocusedActivity();

        // Save the current tab ID.
        final DocumentTabModelSelector selector =
                ChromeApplication.getDocumentTabModelSelector();
        final TabModel tabModel = selector.getModel(false);
        final int firstTabId = selector.getCurrentTabId();
        final int firstTabIndex = tabModel.index();

        // Do a plain click to make the link open in a new foreground Document.
        Runnable fgTrigger = new Runnable() {
            @Override
            public void run() {
                ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                    @Override
                    public void run() {
                        View view = firstActivity.findViewById(android.R.id.content).getRootView();
                        TouchCommon.singleClickView(view);
                    }
                });
            }
        };

        final DocumentActivity lastActivity = ActivityUtils.waitForActivity(
                    getInstrumentation(), DocumentActivity.class, fgTrigger);
        waitForFullLoad(lastActivity, "Page 4");
        assertEquals("Wrong number of tabs", 2, tabModel.getCount());
        assertNotSame("Wrong tab selected", firstTabIndex, tabModel.index());
        assertNotSame("Wrong tab ID in foreground", firstTabId, selector.getCurrentTabId());
        assertNotSame("Wrong Activity in foreground",
                firstActivity, ApplicationStatus.getLastTrackedFocusedActivity());

        if (checkWindowOpenSuccess) {
            assertEquals("New WebContents was not created",
                    SUCCESS_URL, firstActivity.getActivityTab().getUrl());
        }

        return lastActivity.getIntent();
    }
}
