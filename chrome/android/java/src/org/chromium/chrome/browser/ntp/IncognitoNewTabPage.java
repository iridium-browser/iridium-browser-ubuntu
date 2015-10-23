// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.app.Activity;
import android.graphics.Canvas;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.NativePage;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.compositor.layouts.content.InvalidationAwareThumbnailProvider;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.ntp.IncognitoNewTabPageView.IncognitoNewTabPageManager;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Provides functionality when the user interacts with the Incognito NTP.
 */
public class IncognitoNewTabPage implements NativePage, InvalidationAwareThumbnailProvider {
    private final Activity mActivity;

    private final String mTitle;
    private final int mBackgroundColor;
    private final IncognitoNewTabPageView mIncognitoNewTabPageView;

    private boolean mIsLoaded;

    private final IncognitoNewTabPageManager mIncognitoNewTabPageManager =
            new IncognitoNewTabPageManager() {
        @Override
        public void loadIncognitoLearnMore() {
            HelpAndFeedback.getInstance(mActivity).show(mActivity,
                    mActivity.getString(R.string.help_context_incognito_learn_more),
                    Profile.getLastUsedProfile(), null);
        }

        @Override
        public void onLoadingComplete() {
            mIsLoaded = true;
        }
    };

    /**
     * Constructs an Incognito NewTabPage.
     * @param activity The activity used to create the new tab page's View.
     */
    public IncognitoNewTabPage(Activity activity) {
        mActivity = activity;

        mTitle = activity.getResources().getString(R.string.button_new_tab);
        mBackgroundColor = activity.getResources().getColor(R.color.ntp_bg_incognito);

        LayoutInflater inflater = LayoutInflater.from(activity);
        mIncognitoNewTabPageView =
                (IncognitoNewTabPageView) inflater.inflate(R.layout.new_tab_page_incognito, null);
        mIncognitoNewTabPageView.initialize(mIncognitoNewTabPageManager);
    }

    /**
     * @return Whether the NTP has finished loaded.
     */
    @VisibleForTesting
    public boolean isLoadedForTests() {
        return mIsLoaded;
    }

    // NativePage overrides

    @Override
    public void destroy() {
        assert getView().getParent() == null : "Destroy called before removed from window";
    }

    @Override
    public String getUrl() {
        return UrlConstants.NTP_URL;
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public int getBackgroundColor() {
        return mBackgroundColor;
    }

    @Override
    public View getView() {
        return mIncognitoNewTabPageView;
    }

    @Override
    public String getHost() {
        return UrlConstants.NTP_HOST;
    }

    @Override
    public void updateForUrl(String url) {
    }

    // InvalidationAwareThumbnailProvider

    @Override
    public boolean shouldCaptureThumbnail() {
        return mIncognitoNewTabPageView.shouldCaptureThumbnail();
    }

    @Override
    public void captureThumbnail(Canvas canvas) {
        mIncognitoNewTabPageView.captureThumbnail(canvas);
    }
}
