// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import org.chromium.base.CalledByNative;
import org.chromium.chrome.browser.ui.toolbar.ToolbarModelSecurityLevel;
import org.chromium.content_public.browser.WebContents;

/**
 * Provides a way of accessing toolbar data and state.
 */
public class ToolbarModel {

    /**
     * Delegate for providing additional information to the model.
     */
    public interface ToolbarModelDelegate {
        /**
         * @return The currently active WebContents being used by the Toolbar.
         */
        @CalledByNative("ToolbarModelDelegate")
        WebContents getActiveWebContents();
    }

    private long mNativeToolbarModelAndroid;

    /**
     * Fetch the security level for a given web contents.
     *
     * @param webContents The web contents to get the security level for.
     * @return The ToolbarModelSecurityLevel for the specified web contents.
     *
     * @see ToolbarModelSecurityLevel
     */
    public static int getSecurityLevelForWebContents(WebContents webContents) {
        if (webContents == null) return ToolbarModelSecurityLevel.NONE;
        return nativeGetSecurityLevelForWebContents(webContents);
    }

    /**
     * @param webContents The web contents to query for deprecated SHA-1 presence.
     * @return Whether the security level of the page was deprecated due to SHA-1.
     */
    public static boolean isDeprecatedSHA1Present(WebContents webContents) {
        if (webContents == null) return false;
        return nativeIsDeprecatedSHA1Present(webContents);
    }

    /**
     * Initialize the native counterpart of this model.
     * @param delegate The delegate that will be used by the model.
     */
    public void initialize(ToolbarModelDelegate delegate) {
        mNativeToolbarModelAndroid = nativeInit(delegate);
    }

    /**
     * Destroys the native ToolbarModel.
     */
    public void destroy() {
        if (mNativeToolbarModelAndroid == 0) return;
        nativeDestroy(mNativeToolbarModelAndroid);
        mNativeToolbarModelAndroid = 0;
    }

    /** @return The formatted text (URL or search terms) for display. */
    public String getText() {
        if (mNativeToolbarModelAndroid == 0) return null;
        return nativeGetText(mNativeToolbarModelAndroid);
    }

    /** @return The parameter in the url that triggers query extraction. */
    public String getQueryExtractionParam() {
        if (mNativeToolbarModelAndroid == 0) return null;
        return nativeGetQueryExtractionParam(mNativeToolbarModelAndroid);
    }

    /** @return The chip text from the search URL. */
    public String getCorpusChipText() {
        if (mNativeToolbarModelAndroid == 0) return null;
        return nativeGetCorpusChipText(mNativeToolbarModelAndroid);
    }

    private static native int nativeGetSecurityLevelForWebContents(WebContents webContents);
    private static native boolean nativeIsDeprecatedSHA1Present(WebContents webContents);

    private native long nativeInit(ToolbarModelDelegate delegate);
    private native void nativeDestroy(long nativeToolbarModelAndroid);
    private native String nativeGetText(long nativeToolbarModelAndroid);
    private native String nativeGetQueryExtractionParam(long nativeToolbarModelAndroid);
    private native String nativeGetCorpusChipText(long nativeToolbarModelAndroid);
}
