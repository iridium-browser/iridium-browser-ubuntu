// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.shell;

import org.chromium.android_webview.R;
import org.chromium.base.CommandLine;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.content.app.ContentApplication;
import org.chromium.ui.base.ResourceBundle;

/**
 * The android_webview shell Application subclass.
 */
public class AwShellApplication extends ContentApplication {
    public AwShellApplication() {
        super(false /* mShouldInitializeApplicationStatusTracking */);
    }

    @Override
    public void onCreate() {
        super.onCreate();
    }

    @Override
    protected void initializeLibraryDependencies() {
        ResourceBundle.initializeLocalePaks(this, R.array.locale_paks);
    }

    @SuppressFBWarnings("DMI_HARDCODED_ABSOLUTE_FILENAME")
    @Override
    public void initCommandLine() {
        if (!CommandLine.isInitialized()) {
            CommandLine.initFromFile("/data/local/tmp/android-webview-command-line");
        }
    }
}
