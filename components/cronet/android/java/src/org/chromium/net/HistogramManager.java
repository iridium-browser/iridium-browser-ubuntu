// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import java.lang.reflect.Constructor;


/**
 * Controls UMA histograms in native library.
 */
public abstract class HistogramManager {
    private static final String CRONET_HISTOGRAM_MANAGER =
            "org.chromium.net.CronetHistogramManager";

    /**
     * Get histogram deltas serialized as protobuf.
     */
    public abstract byte[] getHistogramDeltas();

    /**
     * Creates Histogram Manager if native library is loaded, returns null if not.
     */
    public static HistogramManager createHistogramManager() {
        HistogramManager histogramManager = null;
        try {
            Class<? extends HistogramManager> histogramManagerClass =
                    HistogramManager.class.getClassLoader()
                            .loadClass(CRONET_HISTOGRAM_MANAGER)
                            .asSubclass(HistogramManager.class);
            Constructor<? extends HistogramManager> constructor =
                    histogramManagerClass.getConstructor();
            histogramManager = constructor.newInstance();
        } catch (ClassNotFoundException e) {
            // Leave as null.
        } catch (Exception e) {
            throw new IllegalStateException(
                    "Cannot instantiate: " + CRONET_HISTOGRAM_MANAGER,
                    e);
        }
        return histogramManager;
    }
}
