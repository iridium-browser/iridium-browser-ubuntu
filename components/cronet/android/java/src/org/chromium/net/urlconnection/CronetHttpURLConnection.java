// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import android.util.Log;
import android.util.Pair;

import org.chromium.net.ExtendedResponseInfo;
import org.chromium.net.ResponseInfo;
import org.chromium.net.UploadDataProvider;
import org.chromium.net.UrlRequest;
import org.chromium.net.UrlRequestContext;
import org.chromium.net.UrlRequestException;
import org.chromium.net.UrlRequestListener;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.ProtocolException;
import java.net.URL;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

/**
 * An implementation of HttpURLConnection that uses Cronet to send requests and
 * receive response. This class inherits a {@code connected} field from the
 * superclass. That field indicates whether a connection has ever been
 * attempted.
 */
public class CronetHttpURLConnection extends HttpURLConnection {
    private static final String TAG = "CronetHttpURLConnection";
    private static final String CONTENT_LENGTH = "Content-Length";
    private final UrlRequestContext mUrlRequestContext;
    private final MessageLoop mMessageLoop;
    private final UrlRequest mRequest;
    private final List<Pair<String, String>> mRequestHeaders;

    private CronetInputStream mInputStream;
    private CronetOutputStream mOutputStream;
    private ResponseInfo mResponseInfo;
    private UrlRequestException mException;
    private boolean mOnRedirectCalled = false;
    private boolean mHasResponse = false;

    public CronetHttpURLConnection(URL url,
            UrlRequestContext urlRequestContext) {
        super(url);
        mUrlRequestContext = urlRequestContext;
        mMessageLoop = new MessageLoop();
        mRequest = mUrlRequestContext.createRequest(url.toString(),
                new CronetUrlRequestListener(), mMessageLoop);
        mInputStream = new CronetInputStream(this);
        mRequestHeaders = new ArrayList<Pair<String, String>>();
    }

    /**
     * Opens a connection to the resource. If the connect method is called when
     * the connection has already been opened (indicated by the connected field
     * having the value true), the call is ignored.
     */
    @Override
    public void connect() throws IOException {
        startRequest();
    }

    /**
     * Releases this connection so that its resources may be either reused or
     * closed.
     */
    @Override
    public void disconnect() {
        // Disconnect before connection is made should have no effect.
        if (connected && mInputStream != null) {
            try {
                mInputStream.close();
            } catch (IOException e) {
                e.printStackTrace();
            }
            mInputStream = null;
            mRequest.cancel();
        }
    }

    /**
     * Returns the response message returned by the remote HTTP server.
     */
    @Override
    public String getResponseMessage() throws IOException {
        getResponse();
        return mResponseInfo.getHttpStatusText();
    }

    /**
     * Returns the response code returned by the remote HTTP server.
     */
    @Override
    public int getResponseCode() throws IOException {
        getResponse();
        return mResponseInfo.getHttpStatusCode();
    }

    /**
     * Returns an unmodifiable map of the response-header fields and values.
     */
    @Override
    public Map<String, List<String>> getHeaderFields() {
        try {
            getResponse();
        } catch (IOException e) {
            return Collections.emptyMap();
        }
        return mResponseInfo.getAllHeaders();
    }

    /**
     * Returns the value of the named header field. If called on a connection
     * that sets the same header multiple times with possibly different values,
     * only the last value is returned.
     */
    @Override
    public final String getHeaderField(String fieldName) {
        try {
            getResponse();
        } catch (IOException e) {
            return null;
        }
        Map<String, List<String>> map = mResponseInfo.getAllHeaders();
        if (!map.containsKey(fieldName)) {
            return null;
        }
        List<String> values = map.get(fieldName);
        return values.get(values.size() - 1);
    }

    /**
     * Returns the name of the header field at the given position pos, or null
     * if there are fewer than pos fields.
     */
    @Override
    public final String getHeaderFieldKey(int pos) {
        Pair<String, String> header = getHeaderFieldPair(pos);
        if (header == null) {
            return null;
        }
        return header.first;
    }

    /**
     * Returns the header value at the field position pos or null if the header
     * has fewer than pos fields.
     */
    @Override
    public final String getHeaderField(int pos) {
        Pair<String, String> header = getHeaderFieldPair(pos);
        if (header == null) {
            return null;
        }
        return header.second;
    }

    /**
     * Returns an InputStream for reading data from the resource pointed by this
     * URLConnection.
     * @throws FileNotFoundException if http response code is equal or greater
     *             than {@link HTTP_BAD_REQUEST}.
     * @throws IOException If the request gets a network error or HTTP error
     *             status code, or if the caller tried to read the response body
     *             of a redirect when redirects are disabled.
     */
    @Override
    public InputStream getInputStream() throws IOException {
        getResponse();
        if (!instanceFollowRedirects && mOnRedirectCalled) {
            throw new IOException("Cannot read response body of a redirect.");
        }
        // Emulate default implementation's behavior to throw
        // FileNotFoundException when we get a 400 and above.
        if (mResponseInfo.getHttpStatusCode() >= HTTP_BAD_REQUEST) {
            throw new FileNotFoundException(url.toString());
        }
        return mInputStream;
    }

    @Override
    public OutputStream getOutputStream() throws IOException {
        if (mOutputStream == null) {
            if (connected) {
                throw new ProtocolException(
                        "Cannot write to OutputStream after receiving response.");
            }
            long fixedStreamingModeContentLength = getStreamingModeContentLength();
            if (fixedStreamingModeContentLength != -1) {
                mOutputStream = new CronetFixedModeOutputStream(this,
                        fixedStreamingModeContentLength, mMessageLoop);
                // Start the request now since all headers can be sent.
                startRequest();
            } else {
                // For the buffered case, start the request only when
                // content-length bytes are received, or when a
                // connect action is initiated by the consumer.
                Log.d(TAG, "Outputstream is being buffered in memory.");
                String length = getRequestProperty(CONTENT_LENGTH);
                if (length == null) {
                    mOutputStream = new CronetBufferedOutputStream(this);
                } else {
                    long lengthParsed = Long.parseLong(length);
                    mOutputStream = new CronetBufferedOutputStream(this, lengthParsed);
                }
            }
        }
        return mOutputStream;
    }

    /**
     * Helper method to get content length passed in by
     * {@link #setFixedLengthStreamingMode}
     */
    private long getStreamingModeContentLength() {
        long contentLength = fixedContentLength;
        // Use reflection to see whether fixedContentLengthLong (only added
        // in API 19) is inherited.
        try {
            Class<?> parent = this.getClass();
            long superFixedContentLengthLong =
                    parent.getField("fixedContentLengthLong").getLong(this);
            if (superFixedContentLengthLong != -1) {
                contentLength = superFixedContentLengthLong;
            }
        } catch (Exception e) {
            // Ignored.
        }
        return contentLength;
    }

    /**
     * Starts the request if {@code connected} is false.
     */
    private void startRequest() throws IOException {
        if (connected) {
            return;
        }
        if (doOutput) {
            if (mOutputStream != null) {
                mRequest.setUploadDataProvider(
                        (UploadDataProvider) mOutputStream, mMessageLoop);
                if (getRequestProperty(CONTENT_LENGTH) == null) {
                    addRequestProperty(CONTENT_LENGTH,
                            Long.toString(((UploadDataProvider) mOutputStream).getLength()));
                }
                // Tells mOutputStream that startRequest() has been called, so
                // the underlying implementation can prepare for reading if needed.
                mOutputStream.setConnected();
            } else {
                if (getRequestProperty(CONTENT_LENGTH) == null) {
                    addRequestProperty(CONTENT_LENGTH, "0");
                }
            }
            // Default Content-Type to application/x-www-form-urlencoded
            if (getRequestProperty("Content-Type") == null) {
                addRequestProperty("Content-Type",
                        "application/x-www-form-urlencoded");
            }
        }
        for (Pair<String, String> requestHeader : mRequestHeaders) {
            mRequest.addHeader(requestHeader.first, requestHeader.second);
        }
        if (!getUseCaches()) {
            mRequest.disableCache();
        }
        connected = true;
        // Start the request.
        mRequest.start();
    }

    /**
     * Returns an input stream from the server in the case of an error such as
     * the requested file has not been found on the remote server.
     */
    @Override
    public InputStream getErrorStream() {
        try {
            getResponse();
        } catch (IOException e) {
            return null;
        }
        if (mResponseInfo.getHttpStatusCode() >= HTTP_BAD_REQUEST) {
            return mInputStream;
        }
        return null;
    }

    /**
     * Adds the given property to the request header.
     */
    @Override
    public final void addRequestProperty(String key, String value) {
        setRequestPropertyInternal(key, value, false);
    }

    /**
     * Sets the value of the specified request header field.
     */
    @Override
    public final void setRequestProperty(String key, String value) {
        setRequestPropertyInternal(key, value, true);
    }

    private final void setRequestPropertyInternal(String key, String value,
            boolean overwrite) {
        if (connected) {
            throw new IllegalStateException(
                    "Cannot modify request property after connection is made.");
        }
        int index = findRequestProperty(key);
        if (index >= 0) {
            if (overwrite) {
                mRequestHeaders.remove(index);
            } else {
                // Cronet does not support adding multiple headers
                // of the same key, see crbug.com/432719 for more details.
                throw new UnsupportedOperationException(
                        "Cannot add multiple headers of the same key. crbug.com/432719.");
            }
        }
        // Adds the new header at the end of mRequestHeaders.
        mRequestHeaders.add(Pair.create(key, value));
    }

    /**
     * Returns an unmodifiable map of general request properties used by this
     * connection.
     */
    @Override
    public Map<String, List<String>> getRequestProperties() {
        if (connected) {
            throw new IllegalStateException(
                    "Cannot access request headers after connection is set.");
        }
        Map<String, List<String>> map = new TreeMap<String, List<String>>(
                String.CASE_INSENSITIVE_ORDER);
        for (Pair<String, String> entry : mRequestHeaders) {
            if (map.containsKey(entry.first)) {
                // This should not happen due to setRequestPropertyInternal.
                throw new IllegalStateException(
                    "Should not have multiple values.");
            } else {
                List<String> values = new ArrayList<String>();
                values.add(entry.second);
                map.put(entry.first, Collections.unmodifiableList(values));
            }
        }
        return Collections.unmodifiableMap(map);
    }

    /**
     * Returns the value of the request header property specified by {code
     * key} or null if there is no key with this name.
     */
    @Override
    public String getRequestProperty(String key) {
        int index = findRequestProperty(key);
        if (index >= 0) {
            return mRequestHeaders.get(index).second;
        }
        return null;
    }

    /**
     * Returns whether this connection uses a proxy server.
     */
    @Override
    public boolean usingProxy() {
        // TODO(xunjieli): implement this.
        return false;
    }

    /**
     * Sets chunked streaming mode.
     */
    @Override
    public void setChunkedStreamingMode(int chunklen) {
        // TODO(xunjieli): implement this.
        throw new UnsupportedOperationException("Chunked mode not supported yet");
    }

    /**
     * Used by {@link CronetInputStream} to get more data from the network
     * stack. This should only be called after the request has started. Note
     * that this call might block if there isn't any more data to be read.
     * Since byteBuffer is passed to the UrlRequest, it must be a direct
     * ByteBuffer.
     */
    void getMoreData(ByteBuffer byteBuffer) throws IOException {
        mRequest.read(byteBuffer);
        mMessageLoop.loop();
    }

    /**
     * Returns the index of request header in {@link #mRequestHeaders} or
     * -1 if not found.
     */
    private int findRequestProperty(String key) {
        for (int i = 0; i < mRequestHeaders.size(); i++) {
            Pair<String, String> entry = mRequestHeaders.get(i);
            if (entry.first.equalsIgnoreCase(key)) {
                return i;
            }
        }
        return -1;
    }

    private class CronetUrlRequestListener implements UrlRequestListener {
        public CronetUrlRequestListener() {
        }

        @Override
        public void onResponseStarted(UrlRequest request, ResponseInfo info) {
            mResponseInfo = info;
            // Quits the message loop since we have the headers now.
            mMessageLoop.quit();
        }

        @Override
        public void onReadCompleted(UrlRequest request, ResponseInfo info,
                ByteBuffer byteBuffer) {
            mResponseInfo = info;
            mMessageLoop.quit();
        }

        @Override
        public void onReceivedRedirect(UrlRequest request, ResponseInfo info,
                String newLocationUrl) {
            mOnRedirectCalled = true;
            if (instanceFollowRedirects) {
                try {
                    url = new URL(newLocationUrl);
                } catch (MalformedURLException e) {
                    // Ignored.
                }
                mRequest.followRedirect();
            } else {
                mResponseInfo = info;
                mRequest.cancel();
                setResponseDataCompleted();
            }
        }

        @Override
        public void onSucceeded(UrlRequest request, ExtendedResponseInfo info) {
            mResponseInfo = info.getResponseInfo();
            setResponseDataCompleted();
        }

        @Override
        public void onFailed(UrlRequest request, ResponseInfo info,
                UrlRequestException exception) {
            if (exception == null) {
                throw new IllegalStateException(
                        "Exception cannot be null in onFailed.");
            }
            mResponseInfo = info;
            mException = exception;
            setResponseDataCompleted();
        }

        /**
         * Notifies {@link #mInputStream} that transferring of response data has
         * completed.
         */
        private void setResponseDataCompleted() {
            if (mInputStream != null) {
                mInputStream.setResponseDataCompleted();
            }
            mMessageLoop.quit();
        }
    }

    /**
     * Blocks until the respone headers are received.
     */
    private void getResponse() throws IOException {
        // Check to see if enough data has been received.
        if (mOutputStream != null) {
            mOutputStream.checkReceivedEnoughContent();
        }
        if (!mHasResponse) {
            startRequest();
            // Blocks until onResponseStarted or onFailed is called.
            mMessageLoop.loop();
            mHasResponse = true;
        }
        checkHasResponse();
    }

    /**
     * Checks whether response headers are received, and throws an exception if
     * an exception occurred before headers received. This method should only be
     * called after onResponseStarted or onFailed.
     */
    private void checkHasResponse() throws IOException {
        if (!mHasResponse) throw new IllegalStateException("No response.");
        if (mException != null) {
            throw mException;
        } else if (mResponseInfo == null) {
            throw new NullPointerException(
                    "Response info is null when there is no exception.");
        }
    }

    /**
     * Helper method to return the response header field at position pos.
     */
    private Pair<String, String> getHeaderFieldPair(int pos) {
        try {
            getResponse();
        } catch (IOException e) {
            return null;
        }
        List<Pair<String, String>> headers =
                mResponseInfo.getAllHeadersAsList();
        if (pos >= headers.size()) {
            return null;
        }
        return headers.get(pos);
    }
}
