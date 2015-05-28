// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.ContextWrapper;
import android.view.LayoutInflater;

import org.chromium.base.annotations.SuppressFBWarnings;

/**
 * This class allows us to wrap the application context so that the WebView implementation can
 * correctly reference both org.chromium.* and application classes which is necessary to properly
 * inflate UI.
 */
public class ResourcesContextWrapperFactory {
    private ResourcesContextWrapperFactory() {}

    public static Context get(Context ctx) {
        // Avoid double-wrapping a context.
        if (ctx instanceof WebViewContextWrapper) {
            return ctx;
        }
        return new WebViewContextWrapper(ctx);
    }

    private static class WebViewContextWrapper extends ContextWrapper {
        private Context mApplicationContext;

        public WebViewContextWrapper(Context base) {
            super(base);
        }

        @SuppressFBWarnings("DP_CREATE_CLASSLOADER_INSIDE_DO_PRIVILEGED")
        @Override
        public ClassLoader getClassLoader() {
            final ClassLoader appCl = getBaseContext().getClassLoader();
            final ClassLoader webViewCl = this.getClass().getClassLoader();
            return new ClassLoader() {
                @Override
                protected Class<?> findClass(String name) throws ClassNotFoundException {
                    // First look in the WebViewProvider class loader.
                    try {
                        return webViewCl.loadClass(name);
                    } catch (ClassNotFoundException e) {
                        // Look in the app class loader; allowing it to throw
                        // ClassNotFoundException.
                        return appCl.loadClass(name);
                    }
                }
            };
        }

        @Override
        public Object getSystemService(String name) {
            if (Context.LAYOUT_INFLATER_SERVICE.equals(name)) {
                LayoutInflater i = (LayoutInflater) getBaseContext().getSystemService(name);
                return i.cloneInContext(this);
            } else {
                return getBaseContext().getSystemService(name);
            }
        }

        @Override
        public Context getApplicationContext() {
            if (mApplicationContext == null) {
                Context appCtx = getBaseContext().getApplicationContext();
                if (appCtx == getBaseContext()) {
                    mApplicationContext = this;
                } else {
                    mApplicationContext = get(appCtx);
                }
            }
            return mApplicationContext;
        }

        @Override
        public void registerComponentCallbacks(ComponentCallbacks callback) {
            // We have to override registerComponentCallbacks and unregisterComponentCallbacks
            // since they call getApplicationContext().[un]registerComponentCallbacks()
            // which causes us to go into a loop.
            getBaseContext().registerComponentCallbacks(callback);
        }

        @Override
        public void unregisterComponentCallbacks(ComponentCallbacks callback) {
            getBaseContext().unregisterComponentCallbacks(callback);
        }
    }
}
