// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import org.chromium.chrome.browser.ChromeApplication;

/**
 * A utility class for launching Chrome Settings.
 */
public class PreferencesLauncher {

    /**
     * Launches settings, either on the top-level page or on a subpage.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragmentName The name of the fragment to show, or null to show the top-level page.
     */
    public static void launchSettingsPage(Context context, String fragmentName) {
        Intent intent = createIntentForSettingsPage(context, fragmentName);
        context.startActivity(intent);
    }

    /**
     * Creates an intent for launching settings, either on the top-level settings page or a specific
     * subpage.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragmentName The name of the fragment to show, or null to show the top-level page.
     */
    public static Intent createIntentForSettingsPage(Context context, String fragmentName) {
        ChromeApplication application = (ChromeApplication) context.getApplicationContext();
        String activityName = application.getSettingsActivityName();

        Intent intent = new Intent();
        intent.setClassName(context, activityName);
        if (!(context instanceof Activity)) {
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        }
        if (fragmentName != null) {
            intent.putExtra(Preferences.EXTRA_SHOW_FRAGMENT, fragmentName);
            intent.putExtra(Preferences.EXTRA_DISPLAY_HOME_AS_UP, false);
        }
        return intent;
    }
}
