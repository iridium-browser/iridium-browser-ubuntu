// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.Smoke;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.WebContents;

import java.util.List;

/** Tests for the InfoBars. */
public class InfoBarTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final long MAX_TIMEOUT = scaleTimeout(2000);
    private static final int CHECK_INTERVAL = 500;
    private static final String GEOLOCATION_PAGE =
            "chrome/test/data/geolocation/geolocation_on_load.html";
    private static final String POPUP_PAGE =
            "chrome/test/data/popup_blocker/popup-window-open.html";
    public static final String HELLO_WORLD_URL = UrlUtils.encodeHtmlDataUri(
            "<html>"
            + "<head><title>Hello, World!</title></head>"
            + "<body>Hello, World!</body>"
            + "</html>");

    private InfoBarTestAnimationListener mListener;

    public InfoBarTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        // Register for animation notifications
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (getActivity().getActivityTab() == null) return false;
                if (getActivity().getActivityTab().getInfoBarContainer() == null) return false;
                return true;
            }
        }));
        InfoBarContainer container = getActivity().getActivityTab().getInfoBarContainer();
        mListener =  new InfoBarTestAnimationListener();
        container.setAnimationListener(mListener);
    }

    /**
     * Verify PopUp InfoBar. Only basic triggering verified due to lack of tabs
     * in ChromeShell
     */
    @Smoke
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testInfoBarForPopUp() throws InterruptedException {
        loadUrl(TestHttpServerClient.getUrl(POPUP_PAGE));
        assertTrue("InfoBar not added", mListener.addInfoBarAnimationFinished());

        List<InfoBar> infoBars = getActivity().getActivityTab().getInfoBarContainer().getInfoBars();
        assertEquals("Wrong infobar count", 1, infoBars.size());
        assertTrue(InfoBarUtil.hasPrimaryButton(infoBars.get(0)));
        assertFalse(InfoBarUtil.hasSecondaryButton(infoBars.get(0)));
        InfoBarUtil.clickPrimaryButton(infoBars.get(0));
        assertTrue("InfoBar not removed.", mListener.removeInfoBarAnimationFinished());
        assertEquals("Wrong infobar count", 0, infoBars.size());

        // A second load should not show the infobar.
        loadUrl(TestHttpServerClient.getUrl(POPUP_PAGE));
        assertFalse("InfoBar added when it should not", mListener.addInfoBarAnimationFinished());
    }

    /**
     * Verify Geolocation creates an InfoBar.
     */
    @Smoke
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testInfoBarForGeolocation() throws InterruptedException {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        loadUrl(TestHttpServerClient.getUrl(GEOLOCATION_PAGE));
        assertTrue("InfoBar not added", mListener.addInfoBarAnimationFinished());

        // Make sure it has OK/Cancel buttons.
        List<InfoBar> infoBars = getActivity().getActivityTab().getInfoBarContainer().getInfoBars();
        assertEquals("Wrong infobar count", 1, infoBars.size());
        assertTrue(InfoBarUtil.hasPrimaryButton(infoBars.get(0)));
        assertTrue(InfoBarUtil.hasSecondaryButton(infoBars.get(0)));

        loadUrl(HELLO_WORLD_URL);
        assertTrue("InfoBar not removed.", mListener.removeInfoBarAnimationFinished());
        infoBars = getActivity().getActivityTab().getInfoBarContainer().getInfoBars();
        assertTrue("Wrong infobar count", infoBars.isEmpty());
    }


    /**
     * Verify Geolocation creates an InfoBar and that it's destroyed when navigating back.
     */
    @MediumTest
    @Feature({"Browser"})
    public void testInfoBarForGeolocationDisappearsOnBack() throws InterruptedException {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        loadUrl(HELLO_WORLD_URL);
        loadUrl(TestHttpServerClient.getUrl(GEOLOCATION_PAGE));
        assertTrue("InfoBar not added.", mListener.addInfoBarAnimationFinished());

        List<InfoBar> infoBars = getActivity().getActivityTab().getInfoBarContainer().getInfoBars();
        assertEquals("Wrong infobar count", 1, infoBars.size());

        // Navigate back and ensure the InfoBar has been removed.
        getInstrumentation().runOnMainSync(
                new Runnable() {
                    @Override
                    public void run() {
                        getActivity().getActivityTab().goBack();
                    }
                });
        CriteriaHelper.pollForCriteria(
                new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        List<InfoBar> infoBars =
                                getActivity().getActivityTab().getInfoBarContainer().getInfoBars();
                        return infoBars.isEmpty();
                    }
                },
                MAX_TIMEOUT, CHECK_INTERVAL);
        assertTrue("InfoBar not removed.", mListener.removeInfoBarAnimationFinished());
    }

    /**
     * Verify InfoBarContainers swap the WebContents they are monitoring properly.
     */
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testInfoBarContainerSwapsWebContents() throws InterruptedException {
        // Add an infobar.
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        loadUrl(TestHttpServerClient.getUrl(GEOLOCATION_PAGE));
        assertTrue("InfoBar not added", mListener.addInfoBarAnimationFinished());
        List<InfoBar> infoBars = getActivity().getActivityTab().getInfoBarContainer().getInfoBars();
        assertEquals("Wrong infobar count", 1, infoBars.size());

        // Swap out the WebContents and send the user somewhere so that the InfoBar gets removed.
        InfoBarTestAnimationListener removeListener = new InfoBarTestAnimationListener();
        getActivity().getActivityTab().getInfoBarContainer().setAnimationListener(removeListener);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                WebContents newContents = WebContentsFactory.createWebContents(false, false);
                getActivity().getActivityTab().swapWebContents(newContents, false, false);
            }
        });
        loadUrl(HELLO_WORLD_URL);
        assertTrue("InfoBar not removed.", removeListener.removeInfoBarAnimationFinished());
        infoBars = getActivity().getActivityTab().getInfoBarContainer().getInfoBars();
        assertEquals("Wrong infobar count", 0, infoBars.size());

        // Revisiting the original page should make the InfoBar reappear.
        InfoBarTestAnimationListener addListener = new InfoBarTestAnimationListener();
        getActivity().getActivityTab().getInfoBarContainer().setAnimationListener(addListener);
        loadUrl(TestHttpServerClient.getUrl(GEOLOCATION_PAGE));
        assertTrue("InfoBar not added", addListener.addInfoBarAnimationFinished());
        infoBars = getActivity().getActivityTab().getInfoBarContainer().getInfoBars();
        assertEquals("Wrong infobar count", 1, infoBars.size());
    }
}
