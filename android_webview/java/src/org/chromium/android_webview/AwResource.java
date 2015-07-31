// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.res.Resources;
import android.util.SparseArray;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

import java.io.IOException;
import java.io.InputStreamReader;
import java.lang.ref.SoftReference;
import java.util.NoSuchElementException;
import java.util.Scanner;

/**
 * A class that defines a set of resource IDs and functionality to resolve
 * those IDs to concrete resources.
 */
@JNINamespace("android_webview::AwResource")
public class AwResource {
    // The following resource ID's must be initialized by the embedder.

    // Raw resource ID for an HTML page to be displayed in the case of
    // a specific load error.
    private static int sRawLoadError;

    // Raw resource ID for an HTML page to be displayed in the case of
    // a generic load error. (It's called NO_DOMAIN for legacy reasons).
    private static int sRawNoDomain;

    // Array resource ID for the configuration of platform specific key-systems.
    private static int sStringArrayConfigKeySystemUUIDMapping;

    // The embedder should inject a Resources object that will be used
    // to resolve Resource IDs into the actual resources.
    private static Resources sResources;

    // Loading some resources is expensive, so cache the results.
    private static SparseArray<SoftReference<String>> sResourceCache;

    private static final int TYPE_STRING = 0;
    private static final int TYPE_RAW = 1;

    public static void setResources(Resources resources) {
        sResources = resources;
        sResourceCache = new SparseArray<SoftReference<String>>();
    }

    public static void setErrorPageResources(int loaderror, int nodomain) {
        sRawLoadError = loaderror;
        sRawNoDomain = nodomain;
    }

    public static void setConfigKeySystemUuidMapping(int config) {
        sStringArrayConfigKeySystemUUIDMapping = config;
    }

    @CalledByNative
    private static String getNoDomainPageContent() {
        return getResource(sRawNoDomain, TYPE_RAW);
    }

    @CalledByNative
    private static String getLoadErrorPageContent() {
        return getResource(sRawLoadError, TYPE_RAW);
    }

    @CalledByNative
    private static String[] getConfigKeySystemUuidMapping() {
        // No need to cache, since this should be called only once.
        return sResources.getStringArray(sStringArrayConfigKeySystemUUIDMapping);
    }

    private static String getResource(int resid, int type) {
        assert resid != 0;
        assert sResources != null;
        assert sResourceCache != null;

        SoftReference<String> stringRef = sResourceCache.get(resid);
        String result = stringRef == null ? null : stringRef.get();
        if (result == null) {
            switch (type) {
                case TYPE_STRING:
                    result = sResources.getString(resid);
                    break;
                case TYPE_RAW:
                    result = getRawFileResourceContent(resid);
                    break;
                default:
                    throw new IllegalArgumentException("Unknown resource type");
            }

            sResourceCache.put(resid, new SoftReference<String>(result));
        }
        return result;
    }

    private static String getRawFileResourceContent(int resid) {
        assert resid != 0;
        assert sResources != null;

        InputStreamReader isr = null;
        String result = null;

        try {
            isr = new InputStreamReader(
                    sResources.openRawResource(resid));
            // \A tells the scanner to use the beginning of the input
            // as the delimiter, hence causes it to read the entire text.
            result = new Scanner(isr).useDelimiter("\\A").next();
        } catch (Resources.NotFoundException e) {
            return "";
        } catch (NoSuchElementException e) {
            return "";
        } finally {
            try {
                if (isr != null) {
                    isr.close();
                }
            } catch (IOException e) {
                // Nothing to do if close() fails.
            }
        }
        return result;
    }
}
