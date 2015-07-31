// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_browsertests_apk;

import android.content.Context;

import org.chromium.base.BaseChromiumApplication;
import org.chromium.base.PathUtils;
import org.chromium.base.ResourceExtractor;

/**
 * A basic content browser tests {@link android.app.Application}.
 */
public class ContentBrowserTestsApplication extends BaseChromiumApplication {

    private static final String[] MANDATORY_PAK_FILES = new String[] {
        "content_shell.pak",
        "icudtl.dat",
        "natives_blob.bin",
        "snapshot_blob.bin"
    };
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "content_shell";

    @Override
    public void onCreate() {
        super.onCreate();
        initializeApplicationParameters(this);
    }

    public static void initializeApplicationParameters(Context context) {
        ResourceExtractor.setMandatoryPaksToExtract(MANDATORY_PAK_FILES);
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX, context);
    }

}
