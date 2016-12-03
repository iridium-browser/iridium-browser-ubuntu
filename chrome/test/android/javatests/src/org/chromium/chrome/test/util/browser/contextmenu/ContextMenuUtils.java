// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.contextmenu;

import android.test.ActivityInstrumentationTestCase2;
import android.text.TextUtils;
import android.view.ContextMenu;
import android.view.MenuItem;

import junit.framework.Assert;

import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;

import java.util.concurrent.TimeoutException;

/**
 * A utility class to help open and interact with context menus triggered by a WebContents.
 */
public class ContextMenuUtils {
    /**
     * Callback helper that also provides access to the last display ContextMenu.
     */
    private static class OnContextMenuShownHelper extends CallbackHelper {
        private ContextMenu mContextMenu;

        public void notifyCalled(ContextMenu menu) {
            mContextMenu = menu;
            notifyCalled();
        }

        public ContextMenu getContextMenu() {
            assert getCallCount() > 0;
            return mContextMenu;
        }
    }

    /**
     * Opens a context menu.
     * @param testCase              The test harness.
     * @param tab                   The tab to open a context menu for.
     * @param openerDOMNodeId       The DOM node to long press to open the context menu for.
     * @return                      The {@link ContextMenu} that was opened.
     * @throws InterruptedException
     * @throws TimeoutException
     */
    public static ContextMenu openContextMenu(ActivityInstrumentationTestCase2<?> testCase,
            Tab tab, String openerDOMNodeId) throws InterruptedException, TimeoutException {
        String jsCode = "document.getElementById('" + openerDOMNodeId + "')";
        return openContextMenuByJs(testCase, tab, jsCode);
    }

    /**
     * Opens a context menu.
     * @param testCase              The test harness.
     * @param tab                   The tab to open a context menu for.
     * @param jsCode                The javascript to get the DOM node to long press to
     *                              open the context menu for.
     * @return                      The {@link ContextMenu} that was opened.
     * @throws InterruptedException
     * @throws TimeoutException
     */
    public static ContextMenu openContextMenuByJs(ActivityInstrumentationTestCase2<?> testCase,
            Tab tab, String jsCode) throws InterruptedException, TimeoutException {
        final OnContextMenuShownHelper helper = new OnContextMenuShownHelper();
        tab.addObserver(new EmptyTabObserver() {
            @Override
            public void onContextMenuShown(Tab tab, ContextMenu menu) {
                helper.notifyCalled(menu);
                tab.removeObserver(this);
            }
        });
        int callCount = helper.getCallCount();
        DOMUtils.longPressNodeByJs(testCase, tab.getContentViewCore(), jsCode);

        helper.waitForCallback(callCount);
        return helper.getContextMenu();
    }

    /**
     * Opens and selects an item from a context menu.
     * @param testCase              The test harness.
     * @param tab                   The tab to open a context menu for.
     * @param openerDOMNodeId       The DOM node to long press to open the context menu for.
     * @param itemId                The context menu item ID to select.
     * @throws InterruptedException
     * @throws TimeoutException
     */
    public static void selectContextMenuItem(ActivityInstrumentationTestCase2<?> testCase,
            Tab tab, String openerDOMNodeId, final int itemId) throws InterruptedException,
            TimeoutException {
        String jsCode = "document.getElementById('" + openerDOMNodeId + "')";
        selectContextMenuItemByJs(testCase, tab, jsCode, itemId);
    }

    /**
     * Long presses to open and selects an item from a context menu.
     * @param testCase              The test harness.
     * @param tab                   The tab to open a context menu for.
     * @param jsCode                The javascript to get the DOM node to long press
     *                              to open the context menu for.
     * @param itemId                The context menu item ID to select.
     * @throws InterruptedException
     * @throws TimeoutException
     */
    public static void selectContextMenuItemByJs(ActivityInstrumentationTestCase2<?> testCase,
            Tab tab, String jsCode, final int itemId) throws InterruptedException,
            TimeoutException {
        ContextMenu menu = openContextMenuByJs(testCase, tab, jsCode);
        Assert.assertNotNull("Failed to open context menu", menu);

        selectOpenContextMenuItem(testCase, menu, itemId);
    }

    /**
     * Opens and selects an item from a context menu.
     * @param testCase              The test harness.
     * @param tab                   The tab to open a context menu for.
     * @param openerDOMNodeId       The DOM node to long press to open the context menu for.
     * @param itemTitle             The title of the context menu item to select.
     * @throws InterruptedException
     * @throws TimeoutException
     */
    public static void selectContextMenuItemByTitle(ActivityInstrumentationTestCase2<?> testCase,
            Tab tab, String openerDOMNodeId,
            String itemTitle) throws InterruptedException, TimeoutException {
        ContextMenu menu = openContextMenu(testCase, tab, openerDOMNodeId);
        Assert.assertNotNull("Failed to open context menu", menu);

        Integer itemId = null;
        for (int i = 0; i < menu.size(); i++) {
            MenuItem item = menu.getItem(i);
            if (TextUtils.equals(item.getTitle(), itemTitle)) {
                itemId = item.getItemId();
                break;
            }
        }
        Assert.assertNotNull("Couldn't find context menu item for '" + itemTitle + "'", itemId);

        selectOpenContextMenuItem(testCase, menu, itemId);
    }

    private static void selectOpenContextMenuItem(
            final ActivityInstrumentationTestCase2<?> testCase, final ContextMenu menu,
            final int itemId) throws InterruptedException {
        MenuItem item = menu.findItem(itemId);
        Assert.assertNotNull("Could not find '" + itemId + "' in menu", item);
        Assert.assertTrue("'" + itemId + "' is not visible", item.isVisible());
        Assert.assertTrue("'" + itemId + "' is not enabled", item.isEnabled());

        testCase.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                boolean activated = menu.performIdentifierAction(itemId, 0);
                Assert.assertTrue("Failed to activate '" + itemId + "' in menu", activated);
            }
        });

        CriteriaHelper.pollInstrumentationThread(new Criteria("Activity did not regain focus.") {
            @Override
            public boolean isSatisfied() {
                return testCase.getActivity().hasWindowFocus();
            }
        });
    }
}
