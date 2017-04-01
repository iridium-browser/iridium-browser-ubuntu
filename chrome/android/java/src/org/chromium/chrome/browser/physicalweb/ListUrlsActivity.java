// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.PorterDuff;
import android.graphics.drawable.AnimationDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.SystemClock;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AppCompatActivity;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.components.location.LocationUtils;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

/**
 * This activity displays a list of nearby URLs as stored in the {@link UrlManager}.
 * This activity does not and should not rely directly or indirectly on the native library.
 */
public class ListUrlsActivity extends AppCompatActivity implements AdapterView.OnItemClickListener,
        SwipeRefreshWidget.OnRefreshListener, UrlManager.Listener {
    public static final String REFERER_KEY = "referer";
    public static final int NOTIFICATION_REFERER = 1;
    public static final int OPTIN_REFERER = 2;
    public static final int PREFERENCE_REFERER = 3;
    public static final int DIAGNOSTICS_REFERER = 4;
    public static final int REFERER_BOUNDARY = 5;
    private static final String TAG = "PhysicalWeb";

    private final List<PwsResult> mPwsResults = new ArrayList<>();

    private Context mContext;
    private NearbyUrlsAdapter mAdapter;
    private PwsClient mPwsClient;
    private ListView mListView;
    private TextView mEmptyListText;
    private ImageView mScanningImageView;
    private SwipeRefreshWidget mSwipeRefreshWidget;
    private boolean mIsInitialDisplayRecorded;
    private boolean mIsRefreshing;
    private boolean mIsRefreshUserInitiated;
    private NearbyForegroundSubscription mNearbyForegroundSubscription;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mContext = this;
        setContentView(R.layout.physical_web_list_urls_activity);

        mAdapter = new NearbyUrlsAdapter(this);

        View emptyView = findViewById(R.id.physical_web_empty);
        mListView = (ListView) findViewById(R.id.physical_web_urls_list);
        mListView.setEmptyView(emptyView);
        mListView.setAdapter(mAdapter);
        mListView.setOnItemClickListener(this);

        mEmptyListText = (TextView) findViewById(R.id.physical_web_empty_list_text);

        mScanningImageView = (ImageView) findViewById(R.id.physical_web_logo);

        mSwipeRefreshWidget =
                (SwipeRefreshWidget) findViewById(R.id.physical_web_swipe_refresh_widget);
        mSwipeRefreshWidget.setOnRefreshListener(this);

        mPwsClient = new PwsClientImpl(this);
        int referer = getIntent().getIntExtra(REFERER_KEY, 0);
        if (savedInstanceState == null) {  // Ensure this is a newly-created activity.
            PhysicalWebUma.onActivityReferral(referer);
        }
        mIsInitialDisplayRecorded = false;
        mIsRefreshing = false;
        mIsRefreshUserInitiated = false;
        mNearbyForegroundSubscription = new NearbyForegroundSubscription(this);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        int tintColor = ContextCompat.getColor(this, R.color.light_normal_color);

        Drawable tintedRefresh = ContextCompat.getDrawable(this, R.drawable.btn_toolbar_reload);
        tintedRefresh.setColorFilter(tintColor, PorterDuff.Mode.SRC_IN);
        menu.add(0, R.id.menu_id_refresh, 1, R.string.physical_web_refresh)
                .setIcon(tintedRefresh)
                .setShowAsActionFlags(MenuItem.SHOW_AS_ACTION_ALWAYS);

        menu.add(0, R.id.menu_id_close, 2, R.string.close)
                .setIcon(R.drawable.btn_close)
                .setShowAsActionFlags(MenuItem.SHOW_AS_ACTION_ALWAYS);

        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();
        if (id == R.id.menu_id_close) {
            finish();
            return true;
        } else if (id == R.id.menu_id_refresh) {
            startRefresh(true, false);
            return true;
        }

        Log.e(TAG, "Unknown menu item selected");
        return super.onOptionsItemSelected(item);
    }

    @Override
    protected void onStart() {
        super.onStart();
        UrlManager.getInstance().addObserver(this);
        // Only connect so that we can subscribe to Nearby if we have the location permission.
        LocationUtils locationUtils = LocationUtils.getInstance();
        if (locationUtils.hasAndroidLocationPermission()
                && locationUtils.isSystemLocationSettingEnabled()) {
            mNearbyForegroundSubscription.connect();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        mNearbyForegroundSubscription.subscribe();
        startRefresh(false, false);
    }

    @Override
    protected void onPause() {
        mNearbyForegroundSubscription.unsubscribe();
        super.onPause();
    }

    @Override
    public void onRefresh() {
        startRefresh(true, true);
    }

    @Override
    protected void onStop() {
        UrlManager.getInstance().removeObserver(this);
        mNearbyForegroundSubscription.disconnect();
        super.onStop();
    }

    private void resolve(Collection<UrlInfo> urls, final boolean isUserInitiated) {
        final long timestamp = SystemClock.elapsedRealtime();
        mPwsClient.resolve(urls, new PwsClient.ResolveScanCallback() {
            @Override
            public void onPwsResults(Collection<PwsResult> pwsResults) {
                long duration = SystemClock.elapsedRealtime() - timestamp;
                if (isUserInitiated) {
                    PhysicalWebUma.onRefreshPwsResolution(duration);
                } else {
                    PhysicalWebUma.onForegroundPwsResolution(duration);
                }

                // filter out duplicate groups.
                for (PwsResult pwsResult : pwsResults) {
                    mPwsResults.add(pwsResult);
                    if (!mAdapter.hasGroupId(pwsResult.groupId)) {
                        mAdapter.add(pwsResult);

                        if (pwsResult.iconUrl != null && !mAdapter.hasIcon(pwsResult.iconUrl)) {
                            fetchIcon(pwsResult.iconUrl);
                        }
                    }
                }
                finishRefresh();
            }
        });
    }

    /**
     * Handle a click event.
     * @param adapterView The AdapterView where the click happened.
     * @param view The View that was clicked inside the AdapterView.
     * @param position The position of the clicked element in the list.
     * @param id The row id of the clicked element in the list.
     */
    @Override
    public void onItemClick(AdapterView<?> adapterView, View view, int position, long id) {
        PhysicalWebUma.onUrlSelected();
        UrlInfo nearestUrlInfo = null;
        PwsResult nearestPwsResult = mAdapter.getItem(position);
        String groupId = nearestPwsResult.groupId;

        // Make sure the PwsResult corresponds to the closest UrlDevice in the group.
        double minDistance = Double.MAX_VALUE;
        for (PwsResult pwsResult : mPwsResults) {
            if (pwsResult.groupId.equals(groupId)) {
                UrlInfo urlInfo = UrlManager.getInstance().getUrlInfoByUrl(pwsResult.requestUrl);
                double distance = urlInfo.getDistance();
                if (distance < minDistance) {
                    minDistance = distance;
                    nearestPwsResult = pwsResult;
                    nearestUrlInfo = urlInfo;
                }
            }
        }
        Intent intent = createNavigateToUrlIntent(nearestPwsResult, nearestUrlInfo);
        mContext.startActivity(intent);
    }

    /**
     * Called when new nearby URLs are found.
     * @param urls The set of newly-found nearby URLs.
     */
    @Override
    public void onDisplayableUrlsAdded(Collection<UrlInfo> urls) {
        resolve(urls, false);
    }

    private void startRefresh(boolean isUserInitiated, boolean isSwipeInitiated) {
        if (mIsRefreshing) {
            return;
        }

        mIsRefreshing = true;
        mIsRefreshUserInitiated = isUserInitiated;

        // Clear the list adapter to trigger the empty list display.
        mAdapter.clear();

        Collection<UrlInfo> urls = UrlManager.getInstance().getUrls(true);

        // Check the Physical Web preference to ensure we do not resolve URLs when Physical Web is
        // off or onboarding. Normally the user will not reach this activity unless the preference
        // is explicitly enabled, but there is a button on the diagnostics page that launches into
        // the activity without checking the preference state.
        if (urls.isEmpty() || !PhysicalWeb.isPhysicalWebPreferenceEnabled()) {
            finishRefresh();
        } else {
            // Show the swipe-to-refresh busy indicator for refreshes initiated by a swipe.
            if (isSwipeInitiated) {
                mSwipeRefreshWidget.setRefreshing(true);
            }

            // Update the empty list view to show a scanning animation.
            mEmptyListText.setText(R.string.physical_web_empty_list_scanning);

            mScanningImageView.setImageResource(R.drawable.physical_web_scanning_animation);
            mScanningImageView.setColorFilter(null);

            AnimationDrawable animationDrawable =
                    (AnimationDrawable) mScanningImageView.getDrawable();
            animationDrawable.start();

            mPwsResults.clear();
            resolve(urls, isUserInitiated);
        }
    }

    private void finishRefresh() {
        // Hide the busy indicator.
        mSwipeRefreshWidget.setRefreshing(false);

        // Stop the scanning animation, show a "nothing found" message.
        mEmptyListText.setText(R.string.physical_web_empty_list);

        int tintColor = ContextCompat.getColor(this, R.color.light_grey);
        mScanningImageView.setImageResource(R.drawable.physical_web_logo);
        mScanningImageView.setColorFilter(tintColor, PorterDuff.Mode.SRC_IN);

        // Record refresh-related UMA.
        if (!mIsInitialDisplayRecorded) {
            mIsInitialDisplayRecorded = true;
            PhysicalWebUma.onUrlsDisplayed(mAdapter.getCount());
        } else if (mIsRefreshUserInitiated) {
            PhysicalWebUma.onUrlsRefreshed(mAdapter.getCount());
        }

        mIsRefreshing = false;
    }

    private void fetchIcon(String iconUrl) {
        mPwsClient.fetchIcon(iconUrl, new PwsClient.FetchIconCallback() {
            @Override
            public void onIconReceived(String url, Bitmap bitmap) {
                mAdapter.setIcon(url, bitmap);
            }
        });
    }

    private static Intent createNavigateToUrlIntent(PwsResult pwsResult, UrlInfo urlInfo) {
        String url = pwsResult.siteUrl;
        if (url == null) {
            url = pwsResult.requestUrl;
        }

        Intent intent = new Intent(Intent.ACTION_VIEW)
                .addCategory(Intent.CATEGORY_BROWSABLE)
                .setData(Uri.parse(url))
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (urlInfo != null && urlInfo.getDeviceAddress() != null) {
            BluetoothAdapter bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
            if (bluetoothAdapter != null) {
                try {
                    intent.putExtra(BluetoothDevice.EXTRA_DEVICE,
                            bluetoothAdapter.getRemoteDevice(urlInfo.getDeviceAddress()));
                } catch (IllegalArgumentException e) {
                    Log.e(TAG, "Invalid device address: " + urlInfo.getDeviceAddress(), e);
                }
            }
        }
        return intent;
    }

    @VisibleForTesting
    void overridePwsClientForTesting(PwsClient pwsClient) {
        mPwsClient = pwsClient;
    }

    @VisibleForTesting
    void overrideContextForTesting(Context context) {
        mContext = context;
    }
}
