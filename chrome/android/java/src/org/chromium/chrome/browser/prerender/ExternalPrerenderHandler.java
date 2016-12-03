// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.prerender;

import android.app.Application;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Point;
import android.graphics.Rect;
import android.view.WindowManager;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

/**
 * A handler class for prerender requests coming from  other applications.
 */
@JNINamespace("prerender")
public class ExternalPrerenderHandler {

    private long mNativeExternalPrerenderHandler;

    public ExternalPrerenderHandler() {
        mNativeExternalPrerenderHandler = nativeInit();
    }

    /**
     * Add a prerender for the given url and given content view dimensions.
     * <p>
     * The generated {@link WebContents} does not actually contain the prerendered contents but
     * must be used as the container that you load the prerendered URL into.
     *
     * @param profile The profile to use for the prerender.
     * @param url The url to prerender.
     * @param referrer The referrer for the prerender request.
     * @param bounds The bounds for the content view (render widget host view) for the prerender.
     * @param prerenderOnCellular Whether the prerender should happen if the device has a cellular
     *                            connection.
     * @return The {@link WebContents} that is linked to this prerender. {@code null} if
     *         unsuccessful.
     */
    public WebContents addPrerender(Profile profile, String url, String referrer,
            Rect bounds, boolean prerenderOnCellular) {
        WebContents webContents = WebContentsFactory.createWebContents(false, false);
        if (addPrerender(profile, webContents, url, referrer, bounds, prerenderOnCellular)) {
            return webContents;
        }
        if (webContents != null) webContents.destroy();
        return null;
    }

    /**
     * Adds a prerender for the given URL to an existing {@link WebContents} with the given
     * dimensions.
     *
     * @param profile The profile to use for the prerender.
     * @param webContents The WebContents to add the prerender to.
     * @param url The url to prerender.
     * @param referrer The referrer for the prerender request.
     * @param bounds The bounds for the content view (render widget host view) for the prerender.
     * @param prerenderOnCellular Whether the prerender should happen if the device has a cellular
     *                            connection.
     * @return Whether the prerender was successful.
     */
    public boolean addPrerender(Profile profile, WebContents webContents, String url,
            String referrer, Rect bounds, boolean prerenderOnCellular) {
        return nativeAddPrerender(mNativeExternalPrerenderHandler, profile, webContents, url,
                referrer, bounds.top, bounds.left, bounds.bottom, bounds.right,
                prerenderOnCellular);
    }

    /**
     * Cancel the current prerender action on this {@link ExternalPrerenderHandler}.
     */
    public void cancelCurrentPrerender() {
        nativeCancelCurrentPrerender(mNativeExternalPrerenderHandler);
    }

    /**
     * Check whether a given url has been prerendering for the given profile and session id for the
     * given web contents.
     * @param profile The profile to check for prerendering.
     * @param url The url to check for prerender.
     * @param webContents The {@link WebContents} for which to compare the session info.
     * @return Whether the given url was prerendered.
     */
    @VisibleForTesting
    public static boolean hasPrerenderedUrl(Profile profile, String url, WebContents webContents)  {
        return nativeHasPrerenderedUrl(profile, url, webContents);
    }

    /**
     * Check whether a given url has been prerendering for the given profile and session id for the
     * given web contents, and has finished loading.
     * @param profile The profile to check for prerendering.
     * @param url The url to check for prerender.
     * @param webContents The {@link WebContents} for which to compare the session info.
     * @return Whether the given url was prerendered and has finished loading.
     */
    @VisibleForTesting
    public static boolean hasPrerenderedAndFinishedLoadingUrl(
            Profile profile, String url, WebContents webContents) {
        return nativeHasPrerenderedAndFinishedLoadingUrl(profile, url, webContents);
    }

    /**
     * Provides an estimate of the contents size.
     *
     * The estimate is likely to be incorrect. This is not a problem, as the aim
     * is to avoid getting a different layout and resources than needed at
     * render time.
     * @param application The application to use for getting resources.
     * @param convertToDp Whether the value should be converted to dp from pixels.
     * @return The estimated prerender size in pixels or dp.
     */
    public static Point estimateContentSize(Application application, boolean convertToDp) {
        // The size is estimated as:
        // X = screenSizeX
        // Y = screenSizeY - top bar - bottom bar - custom tabs bar
        Point screenSize = new Point();
        WindowManager wm = (WindowManager) application.getSystemService(Context.WINDOW_SERVICE);
        wm.getDefaultDisplay().getSize(screenSize);
        Resources resources = application.getResources();
        int statusBarId = resources.getIdentifier("status_bar_height", "dimen", "android");
        try {
            screenSize.y -=
                    resources.getDimensionPixelSize(R.dimen.custom_tabs_control_container_height);
            screenSize.y -= resources.getDimensionPixelSize(statusBarId);
        } catch (Resources.NotFoundException e) {
            // Nothing, this is just a best effort estimate.
        }

        if (convertToDp) {
            float density = resources.getDisplayMetrics().density;
            screenSize.x = (int) Math.ceil(screenSize.x / density);
            screenSize.y = (int) Math.ceil(screenSize.y / density);
        }
        return screenSize;
    }

    private static native long nativeInit();
    private static native boolean nativeAddPrerender(
            long nativeExternalPrerenderHandlerAndroid, Profile profile,
            WebContents webContents, String url, String referrer,
            int top, int left, int bottom, int right, boolean prerenderOnCellular);
    private static native boolean nativeHasPrerenderedUrl(
            Profile profile, String url, WebContents webContents);
    private static native boolean nativeHasPrerenderedAndFinishedLoadingUrl(
            Profile profile, String url, WebContents webContents);
    private static native void nativeCancelCurrentPrerender(
            long nativeExternalPrerenderHandlerAndroid);
}
