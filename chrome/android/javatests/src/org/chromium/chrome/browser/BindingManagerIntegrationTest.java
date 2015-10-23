// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.test.FlakyTest;
import android.test.suitebuilder.annotation.LargeTest;
import android.util.SparseArray;
import android.util.SparseBooleanArray;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.content.browser.BindingManager;
import org.chromium.content.browser.ChildProcessConnection;
import org.chromium.content.browser.ChildProcessLauncher;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.Callable;

/**
 * Integration tests for the BindingManager API. This test plants a mock BindingManager
 * implementation and verifies that the signals it relies on are correctly delivered.
 */
@CommandLineFlags.Add(ChromeSwitches.DISABLE_DOCUMENT_MODE)  // crbug.com/414719
public class BindingManagerIntegrationTest extends ChromeActivityTestCaseBase<ChromeActivity> {

    private static class MockBindingManager implements BindingManager {
        // Maps pid to the last received visibility state of the renderer.
        private final SparseBooleanArray mProcessInForegroundMap = new SparseBooleanArray();
        // Maps pid to a string recording calls to setInForeground() and visibilityDetermined().
        private final SparseArray<String> mVisibilityCallsMap = new SparseArray<String>();
        private boolean mIsReleaseAllModerateBindingsCalled;

        boolean isInForeground(int pid) {
            return mProcessInForegroundMap.get(pid);
        }

        boolean isInBackground(int pid) {
            return !mProcessInForegroundMap.get(pid);
        }

        boolean setInForegroundWasCalled(int pid) {
            return mProcessInForegroundMap.indexOfKey(pid) >= 0;
        }

        String getVisibilityCalls(int pid) {
            return mVisibilityCallsMap.get(pid);
        }

        boolean isReleaseAllModerateBindingsCalled() {
            return mIsReleaseAllModerateBindingsCalled;
        }

        @Override
        public void addNewConnection(int pid, ChildProcessConnection connection) {
            mVisibilityCallsMap.put(pid, "");
        }

        @Override
        public void setInForeground(int pid, boolean inForeground) {
            mProcessInForegroundMap.put(pid, inForeground);

            if (inForeground) {
                mVisibilityCallsMap.put(pid, mVisibilityCallsMap.get(pid) + "FG;");
            } else {
                mVisibilityCallsMap.put(pid, mVisibilityCallsMap.get(pid) + "BG;");
            }
        }

        @Override
        public void determinedVisibility(int pid) {
            mVisibilityCallsMap.put(pid, mVisibilityCallsMap.get(pid) + "DETERMINED;");
        }

        @Override
        public void onSentToBackground() {}

        @Override
        public void onBroughtToForeground() {}

        @Override
        public boolean isOomProtected(int pid) {
            return false;
        }

        @Override
        public void clearConnection(int pid) {}

        @Override
        public void startModerateBindingManagement(
                Context context, int maxSize, float lowReduceRatio, float highReduceRatio) {}

        @Override
        public void releaseAllModerateBindings() {
            mIsReleaseAllModerateBindingsCalled = true;
        }
    }

    private MockBindingManager mBindingManager;

    private static final String FILE_PATH = "chrome/test/data/android/test.html";
    // about:version will always be handled by a different renderer than a local file.
    private static final String ABOUT_VERSION_PATH = "chrome://version/";

    public BindingManagerIntegrationTest() {
        super(ChromeActivity.class);
    }

    /**
     * Verifies that the .setProcessInForeground() signal is called correctly as the tabs are
     * created and switched.
     */
    @LargeTest
    @Feature({"ProcessManagement"})
    public void testTabSwitching() throws InterruptedException {
        // Create two tabs and wait until they are loaded, so that their renderers are around.
        final Tab[] tabs = new Tab[2];
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                // Foreground tab.
                TabCreator tabCreator = getActivity().getCurrentTabCreator();
                tabs[0] = tabCreator.createNewTab(
                        new LoadUrlParams(TestHttpServerClient.getUrl(FILE_PATH)),
                                TabLaunchType.FROM_KEYBOARD, null);
                // Background tab.
                tabs[1] = tabCreator.createNewTab(
                        new LoadUrlParams(TestHttpServerClient.getUrl(FILE_PATH)),
                                TabLaunchType.FROM_LONGPRESS_BACKGROUND, null);
                // On Svelte devices the background tab would not be loaded automatically, so
                // trigger the load manually.
                tabs[1].show(TabSelectionType.FROM_USER);
                tabs[1].hide();
            }
        });
        ChromeTabUtils.waitForTabPageLoaded(tabs[0], TestHttpServerClient.getUrl(FILE_PATH));
        ChromeTabUtils.waitForTabPageLoaded(tabs[1], TestHttpServerClient.getUrl(FILE_PATH));

        // Wait for the new tab animations on phones to finish.
        if (!DeviceFormFactor.isTablet(getActivity())) {
            final ChromeActivity activity = getActivity();
            assertTrue("Did not finish animation",
                    CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                        @Override
                        public boolean isSatisfied() {
                            Layout layout = activity.getCompositorViewHolder()
                                    .getLayoutManager().getActiveLayout();
                            return !layout.isLayoutAnimating();
                        }
                    }));
        }
        getInstrumentation().waitForIdleSync();

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                // Make sure that the renderers were spawned.
                assertTrue(tabs[0].getContentViewCore().getCurrentRenderProcessId() > 0);
                assertTrue(tabs[1].getContentViewCore().getCurrentRenderProcessId() > 0);

                // Verify that the renderer of the foreground tab was signalled as visible.
                assertTrue(mBindingManager.isInForeground(
                        tabs[0].getContentViewCore().getCurrentRenderProcessId()));
                // Verify that the renderer of the tab loaded in background was signalled as not
                // visible.
                assertTrue(mBindingManager.isInBackground(
                        tabs[1].getContentViewCore().getCurrentRenderProcessId()));

                // Select tabs[1] and verify that the renderer visibility was flipped.
                TabModelUtils.setIndex(getActivity().getCurrentTabModel(), indexOf(tabs[1]));
                assertTrue(mBindingManager.isInBackground(
                        tabs[0].getContentViewCore().getCurrentRenderProcessId()));
                assertTrue(mBindingManager.isInForeground(
                        tabs[1].getContentViewCore().getCurrentRenderProcessId()));
            }
        });
    }

    /**
     * Verifies that the .setProcessInForeground() signal is called correctly when a tab that
     * crashed in background is restored in foreground. This is a regression test for
     * http://crbug.com/399521.
     */
    @LargeTest
    @Feature({"ProcessManagement"})
    public void testCrashInBackground() throws InterruptedException {
        // Create two tabs and wait until they are loaded, so that their renderers are around.
        final Tab[] tabs = new Tab[2];
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                // Foreground tab.
                TabCreator tabCreator = getActivity().getCurrentTabCreator();
                tabs[0] = tabCreator.createNewTab(
                        new LoadUrlParams(TestHttpServerClient.getUrl(FILE_PATH)),
                                TabLaunchType.FROM_KEYBOARD, null);
                // Background tab.
                tabs[1] = tabCreator.createNewTab(
                        new LoadUrlParams(TestHttpServerClient.getUrl(FILE_PATH)),
                                TabLaunchType.FROM_LONGPRESS_BACKGROUND, null);
                // On Svelte devices the background tab would not be loaded automatically, so
                // trigger the load manually.
                tabs[1].show(TabSelectionType.FROM_USER);
                tabs[1].hide();
            }
        });
        ChromeTabUtils.waitForTabPageLoaded(tabs[0], TestHttpServerClient.getUrl(FILE_PATH));
        ChromeTabUtils.waitForTabPageLoaded(tabs[1], TestHttpServerClient.getUrl(FILE_PATH));

        // Wait for the new tab animations on phones to finish.
        if (!DeviceFormFactor.isTablet(getActivity())) {
            final ChromeActivity activity = getActivity();
            assertTrue("Did not finish animation",
                    CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                        @Override
                        public boolean isSatisfied() {
                            Layout layout = activity.getCompositorViewHolder()
                                    .getLayoutManager().getActiveLayout();
                            return !layout.isLayoutAnimating();
                        }
                    }));
        }
        getInstrumentation().waitForIdleSync();

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                // Make sure that the renderers were spawned.
                assertTrue(tabs[0].getContentViewCore().getCurrentRenderProcessId() > 0);
                assertTrue(tabs[1].getContentViewCore().getCurrentRenderProcessId() > 0);

                // Verify that the renderer of the foreground tab was signalled as visible.
                assertTrue(mBindingManager.isInForeground(
                        tabs[0].getContentViewCore().getCurrentRenderProcessId()));
                // Verify that the renderer of the tab loaded in background was signalled as not
                // visible.
                assertTrue(mBindingManager.isInBackground(
                        tabs[1].getContentViewCore().getCurrentRenderProcessId()));
            }
        });

        // Kill the renderer and wait for the crash to be noted by the browser process.
        assertTrue(ChildProcessLauncher.crashProcessForTesting(
                tabs[1].getContentViewCore().getCurrentRenderProcessId()));

        assertTrue("Renderer crash wasn't noticed by the browser.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return tabs[1].getContentViewCore().getCurrentRenderProcessId() == 0;
                    }
                }));

        // Switch to the tab that crashed in background.
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                TabModelUtils.setIndex(getActivity().getCurrentTabModel(), indexOf(tabs[1]));
            }
        });

        // Wait until the process is spawned and its visibility is determined.
        assertTrue("Process for the crashed tab was not respawned.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return tabs[1].getContentViewCore().getCurrentRenderProcessId() != 0;
                    }
                }));

        assertTrue("isInForeground() was not called for the process.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return mBindingManager.setInForegroundWasCalled(
                                tabs[1].getContentViewCore().getCurrentRenderProcessId());
                    }
                }));

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                // Verify the visibility of the renderers.
                assertTrue(mBindingManager.isInBackground(
                        tabs[0].getContentViewCore().getCurrentRenderProcessId()));
                assertTrue(mBindingManager.isInForeground(
                        tabs[1].getContentViewCore().getCurrentRenderProcessId()));
            }
        });
    }

    /**
     * Verifies that a renderer that crashes in foreground has the correct visibility when
     * recreated.
     */
    @LargeTest
    @Feature({"ProcessManagement"})
    public void testCrashInForeground() throws InterruptedException {
        // Create a tab in foreground and wait until it is loaded.
        final Tab tab = ThreadUtils.runOnUiThreadBlockingNoException(
                new Callable<Tab>() {
                    @Override
                    public Tab call() throws Exception {
                        TabCreator tabCreator = getActivity().getCurrentTabCreator();
                        return tabCreator.createNewTab(
                                new LoadUrlParams(TestHttpServerClient.getUrl(FILE_PATH)),
                                        TabLaunchType.FROM_KEYBOARD, null);
                    }
                });
        ChromeTabUtils.waitForTabPageLoaded(tab, TestHttpServerClient.getUrl(FILE_PATH));
        getInstrumentation().waitForIdleSync();

        // Kill the renderer and wait for the crash to be noted by the browser process.
        assertTrue(ChildProcessLauncher.crashProcessForTesting(
                tab.getContentViewCore().getCurrentRenderProcessId()));

        assertTrue("Renderer crash wasn't noticed by the browser.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return tab.getContentViewCore().getCurrentRenderProcessId() == 0;
                    }
                }));

        // Reload the tab, respawning the renderer.
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                tab.reload();
            }
        });

        // Wait until the process is spawned and its visibility is determined.
        assertTrue("Process for the crashed tab was not respawned.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return tab.getContentViewCore().getCurrentRenderProcessId() != 0;
                    }
                }));

        assertTrue("isInForeground() was not called for the process.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return mBindingManager.setInForegroundWasCalled(
                                tab.getContentViewCore().getCurrentRenderProcessId());
                    }
                }));

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                // Verify the visibility of the renderer.
                assertTrue(mBindingManager.isInForeground(
                        tab.getContentViewCore().getCurrentRenderProcessId()));
            }
        });
    }

    /**
     * Ensures correctness of the visibilityDetermined() calls, that should be always preceded by
     * setInForeground().
     *
     * Bug: https://crbug.com/474543
     * @LargeTest
     * @Feature({"ProcessManagement"})
     */
    @FlakyTest
    public void testVisibilityDetermined() throws InterruptedException {
        // Create a tab in foreground and wait until it is loaded.
        final Tab fgTab = ThreadUtils.runOnUiThreadBlockingNoException(
                new Callable<Tab>() {
                    @Override
                    public Tab call() {
                        TabCreator tabCreator = getActivity().getCurrentTabCreator();
                        return tabCreator.createNewTab(
                                new LoadUrlParams(TestHttpServerClient.getUrl(FILE_PATH)),
                                        TabLaunchType.FROM_KEYBOARD, null);
                    }});
        ChromeTabUtils.waitForTabPageLoaded(fgTab, TestHttpServerClient.getUrl(FILE_PATH));
        int initialNavigationPid = fgTab.getContentViewCore().getCurrentRenderProcessId();
        // Ensure the following calls happened:
        //  - FG - setInForeground(true) - when the tab is created in the foreground
        //  - DETERMINED - visibilityDetermined() - after the initial navigation is committed
        assertEquals("FG;DETERMINED;", mBindingManager.getVisibilityCalls(initialNavigationPid));

        // Navigate to about:version which requires a different renderer.
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                fgTab.loadUrl(new LoadUrlParams(ABOUT_VERSION_PATH));
            }
        });
        ChromeTabUtils.waitForTabPageLoaded(fgTab, ABOUT_VERSION_PATH);
        int secondNavigationPid = fgTab.getContentViewCore().getCurrentRenderProcessId();
        assertTrue(secondNavigationPid != initialNavigationPid);
        // Ensure the following calls happened:
        //  - BG - setInForeground(false) - when the renderer is created for uncommited frame
        //  - FG - setInForeground(true) - when the frame is swapped in on commit
        //  - DETERMINED - visibilityDetermined() - after the navigation is committed
        assertEquals("BG;FG;DETERMINED;", mBindingManager.getVisibilityCalls(secondNavigationPid));

        // Open a tab in the background and load it.
        final Tab bgTab = ThreadUtils.runOnUiThreadBlockingNoException(
                new Callable<Tab>() {
                    @Override
                    public Tab call() {
                        TabCreator tabCreator = getActivity().getCurrentTabCreator();
                        Tab tab = tabCreator.createNewTab(
                                new LoadUrlParams(TestHttpServerClient.getUrl(FILE_PATH)),
                                        TabLaunchType.FROM_LONGPRESS_BACKGROUND, null);
                        // On Svelte devices the background tab would not be loaded automatically,
                        // so trigger the load manually.
                        tab.show(TabSelectionType.FROM_USER);
                        tab.hide();
                        return tab;
                    }});
        ChromeTabUtils.waitForTabPageLoaded(bgTab, TestHttpServerClient.getUrl(FILE_PATH));
        int bgNavigationPid = bgTab.getContentViewCore().getCurrentRenderProcessId();
        // Ensure the following calls happened:
        //  - BG - setInForeground(false) - when tab is created in the background
        //  - DETERMINED - visibilityDetermined() - after the navigation is committed
        assertEquals("BG;DETERMINED;", mBindingManager.getVisibilityCalls(bgNavigationPid));
    }

    /**
     * Verifies that BindingManager.releaseAllModerateBindings() is called once all the sandboxed
     * services are allocated.
     */
    @CommandLineFlags.Add(ChildProcessLauncher.SWITCH_NUM_SANDBOXED_SERVICES_FOR_TESTING + "=4")
    @LargeTest
    @Feature({"ProcessManagement"})
    public void testReleaseAllModerateBindings() throws InterruptedException {
        final TabCreator tabCreator = getActivity().getCurrentTabCreator();
        final Tab[] tabs = new Tab[3];
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                // Foreground tab.
                tabs[0] = tabCreator.createNewTab(
                        new LoadUrlParams("about:blank"), TabLaunchType.FROM_KEYBOARD, null);
                // Background tab.
                tabs[1] = tabCreator.createNewTab(
                        new LoadUrlParams("about:blank"), TabLaunchType.FROM_KEYBOARD, null);
            }
        });
        ChromeTabUtils.waitForTabPageLoaded(tabs[0], "about:blank");
        ChromeTabUtils.waitForTabPageLoaded(tabs[1], "about:blank");
        // At this point 3 sanboxed services are allocated; the initial one + 2 new tabs.
        assertFalse(mBindingManager.isReleaseAllModerateBindingsCalled());

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                // Foreground tab.
                tabs[2] = tabCreator.createNewTab(
                        new LoadUrlParams("about:blank"), TabLaunchType.FROM_KEYBOARD, null);
            }
        });
        ChromeTabUtils.waitForTabPageLoaded(tabs[2], "about:blank");
        // At this point all the sanboxed services are allocated.
        assertTrue(mBindingManager.isReleaseAllModerateBindingsCalled());
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        // Hook in the test binding manager.
        mBindingManager = new MockBindingManager();
        ChildProcessLauncher.setBindingManagerForTesting(mBindingManager);
    }

    /**
     * @return the index of the given tab in the current tab model
     */
    private int indexOf(Tab tab) {
        return getActivity().getCurrentTabModel().indexOf(tab);
    }
}
