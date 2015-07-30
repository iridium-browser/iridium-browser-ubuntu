// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.child_accounts;

import android.app.Activity;

import org.chromium.base.CalledByNative;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromiumApplication;
import org.chromium.ui.base.WindowAndroid;

/**
 * Java implementation of ChildAccountFeedbackReporterAndroid.
 */
public final class ChildAccountFeedbackReporter {
    /**
     * An {@link ExternalFeedbackReporter} that does nothing.
     */
    public static class NoOpExternalFeedbackReporter implements ExternalFeedbackReporter {
        @Override
        public void reportFeedback(Activity activity, String description, String url) {}
    }

    private static ExternalFeedbackReporter sExternalFeedbackReporter;

    public static void reportFeedback(Activity activity,
                                      String description,
                                      String url) {
        ThreadUtils.assertOnUiThread();
        if (sExternalFeedbackReporter == null) {
            ChromiumApplication application = (ChromiumApplication) activity.getApplication();
            sExternalFeedbackReporter = application.createChildAccountFeedbackLauncher();
        }
        sExternalFeedbackReporter.reportFeedback(activity, description, url);
    }

    @CalledByNative
    public static void reportFeedbackWithWindow(WindowAndroid window,
                                                String description,
                                                String url) {
        reportFeedback(window.getActivity().get(), description, url);
    }

    private ChildAccountFeedbackReporter() {}
}
