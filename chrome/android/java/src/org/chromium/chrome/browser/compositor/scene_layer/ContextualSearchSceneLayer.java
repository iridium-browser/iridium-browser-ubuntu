// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.scene_layer;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchBarControl;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchIconSpriteControl;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPeekPromoControl;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPromoControl;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.resources.ResourceManager;

/**
 * A SceneLayer to render layers for ContextualSearchLayout.
 */
@JNINamespace("chrome::android")
public class ContextualSearchSceneLayer extends SceneOverlayLayer {

    // NOTE: If you use SceneLayer's native pointer here, the JNI generator will try to
    // downcast using reinterpret_cast<>. We keep a separate pointer to avoid it.
    private long mNativePtr;

    /** If the scene layer has been initialized. */
    private boolean mIsInitialized;

    private final float mDpToPx;

    public ContextualSearchSceneLayer(float dpToPx) {
        mDpToPx = dpToPx;
    }

    /**
     * Update the scene layer to draw an OverlayPanel.
     * @param resourceManager Manager to get view and image resources.
     * @param panel The OverlayPanel to render.
     * @param searchBarControl The Search Bar control.
     * @param peekPromoControl The peeking promotion for Contextual Search.
     * @param spriteControl The object controlling the "G" animation for Contextual Search.
     */
    public void update(ResourceManager resourceManager,
            ContextualSearchPanel panel,
            ContextualSearchBarControl searchBarControl,
            ContextualSearchPeekPromoControl peekPromoControl,
            ContextualSearchPromoControl promoControl,
            ContextualSearchIconSpriteControl spriteControl) {
        // Don't try to update the layer if not initialized or showing.
        if (resourceManager == null || !panel.isShowing()) return;

        if (!mIsInitialized) {
            nativeCreateContextualSearchLayer(mNativePtr, resourceManager);
            mIsInitialized = true;
        }

        int searchContextViewId = searchBarControl.getSearchContextViewId();
        int searchTermViewId = searchBarControl.getSearchTermViewId();
        int searchCaptionViewId = searchBarControl.getCaptionViewId();

        int searchPromoViewId = promoControl.getViewId();
        boolean searchPromoVisible = promoControl.isVisible();
        float searchPromoHeightPx = promoControl.getHeightPx();
        float searchPromoOpacity = promoControl.getOpacity();

        int searchPeekPromoTextViewId = peekPromoControl.getViewId();
        boolean searchPeekPromoVisible = peekPromoControl.isVisible();
        float searchPeekPromoHeightPx = peekPromoControl.getHeightPx();
        float searchPeekPromoPaddingPx =  peekPromoControl.getPaddingPx();
        float searchPeekPromoRippleWidthPx = peekPromoControl.getRippleWidthPx();
        float searchPeekPromoRippleOpacity = peekPromoControl.getRippleOpacity();
        float searchPeekPromoTextOpacity = peekPromoControl.getTextOpacity();

        boolean searchProviderIconSpriteVisible = spriteControl.isVisible();
        float searchProviderIconCompletionPercentage = spriteControl.getCompletionPercentage();

        float searchPanelX = panel.getOffsetX();
        float searchPanelY = panel.getOffsetY();
        float searchPanelWidth = panel.getWidth();
        float searchPanelHeight = panel.getHeight();

        float searchBarMarginSide = panel.getBarMarginSide();
        float searchBarHeight = panel.getBarHeight();

        float searchContextOpacity = searchBarControl.getSearchBarContextOpacity();
        float searchTermOpacity = searchBarControl.getSearchBarTermOpacity();

        float searchCaptionAnimationPercentage = searchBarControl.getCaptionAnimationPercentage();
        boolean searchCaptionVisible = searchBarControl.getCaptionVisible();

        boolean searchBarBorderVisible = panel.isBarBorderVisible();
        float searchBarBorderHeight = panel.getBarBorderHeight();

        boolean searchBarShadowVisible = panel.getBarShadowVisible();
        float searchBarShadowOpacity = panel.getBarShadowOpacity();

        float arrowIconOpacity = panel.getArrowIconOpacity();
        float arrowIconRotation = panel.getArrowIconRotation();

        float closeIconOpacity = panel.getCloseIconOpacity();

        boolean isProgressBarVisible = panel.isProgressBarVisible();

        float progressBarHeight = panel.getProgressBarHeight();
        float progressBarOpacity = panel.getProgressBarOpacity();
        int progressBarCompletion = panel.getProgressBarCompletion();

        WebContents panelWebContents = panel.getContentViewCore() != null
                ? panel.getContentViewCore().getWebContents() : null;

        nativeUpdateContextualSearchLayer(mNativePtr,
                R.drawable.contextual_search_bar_background,
                searchContextViewId,
                searchTermViewId,
                searchCaptionViewId,
                R.drawable.contextual_search_bar_shadow,
                R.drawable.google_icon_sprite,
                R.raw.google_icon_sprite,
                R.drawable.breadcrumb_arrow,
                ContextualSearchPanel.CLOSE_ICON_DRAWABLE_ID,
                R.drawable.progress_bar_background,
                R.drawable.progress_bar_foreground,
                searchPromoViewId,
                R.drawable.contextual_search_promo_ripple,
                searchPeekPromoTextViewId,
                mDpToPx,
                panel.getBasePageBrightness(),
                panel.getBasePageY() * mDpToPx,
                panelWebContents,
                searchPromoVisible,
                searchPromoHeightPx,
                searchPromoOpacity,
                searchPeekPromoVisible,
                searchPeekPromoHeightPx,
                searchPeekPromoPaddingPx,
                searchPeekPromoRippleWidthPx,
                searchPeekPromoRippleOpacity,
                searchPeekPromoTextOpacity,
                searchPanelX * mDpToPx,
                searchPanelY * mDpToPx,
                searchPanelWidth * mDpToPx,
                searchPanelHeight * mDpToPx,
                searchBarMarginSide * mDpToPx,
                searchBarHeight * mDpToPx,
                searchContextOpacity,
                searchTermOpacity,
                searchCaptionAnimationPercentage,
                searchCaptionVisible,
                searchBarBorderVisible,
                searchBarBorderHeight * mDpToPx,
                searchBarShadowVisible,
                searchBarShadowOpacity,
                searchProviderIconSpriteVisible,
                searchProviderIconCompletionPercentage,
                arrowIconOpacity,
                arrowIconRotation,
                closeIconOpacity,
                isProgressBarVisible,
                progressBarHeight * mDpToPx,
                progressBarOpacity,
                progressBarCompletion);
    }

    @Override
    public void setContentTree(SceneLayer contentTree) {
        nativeSetContentTree(mNativePtr, contentTree);
    }

    /**
     * Hide the layer tree; for use if the panel is not being shown.
     */
    public void hideTree() {
        if (!mIsInitialized) return;
        nativeHideTree(mNativePtr);
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = nativeInit();
        }
        assert mNativePtr != 0;
    }

    /**
     * Destroys this object and the corresponding native component.
     */
    @Override
    public void destroy() {
        super.destroy();
        mIsInitialized = false;
        mNativePtr = 0;
    }

    private native long nativeInit();
    private native void nativeCreateContextualSearchLayer(
            long nativeContextualSearchSceneLayer,
            ResourceManager resourceManager);
    private native void nativeSetContentTree(
            long nativeContextualSearchSceneLayer,
            SceneLayer contentTree);
    private native void nativeHideTree(
            long nativeContextualSearchSceneLayer);
    private native void nativeUpdateContextualSearchLayer(
            long nativeContextualSearchSceneLayer,
            int searchBarBackgroundResourceId,
            int searchContextResourceId,
            int searchTermResourceId,
            int searchCaptionResourceId,
            int searchBarShadowResourceId,
            int panelIconResourceId,
            int searchProviderIconSpriteMetadataResourceId,
            int arrowUpResourceId,
            int closeIconResourceId,
            int progressBarBackgroundResourceId,
            int progressBarResourceId,
            int searchPromoResourceId,
            int peekPromoRippleResourceId,
            int peekPromoTextResourceId,
            float dpToPx,
            float basePageBrightness,
            float basePageYOffset,
            WebContents webContents,
            boolean searchPromoVisible,
            float searchPromoHeight,
            float searchPromoOpacity,
            boolean searchPeekPromoVisible,
            float searchPeekPromoHeight,
            float searchPeekPromoPaddingPx,
            float searchPeekPromoRippleWidth,
            float searchPeekPromoRippleOpacity,
            float searchPeekPromoTextOpacity,
            float searchPanelX,
            float searchPanelY,
            float searchPanelWidth,
            float searchPanelHeight,
            float searchBarMarginSide,
            float searchBarHeight,
            float searchContextOpacity,
            float searchTermOpacity,
            float searchCaptionAnimationPercentage,
            boolean searchCaptionVisible,
            boolean searchBarBorderVisible,
            float searchBarBorderHeight,
            boolean searchBarShadowVisible,
            float searchBarShadowOpacity,
            boolean searchProviderIconSpriteVisible,
            float searchProviderIconCompletionPercentage,
            float arrowIconOpacity,
            float arrowIconRotation,
            float closeIconOpacity,
            boolean isProgressBarVisible,
            float progressBarHeight,
            float progressBarOpacity,
            int progressBarCompletion);
}
