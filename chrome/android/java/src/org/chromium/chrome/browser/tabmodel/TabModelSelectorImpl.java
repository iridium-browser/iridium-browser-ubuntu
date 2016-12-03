// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.os.Handler;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.ntp.NativePageFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * This class manages all the ContentViews in the app.  As it manipulates views, it must be
 * instantiated and used in the UI Thread.  It acts as a TabModel which delegates all
 * TabModel methods to the active model that it contains.
 */
public class TabModelSelectorImpl extends TabModelSelectorBase implements TabModelDelegate {
    public static final int CUSTOM_TABS_SELECTOR_INDEX = -1;

    private final ChromeActivity mActivity;

    /** Flag set to false when the asynchronous loading of tabs is finished. */
    private final AtomicBoolean mSessionRestoreInProgress =
            new AtomicBoolean(true);
    private final TabPersistentStore mTabSaver;

    // This flag signifies the object has gotten an onNativeReady callback and
    // has not been destroyed.
    private boolean mActiveState = false;

    private boolean mIsUndoSupported;

    private final TabModelOrderController mOrderController;

    private OverviewModeBehavior mOverviewModeBehavior;

    private TabContentManager mTabContentManager;

    private Tab mVisibleTab;

    private final TabModelSelectorUma mUma;

    private CloseAllTabsDelegate mCloseAllTabsDelegate;

    /**
     * Builds a {@link TabModelSelectorImpl} instance.
     * @param activity      The {@link ChromeActivity} this model selector lives in.
     * @param windowAndroid The {@link WindowAndroid} associated with this model selector.
     * @param supportUndo   Whether a tab closure can be undone.
     */
    public TabModelSelectorImpl(ChromeActivity activity, TabPersistencePolicy persistencePolicy,
            WindowAndroid windowAndroid, boolean supportUndo) {
        super();
        mActivity = activity;
        mUma = new TabModelSelectorUma(mActivity);
        final TabPersistentStoreObserver persistentStoreObserver =
                new TabPersistentStoreObserver() {
            @Override
            public void onStateLoaded() {
                markTabStateInitialized();
            }

            @Override
            public void onStateMerged() {
            }

            @Override
            public void onDetailsRead(int index, int id, String url, boolean isStandardActiveIndex,
                    boolean isIncognitoActiveIndex) {
            }

            @Override
            public void onInitialized(int tabCountAtStartup) {
                RecordHistogram.recordCountHistogram("Tabs.CountAtStartup", tabCountAtStartup);
            }

            @Override
            public void onMetadataSavedAsynchronously() {
            }
        };
        mIsUndoSupported = supportUndo;
        // Merge tabs if this is the TabModelSelector for ChromeTabbedActivity and there are no
        // other instances running. This indicates that it is a complete cold start of
        // ChromeTabbedActivity. Tabs should only be merged during a cold start of
        // ChromeTabbedActivity and not other instances (e.g. ChromeTabbedActivity2).
        boolean mergeTabs = FeatureUtilities.isTabModelMergingEnabled()
                && mActivity.getClass().equals(ChromeTabbedActivity.class)
                && TabWindowManager.getInstance().getNumberOfAssignedTabModelSelectors() == 0;

        mTabSaver = new TabPersistentStore(persistencePolicy, this, mActivity,
                persistentStoreObserver, mergeTabs);
        mOrderController = new TabModelOrderController(this);
    }

    @Override
    public void markTabStateInitialized() {
        super.markTabStateInitialized();
        if (!mSessionRestoreInProgress.getAndSet(false)) return;

        // This is the first time we set
        // |mSessionRestoreInProgress|, so we need to broadcast.
        TabModelImpl model = (TabModelImpl) getModel(false);

        if (model != null) {
            model.broadcastSessionRestoreComplete();
        } else {
            assert false : "Normal tab model is null after tab state loaded.";
        }
    }

    private void handleOnPageLoadStopped(Tab tab) {
        if (tab != null) mTabSaver.addTabToSaveQueue(tab);
    }

    /**
     *
     * @param overviewModeBehavior The {@link OverviewModeBehavior} that should be used to determine
     *                             when the app is in overview mode or not.
     */
    public void setOverviewModeBehavior(OverviewModeBehavior overviewModeBehavior) {
        assert overviewModeBehavior != null;
        mOverviewModeBehavior = overviewModeBehavior;
    }

    /**
     * Should be called when the app starts showing a view with multiple tabs.
     */
    public void onTabsViewShown() {
        mUma.onTabsViewShown();
    }

    /**
     * Should be called once the native library is loaded so that the actual internals of this
     * class can be initialized.
     * @param tabContentProvider                      A {@link TabContentManager} instance.
     */
    public void onNativeLibraryReady(TabContentManager tabContentProvider) {
        assert !mActiveState : "onNativeLibraryReady called twice!";
        mTabContentManager = tabContentProvider;

        ChromeTabCreator regularTabCreator = (ChromeTabCreator) mActivity.getTabCreator(false);
        ChromeTabCreator incognitoTabCreator = (ChromeTabCreator) mActivity.getTabCreator(true);
        TabModelImpl normalModel = new TabModelImpl(false, regularTabCreator, incognitoTabCreator,
                mUma, mOrderController, mTabContentManager, mTabSaver, this, mIsUndoSupported);
        TabModel incognitoModel = new OffTheRecordTabModel(new OffTheRecordTabModelImplCreator(
                regularTabCreator, incognitoTabCreator, mUma, mOrderController,
                mTabContentManager, mTabSaver, this));
        initialize(isIncognitoSelected(), normalModel, incognitoModel);
        regularTabCreator.setTabModel(normalModel, mOrderController, mTabContentManager);
        incognitoTabCreator.setTabModel(incognitoModel, mOrderController, mTabContentManager);
        mTabSaver.setTabContentManager(mTabContentManager);

        addObserver(new EmptyTabModelSelectorObserver() {
            @Override
            public void onNewTabCreated(Tab tab) {
                // Only invalidate if the tab exists in the currently selected model.
                if (TabModelUtils.getTabById(getCurrentModel(), tab.getId()) != null) {
                    mTabContentManager.invalidateIfChanged(tab.getId(), tab.getUrl());
                }

                if (tab.hasPendingLoadParams()) mTabSaver.addTabToSaveQueue(tab);
            }
        });

        mActiveState = true;

        new TabModelSelectorTabObserver(this) {
            @Override
            public void onUrlUpdated(Tab tab) {
                TabModel model = getModelForTabId(tab.getId());
                if (model == getCurrentModel()) {
                    mTabContentManager.invalidateIfChanged(tab.getId(), tab.getUrl());
                }
            }

            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                handleOnPageLoadStopped(tab);
            }

            @Override
            public void onPageLoadStarted(Tab tab, String url) {
                String previousUrl = tab.getUrl();
                if (NativePageFactory.isNativePageUrl(previousUrl, tab.isIncognito())) {
                    mTabContentManager.invalidateTabThumbnail(tab.getId(), previousUrl);
                } else {
                    mTabContentManager.removeTabThumbnail(tab.getId());
                }
            }

            @Override
            public void onPageLoadFinished(Tab tab) {
                mUma.onPageLoadFinished(tab.getId());
            }

            @Override
            public void onPageLoadFailed(Tab tab, int errorCode) {
                mUma.onPageLoadFailed(tab.getId());
            }

            @Override
            public void onCrash(Tab tab, boolean sadTabShown) {
                if (sadTabShown) {
                    mTabContentManager.removeTabThumbnail(tab.getId());
                }
                mUma.onTabCrashed(tab.getId());
            }
        };
    }

    /**
     * Exposed to allow tests to initialize the selector with different tab models.
     * @param normalModel The normal tab model.
     * @param incognitoModel The incognito tab model.
     */
    @VisibleForTesting
    public void initializeForTesting(TabModel normalModel, TabModel incognitoModel) {
        initialize(isIncognitoSelected(), normalModel, incognitoModel);
        mActiveState = true;
    }

    @Override
    public void setCloseAllTabsDelegate(CloseAllTabsDelegate delegate) {
        mCloseAllTabsDelegate = delegate;
    }

    @Override
    public TabModel getModelAt(int index) {
        return mActiveState ? super.getModelAt(index) : EmptyTabModel.getInstance();
    }

    @Override
    public void selectModel(boolean incognito) {
        TabModel oldModel = getCurrentModel();
        super.selectModel(incognito);
        TabModel newModel = getCurrentModel();
        if (oldModel != newModel) {
            TabModelUtils.setIndex(newModel, newModel.index());

            // Make the call to notifyDataSetChanged() after any delayed events
            // have had a chance to fire. Otherwise, this may result in some
            // drawing to occur before animations have a chance to work.
            new Handler().post(new Runnable() {
                @Override
                public void run() {
                    notifyChanged();
                }
            });
        }
    }

    /**
     * Commits all pending tab closures for all {@link TabModel}s in this {@link TabModelSelector}.
     */
    @Override
    public void commitAllTabClosures() {
        for (int i = 0; i < getModels().size(); i++) {
            getModelAt(i).commitAllTabClosures();
        }
    }

    @Override
    public boolean closeAllTabsRequest(boolean incognito) {
        return mCloseAllTabsDelegate.closeAllTabsRequest(incognito);
    }

    public void saveState() {
        commitAllTabClosures();
        mTabSaver.saveState();
    }

    /**
     * Load the saved tab state. This should be called before any new tabs are created. The saved
     * tabs shall not be restored until {@link #restoreTabs} is called.
     * @param ignoreIncognitoFiles Whether to skip loading incognito tabs.
     */
    public void loadState(boolean ignoreIncognitoFiles) {
        mTabSaver.loadState(ignoreIncognitoFiles);
    }

    /**
     * Merges the tab states from two tab models.
     */
    public void mergeState() {
        mTabSaver.mergeState();
    }

    /**
     * Restore the saved tabs which were loaded by {@link #loadState}.
     *
     * @param setActiveTab If true, synchronously load saved active tab and set it as the current
     *                     active tab.
     */
    public void restoreTabs(boolean setActiveTab) {
        mTabSaver.restoreTabs(setActiveTab);
    }

    /**
     * If there is an asynchronous session restore in-progress, try to synchronously restore
     * the state of a tab with the given url as a frozen tab. This method has no effect if
     * there isn't a tab being restored with this url, or the tab has already been restored.
     */
    public void tryToRestoreTabStateForUrl(String url) {
        if (isSessionRestoreInProgress()) mTabSaver.restoreTabStateForUrl(url);
    }

    /**
     * If there is an asynchronous session restore in-progress, try to synchronously restore
     * the state of a tab with the given id as a frozen tab. This method has no effect if
     * there isn't a tab being restored with this id, or the tab has already been restored.
     */
    public void tryToRestoreTabStateForId(int id) {
        if (isSessionRestoreInProgress()) mTabSaver.restoreTabStateForId(id);
    }

    public void clearState() {
        mTabSaver.clearState();
    }

    @Override
    public void destroy() {
        mTabSaver.destroy();
        mUma.destroy();
        super.destroy();
        mActiveState = false;
    }

    @Override
    public Tab openNewTab(LoadUrlParams loadUrlParams, TabLaunchType type, Tab parent,
            boolean incognito) {
        return mActivity.getTabCreator(incognito).createNewTab(loadUrlParams, type, parent);
    }

    /**
     * @return Number of restored tabs on cold startup.
     */
    public int getRestoredTabCount() {
        return mTabSaver.getRestoredTabCount();
    }

    @Override
    public void requestToShowTab(Tab tab, TabSelectionType type) {
        boolean isFromExternalApp = tab != null
                && tab.getLaunchType() == TabLaunchType.FROM_EXTERNAL_APP;

        if (mVisibleTab != tab && tab != null && !tab.isNativePage()) {
            TabModelImpl.startTabSwitchLatencyTiming(type);
        }
        if (mVisibleTab != null && mVisibleTab != tab && !mVisibleTab.needsReload()) {
            if (mVisibleTab.isInitialized() && !mVisibleTab.isDetachedForReparenting()) {
                // TODO(dtrainor): Once we figure out why we can't grab a snapshot from the current
                // tab when we have other tabs loading from external apps remove the checks for
                // FROM_EXTERNAL_APP/FROM_NEW.
                if (!mVisibleTab.isClosing()
                        && (!isFromExternalApp || type != TabSelectionType.FROM_NEW)) {
                    cacheTabBitmap(mVisibleTab);
                }
                mVisibleTab.hide();
                mVisibleTab.setFullscreenManager(null);
                mTabSaver.addTabToSaveQueue(mVisibleTab);
            }
            mVisibleTab = null;
        }

        if (tab == null) {
            notifyChanged();
            return;
        }

        // We hit this case when the user enters tab switcher and comes back to the current tab
        // without actual tab switch.
        if (mVisibleTab == tab && !mVisibleTab.isHidden()) {
            // The current tab might have been killed by the os while in tab switcher.
            tab.loadIfNeeded();
            return;
        }

        tab.setFullscreenManager(mActivity.getFullscreenManager());
        mVisibleTab = tab;

        // Don't execute the tab display part if Chrome has just been sent to background. This
        // avoids uneccessary work (tab restore) and prevents pollution of tab display metrics - see
        // http://crbug.com/316166.
        if (type != TabSelectionType.FROM_EXIT) {
            tab.show(type);
            mUma.onShowTab(tab.getId(), tab.isBeingRestored());
        }
    }

    private void cacheTabBitmap(Tab tabToCache) {
        // Trigger a capture of this tab.
        if (tabToCache == null) return;
        mTabContentManager.cacheTabThumbnail(tabToCache);
    }

    @Override
    public boolean isInOverviewMode() {
        return mOverviewModeBehavior != null && mOverviewModeBehavior.overviewVisible();
    }

    @Override
    public boolean isSessionRestoreInProgress() {
        return mSessionRestoreInProgress.get();
    }

    // TODO(tedchoc): Remove the need for this to be exposed.
    @Override
    public void notifyChanged() {
        super.notifyChanged();
    }

    @VisibleForTesting
    public TabPersistentStore getTabPersistentStoreForTesting() {
        return mTabSaver;
    }
}
