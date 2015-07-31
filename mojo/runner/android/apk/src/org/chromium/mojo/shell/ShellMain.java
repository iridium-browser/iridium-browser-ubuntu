// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.shell;

import android.app.Activity;
import android.content.Context;
import android.util.Log;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * A placeholder class to call native functions.
 **/
@JNINamespace("mojo::runner")
public class ShellMain {
    private static final String TAG = "ShellMain";

    // Directory where applications bundled with the shell will be extracted.
    private static final String LOCAL_APP_DIRECTORY = "local_apps";
    // The mojo_shell library is also an executable run in forked processes when running
    // multi-process.
    private static final String MOJO_SHELL_EXECUTABLE = "libmojo_runner.so";

    // Name of the file containing the assets to extract. File format is a file per line.
    private static final String ASSETS_LIST_NAME = "assets_list";

    /**
     * A guard flag for calling nativeInit() only once.
     **/
    private static boolean sInitialized = false;

    /**
     * Returns the names of the assets in ASSETS_LIST_NAME.
     */
    private static List<String> getAssetsList(Context context) throws IOException {
        List<String> results = new ArrayList<String>();
        BufferedReader reader = new BufferedReader(new InputStreamReader(
                context.getAssets().open(ASSETS_LIST_NAME), Charset.forName("UTF-8")));

        try {
            String line;
            while ((line = reader.readLine()) != null) {
                line = line.trim();
                // These two are read by the system and don't need to be extracted.
                if (!line.isEmpty() && !line.equals("bootstrap_java.dex.jar")
                        && !line.equals("libbootstrap.so")) {
                    results.add(line);
                }
            }
        } finally {
            reader.close();
        }
        return results;
    }

    /**
     * Initializes the native system.
     **/
    static void ensureInitialized(Context applicationContext, String[] parameters) {
        if (sInitialized) return;
        File localAppsDir = getLocalAppsDir(applicationContext);
        try {
            for (String assetPath : getAssetsList(applicationContext)) {
                FileHelper.extractFromAssets(applicationContext, assetPath, localAppsDir, false);
            }
            File mojoShell = new File(applicationContext.getApplicationInfo().nativeLibraryDir,
                    MOJO_SHELL_EXECUTABLE);

            List<String> parametersList = new ArrayList<String>();
            // Program name.
            if (parameters != null) {
                parametersList.addAll(Arrays.asList(parameters));
            }

            nativeInit(applicationContext, mojoShell.getAbsolutePath(),
                    parametersList.toArray(new String[parametersList.size()]),
                    localAppsDir.getAbsolutePath(),
                    getTmpDir(applicationContext).getAbsolutePath());
            sInitialized = true;
        } catch (Exception e) {
            Log.e(TAG, "ShellMain initialization failed.", e);
            throw new RuntimeException(e);
        }
    }

    /**
     * Starts the specified application in the specified context.
     *
     * @return <code>true</code> if an application has been launched.
     **/
    static boolean start() {
        return nativeStart();
    }

    /**
     * Adds the given URL to the set of mojo applications to run on start.
     */
    static void addApplicationURL(String url) {
        nativeAddApplicationURL(url);
    }

    private static File getLocalAppsDir(Context context) {
        return context.getDir(LOCAL_APP_DIRECTORY, Context.MODE_PRIVATE);
    }

    private static File getTmpDir(Context context) {
        return new File(context.getCacheDir(), "tmp");
    }

    @CalledByNative
    private static void finishActivity(Activity activity) {
        activity.finish();
    }

    /**
     * Initializes the native system. This API should be called only once per process.
     **/
    private static native void nativeInit(Context context, String mojoShellPath,
            String[] parameters, String bundledAppsDirectory, String tmpDir);

    private static native boolean nativeStart();

    private static native void nativeAddApplicationURL(String url);
}
