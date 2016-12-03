// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Heuristic for Tap suppression in areas where the Bar would overlap the selection.
 * Handles logging of results seen and activation.
 */
public class BarOverlapTapSuppression extends ContextualSearchHeuristic {
    private final ChromeActivity mActivity;
    private final boolean mDoesBarOverlap;
    private final boolean mIsConditionSatisfied;
    private final float mPxToDp;

    /**
     * Constructs a Tap suppression heuristic that handles a Tap near where the Bar shows.
     * @param selectionController The {@link ContextualSearchSelectionController}.
     * @param x The x position of the Tap.
     * @param y The y position of the Tap.
     */
    BarOverlapTapSuppression(
            ContextualSearchSelectionController selectionController, int x, int y) {
        // TODO(donnd): rather than getting the Activity, find a way to access the panel
        // and ask it to determine overlap.  E.g. isCoordinateInsidePeekingBarArea(x, y) modeled
        // after isCoordinateInsideBar(x, y).
        mPxToDp = selectionController.getPxToDp();
        mActivity = selectionController.getActivity();
        mDoesBarOverlap = doesBarOverlap(x, y);
        mIsConditionSatisfied =
                mDoesBarOverlap && ContextualSearchFieldTrial.isBarOverlapSuppressionEnabled();
    }

    @Override
    protected boolean isConditionSatisfied() {
        return mIsConditionSatisfied;
    }

    @Override
    protected void logConditionState() {
        if (ContextualSearchFieldTrial.isBarOverlapSuppressionEnabled()) {
            ContextualSearchUma.logBarOverlapSuppression(mIsConditionSatisfied);
        }
    }

    @Override
    protected void logResultsSeen(boolean wasSearchContentViewSeen, boolean wasActivatedByTap) {
        if (ContextualSearchFieldTrial.isBarOverlapCollectionEnabled()) {
            ContextualSearchUma.logBarOverlapResultsSeen(
                    wasSearchContentViewSeen, wasActivatedByTap, mDoesBarOverlap);
        }
    }

    /**
     * @return The height of the content view area of the base page in pixels, or 0 if the
     *         Height cannot be reliably obtained.
     */
    private float getContentHeightPx() {
        Tab currentTab = mActivity.getActivityTab();
        ChromeFullscreenManager fullscreenManager = mActivity.getFullscreenManager();
        if (fullscreenManager == null || currentTab == null) return 0.f;

        float controlsOffset = fullscreenManager.getControlOffset();
        float controlsHeight = fullscreenManager.getTopControlsHeight();
        float tabHeight = currentTab.getHeight();
        return tabHeight - (controlsHeight + controlsOffset);
    }

    /**
     * @return Whether the Bar would overlap the given x,y coordinate when in its normal
     *         peeking state.
     */
    private boolean doesBarOverlap(int x, int y) {
        float contentHeightPx = getContentHeightPx();
        if (contentHeightPx == 0) return false;

        // First check vertical overlap.
        // TODO(donnd): Ask the panel whether the bar overlaps!
        float barHeightDp = 56; // DPs
        float yDp = y * mPxToDp;
        float contentHeightDp = contentHeightPx * mPxToDp;
        if (yDp < contentHeightDp - barHeightDp) return false;

        // Is there also a horizontal overlap?
        float overlayPanelWidth = 600;
        Tab currentTab = mActivity.getActivityTab();
        float overlayPanelX = (currentTab.getWidth() - overlayPanelWidth) / 2;
        return x > overlayPanelX && x < overlayPanelX + overlayPanelWidth;
    }
}
