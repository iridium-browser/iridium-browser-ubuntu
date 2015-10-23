// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.cast;

import android.support.v7.media.MediaRouter;

import org.chromium.chrome.browser.media.router.ChromeMediaRouter;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Callback for discovering routes with one particular filter. Keeps a set of all source URIs that
 * media sinks were requested for. Once a route is added or removed, updates the
 * {@link ChromeMediaRouter} with the new routes.
 */
public class DiscoveryCallback extends MediaRouter.Callback {
    private final ChromeMediaRouter mMediaRouter;
    private Set<String> mSourceUrns = new HashSet<String>();
    private List<MediaSink> mSinks = new ArrayList<MediaSink>();

    public DiscoveryCallback(String sourceUrn, ChromeMediaRouter mediaRouter) {
        assert mediaRouter != null;
        assert sourceUrn != null && !sourceUrn.isEmpty();
        mMediaRouter = mediaRouter;
        addSourceUrn(sourceUrn);
    }

    public void addSourceUrn(String sourceUrn) {
        if (mSourceUrns.add(sourceUrn)) {
            mMediaRouter.onSinksReceived(sourceUrn, new ArrayList<MediaSink>(mSinks));
        }
    }

    public void removeSourceUrn(String sourceUrn) {
        mSourceUrns.remove(sourceUrn);
    }

    public boolean isEmpty() {
        return mSourceUrns.isEmpty();
    }

    @Override
    public void onRouteAdded(MediaRouter router, MediaRouter.RouteInfo route) {
        MediaSink sink = MediaSink.fromRoute(route);
        if (mSinks.contains(sink)) return;
        mSinks.add(sink);
        updateChromeMediaRouter();
    }

    @Override
    public void onRouteRemoved(MediaRouter router, MediaRouter.RouteInfo route) {
        MediaSink sink = MediaSink.fromRoute(route);
        if (!mSinks.contains(sink)) return;
        mSinks.remove(sink);
        updateChromeMediaRouter();
    }

    private void updateChromeMediaRouter() {
        for (String sourceUrn : mSourceUrns) {
            mMediaRouter.onSinksReceived(sourceUrn, new ArrayList<MediaSink>(mSinks));
        }
    }
}