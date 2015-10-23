// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.findinpage;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.util.AttributeSet;
import android.view.View;

import org.chromium.chrome.R;

/**
 * A phone specific version of the {@link FindToolbar}.
 */
public class FindToolbarPhone extends FindToolbar {
    /**
     * Creates an instance of a {@link FindToolbarPhone}.
     * @param context The Context to create the {@link FindToolbarPhone} under.
     * @param attrs The AttributeSet used to create the {@link FindToolbarPhone}.
     */
    public FindToolbarPhone(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void activate() {
        if (!isViewAvailable()) return;
        setVisibility(View.VISIBLE);
        super.activate();
    }

    @Override
    public void deactivate(boolean clearSelection) {
        super.deactivate(clearSelection);
        setVisibility(View.GONE);
    }

    @Override
    protected void updateVisualsForTabModel(boolean isIncognito) {
        int queryTextColorId;
        if (isIncognito) {
            setBackgroundResource(R.color.incognito_primary_color);
            ColorStateList white = getResources().getColorStateList(R.color.light_mode_tint);
            mFindNextButton.setTint(white);
            mFindPrevButton.setTint(white);
            mCloseFindButton.setTint(white);
            queryTextColorId = R.color.find_in_page_query_white_color;
        } else {
            setBackgroundColor(Color.WHITE);
            ColorStateList dark = getResources().getColorStateList(R.color.dark_mode_tint);
            mFindNextButton.setTint(dark);
            mFindPrevButton.setTint(dark);
            mCloseFindButton.setTint(dark);
            queryTextColorId = R.color.find_in_page_query_color;
        }
        mFindQuery.setTextColor(getContext().getResources().getColor(queryTextColorId));
    }

    @Override
    protected int getStatusColor(boolean failed, boolean incognito) {
        if (!failed && incognito) {
            return getContext().getResources().getColor(
                    R.color.find_in_page_results_status_white_color);
        }

        return super.getStatusColor(failed, incognito);
    }
}
