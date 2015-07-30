// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.app.Activity;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromiumApplication;
import org.chromium.ui.base.WindowAndroid;

/**
 * Java implementation of dom_distiller::android::ExternalFeedbackReporterAndroid.
 */
@JNINamespace("dom_distiller::android")
public final class DomDistillerFeedbackReporter {
    /**
     * An {@link ExternalFeedbackReporter} that does nothing.
     */
    public static class NoOpExternalFeedbackReporter implements ExternalFeedbackReporter {
        @Override
        public void reportFeedback(Activity activity, String url, boolean good) {
        }
    }

    private static ExternalFeedbackReporter sExternalFeedbackReporter;

    /**
     * A static method for native code to call to call the external feedback form.
     * @param window WindowAndroid object to get an activity from.
     * @param url The URL to report feedback for.
     * @param good True if the feedback is good and false if not.
     */
    @CalledByNative
    public static void reportFeedbackWithWindow(WindowAndroid window, String url, boolean good) {
        ThreadUtils.assertOnUiThread();
        Activity activity = window.getActivity().get();
        if (sExternalFeedbackReporter == null) {
            ChromiumApplication application = (ChromiumApplication) activity.getApplication();
            sExternalFeedbackReporter = application.createDomDistillerFeedbackLauncher();
        }
        sExternalFeedbackReporter.reportFeedback(activity, url, good);
    }

    private DomDistillerFeedbackReporter() {}
}
