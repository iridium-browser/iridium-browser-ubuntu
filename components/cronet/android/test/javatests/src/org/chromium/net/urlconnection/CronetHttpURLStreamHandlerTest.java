// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.net.CronetTestActivity;
import org.chromium.net.CronetTestBase;
import org.chromium.net.NativeTestServer;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.InetSocketAddress;
import java.net.Proxy;
import java.net.URL;

public class CronetHttpURLStreamHandlerTest extends CronetTestBase {
    private CronetTestActivity mActivity;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mActivity = launchCronetTestApp();
        assertTrue(NativeTestServer.startNativeTestServer(
                getInstrumentation().getTargetContext()));
    }

    @Override
    protected void tearDown() throws Exception {
        NativeTestServer.shutdownNativeTestServer();
        super.tearDown();
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testOpenConnectionHttp() throws Exception {
        URL url = new URL(NativeTestServer.getEchoMethodURL());
        CronetHttpURLStreamHandler streamHandler =
                new CronetHttpURLStreamHandler(mActivity.mUrlRequestContext);
        HttpURLConnection connection =
                (HttpURLConnection) streamHandler.openConnection(url);
        assertEquals(200, connection.getResponseCode());
        assertEquals("OK", connection.getResponseMessage());
        assertEquals("GET", getResponseAsString(connection));
        connection.disconnect();
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testOpenConnectionHttps() throws Exception {
        URL url = new URL("https://example.com");
        CronetHttpURLStreamHandler streamHandler =
                new CronetHttpURLStreamHandler(mActivity.mUrlRequestContext);
        HttpURLConnection connection =
                (HttpURLConnection) streamHandler.openConnection(url);
        assertNotNull(connection);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testOpenConnectionProtocolNotSupported() throws Exception {
        URL url = new URL("ftp://example.com");
        CronetHttpURLStreamHandler streamHandler =
                new CronetHttpURLStreamHandler(mActivity.mUrlRequestContext);
        try {
            streamHandler.openConnection(url);
            fail();
        } catch (UnsupportedOperationException e) {
            assertEquals("Unexpected protocol:ftp", e.getMessage());
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testOpenConnectionWithProxy() throws Exception {
        URL url = new URL(NativeTestServer.getEchoMethodURL());
        CronetHttpURLStreamHandler streamHandler =
                new CronetHttpURLStreamHandler(mActivity.mUrlRequestContext);
        Proxy proxy = new Proxy(Proxy.Type.HTTP,
                new InetSocketAddress("127.0.0.1", 8080));
        try {
            streamHandler.openConnection(url, proxy);
            fail();
        } catch (UnsupportedOperationException e) {
            // Expected.
        }
    }

    /**
     * Helper method to extract response body as a string for testing.
     */
    // TODO(xunjieli): consider moving this helper method to a util class.
    private String getResponseAsString(HttpURLConnection connection)
            throws Exception {
        InputStream in = connection.getInputStream();
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        int b;
        while ((b = in.read()) != -1) {
            out.write(b);
        }
        return out.toString();
    }
}
