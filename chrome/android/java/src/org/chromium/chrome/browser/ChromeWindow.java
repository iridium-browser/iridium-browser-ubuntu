// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.app.Dialog;

import com.google.android.gms.common.GooglePlayServicesUtil;

import org.chromium.chrome.browser.infobar.MessageInfoBar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.ActivityWindowAndroid;

/**
 * The window that has access to the main activity and is able to create and receive intents,
 * and show error messages.
 */
public class ChromeWindow extends ActivityWindowAndroid {
    /**
     * Creates Chrome specific ActivityWindowAndroid.
     * @param activity The activity that owns the ChromeWindow.
     */
    public ChromeWindow(ChromeActivity activity) {
        super(activity);
    }

    /**
     * @see GooglePlayServicesUtil#getErrorDialog(int, Activity, int)
     */
    public Dialog getGooglePlayServicesErrorDialog(int errorCode, int requestCode) {
        return GooglePlayServicesUtil.getErrorDialog(errorCode, getActivity().get(), requestCode);
    }

    /**
     * Shows an infobar error message overriding the WindowAndroid implementation.
     */
    @Override
    protected void showCallbackNonExistentError(String error) {
        Activity activity = getActivity().get();

        // We can assume that activity is a ChromeActivity because we require one to be passed in
        // in the constructor.
        Tab tab = activity != null ? ((ChromeActivity) activity).getActivityTab() : null;

        if (tab != null) {
            String message = (error);
            MessageInfoBar infobar = new MessageInfoBar(message);
            infobar.setExpireOnNavigation(false);
            tab.getInfoBarContainer().addInfoBar(infobar);
        } else {
            super.showCallbackNonExistentError(error);
        }
    }
}
