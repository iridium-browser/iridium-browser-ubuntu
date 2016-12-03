// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.annotations.JNINamespace;

import java.io.File;
import java.io.IOException;

/**
 * Provides Android WebView debugging entrypoints.
 *
 * Methods in this class can be called from any thread, including threads created by
 * the client of WebView.
 */
@JNINamespace("android_webview")
public class AwDebug {
    /**
     * Dump webview state (predominantly a minidump for all threads,
     * but including other information) to the file descriptor fd.
     *
     * It is expected that this method is found by reflection, as it
     * is not currently exposed by the android framework, and thus it
     * needs to be protected from the unwanted attention of ProGuard.
     *
     * The File argument must refer to a pre-existing file, which must
     * be able to be re-opened for reading and writing via its
     * canonical path. The file will be truncated upon re-opening.
     */
    public static boolean dumpWithoutCrashing(File dumpFile) {
        String dumpPath;
        try {
            dumpPath = dumpFile.getCanonicalPath();
        } catch (IOException e) {
            return false;
        }
        return nativeDumpWithoutCrashing(dumpPath);
    }

    private static native boolean nativeDumpWithoutCrashing(String dumpPath);
}
