// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.variations.firstrun;

import android.app.IntentService;
import android.content.Intent;
import android.os.SystemClock;
import android.support.v4.content.LocalBroadcastManager;

import org.chromium.base.Log;
import org.chromium.base.metrics.CachedMetrics.SparseHistogramSample;
import org.chromium.base.metrics.CachedMetrics.TimesHistogramSample;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.SocketTimeoutException;
import java.net.URL;
import java.net.UnknownHostException;
import java.util.concurrent.TimeUnit;

/**
 * Background service that fetches the variations seed before the actual first run of Chrome.
 */
public class VariationsSeedService extends IntentService {
    private static final String TAG = "VariationsSeedServ";

    public static final String COMPLETE_BROADCAST = "VariationsseedService.Complete";

    private static final String VARIATIONS_SERVER_URL =
            "https://clientservices.googleapis.com/chrome-variations/seed?osname=android";
    private static final int BUFFER_SIZE = 4096;
    private static final int READ_TIMEOUT = 3000; // time in ms
    private static final int REQUEST_TIMEOUT = 1000; // time in ms

    // Values for the "Variations.FirstRun.SeedFetchResult" sparse histogram, which also logs
    // HTTP result codes. These are negative so that they don't conflict with the HTTP codes.
    // These values should not be renumbered or re-used since they are logged to UMA.
    private static final int SEED_FETCH_RESULT_UNKNOWN_HOST_EXCEPTION = -3;
    private static final int SEED_FETCH_RESULT_TIMEOUT = -2;
    private static final int SEED_FETCH_RESULT_IOEXCEPTION = -1;

    public VariationsSeedService() {
        super(TAG);
    }

    @Override
    public void onHandleIntent(Intent intent) {
        // Early return if the seed has already been fetched. In such a case, either the Java-side
        // variations seed pref is set, or a different Java pref is set that indicates that the
        // seed exists in the native prefs.
        // Note: There is no need to check for a concurrent seed fetch here, because the service
        // runs all its intents on the same worker thread serially.
        if (VariationsSeedBridge.hasJavaPref(getApplicationContext())
                || VariationsSeedBridge.hasNativePref(getApplicationContext())) {
            broadcastCompleteIntent();
            return;
        }
        try {
            downloadContent();
        } finally {
            broadcastCompleteIntent();
        }
    }

    private void broadcastCompleteIntent() {
        LocalBroadcastManager.getInstance(this).sendBroadcast(new Intent(COMPLETE_BROADCAST));
    }

    private void recordFetchResultOrCode(int resultOrCode) {
        SparseHistogramSample histogram =
                new SparseHistogramSample("Variations.FirstRun.SeedFetchResult");
        histogram.record(resultOrCode);
    }

    private void recordSeedFetchTime(long timeDeltaMillis) {
        Log.i(TAG, "Fetched first run seed in " + timeDeltaMillis + " ms");
        TimesHistogramSample histogram = new TimesHistogramSample(
                "Variations.FirstRun.SeedFetchTime", TimeUnit.MILLISECONDS);
        histogram.record(timeDeltaMillis);
    }

    private void recordSeedConnectTime(long timeDeltaMillis) {
        TimesHistogramSample histogram = new TimesHistogramSample(
                "Variations.FirstRun.SeedConnectTime", TimeUnit.MILLISECONDS);
        histogram.record(timeDeltaMillis);
    }

    private boolean downloadContent() {
        HttpURLConnection connection = null;
        try {
            long startTimeMillis = SystemClock.elapsedRealtime();
            URL url = new URL(VARIATIONS_SERVER_URL);
            connection = (HttpURLConnection) url.openConnection();
            connection.setReadTimeout(READ_TIMEOUT);
            connection.setConnectTimeout(REQUEST_TIMEOUT);
            connection.setDoInput(true);
            connection.setRequestProperty("A-IM", "gzip");
            connection.connect();
            int responseCode = connection.getResponseCode();
            recordFetchResultOrCode(responseCode);
            if (responseCode != HttpURLConnection.HTTP_OK) {
                Log.w(TAG, "Non-OK response code = %d", responseCode);
                return false;
            }

            recordSeedConnectTime(SystemClock.elapsedRealtime() - startTimeMillis);
            // Convert the InputStream into a byte array.
            byte[] rawSeed = getRawSeed(connection);
            String signature = getHeaderFieldOrEmpty(connection, "X-Seed-Signature");
            String country = getHeaderFieldOrEmpty(connection, "X-Country");
            String date = getHeaderFieldOrEmpty(connection, "Date");
            boolean isGzipCompressed = getHeaderFieldOrEmpty(connection, "IM").equals("gzip");
            VariationsSeedBridge.setVariationsFirstRunSeed(
                    getApplicationContext(), rawSeed, signature, country, date, isGzipCompressed);
            recordSeedFetchTime(SystemClock.elapsedRealtime() - startTimeMillis);
            return true;
        } catch (SocketTimeoutException e) {
            recordFetchResultOrCode(SEED_FETCH_RESULT_TIMEOUT);
            Log.w(TAG, "SocketTimeoutException fetching first run seed: ", e);
            return false;
        } catch (UnknownHostException e) {
            recordFetchResultOrCode(SEED_FETCH_RESULT_UNKNOWN_HOST_EXCEPTION);
            Log.w(TAG, "UnknownHostException fetching first run seed: ", e);
            return false;
        } catch (IOException e) {
            recordFetchResultOrCode(SEED_FETCH_RESULT_IOEXCEPTION);
            Log.w(TAG, "IOException fetching first run seed: ", e);
            return false;
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }
    }

    private String getHeaderFieldOrEmpty(HttpURLConnection connection, String name) {
        String headerField = connection.getHeaderField(name);
        if (headerField == null) {
            return "";
        }
        return headerField.trim();
    }

    private byte[] getRawSeed(HttpURLConnection connection) throws IOException {
        InputStream inputStream = null;
        try {
            inputStream = connection.getInputStream();
            return convertInputStreamToByteArray(inputStream);
        } finally {
            if (inputStream != null) {
                inputStream.close();
            }
        }
    }

    private byte[] convertInputStreamToByteArray(InputStream inputStream) throws IOException {
        ByteArrayOutputStream byteBuffer = new ByteArrayOutputStream();
        byte[] buffer = new byte[BUFFER_SIZE];
        int charactersReadCount = 0;
        while ((charactersReadCount = inputStream.read(buffer)) != -1) {
            byteBuffer.write(buffer, 0, charactersReadCount);
        }
        return byteBuffer.toByteArray();
    }
}
