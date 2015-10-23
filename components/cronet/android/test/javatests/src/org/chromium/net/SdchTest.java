// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.os.ConditionVariable;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Tests Sdch support.
 */
public class SdchTest extends CronetTestBase {
    private CronetTestActivity mActivity;

    private enum Sdch {
        ENABLED,
        DISABLED,
    }

    private enum Api {
        LEGACY,
        ASYNC,
    }

    private void setUp(Sdch setting, Api api) {
        List<String> commandLineArgs = new ArrayList<String>();
        commandLineArgs.add(CronetTestActivity.CACHE_KEY);
        commandLineArgs.add(CronetTestActivity.CACHE_DISK);
        if (setting == Sdch.ENABLED) {
            commandLineArgs.add(CronetTestActivity.SDCH_KEY);
            commandLineArgs.add(CronetTestActivity.SDCH_ENABLE);
        }

        String[] args = new String[commandLineArgs.size()];
        mActivity =
                launchCronetTestAppWithUrlAndCommandLineArgs(null, commandLineArgs.toArray(args));
        long urlRequestContextAdapter = (api == Api.LEGACY)
                ? getContextAdapter((ChromiumUrlRequestFactory) mActivity.mRequestFactory)
                : getContextAdapter((CronetUrlRequestContext) mActivity.mUrlRequestContext);
        NativeTestServer.registerHostResolverProc(urlRequestContextAdapter, api == Api.LEGACY);
        // Start NativeTestServer.
        assertTrue(NativeTestServer.startNativeTestServer(getInstrumentation().getTargetContext()));
    }

    @Override
    protected void tearDown() throws Exception {
        NativeTestServer.shutdownNativeTestServer();
        super.tearDown();
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testSdchEnabled_LegacyApi() throws Exception {
        setUp(Sdch.ENABLED, Api.LEGACY);
        String targetUrl = NativeTestServer.getSdchURL() + "/sdch/test";
        long contextAdapter =
                getContextAdapter((ChromiumUrlRequestFactory) mActivity.mRequestFactory);
        DictionaryAddedObserver observer =
                new DictionaryAddedObserver(targetUrl, contextAdapter, true /** Legacy Api */);

        // Make a request to /sdch/index which advertises the dictionary.
        TestHttpUrlRequestListener listener1 =
                startAndWaitForComplete_LegacyApi(mActivity.mRequestFactory,
                        NativeTestServer.getSdchURL() + "/sdch/index?q=LeQxM80O");
        assertEquals(200, listener1.mHttpStatusCode);
        assertEquals("This is an index page.\n", listener1.mResponseAsString);
        assertEquals(Arrays.asList("/sdch/dict/LeQxM80O"),
                listener1.mResponseHeaders.get("Get-Dictionary"));

        observer.waitForDictionaryAdded();

        // Make a request to fetch encoded response at /sdch/test.
        TestHttpUrlRequestListener listener2 =
                startAndWaitForComplete_LegacyApi(mActivity.mRequestFactory, targetUrl);
        assertEquals(200, listener2.mHttpStatusCode);
        assertEquals("The quick brown fox jumps over the lazy dog.\n", listener2.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testSdchDisabled_LegacyApi() throws Exception {
        setUp(Sdch.DISABLED, Api.LEGACY);
        // Make a request to /sdch/index.
        // Since Sdch is not enabled, no dictionary should be advertised.
        TestHttpUrlRequestListener listener =
                startAndWaitForComplete_LegacyApi(mActivity.mRequestFactory,
                        NativeTestServer.getSdchURL() + "/sdch/index?q=LeQxM80O");
        assertEquals(200, listener.mHttpStatusCode);
        assertEquals("This is an index page.\n", listener.mResponseAsString);
        assertEquals(null, listener.mResponseHeaders.get("Get-Dictionary"));
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testDictionaryNotFound_LegacyApi() throws Exception {
        setUp(Sdch.ENABLED, Api.LEGACY);
        // Make a request to /sdch/index which advertises a bad dictionary that
        // does not exist.
        TestHttpUrlRequestListener listener1 =
                startAndWaitForComplete_LegacyApi(mActivity.mRequestFactory,
                        NativeTestServer.getSdchURL() + "/sdch/index?q=NotFound");
        assertEquals(200, listener1.mHttpStatusCode);
        assertEquals("This is an index page.\n", listener1.mResponseAsString);
        assertEquals(Arrays.asList("/sdch/dict/NotFound"),
                listener1.mResponseHeaders.get("Get-Dictionary"));

        // Make a request to fetch /sdch/test, and make sure request succeeds.
        TestHttpUrlRequestListener listener2 = startAndWaitForComplete_LegacyApi(
                mActivity.mRequestFactory, NativeTestServer.getSdchURL() + "/sdch/test");
        assertEquals(200, listener2.mHttpStatusCode);
        assertEquals("Sdch is not used.\n", listener2.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testSdchEnabled() throws Exception {
        setUp(Sdch.ENABLED, Api.ASYNC);
        String targetUrl = NativeTestServer.getSdchURL() + "/sdch/test";
        long contextAdapter =
                getContextAdapter((CronetUrlRequestContext) mActivity.mUrlRequestContext);
        DictionaryAddedObserver observer =
                new DictionaryAddedObserver(targetUrl, contextAdapter, false /** Legacy Api */);

        // Make a request to /sdch which advertises the dictionary.
        TestUrlRequestListener listener1 = startAndWaitForComplete(mActivity.mUrlRequestContext,
                NativeTestServer.getSdchURL() + "/sdch/index?q=LeQxM80O");
        assertEquals(200, listener1.mResponseInfo.getHttpStatusCode());
        assertEquals("This is an index page.\n", listener1.mResponseAsString);
        assertEquals(Arrays.asList("/sdch/dict/LeQxM80O"),
                listener1.mResponseInfo.getAllHeaders().get("Get-Dictionary"));

        observer.waitForDictionaryAdded();

        // Make a request to fetch encoded response at /sdch/test.
        TestUrlRequestListener listener2 =
                startAndWaitForComplete(mActivity.mUrlRequestContext, targetUrl);
        assertEquals(200, listener2.mResponseInfo.getHttpStatusCode());
        assertEquals("The quick brown fox jumps over the lazy dog.\n", listener2.mResponseAsString);

        // Wait for a bit until SimpleCache finished closing entries before
        // calling shutdown on the UrlRequestContext.
        // TODO(xunjieli): Remove once crbug.com/486120 is fixed.
        Thread.sleep(5000);
        mActivity.mUrlRequestContext.shutdown();

        // Shutting down the context will make JsonPrefStore to flush pending
        // writes to disk.
        String dictUrl = NativeTestServer.getSdchURL() + "/sdch/dict/LeQxM80O";
        assertTrue(fileContainsString("local_prefs.json", dictUrl));

        // Test persistence.
        CronetUrlRequestContext newContext = new CronetUrlRequestContext(
                getInstrumentation().getTargetContext(), mActivity.getContextConfig());

        long newContextAdapter = getContextAdapter(newContext);
        NativeTestServer.registerHostResolverProc(newContextAdapter, false);
        DictionaryAddedObserver newObserver =
                new DictionaryAddedObserver(targetUrl, newContextAdapter, false /** Legacy Api */);
        newObserver.waitForDictionaryAdded();

        // Make a request to fetch encoded response at /sdch/test.
        TestUrlRequestListener listener3 = startAndWaitForComplete(newContext, targetUrl);
        assertEquals(200, listener3.mResponseInfo.getHttpStatusCode());
        assertEquals("The quick brown fox jumps over the lazy dog.\n", listener3.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testSdchDisabled() throws Exception {
        setUp(Sdch.DISABLED, Api.ASYNC);
        // Make a request to /sdch.
        // Since Sdch is not enabled, no dictionary should be advertised.
        TestUrlRequestListener listener = startAndWaitForComplete(mActivity.mUrlRequestContext,
                NativeTestServer.getSdchURL() + "/sdch/index?q=LeQxM80O");
        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals("This is an index page.\n", listener.mResponseAsString);
        assertEquals(null, listener.mResponseInfo.getAllHeaders().get("Get-Dictionary"));
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testDictionaryNotFound() throws Exception {
        setUp(Sdch.ENABLED, Api.ASYNC);
        // Make a request to /sdch/index which advertises a bad dictionary that
        // does not exist.
        TestUrlRequestListener listener1 = startAndWaitForComplete(mActivity.mUrlRequestContext,
                NativeTestServer.getSdchURL() + "/sdch/index?q=NotFound");
        assertEquals(200, listener1.mResponseInfo.getHttpStatusCode());
        assertEquals("This is an index page.\n", listener1.mResponseAsString);
        assertEquals(Arrays.asList("/sdch/dict/NotFound"),
                listener1.mResponseInfo.getAllHeaders().get("Get-Dictionary"));

        // Make a request to fetch /sdch/test, and make sure Sdch encoding is not used.
        TestUrlRequestListener listener2 = startAndWaitForComplete(
                mActivity.mUrlRequestContext, NativeTestServer.getSdchURL() + "/sdch/test");
        assertEquals(200, listener2.mResponseInfo.getHttpStatusCode());
        assertEquals("Sdch is not used.\n", listener2.mResponseAsString);
    }

    private static class DictionaryAddedObserver extends SdchObserver {
        ConditionVariable mBlock = new ConditionVariable();

        public DictionaryAddedObserver(String targetUrl, long contextAdapter, boolean isLegacyAPI) {
            super(targetUrl, contextAdapter, isLegacyAPI);
        }

        @Override
        public void onDictionaryAdded() {
            mBlock.open();
        }

        public void waitForDictionaryAdded() {
            if (!mDictionaryAlreadyPresent) {
                mBlock.block();
                mBlock.close();
            }
        }
    }

    private long getContextAdapter(ChromiumUrlRequestFactory factory) {
        return factory.getRequestContext().getUrlRequestContextAdapter();
    }

    private long getContextAdapter(CronetUrlRequestContext requestContext) {
        return requestContext.getUrlRequestContextAdapter();
    }

    private TestHttpUrlRequestListener startAndWaitForComplete_LegacyApi(
            HttpUrlRequestFactory factory, String url) throws Exception {
        Map<String, String> headers = new HashMap<String, String>();
        TestHttpUrlRequestListener listener = new TestHttpUrlRequestListener();
        HttpUrlRequest request = factory.createRequest(
                url, HttpUrlRequest.REQUEST_PRIORITY_MEDIUM, headers, listener);
        request.start();
        listener.blockForComplete();
        return listener;
    }

    private TestUrlRequestListener startAndWaitForComplete(
            UrlRequestContext requestContext, String url) throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest request = requestContext.createRequest(url, listener, listener.getExecutor());
        request.start();
        listener.blockForDone();
        return listener;
    }

    // Returns whether a file contains a particular string.
    private boolean fileContainsString(String filename, String content) throws IOException {
        BufferedReader reader =
                new BufferedReader(new FileReader(mActivity.getTestStorage() + "/" + filename));
        String line;
        while ((line = reader.readLine()) != null) {
            if (line.contains(content)) {
                reader.close();
                return true;
            }
        }
        reader.close();
        return false;
    }
}
