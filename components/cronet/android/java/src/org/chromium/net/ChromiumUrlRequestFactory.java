// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.os.Build;

import org.chromium.base.annotations.UsedByReflection;

import java.nio.channels.WritableByteChannel;
import java.util.Map;

/**
 * Network request factory using the native http stack implementation.
 * @deprecated Use {@link CronetUrlRequestContext} instead.
 */
@UsedByReflection("HttpUrlRequestFactory.java")
@Deprecated
public class ChromiumUrlRequestFactory extends HttpUrlRequestFactory {
    private ChromiumUrlRequestContext mRequestContext;

    @UsedByReflection("HttpUrlRequestFactory.java")
    public ChromiumUrlRequestFactory(
            Context context, UrlRequestContextConfig config) {
        if (isEnabled()) {
            String userAgent = config.userAgent();
            if (userAgent.isEmpty()) {
                userAgent = UserAgent.from(context);
            }
            mRequestContext = new ChromiumUrlRequestContext(context,
                    userAgent, config);
        }
    }

    @Override
    public boolean isEnabled() {
        return Build.VERSION.SDK_INT >= 14;
    }

    @Override
    public String getName() {
        return "Chromium/" + ChromiumUrlRequestContext.getVersion();
    }

    @Override
    public ChromiumUrlRequest createRequest(String url, int requestPriority,
            Map<String, String> headers, HttpUrlRequestListener listener) {
        return new ChromiumUrlRequest(mRequestContext, url, requestPriority,
                headers, listener);
    }

    @Override
    public ChromiumUrlRequest createRequest(String url, int requestPriority,
            Map<String, String> headers, WritableByteChannel channel,
            HttpUrlRequestListener listener) {
        return new ChromiumUrlRequest(mRequestContext, url, requestPriority,
                headers, channel, listener);
    }

    @Override
    public void startNetLogToFile(String fileName, boolean logAll) {
        mRequestContext.startNetLogToFile(fileName, logAll);
    }

    @Override
    public void stopNetLog() {
        mRequestContext.stopNetLog();
    }

    public ChromiumUrlRequestContext getRequestContext() {
        return mRequestContext;
    }
}
