// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.content.SharedPreferences;

import org.chromium.content.browser.ContentViewStatics;
import org.chromium.net.DefaultAndroidKeyStore;

/**
 * Java side of the Browser Context: contains all the java side objects needed to host one
 * browing session (i.e. profile).
 * Note that due to running in single process mode, and limitations on renderer process only
 * being able to use a single browser context, currently there can only be one AwBrowserContext
 * instance, so at this point the class mostly exists for conceptual clarity.
 */
public class AwBrowserContext {

    private static final String HTTP_AUTH_DATABASE_FILE = "http_auth.db";

    private SharedPreferences mSharedPreferences;

    private AwGeolocationPermissions mGeolocationPermissions;
    private AwCookieManager mCookieManager;
    private AwFormDatabase mFormDatabase;
    private HttpAuthDatabase mHttpAuthDatabase;
    private DefaultAndroidKeyStore mLocalKeyStore;
    private AwMessagePortService mMessagePortService;

    public AwBrowserContext(SharedPreferences sharedPreferences) {
        mSharedPreferences = sharedPreferences;
    }

    public AwGeolocationPermissions getGeolocationPermissions() {
        if (mGeolocationPermissions == null) {
            mGeolocationPermissions = new AwGeolocationPermissions(mSharedPreferences);
        }
        return mGeolocationPermissions;
    }

    public AwCookieManager getCookieManager() {
        if (mCookieManager == null) {
            mCookieManager = new AwCookieManager();
        }
        return mCookieManager;
    }

    public AwFormDatabase getFormDatabase() {
        if (mFormDatabase == null) {
            mFormDatabase = new AwFormDatabase();
        }
        return mFormDatabase;
    }

    public HttpAuthDatabase getHttpAuthDatabase(Context context) {
        if (mHttpAuthDatabase == null) {
            mHttpAuthDatabase = HttpAuthDatabase.newInstance(context, HTTP_AUTH_DATABASE_FILE);
        }
        return mHttpAuthDatabase;
    }

    public DefaultAndroidKeyStore getKeyStore() {
        if (mLocalKeyStore == null) {
            mLocalKeyStore = new DefaultAndroidKeyStore();
        }
        return mLocalKeyStore;
    }

    public AwMessagePortService getMessagePortService() {
        if (mMessagePortService == null) {
            mMessagePortService = new AwMessagePortService();
        }
        return mMessagePortService;
    }

    /**
     * @see android.webkit.WebView#pauseTimers()
     */
    public void pauseTimers() {
        ContentViewStatics.setWebKitSharedTimersSuspended(true);
    }

    /**
     * @see android.webkit.WebView#resumeTimers()
     */
    public void resumeTimers() {
        ContentViewStatics.setWebKitSharedTimersSuspended(false);
    }
}
