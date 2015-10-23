// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.os.ConditionVariable;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertNull;
import static junit.framework.Assert.assertTrue;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadFactory;

/**
 * Listener that tracks information from different callbacks and and has a
 * method to block thread until the request completes on another thread.
 * Allows to cancel, block request or throw an exception from an arbitrary step.
 */
class TestUrlRequestListener implements UrlRequestListener {
    public ArrayList<ResponseInfo> mRedirectResponseInfoList =
            new ArrayList<ResponseInfo>();
    public ArrayList<String> mRedirectUrlList = new ArrayList<String>();
    public ResponseInfo mResponseInfo;
    public ExtendedResponseInfo mExtendedResponseInfo;
    public UrlRequestException mError;

    public ResponseStep mResponseStep = ResponseStep.NOTHING;

    public int mRedirectCount = 0;
    public boolean mOnErrorCalled = false;

    public int mHttpResponseDataLength = 0;
    public byte[] mLastDataReceivedAsBytes;
    public String mResponseAsString = "";

    private static final int READ_BUFFER_SIZE = 32 * 1024;

    // When false, the consumer is responsible for all calls into the request
    // that advance it.
    private boolean mAutoAdvance = true;

    // Conditionally fail on certain steps.
    private FailureType mFailureType = FailureType.NONE;
    private ResponseStep mFailureStep = ResponseStep.NOTHING;

    // Signals when request is done either successfully or not.
    private final ConditionVariable mDone = new ConditionVariable();

    // Signaled on each step when mAutoAdvance is false.
    private final ConditionVariable mStepBlock = new ConditionVariable();

    // Executor Service for Cronet callbacks.
    private final ExecutorService mExecutorService = Executors.newSingleThreadExecutor(
            new ExecutorThreadFactory());
    private Thread mExecutorThread;

    private class ExecutorThreadFactory implements ThreadFactory {
        public Thread newThread(Runnable r) {
            mExecutorThread = new Thread(r);
            return mExecutorThread;
        }
    }

    public enum ResponseStep {
        NOTHING,
        ON_RECEIVED_REDIRECT,
        ON_RESPONSE_STARTED,
        ON_READ_COMPLETED,
        ON_SUCCEEDED
    };

    public enum FailureType {
        NONE,
        CANCEL_SYNC,
        CANCEL_ASYNC,
        // Same as above, but continues to advance the request after posting
        // the cancellation task.
        CANCEL_ASYNC_WITHOUT_PAUSE,
        THROW_SYNC
    };

    public void setAutoAdvance(boolean autoAdvance) {
        mAutoAdvance = autoAdvance;
    }

    public void setFailure(FailureType failureType, ResponseStep failureStep) {
        mFailureStep = failureStep;
        mFailureType = failureType;
    }

    public void blockForDone() {
        mDone.block();
    }

    public void waitForNextStep() {
        mStepBlock.block();
        mStepBlock.close();
    }

    public Executor getExecutor() {
        return mExecutorService;
    }

    public void shutdownExecutor() {
        mExecutorService.shutdown();
    }

    @Override
    public void onReceivedRedirect(UrlRequest request,
            ResponseInfo info,
            String newLocationUrl) {
        assertEquals(mExecutorThread, Thread.currentThread());
        assertFalse(request.isDone());
        assertTrue(mResponseStep == ResponseStep.NOTHING
                   || mResponseStep == ResponseStep.ON_RECEIVED_REDIRECT);
        assertNull(mError);

        mResponseStep = ResponseStep.ON_RECEIVED_REDIRECT;
        mRedirectUrlList.add(newLocationUrl);
        mRedirectResponseInfoList.add(info);
        ++mRedirectCount;
        if (maybeThrowCancelOrPause(request)) {
            return;
        }
        request.followRedirect();
    }

    @Override
    public void onResponseStarted(UrlRequest request, ResponseInfo info) {
        assertEquals(mExecutorThread, Thread.currentThread());
        assertFalse(request.isDone());
        assertTrue(mResponseStep == ResponseStep.NOTHING
                   || mResponseStep == ResponseStep.ON_RECEIVED_REDIRECT);
        assertNull(mError);

        mResponseStep = ResponseStep.ON_RESPONSE_STARTED;
        mResponseInfo = info;
        if (maybeThrowCancelOrPause(request)) {
            return;
        }
        startNextRead(request);
    }

    @Override
    public void onReadCompleted(UrlRequest request,
            ResponseInfo info,
            ByteBuffer byteBuffer) {
        assertEquals(mExecutorThread, Thread.currentThread());
        assertFalse(request.isDone());
        assertTrue(byteBuffer.remaining() > 0);
        assertTrue(mResponseStep == ResponseStep.ON_RESPONSE_STARTED
                   || mResponseStep == ResponseStep.ON_READ_COMPLETED);
        assertNull(mError);

        mResponseStep = ResponseStep.ON_READ_COMPLETED;

        // Make a slice of ByteBuffer, so can read from it without affecting
        // position, which allows tests to check the state of the buffer.
        ByteBuffer slice = byteBuffer.slice();
        mHttpResponseDataLength += slice.remaining();
        mLastDataReceivedAsBytes = new byte[slice.remaining()];
        slice.get(mLastDataReceivedAsBytes);
        mResponseAsString += new String(mLastDataReceivedAsBytes);

        if (maybeThrowCancelOrPause(request)) {
            return;
        }
        startNextRead(request);
    }

    @Override
    public void onSucceeded(UrlRequest request, ExtendedResponseInfo info) {
        assertEquals(mExecutorThread, Thread.currentThread());
        assertTrue(request.isDone());
        assertTrue(mResponseStep == ResponseStep.ON_RESPONSE_STARTED
                   || mResponseStep == ResponseStep.ON_READ_COMPLETED);
        assertNull(mError);

        mResponseStep = ResponseStep.ON_SUCCEEDED;
        mExtendedResponseInfo = info;
        openDone();
        maybeThrowCancelOrPause(request);
    }

    @Override
    public void onFailed(UrlRequest request,
            ResponseInfo info,
            UrlRequestException error) {
        assertEquals(mExecutorThread, Thread.currentThread());
        assertTrue(request.isDone());
        // Shouldn't happen after success.
        assertTrue(mResponseStep != ResponseStep.ON_SUCCEEDED);
        // Should happen at most once for a single request.
        assertFalse(mOnErrorCalled);
        assertNull(mError);

        mOnErrorCalled = true;
        mError = error;
        openDone();
        maybeThrowCancelOrPause(request);
    }

    public void startNextRead(UrlRequest request) {
        request.read(ByteBuffer.allocateDirect(READ_BUFFER_SIZE));
    }

    public boolean isDone() {
        // It's not mentioned by the Android docs, but block(0) seems to block
        // indefinitely, so have to block for one millisecond to get state
        // without blocking.
        return mDone.block(1);
    }

    protected void openDone() {
        mDone.open();
    }

    /**
     * Returns {@code false} if the listener should continue to advance the
     * request.
     */
    private boolean maybeThrowCancelOrPause(final UrlRequest request) {
        if (mResponseStep != mFailureStep || mFailureType == FailureType.NONE) {
            if (!mAutoAdvance) {
                mStepBlock.open();
                return true;
            }
            return false;
        }

        if (mFailureType == FailureType.THROW_SYNC) {
            throw new IllegalStateException("Listener Exception.");
        }
        Runnable task = new Runnable() {
            public void run() {
                request.cancel();
                openDone();
            }
        };
        if (mFailureType == FailureType.CANCEL_ASYNC
                || mFailureType == FailureType.CANCEL_ASYNC_WITHOUT_PAUSE) {
            getExecutor().execute(task);
        } else {
            task.run();
        }
        return mFailureType != FailureType.CANCEL_ASYNC_WITHOUT_PAUSE;
    }
}

