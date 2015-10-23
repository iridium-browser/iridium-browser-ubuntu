// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.precache;

import android.content.Context;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.preferences.privacy.PrivacyPreferencesManager;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.content.browser.test.NativeLibraryTestBase;

/**
 * Unit tests for {@link PrecacheLauncher}.
 *
 * setUp/tearDown code was inspired by org.chromium.chrome.browser.sync.ui.PassphraseActivityTest.
 */
public class PrecacheLauncherTest extends NativeLibraryTestBase {
    private StubProfileSyncService mSync;
    private PrecacheLauncherUnderTest mLauncher;

    private class PrecacheLauncherUnderTest extends PrecacheLauncher {
        private boolean mShouldRun = false;

        @Override
        protected void onPrecacheCompleted(boolean tryAgainSoon) {}

        @Override
        boolean nativeShouldRun() {
            return mShouldRun;
        }

        /**
         * Modify the return value of nativeShouldRun. This will notify sync state subscribers, as
         * if the user changed their sync preferences.
         */
        void setShouldRun(boolean shouldRun) {
            mShouldRun = shouldRun;
            notifySyncChanged();
        }
    }

    private static class StubProfileSyncService extends ProfileSyncService {
        private boolean mSyncInitialized = false;

        public StubProfileSyncService(Context context) {
            super(context);
        }

        @Override
        public boolean isSyncInitialized() {
            return mSyncInitialized;
        }

        public void setSyncInitialized(boolean syncInitialized) {
            mSyncInitialized = syncInitialized;
            syncStateChanged();
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        // This is a PrecacheLauncher with a stubbed out nativeShouldRun so we can change that on
        // the fly without needing to set up a sync backend.
        mLauncher = new PrecacheLauncherUnderTest();

        // The target context persists throughout the entire test run, and so leaks state between
        // tests. We reset the is_precaching_enabled pref to false to make the test run consistent,
        // in case another test class has modified this pref.
        PrecacheServiceLauncher.setIsPrecachingEnabled(getTargetContext(), false);

        // ProfileSyncService needs the browser process to be running.
        loadNativeLibraryAndInitBrowserProcess();

        // ProfileSyncService must be initialized on the UI thread. Oddly, even though
        // ThreadUtils.runningOnUiThread() is true here, it's, no, not really, no.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // The StubProfileSyncService stubs out isSyncInitialized so we can change that on
                // the fly.
                mSync = new StubProfileSyncService(getTargetContext());
                ProfileSyncService.overrideForTests(mSync);

                // This is currently the default, but let's verify that, lest it ever change and we
                // get confusing test failures later.
                assertTrue(PrivacyPreferencesManager.getInstance(getTargetContext())
                                   .shouldPrerender());
            }
        });
    }

    @Override
    protected void tearDown() throws Exception {
        ProfileSyncService.overrideForTests(null);
        PrecacheServiceLauncher.setIsPrecachingEnabled(getTargetContext(), false);
        super.tearDown();
    }

    @SmallTest
    @Feature({"Precache"})
    public void testUpdateEnabled_SyncNotReady_ThenDisabled() {
        mLauncher.updateEnabled(getTargetContext());
        waitUntilUiThreadIdle();

        assertEquals(false, isPrecachingEnabled());

        setSyncInitialized(true);
        assertEquals(false, isPrecachingEnabled());
    }

    @SmallTest
    @Feature({"Precache"})
    public void testUpdateEnabled_SyncNotReady_ThenEnabled() {
        mLauncher.updateEnabled(getTargetContext());
        waitUntilUiThreadIdle();

        assertEquals(false, isPrecachingEnabled());

        mLauncher.setShouldRun(true);
        setSyncInitialized(true);
        assertEquals(true, isPrecachingEnabled());
    }

    @SmallTest
    @Feature({"Precache"})
    public void testUpdateEnabled_Disabled_ThenEnabled() {
        setSyncInitialized(true);
        mLauncher.updateEnabled(getTargetContext());
        waitUntilUiThreadIdle();

        assertEquals(false, isPrecachingEnabled());

        mLauncher.setShouldRun(true);
        assertEquals(true, isPrecachingEnabled());
    }

    @SmallTest
    @Feature({"Precache"})
    public void testUpdateEnabled_Enabled_ThenDisabled() {
        mLauncher.setShouldRun(true);
        setSyncInitialized(true);
        mLauncher.updateEnabled(getTargetContext());
        waitUntilUiThreadIdle();

        assertEquals(true, isPrecachingEnabled());

        mLauncher.setShouldRun(false);
        assertEquals(false, isPrecachingEnabled());
    }

    /** Return the Context for the Chromium app. */
    private Context getTargetContext() {
        return getInstrumentation().getTargetContext();
    }

    /** Block until all tasks posted to the UI thread have completed. */
    private void waitUntilUiThreadIdle() {
        getInstrumentation().waitForIdleSync();
    }

    /** Return the value of the is_precaching_enabled pref, as set by updateEnabledSync. */
    private boolean isPrecachingEnabled() {
        return PrecacheServiceLauncher.isPrecachingEnabled(getTargetContext());
    }

    /** Pretend the sync backend is initialized or not. */
    private void setSyncInitialized(final boolean syncInitialized) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mSync.setSyncInitialized(syncInitialized);
            }
        });
    }

    /** Notify listeners that sync preferences have changed. This is run by setShouldRun. */
    private void notifySyncChanged() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mSync.syncStateChanged();
            }
        });
    }
}
