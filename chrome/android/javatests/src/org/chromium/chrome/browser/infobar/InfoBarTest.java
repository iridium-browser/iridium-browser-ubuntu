// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.content.Context;
import android.os.Environment;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.Smoke;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.preferences.datareduction.DataReductionPromoUtils;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.test.EmbeddedTestServer;

import java.net.HttpURLConnection;
import java.util.List;

/** Tests for the InfoBars. */
public class InfoBarTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final long MAX_TIMEOUT = scaleTimeout(2000);
    private static final int CHECK_INTERVAL = 500;
    private static final String GEOLOCATION_PAGE =
            "/chrome/test/data/geolocation/geolocation_on_load.html";
    private static final String POPUP_PAGE =
            "/chrome/test/data/popup_blocker/popup-window-open.html";
    public static final String HELLO_WORLD_URL = UrlUtils.encodeHtmlDataUri(
            "<html>"
            + "<head><title>Hello, World!</title></head>"
            + "<body>Hello, World!</body>"
            + "</html>");
    private static final String SHARED_PREF_DISPLAYED_FRE_OR_SECOND_PROMO_VERSION =
            "displayed_data_reduction_promo_version";
    private static final String M51_VERSION = "Chrome 51.0.2704.0";

    private EmbeddedTestServer mTestServer;
    private InfoBarTestAnimationListener mListener;

    private void waitUntilNoInfoBarsExist() throws InterruptedException {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getInfoBars().isEmpty();
            }
        });
    }

    private void waitUntilDataReductionPromoInfoBarAppears() throws InterruptedException {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                List<InfoBar> infobars = getInfoBars();
                if (infobars.size() != 1) return false;
                return infobars.get(0) instanceof DataReductionPromoInfoBar;
            }
        });
    }

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
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (getActivity().getActivityTab() == null) return false;
                if (getActivity().getActivityTab().getInfoBarContainer() == null) return false;
                return true;
            }
        });
        InfoBarContainer container = getActivity().getActivityTab().getInfoBarContainer();
        mListener =  new InfoBarTestAnimationListener();
        container.setAnimationListener(mListener);

        mTestServer = EmbeddedTestServer.createAndStartFileServer(
                getInstrumentation().getContext(), Environment.getExternalStorageDirectory());

        // Using an AdvancedMockContext allows us to use a fresh in-memory SharedPreference.
        Context context = new AdvancedMockContext(
                getInstrumentation().getTargetContext().getApplicationContext());
        ContextUtils.initApplicationContextForTests(context);
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    /**
     * Verify PopUp InfoBar.
     */
    @Smoke
    @MediumTest
    @Feature({"Browser", "Main"})
    @DisabledTest(message = "crbug.com/593003")
    public void testInfoBarForPopUp() throws InterruptedException {
        loadUrl(mTestServer.getURL(POPUP_PAGE));
        assertTrue("InfoBar not added", mListener.addInfoBarAnimationFinished());

        List<InfoBar> infoBars = getInfoBars();
        assertEquals("Wrong infobar count", 1, infoBars.size());
        assertTrue(InfoBarUtil.hasPrimaryButton(infoBars.get(0)));
        assertFalse(InfoBarUtil.hasSecondaryButton(infoBars.get(0)));
        InfoBarUtil.clickPrimaryButton(infoBars.get(0));
        assertTrue("InfoBar not removed.", mListener.removeInfoBarAnimationFinished());
        assertEquals("Wrong infobar count", 0, infoBars.size());

        // A second load should not show the infobar.
        loadUrl(mTestServer.getURL(POPUP_PAGE));
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
        loadUrl(mTestServer.getURL(GEOLOCATION_PAGE));
        assertTrue("InfoBar not added", mListener.addInfoBarAnimationFinished());

        // Make sure it has OK/Cancel buttons.
        List<InfoBar> infoBars = getInfoBars();
        assertEquals("Wrong infobar count", 1, infoBars.size());
        assertTrue(InfoBarUtil.hasPrimaryButton(infoBars.get(0)));
        assertTrue(InfoBarUtil.hasSecondaryButton(infoBars.get(0)));

        loadUrl(HELLO_WORLD_URL);
        assertTrue("InfoBar not removed.", mListener.removeInfoBarAnimationFinished());
        assertTrue("Wrong infobar count", getInfoBars().isEmpty());
    }


    /**
     * Verify Geolocation creates an InfoBar and that it's destroyed when navigating back.
     */
    @MediumTest
    @Feature({"Browser"})
    public void testInfoBarForGeolocationDisappearsOnBack() throws InterruptedException {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        loadUrl(HELLO_WORLD_URL);
        loadUrl(mTestServer.getURL(GEOLOCATION_PAGE));
        assertTrue("InfoBar not added.", mListener.addInfoBarAnimationFinished());

        assertEquals("Wrong infobar count", 1, getInfoBars().size());

        // Navigate back and ensure the InfoBar has been removed.
        getInstrumentation().runOnMainSync(
                new Runnable() {
                    @Override
                    public void run() {
                        getActivity().getActivityTab().goBack();
                    }
                });
        CriteriaHelper.pollInstrumentationThread(
                new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return getInfoBars().isEmpty();
                    }
                },
                MAX_TIMEOUT, CHECK_INTERVAL);
        assertTrue("InfoBar not removed.", mListener.removeInfoBarAnimationFinished());
    }

    /**
     * Verify the Data Reduction Promo infobar is shown and clicking the primary button dismisses
     * it.
     */
    @MediumTest
    @CommandLineFlags.Add("force-fieldtrials=DataCompressionProxyPromoVisibility/Enabled")
    @Feature({"Browser", "Main"})
    public void testDataReductionPromoInfoBar() throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertFalse("Data Reduction Proxy enabled",
                        DataReductionProxySettings.getInstance().isDataReductionProxyEnabled());
                // Fake the FRE or second run promo being shown in M51.
                DataReductionPromoUtils.saveFreOrSecondRunPromoDisplayed();
                ContextUtils.getAppSharedPreferences()
                        .edit()
                        .putString(SHARED_PREF_DISPLAYED_FRE_OR_SECOND_PROMO_VERSION, M51_VERSION)
                        .apply();
                // Add an infobar.
                assertTrue(DataReductionPromoInfoBar.maybeLaunchPromoInfoBar(
                        getActivity(), getActivity().getActivityTab().getWebContents(),
                        "http://google.com", false, false, HttpURLConnection.HTTP_OK));
            }
        });

        waitUntilDataReductionPromoInfoBarAppears();
        final List<InfoBar> infoBars = getInfoBars();
        assertTrue("InfoBar does not have primary button",
                InfoBarUtil.hasPrimaryButton(infoBars.get(0)));
        assertTrue("InfoBar does not have secondary button",
                InfoBarUtil.hasSecondaryButton(infoBars.get(0)));

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                InfoBarUtil.clickPrimaryButton(infoBars.get(0));
            }
        });

        // The renderer should have been killed and the infobar removed.
        waitUntilNoInfoBarsExist();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertTrue("Data Reduction Proxy not enabled",
                        DataReductionProxySettings.getInstance().isDataReductionProxyEnabled());
                // Turn Data Saver off so the promo can be reshown.
                DataReductionProxySettings.getInstance().setDataReductionProxyEnabled(getActivity(),
                        false);
                // Try to add an infobar. Infobar should not be added since it has already been
                // shown.
                assertFalse(DataReductionPromoInfoBar.maybeLaunchPromoInfoBar(
                        getActivity(), getActivity().getActivityTab().getWebContents(),
                        "http://google.com", false, false, HttpURLConnection.HTTP_OK));
            }
        });
    }

    /**
     * Verify the Data Reduction Promo infobar is shown and clicking the secondary button dismisses
     * it.
     */
    @MediumTest
    @CommandLineFlags.Add("force-fieldtrials=DataCompressionProxyPromoVisibility/Enabled")
    @Feature({"Browser", "Main"})
    public void testDataReductionPromoInfoBarDismissed() throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertFalse("Data Reduction Proxy enabled",
                        DataReductionProxySettings.getInstance().isDataReductionProxyEnabled());
                // Fake the first run experience or second run promo being shown in M51.
                DataReductionPromoUtils.saveFreOrSecondRunPromoDisplayed();
                ContextUtils.getAppSharedPreferences()
                        .edit()
                        .putString(SHARED_PREF_DISPLAYED_FRE_OR_SECOND_PROMO_VERSION, M51_VERSION)
                        .apply();
                // Add an infobar.
                assertTrue(DataReductionPromoInfoBar.maybeLaunchPromoInfoBar(
                        getActivity(), getActivity().getActivityTab().getWebContents(),
                        "http://google.com", false, false, HttpURLConnection.HTTP_OK));
            }
        });

        waitUntilDataReductionPromoInfoBarAppears();
        final List<InfoBar> infoBars = getInfoBars();
        assertTrue("InfoBar does not have primary button",
                InfoBarUtil.hasPrimaryButton(infoBars.get(0)));
        assertTrue("InfoBar does not have secondary button",
                InfoBarUtil.hasSecondaryButton(infoBars.get(0)));

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                InfoBarUtil.clickSecondaryButton(infoBars.get(0));
            }
        });

        // The renderer should have been killed and the infobar removed.
        waitUntilNoInfoBarsExist();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertFalse("Data Reduction Proxy enabled",
                        DataReductionProxySettings.getInstance().isDataReductionProxyEnabled());
                // Try to add an infobar. Infobar should not be added since the user clicked
                // dismiss.
                assertFalse(DataReductionPromoInfoBar.maybeLaunchPromoInfoBar(
                        getActivity(), getActivity().getActivityTab().getWebContents(),
                        "http://google.com", false, false, HttpURLConnection.HTTP_OK));
            }
        });
    }

    /**
     * Verify the Data Reduction Promo infobar is not shown when the fre or second run promo version
     * was not stored and the package was installed after M48.
     */
    @UiThreadTest
    @MediumTest
    @CommandLineFlags.Add("force-fieldtrials=DataCompressionProxyPromoVisibility/Enabled")
    @Feature({"Browser", "Main"})
    public void testDataReductionPromoInfoBarPostM48Install() {
        assertFalse("Data Reduction Proxy enabled",
                DataReductionProxySettings.getInstance().isDataReductionProxyEnabled());
        // Fake the first run experience or second run promo being shown.
        DataReductionPromoUtils.saveFreOrSecondRunPromoDisplayed();
        // Remove the version. Versions prior to M51 will not have the version pref.
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putString(SHARED_PREF_DISPLAYED_FRE_OR_SECOND_PROMO_VERSION, "")
                .apply();
        // Add an infobar. Infobar should not be added since the first run experience or second run
        // promo version was not shown and the package was installed after M48.
        assertFalse(DataReductionPromoInfoBar.maybeLaunchPromoInfoBar(
                getActivity(), getActivity().getActivityTab().getWebContents(),
                "http://google.com", false, false, HttpURLConnection.HTTP_OK));
    }

    /**
     * Verify that the Data Reduction Promo infobar is not shown if the first run experience or
     * Infobar promo hasn't been shown or if it hasn't been two versions since the promo was shown.
     */
    @UiThreadTest
    @MediumTest
    @CommandLineFlags.Add("force-fieldtrials=DataCompressionProxyPromoVisibility/Enabled")
    @Feature({"Browser", "Main"})
    public void testDataReductionPromoInfoBarFreOptOut() {
        // Try to add an infobar. Infobar should not be added since the first run experience or
        // second run promo hasn't been shown.
        assertFalse(DataReductionPromoInfoBar.maybeLaunchPromoInfoBar(
                getActivity(), getActivity().getActivityTab().getWebContents(),
                "http://google.com", false, false, HttpURLConnection.HTTP_OK));

        // Fake showing the FRE.
        DataReductionPromoUtils.saveFreOrSecondRunPromoDisplayed();

        // Try to add an infobar. Infobar should not be added since the first run experience was
        // just shown.
        assertFalse(DataReductionPromoInfoBar.maybeLaunchPromoInfoBar(
                getActivity(), getActivity().getActivityTab().getWebContents(),
                "http://google.com", false, false, HttpURLConnection.HTTP_OK));

        // Fake the first run experience or second run promo being shown in M51.
        DataReductionPromoUtils.saveFreOrSecondRunPromoDisplayed();
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putString(SHARED_PREF_DISPLAYED_FRE_OR_SECOND_PROMO_VERSION, M51_VERSION)
                .apply();
        DataReductionPromoUtils.saveFrePromoOptOut(true);

        // Try to add an infobar. Infobar should not be added since the user opted out on the
        // first run experience.
        assertFalse(DataReductionPromoInfoBar.maybeLaunchPromoInfoBar(
                getActivity(), getActivity().getActivityTab().getWebContents(),
                "http://google.com", false, false, HttpURLConnection.HTTP_OK));
    }

    /**
     * Verifies the unresponsive renderer notification creates an InfoBar.
     */
    @Smoke
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testInfoBarForHungRenderer() throws InterruptedException {
        loadUrl(HELLO_WORLD_URL);

        // Fake an unresponsive renderer signal.
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                CommandLine.getInstance().appendSwitch(ChromeSwitches.ENABLE_HUNG_RENDERER_INFOBAR);
                getActivity()
                        .getActivityTab()
                        .getTabWebContentsDelegateAndroid()
                        .rendererUnresponsive();
            }
        });
        assertTrue("InfoBar not added", mListener.addInfoBarAnimationFinished());

        // Make sure it has Kill/Wait buttons.
        List<InfoBar> infoBars = getInfoBars();
        assertEquals("Wrong infobar count", 1, infoBars.size());
        assertTrue(InfoBarUtil.hasPrimaryButton(infoBars.get(0)));
        assertTrue(InfoBarUtil.hasSecondaryButton(infoBars.get(0)));

        // Fake a responsive renderer signal.
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                getActivity()
                        .getActivityTab()
                        .getTabWebContentsDelegateAndroid()
                        .rendererResponsive();
            }
        });
        assertTrue("InfoBar not removed.", mListener.removeInfoBarAnimationFinished());
        assertTrue("Wrong infobar count", getInfoBars().isEmpty());
    }

    /**
     * Verifies the hung renderer InfoBar can kill the hung renderer.
     */
    @Smoke
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testInfoBarForHungRendererCanKillRenderer() throws InterruptedException {
        loadUrl(HELLO_WORLD_URL);

        // Fake an unresponsive renderer signal.
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                CommandLine.getInstance().appendSwitch(ChromeSwitches.ENABLE_HUNG_RENDERER_INFOBAR);
                getActivity()
                        .getActivityTab()
                        .getTabWebContentsDelegateAndroid()
                        .rendererUnresponsive();
            }
        });
        assertTrue("InfoBar not added", mListener.addInfoBarAnimationFinished());

        // Make sure it has Kill/Wait buttons.
        final List<InfoBar> infoBars = getInfoBars();
        assertEquals("Wrong infobar count", 1, infoBars.size());
        assertTrue(InfoBarUtil.hasPrimaryButton(infoBars.get(0)));
        assertTrue(InfoBarUtil.hasSecondaryButton(infoBars.get(0)));

        // Activite the Kill button.
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                InfoBarUtil.clickPrimaryButton(infoBars.get(0));
            }
        });

        // The renderer should have been killed and the InfoBar removed.
        assertTrue("InfoBar not removed.", mListener.removeInfoBarAnimationFinished());
        assertTrue("Wrong infobar count", getInfoBars().isEmpty());
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getActivity().getActivityTab().isShowingSadTab();
            }
        }, MAX_TIMEOUT, CHECK_INTERVAL);
    }

    /**
     * Verify InfoBarContainers swap the WebContents they are monitoring properly.
     */
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testInfoBarContainerSwapsWebContents() throws InterruptedException {
        // Add an infobar.
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        loadUrl(mTestServer.getURL(GEOLOCATION_PAGE));
        assertTrue("InfoBar not added", mListener.addInfoBarAnimationFinished());
        assertEquals("Wrong infobar count", 1, getInfoBars().size());

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
        assertEquals("Wrong infobar count", 0, getInfoBars().size());

        // Revisiting the original page should make the InfoBar reappear.
        InfoBarTestAnimationListener addListener = new InfoBarTestAnimationListener();
        getActivity().getActivityTab().getInfoBarContainer().setAnimationListener(addListener);
        loadUrl(mTestServer.getURL(GEOLOCATION_PAGE));
        assertTrue("InfoBar not added", addListener.addInfoBarAnimationFinished());
        assertEquals("Wrong infobar count", 1, getInfoBars().size());
    }
}
