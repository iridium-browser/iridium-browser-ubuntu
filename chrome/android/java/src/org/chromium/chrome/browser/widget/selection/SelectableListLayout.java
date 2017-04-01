// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.selection;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.support.v4.widget.DrawerLayout;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.Adapter;
import android.support.v7.widget.RecyclerView.AdapterDataObserver;
import android.support.v7.widget.Toolbar.OnMenuItemClickListener;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewStub;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.FadingShadow;
import org.chromium.chrome.browser.widget.FadingShadowView;
import org.chromium.chrome.browser.widget.LoadingView;
import org.chromium.ui.base.DeviceFormFactor;

import javax.annotation.Nullable;

/**
 * Contains UI elements common to selectable list views: a loading view, empty view, selection
 * toolbar, shadow, and recycler view.
 *
 * After the SelectableListLayout is inflated, it should be initialized through calls to
 * #initializeRecyclerView(), #initializeToolbar(), and #initializeEmptyView().
 *
 * @param <E> The type of the selectable items this layout holds.
 */
public class SelectableListLayout<E> extends RelativeLayout {
    private Adapter<RecyclerView.ViewHolder> mAdapter;
    private ViewStub mToolbarStub;
    private TextView mEmptyView;
    private LoadingView mLoadingView;
    private RecyclerView mRecyclerView;
    SelectionToolbar<E> mToolbar;

    private final AdapterDataObserver mAdapterObserver = new AdapterDataObserver() {
        @Override
        public void onChanged() {
            if (mAdapter.getItemCount() == 0) {
                mEmptyView.setVisibility(View.VISIBLE);
                mRecyclerView.setVisibility(View.GONE);
            } else {
                mEmptyView.setVisibility(View.GONE);
                mRecyclerView.setVisibility(View.VISIBLE);
            }
            // At inflation, the RecyclerView is set to gone, and the loading view is visible. As
            // long as the adapter data changes, we show the recycler view, and hide loading view.
            mLoadingView.hideLoadingUI();

            mToolbar.onDataChanged(mAdapter.getItemCount());
        }
    };

    public SelectableListLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        LayoutInflater.from(getContext()).inflate(R.layout.selectable_list_layout, this);

        mEmptyView = (TextView) findViewById(R.id.empty_view);
        mLoadingView = (LoadingView) findViewById(R.id.loading_view);
        mLoadingView.showLoadingUI();

        mToolbarStub = (ViewStub) findViewById(R.id.action_bar_stub);

        setFocusable(true);
        setFocusableInTouchMode(true);
    }

    /**
     * Initializes the RecyclerView.
     *
     * @param adapter The adapter that provides a binding from an app-specific data set to views
     *                that are displayed within the RecyclerView.
     * @return The RecyclerView itself.
     */
    public RecyclerView initializeRecyclerView(Adapter<RecyclerView.ViewHolder> adapter) {
        mAdapter = adapter;
        mAdapter.registerAdapterDataObserver(mAdapterObserver);

        mRecyclerView = (RecyclerView) findViewById(R.id.recycler_view);
        mRecyclerView.setAdapter(mAdapter);
        mRecyclerView.setLayoutManager(new LinearLayoutManager(getContext()));
        return mRecyclerView;
    }

    /**
     * Initializes the SelectionToolbar.
     *
     * @param toolbarLayoutId The resource id of the toolbar layout. This will be inflated into
     *                        a ViewStub.
     * @param delegate The SelectionDelegate that will inform the toolbar of selection changes.
     * @param titleResId The resource id of the title string. May be 0 if this class shouldn't set
     *                   set a title when the selection is cleared.
     * @param drawerLayout The DrawerLayout whose navigation icon is displayed in this toolbar.
     * @param normalGroupResId The resource id of the menu group to show when a selection isn't
     *                         established.
     * @param selectedGroupResId The resource id of the menu item to show when a selection is
     *                           established.
     * @param normalBackgroundColorResId The resource id of the color to use as the background color
     *                                   when selection is not enabled. If null the default appbar
     *                                   background color will be used.
     * @param hideShadowOnLargeTablets Whether the toolbar shadow should be hidden on large tablets.
     * @param listener The OnMenuItemClickListener to set on the toolbar.
     * @return The initialized SelectionToolbar.
     */
    public SelectionToolbar<E> initializeToolbar(int toolbarLayoutId,
            SelectionDelegate<E> delegate, int titleResId, @Nullable DrawerLayout drawerLayout,
            int normalGroupResId, int selectedGroupResId,
            @Nullable Integer normalBackgroundColorResId, boolean hideShadowOnLargeTablets,
            OnMenuItemClickListener listener) {

        FadingShadowView shadow = (FadingShadowView) findViewById(R.id.shadow);
        if (hideShadowOnLargeTablets && DeviceFormFactor.isLargeTablet(getContext())) {
            shadow.setVisibility(View.GONE);
        } else {
            shadow.init(ApiCompatibilityUtils.getColor(getResources(),
                    R.color.toolbar_shadow_color), FadingShadow.POSITION_TOP);
        }

        mToolbarStub.setLayoutResource(toolbarLayoutId);
        @SuppressWarnings("unchecked")
        SelectionToolbar<E> toolbar = (SelectionToolbar<E>) mToolbarStub.inflate();
        mToolbar = toolbar;
        mToolbar.initialize(delegate, titleResId, drawerLayout, normalGroupResId,
                selectedGroupResId, normalBackgroundColorResId);
        mToolbar.setOnMenuItemClickListener(listener);
        return mToolbar;
    }

    /**
     * Initializes the view shown when the selectable list is empty.
     *
     * @param emptyDrawable The Drawable to show when the selectable list is empty.
     * @param emptyStringResId The string to show when the selectable list is empty.
     * @return The {@link TextView} displayed when the list is empty.
     */
    public TextView initializeEmptyView(Drawable emptyDrawable, int emptyStringResId) {
        mEmptyView.setCompoundDrawablesWithIntrinsicBounds(null, emptyDrawable, null, null);
        mEmptyView.setText(emptyStringResId);
        return mEmptyView;
    }

    /**
     * @param emptyStringResId The string to show when the selectable list is empty.
     */
    public void setEmptyViewText(int emptyStringResId) {
        mEmptyView.setText(emptyStringResId);
    }

    /**
     * Called when the view that owns the SelectableListLayout is destroyed.
     */
    public void onDestroyed() {
        mAdapter.unregisterAdapterDataObserver(mAdapterObserver);
    }
}