// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.media.AudioManager;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.common.ContentSwitches;
import org.chromium.content_shell_apk.ContentShellTestBase;

import java.util.concurrent.Callable;

/**
 * Tests for MediaSession.
 */
@CommandLineFlags.Add(ContentSwitches.DISABLE_GESTURE_REQUIREMENT_FOR_MEDIA_PLAYBACK)
public class MediaSessionTest extends ContentShellTestBase {
    private static final String MEDIA_SESSION_TEST_URL =
            "content/test/data/media/session/media-session.html";
    private static final String VERY_SHORT_AUDIO = "very-short-audio";
    private static final String SHORT_AUDIO = "short-audio";
    private static final String LONG_AUDIO = "long-audio";
    private static final String VERY_SHORT_VIDEO = "very-short-video";
    private static final String SHORT_VIDEO = "short-video";
    private static final String LONG_VIDEO = "long-video";
    private static final String LONG_VIDEO_SILENT = "long-video-silent";
    private static final int AUDIO_FOCUS_CHANGE_TIMEOUT = 500;  // ms

    private AudioManager getAudioManager() {
        return (AudioManager) getActivity().getApplicationContext().getSystemService(
                Context.AUDIO_SERVICE);
    }

    private class MockAudioFocusChangeListener implements AudioManager.OnAudioFocusChangeListener {
        private int mAudioFocusState = AudioManager.AUDIOFOCUS_LOSS;

        @Override
        public void onAudioFocusChange(int focusChange) {
            mAudioFocusState = focusChange;
        }

        public int getAudioFocusState() {
            return mAudioFocusState;
        }

        public void requestAudioFocus(int focusType) throws Exception {
            int result = getAudioManager().requestAudioFocus(
                    this, AudioManager.STREAM_MUSIC, focusType);
            if (result != AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
                fail("Did not get audio focus");
            } else {
                mAudioFocusState = focusType;
            }
        }

        public void abandonAudioFocus() {
            getAudioManager().abandonAudioFocus(this);
            mAudioFocusState = AudioManager.AUDIOFOCUS_LOSS;
        }

        public void waitForFocusStateChange(int focusType) throws InterruptedException {
            CriteriaHelper.pollInstrumentationThread(
                    Criteria.equals(focusType, new Callable<Integer>() {
                        @Override
                        public Integer call() {
                            return getAudioFocusState();
                        }
                    }));
        }
    }

    private MockAudioFocusChangeListener mAudioFocusChangeListener;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        try {
            startActivityWithTestUrl(MEDIA_SESSION_TEST_URL);
        } catch (Throwable t) {
            fail("Couldn't load test page");
        }

        mAudioFocusChangeListener = new MockAudioFocusChangeListener();
    }

    @Override
    public void tearDown() throws Exception {
        mAudioFocusChangeListener.abandonAudioFocus();

        super.tearDown();
    }

    @SmallTest
    @Feature({"MediaSession"})
    public void testDontStopEachOther() throws Exception {
        assertTrue(DOMUtils.isMediaPaused(getWebContents(), LONG_AUDIO));
        DOMUtils.playMedia(getWebContents(), LONG_AUDIO);
        DOMUtils.waitForMediaPlay(getWebContents(), LONG_AUDIO);

        assertTrue(DOMUtils.isMediaPaused(getWebContents(), LONG_VIDEO));
        DOMUtils.playMedia(getWebContents(), LONG_VIDEO);
        DOMUtils.waitForMediaPlay(getWebContents(), LONG_VIDEO);

        assertTrue(DOMUtils.isMediaPaused(getWebContents(), SHORT_VIDEO));
        DOMUtils.playMedia(getWebContents(), SHORT_VIDEO);
        DOMUtils.waitForMediaPlay(getWebContents(), SHORT_VIDEO);

        assertTrue(DOMUtils.isMediaPaused(getWebContents(), SHORT_AUDIO));
        DOMUtils.playMedia(getWebContents(), SHORT_AUDIO);
        DOMUtils.waitForMediaPlay(getWebContents(), SHORT_AUDIO);

        assertFalse(DOMUtils.isMediaPaused(getWebContents(), SHORT_AUDIO));
        assertFalse(DOMUtils.isMediaPaused(getWebContents(), LONG_AUDIO));
        assertFalse(DOMUtils.isMediaPaused(getWebContents(), SHORT_VIDEO));
        assertFalse(DOMUtils.isMediaPaused(getWebContents(), LONG_VIDEO));
    }

    @MediumTest
    @Feature({"MediaSession"})
    public void testShortAudioIsTransient() throws Exception {
        assertEquals(AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        assertEquals(AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(getWebContents(), VERY_SHORT_AUDIO);
        DOMUtils.waitForMediaPlay(getWebContents(), VERY_SHORT_AUDIO);

        mAudioFocusChangeListener.waitForFocusStateChange(
                AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK);
        mAudioFocusChangeListener.waitForFocusStateChange(AudioManager.AUDIOFOCUS_GAIN);
    }

    @MediumTest
    @Feature({"MediaSession"})
    public void testShortVideoIsTransient() throws Exception {
        assertEquals(AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        assertEquals(AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(getWebContents(), VERY_SHORT_VIDEO);
        DOMUtils.waitForMediaPlay(getWebContents(), VERY_SHORT_VIDEO);

        mAudioFocusChangeListener.waitForFocusStateChange(
                AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK);
        mAudioFocusChangeListener.waitForFocusStateChange(AudioManager.AUDIOFOCUS_GAIN);
    }

    @SmallTest
    @Feature({"MediaSession"})
    public void testAudioGainFocus() throws Exception {
        assertEquals(AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        assertEquals(AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(getWebContents(), LONG_AUDIO);
        DOMUtils.waitForMediaPlay(getWebContents(), LONG_AUDIO);

        mAudioFocusChangeListener.waitForFocusStateChange(AudioManager.AUDIOFOCUS_LOSS);
    }

    @SmallTest
    @Feature({"MediaSession"})
    public void testVideoGainFocus() throws Exception {
        assertEquals(AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        assertEquals(AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(getWebContents(), LONG_VIDEO);
        DOMUtils.waitForMediaPlay(getWebContents(), LONG_VIDEO);

        mAudioFocusChangeListener.waitForFocusStateChange(AudioManager.AUDIOFOCUS_LOSS);
    }

    @MediumTest
    @Feature({"MediaSession"})
    public void testSilentVideoDontGainFocus() throws Exception {
        assertEquals(AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        assertEquals(AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(getWebContents(), LONG_VIDEO_SILENT);
        DOMUtils.waitForMediaPlay(getWebContents(), LONG_VIDEO_SILENT);

        // TODO(zqzhang): we need to wait for the OS to notify the audio focus loss.
        Thread.sleep(AUDIO_FOCUS_CHANGE_TIMEOUT);
        assertEquals(AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());
    }

    @SmallTest
    @Feature({"MediaSession"})
    public void testLongAudioAfterShortGainsFocus() throws Exception {
        assertEquals(AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        assertEquals(AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(getWebContents(), SHORT_AUDIO);
        DOMUtils.waitForMediaPlay(getWebContents(), SHORT_AUDIO);
        mAudioFocusChangeListener.waitForFocusStateChange(
                AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK);

        DOMUtils.playMedia(getWebContents(), LONG_AUDIO);
        DOMUtils.waitForMediaPlay(getWebContents(), LONG_AUDIO);
        mAudioFocusChangeListener.waitForFocusStateChange(AudioManager.AUDIOFOCUS_LOSS);
    }

    @SmallTest
    @Feature({"MediaSession"})
    public void testLongVideoAfterShortGainsFocus() throws Exception {
        assertEquals(AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        assertEquals(AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(getWebContents(), SHORT_VIDEO);
        DOMUtils.waitForMediaPlay(getWebContents(), SHORT_VIDEO);
        mAudioFocusChangeListener.waitForFocusStateChange(
                AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK);

        DOMUtils.playMedia(getWebContents(), LONG_VIDEO);
        DOMUtils.waitForMediaPlay(getWebContents(), LONG_VIDEO);
        mAudioFocusChangeListener.waitForFocusStateChange(AudioManager.AUDIOFOCUS_LOSS);
    }

    // TODO(zqzhang): Investigate why this test fails after switching to .ogg from .mp3
    @DisabledTest
    @SmallTest
    @Feature({"MediaSession"})
    public void testShortAudioStopsIfLostFocus() throws Exception {
        assertEquals(AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        assertEquals(AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(getWebContents(), SHORT_AUDIO);
        DOMUtils.waitForMediaPlay(getWebContents(), SHORT_AUDIO);

        // Wait for the media to be really playing.
        mAudioFocusChangeListener.waitForFocusStateChange(
                AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK);

        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        assertEquals(AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.waitForMediaPauseBeforeEnd(getWebContents(), SHORT_AUDIO);
    }

    @SmallTest
    @Feature({"MediaSession"})
    public void testShortVideoStopsIfLostFocus() throws Exception {
        assertEquals(AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        assertEquals(AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(getWebContents(), SHORT_VIDEO);
        DOMUtils.waitForMediaPlay(getWebContents(), SHORT_VIDEO);

        // Wait for the media to be really playing.
        mAudioFocusChangeListener.waitForFocusStateChange(
                AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK);

        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        assertEquals(AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.waitForMediaPauseBeforeEnd(getWebContents(), SHORT_VIDEO);
    }

    @MediumTest
    @Feature({"MediaSession"})
    public void testAudioStopsIfLostFocus() throws Exception {
        assertEquals(AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        assertEquals(AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(getWebContents(), LONG_AUDIO);
        DOMUtils.waitForMediaPlay(getWebContents(), LONG_AUDIO);

        // Wait for the media to be really playing.
        mAudioFocusChangeListener.waitForFocusStateChange(AudioManager.AUDIOFOCUS_LOSS);

        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        assertEquals(AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.waitForMediaPauseBeforeEnd(getWebContents(), LONG_AUDIO);
    }

    @SmallTest
    @Feature({"MediaSession"})
    public void testVideoStopsIfLostFocus() throws Exception {
        assertEquals(AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        assertEquals(AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(getWebContents(), LONG_VIDEO);
        DOMUtils.waitForMediaPlay(getWebContents(), LONG_VIDEO);

        // Wait for the media to be really playing.
        mAudioFocusChangeListener.waitForFocusStateChange(AudioManager.AUDIOFOCUS_LOSS);

        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        assertEquals(AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.waitForMediaPauseBeforeEnd(getWebContents(), LONG_VIDEO);
    }

    @SmallTest
    @Feature({"MediaSession"})
    @DisabledTest(message = "crbug.com/625584")
    public void testMediaDuck() throws Exception {
        assertEquals(AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        assertEquals(AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(getWebContents(), LONG_AUDIO);
        DOMUtils.waitForMediaPlay(getWebContents(), LONG_AUDIO);
        DOMUtils.playMedia(getWebContents(), LONG_VIDEO);
        DOMUtils.waitForMediaPlay(getWebContents(), LONG_VIDEO);

        // Wait for the media to be really playing.
        mAudioFocusChangeListener.waitForFocusStateChange(AudioManager.AUDIOFOCUS_LOSS);

        mAudioFocusChangeListener.requestAudioFocus(
                AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK);
        assertEquals(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK,
                mAudioFocusChangeListener.getAudioFocusState());

        // TODO(zqzhang): Currently, the volume change cannot be observed. If it could, the volume
        // should be lower now.
        Thread.sleep(AUDIO_FOCUS_CHANGE_TIMEOUT);
        assertFalse(DOMUtils.isMediaPaused(getWebContents(), LONG_AUDIO));
        assertFalse(DOMUtils.isMediaPaused(getWebContents(), LONG_VIDEO));

        mAudioFocusChangeListener.abandonAudioFocus();
        assertEquals(AudioManager.AUDIOFOCUS_LOSS,
                mAudioFocusChangeListener.getAudioFocusState());

        // TODO(zqzhang): Currently, the volume change cannot be observed. If it could, the volume
        // should be higher now.
        Thread.sleep(AUDIO_FOCUS_CHANGE_TIMEOUT);
        assertFalse(DOMUtils.isMediaPaused(getWebContents(), LONG_AUDIO));
        assertFalse(DOMUtils.isMediaPaused(getWebContents(), LONG_VIDEO));
    }

    @MediumTest
    @Feature({"MediaSession"})
    public void testMediaResumeAfterTransientFocusLoss() throws Exception {
        assertEquals(AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        assertEquals(AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(getWebContents(), LONG_AUDIO);
        DOMUtils.waitForMediaPlay(getWebContents(), LONG_AUDIO);
        DOMUtils.playMedia(getWebContents(), LONG_VIDEO);
        DOMUtils.waitForMediaPlay(getWebContents(), LONG_VIDEO);

        // Wait for the media to be really playing.
        mAudioFocusChangeListener.waitForFocusStateChange(AudioManager.AUDIOFOCUS_LOSS);

        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT);
        assertEquals(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT,
                mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.waitForMediaPauseBeforeEnd(getWebContents(), LONG_AUDIO);
        DOMUtils.waitForMediaPauseBeforeEnd(getWebContents(), LONG_VIDEO);

        mAudioFocusChangeListener.abandonAudioFocus();

        DOMUtils.waitForMediaPlay(getWebContents(), LONG_AUDIO);
        DOMUtils.waitForMediaPlay(getWebContents(), LONG_VIDEO);
    }
}
