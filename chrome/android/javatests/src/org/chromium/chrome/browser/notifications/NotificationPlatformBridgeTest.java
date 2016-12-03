// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.annotation.SuppressLint;
import android.app.Notification;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.website.ContentSetting;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.chrome.test.util.browser.notifications.MockNotificationManagerProxy.NotificationEntry;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.List;

/**
 * Instrumentation tests for the Notification Bridge.
 *
 * Web Notifications are only supported on Android JellyBean and beyond.
 */
// TODO(peter): remove @SuppressLint once crbug.com/501900 is fixed.
@SuppressLint("NewApi")
// TODO(peter): fix deprecation warnings crbug.com/528076
@SuppressWarnings("deprecation")
public class NotificationPlatformBridgeTest extends NotificationTestBase {
    private static final String NOTIFICATION_TEST_PAGE =
            "/chrome/test/data/notifications/android_test.html";

    /**
     * Verifies that the intended default properties of a notification will indeed be set on the
     * Notification object that will be send to Android.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testDefaultNotificationProperties() throws Exception {
        loadUrl(getTestServer().getURL(NOTIFICATION_TEST_PAGE));
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification = showAndGetNotification("MyNotification", "{ body: 'Hello' }");

        // Validate the contents of the notification.
        assertEquals("MyNotification", notification.extras.getString(Notification.EXTRA_TITLE));
        assertEquals("Hello", notification.extras.getString(Notification.EXTRA_TEXT));
        assertEquals(UrlFormatter.formatUrlForSecurityDisplay(getOrigin(), false /* showScheme */),
                notification.extras.getString(Notification.EXTRA_SUB_TEXT));

        // Verify that the ticker text contains the notification's title and body.
        String tickerText = notification.tickerText.toString();

        assertTrue(tickerText.contains("MyNotification"));
        assertTrue(tickerText.contains("Hello"));

        // Verify that the notification's timestamp is set in the past 60 seconds. This number has
        // no significance, but needs to be high enough to not cause flakiness as it's set by the
        // renderer process on notification creation.
        assertTrue(Math.abs(System.currentTimeMillis() - notification.when) < 60 * 1000);

        assertNotNull(notification.largeIcon);

        // Validate the notification's behavior.
        assertEquals(Notification.DEFAULT_ALL, notification.defaults);
        assertEquals(Notification.PRIORITY_DEFAULT, notification.priority);
    }

    /**
     * Verifies that the ONLY_ALERT_ONCE flag is not set when renotify is true.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationRenotifyProperty() throws Exception {
        loadUrl(getTestServer().getURL(NOTIFICATION_TEST_PAGE));
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification =
                showAndGetNotification("MyNotification", "{ tag: 'myTag', renotify: true }");

        assertEquals(0, notification.flags & Notification.FLAG_ONLY_ALERT_ONCE);
    }

    /**
     * Verifies that notifications created with the "silent" flag do not inherit system defaults
     * in regards to their sound, vibration and light indicators.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationSilentProperty() throws Exception {
        loadUrl(getTestServer().getURL(NOTIFICATION_TEST_PAGE));
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification = showAndGetNotification("MyNotification", "{ silent: true }");

        // Zero indicates that no defaults should be inherited from the system.
        assertEquals(0, notification.defaults);
    }

    private void verifyVibrationNotRequestedWhenDisabledInPrefs(String notificationOptions)
            throws Exception {
        loadUrl(getTestServer().getURL(NOTIFICATION_TEST_PAGE));
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        // Disable notification vibration in preferences.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PrefServiceBridge.getInstance().setNotificationsVibrateEnabled(false);
            }
        });

        Notification notification = showAndGetNotification("MyNotification", notificationOptions);

        // Vibration should not be in the defaults.
        assertEquals(
                Notification.DEFAULT_ALL & ~Notification.DEFAULT_VIBRATE, notification.defaults);

        // There should be a custom no-op vibration pattern.
        assertEquals(1, notification.vibrate.length);
        assertEquals(0L, notification.vibrate[0]);
    }

    /**
     * Verifies that when notification vibration is disabled in preferences and no custom pattern is
     * specified, no vibration is requested from the framework.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationVibratePreferenceDisabledDefault() throws Exception {
        verifyVibrationNotRequestedWhenDisabledInPrefs("{}");
    }

    /**
     * Verifies that when notification vibration is disabled in preferences and a custom pattern is
     * specified, no vibration is requested from the framework.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationVibratePreferenceDisabledCustomPattern() throws Exception {
        verifyVibrationNotRequestedWhenDisabledInPrefs("{ vibrate: 42 }");
    }

    /**
     * Verifies that by default the notification vibration preference is enabled, and a custom
     * pattern is passed along.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationVibrateCustomPattern() throws Exception {
        loadUrl(getTestServer().getURL(NOTIFICATION_TEST_PAGE));
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        // By default, vibration is enabled in notifications.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertTrue(PrefServiceBridge.getInstance().isNotificationsVibrateEnabled());
            }
        });

        Notification notification = showAndGetNotification("MyNotification", "{ vibrate: 42 }");

        // Vibration should not be in the defaults, a custom pattern was provided.
        assertEquals(
                Notification.DEFAULT_ALL & ~Notification.DEFAULT_VIBRATE, notification.defaults);

        // The custom pattern should have been passed along.
        assertEquals(2, notification.vibrate.length);
        assertEquals(0L, notification.vibrate[0]);
        assertEquals(42L, notification.vibrate[1]);
    }

    /**
     * Verifies that notifications which specify an icon will have that icon fetched, converted into
     * a Bitmap and included as the large icon in the notification.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testShowNotificationWithIcon() throws Exception {
        loadUrl(getTestServer().getURL(NOTIFICATION_TEST_PAGE));
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification = showAndGetNotification("MyNotification", "{icon: 'icon.png'}");

        assertEquals("MyNotification", notification.extras.getString(Notification.EXTRA_TITLE));
        assertNotNull(notification.largeIcon);

        // TODO(peter): Do some more sensible checking that |icon.png| could actually be loaded.
        // One option might be to give that icon a solid color and check for it in the Bitmap, but
        // I'm not certain how reliable that would be.
    }

    /**
     * Verifies that notifications which don't specify an icon will get an automatically generated
     * icon based on their origin. The size of these icons are dependent on the resolution of the
     * device the test is being ran on, so we create it again in order to compare.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testShowNotificationWithoutIcon() throws Exception {
        loadUrl(getTestServer().getURL(NOTIFICATION_TEST_PAGE));
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification = showAndGetNotification("NoIconNotification", "{}");

        assertEquals("NoIconNotification", notification.extras.getString(Notification.EXTRA_TITLE));
        assertNotNull(notification.largeIcon);

        NotificationPlatformBridge notificationBridge =
                NotificationPlatformBridge.getInstanceForTests();
        assertNotNull(notificationBridge);

        // Create a second rounded icon for the test's origin, and compare its dimensions against
        // those of the icon associated to the notification itself.
        RoundedIconGenerator generator = notificationBridge.mIconGenerator;
        assertNotNull(generator);

        Bitmap generatedIcon = generator.generateIconForUrl(getOrigin());
        assertNotNull(generatedIcon);

        assertEquals(generatedIcon.getWidth(), notification.largeIcon.getWidth());
        assertEquals(generatedIcon.getHeight(), notification.largeIcon.getHeight());
    }

    /**
     * Tests the three paths for ensuring that a notification will be shown with a normalized icon:
     *     (1) NULL bitmaps should have an auto-generated image.
     *     (2) Large bitmaps should be resized to the device's intended size.
     *     (3) Smaller bitmaps should be left alone.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testEnsureNormalizedIconBehavior() throws Exception {
        loadUrl(getTestServer().getURL(NOTIFICATION_TEST_PAGE));
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        // Create a notification to ensure that the NotificationPlatformBridge is initialized.
        showAndGetNotification("MyNotification", "{}");

        // Get the dimensions of the notification icon that will be presented to the user.
        Context appContext = getInstrumentation().getTargetContext().getApplicationContext();
        Resources resources = appContext.getResources();

        int largeIconWidthPx =
                resources.getDimensionPixelSize(android.R.dimen.notification_large_icon_width);
        int largeIconHeightPx =
                resources.getDimensionPixelSize(android.R.dimen.notification_large_icon_height);

        String origin = "https://example.com";

        NotificationPlatformBridge notificationBridge =
                NotificationPlatformBridge.getInstanceForTests();
        assertNotNull(notificationBridge);

        Bitmap fromNullIcon = notificationBridge.ensureNormalizedIcon(null, origin);
        assertNotNull(fromNullIcon);
        assertEquals(largeIconWidthPx, fromNullIcon.getWidth());
        assertEquals(largeIconHeightPx, fromNullIcon.getHeight());

        Bitmap largeIcon = Bitmap.createBitmap(largeIconWidthPx * 2, largeIconHeightPx * 2,
                                               Bitmap.Config.ALPHA_8);

        Bitmap fromLargeIcon = notificationBridge.ensureNormalizedIcon(largeIcon, origin);
        assertNotNull(fromLargeIcon);
        assertEquals(largeIconWidthPx, fromLargeIcon.getWidth());
        assertEquals(largeIconHeightPx, fromLargeIcon.getHeight());

        Bitmap smallIcon = Bitmap.createBitmap(largeIconWidthPx / 2, largeIconHeightPx / 2,
                                               Bitmap.Config.ALPHA_8);

        Bitmap fromSmallIcon = notificationBridge.ensureNormalizedIcon(smallIcon, origin);
        assertNotNull(fromSmallIcon);
        assertEquals(smallIcon, fromSmallIcon);
    }

    /*
     * Verifies that starting the PendingIntent stored as the notification's content intent will
     * start up the associated Service Worker, where the JavaScript code will close the notification
     * by calling event.notification.close().
     */
    @LargeTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationContentIntentClosesNotification() throws Exception {
        loadUrl(getTestServer().getURL(NOTIFICATION_TEST_PAGE));
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification = showAndGetNotification("MyNotification", "{}");

        // Sending the PendingIntent resembles activating the notification.
        assertNotNull(notification.contentIntent);
        notification.contentIntent.send();

        // The Service Worker will close the notification upon receiving the notificationclick
        // event. This will eventually bubble up to a call to cancel() in the NotificationManager.
        waitForNotificationManagerMutation();
        assertTrue(getNotificationEntries().isEmpty());
    }

    /**
     * Verifies that starting the PendingIntent stored as the notification's content intent will
     * start up the associated Service Worker, where the JavaScript code will create a new tab for
     * displaying the notification's event to the user.
     */
    @LargeTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationContentIntentCreatesTab() throws Exception {
        loadUrl(getTestServer().getURL(NOTIFICATION_TEST_PAGE));
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        assertEquals("Expected the notification test page to be the sole tab in the current model",
                1, getActivity().getCurrentTabModel().getCount());

        Notification notification =
                showAndGetNotification("MyNotification", "{ data: 'ACTION_CREATE_TAB' }");

        // Sending the PendingIntent resembles activating the notification.
        assertNotNull(notification.contentIntent);
        notification.contentIntent.send();

        // The Service Worker, upon receiving the notificationclick event, will create a new tab
        // after which it closes the notification.
        waitForNotificationManagerMutation();
        assertTrue(getNotificationEntries().isEmpty());

        CriteriaHelper.pollInstrumentationThread(new Criteria("Expected a new tab to be created") {
            @Override
            public boolean isSatisfied() {
                return 2 == getActivity().getCurrentTabModel().getCount();
            }
        });
    }

    /**
     * Verifies that creating a notification with an associated "tag" will cause any previous
     * notification with the same tag to be dismissed prior to being shown.
     */
    @SuppressFBWarnings("DLS_DEAD_LOCAL_STORE")
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationTagReplacement() throws Exception {
        loadUrl(getTestServer().getURL(NOTIFICATION_TEST_PAGE));
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        runJavaScriptCodeInCurrentTab("showNotification('MyNotification', {tag: 'myTag'});");
        waitForNotificationManagerMutation();
        List<NotificationEntry> notifications = getNotificationEntries();
        String tag = notifications.get(0).tag;
        int id = notifications.get(0).id;

        runJavaScriptCodeInCurrentTab("showNotification('SecondNotification', {tag: 'myTag'});");
        waitForNotificationManagerMutation();

        // Verify that the notification was successfully replaced.
        notifications = getNotificationEntries();
        assertEquals(1, notifications.size());
        assertEquals("SecondNotification",
                notifications.get(0).notification.extras.getString(Notification.EXTRA_TITLE));

        // Verify that for replaced notifications their tag was the same.
        assertEquals(tag, notifications.get(0).tag);

        // Verify that as always, the same integer is used, also for replaced notifications.
        assertEquals(id, notifications.get(0).id);
        assertEquals(NotificationPlatformBridge.PLATFORM_ID, notifications.get(0).id);
    }

    /**
     * Verifies that multiple notifications without a tag can be opened and closed without
     * affecting eachother.
     */
    @LargeTest
    @Feature({"Browser", "Notifications"})
    public void testShowAndCloseMultipleNotifications() throws Exception {
        loadUrl(getTestServer().getURL(NOTIFICATION_TEST_PAGE));
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        // Open the first notification and verify it is displayed.
        runJavaScriptCodeInCurrentTab("showNotification('One');");
        waitForNotificationManagerMutation();
        List<NotificationEntry> notifications = getNotificationEntries();
        assertEquals(1, notifications.size());
        Notification notificationOne = notifications.get(0).notification;
        assertEquals("One", notificationOne.extras.getString(Notification.EXTRA_TITLE));

        // Open the second notification and verify it is displayed.
        runJavaScriptCodeInCurrentTab("showNotification('Two');");
        waitForNotificationManagerMutation();
        notifications = getNotificationEntries();
        assertEquals(2, notifications.size());
        Notification notificationTwo = notifications.get(1).notification;
        assertEquals("Two", notificationTwo.extras.getString(Notification.EXTRA_TITLE));

        // The same integer id is always used as it is not needed for uniqueness, we rely on the tag
        // for uniqueness when the replacement behavior is not needed.
        assertEquals(NotificationPlatformBridge.PLATFORM_ID, notifications.get(0).id);
        assertEquals(NotificationPlatformBridge.PLATFORM_ID, notifications.get(1).id);

        // As these notifications were not meant to replace eachother, they must not have the same
        // tag internally.
        assertFalse(notifications.get(0).tag.equals(notifications.get(1).tag));

        // Verify that the PendingIntent for content and delete is different for each notification.
        assertFalse(notificationOne.contentIntent.equals(notificationTwo.contentIntent));
        assertFalse(notificationOne.deleteIntent.equals(notificationTwo.deleteIntent));

        // Close the first notification and verify that only the second remains.
        // Sending the content intent resembles touching the notification. In response tho this the
        // notificationclick event is fired. The test service worker will close the notification
        // upon receiving the event.
        notificationOne.contentIntent.send();
        waitForNotificationManagerMutation();
        notifications = getNotificationEntries();
        assertEquals(1, notifications.size());
        assertEquals("Two",
                notifications.get(0).notification.extras.getString(Notification.EXTRA_TITLE));

        // Close the last notification and verify that none remain.
        notifications.get(0).notification.contentIntent.send();
        waitForNotificationManagerMutation();
        assertTrue(getNotificationEntries().isEmpty());
    }
}
