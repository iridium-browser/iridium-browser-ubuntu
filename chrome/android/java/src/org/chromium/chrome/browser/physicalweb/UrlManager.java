// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.app.AlarmManager;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.AsyncTask;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.support.v4.app.NotificationCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationManagerProxy;
import org.chromium.chrome.browser.notifications.NotificationManagerProxyImpl;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.PriorityQueue;
import java.util.Set;

/**
 * This class stores URLs which are discovered by scanning for Physical Web beacons, and updates a
 * Notification as the set changes.
 *
 * There are two sets of URLs maintained:
 * - Those which are currently nearby, as tracked by calls to addUrl/removeUrl
 * - Those which have ever resolved through the Physical Web Service (e.g. are known to produce
 *     good results).
 *
 * Whenever either list changes, we update the Physical Web Notification, based on the intersection
 * of currently-nearby and known-resolved URLs.
 */
class UrlManager {
    private static final String TAG = "PhysicalWeb";
    private static final String PREFS_VERSION_KEY = "physicalweb_version";
    private static final String PREFS_ALL_URLS_KEY = "physicalweb_all_urls";
    private static final String PREFS_NEARBY_URLS_KEY = "physicalweb_nearby_urls";
    private static final String PREFS_RESOLVED_URLS_KEY = "physicalweb_resolved_urls";
    private static final String PREFS_NOTIFICATION_UPDATE_TIMESTAMP =
            "physicalweb_notification_update_timestamp";
    private static final int PREFS_VERSION = 3;
    private static final long STALE_NOTIFICATION_TIMEOUT_MILLIS = 30 * 60 * 1000;  // 30 Minutes
    private static final long MAX_CACHE_TIME = 24 * 60 * 60 * 1000;  // 1 Day
    private static final int MAX_CACHE_SIZE = 100;
    private static UrlManager sInstance = null;
    private final Context mContext;
    private final ObserverList<Listener> mObservers;
    private final Set<String> mNearbyUrls;
    private final Set<String> mResolvedUrls;
    private final Map<String, UrlInfo> mUrlInfoMap;
    private final PriorityQueue<String> mUrlsSortedByTimestamp;
    private NotificationManagerProxy mNotificationManager;
    private PwsClient mPwsClient;

    /**
     * Interface for observers that should be notified when the nearby URL list changes.
     */
    public interface Listener {
        /**
         * Callback called when one or more URLs are added to the URL list.
         * @param urls A set of UrlInfos containing nearby URLs resolvable with our resolution
         * service.
         */
        void onDisplayableUrlsAdded(Collection<UrlInfo> urls);
    }

    /**
     * Construct the UrlManager.
     * @param context An instance of android.content.Context
     */
    @VisibleForTesting
    public UrlManager(Context context) {
        mContext = context;
        mNotificationManager = new NotificationManagerProxyImpl(
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE));
        mPwsClient = new PwsClientImpl(context);
        mObservers = new ObserverList<Listener>();
        mNearbyUrls = new HashSet<>();
        mResolvedUrls = new HashSet<>();
        mUrlInfoMap = new HashMap<>();
        mUrlsSortedByTimestamp = new PriorityQueue<String>(1, new Comparator<String>() {
            @Override
            public int compare(String url1, String url2) {
                Long scanTimestamp1 = Long.valueOf(mUrlInfoMap.get(url1).getScanTimestamp());
                Long scanTimestamp2 = Long.valueOf(mUrlInfoMap.get(url2).getScanTimestamp());
                return scanTimestamp1.compareTo(scanTimestamp2);
            }
        });
        initSharedPreferences();
    }

    /**
     * Get a singleton instance of this class.
     * @return A singleton instance of this class.
     */
    public static UrlManager getInstance() {
        if (sInstance == null) {
            sInstance = new UrlManager(ContextUtils.getApplicationContext());
        }
        return sInstance;
    }

    /**
     * Get a singleton instance of this class.
     * @param context unused
     * @return A singleton instance of this class.
     */
    public static UrlManager getInstance(Context context) {
        return getInstance();
    }

    /**
     * Add an observer to be notified on changes to the nearby URL list.
     * @param observer The observer to add.
     */
    public void addObserver(Listener observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Remove an observer from the observer list.
     * @param observer The observer to remove.
     */
    public void removeObserver(Listener observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Add a URL to the store of URLs.
     * This method additionally updates the Physical Web notification.
     * @param urlInfo The URL to add.
     */
    @VisibleForTesting
    public void addUrl(UrlInfo urlInfo) {
        Log.d(TAG, "URL found: %s", urlInfo);
        urlInfo = updateCacheEntry(urlInfo);
        garbageCollect();
        putCachedUrlInfoMap();

        recordUpdate();
        if (mNearbyUrls.contains(urlInfo.getUrl())
                // In the rare event that our entry is immediately garbage collected from the cache,
                // we should stop here.
                || !mUrlInfoMap.containsKey(urlInfo.getUrl())) {
            return;
        }
        mNearbyUrls.add(urlInfo.getUrl());
        putCachedNearbyUrls();

        if (!PhysicalWeb.isOnboarding() && !mResolvedUrls.contains(urlInfo.getUrl())) {
            // We need to resolve the URL.
            resolveUrl(urlInfo);
            return;
        }
        registerNewDisplayableUrl(urlInfo);
    }

    /**
     * Remove a URL to the store of URLs.
     * This method additionally updates the Physical Web notification.
     * @param urlInfo The URL to remove.
     */
    @VisibleForTesting
    public void removeUrl(UrlInfo urlInfo) {
        Log.d(TAG, "URL lost: %s", urlInfo);
        recordUpdate();
        mNearbyUrls.remove(urlInfo.getUrl());
        putCachedNearbyUrls();

        // If there are no URLs nearby to display, clear the notification.
        if (getUrls(PhysicalWeb.isOnboarding()).isEmpty()) {
            clearNotification();
        }
    }

    /**
     * Get the list of URLs which are both nearby and resolved through PWS.
     * @return A set of nearby and resolved URLs, sorted by distance.
     */
    @VisibleForTesting
    public List<UrlInfo> getUrls() {
        return getUrls(false);
    }

    /**
     * Get the list of URLs which are both nearby and resolved through PWS.
     * @param allowUnresolved If true, include unresolved URLs only if the
     * resolved URL list is empty.
     * @return A set of nearby URLs, sorted by distance.
     */
    @VisibleForTesting
    public List<UrlInfo> getUrls(boolean allowUnresolved) {
        Set<String> intersection = new HashSet<>(mNearbyUrls);
        intersection.retainAll(mResolvedUrls);
        Log.d(TAG, "Get URLs With: %d nearby, %d resolved, and %d in intersection.",
                mNearbyUrls.size(), mResolvedUrls.size(), intersection.size());

        List<UrlInfo> urlInfos = null;
        if (allowUnresolved && mResolvedUrls.isEmpty()) {
            urlInfos = getUrlInfoList(mNearbyUrls);
        } else {
            urlInfos = getUrlInfoList(intersection);
        }
        Collections.sort(urlInfos, new Comparator<UrlInfo>() {
            @Override
            public int compare(UrlInfo urlInfo1, UrlInfo urlInfo2) {
                Double distance1 = Double.valueOf(urlInfo1.getDistance());
                Double distance2 = Double.valueOf(urlInfo2.getDistance());
                return distance1.compareTo(distance2);
            }
        });
        return urlInfos;
    }

    public UrlInfo getUrlInfoByUrl(String url) {
        return mUrlInfoMap.get(url);
    }

    public Set<String> getNearbyUrls() {
        return mNearbyUrls;
    }

    public Set<String> getResolvedUrls() {
        return mResolvedUrls;
    }

    /**
     * Forget all stored URLs and clear the notification.
     */
    public void clearAllUrls() {
        clearNearbyUrls();
        mResolvedUrls.clear();
        mUrlsSortedByTimestamp.clear();
        mUrlInfoMap.clear();
        putCachedResolvedUrls();
        putCachedUrlInfoMap();
    }

    /**
     * Forget all nearby URLs and clear the notification.
     */
    public void clearNearbyUrls() {
        mNearbyUrls.clear();
        putCachedNearbyUrls();
        clearNotification();
        cancelClearNotificationAlarm();
    }

    /**
     * Clear the URLManager's notification.
     * Typically, this should not be called except when we want to clear the notification without
     * modifying the list of URLs, as is the case when we want to remove stale notifications.
     */
    public void clearNotification() {
        mNotificationManager.cancel(NotificationConstants.NOTIFICATION_ID_PHYSICAL_WEB);
        cancelClearNotificationAlarm();
    }

    private List<UrlInfo> getUrlInfoList(Set<String> urls) {
        List<UrlInfo> result = new ArrayList<>();
        for (String url : urls) {
            result.add(mUrlInfoMap.get(url));
        }
        return result;
    }

    /**
     * Adds a URL that has been resolved by the PWS.
     * @param urlInfo This needs to be a UrlInfo that exists in mUrlInfoMap
     */
    private void addResolvedUrl(UrlInfo urlInfo) {
        Log.d(TAG, "PWS resolved: %s", urlInfo.getUrl());
        if (mResolvedUrls.contains(urlInfo.getUrl())) {
            return;
        }

        mResolvedUrls.add(urlInfo.getUrl());
        putCachedResolvedUrls();

        if (!mNearbyUrls.contains(urlInfo.getUrl())) {
            return;
        }
        registerNewDisplayableUrl(urlInfo);
    }

    private void removeResolvedUrl(UrlInfo url) {
        Log.d(TAG, "PWS unresolved: %s", url);
        mResolvedUrls.remove(url.getUrl());
        putCachedResolvedUrls();

        // If there are no URLs nearby to display, clear the notification.
        if (getUrls(PhysicalWeb.isOnboarding()).isEmpty()) {
            clearNotification();
        }
    }

    private void initSharedPreferences() {
        // Check the version.
        final SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        if (prefs.getInt(PREFS_VERSION_KEY, 0) != PREFS_VERSION) {
            new AsyncTask<Void, Void, Void>() {
                @Override
                protected Void doInBackground(Void... params) {
                    prefs.edit()
                            .putInt(PREFS_VERSION_KEY, PREFS_VERSION)
                            .apply();
                    return null;
                }
            }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
            return;
        }

        // Read the cache.
        mNearbyUrls.addAll(prefs.getStringSet(PREFS_NEARBY_URLS_KEY, new HashSet<String>()));
        mResolvedUrls.addAll(
                prefs.getStringSet(PREFS_RESOLVED_URLS_KEY, new HashSet<String>()));
        for (String serializedUrl : prefs.getStringSet(PREFS_ALL_URLS_KEY, new HashSet<String>())) {
            try {
                JSONObject jsonObject = new JSONObject(serializedUrl);
                UrlInfo urlInfo = UrlInfo.jsonDeserialize(jsonObject);
                mUrlInfoMap.put(urlInfo.getUrl(), urlInfo);
                mUrlsSortedByTimestamp.add(urlInfo.getUrl());
            } catch (JSONException e) {
                Log.e(TAG, "Could not deserialize UrlInfo", e);
            }
        }
        garbageCollect();
    }

    private void setUrlInfoCollectionInSharedPreferences(
            String preferenceName, Collection<UrlInfo> urls) {
        Set<String> serializedUrls = new HashSet<>();
        for (UrlInfo url : urls) {
            try {
                serializedUrls.add(url.jsonSerialize().toString());
            } catch (JSONException e) {
                Log.e(TAG, "Could not serialize UrlInfo", e);
            }
        }

        SharedPreferences.Editor editor = ContextUtils.getAppSharedPreferences().edit();
        editor.putStringSet(preferenceName, serializedUrls);
        editor.apply();
    }

    private void setStringSetInSharedPreferences(String preferenceName, Set<String> urls) {
        ContextUtils.getAppSharedPreferences().edit()
                .putStringSet(preferenceName, urls)
                .apply();
    }

    private void putCachedUrlInfoMap() {
        setUrlInfoCollectionInSharedPreferences(PREFS_ALL_URLS_KEY, mUrlInfoMap.values());
    }

    private void putCachedNearbyUrls() {
        setStringSetInSharedPreferences(PREFS_NEARBY_URLS_KEY, mNearbyUrls);
    }

    private void putCachedResolvedUrls() {
        setStringSetInSharedPreferences(PREFS_RESOLVED_URLS_KEY, mResolvedUrls);
    }

    private PendingIntent createListUrlsIntent() {
        Intent intent = new Intent(mContext, ListUrlsActivity.class);
        intent.putExtra(ListUrlsActivity.REFERER_KEY, ListUrlsActivity.NOTIFICATION_REFERER);
        PendingIntent pendingIntent = PendingIntent.getActivity(mContext, 0, intent, 0);
        return pendingIntent;
    }

    private PendingIntent createOptInIntent() {
        Intent intent = new Intent(mContext, PhysicalWebOptInActivity.class);
        PendingIntent pendingIntent = PendingIntent.getActivity(mContext, 0, intent, 0);
        return pendingIntent;
    }

    /**
     * Updates a cache entry with new information.
     * When we reencounter a URL, a subset of its metadata should update.  Only distance and
     * scanTimestamp fall into this category.
     * @param urlInfo This should be a freshly discovered UrlInfo, though it does not have to be
     * previously undiscovered.
     * @return The updated cache entry
     */
    private UrlInfo updateCacheEntry(UrlInfo urlInfo) {
        UrlInfo currentUrlInfo = mUrlInfoMap.get(urlInfo.getUrl());
        if (currentUrlInfo == null) {
            mUrlInfoMap.put(urlInfo.getUrl(), urlInfo);
            currentUrlInfo = urlInfo;
        } else {
            mUrlsSortedByTimestamp.remove(urlInfo.getUrl());
            currentUrlInfo.setScanTimestamp(urlInfo.getScanTimestamp());
            currentUrlInfo.setDistance(urlInfo.getDistance());
        }
        mUrlsSortedByTimestamp.add(urlInfo.getUrl());
        return currentUrlInfo;
    }

    private void resolveUrl(final UrlInfo url) {
        Set<UrlInfo> urls = new HashSet<UrlInfo>(Arrays.asList(url));
        final long timestamp = SystemClock.elapsedRealtime();
        mPwsClient.resolve(urls, new PwsClient.ResolveScanCallback() {
            @Override
            public void onPwsResults(final Collection<PwsResult> pwsResults) {
                long duration = SystemClock.elapsedRealtime() - timestamp;
                PhysicalWebUma.onBackgroundPwsResolution(mContext, duration);
                new Handler(Looper.getMainLooper()).post(new Runnable() {
                    @Override
                    public void run() {
                        for (PwsResult pwsResult : pwsResults) {
                            String requestUrl = pwsResult.requestUrl;
                            if (url.getUrl().equalsIgnoreCase(requestUrl)) {
                                addResolvedUrl(url);
                                return;
                            }
                        }
                        removeResolvedUrl(url);
                    }
                });
            }
        });
    }

    /**
     * Gets the time since the last notification update.
     * @return the elapsed realtime since the most recent notification update.
     */
    public long getTimeSinceNotificationUpdate() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        long timestamp = prefs.getLong(PREFS_NOTIFICATION_UPDATE_TIMESTAMP, 0);
        return SystemClock.elapsedRealtime() - timestamp;
    }

    private void recordUpdate() {
        // Record a timestamp.
        // This is useful for tracking whether a notification is pressed soon after an update or
        // much later.
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = prefs.edit();
        editor.putLong(PREFS_NOTIFICATION_UPDATE_TIMESTAMP, SystemClock.elapsedRealtime());
        editor.apply();
    }

    private void showNotification() {
        // We should only show notifications if there's no other notification-based client.
        if (!PhysicalWeb.shouldIgnoreOtherClients()
                && PhysicalWebEnvironment
                        .getInstance((ChromeApplication) mContext.getApplicationContext())
                        .hasNotificationBasedClient()) {
            return;
        }

        if (PhysicalWeb.isOnboarding()) {
            if (PhysicalWeb.getOptInNotifyCount() < PhysicalWeb.OPTIN_NOTIFY_MAX_TRIES) {
                // high priority notification
                createOptInNotification(true);
                PhysicalWeb.recordOptInNotification();
                PhysicalWebUma.onOptInHighPriorityNotificationShown(mContext);
            } else {
                // min priority notification
                createOptInNotification(false);
                PhysicalWebUma.onOptInMinPriorityNotificationShown(mContext);
            }
        } else if (PhysicalWeb.isPhysicalWebPreferenceEnabled()) {
            createNotification();
        }
    }

    private void createNotification() {
        PendingIntent pendingIntent = createListUrlsIntent();

        // Get values to display.
        Resources resources = mContext.getResources();
        String title = resources.getString(R.string.physical_web_notification_title);
        Bitmap largeIcon = BitmapFactory.decodeResource(resources,
                R.drawable.physical_web_notification_large);

        // Create the notification.
        Notification notification = new NotificationCompat.Builder(mContext)
                .setLargeIcon(largeIcon)
                .setSmallIcon(R.drawable.ic_chrome)
                .setContentTitle(title)
                .setContentIntent(pendingIntent)
                .setPriority(NotificationCompat.PRIORITY_MIN)
                .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
                .setLocalOnly(true)
                .build();
        mNotificationManager.notify(NotificationConstants.NOTIFICATION_ID_PHYSICAL_WEB,
                                    notification);
    }

    private void createOptInNotification(boolean highPriority) {
        PendingIntent pendingIntent = createOptInIntent();

        int priority = highPriority ? NotificationCompat.PRIORITY_HIGH
                : NotificationCompat.PRIORITY_MIN;

        // Get values to display.
        Resources resources = mContext.getResources();
        String title = resources.getString(R.string.physical_web_optin_notification_title);
        String text = resources.getString(R.string.physical_web_optin_notification_text);
        Bitmap largeIcon = BitmapFactory.decodeResource(resources, R.mipmap.app_icon);

        // Create the notification.
        Notification notification = new NotificationCompat.Builder(mContext)
                .setLargeIcon(largeIcon)
                .setSmallIcon(R.drawable.ic_physical_web_notification)
                .setContentTitle(title)
                .setContentText(text)
                .setContentIntent(pendingIntent)
                .setPriority(priority)
                .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
                .setAutoCancel(true)
                .setLocalOnly(true)
                .build();
        mNotificationManager.notify(NotificationConstants.NOTIFICATION_ID_PHYSICAL_WEB,
                                    notification);
    }

    private PendingIntent createClearNotificationAlarmIntent() {
        Intent intent = new Intent(mContext, ClearNotificationAlarmReceiver.class);
        return PendingIntent.getBroadcast(mContext, 0, intent, PendingIntent.FLAG_CANCEL_CURRENT);
    }

    private void scheduleClearNotificationAlarm() {
        PendingIntent pendingIntent = createClearNotificationAlarmIntent();
        AlarmManager alarmManager = (AlarmManager) mContext.getSystemService(Context.ALARM_SERVICE);
        long time = SystemClock.elapsedRealtime() + STALE_NOTIFICATION_TIMEOUT_MILLIS;
        alarmManager.set(AlarmManager.ELAPSED_REALTIME, time, pendingIntent);
    }

    private void cancelClearNotificationAlarm() {
        PendingIntent pendingIntent = createClearNotificationAlarmIntent();
        AlarmManager alarmManager = (AlarmManager) mContext.getSystemService(Context.ALARM_SERVICE);
        alarmManager.cancel(pendingIntent);
    }

    private void registerNewDisplayableUrl(UrlInfo urlInfo) {
        // Notify listeners about the new displayable URL.
        Collection<UrlInfo> urlInfos = new ArrayList<>();
        urlInfos.add(urlInfo);
        Collection<UrlInfo> wrappedUrlInfos = Collections.unmodifiableCollection(urlInfos);
        for (Listener observer : mObservers) {
            observer.onDisplayableUrlsAdded(wrappedUrlInfos);
        }

        // Only trigger the notification if we know we didn't have a notification up already
        // (i.e., we have exactly 1 displayble URL) or this URL doesn't exist in the cache
        // (and hence the user hasn't swiped away a notification for this URL recently).
        if (getUrls(PhysicalWeb.isOnboarding()).size() != 1
                && urlInfo.hasBeenDisplayed()) {
            return;
        }

        // Show a notification and mark the URL as displayed.
        showNotification();
        urlInfo.setHasBeenDisplayed();
    }

    private void garbageCollect() {
        for (String url = mUrlsSortedByTimestamp.peek(); url != null;
                url = mUrlsSortedByTimestamp.peek()) {
            UrlInfo urlInfo = mUrlInfoMap.get(url);
            if ((System.currentTimeMillis() - urlInfo.getScanTimestamp() <= MAX_CACHE_TIME
                    && mUrlsSortedByTimestamp.size() <= MAX_CACHE_SIZE)
                    || mNearbyUrls.contains(url)) {
                Log.d(TAG, "Not garbage collecting: ", urlInfo);
                break;
            }
            Log.d(TAG, "Garbage collecting: ", urlInfo);
            // The min value cannot have changed at this point, so it's OK to just remove via
            // poll().
            mUrlsSortedByTimestamp.poll();
            mUrlInfoMap.remove(url);
            mResolvedUrls.remove(url);
        }
    }

    @VisibleForTesting
    void overridePwsClientForTesting(PwsClient pwsClient) {
        mPwsClient = pwsClient;
    }

    @VisibleForTesting
    void overrideNotificationManagerForTesting(
            NotificationManagerProxy notificationManager) {
        mNotificationManager = notificationManager;
    }

    @VisibleForTesting
    static void clearPrefsForTesting(Context context) {
        ContextUtils.getAppSharedPreferences().edit()
                .remove(PREFS_VERSION_KEY)
                .remove(PREFS_NEARBY_URLS_KEY)
                .remove(PREFS_RESOLVED_URLS_KEY)
                .remove(PREFS_NOTIFICATION_UPDATE_TIMESTAMP)
                .apply();
    }

    @VisibleForTesting
    static String getVersionKey() {
        return PREFS_VERSION_KEY;
    }

    @VisibleForTesting
    static int getVersion() {
        return PREFS_VERSION;
    }

    @VisibleForTesting
    boolean containsInAnyCache(String url) {
        return mNearbyUrls.contains(url)
                || mResolvedUrls.contains(url)
                || mUrlInfoMap.containsKey(url)
                || mUrlsSortedByTimestamp.contains(url);
    }

    @VisibleForTesting
    int getMaxCacheSize() {
        return MAX_CACHE_SIZE;
    }
}
