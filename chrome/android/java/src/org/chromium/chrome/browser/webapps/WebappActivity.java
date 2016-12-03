// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.StrictMode;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.document.DocumentUtils;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.metrics.WebappUma;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.chrome.browser.widget.ControlContainer;
import org.chromium.content.browser.ScreenOrientationProvider;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.ui.base.PageTransition;

import java.io.File;
import java.util.concurrent.TimeUnit;

/**
 * Displays a webapp in a nearly UI-less Chrome (InfoBars still appear).
 */
public class WebappActivity extends FullScreenActivity {
    public static final String WEBAPP_SCHEME = "webapp";

    private static final String TAG = "WebappActivity";
    private static final long MS_BEFORE_NAVIGATING_BACK_FROM_INTERSTITIAL = 1000;

    private final WebappDirectoryManager mDirectoryManager;

    protected WebappInfo mWebappInfo;

    private boolean mOldWebappCleanupStarted;

    private ViewGroup mSplashScreen;
    private WebappUrlBar mUrlBar;

    private boolean mIsInitialized;
    private Integer mBrandColor;

    private WebappUma mWebappUma;

    private Bitmap mLargestFavicon;

    /**
     * Construct all the variables that shouldn't change.  We do it here both to clarify when the
     * objects are created and to ensure that they exist throughout the parallelized initialization
     * of the WebappActivity.
     */
    public WebappActivity() {
        mWebappInfo = WebappInfo.createEmpty();
        mDirectoryManager = new WebappDirectoryManager();
        mWebappUma = new WebappUma();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        if (intent == null) return;
        super.onNewIntent(intent);

        WebappInfo newWebappInfo = WebappInfo.create(intent);
        if (newWebappInfo == null) {
            Log.e(TAG, "Failed to parse new Intent: " + intent);
            finish();
        } else if (!TextUtils.equals(mWebappInfo.id(), newWebappInfo.id())) {
            mWebappInfo = newWebappInfo;
            resetSavedInstanceState();
            if (mIsInitialized) initializeUI(null);
            // TODO(dominickn): send the web app into fullscreen if mDisplayMode is
            // WebDisplayMode.Fullscreen. See crbug.com/581522
        }
    }

    protected boolean isInitialized() {
        return mIsInitialized;
    }

    private void initializeUI(Bundle savedInstanceState) {
        // We do not load URL when restoring from saved instance states.
        if (savedInstanceState == null && mWebappInfo.isInitialized()) {
            if (TextUtils.isEmpty(getActivityTab().getUrl())) {
                getActivityTab().loadUrl(new LoadUrlParams(
                        mWebappInfo.uri().toString(), PageTransition.AUTO_TOPLEVEL));
            }
        } else {
            if (NetworkChangeNotifier.isOnline()) getActivityTab().reloadIgnoringCache();
        }

        getActivityTab().addObserver(createTabObserver());
        getActivityTab().getTabWebContentsDelegateAndroid().setDisplayMode(
                WebDisplayMode.Standalone);
        // TODO(dominickn): send the web app into fullscreen if mDisplayMode is
        // WebDisplayMode.Fullscreen. See crbug.com/581522
    }

    @Override
    public void preInflationStartup() {
        WebappInfo info = WebappInfo.create(getIntent());
        if (info != null) mWebappInfo = info;

        ScreenOrientationProvider.lockOrientation((byte) mWebappInfo.orientation(), this);
        super.preInflationStartup();
    }

    @Override
    public void finishNativeInitialization() {
        if (!mWebappInfo.isInitialized()) finish();
        super.finishNativeInitialization();
        initializeUI(getSavedInstanceState());
        mIsInitialized = true;
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        if (getActivityTab() != null) {
            outState.putInt(BUNDLE_TAB_ID, getActivityTab().getId());
            outState.putString(BUNDLE_TAB_URL, getActivityTab().getUrl());
        }
    }

    @Override
    public void onStartWithNative() {
        super.onStartWithNative();
        mDirectoryManager.cleanUpDirectories(this, getId());
    }

    @Override
    public void onStopWithNative() {
        super.onStopWithNative();
        mDirectoryManager.cancelCleanup();
        if (getActivityTab() != null) saveState(getActivityDirectory());
        if (getFullscreenManager() != null) {
            getFullscreenManager().setPersistentFullscreenMode(false);
        }
    }

    /**
     * Saves the tab data out to a file.
     */
    void saveState(File activityDirectory) {
        String tabFileName = TabState.getTabStateFilename(getActivityTab().getId(), false);
        File tabFile = new File(activityDirectory, tabFileName);

        // Temporarily allowing disk access while fixing. TODO: http://crbug.com/525781
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        StrictMode.allowThreadDiskWrites();
        try {
            long time = SystemClock.elapsedRealtime();
            TabState.saveState(tabFile, getActivityTab().getState(), false);
            RecordHistogram.recordTimesHistogram("Android.StrictMode.WebappSaveState",
                    SystemClock.elapsedRealtime() - time, TimeUnit.MILLISECONDS);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    @Override
    public void onResume() {
        if (!isFinishing()) {
            if (getIntent() != null) {
                // Avoid situations where Android starts two Activities with the same data.
                DocumentUtils.finishOtherTasksWithData(getIntent().getData(), getTaskId());
            }
            updateTaskDescription();
        }
        super.onResume();

        // Kick off the old web app cleanup (if we haven't already) now that we have queued the
        // current web app's storage to be opened.
        if (!mOldWebappCleanupStarted) {
            WebappRegistry.unregisterOldWebapps(this, System.currentTimeMillis());
            mOldWebappCleanupStarted = true;
        }
    }

    @Override
    public void onResumeWithNative() {
        super.onResumeWithNative();
        mWebappUma.commitMetrics();
    }

    @Override
    protected int getControlContainerLayoutId() {
        return R.layout.webapp_control_container;
    }

    @Override
    public void postInflationStartup() {
        initializeWebappData();

        super.postInflationStartup();
        WebappControlContainer controlContainer =
                (WebappControlContainer) findViewById(R.id.control_container);
        mUrlBar = (WebappUrlBar) controlContainer.findViewById(R.id.webapp_url_bar);
    }

    /**
     * @return Structure containing data about the webapp currently displayed.
     *         The return value should not be cached.
     */
    WebappInfo getWebappInfo() {
        return mWebappInfo;
    }

    protected int getBackgroundColor() {
        return ColorUtils.getOpaqueColor(mWebappInfo.backgroundColor(
                ApiCompatibilityUtils.getColor(getResources(), R.color.webapp_default_bg)));
    }

    private void initializeWebappData() {
        mSplashScreen = new FrameLayout(this);
        mSplashScreen.setBackgroundColor(getBackgroundColor());

        ViewGroup contentView = (ViewGroup) findViewById(android.R.id.content);
        contentView.addView(mSplashScreen);

        mWebappUma.splashscreenVisible();
        mWebappUma.recordSplashscreenBackgroundColor(mWebappInfo.hasValidBackgroundColor()
                ? WebappUma.SPLASHSCREEN_COLOR_STATUS_CUSTOM
                : WebappUma.SPLASHSCREEN_COLOR_STATUS_DEFAULT);

        WebappRegistry.getWebappDataStorage(this, mWebappInfo.id(),
                new WebappRegistry.FetchWebappDataStorageCallback() {
                    @Override
                    public void onWebappDataStorageRetrieved(WebappDataStorage storage) {
                        onDataStorageFetched(storage);
                    }
                }
        );
    }

    protected void recordSplashScreenThemeColorUma() {
        mWebappUma.recordSplashscreenThemeColor(mWebappInfo.hasValidThemeColor()
                ? WebappUma.SPLASHSCREEN_COLOR_STATUS_CUSTOM
                : WebappUma.SPLASHSCREEN_COLOR_STATUS_DEFAULT);
    }

    protected void onDataStorageFetched(WebappDataStorage storage) {
        recordSplashScreenThemeColorUma();
        if (storage == null) return;

        // The information in the WebappDataStorage may have been purged by the
        // user clearing their history or not launching the web app recently.
        // Restore the data if necessary from the intent.
        storage.updateFromShortcutIntent(getIntent());

        // A recent last used time is the indicator that the web app is still
        // present on the home screen, and enables sources such as notifications to
        // launch web apps. Thus, we do not update the last used time when the web
        // app is not directly launched from the home screen, as this interferes
        // with the heuristic.
        if (mWebappInfo.isLaunchedFromHomescreen()) {
            storage.updateLastUsedTime();
        }

        retrieveSplashScreenImage(storage);
    }

    protected void retrieveSplashScreenImage(WebappDataStorage storage) {
        // Retrieve the splash image if it exists.
        storage.getSplashScreenImage(new WebappDataStorage.FetchCallback<Bitmap>() {
            @Override
            public void onDataRetrieved(Bitmap splashImage) {
                initializeSplashScreenWidgets(splashImage);
            }
        });
    }

    protected void initializeSplashScreenWidgets(Bitmap splashImage) {
        Bitmap displayIcon = splashImage == null ? mWebappInfo.icon() : splashImage;
        int minimiumSizeThreshold = getResources().getDimensionPixelSize(
                R.dimen.webapp_splash_image_size_minimum);
        int bigThreshold = getResources().getDimensionPixelSize(
                R.dimen.webapp_splash_image_size_threshold);

        // Inflate the correct layout for the image.
        int layoutId;
        if (displayIcon == null || displayIcon.getWidth() < minimiumSizeThreshold
                || (displayIcon == mWebappInfo.icon() && mWebappInfo.isIconGenerated())) {
            mWebappUma.recordSplashscreenIconType(WebappUma.SPLASHSCREEN_ICON_TYPE_NONE);
            layoutId = R.layout.webapp_splash_screen_no_icon;
        } else {
            // The size of the splash screen image determines which layout to use.
            boolean isUsingSmallSplashImage = displayIcon.getWidth() <= bigThreshold
                    || displayIcon.getHeight() <= bigThreshold;
            if (isUsingSmallSplashImage) {
                layoutId = R.layout.webapp_splash_screen_small;
            } else {
                layoutId = R.layout.webapp_splash_screen_large;
            }

            // Record stats about the splash screen.
            int splashScreenIconType;
            if (splashImage == null) {
                splashScreenIconType = WebappUma.SPLASHSCREEN_ICON_TYPE_FALLBACK;
            } else if (isUsingSmallSplashImage) {
                splashScreenIconType = WebappUma.SPLASHSCREEN_ICON_TYPE_CUSTOM_SMALL;
            } else {
                splashScreenIconType = WebappUma.SPLASHSCREEN_ICON_TYPE_CUSTOM;
            }
            mWebappUma.recordSplashscreenIconType(splashScreenIconType);
            mWebappUma.recordSplashscreenIconSize(
                    Math.round(displayIcon.getWidth()
                            / getResources().getDisplayMetrics().density));
        }

        ViewGroup subLayout = (ViewGroup) LayoutInflater.from(WebappActivity.this)
                .inflate(layoutId, mSplashScreen, true);

        // Set up the elements of the splash screen.
        TextView appNameView = (TextView) subLayout.findViewById(
                R.id.webapp_splash_screen_name);
        ImageView splashIconView = (ImageView) subLayout.findViewById(
                R.id.webapp_splash_screen_icon);
        appNameView.setText(mWebappInfo.name());
        if (splashIconView != null) splashIconView.setImageBitmap(displayIcon);

        if (ColorUtils.shouldUseLightForegroundOnBackground(getBackgroundColor())) {
            appNameView.setTextColor(ApiCompatibilityUtils.getColor(getResources(),
                    R.color.webapp_splash_title_light));
        }
    }

    private void updateUrlBar() {
        Tab tab = getActivityTab();
        if (tab == null || mUrlBar == null) return;
        mUrlBar.update(tab.getUrl(), tab.getSecurityLevel());
    }

    private boolean isWebappDomain() {
        return UrlUtilities.sameDomainOrHost(
                getActivityTab().getUrl(), getWebappInfo().uri().toString(), true);
    }

    protected TabObserver createTabObserver() {
        return new EmptyTabObserver() {

            @Override
            public void onSSLStateUpdated(Tab tab) {
                updateUrlBar();
            }

            @Override
            public void onDidStartProvisionalLoadForFrame(
                    Tab tab, long frameId, long parentFrameId, boolean isMainFrame,
                    String validatedUrl, boolean isErrorPage, boolean isIframeSrcdoc) {
                if (isMainFrame) updateUrlBar();
            }

            @Override
            public void onDidChangeThemeColor(Tab tab, int color) {
                if (!isWebappDomain()) return;
                mBrandColor = color;
                updateTaskDescription();
            }

            @Override
            public void onTitleUpdated(Tab tab) {
                if (!isWebappDomain()) return;
                updateTaskDescription();
            }

            @Override
            public void onFaviconUpdated(Tab tab, Bitmap icon) {
                if (!isWebappDomain()) return;
                // No need to cache the favicon if there is an icon declared in app manifest.
                if (mWebappInfo.icon() != null) return;
                if (icon == null) return;
                if (mLargestFavicon == null || icon.getWidth() > mLargestFavicon.getWidth()
                        || icon.getHeight() > mLargestFavicon.getHeight()) {
                    mLargestFavicon = icon;
                    updateTaskDescription();
                }
            }

            @Override
            public void onDidNavigateMainFrame(Tab tab, String url, String baseUrl,
                    boolean isNavigationToDifferentPage, boolean isNavigationInPage,
                    int statusCode) {
                updateUrlBar();
            }

            @Override
            public void onDidAttachInterstitialPage(Tab tab) {
                updateUrlBar();

                int state = ApplicationStatus.getStateForActivity(WebappActivity.this);
                if (state == ActivityState.PAUSED || state == ActivityState.STOPPED
                        || state == ActivityState.DESTROYED) {
                    return;
                }

                // Kick the interstitial navigation to Chrome.
                Intent intent = new Intent(
                        Intent.ACTION_VIEW, Uri.parse(getActivityTab().getUrl()));
                intent.setPackage(getPackageName());
                intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                startActivity(intent);

                // Pretend like the navigation never happened.  We delay so that this happens while
                // the Activity is in the background.
                mHandler.postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        getActivityTab().goBack();
                    }
                }, MS_BEFORE_NAVIGATING_BACK_FROM_INTERSTITIAL);
            }

            @Override
            public void onDidDetachInterstitialPage(Tab tab) {
                updateUrlBar();
            }

            @Override
            public void didFirstVisuallyNonEmptyPaint(Tab tab) {
                hideSplashScreen(WebappUma.SPLASHSCREEN_HIDES_REASON_PAINT);
            }

            @Override
            public void onPageLoadFinished(Tab tab) {
                hideSplashScreen(WebappUma.SPLASHSCREEN_HIDES_REASON_LOAD_FINISHED);
            }

            @Override
            public void onPageLoadFailed(Tab tab, int errorCode) {
                hideSplashScreen(WebappUma.SPLASHSCREEN_HIDES_REASON_LOAD_FAILED);
            }

            @Override
            public void onCrash(Tab tab, boolean sadTabShown) {
                hideSplashScreen(WebappUma.SPLASHSCREEN_HIDES_REASON_CRASH);
            }
        };
    }

    private void updateTaskDescription() {
        String title = null;
        if (!TextUtils.isEmpty(mWebappInfo.shortName())) {
            title = mWebappInfo.shortName();
        } else if (getActivityTab() != null) {
            title = getActivityTab().getTitle();
        }

        Bitmap icon = null;
        if (mWebappInfo.icon() != null) {
            icon = mWebappInfo.icon();
        } else if (getActivityTab() != null) {
            icon = mLargestFavicon;
        }

        if (mBrandColor == null && mWebappInfo.hasValidThemeColor()) {
            mBrandColor = (int) mWebappInfo.themeColor();
        }

        int taskDescriptionColor =
                ApiCompatibilityUtils.getColor(getResources(), R.color.default_primary_color);
        int statusBarColor = Color.BLACK;
        if (mBrandColor != null) {
            taskDescriptionColor = mBrandColor;
            statusBarColor = ColorUtils.getDarkenedColorForStatusBar(mBrandColor);
        }

        ApiCompatibilityUtils.setTaskDescription(this, title, icon,
                ColorUtils.getOpaqueColor(taskDescriptionColor));
        ApiCompatibilityUtils.setStatusBarColor(getWindow(), statusBarColor);
    }

    @Override
    protected void setStatusBarColor(Tab tab, int color) {
        // Intentionally do nothing as WebappActivity explicitly sets status bar color.
    }

    /** Returns a unique identifier for this WebappActivity. */
    protected String getId() {
        return mWebappInfo.id();
    }

    /**
     * Get the active directory by this web app.
     *
     * @return The directory used for the current web app.
     */
    @Override
    protected final File getActivityDirectory() {
        return mDirectoryManager.getWebappDirectory(this, getId());
    }

    private void hideSplashScreen(final int reason) {
        if (mSplashScreen == null) return;

        mSplashScreen.animate()
                .alpha(0f)
                .withEndAction(new Runnable() {
                    @Override
                    public void run() {
                        ViewGroup contentView =
                                (ViewGroup) findViewById(android.R.id.content);
                        if (mSplashScreen == null) return;
                        contentView.removeView(mSplashScreen);
                        mSplashScreen = null;
                        mWebappUma.splashscreenHidden(reason);
                    }
                });
    }

    @VisibleForTesting
    boolean isSplashScreenVisibleForTests() {
        return mSplashScreen != null;
    }

    @VisibleForTesting
    ViewGroup getSplashScreenForTests() {
        return mSplashScreen;
    }

    @VisibleForTesting
    WebappUrlBar getUrlBarForTests() {
        return mUrlBar;
    }

    @VisibleForTesting
    boolean isUrlBarVisible() {
        return findViewById(R.id.control_container).getVisibility() == View.VISIBLE;
    }

    @Override
    protected final ChromeFullscreenManager createFullscreenManager(
            ControlContainer controlContainer) {
        return new ChromeFullscreenManager(this, controlContainer, getTabModelSelector(),
                getControlContainerHeightResource(), false /* supportsBrowserOverride */);
    }

    @Override
    public int getControlContainerHeightResource() {
        return R.dimen.webapp_control_container_height;
    }

    @Override
    protected Drawable getBackgroundDrawable() {
        return null;
    }

    @Override
    protected TabDelegateFactory createTabDelegateFactory() {
        return new WebappDelegateFactory(this);
    }

    // We're temporarily disable CS on webapp since there are some issues. (http://crbug.com/471950)
    // TODO(changwan): re-enable it once the issues are resolved.
    @Override
    protected boolean isContextualSearchAllowed() {
        return false;
    }
}
