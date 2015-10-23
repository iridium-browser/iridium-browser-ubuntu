// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.test.suitebuilder.annotation.LargeTest;
import android.view.KeyEvent;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.PrerenderTestHelper;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.content.browser.test.util.KeyUtils;
import org.chromium.ui.base.PageTransition;

import java.util.concurrent.TimeoutException;

/**
 * Prerender tests.
 *
 * Tests are disabled on low-end devices. These only support one renderer for performance reasons.
 */
public class PrerenderTest extends ChromeTabbedActivityTestBase {

    // junit.framework.Assert has
    //  assertEquals(Object,Object)
    //  assertEquals(String,String) and
    //  assertNotSame(Object,Object), but no
    //  assertNotSame(String,String).
    // Since String equality needs equals() and object equality uses
    // ==, the lack of a proper API means it's easy to use object
    // equality by accident since Object is a base class of String.
    // But that's not what you want!
    void assertNotEquals(String expected, String actual) {
        assertFalse(expected.equals(actual));
    }

    /**
     * We are using Autocomplete Action Predictor to decide whether or not to prerender.
    /* Without any training data the default action should be no-prerender.
     */
    @LargeTest
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"TabContents"})
    public void testNoPrerender() throws InterruptedException {
        String testUrl = TestHttpServerClient.getUrl(
                "chrome/test/data/android/prerender/google.html");
        final Tab tab = getActivity().getActivityTab();

        // Mimic user behavior: touch to focus then type some URL.
        // Since this is a URL, it should be prerendered.
        // Type one character at a time to properly simulate input
        // to the action predictor.
        typeInOmnibox(testUrl, true);

        assertFalse("URL should not have been prerendered.",
                PrerenderTestHelper.waitForPrerenderUrl(tab, testUrl, true));
        // Navigate should not use the prerendered version.
        assertEquals(TabLoadStatus.DEFAULT_PAGE_LOAD,
                loadUrlInTab(testUrl, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR, tab));
    }

    /*
    @LargeTest
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"TabContents"})
    crbug.com/339668
    */
    @DisabledTest
    public void testPrerenderNotDead() throws InterruptedException, TimeoutException {
        String testUrl = TestHttpServerClient.getUrl(
                "chrome/test/data/android/prerender/google.html");
        PrerenderTestHelper.trainAutocompleteActionPredictorAndTestPrerender(testUrl, this);
        final Tab tab = getActivity().getActivityTab();
        // Navigate should use the prerendered version.
        assertEquals(TabLoadStatus.FULL_PRERENDERED_PAGE_LOAD,
                loadUrlInTab(testUrl, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR, tab));

        // Prerender again with new text; make sure we get something different.
        String newTitle = "Welcome to the YouTube";
        testUrl = TestHttpServerClient.getUrl("chrome/test/data/android/prerender/youtube.html");
        PrerenderTestHelper.trainAutocompleteActionPredictorAndTestPrerender(testUrl, this);

        // Make sure the current tab title is NOT from the prerendered page.
        assertNotEquals(newTitle, tab.getTitle());

        TabTitleObserver observer = new TabTitleObserver(tab, newTitle);

        // Now commit and see the new title.
        final UrlBar urlBar = (UrlBar) getActivity().findViewById(R.id.url_bar);
        assertNotNull("urlBar is null", urlBar);
        KeyUtils.singleKeyEventView(getInstrumentation(), urlBar, KeyEvent.KEYCODE_ENTER);

        observer.waitForTitleUpdate(5);
        assertEquals(newTitle, tab.getTitle());
    }

    /**
     * Tests that we do get the page load finished notification even when a page has been fully
     * prerendered.
     */
    /*
    @LargeTest
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"TabContents"})
    crbug.com/339668
    */
    @DisabledTest
    public void testPageLoadFinishNotification() throws InterruptedException {
        String url = TestHttpServerClient.getUrl("chrome/test/data/android/prerender/google.html");
        PrerenderTestHelper.trainAutocompleteActionPredictorAndTestPrerender(url, this);
        // Now let's press enter to validate the suggestion. The prerendered page should be
        // committed and we should get a page load finished notification (which would trigger the
        // page load).
        ChromeTabUtils.waitForTabPageLoaded(getActivity().getActivityTab(), new Runnable() {
            @Override
            public void run() {
                final UrlBar urlBar = (UrlBar) getActivity().findViewById(R.id.url_bar);
                assertNotNull("urlBar is null", urlBar);
                KeyUtils.singleKeyEventView(getInstrumentation(), urlBar, KeyEvent.KEYCODE_ENTER);
            }
        });
    }

    /**
     * Tests that we don't crash when dismissing a prerendered page with infobars and unlonad
     * handler (See bug 5757331).
     * Note that this bug happened with the instant code. Now that we use Wicked Fast, we don't
     * deal with infobars ourselves.
     */
    /*
    @LargeTest
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"TabContents"})
    crbug.com/339668
    */
    @DisabledTest
    public void testInfoBarDismissed() throws InterruptedException {
        final String url = TestHttpServerClient.getUrl(
                "chrome/test/data/geolocation/geolocation_on_load.html");
        PrerenderTestHelper.trainAutocompleteActionPredictorAndTestPrerender(url, this);
        // Let's clear the URL bar, this will discard the prerendered WebContents and close the
        // infobars.
        final UrlBar urlBar = (UrlBar) getActivity().findViewById(R.id.url_bar);
        assertNotNull(urlBar);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                urlBar.requestFocus();
                urlBar.setText("");
            }
        });
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}
