// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.os.AsyncTask;

import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.content_public.common.ScreenOrientationValues;

import java.util.Map;
import java.util.concurrent.TimeUnit;

/**
 * Stores data about an installed web app. Uses SharedPreferences to persist the data to disk.
 * This class must only be accessed via {@link WebappRegistry}, which is used to register and keep
 * track of web app data known to Chrome.
 */
public class WebappDataStorage {

    static final String SHARED_PREFS_FILE_PREFIX = "webapp_";
    static final String KEY_SPLASH_ICON = "splash_icon";
    static final String KEY_LAST_USED = "last_used";
    static final String KEY_URL = "url";
    static final String KEY_SCOPE = "scope";
    static final String KEY_ICON = "icon";
    static final String KEY_NAME = "name";
    static final String KEY_SHORT_NAME = "short_name";
    static final String KEY_DISPLAY_MODE = "display_mode";
    static final String KEY_ORIENTATION = "orientation";
    static final String KEY_THEME_COLOR = "theme_color";
    static final String KEY_BACKGROUND_COLOR = "background_color";
    static final String KEY_SOURCE = "source";
    static final String KEY_ACTION = "action";
    static final String KEY_IS_ICON_GENERATED = "is_icon_generated";
    static final String KEY_VERSION = "version";
    static final String KEY_WEBAPK_PACKAGE_NAME = "webapk_package_name";

    // The last time that Chrome checked for Web Manifest updates for a WebAPK.
    static final String KEY_LAST_CHECK_WEB_MANIFEST_UPDATE_TIME =
            "last_check_web_manifest_update_time";

    // The last time that the WebAPK update request completed (successfully or
    // unsuccessfully).
    static final String KEY_LAST_WEBAPK_UPDATE_REQUEST_COMPLETE_TIME =
            "last_webapk_update_request_complete_time";

    // Whether the last WebAPK update request succeeded.
    static final String KEY_DID_LAST_WEBAPK_UPDATE_REQUEST_SUCCEED =
            "did_last_webapk_update_request_succeed";

    // Unset/invalid constants for last used times and URLs. 0 is used as the null last
    // used time as WebappRegistry assumes that this is always a valid timestamp.
    static final long LAST_USED_UNSET = 0;
    static final long LAST_USED_INVALID = -1;
    static final String URL_INVALID = "";
    static final int VERSION_INVALID = 0;

    // We use a heuristic to determine whether a web app is still installed on the home screen, as
    // there is no way to do so directly. Any web app which has been opened in the last ten days
    // is considered to be still on the home screen.
    static final long WEBAPP_LAST_OPEN_MAX_TIME = TimeUnit.DAYS.toMillis(10L);

    private static Clock sClock = new Clock();
    private static Factory sFactory = new Factory();

    private final String mId;
    private final SharedPreferences mPreferences;

    /**
     * Opens an instance of WebappDataStorage for the web app specified. Must not be run on the UI
     * thread.
     * @param context  The context to open the SharedPreferences.
     * @param webappId The ID of the web app which is being opened.
     */
    static WebappDataStorage open(final Context context, final String webappId) {
        final WebappDataStorage storage = sFactory.create(context, webappId);
        if (storage.getLastUsedTime() == LAST_USED_INVALID) {
            // If the last used time is invalid then ensure that there is no data in the
            // WebappDataStorage which needs to be cleaned up.
            assert storage.getAllData().isEmpty();
        }
        return storage;
    }

    /**
     * Asynchronously retrieves the time which this WebappDataStorage was last opened. Used in
     * testing.
     * @param context  The context to read the SharedPreferences file.
     * @param webappId The ID of the web app the used time is being read for.
     * @param callback Called when the last used time has been retrieved.
     */
    @VisibleForTesting
    public static void getLastUsedTime(final Context context, final String webappId,
            final FetchCallback<Long> callback) {
        new AsyncTask<Void, Void, Long>() {
            @Override
            protected final Long doInBackground(Void... nothing) {
                long lastUsed = new WebappDataStorage(context.getApplicationContext(), webappId)
                        .getLastUsedTime();
                assert lastUsed != LAST_USED_INVALID;
                return lastUsed;
            }

            @Override
            protected final void onPostExecute(Long lastUsed) {
                assert callback != null;
                callback.onDataRetrieved(lastUsed);
            }
        }.execute();
    }

    /**
     * Asynchronously retrieves the scope stored in this WebappDataStorage. The scope is the URL
     * over which the web app data is applied to. Used in testing.
     * @param context  The context to read the SharedPreferences file.
     * @param webappId The ID of the web app the used time is being read for.
     * @param callback Called when the scope has been retrieved.
     */
    @VisibleForTesting
    public static void getScope(final Context context, final String webappId,
            final FetchCallback<String> callback) {
        new AsyncTask<Void, Void, String>() {
            @Override
            protected final String doInBackground(Void... nothing) {
                return new WebappDataStorage(context.getApplicationContext(), webappId).getScope();
            }

            @Override
            protected final void onPostExecute(String scope) {
                assert callback != null;
                callback.onDataRetrieved(scope);
            }
        }.execute();
    }

    /**
     * Asynchronously retrieves the URL stored in this WebappDataStorage. Used in testing.
     * @param context  The context to read the SharedPreferences file.
     * @param webappId The ID of the web app the used time is being read for.
     * @param callback Called when the URL has been retrieved.
     */
    @VisibleForTesting
    public static void getUrl(final Context context, final String webappId,
            final FetchCallback<String> callback) {
        new AsyncTask<Void, Void, String>() {
            @Override
            protected final String doInBackground(Void... nothing) {
                return new WebappDataStorage(context.getApplicationContext(), webappId).getUrl();
            }

            @Override
            protected final void onPostExecute(String url) {
                assert callback != null;
                callback.onDataRetrieved(url);
            }
        }.execute();
    }

    /**
     * Deletes the data for a web app by clearing all the information inside the SharedPreferences
     * file. This does NOT delete the file itself but the file is left empty.
     * @param context  The context to read the SharedPreferences file.
     * @param webappId The ID of the web app being deleted.
     */
    static void deleteDataForWebapp(final Context context, final String webappId) {
        assert !ThreadUtils.runningOnUiThread();
        openSharedPreferences(context, webappId).edit().clear().apply();
    }

    /**
     * Deletes the URL and scope, and sets all timestamps to 0 in SharedPreferences.
     * This does not remove the stored splash screen image (if any) for the app.
     * @param context  The context to read the SharedPreferences file.
     * @param webappId The ID of the web app for which history is being cleared.
     */
    static void clearHistory(final Context context, final String webappId) {
        assert !ThreadUtils.runningOnUiThread();
        SharedPreferences.Editor editor = openSharedPreferences(context, webappId).edit();

        // The last used time is set to 0 to ensure that a valid value is always present.
        // If the web app is not launched prior to the next cleanup, then its remaining data will be
        // removed. Otherwise, the next launch from home screen will update the last used time.
        editor.putLong(KEY_LAST_USED, LAST_USED_UNSET);
        editor.remove(KEY_URL);
        editor.remove(KEY_SCOPE);
        editor.remove(KEY_LAST_CHECK_WEB_MANIFEST_UPDATE_TIME);
        editor.remove(KEY_LAST_WEBAPK_UPDATE_REQUEST_COMPLETE_TIME);
        editor.remove(KEY_DID_LAST_WEBAPK_UPDATE_REQUEST_SUCCEED);
        editor.apply();
    }

    /**
     * Sets the clock used to get the current time.
     */
    @VisibleForTesting
    public static void setClockForTests(Clock clock) {
        sClock = clock;
    }

    /**
     * Sets the factory used to generate WebappDataStorage objects.
     */
    @VisibleForTesting
    public static void setFactoryForTests(Factory factory) {
        sFactory = factory;
    }

    private static SharedPreferences openSharedPreferences(Context context, String webappId) {
        return context.getApplicationContext().getSharedPreferences(
                SHARED_PREFS_FILE_PREFIX + webappId, Context.MODE_PRIVATE);
    }

    protected WebappDataStorage(Context context, String webappId) {
        mId = webappId;
        mPreferences = openSharedPreferences(context, webappId);
    }

    /**
     * Asynchronously retrieves the splash screen image associated with the current web app.
     * @param callback Called when the splash screen image has been retrieved.
     *                 The bitmap result may be null if no image was found.
     */
    public void getSplashScreenImage(final FetchCallback<Bitmap> callback) {
        new AsyncTask<Void, Void, Bitmap>() {
            @Override
            protected final Bitmap doInBackground(Void... nothing) {
                return ShortcutHelper.decodeBitmapFromString(
                        mPreferences.getString(KEY_SPLASH_ICON, null));
            }

            @Override
            protected final void onPostExecute(Bitmap result) {
                assert callback != null;
                callback.onDataRetrieved(result);
            }
        }.execute();
    }

    /**
     * Update the information associated with the web app with the specified data.
     * @param splashScreenImage The image which should be shown on the splash screen of the web app.
     */
    public void updateSplashScreenImage(final Bitmap splashScreenImage) {
        // Use an AsyncTask as this method is invoked on the UI thread from the callbacks leading to
        // ShortcutHelper.storeWebappSplashImage.
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected final Void doInBackground(Void... nothing) {
                String bitmap = ShortcutHelper.encodeBitmapAsString(splashScreenImage);
                mPreferences.edit().putString(KEY_SPLASH_ICON, bitmap).apply();
                return null;
            }
        }.execute();
    }

    /**
     * Creates and returns a web app launch intent from the data stored in this object. Must not be
     * called on the UI thread as a Bitmap is decoded from a String (a potentially expensive
     * operation).
     * @return The web app launch intent.
     */
    public Intent createWebappLaunchIntent() {
        assert !ThreadUtils.runningOnUiThread();
        // Assume that all of the data is invalid if the version isn't set, so return a null intent.
        int version = mPreferences.getInt(KEY_VERSION, VERSION_INVALID);
        if (version == VERSION_INVALID) return null;

        // Use "standalone" as the default display mode as this was the original assumed default for
        // all web apps.
        return ShortcutHelper.createWebappShortcutIntent(mId,
                mPreferences.getString(KEY_ACTION, null),
                mPreferences.getString(KEY_URL, null),
                mPreferences.getString(KEY_SCOPE, null),
                mPreferences.getString(KEY_NAME, null),
                mPreferences.getString(KEY_SHORT_NAME, null),
                ShortcutHelper.decodeBitmapFromString(
                        mPreferences.getString(KEY_ICON, null)), version,
                mPreferences.getInt(KEY_DISPLAY_MODE, WebDisplayMode.Standalone),
                mPreferences.getInt(KEY_ORIENTATION, ScreenOrientationValues.DEFAULT),
                mPreferences.getLong(KEY_THEME_COLOR,
                        ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING),
                mPreferences.getLong(KEY_BACKGROUND_COLOR,
                        ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING),
                mPreferences.getBoolean(KEY_IS_ICON_GENERATED, false));
    }

    /**
     * Updates the data stored in this object to match that in the supplied intent.
     * @param shortcutIntent The intent to pull web app data from.
     */
    public void updateFromShortcutIntent(Intent shortcutIntent) {
        if (shortcutIntent == null) return;

        SharedPreferences.Editor editor = mPreferences.edit();
        boolean updated = false;

        // The URL and scope may have been deleted by the user clearing their history. Check whether
        // they are present, and update if necessary.
        String url = mPreferences.getString(KEY_URL, URL_INVALID);
        if (url.equals(URL_INVALID)) {
            url = IntentUtils.safeGetStringExtra(shortcutIntent, ShortcutHelper.EXTRA_URL);
            editor.putString(KEY_URL, url);
            updated = true;
        }

        if (mPreferences.getString(KEY_SCOPE, URL_INVALID).equals(URL_INVALID)) {
            String scope = IntentUtils.safeGetStringExtra(
                    shortcutIntent, ShortcutHelper.EXTRA_SCOPE);
            if (scope == null) {
                scope = ShortcutHelper.getScopeFromUrl(url);
            }
            editor.putString(KEY_SCOPE, scope);
            updated = true;
        }

        // For all other fields, assume that if the version key is present and equal to
        // ShortcutHelper.WEBAPP_SHORTCUT_VERSION, then all fields are present and do not need to be
        // updated. All fields except for the last used time, scope, and URL are either set or
        // cleared together.
        if (mPreferences.getInt(KEY_VERSION, VERSION_INVALID)
                != ShortcutHelper.WEBAPP_SHORTCUT_VERSION) {
            editor.putString(KEY_NAME, IntentUtils.safeGetStringExtra(
                        shortcutIntent, ShortcutHelper.EXTRA_NAME));
            editor.putString(KEY_SHORT_NAME, IntentUtils.safeGetStringExtra(
                        shortcutIntent, ShortcutHelper.EXTRA_SHORT_NAME));
            editor.putString(KEY_ICON, IntentUtils.safeGetStringExtra(
                        shortcutIntent, ShortcutHelper.EXTRA_ICON));
            editor.putInt(KEY_VERSION, ShortcutHelper.WEBAPP_SHORTCUT_VERSION);

            // "Standalone" was the original assumed default for all web apps.
            editor.putInt(KEY_DISPLAY_MODE, IntentUtils.safeGetIntExtra(
                        shortcutIntent, ShortcutHelper.EXTRA_DISPLAY_MODE,
                        WebDisplayMode.Standalone));
            editor.putInt(KEY_ORIENTATION, IntentUtils.safeGetIntExtra(
                        shortcutIntent, ShortcutHelper.EXTRA_ORIENTATION,
                        ScreenOrientationValues.DEFAULT));
            editor.putLong(KEY_THEME_COLOR, IntentUtils.safeGetLongExtra(
                        shortcutIntent, ShortcutHelper.EXTRA_THEME_COLOR,
                        ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING));
            editor.putLong(KEY_BACKGROUND_COLOR, IntentUtils.safeGetLongExtra(
                        shortcutIntent, ShortcutHelper.EXTRA_BACKGROUND_COLOR,
                        ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING));
            editor.putBoolean(KEY_IS_ICON_GENERATED, IntentUtils.safeGetBooleanExtra(
                        shortcutIntent, ShortcutHelper.EXTRA_IS_ICON_GENERATED, false));
            editor.putString(KEY_ACTION, shortcutIntent.getAction());
            editor.putInt(KEY_SOURCE, IntentUtils.safeGetIntExtra(
                        shortcutIntent, ShortcutHelper.EXTRA_SOURCE,
                        ShortcutSource.UNKNOWN));
            editor.putString(KEY_WEBAPK_PACKAGE_NAME, IntentUtils.safeGetStringExtra(
                    shortcutIntent, ShortcutHelper.EXTRA_WEBAPK_PACKAGE_NAME));
            updated = true;
        }
        if (updated) editor.apply();
    }

    /**
     * Returns the scope stored in this object, or URL_INVALID if it is not stored.
     */
    String getScope() {
        return mPreferences.getString(KEY_SCOPE, URL_INVALID);
    }

    /**
     * Returns the URL stored in this object, or URL_INVALID if it is not stored.
     */
    String getUrl() {
        return mPreferences.getString(KEY_URL, URL_INVALID);
    }

    /**
     * Returns the theme color stored in this object, or
     * ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING if it is not stored.
     */
    long getThemeColor() {
        return mPreferences.getLong(KEY_THEME_COLOR,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING);
    }

    /**
     * Returns the orientation stored in this object, or ScreenOrientationValues.DEFAULT if it is
     * not stored.
     */
    int getOrientation() {
        return mPreferences.getInt(KEY_ORIENTATION, ScreenOrientationValues.DEFAULT);
    }

    /**
     * Updates the last used time of this object.
     */
    void updateLastUsedTime() {
        mPreferences.edit().putLong(KEY_LAST_USED, sClock.currentTimeMillis()).apply();
    }

    /**
     * Returns the last used time of this object, or -1 if it is not stored.
     */
    long getLastUsedTime() {
        return mPreferences.getLong(KEY_LAST_USED, LAST_USED_INVALID);
    }

    /**
     * Returns the package name if the data is for a WebAPK, null otherwise.
     */
    String getWebApkPackageName() {
        return mPreferences.getString(KEY_WEBAPK_PACKAGE_NAME, null);
    }

    /**
     *  Updates the time of the last check for whether the WebAPK's Web Manifest was updated.
     */
    void updateTimeOfLastCheckForUpdatedWebManifest() {
        mPreferences.edit()
                .putLong(KEY_LAST_CHECK_WEB_MANIFEST_UPDATE_TIME, sClock.currentTimeMillis())
                .apply();
    }

    /**
     * Returns the time of the last check for whether the WebAPK's Web Manifest was updated.
     */
    long getLastCheckForWebManifestUpdateTime() {
        return mPreferences.getLong(KEY_LAST_CHECK_WEB_MANIFEST_UPDATE_TIME, LAST_USED_INVALID);
    }

    /**
     * Updates the time that the last WebAPK update request completed (successfully or
     * unsuccessfully).
     */
    void updateTimeOfLastWebApkUpdateRequestCompletion() {
        mPreferences.edit()
                .putLong(KEY_LAST_WEBAPK_UPDATE_REQUEST_COMPLETE_TIME, sClock.currentTimeMillis())
                .apply();
    }

    /**
     * Returns the time that the last WebAPK update request completed (successfully or
     * unsuccessfully).
     */
    long getLastWebApkUpdateRequestCompletionTime() {
        return mPreferences.getLong(
                KEY_LAST_WEBAPK_UPDATE_REQUEST_COMPLETE_TIME, LAST_USED_INVALID);
    }

    /**
     * Updates the result of whether the last update request to WebAPK Server succeeded.
     */
    void updateDidLastWebApkUpdateRequestSucceed(boolean sucess) {
        mPreferences.edit()
                .putBoolean(KEY_DID_LAST_WEBAPK_UPDATE_REQUEST_SUCCEED, sucess)
                .apply();
    }

    /**
     * Returns whether the last update request to WebAPK Server succeeded.
     */
    boolean getDidLastWebApkUpdateRequestSucceed() {
        return mPreferences.getBoolean(KEY_DID_LAST_WEBAPK_UPDATE_REQUEST_SUCCEED, false);
    }
    /**
     * Returns true if this web app has been launched from home screen recently (within
     * WEBAPP_LAST_OPEN_MAX_TIME milliseconds).
     */
    public boolean wasLaunchedRecently() {
        // Registering the web app sets the last used time, so that counts as a 'launch'.
        return (sClock.currentTimeMillis() - getLastUsedTime() < WEBAPP_LAST_OPEN_MAX_TIME);
    }

    private Map<String, ?> getAllData() {
        return mPreferences.getAll();
    }

    /**
     * Called after data has been retrieved from storage.
     */
    public interface FetchCallback<T> {
        public void onDataRetrieved(T readObject);
    }

    /**
     * Factory used to generate WebappDataStorage objects.
     *
     * It is used in tests to override methods in WebappDataStorage and inject the mocked objects.
     */
    public static class Factory {

        /**
         * Generates a WebappDataStorage class for a specified web app.
         */
        public WebappDataStorage create(final Context context, final String webappId) {
            return new WebappDataStorage(context, webappId);
        }
    }

    /**
     * Clock used to generate the current time in millseconds for updating and setting last used
     * time.
     */
    public static class Clock {
        /**
         * Returns the current time in milliseconds.
         */
        public long currentTimeMillis() {
            return System.currentTimeMillis();
        }
    }
}
