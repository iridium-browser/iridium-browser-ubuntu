// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.text.TextUtils;
import android.util.Base64;
import android.util.Log;

import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.content_public.common.ScreenOrientationValues;

/**
 * Stores info about a web app.
 */
public class WebappInfo {
    private boolean mIsInitialized;
    private String mId;
    private String mEncodedIcon;
    private Bitmap mDecodedIcon;
    private Uri mUri;
    private String mName;
    private String mShortName;
    private int mOrientation;
    private int mSource;
    private long mThemeColor;
    private long mBackgroundColor;

    public static WebappInfo createEmpty() {
        return new WebappInfo();
    }

    private static String titleFromIntent(Intent intent) {
        // The reference to title has been kept for reasons of backward compatibility. For intents
        // and shortcuts which were created before we utilized the concept of name and shortName,
        // we set the name and shortName to be the title.
        String title = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_TITLE);
        return title == null ? "" : title;
    }

    public static String nameFromIntent(Intent intent) {
        String name = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_NAME);
        return name == null ? titleFromIntent(intent) : name;
    }

    public static String shortNameFromIntent(Intent intent) {
        String shortName = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_SHORT_NAME);
        return shortName == null ? titleFromIntent(intent) : shortName;
    }

    /**
     * Construct a WebappInfo.
     * @param intent Intent containing info about the app.
     */
    public static WebappInfo create(Intent intent) {
        String id = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_ID);
        String icon = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_ICON);
        String url = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_URL);
        int orientation = IntentUtils.safeGetIntExtra(intent,
                ShortcutHelper.EXTRA_ORIENTATION, ScreenOrientationValues.DEFAULT);
        int source = IntentUtils.safeGetIntExtra(intent,
                ShortcutHelper.EXTRA_SOURCE, ShortcutSource.UNKNOWN);
        long themeColor = IntentUtils.safeGetLongExtra(intent,
                ShortcutHelper.EXTRA_THEME_COLOR,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING);
        long backgroundColor = IntentUtils.safeGetLongExtra(intent,
                ShortcutHelper.EXTRA_BACKGROUND_COLOR,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING);

        String name = nameFromIntent(intent);
        String shortName = shortNameFromIntent(intent);

        return create(id, url, icon, name, shortName, orientation, source,
                themeColor, backgroundColor);
    }

    /**
     * Construct a WebappInfo.
     * @param id ID for the webapp.
     * @param url URL for the webapp.
     * @param icon Icon to show for the webapp.
     * @param name Name of the webapp.
     * @param shortName The short name of the webapp.
     * @param orientation Orientation of the webapp.
     * @param source Source where the webapp was added from.
     * @param themeColor The theme color of the webapp.
     */
    public static WebappInfo create(String id, String url, String icon, String name,
            String shortName, int orientation, int source, long themeColor,
            long backgroundColor) {
        if (id == null || url == null) {
            Log.e("WebappInfo", "Data passed in was incomplete: " + id + ", " + url);
            return null;
        }

        Uri uri = Uri.parse(url);
        return new WebappInfo(id, uri, icon, name, shortName, orientation, source,
                themeColor, backgroundColor);
    }

    private WebappInfo(String id, Uri uri, String encodedIcon, String name,
            String shortName, int orientation, int source, long themeColor,
            long backgroundColor) {
        mEncodedIcon = encodedIcon;
        mId = id;
        mName = name;
        mShortName = shortName;
        mUri = uri;
        mOrientation = orientation;
        mSource = source;
        mThemeColor = themeColor;
        mBackgroundColor = backgroundColor;
        mIsInitialized = mUri != null;
    }

    private WebappInfo() {
    }

    /**
     * Copies all the fields from the given WebappInfo into this instance.
     * @param newInfo Information about the new webapp.
     */
    void copy(WebappInfo newInfo) {
        mIsInitialized = newInfo.mIsInitialized;
        mEncodedIcon = newInfo.mEncodedIcon;
        mDecodedIcon = newInfo.mDecodedIcon;
        mId = newInfo.mId;
        mUri = newInfo.mUri;
        mName = newInfo.mName;
        mShortName = newInfo.mShortName;
        mOrientation = newInfo.mOrientation;
        mSource = newInfo.mSource;
        mThemeColor = newInfo.mThemeColor;
        mBackgroundColor = newInfo.mBackgroundColor;
    }

    public boolean isInitialized() {
        return mIsInitialized;
    }

    public String id() {
        return mId;
    }

    public Uri uri() {
        return mUri;
    }

    public String name() {
        return mName;
    }

    public String shortName() {
        return mShortName;
    }

    public int orientation() {
        return mOrientation;
    }

    public int source() {
        return mSource;
    }

    /**
     * Theme color is actually a 32 bit unsigned integer which encodes a color
     * in ARGB format. mThemeColor is a long because we also need to encode the
     * error state of ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING.
     */
    public long themeColor() {
        return mThemeColor;
    }

    /**
     * Background color is actually a 32 bit unsigned integer which encodes a color
     * in ARGB format. mBackgroundColor is a long because we also need to encode the
     * error state of ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING.
     */
    public long backgroundColor() {
        return mBackgroundColor;
    }

    // This is needed for clients that want to send the icon trough an intent.
    public String encodedIcon() {
        return mEncodedIcon;
    }

    /**
     * Returns the icon in Bitmap form.  Caches the result for future retrievals.
     */
    public Bitmap icon() {
        if (mDecodedIcon != null) return mDecodedIcon;
        if (TextUtils.isEmpty(mEncodedIcon)) return null;

        byte[] decoded = Base64.decode(mEncodedIcon, Base64.DEFAULT);
        mDecodedIcon = BitmapFactory.decodeByteArray(decoded, 0, decoded.length);
        return mDecodedIcon;
    }

    /**
     * Sets extras on an Intent that will launch a WebappActivity.
     * @param intent Intent that will be used to launch a WebappActivity.
     */
    public void setWebappIntentExtras(Intent intent) {
        intent.putExtra(ShortcutHelper.EXTRA_ID, id());
        intent.putExtra(ShortcutHelper.EXTRA_URL, uri().toString());
        intent.putExtra(ShortcutHelper.EXTRA_ICON, encodedIcon());
        intent.putExtra(ShortcutHelper.EXTRA_NAME, name());
        intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName());
        intent.putExtra(ShortcutHelper.EXTRA_ORIENTATION, orientation());
        intent.putExtra(ShortcutHelper.EXTRA_SOURCE, source());
        intent.putExtra(ShortcutHelper.EXTRA_THEME_COLOR, themeColor());
        intent.putExtra(ShortcutHelper.EXTRA_BACKGROUND_COLOR, backgroundColor());
    }
}
