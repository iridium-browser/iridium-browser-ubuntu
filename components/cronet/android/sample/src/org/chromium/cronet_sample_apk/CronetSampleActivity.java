// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_sample_apk;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.Environment;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;

import org.chromium.base.Log;
import org.chromium.net.ExtendedResponseInfo;
import org.chromium.net.ResponseInfo;
import org.chromium.net.UploadDataProvider;
import org.chromium.net.UploadDataSink;
import org.chromium.net.UrlRequest;
import org.chromium.net.UrlRequestContext;
import org.chromium.net.UrlRequestContextConfig;
import org.chromium.net.UrlRequestException;
import org.chromium.net.UrlRequestListener;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.Channels;
import java.nio.channels.WritableByteChannel;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

/**
 * Activity for managing the Cronet Sample.
 */
public class CronetSampleActivity extends Activity {
    private static final String TAG = "cr.CronetSample";

    private UrlRequestContext mRequestContext;

    private String mUrl;
    private boolean mLoading = false;
    private int mHttpStatusCode = 0;
    private TextView mResultText;
    private TextView mReceiveDataText;

    class SimpleUrlRequestListener implements UrlRequestListener {
        private ByteArrayOutputStream mBytesReceived = new ByteArrayOutputStream();
        private WritableByteChannel mReceiveChannel = Channels.newChannel(mBytesReceived);

        @Override
        public void onReceivedRedirect(
                UrlRequest request, ResponseInfo info, String newLocationUrl) {
            Log.i(TAG, "****** onReceivedRedirect ******");
            request.followRedirect();
        }

        @Override
        public void onResponseStarted(UrlRequest request, ResponseInfo info) {
            Log.i(TAG, "****** Response Started ******");
            Log.i(TAG, "*** Headers Are *** " + info.getAllHeaders());

            request.read(ByteBuffer.allocateDirect(32 * 1024));
        }

        @Override
        public void onReadCompleted(UrlRequest request, ResponseInfo info, ByteBuffer byteBuffer) {
            Log.i(TAG, "****** onReadCompleted ******" + byteBuffer);

            try {
                mReceiveChannel.write(byteBuffer);
            } catch (IOException e) {
                Log.i(TAG, "IOException during ByteBuffer read. Details: ", e);
            }
            byteBuffer.position(0);
            request.read(byteBuffer);
        }

        @Override
        public void onSucceeded(UrlRequest request, ExtendedResponseInfo info) {
            ResponseInfo responseInfo = info.getResponseInfo();
            mHttpStatusCode = responseInfo.getHttpStatusCode();
            Log.i(TAG, "****** Request Completed, status code is " + mHttpStatusCode
                            + ", total received bytes is " + info.getTotalReceivedBytes());

            final String receivedData = mBytesReceived.toString();
            final String url = responseInfo.getUrl();
            final String text = "Completed " + url + " (" + mHttpStatusCode + ")";
            CronetSampleActivity.this.runOnUiThread(new Runnable() {
                public void run() {
                    mLoading = false;
                    mResultText.setText(text);
                    mReceiveDataText.setText(receivedData);
                    promptForURL(url);
                }
            });
        }

        @Override
        public void onFailed(UrlRequest request, ResponseInfo info, UrlRequestException error) {
            Log.i(TAG, "****** onFailed, error is: " + error.getMessage());

            final String url = mUrl;
            final String text = "Failed " + mUrl + " (" + error.getMessage() + ")";
            CronetSampleActivity.this.runOnUiThread(new Runnable() {
                public void run() {
                    mLoading = false;
                    mResultText.setText(text);
                    promptForURL(url);
                }
            });
        }
    }

    static class SimpleUploadDataProvider implements UploadDataProvider {
        private byte[] mUploadData;
        private int mOffset;

        SimpleUploadDataProvider(byte[] uploadData) {
            mUploadData = uploadData;
            mOffset = 0;
        }

        @Override
        public long getLength() {
            return mUploadData.length;
        }

        @Override
        public void read(final UploadDataSink uploadDataSink, final ByteBuffer byteBuffer)
                throws IOException {
            if (byteBuffer.remaining() >= mUploadData.length - mOffset) {
                byteBuffer.put(mUploadData, mOffset, mUploadData.length - mOffset);
                mOffset = mUploadData.length;
            } else {
                int length = byteBuffer.remaining();
                byteBuffer.put(mUploadData, mOffset, length);
                mOffset += length;
            }
            uploadDataSink.onReadSucceeded(false);
        }

        @Override
        public void rewind(final UploadDataSink uploadDataSink) throws IOException {
            mOffset = 0;
            uploadDataSink.onRewindSucceeded();
        }
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        mResultText = (TextView) findViewById(R.id.resultView);
        mReceiveDataText = (TextView) findViewById(R.id.dataView);

        UrlRequestContextConfig myConfig = new UrlRequestContextConfig();
        myConfig.enableHttpCache(UrlRequestContextConfig.HTTP_CACHE_IN_MEMORY, 100 * 1024)
                .enableHTTP2(true)
                .enableQUIC(true);

        mRequestContext = UrlRequestContext.createContext(this, myConfig);

        String appUrl = (getIntent() != null ? getIntent().getDataString() : null);
        if (appUrl == null) {
            promptForURL("https://");
        } else {
            startWithURL(appUrl);
        }
    }

    private void promptForURL(String url) {
        Log.i(TAG, "No URL provided via intent, prompting user...");
        AlertDialog.Builder alert = new AlertDialog.Builder(this);
        alert.setTitle("Enter a URL");
        LayoutInflater inflater = getLayoutInflater();
        View alertView = inflater.inflate(R.layout.dialog_url, null);
        final EditText urlInput = (EditText) alertView.findViewById(R.id.urlText);
        urlInput.setText(url);
        final EditText postInput = (EditText) alertView.findViewById(R.id.postText);
        alert.setView(alertView);

        alert.setPositiveButton("Load", new DialogInterface.OnClickListener() {
            public void onClick(DialogInterface dialog, int button) {
                String url = urlInput.getText().toString();
                String postData = postInput.getText().toString();
                startWithURL(url, postData);
            }
        });
        alert.show();
    }

    private void applyPostDataToUrlRequest(UrlRequest request, Executor executor, String postData) {
        if (postData != null && postData.length() > 0) {
            UploadDataProvider uploadDataProvider =
                    new SimpleUploadDataProvider(postData.getBytes());
            request.setHttpMethod("POST");
            request.addHeader("Content-Type", "application/x-www-form-urlencoded");
            request.setUploadDataProvider(uploadDataProvider, executor);
        }
    }

    private void startWithURL(String url) {
        startWithURL(url, null);
    }

    private void startWithURL(String url, String postData) {
        Log.i(TAG, "Cronet started: " + url);
        mUrl = url;
        mLoading = false;

        Executor executor = Executors.newSingleThreadExecutor();
        UrlRequestListener listener = new SimpleUrlRequestListener();
        UrlRequest request = mRequestContext.createRequest(url, listener, executor);
        applyPostDataToUrlRequest(request, executor, postData);
        request.start();
    }

    /**
     * This method is used in testing.
     */
    public String getUrl() {
        return mUrl;
    }

    /**
     * This method is used in testing.
     */
    public boolean isLoading() {
        return mLoading;
    }

    /**
     * This method is used in testing.
     */
    public int getHttpStatusCode() {
        return mHttpStatusCode;
    }

    private void startNetLog() {
        mRequestContext.startNetLogToFile(
                Environment.getExternalStorageDirectory().getPath() + "/cronet_sample_netlog.json",
                false);
    }

    private void stopNetLog() {
        mRequestContext.stopNetLog();
    }
}
