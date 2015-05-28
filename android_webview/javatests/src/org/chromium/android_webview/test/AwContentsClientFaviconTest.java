// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Build;
import android.test.suitebuilder.annotation.SmallTest;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.net.test.util.TestWebServer;

import java.io.InputStream;
import java.net.URL;
import java.util.HashMap;
import java.util.concurrent.Callable;

/**
 * Tests for the Favicon and TouchIcon related APIs.
 */
@MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT)
public class AwContentsClientFaviconTest extends AwTestBase {

    private static final String FAVICON1_URL = "/favicon1.png";
    private static final String FAVICON1_PAGE_URL = "/favicon1.html";
    private static final String FAVICON1_PAGE_HTML =
            CommonResources.makeHtmlPageFrom(
                    "<link rel=\"icon\" href=\"" + FAVICON1_URL + "\" />",
                    "Body");

    private static final String TOUCHICON_REL_LINK = "touch.png";
    private static final String TOUCHICON_REL_LINK_72 = "touch_72.png";
    private static final String TOUCHICON_REL_URL = "/" + TOUCHICON_REL_LINK;
    private static final String TOUCHICON_REL_URL_72 = "/" + TOUCHICON_REL_LINK_72;
    private static final String TOUCHICON_REL_PAGE_HTML =
            CommonResources.makeHtmlPageFrom(
                    "<link rel=\"apple-touch-icon\" href=\"" + TOUCHICON_REL_URL + "\" />"
                    + "<link rel=\"apple-touch-icon\" sizes=\"72x72\" href=\""
                    + TOUCHICON_REL_URL_72 + "\" />",
                    "Body");

    // Maximum number of milliseconds within which a request to web server is made.
    private static final long MAX_REQUEST_WAITING_LIMIT_MS = scaleTimeout(500);

    private static class FaviconHelper extends CallbackHelper {
        private Bitmap mIcon;
        private HashMap<String, Boolean> mTouchIcons = new HashMap<String, Boolean>();

        public void notifyFavicon(Bitmap icon) {
            mIcon = icon;
            super.notifyCalled();
        }

        public void notifyTouchIcon(String url, boolean precomposed) {
            mTouchIcons.put(url, precomposed);
            super.notifyCalled();
        }
    }

    private static class TestAwContentsClientBase
            extends org.chromium.android_webview.test.TestAwContentsClient {
        FaviconHelper mFaviconHelper = new FaviconHelper();
    }

    private static class TestAwContentsClientFavicon extends TestAwContentsClientBase {
        @Override
        public void onReceivedIcon(Bitmap bitmap) {
            // We don't inform the API client about the URL of the icon.
            mFaviconHelper.notifyFavicon(bitmap);
        }
    }

    private static class TestAwContentsClientTouchIcon extends TestAwContentsClientBase {
        @Override
        public void onReceivedTouchIconUrl(String url, boolean precomposed) {
            mFaviconHelper.notifyTouchIcon(url, precomposed);
        }
    }

    private TestAwContentsClientBase mContentsClient;
    private AwContents mAwContents;
    private TestWebServer mWebServer;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        AwContents.setShouldDownloadFavicons();
        mWebServer = TestWebServer.start();
    }

    private void init(TestAwContentsClientBase contentsClient) throws Exception {
        mContentsClient = contentsClient;
        AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
    }

    @Override
    protected void tearDown() throws Exception {
        if (mWebServer != null) mWebServer.shutdown();
        super.tearDown();
    }

    @SmallTest
    public void testReceiveBasicFavicon() throws Throwable {
        init(new TestAwContentsClientFavicon());
        int callCount = mContentsClient.mFaviconHelper.getCallCount();

        final String faviconUrl = mWebServer.setResponseBase64(FAVICON1_URL,
                CommonResources.FAVICON_DATA_BASE64, CommonResources.getImagePngHeaders(true));
        final String pageUrl = mWebServer.setResponse(FAVICON1_PAGE_URL, FAVICON1_PAGE_HTML,
                CommonResources.getTextHtmlHeaders(true));

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);

        mContentsClient.mFaviconHelper.waitForCallback(callCount);
        assertEquals(1, mWebServer.getRequestCount(FAVICON1_URL));
        Object originalFaviconSource = (new URL(faviconUrl)).getContent();
        Bitmap originalFavicon = BitmapFactory.decodeStream((InputStream) originalFaviconSource);
        assertNotNull(originalFavicon);
        assertNotNull(mContentsClient.mFaviconHelper.mIcon);
        assertTrue(mContentsClient.mFaviconHelper.mIcon.sameAs(originalFavicon));

        // Make sure the request counter for favicon is incremented when the page is loaded again
        // successfully.
        loadUrlAsync(mAwContents, pageUrl);
        mContentsClient.mFaviconHelper.waitForCallback(callCount);
        assertEquals(2, mWebServer.getRequestCount(FAVICON1_URL));
    }

    @SmallTest
    public void testDoNotMakeRequestForFaviconAfter404() throws Throwable {
        init(new TestAwContentsClientFavicon());

        mWebServer.setResponseWithNotFoundStatus(FAVICON1_URL);
        final String pageUrl = mWebServer.setResponse(FAVICON1_PAGE_URL, FAVICON1_PAGE_HTML,
                CommonResources.getTextHtmlHeaders(true));

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mWebServer.getRequestCount(FAVICON1_URL) == 1;
            }
        });

        // Make sure the request counter for favicon is not incremented, since we already got 404.
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        // If a request hasn't been done within this time period, we assume it won't be done.
        Thread.sleep(MAX_REQUEST_WAITING_LIMIT_MS);
        assertEquals(1, mWebServer.getRequestCount(FAVICON1_URL));
    }

    @SmallTest
    public void testReceiveBasicTouchIconLinkRel() throws Throwable {
        init(new TestAwContentsClientTouchIcon());
        int callCount = mContentsClient.mFaviconHelper.getCallCount();

        final String pageUrl = mWebServer.setResponse(TOUCHICON_REL_URL, TOUCHICON_REL_PAGE_HTML,
                CommonResources.getTextHtmlHeaders(true));

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);

        mContentsClient.mFaviconHelper.waitForCallback(callCount, 2);
        HashMap<String, Boolean> touchIcons = mContentsClient.mFaviconHelper.mTouchIcons;
        assertEquals(2, touchIcons.size());
        assertFalse(touchIcons.get(mWebServer.getBaseUrl() + TOUCHICON_REL_LINK));
        assertFalse(touchIcons.get(mWebServer.getBaseUrl() + TOUCHICON_REL_LINK_72));
    }
}
