// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.provider.Browser;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.FrameLayout;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.MemoryPressureListener;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler.IntentHandlerDelegate;
import org.chromium.chrome.browser.IntentHandler.TabOpenType;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome.OverviewLayoutFactoryDelegate;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromePhone;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromeTablet;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EventFilter;
import org.chromium.chrome.browser.compositor.layouts.phone.StackLayout;
import org.chromium.chrome.browser.cookies.CookiesFetcher;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.firstrun.FirstRunActivity;
import org.chromium.chrome.browser.firstrun.FirstRunFlowSequencer;
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.incognito.IncognitoNotificationManager;
import org.chromium.chrome.browser.infobar.DataReductionPromoInfoBar;
import org.chromium.chrome.browser.metrics.ActivityStopMetrics;
import org.chromium.chrome.browser.metrics.LaunchMetrics;
import org.chromium.chrome.browser.metrics.StartupMetrics;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.ntp.NativePageAssassin;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.omaha.OmahaClient;
import org.chromium.chrome.browser.omnibox.AutocompleteController;
import org.chromium.chrome.browser.partnercustomizations.HomepageManager;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.datareduction.DataReductionPromoScreen;
import org.chromium.chrome.browser.signin.SigninPromoUtil;
import org.chromium.chrome.browser.snackbar.undo.UndoBarController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import org.chromium.chrome.browser.toolbar.ToolbarControlContainer;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.widget.emptybackground.EmptyBackgroundViewWrapper;
import org.chromium.chrome.browser.widget.findinpage.FindToolbarManager;
import org.chromium.content.browser.ContentVideoView;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.crypto.CipherFactory;
import org.chromium.content.common.ContentSwitches;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;
import java.util.List;

/**
 * This is the main activity for ChromeMobile when not running in document mode.  All the tabs
 * are accessible via a chrome specific tab switching UI.
 */
public class ChromeTabbedActivity extends ChromeActivity implements OverviewModeObserver {

    private static final int FIRST_RUN_EXPERIENCE_RESULT = 101;
    private static final int CCT_RESULT = 102;

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        BACK_PRESSED_NOTHING_HAPPENED,
        BACK_PRESSED_HELP_URL_CLOSED,
        BACK_PRESSED_MINIMIZED_NO_TAB_CLOSED,
        BACK_PRESSED_MINIMIZED_TAB_CLOSED,
        BACK_PRESSED_TAB_CLOSED,
        BACK_PRESSED_TAB_IS_NULL,
        BACK_PRESSED_EXITED_TAB_SWITCHER,
        BACK_PRESSED_EXITED_FULLSCREEN,
        BACK_PRESSED_NAVIGATED_BACK
    })
    private @interface BackPressedResult {}
    private static final int BACK_PRESSED_NOTHING_HAPPENED = 0;
    private static final int BACK_PRESSED_HELP_URL_CLOSED = 1;
    private static final int BACK_PRESSED_MINIMIZED_NO_TAB_CLOSED = 2;
    private static final int BACK_PRESSED_MINIMIZED_TAB_CLOSED = 3;
    private static final int BACK_PRESSED_TAB_CLOSED = 4;
    private static final int BACK_PRESSED_TAB_IS_NULL = 5;
    private static final int BACK_PRESSED_EXITED_TAB_SWITCHER = 6;
    private static final int BACK_PRESSED_EXITED_FULLSCREEN = 7;
    private static final int BACK_PRESSED_NAVIGATED_BACK = 8;
    private static final int BACK_PRESSED_COUNT = 9;

    private static final String TAG = "ChromeTabbedActivity";

    private static final String HELP_URL_PREFIX = "https://support.google.com/chrome/";

    private static final String FRE_RUNNING = "First run is running";

    private static final String WINDOW_INDEX = "window_index";

    // How long to delay closing the current tab when our app is minimized.  Have to delay this
    // so that we don't show the contents of the next tab while minimizing.
    private static final long CLOSE_TAB_ON_MINIMIZE_DELAY_MS = 500;

    // Maximum delay for initial tab creation. This is for homepage and NTP, not previous tabs
    // restore. This is needed because we do not know when reading PartnerBrowserCustomizations
    // provider will be finished.
    private static final int INITIAL_TAB_CREATION_TIMEOUT_MS = 500;

    /**
     * Sending an intent with this extra sets the app into single process mode.
     * This is only used for testing, when certain tests want to force this behaviour.
     */
    public static final String INTENT_EXTRA_TEST_RENDER_PROCESS_LIMIT = "render_process_limit";

    /**
     * Sending an intent with this extra sets the ChromeTabbedActivity to skip the First Run
     * Experience.
     */
    public static final String SKIP_FIRST_RUN_EXPERIENCE = "skip_first_run_experience";

    /**
     * Sending an intent with this action to Chrome will cause it to close all tabs
     * (iff the --enable-test-intents command line flag is set). If a URL is supplied in the
     * intent data, this will be loaded and unaffected by the close all action.
     */
    private static final String ACTION_CLOSE_TABS =
            "com.google.android.apps.chrome.ACTION_CLOSE_TABS";

    private final ActivityStopMetrics mActivityStopMetrics = new ActivityStopMetrics();

    private FindToolbarManager mFindToolbarManager;

    private UndoBarController mUndoBarPopupController;

    private LayoutManagerChrome mLayoutManager;

    private ViewGroup mContentContainer;

    private ToolbarControlContainer mControlContainer;

    private TabModelSelectorImpl mTabModelSelectorImpl;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private TabModelObserver mTabModelObserver;

    private boolean mUIInitialized = false;

    private boolean mIsOnFirstRun = false;
    private Boolean mMergeTabsOnResume;

    private Boolean mIsAccessibilityEnabled;

    /**
     * Keeps track of whether or not a specific tab was created based on the startup intent.
     */
    private boolean mCreatedTabOnStartup = false;

    // Whether or not chrome was launched with an intent to open a tab.
    private boolean mIntentWithEffect = false;

    // Time at which an intent was received and handled.
    private long mIntentHandlingTimeMs = 0;

    private class TabbedAssistStatusHandler extends AssistStatusHandler {
        public TabbedAssistStatusHandler(Activity activity) {
            super(activity);
        }

        @Override
        public boolean isAssistSupported() {
            // If we are in the tab switcher and any incognito tabs are present, disable assist.
            if (isInOverviewMode() && mTabModelSelectorImpl != null
                    && mTabModelSelectorImpl.getModel(true).getCount() > 0) {
                return false;
            }
            return super.isAssistSupported();
        }
    }

    @Override
    public void initializeCompositor() {
        try {
            TraceEvent.begin("ChromeTabbedActivity.initializeCompositor");
            super.initializeCompositor();

            mTabModelSelectorImpl.onNativeLibraryReady(getTabContentManager());

            mTabModelObserver = new EmptyTabModelObserver() {
                @Override
                public void didCloseTab(int tabId, boolean incognito) {
                    closeIfNoTabsAndHomepageEnabled(false);
                }

                @Override
                public void tabPendingClosure(Tab tab) {
                    closeIfNoTabsAndHomepageEnabled(true);
                }

                @Override
                public void tabRemoved(Tab tab) {
                    closeIfNoTabsAndHomepageEnabled(false);
                }

                private void closeIfNoTabsAndHomepageEnabled(boolean isPendingClosure) {
                    if (getTabModelSelector().getTotalTabCount() == 0) {
                        // If the last tab is closed, and homepage is enabled, then exit Chrome.
                        if (HomepageManager.isHomepageEnabled(getApplicationContext())) {
                            finish();
                        } else if (isPendingClosure) {
                            NewTabPageUma.recordNTPImpression(
                                    NewTabPageUma.NTP_IMPESSION_POTENTIAL_NOTAB);
                        }
                    }
                }

                @Override
                public void didAddTab(Tab tab, TabLaunchType type) {
                    if (type == TabLaunchType.FROM_LONGPRESS_BACKGROUND
                            && !DeviceClassManager.enableAnimations(getApplicationContext())) {
                        Toast.makeText(ChromeTabbedActivity.this,
                                R.string.open_in_new_tab_toast,
                                Toast.LENGTH_SHORT).show();
                    }
                }

                @Override
                public void allTabsPendingClosure(List<Integer> tabIds) {
                    NewTabPageUma.recordNTPImpression(
                            NewTabPageUma.NTP_IMPESSION_POTENTIAL_NOTAB);
                }
            };
            for (TabModel model : mTabModelSelectorImpl.getModels()) {
                model.addObserver(mTabModelObserver);
            }

            Bundle state = getSavedInstanceState();
            if (state != null && state.containsKey(FRE_RUNNING)) {
                mIsOnFirstRun = state.getBoolean(FRE_RUNNING);
            }
        } finally {
            TraceEvent.end("ChromeTabbedActivity.initializeCompositor");
        }
    }

    private void refreshSignIn() {
        if (mIsOnFirstRun) return;
        FirstRunSignInProcessor.start(this);
    }

    @Override
    public void onNewIntent(Intent intent) {
        mIntentHandlingTimeMs = SystemClock.uptimeMillis();
        super.onNewIntent(intent);
    }

    @Override
    public void finishNativeInitialization() {
        try {
            TraceEvent.begin("ChromeTabbedActivity.finishNativeInitialization");

            launchFirstRunExperience();

            ChromePreferenceManager preferenceManager = ChromePreferenceManager.getInstance(this);
            // Promos can only be shown when we start with ACTION_MAIN intent and
            // after FRE is complete.
            if (!mIntentWithEffect && FirstRunStatus.getFirstRunFlowComplete(this)) {
                // Only show promos on the second oppurtunity. This is because we show FRE on the
                // first oppurtunity, and we don't want to show such content back to back.
                if (preferenceManager.getPromosSkippedOnFirstStart()) {
                    // Data reduction promo should be temporarily suppressed if the sign in promo is
                    // shown to avoid nagging users too much.
                    if (!SigninPromoUtil.launchSigninPromoIfNeeded(this)) {
                        DataReductionPromoScreen.launchDataReductionPromo(this);
                    }
                } else {
                    preferenceManager.setPromosSkippedOnFirstStart(true);
                }
            }

            refreshSignIn();

            initializeUI();

            // The dataset has already been created, we need to initialize our state.
            mTabModelSelectorImpl.notifyChanged();

            getWindow().setFeatureInt(Window.FEATURE_INDETERMINATE_PROGRESS,
                    Window.PROGRESS_VISIBILITY_OFF);

            // Check for incognito tabs to handle the case where Chrome was swiped away in the
            // background.
            int incognitoCount = TabWindowManager.getInstance().getIncognitoTabCount();
            if (incognitoCount == 0) IncognitoNotificationManager.dismissIncognitoNotification();

            super.finishNativeInitialization();
        } finally {
            TraceEvent.end("ChromeTabbedActivity.finishNativeInitialization");
        }
    }

    @Override
    public void onResumeWithNative() {
        super.onResumeWithNative();
        CookiesFetcher.restoreCookies(this);
        StartupMetrics.getInstance().recordHistogram(false);

        if (FeatureUtilities.isTabModelMergingEnabled()) {
            boolean inMultiWindowMode = MultiWindowUtils.getInstance().isInMultiWindowMode(this);
            // Merge tabs if the activity is not in multi-window mode and mMergeTabsOnResume is true
            // or unset because the activity is just starting or was destroyed.
            if (!inMultiWindowMode && (mMergeTabsOnResume == null || mMergeTabsOnResume)) {
                maybeMergeTabs();
            }
            mMergeTabsOnResume = false;
        }
    }

    @Override
    public void onPauseWithNative() {
        mTabModelSelectorImpl.commitAllTabClosures();
        CookiesFetcher.persistCookies(this);
        super.onPauseWithNative();
    }

    @Override
    public void onStopWithNative() {
        super.onStopWithNative();

        if (getActivityTab() != null) getActivityTab().setIsAllowedToReturnToExternalApp(false);

        mTabModelSelectorImpl.saveState();
        StartupMetrics.getInstance().recordHistogram(true);
        mActivityStopMetrics.onStopWithNative(this);
    }

    @Override
    public void onStart() {
        super.onStart();
        StartupMetrics.getInstance().updateIntent(getIntent());
    }

    @Override
    public void onStartWithNative() {
        super.onStartWithNative();
        // If we don't have a current tab, show the overview mode.
        if (getActivityTab() == null) mLayoutManager.showOverview(false);

        resetSavedInstanceState();
    }

    @Override
    public void onNewIntentWithNative(Intent intent) {
        try {
            TraceEvent.begin("ChromeTabbedActivity.onNewIntentWithNative");

            super.onNewIntentWithNative(intent);
            if (CommandLine.getInstance().hasSwitch(ContentSwitches.ENABLE_TEST_INTENTS)) {
                handleDebugIntent(intent);
            }
        } finally {
            TraceEvent.end("ChromeTabbedActivity.onNewIntentWithNative");
        }
    }

    @Override
    public ChromeTabCreator getTabCreator(boolean incognito) {
        TabCreator tabCreator = super.getTabCreator(incognito);
        assert tabCreator instanceof ChromeTabCreator;
        return (ChromeTabCreator) tabCreator;
    }

    @Override
    public ChromeTabCreator getCurrentTabCreator() {
        TabCreator tabCreator = super.getCurrentTabCreator();
        assert tabCreator instanceof ChromeTabCreator;
        return (ChromeTabCreator) tabCreator;
    }

    @Override
    protected AssistStatusHandler createAssistStatusHandler() {
        return new TabbedAssistStatusHandler(this);
    }

    private void handleDebugIntent(Intent intent) {
        if (ACTION_CLOSE_TABS.equals(intent.getAction())) {
            getTabModelSelector().closeAllTabs();
        } else if (MemoryPressureListener.handleDebugIntent(ChromeTabbedActivity.this,
                intent.getAction())) {
            // Handled.
        }
    }

    private static class StackLayoutFactory implements OverviewLayoutFactoryDelegate {
        @Override
        public Layout createOverviewLayout(Context context, LayoutUpdateHost updateHost,
                LayoutRenderHost renderHost, EventFilter eventFilter) {
            return new StackLayout(context, updateHost, renderHost, eventFilter);
        }
    }

    private void initializeUI() {
        try {
            TraceEvent.begin("ChromeTabbedActivity.initializeUI");

            CommandLine commandLine = CommandLine.getInstance();

            commandLine.appendSwitch(ContentSwitches.ENABLE_INSTANT_EXTENDED_API);

            CompositorViewHolder compositorViewHolder = getCompositorViewHolder();
            if (DeviceFormFactor.isTablet(this)) {
                boolean enableTabSwitcher =
                        CommandLine.getInstance().hasSwitch(ChromeSwitches.ENABLE_TABLET_TAB_STACK);
                mLayoutManager = new LayoutManagerChromeTablet(compositorViewHolder,
                        enableTabSwitcher ? new StackLayoutFactory() : null);
            } else {
                mLayoutManager = new LayoutManagerChromePhone(compositorViewHolder,
                        new StackLayoutFactory());
            }
            mLayoutManager.setEnableAnimations(DeviceClassManager.enableAnimations(this));
            mLayoutManager.addOverviewModeObserver(this);

            // TODO(yusufo): get rid of findViewById(R.id.url_bar).
            initializeCompositorContent(mLayoutManager, findViewById(R.id.url_bar),
                    mContentContainer, mControlContainer);

            mTabModelSelectorImpl.setOverviewModeBehavior(mLayoutManager);

            mUndoBarPopupController.initialize();

            // Adjust the content container if we're not entering fullscreen mode.
            if (getFullscreenManager() == null) {
                float controlHeight = getResources().getDimension(R.dimen.control_container_height);
                ((FrameLayout.LayoutParams) mContentContainer.getLayoutParams()).topMargin =
                        (int) controlHeight;
            }

            // Bootstrap the first tab as it may have been created before initializing the
            // fullscreen manager.
            if (mTabModelSelectorImpl != null && mTabModelSelectorImpl.getCurrentTab() != null) {
                mTabModelSelectorImpl.getCurrentTab().setFullscreenManager(getFullscreenManager());
            }

            mFindToolbarManager = new FindToolbarManager(this,
                    getToolbarManager().getActionModeController().getActionModeCallback());
            if (getContextualSearchManager() != null) {
                getContextualSearchManager().setFindToolbarManager(mFindToolbarManager);
            }

            OnClickListener tabSwitcherClickHandler = new OnClickListener() {
                @Override
                public void onClick(View v) {
                    if (getFullscreenManager() != null
                            && getFullscreenManager().getPersistentFullscreenMode()) {
                        return;
                    }
                    toggleOverview();
                }
            };
            OnClickListener newTabClickHandler = new OnClickListener() {
                @Override
                public void onClick(View v) {
                    // This assumes that the keyboard can not be seen at the same time as the
                    // newtab button on the toolbar.
                    getCurrentTabCreator().launchNTP();
                }
            };
            OnClickListener bookmarkClickHandler = new OnClickListener() {
                @Override
                public void onClick(View v) {
                    addOrEditBookmark(getActivityTab());
                }
            };

            getToolbarManager().initializeWithNative(mTabModelSelectorImpl, getFullscreenManager(),
                    mFindToolbarManager, mLayoutManager, mLayoutManager,
                    tabSwitcherClickHandler, newTabClickHandler, bookmarkClickHandler, null);
            getToolbarManager().getToolbar().setReturnButtonListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    if (getActivityTab() == null) return;
                    mActivityStopMetrics.setStopReason(
                            ActivityStopMetrics.STOP_REASON_RETURN_BUTTON);
                    RecordUserAction.record("TaskManagement.ReturnButtonClicked");
                    sendToBackground(getActivityTab());
                }
            });

            if (isTablet()) {
                EmptyBackgroundViewWrapper bgViewWrapper = new EmptyBackgroundViewWrapper(
                        getTabModelSelector(), getTabCreator(false), ChromeTabbedActivity.this,
                        getAppMenuHandler(), getSnackbarManager(), mLayoutManager);
                bgViewWrapper.initialize();
            }

            mLayoutManager.hideOverview(false);

            mUIInitialized = true;
        } finally {
            TraceEvent.end("ChromeTabbedActivity.initializeUI");
        }
    }

    @Override
    public void initializeState() {
        // This method goes through 3 steps:
        // 1. Load the saved tab state (but don't start restoring the tabs yet).
        // 2. Process the Intent that this activity received and if that should result in any
        //    new tabs, create them.  This is done after step 1 so that the new tab gets
        //    created after previous tab state was restored.
        // 3. If no tabs were created in any of the above steps, create an NTP, otherwise
        //    start asynchronous tab restore (loading the previously active tab synchronously
        //    if no new tabs created in step 2).

        // Only look at the original intent if this is not a "restoration" and we are allowed to
        // process intents. Any subsequent intents are carried through onNewIntent.
        try {
            TraceEvent.begin("ChromeTabbedActivity.initializeState");

            super.initializeState();

            Intent intent = getIntent();

            boolean hadCipherData =
                    CipherFactory.getInstance().restoreFromBundle(getSavedInstanceState());

            boolean noRestoreState =
                    CommandLine.getInstance().hasSwitch(ChromeSwitches.NO_RESTORE_STATE);
            if (noRestoreState) {
                // Clear the state files because they are inconsistent and useless from now on.
                mTabModelSelectorImpl.clearState();
            } else if (!mIsOnFirstRun) {
                // State should be clear when we start first run and hence we do not need to load
                // a previous state. This may change the current Model, watch out for initialization
                // based on the model.
                // Never attempt to restore incognito tabs when this activity was previously swiped
                // away in Recents. http://crbug.com/626629
                boolean ignoreIncognitoFiles = !hadCipherData;
                mTabModelSelectorImpl.loadState(ignoreIncognitoFiles);
            }

            mIntentWithEffect = false;
            if ((mIsOnFirstRun || getSavedInstanceState() == null) && intent != null
                    && !mIntentHandler.shouldIgnoreIntent(ChromeTabbedActivity.this, intent)) {
                mIntentWithEffect = mIntentHandler.onNewIntent(ChromeTabbedActivity.this, intent);
            }

            mCreatedTabOnStartup = getCurrentTabModel().getCount() > 0
                    || mTabModelSelectorImpl.getRestoredTabCount() > 0
                    || mIntentWithEffect;

            // We always need to try to restore tabs. The set of tabs might be empty, but at least
            // it will trigger the notification that tab restore is complete which is needed by
            // other parts of Chrome such as sync.
            boolean activeTabBeingRestored = !mIntentWithEffect;
            mTabModelSelectorImpl.restoreTabs(activeTabBeingRestored);

            // Only create an initial tab if no tabs were restored and no intent was handled.
            // Also, check whether the active tab was supposed to be restored and that the total
            // tab count is now non zero.  If this is not the case, tab restore failed and we need
            // to create a new tab as well.
            if (!mCreatedTabOnStartup
                    || (activeTabBeingRestored && getTabModelSelector().getTotalTabCount() == 0)) {
                // If homepage URI is not determined, due to PartnerBrowserCustomizations provider
                // async reading, then create a tab at the async reading finished. If it takes
                // too long, just create NTP.
                PartnerBrowserCustomizations.setOnInitializeAsyncFinished(
                        new Runnable() {
                            @Override
                            public void run() {
                                createInitialTab();
                            }
                        }, INITIAL_TAB_CREATION_TIMEOUT_MS);
            }

            RecordHistogram.recordBooleanHistogram(
                    "MobileStartup.ColdStartupIntent", mIntentWithEffect);
        } finally {
            TraceEvent.end("ChromeTabbedActivity.initializeState");
        }
    }

    /**
     * Create an initial tab for cold start without restored tabs.
     */
    private void createInitialTab() {
        String url = HomepageManager.getHomepageUri(getApplicationContext());
        if (TextUtils.isEmpty(url)) url = UrlConstants.NTP_URL;
        getTabCreator(false).launchUrl(url, TabLaunchType.FROM_CHROME_UI);
    }

    @Override
    public boolean onActivityResultWithNative(int requestCode, int resultCode, Intent data) {
        if (super.onActivityResultWithNative(requestCode, resultCode, data)) return true;

        if (requestCode == FIRST_RUN_EXPERIENCE_RESULT) {
            mIsOnFirstRun = false;
            if (resultCode == RESULT_OK) {
                refreshSignIn();
            } else {
                if (data != null && data.getBooleanExtra(
                        FirstRunActivity.RESULT_CLOSE_APP, false)) {
                    getTabModelSelector().closeAllTabs(true);
                    finish();
                } else {
                    launchFirstRunExperience();
                }
            }
            return true;
        } else if (requestCode == CCT_RESULT) {
            Log.d(TAG, "Custom Tab result: " + resultCode);
            if (resultCode == CustomTabActivity.RESULT_STOPPED && data != null) {
                // Open the last URL shown in the CustomTabActivity.  Unfortunately, there isn't a
                // good TabLaunchType to use here (FROM_EXTERNAL_APP changes back behavior, e.g.),
                // but this shouldn't be a problem once Tab reparenting lands.
                //
                // TODO(dfalcantara): Use real tab reparenting here when possible.  We should fall
                //                    back to using the TabState file or URL, in that order.
                getTabCreator(false).createNewTab(new LoadUrlParams(
                        data.getDataString()), TabLaunchType.FROM_CHROME_UI, null);
            } else if ((resultCode == CustomTabActivity.RESULT_BACK_PRESSED
                    || resultCode == CustomTabActivity.RESULT_CLOSED) && data != null) {
                // Herb UMA should have already been recorded by the CustomTabActivity.
                Log.d(TAG, "Herb: Sending app to the background");
                mActivityStopMetrics.setStopReason(
                        ActivityStopMetrics.STOP_REASON_CUSTOM_TAB_STOPPED);
                sendToBackground(null);
            }
            return true;
        }
        return false;
    }

    @Override
    public void onAccessibilityModeChanged(boolean enabled) {
        super.onAccessibilityModeChanged(enabled);

        if (mLayoutManager != null) {
            mLayoutManager.setEnableAnimations(
                    DeviceClassManager.enableAnimations(getApplicationContext()));
        }
        if (isTablet()) {
            if (getCompositorViewHolder() != null) {
                getCompositorViewHolder().onAccessibilityStatusChanged(enabled);
            }
        }
        if (mLayoutManager != null && mLayoutManager.overviewVisible()
                && mIsAccessibilityEnabled != enabled) {
            mLayoutManager.hideOverview(false);
            if (getTabModelSelector().getCurrentModel().getCount() == 0) {
                getCurrentTabCreator().launchNTP();
            }
        }
        mIsAccessibilityEnabled = enabled;
    }

    /**
     * Internal class which performs the intent handling operations delegated by IntentHandler.
     */
    private class InternalIntentDelegate implements IntentHandler.IntentHandlerDelegate {
        /**
         * Processes a url view intent.
         *
         * @param url The url from the intent.
         */
        @Override
        public void processUrlViewIntent(String url, String referer, String headers,
                TabOpenType tabOpenType, String externalAppId, int tabIdToBringToFront,
                boolean hasUserGesture, Intent intent) {
            TabModel tabModel = getCurrentTabModel();
            boolean fromLauncherShortcut = IntentUtils.safeGetBooleanExtra(
                    intent, IntentHandler.EXTRA_INVOKED_FROM_SHORTCUT, false);
            switch (tabOpenType) {
                case REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB:
                    // Used by the bookmarks application.
                    if (tabModel.getCount() > 0 && mUIInitialized
                            && mLayoutManager.overviewVisible()) {
                        mLayoutManager.hideOverview(true);
                    }
                    mTabModelSelectorImpl.tryToRestoreTabStateForUrl(url);
                    int tabToBeClobberedIndex = TabModelUtils.getTabIndexByUrl(tabModel, url);
                    Tab tabToBeClobbered = tabModel.getTabAt(tabToBeClobberedIndex);
                    if (tabToBeClobbered != null) {
                        TabModelUtils.setIndex(tabModel, tabToBeClobberedIndex);
                        tabToBeClobbered.reload();
                        RecordUserAction.record("MobileTabClobbered");
                    } else {
                        launchIntent(url, referer, headers, externalAppId, true, intent);
                    }
                    RecordUserAction.record("MobileReceivedExternalIntent");
                    int shortcutSource = intent.getIntExtra(
                            ShortcutHelper.EXTRA_SOURCE, ShortcutSource.UNKNOWN);
                    LaunchMetrics.recordHomeScreenLaunchIntoTab(url, shortcutSource);
                    break;
                case BRING_TAB_TO_FRONT:
                    mTabModelSelectorImpl.tryToRestoreTabStateForId(tabIdToBringToFront);

                    int tabIndex = TabModelUtils.getTabIndexById(tabModel, tabIdToBringToFront);
                    if (tabIndex == TabModel.INVALID_TAB_INDEX) {
                        TabModel otherModel =
                                getTabModelSelector().getModel(!tabModel.isIncognito());
                        tabIndex = TabModelUtils.getTabIndexById(otherModel, tabIdToBringToFront);
                        if (tabIndex != TabModel.INVALID_TAB_INDEX) {
                            getTabModelSelector().selectModel(otherModel.isIncognito());
                            TabModelUtils.setIndex(otherModel, tabIndex);
                        } else {
                            Log.e(TAG, "Failed to bring tab to front because it doesn't exist.");
                            return;
                        }
                    } else {
                        TabModelUtils.setIndex(tabModel, tabIndex);
                    }
                    RecordUserAction.record("MobileReceivedExternalIntent");
                    break;
                case CLOBBER_CURRENT_TAB:
                    // The browser triggered the intent. This happens when clicking links which
                    // can be handled by other applications (e.g. www.youtube.com links).
                    Tab currentTab = getActivityTab();
                    if (currentTab != null) {
                        currentTab.getTabRedirectHandler().updateIntent(intent);
                        int transitionType = PageTransition.LINK | PageTransition.FROM_API;
                        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
                        loadUrlParams.setIntentReceivedTimestamp(mIntentHandlingTimeMs);
                        loadUrlParams.setHasUserGesture(hasUserGesture);
                        loadUrlParams.setTransitionType(IntentHandler.getTransitionTypeFromIntent(
                                getApplicationContext(), intent, transitionType));
                        currentTab.loadUrl(loadUrlParams);
                        RecordUserAction.record("MobileTabClobbered");
                    } else {
                        launchIntent(url, referer, headers, externalAppId, true, intent);
                    }
                    break;
                case REUSE_APP_ID_MATCHING_TAB_ELSE_NEW_TAB:
                    openNewTab(url, referer, headers, externalAppId, intent, false);
                    break;
                case OPEN_NEW_TAB:
                    if (fromLauncherShortcut) recordLauncherShortcutAction(false);
                    openNewTab(url, referer, headers, externalAppId, intent, true);
                    break;
                case OPEN_NEW_INCOGNITO_TAB:
                    if (url == null || url.equals(UrlConstants.NTP_URL)) {
                        TabLaunchType launchType;
                        if (fromLauncherShortcut) {
                            getTabCreator(true).launchUrl(
                                    UrlConstants.NTP_URL, TabLaunchType.FROM_EXTERNAL_APP);
                            recordLauncherShortcutAction(true);
                        } else if (TextUtils.equals(externalAppId, getPackageName())) {
                            // Used by the Account management screen to open a new incognito tab.
                            // Account management screen collects its metrics separately.
                            getTabCreator(true).launchUrl(
                                    UrlConstants.NTP_URL, TabLaunchType.FROM_CHROME_UI);
                        } else {
                            getTabCreator(true).launchUrl(
                                    UrlConstants.NTP_URL, TabLaunchType.FROM_EXTERNAL_APP);
                            RecordUserAction.record("MobileReceivedExternalIntent");
                        }
                    } else {
                        if (TextUtils.equals(externalAppId, getPackageName())) {
                            getTabCreator(true).launchUrl(
                                    url, TabLaunchType.FROM_LINK, intent, mIntentHandlingTimeMs);
                        } else {
                            getTabCreator(true).launchUrlFromExternalApp(url, referer, headers,
                                    externalAppId, true, intent, mIntentHandlingTimeMs);
                            RecordUserAction.record("MobileReceivedExternalIntent");
                        }
                    }
                    break;
                default:
                    assert false : "Unknown TabOpenType: " + tabOpenType;
                    break;
            }
            getToolbarManager().setUrlBarFocus(false);
        }

        @Override
        public void processWebSearchIntent(String query) {
            assert false;
        }

        /**
         * Opens a new Tab with the possibility of showing it in a Custom Tab, instead.
         *
         * See IntentHandler#processUrlViewIntent() for an explanation most of the parameters.
         * @param forceNewTab If not handled by a Custom Tab, forces the new tab to be created.
         */
        private void openNewTab(String url, String referer, String headers,
                String externalAppId, Intent intent, boolean forceNewTab) {
            boolean isAllowedToReturnToExternalApp = IntentUtils.safeGetBooleanExtra(intent,
                    ChromeLauncherActivity.EXTRA_IS_ALLOWED_TO_RETURN_TO_PARENT, true);

            String herbFlavor = FeatureUtilities.getHerbFlavor();
            if (isAllowedToReturnToExternalApp
                    && ChromeLauncherActivity.canBeHijackedByHerb(intent)
                    && TextUtils.equals(ChromeSwitches.HERB_FLAVOR_DILL, herbFlavor)) {
                Log.d(TAG, "Sending to CustomTabActivity");
                mActivityStopMetrics.setStopReason(
                        ActivityStopMetrics.STOP_REASON_CUSTOM_TAB_STARTED);

                Intent newIntent = ChromeLauncherActivity.createCustomTabActivityIntent(
                        ChromeTabbedActivity.this, intent, false);
                newIntent.putExtra(Browser.EXTRA_APPLICATION_ID, getPackageName());
                newIntent.putExtra(
                        CustomTabIntentDataProvider.EXTRA_IS_OPENED_BY_CHROME, true);
                ChromeLauncherActivity.updateHerbIntent(ChromeTabbedActivity.this, newIntent);

                // Launch the Activity on top of this task.
                int updatedFlags = newIntent.getFlags();
                updatedFlags &= ~Intent.FLAG_ACTIVITY_NEW_TASK;
                updatedFlags &= ~Intent.FLAG_ACTIVITY_NEW_DOCUMENT;
                newIntent.setFlags(updatedFlags);
                startActivityForResult(newIntent, CCT_RESULT);
            } else {
                // Create a new tab.
                Tab newTab =
                        launchIntent(url, referer, headers, externalAppId, forceNewTab, intent);
                newTab.setIsAllowedToReturnToExternalApp(isAllowedToReturnToExternalApp);
                RecordUserAction.record("MobileReceivedExternalIntent");
            }
        }
    }

    @Override
    public void preInflationStartup() {
        super.preInflationStartup();

        // Decide whether to record startup UMA histograms. This is done  early in the main
        // Activity.onCreate() to avoid recording navigation delays when they require user input to
        // proceed. For example, FRE (First Run Experience) happens before the activity is created,
        // and triggers initialization of the native library. At the moment it seems safe to assume
        // that uninitialized native library is an indication of an application start that is
        // followed by navigation immediately without user input.
        if (!LibraryLoader.isInitialized()) {
            UmaUtils.setRunningApplicationStart(true);
        }

        CommandLine commandLine = CommandLine.getInstance();
        if (commandLine.hasSwitch(ContentSwitches.ENABLE_TEST_INTENTS)
                && getIntent() != null
                && getIntent().hasExtra(
                        ChromeTabbedActivity.INTENT_EXTRA_TEST_RENDER_PROCESS_LIMIT)) {
            int value = getIntent().getIntExtra(
                    ChromeTabbedActivity.INTENT_EXTRA_TEST_RENDER_PROCESS_LIMIT, -1);
            if (value != -1) {
                String[] args = new String[1];
                args[0] = "--" + ContentSwitches.RENDER_PROCESS_LIMIT
                        + "=" + Integer.toString(value);
                commandLine.appendSwitchesAndArguments(args);
            }
        }

        supportRequestWindowFeature(Window.FEATURE_ACTION_MODE_OVERLAY);

        // We are starting from history with a URL after data has been cleared. On Samsung this
        // can happen after user clears data and clicks on a recents item on pre-L devices.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP
                && getIntent().getData() != null
                && (getIntent().getFlags() & Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY) != 0
                && OmahaClient.isFreshInstallOrDataHasBeenCleared(getApplicationContext())) {
            getIntent().setData(null);
        }
    }

    @Override
    protected int getControlContainerLayoutId() {
        return R.layout.control_container;
    }

    @Override
    public void postInflationStartup() {
        super.postInflationStartup();

        // Critical path for startup. Create the minimum objects needed
        // to allow a blank screen draw (without depending on any native code)
        // and then yield ASAP.
        createTabModelSelectorImpl(getSavedInstanceState());

        if (isFinishing()) return;

        // Don't show the keyboard until user clicks in.
        getWindow().setSoftInputMode(
                WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN
                | WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);

        mContentContainer = (ViewGroup) findViewById(android.R.id.content);
        mControlContainer = (ToolbarControlContainer) findViewById(R.id.control_container);

        mUndoBarPopupController = new UndoBarController(this, mTabModelSelectorImpl,
                getSnackbarManager());
    }

    @Override
    protected void initializeToolbar() {
        super.initializeToolbar();
        if (DeviceFormFactor.isTablet(getApplicationContext())) {
            getToolbarManager().setShouldUpdateToolbarPrimaryColor(false);
        }
    }

    /**
     * Launch the First Run flow to set up Chrome.
     * There are two different pathways that can occur:
     * 1) The First Run Experience activity is run, which walks the user through the ToS, signing
     * in, and turning on UMA reporting.  This happens in most cases.
     * 2) We automatically try to sign-in the user and skip the FRE activity, then ask the user to
     * turn on UMA reporting some time later using an InfoBar.  This happens if Chrome is opened
     * with an Intent to view a URL, or if we're on a Nexus device where the user has already
     * been exposed to the ToS and Privacy Notice.
     */
    private void launchFirstRunExperience() {
        if (mIsOnFirstRun) {
            mTabModelSelectorImpl.clearState();
            return;
        }

        if (getIntent() != null && getIntent().getBooleanExtra(SKIP_FIRST_RUN_EXPERIENCE, false)) {
            return;
        }

        final Intent freIntent =
                FirstRunFlowSequencer.checkIfFirstRunIsNecessary(this, getIntent(), false);
        if (freIntent == null) return;

        mIsOnFirstRun = true;

        // TODO(dtrainor): Investigate this further and revert once Android pushes fix?
        // Posting this due to Android bug where we apparently are stopping a
        // non-resumed activity.  That statement looks incorrect, but need to not hit
        // the runtime exception here.
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                startActivityForResult(freIntent, FIRST_RUN_EXPERIENCE_RESULT);
            }
        });
    }

    @Override
    protected void onDeferredStartup() {
        super.onDeferredStartup();
        DeferredStartupHandler.getInstance().addDeferredTask(new Runnable() {
            @Override
            public void run() {
                ActivityManager am =
                        (ActivityManager) getSystemService(Context.ACTIVITY_SERVICE);
                RecordHistogram.recordSparseSlowlyHistogram(
                        "MemoryAndroid.DeviceMemoryClass", am.getMemoryClass());

                AutocompleteController.nativePrefetchZeroSuggestResults();
            }
        });
    }

    private void createTabModelSelectorImpl(Bundle savedInstanceState) {
        // We determine the model as soon as possible so every systems get initialized coherently.
        boolean startIncognito = savedInstanceState != null
                && savedInstanceState.getBoolean("is_incognito_selected", false);
        int index = savedInstanceState != null ? savedInstanceState.getInt(WINDOW_INDEX, 0) : 0;
        mTabModelSelectorImpl = (TabModelSelectorImpl)
                TabWindowManager.getInstance().requestSelector(this, getWindowAndroid(), index);
        if (mTabModelSelectorImpl == null) {
            Toast.makeText(this, getString(R.string.unsupported_number_of_windows),
                    Toast.LENGTH_LONG).show();
            finish();
            return;
        }
        setTabCreators(
                new ChromeTabCreator(this, getWindowAndroid(), false),
                new ChromeTabCreator(this, getWindowAndroid(), true));
        mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(mTabModelSelectorImpl) {

            private boolean mIsFirstPageLoadStart = true;

            @Override
            public void onPageLoadStarted(Tab tab, String url) {
                // Discard startup navigation measurements when the user interfered and started the
                // 2nd navigation (in activity lifetime) in parallel.
                if (!mIsFirstPageLoadStart) {
                    UmaUtils.setRunningApplicationStart(false);
                } else {
                    mIsFirstPageLoadStart = false;
                }
            }

            @Override
            public void onDidNavigateMainFrame(Tab tab, String url, String baseUrl,
                    boolean isNavigationToDifferentPage, boolean isFragmentNavigation,
                    int statusCode) {
                DataReductionPromoInfoBar.maybeLaunchPromoInfoBar(ChromeTabbedActivity.this,
                        tab.getWebContents(), url, tab.isShowingErrorPage(), isFragmentNavigation,
                        statusCode);
            }
        };

        if (startIncognito) mTabModelSelectorImpl.selectModel(true);
        setTabModelSelector(mTabModelSelectorImpl);
    }

    @Override
    public void terminateIncognitoSession() {
        getTabModelSelector().getModel(true).closeAllTabs();
    }

    @Override
    public boolean onMenuOrKeyboardAction(final int id, boolean fromMenu) {
        final Tab currentTab = getActivityTab();
        if (id == R.id.move_to_other_window_menu_id) {
            if (currentTab != null) moveTabToOtherWindow(currentTab);
        } else if (id == R.id.new_tab_menu_id) {
            RecordUserAction.record("MobileMenuNewTab");
            RecordUserAction.record("MobileNewTabOpened");
            getTabCreator(false).launchUrl(UrlConstants.NTP_URL, TabLaunchType.FROM_CHROME_UI);
        } else if (id == R.id.new_incognito_tab_menu_id) {
            if (PrefServiceBridge.getInstance().isIncognitoModeEnabled()) {
                // This action must be recorded before opening the incognito tab since UMA actions
                // are dropped when an incognito tab is open.
                RecordUserAction.record("MobileMenuNewIncognitoTab");
                RecordUserAction.record("MobileNewTabOpened");
                getTabCreator(true).launchUrl(UrlConstants.NTP_URL, TabLaunchType.FROM_CHROME_UI);
            }
        } else if (id == R.id.all_bookmarks_menu_id) {
            if (currentTab != null) {
                getCompositorViewHolder().hideKeyboard(new Runnable() {
                    @Override
                    public void run() {
                        StartupMetrics.getInstance().recordOpenedBookmarks();
                        BookmarkUtils.showBookmarkManager(ChromeTabbedActivity.this);
                    }
                });
                RecordUserAction.record("MobileMenuAllBookmarks");
            }
        } else if (id == R.id.recent_tabs_menu_id) {
            if (currentTab != null) {
                currentTab.loadUrl(new LoadUrlParams(
                        UrlConstants.RECENT_TABS_URL,
                        PageTransition.AUTO_BOOKMARK));
                RecordUserAction.record("MobileMenuOpenTabs");
            }
        } else if (id == R.id.close_all_tabs_menu_id) {
            // Close both incognito and normal tabs
            getTabModelSelector().closeAllTabs();
            RecordUserAction.record("MobileMenuCloseAllTabs");
        } else if (id == R.id.close_all_incognito_tabs_menu_id) {
            // Close only incognito tabs
            getTabModelSelector().getModel(true).closeAllTabs();
            // TODO(nileshagrawal) Record unique action for this. See bug http://b/5542946.
            RecordUserAction.record("MobileMenuCloseAllTabs");
        } else if (id == R.id.find_in_page_id) {
            mFindToolbarManager.showToolbar();
            if (getContextualSearchManager() != null) {
                getContextualSearchManager().hideContextualSearch(StateChangeReason.UNKNOWN);
            }
            if (fromMenu) {
                RecordUserAction.record("MobileMenuFindInPage");
            } else {
                RecordUserAction.record("MobileShortcutFindInPage");
            }
        } else if (id == R.id.focus_url_bar) {
            boolean isUrlBarVisible = !mLayoutManager.overviewVisible()
                    && (!isTablet() || getCurrentTabModel().getCount() != 0);
            if (isUrlBarVisible) {
                getToolbarManager().setUrlBarFocus(true);
            }
        } else if (id == R.id.downloads_menu_id) {
            DownloadUtils.showDownloadManager(this, currentTab);
            RecordUserAction.record("MobileMenuDownloadManager");
        } else if (id == R.id.open_recently_closed_tab) {
            TabModel currentModel = mTabModelSelectorImpl.getCurrentModel();
            if (!currentModel.isIncognito()) currentModel.openMostRecentlyClosedTab();
            RecordUserAction.record("MobileTabClosedUndoShortCut");
        } else {
            return super.onMenuOrKeyboardAction(id, fromMenu);
        }
        return true;
    }

    private void recordBackPressedUma(String logMessage, @BackPressedResult int action) {
        Log.i(TAG, "Back pressed: " + logMessage);
        RecordHistogram.recordEnumeratedHistogram(
                "Android.Activity.ChromeTabbedActivity.SystemBackAction",
                action, BACK_PRESSED_COUNT);
    }

    private void recordLauncherShortcutAction(boolean isIncognito) {
        if (isIncognito) {
            RecordUserAction.record("Android.LauncherShortcut.NewIncognitoTab");
        } else {
            RecordUserAction.record("Android.LauncherShortcut.NewTab");
        }
    }

    private void moveTabToOtherWindow(Tab tab) {
        Class<? extends Activity> targetActivity =
                MultiWindowUtils.getInstance().getOpenInOtherWindowActivity(this);
        if (targetActivity == null) return;

        Intent intent = new Intent(this, targetActivity);
        MultiWindowUtils.setOpenInOtherWindowIntentExtras(intent, this, targetActivity);

        tab.detachAndStartReparenting(intent, null, null, true);
    }

    @Override
    public boolean handleBackPressed() {
        if (!mUIInitialized) return false;
        final Tab currentTab = getActivityTab();

        if (currentTab == null) {
            recordBackPressedUma("currentTab is null", BACK_PRESSED_TAB_IS_NULL);
            moveTaskToBack(true);
            return true;
        }

        // If we are in overview mode and not a tablet, then leave overview mode on back.
        if (mLayoutManager.overviewVisible() && !isTablet()) {
            recordBackPressedUma("Hid overview", BACK_PRESSED_EXITED_TAB_SWITCHER);
            mLayoutManager.hideOverview(true);
            return true;
        }

        if (exitFullscreenIfShowing()) {
            recordBackPressedUma("Exited fullscreen", BACK_PRESSED_EXITED_FULLSCREEN);
            return true;
        }

        if (getToolbarManager().back()) {
            recordBackPressedUma("Navigating backward", BACK_PRESSED_NAVIGATED_BACK);
            RecordUserAction.record("MobileTabClobbered");
            return true;
        }

        // If the current tab url is HELP_URL, then the back button should close the tab to
        // get back to the previous state. The reason for startsWith check is that the
        // actual redirected URL is a different system language based help url.
        final TabLaunchType type = currentTab.getLaunchType();
        final boolean helpUrl = currentTab.getUrl().startsWith(HELP_URL_PREFIX);
        if (type == TabLaunchType.FROM_CHROME_UI && helpUrl) {
            getCurrentTabModel().closeTab(currentTab);
            recordBackPressedUma("Closed tab for help URL", BACK_PRESSED_HELP_URL_CLOSED);
            return true;
        }

        // The current spec says that tabs are closed only when there is NO "X" visible, i.e. when
        // the tab is NOT allowed to return to the external app.
        boolean isAllowedToCloseTab = true;
        String herbFlavor = FeatureUtilities.getHerbFlavor();
        if (TextUtils.equals(ChromeSwitches.HERB_FLAVOR_BASIL, herbFlavor)
                || TextUtils.equals(ChromeSwitches.HERB_FLAVOR_CHIVE, herbFlavor)) {
            isAllowedToCloseTab = !currentTab.isAllowedToReturnToExternalApp();
        }

        // [true]: Reached the bottom of the back stack on a tab the user did not explicitly
        // create (i.e. it was created by an external app or opening a link in background, etc).
        // [false]: Reached the bottom of the back stack on a tab that the user explicitly
        // created (e.g. selecting "new tab" from menu).
        final int parentId = currentTab.getParentId();
        final boolean shouldCloseTab = type == TabLaunchType.FROM_LINK
                || (type == TabLaunchType.FROM_EXTERNAL_APP && isAllowedToCloseTab)
                || type == TabLaunchType.FROM_LONGPRESS_FOREGROUND
                || type == TabLaunchType.FROM_LONGPRESS_BACKGROUND
                || (type == TabLaunchType.FROM_RESTORE && parentId != Tab.INVALID_TAB_ID);

        // Minimize the app if either:
        // - we decided not to close the tab
        // - we decided to close the tab, but it was opened by an external app, so we will go
        //   exit Chrome on top of closing the tab
        final boolean minimizeApp = !shouldCloseTab || currentTab.isCreatedForExternalApp();
        if (minimizeApp) {
            if (shouldCloseTab) {
                recordBackPressedUma("Minimized and closed tab", BACK_PRESSED_MINIMIZED_TAB_CLOSED);
                mActivityStopMetrics.setStopReason(ActivityStopMetrics.STOP_REASON_BACK_BUTTON);
                sendToBackground(currentTab);
                return true;
            } else {
                recordBackPressedUma("Minimized, kept tab", BACK_PRESSED_MINIMIZED_NO_TAB_CLOSED);
                mActivityStopMetrics.setStopReason(ActivityStopMetrics.STOP_REASON_BACK_BUTTON);
                sendToBackground(null);
                return true;
            }
        } else if (shouldCloseTab) {
            recordBackPressedUma("Tab closed", BACK_PRESSED_TAB_CLOSED);
            getCurrentTabModel().closeTab(currentTab, true, false, false);
            return true;
        }

        assert false : "The back button should have already been handled by this point";
        recordBackPressedUma("Unhandled", BACK_PRESSED_NOTHING_HAPPENED);
        return false;
    }

    /**
     * Sends this Activity to the background.
     *
     * @param tabToClose Tab that will be closed once the app is not visible.
     */
    private void sendToBackground(@Nullable final Tab tabToClose) {
        Log.i(TAG, "sendToBackground(): " + tabToClose);
        moveTaskToBack(true);
        if (tabToClose != null) {
            // In the case of closing a tab upon minimization, don't allow the close action to
            // happen until after our app is minimized to make sure we don't get a brief glimpse of
            // the newly active tab before we exit Chrome.
            //
            // If the runnable doesn't run before the Activity dies, Chrome won't crash but the tab
            // won't be closed (crbug.com/587565).
            mHandler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    boolean hasNextTab =
                            getCurrentTabModel().getNextTabIfClosed(tabToClose.getId()) != null;
                    getCurrentTabModel().closeTab(tabToClose, false, true, false);

                    // If there is no next tab to open, enter overview mode.
                    if (!hasNextTab) mLayoutManager.showOverview(false);
                }
            }, CLOSE_TAB_ON_MINIMIZE_DELAY_MS);
        }
    }

    /**
     * Launch a URL from an intent.
     *
     * @param url           The url from the intent.
     * @param referer       Optional referer URL to be used.
     * @param headers       Optional headers to be sent when opening the URL.
     * @param externalAppId External app id.
     * @param forceNewTab   Whether to force the URL to be launched in a new tab or to fall
     *                      back to the default behavior for making that determination.
     * @param intent        The original intent.
     */
    private Tab launchIntent(String url, String referer, String headers,
            String externalAppId, boolean forceNewTab, Intent intent) {
        if (mUIInitialized) {
            mLayoutManager.hideOverview(false);
            getToolbarManager().finishAnimations();
        }
        if (TextUtils.equals(externalAppId, getPackageName())) {
            // If the intent was launched by chrome, open the new tab in the appropriate model.
            // Using FROM_LINK ensures the tab is parented to the current tab, which allows
            // the back button to close these tabs and restore selection to the previous tab.
            boolean isIncognito = IntentUtils.safeGetBooleanExtra(intent,
                    IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false);
            boolean fromLauncherShortcut = IntentUtils.safeGetBooleanExtra(
                    intent, IntentHandler.EXTRA_INVOKED_FROM_SHORTCUT, false);
            LoadUrlParams loadUrlParams = new LoadUrlParams(url);
            loadUrlParams.setIntentReceivedTimestamp(mIntentHandlingTimeMs);
            loadUrlParams.setVerbatimHeaders(headers);
            return getTabCreator(isIncognito).createNewTab(
                    loadUrlParams,
                    fromLauncherShortcut ? TabLaunchType.FROM_EXTERNAL_APP
                            : TabLaunchType.FROM_LINK,
                    null,
                    intent);
        } else {
            return getTabCreator(false).launchUrlFromExternalApp(url, referer, headers,
                    externalAppId, forceNewTab, intent, mIntentHandlingTimeMs);
        }
    }

    private void toggleOverview() {
        Tab currentTab = getActivityTab();
        ContentViewCore contentViewCore =
                currentTab != null ? currentTab.getContentViewCore() : null;

        if (!mLayoutManager.overviewVisible()) {
            getCompositorViewHolder().hideKeyboard(new Runnable() {
                @Override
                public void run() {
                    mLayoutManager.showOverview(true);
                }
            });
            if (contentViewCore != null) {
                contentViewCore.setAccessibilityState(false);
            }
        } else {
            Layout activeLayout = mLayoutManager.getActiveLayout();
            if (activeLayout instanceof StackLayout) {
                ((StackLayout) activeLayout).commitOutstandingModelState(LayoutManager.time());
            }
            if (getCurrentTabModel().getCount() != 0) {
                // Don't hide overview if current tab stack is empty()
                mLayoutManager.hideOverview(true);

                // hideOverview could change the current tab.  Update the local variables.
                currentTab = getActivityTab();
                contentViewCore = currentTab != null ? currentTab.getContentViewCore() : null;

                if (contentViewCore != null) {
                    contentViewCore.setAccessibilityState(true);
                }
            }
        }
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        CipherFactory.getInstance().saveToBundle(outState);
        outState.putBoolean("is_incognito_selected", getCurrentTabModel().isIncognito());
        outState.putBoolean(FRE_RUNNING, mIsOnFirstRun);
        outState.putInt(WINDOW_INDEX,
                TabWindowManager.getInstance().getIndexForWindow(this));
    }

    @Override
    public void onDestroyInternal() {
        if (mLayoutManager != null) mLayoutManager.removeOverviewModeObserver(this);

        if (mTabModelSelectorTabObserver != null) {
            mTabModelSelectorTabObserver.destroy();
            mTabModelSelectorTabObserver = null;
        }

        if (mTabModelObserver != null) {
            for (TabModel model : mTabModelSelectorImpl.getModels()) {
                model.removeObserver(mTabModelObserver);
            }
        }

        if (mUndoBarPopupController != null) {
            mUndoBarPopupController.destroy();
            mUndoBarPopupController = null;
        }

        super.onDestroyInternal();
    }

    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
        // The conditions are expressed using ranges to capture intermediate levels possibly added
        // to the API in the future.
        if ((level >= TRIM_MEMORY_RUNNING_LOW && level < TRIM_MEMORY_UI_HIDDEN)
                || level >= TRIM_MEMORY_MODERATE) {
            NativePageAssassin.getInstance().freezeAllHiddenPages();
        }
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        Boolean result = KeyboardShortcuts.dispatchKeyEvent(event, this, mUIInitialized);
        return result != null ? result : super.dispatchKeyEvent(event);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (!mUIInitialized) {
            return super.onKeyDown(keyCode, event);
        }
        boolean isCurrentTabVisible = !mLayoutManager.overviewVisible()
                && (!isTablet() || getCurrentTabModel().getCount() != 0);
        return KeyboardShortcuts.onKeyDown(event, this, isCurrentTabVisible, true)
                || super.onKeyDown(keyCode, event);
    }

    @VisibleForTesting
    public View getTabsView() {
        return getCompositorViewHolder();
    }

    @VisibleForTesting
    public LayoutManagerChrome getLayoutManager() {
        return (LayoutManagerChrome) getCompositorViewHolder().getLayoutManager();
    }

    @VisibleForTesting
    public Layout getOverviewListLayout() {
        return getLayoutManager().getOverviewListLayout();
    }

    @Override
    public boolean isOverlayVisible() {
        return getCompositorViewHolder() != null && !getCompositorViewHolder().isTabInteractive();
    }

    // App Menu related code -----------------------------------------------------------------------

    @Override
    public boolean shouldShowAppMenu() {
        // The popup menu relies on the model created during the full UI initialization, so do not
        // attempt to show the menu until the UI creation has finished.
        if (!mUIInitialized) return false;

        // Do not show the menu if we are in find in page view.
        if (mFindToolbarManager != null && mFindToolbarManager.isShowing() && !isTablet()) {
            return false;
        }

        return super.shouldShowAppMenu();
    }

    @Override
    protected void showAppMenuForKeyboardEvent() {
        if (!mUIInitialized || isFullscreenVideoPlaying()) return;
        super.showAppMenuForKeyboardEvent();
    }

    private boolean isFullscreenVideoPlaying() {
        View view = ContentVideoView.getContentVideoView();
        return view != null && view.getContext() == this;
    }

    @Override
    public boolean isInOverviewMode() {
        return mLayoutManager != null && mLayoutManager.overviewVisible();
    }

    @Override
    protected IntentHandlerDelegate createIntentHandlerDelegate() {
        return new InternalIntentDelegate();
    }

    @Override
    public void onSceneChange(Layout layout) {
        super.onSceneChange(layout);
        if (!layout.shouldDisplayContentOverlay()) mTabModelSelectorImpl.onTabsViewShown();
    }

    @Override
    public void onOverviewModeStartedShowing(boolean showToolbar) {
        if (mFindToolbarManager != null) mFindToolbarManager.hideToolbar();
        if (getAssistStatusHandler() != null) getAssistStatusHandler().updateAssistState();
        if (getAppMenuHandler() != null) getAppMenuHandler().hideAppMenu();
        ApiCompatibilityUtils.setStatusBarColor(getWindow(), Color.BLACK);
        StartupMetrics.getInstance().recordOpenedTabSwitcher();
    }

    @Override
    public void onOverviewModeFinishedShowing() {}

    @Override
    public void onOverviewModeStartedHiding(boolean showToolbar, boolean delayAnimation) {
        if (getAppMenuHandler() != null) getAppMenuHandler().hideAppMenu();
    }

    @Override
    public void onOverviewModeFinishedHiding() {
        if (getAssistStatusHandler() != null) getAssistStatusHandler().updateAssistState();
        if (getActivityTab() != null) {
            setStatusBarColor(getActivityTab(), getActivityTab().getThemeColor());
        }
    }

    @Override
    protected void setStatusBarColor(Tab tab, int color) {
        if (DeviceFormFactor.isTablet(getApplicationContext())) return;
        super.setStatusBarColor(tab, isInOverviewMode() ? Color.BLACK : color);
    }

    @Override
    public void onMultiWindowModeChanged(boolean isInMultiWindowMode) {
        super.onMultiWindowModeChanged(isInMultiWindowMode);
        if (!FeatureUtilities.isTabModelMergingEnabled()) return;
        if (!isInMultiWindowMode) {
            // If the activity is currently resumed when multi-window mode is exited, try to merge
            // tabs from the other activity instance.
            if (ApplicationStatus.getStateForActivity(this) == ActivityState.RESUMED) {
                maybeMergeTabs();
            } else {
                mMergeTabsOnResume = true;
            }
        }
    }

    /**
     * Writes the tab state to disk.
     */
    @VisibleForTesting
    public void saveState() {
        mTabModelSelectorImpl.saveState();
    }

    /**
     * Merges tabs from a second ChromeTabbedActivity instance if necesssary and calls
     * finishAndRemoveTask() on the other activity.
     */
    @TargetApi(Build.VERSION_CODES.M)
    @VisibleForTesting
    public void maybeMergeTabs() {
        if (!FeatureUtilities.isTabModelMergingEnabled()) return;

        Class<?> otherWindowActivityClass =
                MultiWindowUtils.getInstance().getOpenInOtherWindowActivity(this);

        // 1. Find the other activity's task if it's still running so that it can be removed from
        //    Android recents.
        ActivityManager activityManager = (ActivityManager) getSystemService(ACTIVITY_SERVICE);
        List<ActivityManager.AppTask> appTasks = activityManager.getAppTasks();
        ActivityManager.AppTask otherActivityTask = null;
        for (ActivityManager.AppTask task : appTasks) {
            if (task.getTaskInfo() == null || task.getTaskInfo().baseActivity == null) continue;
            String baseActivity = task.getTaskInfo().baseActivity.getClassName();
            if (baseActivity.equals(otherWindowActivityClass.getName())) {
                otherActivityTask = task;
            }
        }

        if (otherActivityTask != null) {
            for (WeakReference<Activity> activityRef : ApplicationStatus.getRunningActivities()) {
                Activity activity = activityRef.get();
                if (activity == null) continue;
                // 2. If the other activity is still running (not destroyed), save its tab list.
                //    Saving the tab list prevents missing tabs or duplicate tabs if tabs have been
                //    reparented.
                // TODO(twellington): saveState() gets called in onStopWithNative() after the merge
                // starts, causing some duplicate work to be done. Avoid the redundancy.
                if (activity.getClass().equals(otherWindowActivityClass)) {
                    ((ChromeTabbedActivity) activity).saveState();
                    break;
                }
            }
            // 3. Kill the other activity's task to remove it from Android recents.
            otherActivityTask.finishAndRemoveTask();
        }

        // 4. Ask TabPersistentStore to merge state.
        RecordUserAction.record("Android.MergeState.Live");
        mTabModelSelectorImpl.mergeState();
    }
}
