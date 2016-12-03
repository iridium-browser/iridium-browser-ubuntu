// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.test.suitebuilder.annotation.MediumTest;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.PopupMenu;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.test.partnercustomizations.TestPartnerBrowserCustomizationsProvider;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Integration tests for the partner disabling incognito mode feature.
 */
public class PartnerDisableIncognitoModeIntegrationTest extends
        BasePartnerBrowserCustomizationIntegrationTest {

    @Override
    public void startMainActivity() throws InterruptedException {
        // Each test will launch main activity, so purposefully omit here.
    }

    private void setParentalControlsEnabled(boolean enabled) {
        Uri uri = PartnerBrowserCustomizations.buildQueryUri(
                PartnerBrowserCustomizations.PARTNER_DISABLE_INCOGNITO_MODE_PATH);
        Bundle bundle = new Bundle();
        bundle.putBoolean(
                TestPartnerBrowserCustomizationsProvider.INCOGNITO_MODE_DISABLED_KEY, enabled);
        Context context = getInstrumentation().getTargetContext();
        context.getContentResolver().call(uri, "setIncognitoModeDisabled", null, bundle);
    }

    private void assertIncognitoMenuItemEnabled(boolean enabled) throws ExecutionException {
        Menu menu = ThreadUtils.runOnUiThreadBlocking(new Callable<Menu>() {
            @Override
            public Menu call() throws Exception {
                // PopupMenu is a convenient way of building a temp menu.
                PopupMenu tempMenu = new PopupMenu(
                        getActivity(), getActivity().findViewById(R.id.menu_anchor_stub));
                tempMenu.inflate(R.menu.main_menu);
                Menu menu = tempMenu.getMenu();

                getActivity().prepareMenu(menu);
                return menu;
            }
        });
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            if (item.getItemId() == R.id.new_incognito_tab_menu_id && item.isVisible()) {
                assertEquals("Menu item enabled state is not correct.", enabled, item.isEnabled());
            }
        }
    }

    private void waitForParentalControlsEnabledState(final boolean parentalControlsEnabled)
            throws InterruptedException {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                // areParentalControlsEnabled is updated on a background thread, so we
                // also wait on the isIncognitoModeEnabled to ensure the updates on the
                // UI thread have also triggered.
                boolean retVal = parentalControlsEnabled
                        == PartnerBrowserCustomizations.isIncognitoDisabled();
                retVal &= parentalControlsEnabled
                        != PrefServiceBridge.getInstance().isIncognitoModeEnabled();
                return retVal;
            }
        });
    }

    private void toggleActivityForegroundState() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().onPause();
            }
        });
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().onStop();
            }
        });
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().onStart();
            }
        });
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().onResume();
            }
        });
    }

    @MediumTest
    @Feature({"DisableIncognitoMode"})
    public void testIncognitoEnabledIfNoParentalControls() throws InterruptedException {
        setParentalControlsEnabled(false);
        startMainActivityOnBlankPage();
        waitForParentalControlsEnabledState(false);
        newIncognitoTabFromMenu();
    }

    @MediumTest
    @Feature({"DisableIncognitoMode"})
    public void testIncognitoMenuItemEnabledBasedOnParentalControls()
            throws InterruptedException, ExecutionException {
        setParentalControlsEnabled(true);
        startMainActivityOnBlankPage();
        waitForParentalControlsEnabledState(true);
        assertIncognitoMenuItemEnabled(false);

        setParentalControlsEnabled(false);
        toggleActivityForegroundState();
        waitForParentalControlsEnabledState(false);
        assertIncognitoMenuItemEnabled(true);
    }

    @MediumTest
    @Feature({"DisableIncognitoMode"})
    public void testEnabledParentalControlsClosesIncognitoTabs() throws InterruptedException {
        EmbeddedTestServer testServer = EmbeddedTestServer.createAndStartFileServer(
                getInstrumentation().getContext(), Environment.getExternalStorageDirectory());

        try {
            String[] testUrls = {
                testServer.getURL("/chrome/test/data/android/about.html"),
                testServer.getURL("/chrome/test/data/android/ok.txt"),
                testServer.getURL("/chrome/test/data/android/test.html")
            };

            setParentalControlsEnabled(false);
            startMainActivityOnBlankPage();
            waitForParentalControlsEnabledState(false);

            loadUrlInNewTab(testUrls[0], true);
            loadUrlInNewTab(testUrls[1], true);
            loadUrlInNewTab(testUrls[2], true);
            loadUrlInNewTab(testUrls[0], false);

            setParentalControlsEnabled(true);
            toggleActivityForegroundState();
            waitForParentalControlsEnabledState(true);

            CriteriaHelper.pollInstrumentationThread(Criteria.equals(0, new Callable<Integer>() {
                @Override
                public Integer call() {
                    return incognitoTabsCount();
                }
            }));
        } finally {
            testServer.stopAndDestroyServer();
        }
    }
}
