// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import android.app.Notification;
import android.app.PendingIntent;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.media.AudioManager;
import android.os.Build;
import android.os.IBinder;
import android.support.v4.app.NotificationManagerCompat;
import android.support.v4.media.MediaMetadataCompat;
import android.support.v4.media.session.MediaSessionCompat;
import android.support.v4.media.session.PlaybackStateCompat;
import android.support.v7.app.NotificationCompat;
import android.support.v7.media.MediaRouter;
import android.text.TextUtils;
import android.util.SparseArray;
import android.view.KeyEvent;

import org.chromium.base.SysUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.blink.mojom.MediaSessionAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.notifications.ChromeNotificationBuilder;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.content_public.common.MediaMetadata;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import javax.annotation.Nullable;

/**
 * A class for notifications that provide information and optional media controls for a given media.
 * Internally implements a Service for transforming notification Intents into
 * {@link MediaNotificationListener} calls for all registered listeners.
 * There's one service started for a distinct notification id.
 */
public class MediaNotificationManager {
    private static final String TAG = "MediaNotification";

    @VisibleForTesting
    static final int CUSTOM_MEDIA_SESSION_ACTION_STOP = MediaSessionAction.LAST + 1;

    // The media artwork image resolution on high-end devices.
    private static final int HIGH_IMAGE_SIZE_PX = 512;

    // The media artwork image resolution on high-end devices.
    private static final int LOW_IMAGE_SIZE_PX = 256;

    // The maximum number of actions in CompactView media notification.
    private static final int COMPACT_VIEW_ACTIONS_COUNT = 3;

    // The maximum number of actions in BigView media notification.
    private static final int BIG_VIEW_ACTIONS_COUNT = isRunningN() ? 5 : 3;

    // We're always used on the UI thread but the LOCK is required by lint when creating the
    // singleton.
    private static final Object LOCK = new Object();

    // Maps the notification ids to their corresponding notification managers.
    private static SparseArray<MediaNotificationManager> sManagers;

    private final Context mContext;

    // ListenerService running for the notification. Only non-null when showing.
    private ListenerService mService;

    private SparseArray<MediaButtonInfo> mActionToButtonInfo;

    private ChromeNotificationBuilder mNotificationBuilder;

    private Bitmap mDefaultNotificationLargeIcon;

    // |mMediaNotificationInfo| should be not null if and only if the notification is showing.
    private MediaNotificationInfo mMediaNotificationInfo;

    private MediaSessionCompat mMediaSession;

    private final MediaSessionCompat.Callback mMediaSessionCallback =
            new MediaSessionCompat.Callback() {
                @Override
                public void onPlay() {
                    MediaNotificationManager.this.onPlay(
                            MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION);
                }

                @Override
                public void onPause() {
                    MediaNotificationManager.this.onPause(
                            MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION);
                }

                @Override
                public void onSkipToPrevious() {
                    MediaNotificationManager.this.onMediaSessionAction(
                            MediaSessionAction.PREVIOUS_TRACK);
                }

                @Override
                public void onSkipToNext() {
                    MediaNotificationManager.this.onMediaSessionAction(
                            MediaSessionAction.NEXT_TRACK);
                }

                @Override
                public void onFastForward() {
                    MediaNotificationManager.this.onMediaSessionAction(
                            MediaSessionAction.SEEK_FORWARD);
                }

                @Override
                public void onRewind() {
                    MediaNotificationManager.this.onMediaSessionAction(
                            MediaSessionAction.SEEK_BACKWARD);
                }
            };

    /**
     * Service used to transform intent requests triggered from the notification into
     * {@code MediaNotificationListener} callbacks. We have to create a separate derived class for
     * each type of notification since one class corresponds to one instance of the service only.
     */
    private abstract static class ListenerService extends Service {
        private static final String ACTION_PLAY =
                "MediaNotificationManager.ListenerService.PLAY";
        private static final String ACTION_PAUSE =
                "MediaNotificationManager.ListenerService.PAUSE";
        private static final String ACTION_STOP =
                "MediaNotificationManager.ListenerService.STOP";
        private static final String ACTION_SWIPE =
                "MediaNotificationManager.ListenerService.SWIPE";
        private static final String ACTION_CANCEL =
                "MediaNotificationManager.ListenerService.CANCEL";
        private static final String ACTION_PREVIOUS_TRACK =
                "MediaNotificationManager.ListenerService.PREVIOUS_TRACK";
        private static final String ACTION_NEXT_TRACK =
                "MediaNotificationManager.ListenerService.NEXT_TRACK";
        private static final String ACTION_SEEK_FORWARD =
                "MediaNotificationManager.ListenerService.SEEK_FORWARD";
        private static final String ACTION_SEEK_BACKWARD =
                "MediaNotificationmanager.ListenerService.SEEK_BACKWARD";

        @Override
        public IBinder onBind(Intent intent) {
            return null;
        }

        @Override
        public void onDestroy() {
            super.onDestroy();

            MediaNotificationManager manager = getManager();
            if (manager == null) return;

            manager.onServiceDestroyed();
        }

        @Override
        public int onStartCommand(Intent intent, int flags, int startId) {
            if (!processIntent(intent)) stopSelf();

            return START_NOT_STICKY;
        }

        @Nullable
        protected abstract MediaNotificationManager getManager();

        private boolean processIntent(Intent intent) {
            if (intent == null) return false;

            MediaNotificationManager manager = getManager();
            if (manager == null || manager.mMediaNotificationInfo == null) return false;

            if (intent.getAction() == null) {
                // The intent comes from {@link startService()} or
                // {@link startForegroundService}.
                manager.onServiceStarted(this);
            } else {
                // The intent comes from the notification. In this case, {@link onServiceStarted()}
                // does need to be called.
                processAction(intent, manager);
            }
            return true;
        }

        private void processAction(Intent intent, MediaNotificationManager manager) {
            String action = intent.getAction();

            // Before Android L, instead of using the MediaSession callback, the system will fire
            // ACTION_MEDIA_BUTTON intents which stores the information about the key event.
            if (Intent.ACTION_MEDIA_BUTTON.equals(action)) {
                assert Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP;

                KeyEvent event = (KeyEvent) intent.getParcelableExtra(Intent.EXTRA_KEY_EVENT);
                if (event == null) return;
                if (event.getAction() != KeyEvent.ACTION_DOWN) return;

                switch (event.getKeyCode()) {
                    case KeyEvent.KEYCODE_MEDIA_PLAY:
                        manager.onPlay(
                                MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION);
                        break;
                    case KeyEvent.KEYCODE_MEDIA_PAUSE:
                        manager.onPause(
                                MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION);
                        break;
                    case KeyEvent.KEYCODE_HEADSETHOOK:
                    case KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:
                        if (manager.mMediaNotificationInfo.isPaused) {
                            manager.onPlay(MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION);
                        } else {
                            manager.onPause(
                                    MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION);
                        }
                        break;
                    case KeyEvent.KEYCODE_MEDIA_PREVIOUS:
                        manager.onMediaSessionAction(MediaSessionAction.PREVIOUS_TRACK);
                        break;
                    case KeyEvent.KEYCODE_MEDIA_NEXT:
                        manager.onMediaSessionAction(MediaSessionAction.NEXT_TRACK);
                        break;
                    case KeyEvent.KEYCODE_MEDIA_FAST_FORWARD:
                        manager.onMediaSessionAction(MediaSessionAction.SEEK_FORWARD);
                        break;
                    case KeyEvent.KEYCODE_MEDIA_REWIND:
                        manager.onMediaSessionAction(MediaSessionAction.SEEK_BACKWARD);
                        break;
                    default:
                        break;
                }
            } else if (ACTION_STOP.equals(action)
                    || ACTION_SWIPE.equals(action)
                    || ACTION_CANCEL.equals(action)) {
                manager.onStop(
                        MediaNotificationListener.ACTION_SOURCE_MEDIA_NOTIFICATION);
                stopSelf();
            } else if (ACTION_PLAY.equals(action)) {
                manager.onPlay(MediaNotificationListener.ACTION_SOURCE_MEDIA_NOTIFICATION);
            } else if (ACTION_PAUSE.equals(action)) {
                manager.onPause(MediaNotificationListener.ACTION_SOURCE_MEDIA_NOTIFICATION);
            } else if (AudioManager.ACTION_AUDIO_BECOMING_NOISY.equals(action)) {
                manager.onPause(MediaNotificationListener.ACTION_SOURCE_HEADSET_UNPLUG);
            } else if (ACTION_PREVIOUS_TRACK.equals(action)) {
                manager.onMediaSessionAction(MediaSessionAction.PREVIOUS_TRACK);
            } else if (ACTION_NEXT_TRACK.equals(action)) {
                manager.onMediaSessionAction(MediaSessionAction.NEXT_TRACK);
            } else if (ACTION_SEEK_FORWARD.equals(action)) {
                manager.onMediaSessionAction(MediaSessionAction.SEEK_FORWARD);
            } else if (ACTION_SEEK_BACKWARD.equals(action)) {
                manager.onMediaSessionAction(MediaSessionAction.SEEK_BACKWARD);
            }
        }
    }

    /**
     * This class is used internally but have to be public to be able to launch the service.
     */
    public static final class PlaybackListenerService extends ListenerService {
        private static final int NOTIFICATION_ID = R.id.media_playback_notification;

        @Override
        public void onCreate() {
            super.onCreate();
            IntentFilter filter = new IntentFilter(AudioManager.ACTION_AUDIO_BECOMING_NOISY);
            registerReceiver(mAudioBecomingNoisyReceiver, filter);
        }

        @Override
        public void onDestroy() {
            unregisterReceiver(mAudioBecomingNoisyReceiver);
            super.onDestroy();
        }

        @Override
        @Nullable
        protected MediaNotificationManager getManager() {
            return MediaNotificationManager.getManager(NOTIFICATION_ID);
        }

        private BroadcastReceiver mAudioBecomingNoisyReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    if (!AudioManager.ACTION_AUDIO_BECOMING_NOISY.equals(intent.getAction())) {
                        return;
                    }

                    Intent i = new Intent(context, PlaybackListenerService.class);
                    i.setAction(intent.getAction());
                    context.startService(i);
                }
            };
    }

    /**
     * This class is used internally but have to be public to be able to launch the service.
     */
    public static final class PresentationListenerService extends ListenerService {
        private static final int NOTIFICATION_ID = R.id.presentation_notification;

        @Override
        @Nullable
        protected MediaNotificationManager getManager() {
            return MediaNotificationManager.getManager(NOTIFICATION_ID);
        }
    }

    /**
     * This class is used internally but have to be public to be able to launch the service.
     */
    public static final class CastListenerService extends ListenerService {
        private static final int NOTIFICATION_ID = R.id.remote_notification;

        @Override
        @Nullable
        protected MediaNotificationManager getManager() {
            return MediaNotificationManager.getManager(NOTIFICATION_ID);
        }
    }

    // Three classes to specify the right notification id in the intent.

    /**
     * This class is used internally but have to be public to be able to launch the service.
     */
    public static final class PlaybackMediaButtonReceiver extends MediaButtonReceiver {
        @Override
        public String getServiceClassName() {
            return PlaybackListenerService.class.getName();
        }
    }

    /**
     * This class is used internally but have to be public to be able to launch the service.
     */
    public static final class PresentationMediaButtonReceiver extends MediaButtonReceiver {
        @Override
        public String getServiceClassName() {
            return PresentationListenerService.class.getName();
        }
    }

    /**
     * This class is used internally but have to be public to be able to launch the service.
     */
    public static final class CastMediaButtonReceiver extends MediaButtonReceiver {
        @Override
        public String getServiceClassName() {
            return CastListenerService.class.getName();
        }
    }

    private Intent createIntent(Context context) {
        Intent intent = null;
        if (mMediaNotificationInfo.id == PlaybackListenerService.NOTIFICATION_ID) {
            intent = new Intent(context, PlaybackListenerService.class);
        } else if (mMediaNotificationInfo.id == PresentationListenerService.NOTIFICATION_ID) {
            intent = new Intent(context, PresentationListenerService.class);
        }  else if (mMediaNotificationInfo.id == CastListenerService.NOTIFICATION_ID) {
            intent = new Intent(context, CastListenerService.class);
        }
        return intent;
    }

    private PendingIntent createPendingIntent(String action) {
        Intent intent = createIntent(mContext).setAction(action);
        return PendingIntent.getService(mContext, 0, intent, PendingIntent.FLAG_CANCEL_CURRENT);
    }

    private String getButtonReceiverClassName() {
        if (mMediaNotificationInfo.id == PlaybackListenerService.NOTIFICATION_ID) {
            return PlaybackMediaButtonReceiver.class.getName();
        } else if (mMediaNotificationInfo.id == PresentationListenerService.NOTIFICATION_ID) {
            return PresentationMediaButtonReceiver.class.getName();
        } else if (mMediaNotificationInfo.id == CastListenerService.NOTIFICATION_ID) {
            return CastMediaButtonReceiver.class.getName();
        }

        assert false;
        return null;
    }

    // Returns the notification group name used to prevent automatic grouping.
    private String getNotificationGroupName() {
        if (mMediaNotificationInfo.id == PlaybackListenerService.NOTIFICATION_ID) {
            return NotificationConstants.GROUP_MEDIA_PLAYBACK;
        } else if (mMediaNotificationInfo.id == PresentationListenerService.NOTIFICATION_ID) {
            return NotificationConstants.GROUP_MEDIA_PRESENTATION;
        } else if (mMediaNotificationInfo.id == CastListenerService.NOTIFICATION_ID) {
            return NotificationConstants.GROUP_MEDIA_REMOTE;
        }

        assert false;
        return null;
    }

    /**
     * Shows the notification with media controls with the specified media info. Replaces/updates
     * the current notification if already showing. Does nothing if |mediaNotificationInfo| hasn't
     * changed from the last one. If |mediaNotificationInfo.isPaused| is true and the tabId
     * mismatches |mMediaNotificationInfo.isPaused|, it is also no-op.
     *
     * @param applicationContext context to create the notification with
     * @param notificationInfo information to show in the notification
     */
    public static void show(Context applicationContext,
                            MediaNotificationInfo notificationInfo) {
        synchronized (LOCK) {
            if (sManagers == null) {
                sManagers = new SparseArray<MediaNotificationManager>();
            }
        }

        MediaNotificationManager manager = sManagers.get(notificationInfo.id);
        if (manager == null) {
            manager = new MediaNotificationManager(applicationContext, notificationInfo.id);
            sManagers.put(notificationInfo.id, manager);
        }

        manager.showNotification(notificationInfo);
    }

    /**
     * Hides the notification for the specified tabId and notificationId
     *
     * @param tabId the id of the tab that showed the notification or invalid tab id.
     * @param notificationId the id of the notification to hide for this tab.
     */
    public static void hide(int tabId, int notificationId) {
        MediaNotificationManager manager = getManager(notificationId);
        if (manager == null) return;

        manager.hideNotification(tabId);
    }

    /**
     * Hides notifications with the specified id for all tabs if shown.
     *
     * @param notificationId the id of the notification to hide for all tabs.
     */
    public static void clear(int notificationId) {
        MediaNotificationManager manager = getManager(notificationId);
        if (manager == null) return;

        manager.clearNotification();
        sManagers.remove(notificationId);
    }

    /**
     * Hides notifications with all known ids for all tabs if shown.
     */
    public static void clearAll() {
        if (sManagers == null) return;

        for (int i = 0; i < sManagers.size(); ++i) {
            MediaNotificationManager manager = sManagers.valueAt(i);
            manager.clearNotification();
        }
        sManagers.clear();
    }

    /**
     * Activates the Android MediaSession. This method is used to activate Android MediaSession more
     * often because some old version of Android might send events to the latest active session
     * based on when setActive(true) was called and regardless of the current playback state.
     * @param tabId the id of the tab requesting to reactivate the Android MediaSession.
     * @param notificationId the id of the notification to reactivate Android MediaSession for.
     */
    public static void activateAndroidMediaSession(int tabId, int notificationId) {
        MediaNotificationManager manager = getManager(notificationId);
        if (manager == null) return;
        manager.activateAndroidMediaSession(tabId);
    }

    /**
     * Downscale |icon| for display in the notification if needed. Returns null if |icon| is null.
     * If |icon| is larger than {@link getIdealMediaImageSize()}, scale it down to
     * {@link getIdealMediaImageSize()} and return. Otherwise return the original |icon|.
     * @param icon The icon to be scaled.
     */
    @Nullable
    public static Bitmap downscaleIconToIdealSize(@Nullable Bitmap icon) {
        if (icon == null) return null;

        int targetSize = getIdealMediaImageSize();

        Matrix m = new Matrix();
        int dominantLength = Math.max(icon.getWidth(), icon.getHeight());

        if (dominantLength < getIdealMediaImageSize()) return icon;

        // Move the center to (0,0).
        m.postTranslate(icon.getWidth() / -2.0f, icon.getHeight() / -2.0f);
        // Scale to desired size.
        float scale = 1.0f * targetSize / dominantLength;
        m.postScale(scale, scale);
        // Move to the desired place.
        m.postTranslate(targetSize / 2.0f, targetSize / 2.0f);

        // Draw the image.
        Bitmap paddedBitmap = Bitmap.createBitmap(targetSize, targetSize, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(paddedBitmap);
        Paint paint = new Paint(Paint.FILTER_BITMAP_FLAG);
        canvas.drawBitmap(icon, m, paint);
        return paddedBitmap;
    }

    /**
     * @returns The ideal size of the media image.
     */
    public static int getIdealMediaImageSize() {
        if (SysUtils.isLowEndDevice()) {
            return LOW_IMAGE_SIZE_PX;
        }
        return HIGH_IMAGE_SIZE_PX;
    }

    private static MediaNotificationManager getManager(int notificationId) {
        if (sManagers == null) return null;

        return sManagers.get(notificationId);
    }

    @VisibleForTesting
    static boolean hasManagerForTesting(int notificationId) {
        return getManager(notificationId) != null;
    }

    @VisibleForTesting
    @Nullable
    static ChromeNotificationBuilder getNotificationBuilderForTesting(int notificationId) {
        MediaNotificationManager manager = getManager(notificationId);
        if (manager == null) return null;

        return manager.mNotificationBuilder;
    }

    @VisibleForTesting
    @Nullable
    static MediaNotificationInfo getMediaNotificationInfoForTesting(
            int notificationId) {
        MediaNotificationManager manager = getManager(notificationId);

        return (manager == null) ? null : manager.mMediaNotificationInfo;
    }

    private static boolean isRunningN() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.N;
    }

    /**
     * The class containing all the information for adding a button in the notification for an
     * action.
     */
    private static final class MediaButtonInfo {
        /** The resource ID of this media button icon. */
        public int iconResId;

        /** The resource ID of this media button description. */
        public int descriptionResId;

        /** The intent string to be fired when this media button is clicked. */
        public String intentString;

        public MediaButtonInfo(int buttonResId, int descriptionResId, String intentString) {
            this.iconResId = buttonResId;
            this.descriptionResId = descriptionResId;
            this.intentString = intentString;
        }
    }
    private MediaNotificationManager(Context context, int notificationId) {
        mContext = context;

        mActionToButtonInfo = new SparseArray<>();

        mActionToButtonInfo.put(MediaSessionAction.PLAY,
                new MediaButtonInfo(R.drawable.ic_play_arrow_white_36dp,
                        R.string.accessibility_play, ListenerService.ACTION_PLAY));
        mActionToButtonInfo.put(MediaSessionAction.PAUSE,
                new MediaButtonInfo(R.drawable.ic_pause_white_36dp, R.string.accessibility_pause,
                        ListenerService.ACTION_PAUSE));
        mActionToButtonInfo.put(CUSTOM_MEDIA_SESSION_ACTION_STOP,
                new MediaButtonInfo(R.drawable.ic_stop_white_36dp, R.string.accessibility_stop,
                        ListenerService.ACTION_STOP));
        mActionToButtonInfo.put(MediaSessionAction.PREVIOUS_TRACK,
                new MediaButtonInfo(R.drawable.ic_skip_previous_white_36dp,
                        R.string.accessibility_previous_track,
                        ListenerService.ACTION_PREVIOUS_TRACK));
        mActionToButtonInfo.put(MediaSessionAction.NEXT_TRACK,
                new MediaButtonInfo(R.drawable.ic_skip_next_white_36dp,
                        R.string.accessibility_next_track, ListenerService.ACTION_NEXT_TRACK));
        mActionToButtonInfo.put(MediaSessionAction.SEEK_FORWARD,
                new MediaButtonInfo(R.drawable.ic_fast_forward_white_36dp,
                        R.string.accessibility_seek_forward, ListenerService.ACTION_SEEK_FORWARD));
        mActionToButtonInfo.put(MediaSessionAction.SEEK_BACKWARD,
                new MediaButtonInfo(R.drawable.ic_fast_rewind_white_36dp,
                        R.string.accessibility_seek_backward,
                        ListenerService.ACTION_SEEK_BACKWARD));
    }

    /**
     * Registers the started {@link Service} with the manager and creates the notification.
     *
     * @param service the service that was started
     */
    private void onServiceStarted(ListenerService service) {
        if (mService == service) return;

        mService = service;
        updateNotification();
    }

    /**
     * Handles the service destruction destruction.
     */
    private void onServiceDestroyed() {
        mService = null;
        if (mMediaNotificationInfo != null) clear(mMediaNotificationInfo.id);
    }

    private void onPlay(int actionSource) {
        if (!mMediaNotificationInfo.isPaused) return;
        mMediaNotificationInfo.listener.onPlay(actionSource);
    }

    private void onPause(int actionSource) {
        if (mMediaNotificationInfo.isPaused) return;
        mMediaNotificationInfo.listener.onPause(actionSource);
    }

    private void onStop(int actionSource) {
        mMediaNotificationInfo.listener.onStop(actionSource);
    }

    private void onMediaSessionAction(int action) {
        mMediaNotificationInfo.listener.onMediaSessionAction(action);
    }

    private void showNotification(MediaNotificationInfo mediaNotificationInfo) {
        if (mediaNotificationInfo.equals(mMediaNotificationInfo)) return;
        if (mediaNotificationInfo.isPaused && mMediaNotificationInfo != null
                && mediaNotificationInfo.tabId != mMediaNotificationInfo.tabId) {
            return;
        }

        mMediaNotificationInfo = mediaNotificationInfo;

        // If there's no pending service start request, don't try to start service. If there is a
        // pending service start request but the service haven't started yet, only update the
        // |mMediaNotificationInfo|. The service will update the notification later once it's
        // started.
        if (mService == null && mediaNotificationInfo.isPaused) return;

        if (mService == null) {
            updateMediaSession();
            updateNotificationBuilder();
            AppHooks.get().startForegroundService(createIntent(mContext));
        } else {
            mService.startService(createIntent(mContext));
        }
        updateNotification();
    }

    private void clearNotification() {
        if (mMediaNotificationInfo == null) return;

        NotificationManagerCompat manager = NotificationManagerCompat.from(mContext);
        manager.cancel(mMediaNotificationInfo.id);

        if (mMediaSession != null) {
            mMediaSession.setCallback(null);
            mMediaSession.setActive(false);
            mMediaSession.release();
            mMediaSession = null;
        }
        if (mService != null) {
            mContext.stopService(createIntent(mContext));
        }
        mMediaNotificationInfo = null;
        mNotificationBuilder = null;
    }

    private void hideNotification(int tabId) {
        if (mMediaNotificationInfo == null || tabId != mMediaNotificationInfo.tabId) return;
        clearNotification();
    }

    @Nullable
    private MediaMetadataCompat createMetadata() {
        if (mMediaNotificationInfo.isPrivate) return null;

        MediaMetadataCompat.Builder metadataBuilder = new MediaMetadataCompat.Builder();

        metadataBuilder.putString(MediaMetadataCompat.METADATA_KEY_TITLE,
                mMediaNotificationInfo.metadata.getTitle());
        metadataBuilder.putString(MediaMetadataCompat.METADATA_KEY_ARTIST,
                mMediaNotificationInfo.origin);

        if (!TextUtils.isEmpty(mMediaNotificationInfo.metadata.getArtist())) {
            metadataBuilder.putString(MediaMetadataCompat.METADATA_KEY_ARTIST,
                    mMediaNotificationInfo.metadata.getArtist());
        }
        if (!TextUtils.isEmpty(mMediaNotificationInfo.metadata.getAlbum())) {
            metadataBuilder.putString(MediaMetadataCompat.METADATA_KEY_ALBUM,
                    mMediaNotificationInfo.metadata.getAlbum());
        }
        if (mMediaNotificationInfo.mediaSessionImage != null) {
            metadataBuilder.putBitmap(MediaMetadataCompat.METADATA_KEY_ALBUM_ART,
                                      mMediaNotificationInfo.mediaSessionImage);
        }

        return metadataBuilder.build();
    }

    private void updateNotification() {
        if (mService == null) return;

        if (mMediaNotificationInfo == null) return;

        updateMediaSession();
        updateNotificationBuilder();

        Notification notification = mNotificationBuilder.build();

        // We keep the service as a foreground service while the media is playing. When it is not,
        // the service isn't stopped but is no longer in foreground, thus at a lower priority.
        // While the service is in foreground, the associated notification can't be swipped away.
        // Moving it back to background allows the user to remove the notification.
        if (mMediaNotificationInfo.supportsSwipeAway() && mMediaNotificationInfo.isPaused) {
            mService.stopForeground(false /* removeNotification */);

            NotificationManagerCompat manager = NotificationManagerCompat.from(mContext);
            manager.notify(mMediaNotificationInfo.id, notification);
        } else {
            mService.startForeground(mMediaNotificationInfo.id, notification);
        }
    }

    private void updateNotificationBuilder() {
        mNotificationBuilder = AppHooks.get().createChromeNotificationBuilder(
                true /* preferCompat */, NotificationConstants.CATEGORY_ID_BROWSER,
                mContext.getString(org.chromium.chrome.R.string.notification_category_browser),
                NotificationConstants.CATEGORY_GROUP_ID_GENERAL,
                mContext.getString(
                        org.chromium.chrome.R.string.notification_category_group_general));
        setMediaStyleLayoutForNotificationBuilder(mNotificationBuilder);

        mNotificationBuilder.setSmallIcon(mMediaNotificationInfo.notificationSmallIcon);
        mNotificationBuilder.setAutoCancel(false);
        mNotificationBuilder.setLocalOnly(true);
        mNotificationBuilder.setGroup(getNotificationGroupName());
        mNotificationBuilder.setGroupSummary(true);

        if (mMediaNotificationInfo.supportsSwipeAway()) {
            mNotificationBuilder.setOngoing(!mMediaNotificationInfo.isPaused);
            mNotificationBuilder.setDeleteIntent(createPendingIntent(ListenerService.ACTION_SWIPE));
        }

        // The intent will currently only be null when using a custom tab.
        // TODO(avayvod) work out what we should do in this case. See https://crbug.com/585395.
        if (mMediaNotificationInfo.contentIntent != null) {
            mNotificationBuilder.setContentIntent(PendingIntent.getActivity(mContext,
                    mMediaNotificationInfo.tabId, mMediaNotificationInfo.contentIntent,
                    PendingIntent.FLAG_UPDATE_CURRENT));
            // Set FLAG_UPDATE_CURRENT so that the intent extras is updated, otherwise the
            // intent extras will stay the same for the same tab.
        }

        mNotificationBuilder.setVisibility(
                mMediaNotificationInfo.isPrivate ? NotificationCompat.VISIBILITY_PRIVATE
                                                 : NotificationCompat.VISIBILITY_PUBLIC);
    }

    private void updateMediaSession() {
        if (!mMediaNotificationInfo.supportsPlayPause()) return;

        if (mMediaSession == null) mMediaSession = createMediaSession();

        activateAndroidMediaSession(mMediaNotificationInfo.tabId);

        try {
            // Tell the MediaRouter about the session, so that Chrome can control the volume
            // on the remote cast device (if any).
            // Pre-MR1 versions of JB do not have the complete MediaRouter APIs,
            // so getting the MediaRouter instance will throw an exception.
            MediaRouter.getInstance(mContext).setMediaSessionCompat(mMediaSession);
        } catch (NoSuchMethodError e) {
            // Do nothing. Chrome can't be casting without a MediaRouter, so there is nothing
            // to do here.
        }

        mMediaSession.setMetadata(createMetadata());

        PlaybackStateCompat.Builder playbackStateBuilder =
                new PlaybackStateCompat.Builder().setActions(computeMediaSessionActions());
        if (mMediaNotificationInfo.isPaused) {
            playbackStateBuilder.setState(PlaybackStateCompat.STATE_PAUSED,
                    PlaybackStateCompat.PLAYBACK_POSITION_UNKNOWN, 1.0f);
        } else {
            // If notification only supports stop, still pretend
            playbackStateBuilder.setState(PlaybackStateCompat.STATE_PLAYING,
                    PlaybackStateCompat.PLAYBACK_POSITION_UNKNOWN, 1.0f);
        }
        mMediaSession.setPlaybackState(playbackStateBuilder.build());
    }

    private long computeMediaSessionActions() {
        assert mMediaNotificationInfo != null;

        long actions = PlaybackStateCompat.ACTION_PLAY | PlaybackStateCompat.ACTION_PAUSE;
        if (mMediaNotificationInfo.mediaSessionActions.contains(
                    MediaSessionAction.PREVIOUS_TRACK)) {
            actions |= PlaybackStateCompat.ACTION_SKIP_TO_PREVIOUS;
        }
        if (mMediaNotificationInfo.mediaSessionActions.contains(MediaSessionAction.NEXT_TRACK)) {
            actions |= PlaybackStateCompat.ACTION_SKIP_TO_NEXT;
        }
        if (mMediaNotificationInfo.mediaSessionActions.contains(MediaSessionAction.SEEK_FORWARD)) {
            actions |= PlaybackStateCompat.ACTION_FAST_FORWARD;
        }
        if (mMediaNotificationInfo.mediaSessionActions.contains(MediaSessionAction.SEEK_BACKWARD)) {
            actions |= PlaybackStateCompat.ACTION_REWIND;
        }
        return actions;
    }

    private MediaSessionCompat createMediaSession() {
        MediaSessionCompat mediaSession = new MediaSessionCompat(
                mContext,
                mContext.getString(R.string.app_name),
                new ComponentName(mContext.getPackageName(),
                        getButtonReceiverClassName()),
                null);
        mediaSession.setFlags(MediaSessionCompat.FLAG_HANDLES_MEDIA_BUTTONS
                | MediaSessionCompat.FLAG_HANDLES_TRANSPORT_CONTROLS);
        mediaSession.setCallback(mMediaSessionCallback);

        // TODO(mlamouri): the following code is to work around a bug that hopefully
        // MediaSessionCompat will handle directly. see b/24051980.
        try {
            mediaSession.setActive(true);
        } catch (NullPointerException e) {
            // Some versions of KitKat do not support AudioManager.registerMediaButtonIntent
            // with a PendingIntent. They will throw a NullPointerException, in which case
            // they should be able to activate a MediaSessionCompat with only transport
            // controls.
            mediaSession.setActive(false);
            mediaSession.setFlags(MediaSessionCompat.FLAG_HANDLES_TRANSPORT_CONTROLS);
            mediaSession.setActive(true);
        }
        return mediaSession;
    }

    private void activateAndroidMediaSession(int tabId) {
        if (mMediaNotificationInfo == null) return;
        if (mMediaNotificationInfo.tabId != tabId) return;
        if (!mMediaNotificationInfo.supportsPlayPause() || mMediaNotificationInfo.isPaused) return;
        if (mMediaSession == null) return;
        mMediaSession.setActive(true);
    }

    private void setMediaStyleLayoutForNotificationBuilder(ChromeNotificationBuilder builder) {
        setMediaStyleNotificationText(builder);
        if (!mMediaNotificationInfo.supportsPlayPause()) {
            builder.setLargeIcon(null);
        } else if (mMediaNotificationInfo.notificationLargeIcon != null) {
            builder.setLargeIcon(mMediaNotificationInfo.notificationLargeIcon);
        } else if (!isRunningN()) {
            if (mDefaultNotificationLargeIcon == null) {
                int resourceId = (mMediaNotificationInfo.defaultNotificationLargeIcon != 0)
                        ? mMediaNotificationInfo.defaultNotificationLargeIcon
                        : R.drawable.audio_playing_square;
                mDefaultNotificationLargeIcon = downscaleIconToIdealSize(
                    BitmapFactory.decodeResource(mContext.getResources(), resourceId));
            }
            builder.setLargeIcon(mDefaultNotificationLargeIcon);
        }
        // TODO(zqzhang): It's weird that setShowWhen() don't work on K. Calling setWhen() to force
        // removing the time.
        builder.setShowWhen(false).setWhen(0);

        addNotificationButtons(builder);
    }

    private void addNotificationButtons(ChromeNotificationBuilder builder) {
        Set<Integer> actions = new HashSet<>();

        // TODO(zqzhang): handle other actions when play/pause is not supported? See
        // https://crbug.com/667500
        if (mMediaNotificationInfo.supportsPlayPause()) {
            actions.addAll(mMediaNotificationInfo.mediaSessionActions);
            if (mMediaNotificationInfo.isPaused) {
                actions.remove(MediaSessionAction.PAUSE);
                actions.add(MediaSessionAction.PLAY);
            } else {
                actions.remove(MediaSessionAction.PLAY);
                actions.add(MediaSessionAction.PAUSE);
            }
        }

        if (mMediaNotificationInfo.supportsStop()) {
            actions.add(CUSTOM_MEDIA_SESSION_ACTION_STOP);
        }

        List<Integer> bigViewActions = computeBigViewActions(actions);

        for (int action : bigViewActions) {
            MediaButtonInfo buttonInfo = mActionToButtonInfo.get(action);
            builder.addAction(buttonInfo.iconResId,
                    mContext.getResources().getString(buttonInfo.descriptionResId),
                    createPendingIntent(buttonInfo.intentString));
        }

        // Only apply MediaStyle when NotificationInfo supports play/pause.
        if (mMediaNotificationInfo.supportsPlayPause()) {
            builder.setMediaStyle(mMediaSession, computeCompactViewActionIndices(bigViewActions),
                    createPendingIntent(ListenerService.ACTION_CANCEL), true);
        }
    }

    private Bitmap drawableToBitmap(Drawable drawable) {
        if (!(drawable instanceof BitmapDrawable)) return null;

        BitmapDrawable bitmapDrawable = (BitmapDrawable) drawable;
        return bitmapDrawable.getBitmap();
    }

    private void setMediaStyleNotificationText(ChromeNotificationBuilder builder) {
        builder.setContentTitle(mMediaNotificationInfo.metadata.getTitle());
        String artistAndAlbumText = getArtistAndAlbumText(mMediaNotificationInfo.metadata);
        if (isRunningN() || !artistAndAlbumText.isEmpty()) {
            builder.setContentText(artistAndAlbumText);
            builder.setSubText(mMediaNotificationInfo.origin);
        } else {
            // Leaving ContentText empty looks bad, so move origin up to the ContentText.
            builder.setContentText(mMediaNotificationInfo.origin);
        }
    }

    private String getArtistAndAlbumText(MediaMetadata metadata) {
        String artist = (metadata.getArtist() == null) ? "" : metadata.getArtist();
        String album = (metadata.getAlbum() == null) ? "" : metadata.getAlbum();
        if (artist.isEmpty() || album.isEmpty()) {
            return artist + album;
        }
        return artist + " - " + album;
    }

    /**
     * Compute the actions to be shown in BigView media notification.
     *
     * The method assumes STOP cannot coexist with switch track actions and seeking actions. It also
     * assumes PLAY and PAUSE cannot coexist.
     *
     * TODO(zqzhang): get UX feedback to decide which actions to select when the number of actions
     * is greater than that can be displayed. See https://crbug.com/667500
     */
    private List<Integer> computeBigViewActions(Set<Integer> actions) {
        // STOP cannot coexist with switch track actions and seeking actions.
        assert !actions.contains(CUSTOM_MEDIA_SESSION_ACTION_STOP)
                || !(actions.contains(MediaSessionAction.PREVIOUS_TRACK)
                        && actions.contains(MediaSessionAction.NEXT_TRACK)
                        && actions.contains(MediaSessionAction.SEEK_BACKWARD)
                        && actions.contains(MediaSessionAction.SEEK_FORWARD));
        // PLAY and PAUSE cannot coexist.
        assert !actions.contains(MediaSessionAction.PLAY)
                || !actions.contains(MediaSessionAction.PAUSE);

        int[] actionByOrder = {
                MediaSessionAction.PREVIOUS_TRACK,
                MediaSessionAction.SEEK_BACKWARD,
                MediaSessionAction.PLAY,
                MediaSessionAction.PAUSE,
                MediaSessionAction.SEEK_FORWARD,
                MediaSessionAction.NEXT_TRACK,
                CUSTOM_MEDIA_SESSION_ACTION_STOP,
        };

        // First, select at most |BIG_VIEW_ACTIONS_COUNT| actions by priority.
        Set<Integer> selectedActions;
        if (actions.size() <= BIG_VIEW_ACTIONS_COUNT) {
            selectedActions = actions;
        } else {
            selectedActions = new HashSet<>();
            if (actions.contains(CUSTOM_MEDIA_SESSION_ACTION_STOP)) {
                selectedActions.add(CUSTOM_MEDIA_SESSION_ACTION_STOP);
            } else {
                if (actions.contains(MediaSessionAction.PLAY)) {
                    selectedActions.add(MediaSessionAction.PLAY);
                } else {
                    selectedActions.add(MediaSessionAction.PAUSE);
                }
                if (actions.contains(MediaSessionAction.PREVIOUS_TRACK)
                        && actions.contains(MediaSessionAction.NEXT_TRACK)) {
                    selectedActions.add(MediaSessionAction.PREVIOUS_TRACK);
                    selectedActions.add(MediaSessionAction.NEXT_TRACK);
                } else {
                    selectedActions.add(MediaSessionAction.SEEK_BACKWARD);
                    selectedActions.add(MediaSessionAction.SEEK_FORWARD);
                }
            }
        }

        // Second, sort the selected actions.
        List<Integer> sortedActions = new ArrayList<>();
        for (int action : actionByOrder) {
            if (selectedActions.contains(action)) sortedActions.add(action);
        }

        return sortedActions;
    }

    /**
     * Compute the actions to be shown in CompactView media notification.
     *
     * The method assumes STOP cannot coexist with switch track actions and seeking actions. It also
     * assumes PLAY and PAUSE cannot coexist.
     *
     * Actions in pairs are preferred if there are more actions than |COMPACT_VIEW_ACTIONS_COUNT|.
     */
    @VisibleForTesting
    static int[] computeCompactViewActionIndices(List<Integer> actions) {
        // STOP cannot coexist with switch track actions and seeking actions.
        assert !actions.contains(CUSTOM_MEDIA_SESSION_ACTION_STOP)
                || !(actions.contains(MediaSessionAction.PREVIOUS_TRACK)
                        && actions.contains(MediaSessionAction.NEXT_TRACK)
                        && actions.contains(MediaSessionAction.SEEK_BACKWARD)
                        && actions.contains(MediaSessionAction.SEEK_FORWARD));
        // PLAY and PAUSE cannot coexist.
        assert !actions.contains(MediaSessionAction.PLAY)
                || !actions.contains(MediaSessionAction.PAUSE);

        if (actions.size() <= COMPACT_VIEW_ACTIONS_COUNT) {
            // If the number of actions is less than |COMPACT_VIEW_ACTIONS_COUNT|, just return an
            // array of 0, 1, ..., |actions.size()|-1.
            int[] actionsArray = new int[actions.size()];
            for (int i = 0; i < actions.size(); ++i) actionsArray[i] = i;
            return actionsArray;
        }

        if (actions.contains(CUSTOM_MEDIA_SESSION_ACTION_STOP)) {
            List<Integer> compactActions = new ArrayList<>();
            if (actions.contains(MediaSessionAction.PLAY)) {
                compactActions.add(actions.indexOf(MediaSessionAction.PLAY));
            }
            compactActions.add(actions.indexOf(CUSTOM_MEDIA_SESSION_ACTION_STOP));
            return convertIntegerListToIntArray(compactActions);
        }

        int[] actionsArray = new int[COMPACT_VIEW_ACTIONS_COUNT];
        if (actions.contains(MediaSessionAction.PREVIOUS_TRACK)
                && actions.contains(MediaSessionAction.NEXT_TRACK)) {
            actionsArray[0] = actions.indexOf(MediaSessionAction.PREVIOUS_TRACK);
            if (actions.contains(MediaSessionAction.PLAY)) {
                actionsArray[1] = actions.indexOf(MediaSessionAction.PLAY);
            } else {
                actionsArray[1] = actions.indexOf(MediaSessionAction.PAUSE);
            }
            actionsArray[2] = actions.indexOf(MediaSessionAction.NEXT_TRACK);
            return actionsArray;
        }

        assert actions.contains(MediaSessionAction.SEEK_BACKWARD)
                && actions.contains(MediaSessionAction.SEEK_FORWARD);
        actionsArray[0] = actions.indexOf(MediaSessionAction.SEEK_BACKWARD);
        if (actions.contains(MediaSessionAction.PLAY)) {
            actionsArray[1] = actions.indexOf(MediaSessionAction.PLAY);
        } else {
            actionsArray[1] = actions.indexOf(MediaSessionAction.PAUSE);
        }
        actionsArray[2] = actions.indexOf(MediaSessionAction.SEEK_FORWARD);

        return actionsArray;
    }

    static int[] convertIntegerListToIntArray(List<Integer> intList) {
        int[] intArray = new int[intList.size()];
        for (int i = 0; i < intList.size(); ++i) intArray[i] = i;
        return intArray;
    }
}
