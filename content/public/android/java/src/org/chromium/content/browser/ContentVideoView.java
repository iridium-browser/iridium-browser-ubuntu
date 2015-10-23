// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Point;
import android.provider.Settings;
import android.view.Display;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * This class implements accelerated fullscreen video playback using surface view.
 */
@JNINamespace("content")
public class ContentVideoView extends FrameLayout
        implements SurfaceHolder.Callback {

    private static final String TAG = "cr.ContentVideoView";

    /* Do not change these values without updating their counterparts
     * in include/media/mediaplayer.h!
     */
    private static final int MEDIA_NOP = 0; // interface test message
    private static final int MEDIA_PREPARED = 1;
    private static final int MEDIA_PLAYBACK_COMPLETE = 2;
    private static final int MEDIA_BUFFERING_UPDATE = 3;
    private static final int MEDIA_SEEK_COMPLETE = 4;
    private static final int MEDIA_SET_VIDEO_SIZE = 5;
    private static final int MEDIA_ERROR = 100;
    private static final int MEDIA_INFO = 200;

    /**
     * Keep these error codes in sync with the code we defined in
     * MediaPlayerListener.java.
     */
    public static final int MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK = 2;
    public static final int MEDIA_ERROR_INVALID_CODE = 3;

    // all possible internal states
    private static final int STATE_ERROR              = -1;
    private static final int STATE_IDLE               = 0;
    private static final int STATE_PLAYING            = 1;
    private static final int STATE_PAUSED             = 2;
    private static final int STATE_PLAYBACK_COMPLETED = 3;

    private SurfaceHolder mSurfaceHolder;
    private int mVideoWidth;
    private int mVideoHeight;

    // Native pointer to C++ ContentVideoView object.
    private long mNativeContentVideoView;

    // webkit should have prepared the media
    private int mCurrentState = STATE_IDLE;

    // Strings for displaying media player errors
    private String mPlaybackErrorText;
    private String mUnknownErrorText;
    private String mErrorButton;
    private String mErrorTitle;
    private String mVideoLoadingText;

    // This view will contain the video.
    private VideoSurfaceView mVideoSurfaceView;

    // Progress view when the video is loading.
    private View mProgressView;

    private final ContentVideoViewClient mClient;

    private boolean mInitialOrientation;
    private boolean mPossibleAccidentalChange;
    private boolean mUmaRecorded;
    private long mOrientationChangedTime;
    private long mPlaybackStartTime;

    private class VideoSurfaceView extends SurfaceView {

        public VideoSurfaceView(Context context) {
            super(context);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            // set the default surface view size to (1, 1) so that it won't block
            // the infobar. (0, 0) is not a valid size for surface view.
            int width = 1;
            int height = 1;
            if (mVideoWidth > 0 && mVideoHeight > 0) {
                width = getDefaultSize(mVideoWidth, widthMeasureSpec);
                height = getDefaultSize(mVideoHeight, heightMeasureSpec);
                if (mVideoWidth * height  > width * mVideoHeight) {
                    height = width * mVideoHeight / mVideoWidth;
                } else if (mVideoWidth * height  < width * mVideoHeight) {
                    width = height * mVideoWidth / mVideoHeight;
                }
            }
            if (mUmaRecorded) {
                // If we have never switched orientation, record the orientation
                // time.
                if (mPlaybackStartTime == mOrientationChangedTime) {
                    if (isOrientationPortrait() != mInitialOrientation) {
                        mOrientationChangedTime = System.currentTimeMillis();
                    }
                } else {
                    // if user quickly switched the orientation back and force, don't
                    // count it in UMA.
                    if (!mPossibleAccidentalChange
                            && isOrientationPortrait() == mInitialOrientation
                            && System.currentTimeMillis() - mOrientationChangedTime < 5000) {
                        mPossibleAccidentalChange = true;
                    }
                }
            }
            setMeasuredDimension(width, height);
        }
    }

    private static class ProgressView extends LinearLayout {

        private final ProgressBar mProgressBar;
        private final TextView mTextView;

        public ProgressView(Context context, String videoLoadingText) {
            super(context);
            setOrientation(LinearLayout.VERTICAL);
            setLayoutParams(new LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.WRAP_CONTENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT));
            mProgressBar = new ProgressBar(context, null, android.R.attr.progressBarStyleLarge);
            mTextView = new TextView(context);
            mTextView.setText(videoLoadingText);
            addView(mProgressBar);
            addView(mTextView);
        }
    }

    private final Runnable mExitFullscreenRunnable = new Runnable() {
        @Override
        public void run() {
            exitFullscreen(true);
        }
    };

    private ContentVideoView(Context context, long nativeContentVideoView,
            ContentVideoViewClient client) {
        super(context);
        mNativeContentVideoView = nativeContentVideoView;
        mClient = client;
        mUmaRecorded = false;
        mPossibleAccidentalChange = false;
        initResources(context);
        mVideoSurfaceView = new VideoSurfaceView(context);
        showContentVideoView();
        setVisibility(View.VISIBLE);
    }

    private ContentVideoViewClient getContentVideoViewClient() {
        return mClient;
    }

    private void initResources(Context context) {
        if (mPlaybackErrorText != null) return;
        mPlaybackErrorText = context.getString(
                org.chromium.content.R.string.media_player_error_text_invalid_progressive_playback);
        mUnknownErrorText = context.getString(
                org.chromium.content.R.string.media_player_error_text_unknown);
        mErrorButton = context.getString(
                org.chromium.content.R.string.media_player_error_button);
        mErrorTitle = context.getString(
                org.chromium.content.R.string.media_player_error_title);
        mVideoLoadingText = context.getString(
                org.chromium.content.R.string.media_player_loading_video);
    }

    private void showContentVideoView() {
        mVideoSurfaceView.getHolder().addCallback(this);
        this.addView(mVideoSurfaceView, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
                Gravity.CENTER));

        mProgressView = mClient.getVideoLoadingProgressView();
        if (mProgressView == null) {
            mProgressView = new ProgressView(getContext(), mVideoLoadingText);
        }
        this.addView(mProgressView, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
                Gravity.CENTER));
    }

    private SurfaceView getSurfaceView() {
        return mVideoSurfaceView;
    }

    @CalledByNative
    public void onMediaPlayerError(int errorType) {
        Log.d(TAG, "OnMediaPlayerError: %d", errorType);
        if (mCurrentState == STATE_ERROR || mCurrentState == STATE_PLAYBACK_COMPLETED) {
            return;
        }

        // Ignore some invalid error codes.
        if (errorType == MEDIA_ERROR_INVALID_CODE) {
            return;
        }

        mCurrentState = STATE_ERROR;

        if (ContentViewCore.activityFromContext(getContext()) == null) {
            Log.w(TAG, "Unable to show alert dialog because it requires an activity context");
            return;
        }

        /* Pop up an error dialog so the user knows that
         * something bad has happened. Only try and pop up the dialog
         * if we're attached to a window. When we're going away and no
         * longer have a window, don't bother showing the user an error.
         *
         * TODO(qinmin): We need to review whether this Dialog is OK with
         * the rest of the browser UI elements.
         */
        if (getWindowToken() != null) {
            String message;

            if (errorType == MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK) {
                message = mPlaybackErrorText;
            } else {
                message = mUnknownErrorText;
            }

            try {
                new AlertDialog.Builder(getContext())
                    .setTitle(mErrorTitle)
                    .setMessage(message)
                    .setPositiveButton(mErrorButton,
                            new DialogInterface.OnClickListener() {
                                @Override
                                public void onClick(DialogInterface dialog, int whichButton) {
                                    // Inform that the video is over.
                                    onCompletion();
                                }
                            })
                    .setCancelable(false)
                    .show();
            } catch (RuntimeException e) {
                Log.e(TAG, "Cannot show the alert dialog, error message: %s", message, e);
            }
        }
    }

    @CalledByNative
    private void onVideoSizeChanged(int width, int height) {
        mVideoWidth = width;
        mVideoHeight = height;
        // This will trigger the SurfaceView.onMeasure() call.
        mVideoSurfaceView.getHolder().setFixedSize(mVideoWidth, mVideoHeight);
    }

    @CalledByNative
    private void onBufferingUpdate(int percent) {
    }

    @CalledByNative
    private void onPlaybackComplete() {
        onCompletion();
    }

    @CalledByNative
    private void onUpdateMediaMetadata(
            int videoWidth,
            int videoHeight,
            int duration,
            boolean canPause,
            boolean canSeekBack,
            boolean canSeekForward) {
        mProgressView.setVisibility(View.GONE);
        mCurrentState = isPlaying() ? STATE_PLAYING : STATE_PAUSED;
        onVideoSizeChanged(videoWidth, videoHeight);
        if (mUmaRecorded) return;
        try {
            if (Settings.System.getInt(getContext().getContentResolver(),
                    Settings.System.ACCELEROMETER_ROTATION) == 0) {
                return;
            }
        } catch (Settings.SettingNotFoundException e) {
            return;
        }
        mInitialOrientation = isOrientationPortrait();
        mUmaRecorded = true;
        mPlaybackStartTime = System.currentTimeMillis();
        mOrientationChangedTime = mPlaybackStartTime;
        nativeRecordFullscreenPlayback(
                mNativeContentVideoView, videoHeight > videoWidth, mInitialOrientation);
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        mSurfaceHolder = holder;
        openVideo();
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        if (mNativeContentVideoView != 0) {
            nativeSetSurface(mNativeContentVideoView, null);
        }
        mSurfaceHolder = null;
        post(mExitFullscreenRunnable);
    }

    @CalledByNative
    private void openVideo() {
        if (mSurfaceHolder != null) {
            mCurrentState = STATE_IDLE;
            if (mNativeContentVideoView != 0) {
                nativeRequestMediaMetadata(mNativeContentVideoView);
                nativeSetSurface(mNativeContentVideoView,
                        mSurfaceHolder.getSurface());
            }
        }
    }

    private void onCompletion() {
        mCurrentState = STATE_PLAYBACK_COMPLETED;
    }

    public boolean isPlaying() {
        return mNativeContentVideoView != 0 && nativeIsPlaying(mNativeContentVideoView);
    }

    @CalledByNative
    private static ContentVideoView createContentVideoView(
            ContentViewCore contentViewCore, long nativeContentVideoView) {
        ThreadUtils.assertOnUiThread();
        Context context = contentViewCore.getContext();
        ContentVideoViewClient client = contentViewCore.getContentVideoViewClient();
        ContentVideoView videoView = new ContentVideoView(context, nativeContentVideoView, client);
        client.enterFullscreenVideo(videoView);
        return videoView;
    }

    public void removeSurfaceView() {
        removeView(mVideoSurfaceView);
        removeView(mProgressView);
        mVideoSurfaceView = null;
        mProgressView = null;
    }

    public void exitFullscreen(boolean relaseMediaPlayer) {
        if (mNativeContentVideoView != 0) {
            destroyContentVideoView(false);
            if (mUmaRecorded && !mPossibleAccidentalChange) {
                long currentTime = System.currentTimeMillis();
                long timeBeforeOrientationChange = mOrientationChangedTime - mPlaybackStartTime;
                long timeAfterOrientationChange = currentTime - mOrientationChangedTime;
                if (timeBeforeOrientationChange == 0) {
                    timeBeforeOrientationChange = timeAfterOrientationChange;
                    timeAfterOrientationChange = 0;
                }
                nativeRecordExitFullscreenPlayback(mNativeContentVideoView, mInitialOrientation,
                        timeBeforeOrientationChange, timeAfterOrientationChange);
            }
            nativeExitFullscreen(mNativeContentVideoView, relaseMediaPlayer);
            mNativeContentVideoView = 0;
        }
    }

    /**
     * Called when the fullscreen window gets focused.
     */
    public void onFullscreenWindowFocused() {
        mClient.setSystemUiVisibility(true);
    }

    @CalledByNative
    private void onExitFullscreen() {
        exitFullscreen(false);
    }

    /**
     * This method shall only be called by native and exitFullscreen,
     * To exit fullscreen, use exitFullscreen in Java.
     */
    @CalledByNative
    private void destroyContentVideoView(boolean nativeViewDestroyed) {
        if (mVideoSurfaceView != null) {
            removeSurfaceView();
            setVisibility(View.GONE);

            // To prevent re-entrance, call this after removeSurfaceView.
            mClient.exitFullscreenVideo();
        }
        if (nativeViewDestroyed) {
            mNativeContentVideoView = 0;
        }
    }

    public static ContentVideoView getContentVideoView() {
        return nativeGetSingletonJavaContentVideoView();
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            exitFullscreen(false);
            return true;
        }
        return super.onKeyUp(keyCode, event);
    }

    private boolean isOrientationPortrait() {
        Context context = getContext();
        WindowManager manager = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        Display display = manager.getDefaultDisplay();
        Point outputSize = new Point(0, 0);
        display.getSize(outputSize);
        return outputSize.x <= outputSize.y;
    }

    private static native ContentVideoView nativeGetSingletonJavaContentVideoView();
    private native void nativeExitFullscreen(long nativeContentVideoView,
            boolean relaseMediaPlayer);
    private native void nativeRequestMediaMetadata(long nativeContentVideoView);
    private native boolean nativeIsPlaying(long nativeContentVideoView);
    private native void nativeSetSurface(long nativeContentVideoView, Surface surface);
    private native void nativeRecordFullscreenPlayback(
            long nativeContentVideoView, boolean isVideoPortrait, boolean isOrientationPortrait);
    private native void nativeRecordExitFullscreenPlayback(
            long nativeContentVideoView, boolean isOrientationPortrait,
            long playbackDurationBeforeOrientationChange,
            long playbackDurationAfterOrientationChange);
}
