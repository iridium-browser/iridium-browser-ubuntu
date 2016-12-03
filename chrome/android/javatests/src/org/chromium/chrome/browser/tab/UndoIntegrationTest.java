// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.test.suitebuilder.annotation.LargeTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;

import java.util.concurrent.TimeoutException;

/**
 * Tests undo and it's interactions with the UI.
 */
public class UndoIntegrationTest extends ChromeTabbedActivityTestBase {
    private static final String WINDOW_OPEN_BUTTON_URL = UrlUtils.encodeHtmlDataUri(
            "<html>"
            + "  <head>"
            + "  <script>"
            + "    function openWindow() {"
            + "      window.open('about:blank');"
            + "    }"
            + "  </script>"
            + "  </head>"
            + "  <body>"
            + "    <a id=\"link\" onclick=\"setTimeout(openWindow, 500);\">Open</a>"
            + "  </body>"
            + "</html>"
    );

    @Override
    public void startMainActivity() throws InterruptedException {
        SnackbarManager.setDurationForTesting(1500);
        startMainActivityOnBlankPage();
    }

    /**
     * Test that a tab that is closing can't open other windows.
     * @throws InterruptedException
     * @throws TimeoutException
     */
    @LargeTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testAddNewContentsFromClosingTab() throws InterruptedException, TimeoutException {
        loadUrl(WINDOW_OPEN_BUTTON_URL);

        final TabModel model = getActivity().getTabModelSelector().getCurrentModel();
        final Tab tab = TabModelUtils.getCurrentTab(model);

        // Clock on the link that will trigger a delayed window popup.
        DOMUtils.clickNode(this, tab.getContentViewCore(), "link");

        // Attempt to close the tab, which will delay closing until the undo timeout goes away.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                TabModelUtils.closeTabById(model, tab.getId(), true);
                assertTrue("Tab was not marked as closing", tab.isClosing());
                assertTrue("Tab is not actually closing", model.isClosurePending(tab.getId()));
            }
        });

        // Give the model a chance to process the undo and close the tab.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !model.isClosurePending(tab.getId()) && model.getCount() == 0;
            }
        });

        // Validate that the model doesn't contain the original tab or any newly opened tabs.
        assertFalse("Model is still waiting to close the tab", model.isClosurePending(tab.getId()));
        assertEquals("Model still has tabs", 0, model.getCount());
    }
}
