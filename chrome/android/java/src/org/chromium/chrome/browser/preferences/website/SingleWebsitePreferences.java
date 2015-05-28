// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.net.Uri;
import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.PreferenceFragment;
import android.preference.PreferenceScreen;
import android.text.format.Formatter;
import android.widget.ListAdapter;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ContentSettingsType;
import org.chromium.chrome.browser.UrlUtilities;

import java.net.URI;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Shows a list of HTML5 settings for a single website.
 */
public class SingleWebsitePreferences extends PreferenceFragment
        implements DialogInterface.OnClickListener, OnPreferenceChangeListener,
                OnPreferenceClickListener {
    // SingleWebsitePreferences expects either EXTRA_SITE (a Website) or
    // EXTRA_ORIGIN (a WebsiteAddress) to be present (but not both). If
    // EXTRA_SITE is present, the fragment will display the permissions in that
    // Website object. If EXTRA_ORIGIN is present, the fragment will find all
    // permissions for that website address and display those. If EXTRA_LOCATION
    // is present, the fragment will add a Location toggle, even if the site
    // specifies no Location permission.
    public static final String EXTRA_SITE = "org.chromium.chrome.preferences.site";
    public static final String EXTRA_ORIGIN = "org.chromium.chrome.preferences.origin";
    public static final String EXTRA_LOCATION = "org.chromium.chrome.preferences.location";

    // Preference keys, see single_website_preferences.xml
    // Headings:
    public static final String PREF_SITE_TITLE = "site_title";
    public static final String PREF_USAGE = "site_usage";
    public static final String PREF_PERMISSIONS = "site_permissions";
    // Actions at the top (if adding new, see hasUsagePreferences below):
    public static final String PREF_CLEAR_DATA = "clear_data";
    // Buttons:
    public static final String PREF_RESET_SITE = "reset_site_button";
    // Website permissions (if adding new, see hasPermissionsPreferences and resetSite below):
    public static final String PREF_COOKIES_PERMISSION = "cookies_permission_list";
    public static final String PREF_FULLSCREEN_PERMISSION = "fullscreen_permission_list";
    public static final String PREF_JAVASCRIPT_PERMISSION = "javascript_permission_list";
    public static final String PREF_LOCATION_ACCESS = "location_access_list";
    public static final String PREF_MIDI_SYSEX_PERMISSION = "midi_sysex_permission_list";
    public static final String PREF_POPUP_PERMISSION = "popup_permission_list";
    public static final String PREF_PROTECTED_MEDIA_IDENTIFIER_PERMISSION =
            "protected_media_identifier_permission_list";
    public static final String PREF_PUSH_NOTIFICATIONS_PERMISSION =
            "push_notifications_list";
    public static final String PREF_VOICE_AND_VIDEO_CAPTURE_PERMISSION =
            "voice_and_video_capture_permission_list";

    // All permissions from the permissions preference category must be listed here.
    // TODO(mvanouwerkerk): Use this array in more places to reduce verbosity.
    private static final String[] PERMISSION_PREFERENCE_KEYS = {
        PREF_COOKIES_PERMISSION,
        PREF_FULLSCREEN_PERMISSION,
        PREF_JAVASCRIPT_PERMISSION,
        PREF_LOCATION_ACCESS,
        PREF_MIDI_SYSEX_PERMISSION,
        PREF_POPUP_PERMISSION,
        PREF_PROTECTED_MEDIA_IDENTIFIER_PERMISSION,
        PREF_PUSH_NOTIFICATIONS_PERMISSION,
        PREF_VOICE_AND_VIDEO_CAPTURE_PERMISSION
    };

    // The website this page is displaying details about.
    private Website mSite;

    // The address of the site we want to display. Used only if EXTRA_ADDRESS is provided.
    private WebsiteAddress mSiteAddress;

    // A list of possible options for each list preference summary.
    private String[] mListPreferenceSummaries;

    private class SingleWebsitePermissionsPopulator
            implements WebsitePermissionsFetcher.WebsitePermissionsCallback {
        @Override
        public void onWebsitePermissionsAvailable(
                Map<String, Set<Website>> sitesByOrigin, Map<String, Set<Website>> sitesByHost) {
            // This method may be called after the activity has been destroyed.
            // In that case, bail out.
            if (getActivity() == null) return;

            // TODO(mvanouwerkerk): Do this merge at data retrieval time in C++, instead of now.
            List<Set<Website>> allSites = new ArrayList<>();
            allSites.addAll(sitesByOrigin.values());
            allSites.addAll(sitesByHost.values());
            // TODO(mvanouwerkerk): Avoid modifying the outer class from this inner class.
            mSite = mergePermissionInfoForTopLevelOrigin(mSiteAddress, allSites);
            displaySitePermissions();
        }
    }

    /**
     * Creates a Bundle with the correct arguments for opening this fragment for
     * the website with the given url.
     *
     * @param url The URL to open the fragment with. This is a complete url including scheme,
     *            domain, port,  path, etc.
     * @return The bundle to attach to the preferences intent.
     */
    public static Bundle createFragmentArgsForSite(String url) {
        Bundle fragmentArgs = new Bundle();
        // TODO(mvanouwerkerk): Define a pure getOrigin method in UrlUtilities that is the
        // equivalent of the call below, because this is perfectly fine for non-display purposes.
        String origin = UrlUtilities.getOriginForDisplay(URI.create(url), true /*  schowScheme */);
        fragmentArgs.putString(SingleWebsitePreferences.EXTRA_ORIGIN, origin);
        return fragmentArgs;
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        getActivity().setTitle(R.string.prefs_content_settings);
        Object extraSite = getArguments().getSerializable(EXTRA_SITE);
        Object extraOrigin = getArguments().getSerializable(EXTRA_ORIGIN);

        if (extraSite != null && extraOrigin == null) {
            mSite = (Website) extraSite;
            displaySitePermissions();
        } else if (extraOrigin != null && extraSite == null) {
            mSiteAddress = WebsiteAddress.create((String) extraOrigin);
            WebsitePermissionsFetcher fetcher =
                    new WebsitePermissionsFetcher(new SingleWebsitePermissionsPopulator());
            fetcher.fetchAllPreferences();
        } else {
            assert false : "Exactly one of EXTRA_SITE or EXTRA_SITE_ADDRESS must be provided.";
        }

        super.onActivityCreated(savedInstanceState);
    }

    /**
     * Given an address and a list of sets of websites, returns a new site with the same origin
     * as |address| which has merged into it the permissions of the matching input sites. If a
     * permission is found more than once, the one found first is used and the latter are ignored.
     * This should not drop any relevant data as there should not be duplicates like that in the
     * first place.
     *
     * @param address The address to search for.
     * @param websiteSets The websites to search in.
     * @return The merged website.
     */
    private static Website mergePermissionInfoForTopLevelOrigin(
            WebsiteAddress address, List<Set<Website>> websiteSets) {
        String origin = address.getOrigin();
        String host = Uri.parse(origin).getHost();
        Website merged = new Website(address);
        // This nested loop looks expensive, but the amount of data is likely to be relatively
        // small because most sites have very few permissions.
        for (Set<Website> websiteSet : websiteSets) {
            for (Website other : websiteSet) {
                if (merged.getCookieInfo() == null && other.getCookieInfo() != null
                        && permissionInfoIsForTopLevelOrigin(other.getCookieInfo(), origin)) {
                    merged.setCookieInfo(other.getCookieInfo());
                }
                if (merged.getFullscreenInfo() == null && other.getFullscreenInfo() != null
                        && permissionInfoIsForTopLevelOrigin(other.getFullscreenInfo(), origin)) {
                    merged.setFullscreenInfo(other.getFullscreenInfo());
                }
                if (merged.getGeolocationInfo() == null && other.getGeolocationInfo() != null
                        && permissionInfoIsForTopLevelOrigin(other.getGeolocationInfo(), origin)) {
                    merged.setGeolocationInfo(other.getGeolocationInfo());
                }
                if (merged.getMidiInfo() == null && other.getMidiInfo() != null
                        && permissionInfoIsForTopLevelOrigin(other.getMidiInfo(), origin)) {
                    merged.setMidiInfo(other.getMidiInfo());
                }
                if (merged.getProtectedMediaIdentifierInfo() == null
                        && other.getProtectedMediaIdentifierInfo() != null
                        && permissionInfoIsForTopLevelOrigin(
                                   other.getProtectedMediaIdentifierInfo(), origin)) {
                    merged.setProtectedMediaIdentifierInfo(other.getProtectedMediaIdentifierInfo());
                }
                if (merged.getPushNotificationInfo() == null
                        && other.getPushNotificationInfo() != null
                        && permissionInfoIsForTopLevelOrigin(
                                   other.getPushNotificationInfo(), origin)) {
                    merged.setPushNotificationInfo(other.getPushNotificationInfo());
                }
                if (merged.getVoiceAndVideoCaptureInfo() == null
                        && other.getVoiceAndVideoCaptureInfo() != null) {
                    if (origin.equals(other.getVoiceAndVideoCaptureInfo().getOrigin())
                            && (origin.equals(other.getVoiceAndVideoCaptureInfo().getEmbedderSafe())
                                       || "*".equals(other.getVoiceAndVideoCaptureInfo()
                                                             .getEmbedderSafe()))) {
                        merged.setVoiceAndVideoCaptureInfo(other.getVoiceAndVideoCaptureInfo());
                    }
                }
                if (merged.getLocalStorageInfo() == null
                        && other.getLocalStorageInfo() != null
                        && origin.equals(other.getLocalStorageInfo().getOrigin())) {
                    merged.setLocalStorageInfo(other.getLocalStorageInfo());
                }
                for (StorageInfo storageInfo : other.getStorageInfo()) {
                    if (host.equals(storageInfo.getHost())) {
                        merged.addStorageInfo(storageInfo);
                    }
                }

                // TODO(mvanouwerkerk): Make the various info types share a common interface that
                // supports reading the origin or host.
                // TODO(mvanouwerkerk): Merge in PopupExceptionInfo? It uses a pattern, and is never
                // set on Android.
                // TODO(mvanouwerkerk): Merge in JavaScriptExceptionInfo? It uses a pattern, and is
                // never set on Android.
            }
        }
        return merged;
    }

    private static boolean permissionInfoIsForTopLevelOrigin(PermissionInfo info, String origin) {
        // TODO(mvanouwerkerk): Find a more generic place for this method.
        return origin.equals(info.getOrigin())
                && (origin.equals(info.getEmbedderSafe()) || "*".equals(info.getEmbedderSafe()));
    }

    /**
     * Updates the permissions displayed in the UI by fetching them from mSite.
     * Must only be called once mSite is set.
     */
    private void displaySitePermissions() {
        addPreferencesFromResource(R.xml.single_website_preferences);
        mListPreferenceSummaries = getActivity().getResources().getStringArray(
                R.array.website_settings_permission_options);

        ListAdapter preferences = getPreferenceScreen().getRootAdapter();
        for (int i = 0; i < preferences.getCount(); ++i) {
            Preference preference = (Preference) preferences.getItem(i);
            if (PREF_SITE_TITLE.equals(preference.getKey())) {
                preference.setTitle(mSite.getTitle());
            } else if (PREF_CLEAR_DATA.equals(preference.getKey())) {
                long usage = mSite.getTotalUsage();
                if (usage > 0) {
                    Context context = preference.getContext();
                    preference.setTitle(String.format(
                            context.getString(R.string.origin_settings_storage_usage_brief),
                            Formatter.formatShortFileSize(context, usage)));
                    ((ClearWebsiteStorage) preference).setConfirmationListener(this);
                } else {
                    getPreferenceScreen().removePreference(preference);
                }
            } else if (PREF_RESET_SITE.equals(preference.getKey())) {
                preference.setOnPreferenceClickListener(this);
            } else if (PREF_COOKIES_PERMISSION.equals(preference.getKey())) {
                setUpListPreference(preference, mSite.getCookiePermission());
            } else if (PREF_FULLSCREEN_PERMISSION.equals(preference.getKey())) {
                setUpListPreference(preference, mSite.getFullscreenPermission());
            } else if (PREF_JAVASCRIPT_PERMISSION.equals(preference.getKey())) {
                setUpListPreference(preference, mSite.getJavaScriptPermission());
            } else if (PREF_LOCATION_ACCESS.equals(preference.getKey())) {
                Object locationAllowed = getArguments().getSerializable(EXTRA_LOCATION);
                if (mSite.getGeolocationPermission() == null && locationAllowed != null) {
                    String origin = mSite.getAddress().getOrigin();
                    mSite.setGeolocationInfo(new GeolocationInfo(origin, origin));
                    setUpListPreference(preference, (boolean) locationAllowed
                            ? ContentSetting.ALLOW : ContentSetting.BLOCK);
                } else {
                  setUpListPreference(preference, mSite.getGeolocationPermission());
                }
            } else if (PREF_MIDI_SYSEX_PERMISSION.equals(preference.getKey())) {
                setUpListPreference(preference, mSite.getMidiPermission());
            } else if (PREF_POPUP_PERMISSION.equals(preference.getKey())) {
                setUpListPreference(preference, mSite.getPopupPermission());
            } else if (PREF_PROTECTED_MEDIA_IDENTIFIER_PERMISSION.equals(preference.getKey())) {
                setUpListPreference(preference, mSite.getProtectedMediaIdentifierPermission());
            } else if (PREF_PUSH_NOTIFICATIONS_PERMISSION.equals(preference.getKey())) {
                setUpListPreference(preference, mSite.getPushNotificationPermission());
            } else if (PREF_VOICE_AND_VIDEO_CAPTURE_PERMISSION.equals(preference.getKey())) {
                configureVoiceAndVideoPreference(preference);
            }
        }

        // Remove categories if no sub-items.
        PreferenceScreen preferenceScreen = getPreferenceScreen();
        if (!hasUsagePreferences()) {
            Preference heading = preferenceScreen.findPreference(PREF_USAGE);
            preferenceScreen.removePreference(heading);
        }
        if (!hasPermissionsPreferences()) {
            Preference heading = preferenceScreen.findPreference(PREF_PERMISSIONS);
            preferenceScreen.removePreference(heading);
        }
    }

    private boolean hasUsagePreferences() {
        // New actions under the Usage preference category must be listed here so that the category
        // heading can be removed when no actions are shown.
        return getPreferenceScreen().findPreference(PREF_CLEAR_DATA) != null;
    }

    private boolean hasPermissionsPreferences() {
        PreferenceScreen screen = getPreferenceScreen();
        for (String key : PERMISSION_PREFERENCE_KEYS) {
            if (screen.findPreference(key) != null) return true;
        }
        return false;
    }

    /**
     * Initialize a ListPreference with a certain value.
     * @param preference The ListPreference to initialize.
     * @param value The value to initialize it to.
     */
    private void setUpListPreference(Preference preference, ContentSetting value) {
        if (value == null) {
            getPreferenceScreen().removePreference(preference);
            return;
        }

        ListPreference listPreference = (ListPreference) preference;

        Website.PermissionDataEntry entry = getPermissionDataEntry(preference.getKey());
        CharSequence[] keys = new String[2];
        CharSequence[] descriptions = new String[2];
        keys[0] = ContentSetting.ALLOW.toString();
        keys[1] = ContentSetting.BLOCK.toString();
        descriptions[0] = getResources().getString(
                entry.contentSettingToResourceIdForSite(ContentSetting.ALLOW));
        descriptions[1] = getResources().getString(
                entry.contentSettingToResourceIdForSite(ContentSetting.BLOCK));
        listPreference.setEntryValues(keys);
        listPreference.setEntries(descriptions);
        int index = (value == ContentSetting.ALLOW ? 0 : 1);
        listPreference.setValueIndex(index);
        if (entry.explanationResourceId != 0) {
            listPreference.setTitle(entry.explanationResourceId);
        }
        listPreference.setIcon(entry.iconResourceId);

        preference.setSummary(mListPreferenceSummaries[index]);
        listPreference.setOnPreferenceChangeListener(this);
    }

    private Website.PermissionDataEntry getPermissionDataEntry(String preferenceKey) {
        if (PREF_COOKIES_PERMISSION.equals(preferenceKey)) {
            return Website.PermissionDataEntry.getPermissionDataEntry(
                    ContentSettingsType.CONTENT_SETTINGS_TYPE_COOKIES);
        } else if (PREF_FULLSCREEN_PERMISSION.equals(preferenceKey)) {
            return Website.PermissionDataEntry.getPermissionDataEntry(
                    ContentSettingsType.CONTENT_SETTINGS_TYPE_FULLSCREEN);
        } else if (PREF_JAVASCRIPT_PERMISSION.equals(preferenceKey)) {
            return Website.PermissionDataEntry.getPermissionDataEntry(
                    ContentSettingsType.CONTENT_SETTINGS_TYPE_JAVASCRIPT);
        } else if (PREF_LOCATION_ACCESS.equals(preferenceKey)) {
            return Website.PermissionDataEntry.getPermissionDataEntry(
                    ContentSettingsType.CONTENT_SETTINGS_TYPE_GEOLOCATION);
        } else if (PREF_MIDI_SYSEX_PERMISSION.equals(preferenceKey)) {
            return Website.PermissionDataEntry.getPermissionDataEntry(
                    ContentSettingsType.CONTENT_SETTINGS_TYPE_MIDI_SYSEX);
        } else if (PREF_POPUP_PERMISSION.equals(preferenceKey)) {
            return Website.PermissionDataEntry.getPermissionDataEntry(
                    ContentSettingsType.CONTENT_SETTINGS_TYPE_POPUPS);
        } else if (PREF_PROTECTED_MEDIA_IDENTIFIER_PERMISSION.equals(preferenceKey)) {
            return Website.PermissionDataEntry.getPermissionDataEntry(
                    ContentSettingsType.CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER);
        } else if (PREF_PUSH_NOTIFICATIONS_PERMISSION.equals(preferenceKey)) {
            return Website.PermissionDataEntry.getPermissionDataEntry(
                    ContentSettingsType.CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
        } else if (PREF_VOICE_AND_VIDEO_CAPTURE_PERMISSION.equals(preferenceKey)) {
            return Website.PermissionDataEntry.getPermissionDataEntry(
                    ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM);
        }
        return null;
    }

    /**
     * Based on the type of media allowed or denied for this website, the title and summary
     * of the CheckBoxPreference will change. If this website has no media related permission, then
     * the preference will be removed.
     * @param preference CheckboxPreference whose title and summary will be set.
     */
    private void configureVoiceAndVideoPreference(Preference preference) {
        ContentSetting voice = mSite.getVoiceCapturePermission();
        ContentSetting video = mSite.getVideoCapturePermission();
        if (voice == null && video == null) {
            getPreferenceScreen().removePreference(preference);
            return;
        }

        int mediaAccessType = mSite.getMediaAccessType();
        switch (mediaAccessType) {
            case Website.CAMERA_ACCESS_ALLOWED:
            case Website.CAMERA_ACCESS_DENIED:
                preference.setTitle(R.string.video_permission_title);
                break;
            case Website.MICROPHONE_ACCESS_ALLOWED:
            case Website.MICROPHONE_ACCESS_DENIED:
                preference.setTitle(R.string.voice_permission_title);
                break;
            case Website.MICROPHONE_AND_CAMERA_ACCESS_ALLOWED:
            case Website.MICROPHONE_AND_CAMERA_ACCESS_DENIED:
                preference.setTitle(R.string.voice_and_video_permission_title);
                break;
            case Website.INVALID_CAMERA_OR_MICROPHONE_ACCESS:
            default:
                getPreferenceScreen().removePreference(preference);
        }
        setUpListPreference(
                preference, mediaAccessType == Website.CAMERA_ACCESS_ALLOWED
                        || mediaAccessType == Website.MICROPHONE_ACCESS_ALLOWED
                        || mediaAccessType == Website.MICROPHONE_AND_CAMERA_ACCESS_ALLOWED
                                ? ContentSetting.ALLOW :
                                  ContentSetting.BLOCK);
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        clearStoredData();
    }

    private void clearStoredData() {
        mSite.clearAllStoredData(
                new Website.StoredDataClearedCallback() {
                    @Override
                    public void onStoredDataCleared() {
                        PreferenceScreen preferenceScreen = getPreferenceScreen();
                        preferenceScreen.removePreference(
                                preferenceScreen.findPreference(PREF_CLEAR_DATA));
                        if (!hasUsagePreferences()) {
                            preferenceScreen.removePreference(
                                    preferenceScreen.findPreference(PREF_USAGE));
                        }
                        popBackIfNoSettings();
                    }
                });
    }

    private void popBackIfNoSettings() {
        if (!hasPermissionsPreferences() && !hasUsagePreferences()) {
            getActivity().finish();
        }
    }

    private void setVoiceAndVideoCaptureSetting(ContentSetting value) {
        int mediaAccessType = mSite.getMediaAccessType();
        if (value == ContentSetting.ALLOW) {
            switch (mediaAccessType) {
                case Website.CAMERA_ACCESS_DENIED:
                    mSite.setVideoCapturePermission(ContentSetting.ALLOW);
                    break;
                case Website.MICROPHONE_ACCESS_DENIED:
                    mSite.setVoiceCapturePermission(ContentSetting.ALLOW);
                    break;
                case Website.MICROPHONE_AND_CAMERA_ACCESS_DENIED:
                    mSite.setVideoCapturePermission(ContentSetting.ALLOW);
                    mSite.setVoiceCapturePermission(ContentSetting.ALLOW);
                    break;
                default:
                    assert false;
            }
        } else {
            switch (mediaAccessType) {
                case Website.CAMERA_ACCESS_ALLOWED:
                    mSite.setVideoCapturePermission(ContentSetting.BLOCK);
                    break;
                case Website.MICROPHONE_ACCESS_ALLOWED:
                    mSite.setVoiceCapturePermission(ContentSetting.BLOCK);
                    break;
                case Website.MICROPHONE_AND_CAMERA_ACCESS_ALLOWED:
                    mSite.setVideoCapturePermission(ContentSetting.BLOCK);
                    mSite.setVoiceCapturePermission(ContentSetting.BLOCK);
                    break;
                default:
                    assert false;
            }
        }
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        ContentSetting permission =
                ContentSetting.fromString((String) newValue);
        if (PREF_COOKIES_PERMISSION.equals(preference.getKey())) {
            mSite.setCookiePermission(permission);
        } else if (PREF_FULLSCREEN_PERMISSION.equals(preference.getKey())) {
            mSite.setFullscreenPermission(permission);
        } else if (PREF_JAVASCRIPT_PERMISSION.equals(preference.getKey())) {
            mSite.setJavaScriptPermission(permission);
        } else if (PREF_LOCATION_ACCESS.equals(preference.getKey())) {
            mSite.setGeolocationPermission(permission);
        } else if (PREF_MIDI_SYSEX_PERMISSION.equals(preference.getKey())) {
            mSite.setMidiPermission(permission);
        } else if (PREF_POPUP_PERMISSION.equals(preference.getKey())) {
            mSite.setPopupPermission(permission);
        } else if (PREF_PROTECTED_MEDIA_IDENTIFIER_PERMISSION.equals(preference.getKey())) {
            mSite.setProtectedMediaIdentifierPermission(permission);
        } else if (PREF_PUSH_NOTIFICATIONS_PERMISSION.equals(preference.getKey())) {
            mSite.setPushNotificationPermission(permission);
        } else if (PREF_VOICE_AND_VIDEO_CAPTURE_PERMISSION.equals(preference.getKey())) {
            setVoiceAndVideoCaptureSetting(permission);
        } else {
            return true;
        }

        int index = permission == ContentSetting.ALLOW ? 0 : 1;
        preference.setSummary(mListPreferenceSummaries[index]);
        return true;
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        // Handle the Clear & Reset preference click by showing a confirmation.
        new AlertDialog.Builder(getActivity())
                .setTitle(R.string.website_reset)
                .setMessage(R.string.website_reset_confirmation)
                .setPositiveButton(R.string.website_reset, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        resetSite();
                    }
                })
                .setNegativeButton(R.string.cancel, null)
                .show();
        return true;
    }

    private void resetSite() {
        // Clear the screen.
        // TODO(mvanouwerkerk): Refactor this class so that it does not depend on the screen state
        // for its logic. This class should maintain its own data model, and only update the screen
        // after a change is made.
        PreferenceScreen screen = getPreferenceScreen();
        for (String key : PERMISSION_PREFERENCE_KEYS) {
            Preference preference = screen.findPreference(key);
            if (preference != null) screen.removePreference(preference);
        }

        // Clear the permissions.
        mSite.setCookiePermission(null);
        WebsitePreferenceBridge.nativeClearCookieData(mSite.getAddress().getOrigin());
        mSite.setFullscreenPermission(null);
        mSite.setGeolocationPermission(null);
        mSite.setJavaScriptPermission(null);
        mSite.setMidiPermission(null);
        mSite.setPopupPermission(null);
        mSite.setProtectedMediaIdentifierPermission(null);
        mSite.setPushNotificationPermission(null);
        mSite.setVideoCapturePermission(null);
        mSite.setVoiceCapturePermission(null);

        // Clear the storage and finish the activity if necessary.
        if (mSite.getTotalUsage() > 0) {
            clearStoredData();
        } else {
            // Clearing stored data implies popping back to parent menu if there
            // is nothing left to show. Therefore, we only need to explicitly
            // close the activity if there's no stored data to begin with.
            getActivity().finish();
        }
    }
}
