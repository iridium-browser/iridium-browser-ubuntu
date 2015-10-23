// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_TAB_SPECIFIC_CONTENT_SETTINGS_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_TAB_SPECIFIC_CONTENT_SETTINGS_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/content_settings/local_shared_objects_container.h"
#include "chrome/common/custom_handlers/protocol_handler.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/content_settings_usages_state.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/cookies/canonical_cookie.h"

class HostContentSettingsMap;

namespace content {
class RenderViewHost;
}

namespace net {
class CookieOptions;
}

// This class manages state about permissions, content settings, cookies and
// site data for a specific WebContents. It tracks which content was accessed
// and which content was blocked. Based on this it provides information about
// which types of content were accessed and blocked.
class TabSpecificContentSettings
    : public content::WebContentsObserver,
      public content_settings::Observer,
      public content::WebContentsUserData<TabSpecificContentSettings> {
 public:
  // Fields describing the current mic/camera state. If a page has attempted to
  // access a device, the XXX_ACCESSED bit will be set. If access was blocked,
  // XXX_BLOCKED will be set.
  enum MicrophoneCameraStateFlags {
    MICROPHONE_CAMERA_NOT_ACCESSED = 0,
    MICROPHONE_ACCESSED = 1 << 0,
    MICROPHONE_BLOCKED = 1 << 1,
    CAMERA_ACCESSED = 1 << 2,
    CAMERA_BLOCKED = 1 << 3,
  };
  // Use signed int, that's what the enum flags implicitly convert to.
  typedef int32_t MicrophoneCameraState;

  // Classes that want to be notified about site data events must implement
  // this abstract class and add themselves as observer to the
  // |TabSpecificContentSettings|.
  class SiteDataObserver {
   public:
    explicit SiteDataObserver(
        TabSpecificContentSettings* tab_specific_content_settings);
    virtual ~SiteDataObserver();

    // Called whenever site data is accessed.
    virtual void OnSiteDataAccessed() = 0;

    TabSpecificContentSettings* tab_specific_content_settings() {
      return tab_specific_content_settings_;
    }

    // Called when the TabSpecificContentSettings is destroyed; nulls out
    // the local reference.
    void ContentSettingsDestroyed();

   private:
    TabSpecificContentSettings* tab_specific_content_settings_;

    DISALLOW_COPY_AND_ASSIGN(SiteDataObserver);
  };

  ~TabSpecificContentSettings() override;

  // Returns the object given a RenderFrameHost ids.
  static TabSpecificContentSettings* GetForFrame(int render_process_id,
                                                 int render_frame_id);

  // Static methods called on the UI threads.
  // Called when cookies for the given URL were read either from within the
  // current page or while loading it. |blocked_by_policy| should be true, if
  // reading cookies was blocked due to the user's content settings. In that
  // case, this function should invoke OnContentBlocked.
  static void CookiesRead(int render_process_id,
                          int render_frame_id,
                          const GURL& url,
                          const GURL& first_party_url,
                          const net::CookieList& cookie_list,
                          bool blocked_by_policy);

  // Called when a specific cookie in the current page was changed.
  // |blocked_by_policy| should be true, if the cookie was blocked due to the
  // user's content settings. In that case, this function should invoke
  // OnContentBlocked.
  static void CookieChanged(int render_process_id,
                            int render_frame_id,
                            const GURL& url,
                            const GURL& first_party_url,
                            const std::string& cookie_line,
                            const net::CookieOptions& options,
                            bool blocked_by_policy);

  // Called when a specific Web database in the current page was accessed. If
  // access was blocked due to the user's content settings,
  // |blocked_by_policy| should be true, and this function should invoke
  // OnContentBlocked.
  static void WebDatabaseAccessed(int render_process_id,
                                  int render_frame_id,
                                  const GURL& url,
                                  const base::string16& name,
                                  const base::string16& display_name,
                                  bool blocked_by_policy);

  // Called when a specific DOM storage area in the current page was
  // accessed. If access was blocked due to the user's content settings,
  // |blocked_by_policy| should be true, and this function should invoke
  // OnContentBlocked.
  static void DOMStorageAccessed(int render_process_id,
                                 int render_frame_id,
                                 const GURL& url,
                                 bool local,
                                 bool blocked_by_policy);

  // Called when a specific indexed db factory in the current page was
  // accessed. If access was blocked due to the user's content settings,
  // |blocked_by_policy| should be true, and this function should invoke
  // OnContentBlocked.
  static void IndexedDBAccessed(int render_process_id,
                                int render_frame_id,
                                const GURL& url,
                                const base::string16& description,
                                bool blocked_by_policy);

  // Called when a specific file system in the current page was accessed.
  // If access was blocked due to the user's content settings,
  // |blocked_by_policy| should be true, and this function should invoke
  // OnContentBlocked.
  static void FileSystemAccessed(int render_process_id,
                                 int render_frame_id,
                                 const GURL& url,
                                 bool blocked_by_policy);

  // Called when a specific Service Worker scope was accessed.
  // If access was blocked due to the user's content settings,
  // |blocked_by_policy| should be true, and this function should invoke
  // OnContentBlocked.
  static void ServiceWorkerAccessed(int render_process_id,
                                    int render_frame_id,
                                    const GURL& scope,
                                    bool blocked_by_policy);

  // Resets the |content_blocked_| and |content_allowed_| arrays, except for
  // CONTENT_SETTINGS_TYPE_COOKIES related information.
  // TODO(vabr): Only public for tests. Move to a test client.
  void ClearBlockedContentSettingsExceptForCookies();

  // Resets all cookies related information.
  // TODO(vabr): Only public for tests. Move to a test client.
  void ClearCookieSpecificContentSettings();

  // Changes the |content_blocked_| entry for popups.
  void SetPopupsBlocked(bool blocked);

  // Changes the |content_blocked_| entry for downloads.
  void SetDownloadsBlocked(bool blocked);

  // Returns whether a particular kind of content has been blocked for this
  // page.
  bool IsContentBlocked(ContentSettingsType content_type) const;

  // Returns true if content blockage was indicated to the user.
  bool IsBlockageIndicated(ContentSettingsType content_type) const;

  void SetBlockageHasBeenIndicated(ContentSettingsType content_type);

  // Returns whether a particular kind of content has been allowed. Currently
  // only tracks cookies.
  bool IsContentAllowed(ContentSettingsType content_type) const;

  // Returns the names of plugins that have been blocked for this tab.
  const std::vector<base::string16>& blocked_plugin_names() const {
    return blocked_plugin_names_;
  }

  const GURL& media_stream_access_origin() const {
    return media_stream_access_origin_;
  }

  const std::string& media_stream_requested_audio_device() const {
    return media_stream_requested_audio_device_;
  }

  const std::string& media_stream_requested_video_device() const {
    return media_stream_requested_video_device_;
  }

  // TODO(vabr): Only public for tests. Move to a test client.
  const std::string& media_stream_selected_audio_device() const {
    return media_stream_selected_audio_device_;
  }

  // TODO(vabr): Only public for tests. Move to a test client.
  const std::string& media_stream_selected_video_device() const {
    return media_stream_selected_video_device_;
  }

  // Returns the state of the camera and microphone usage.
  // The return value always includes all active media capture devices, on top
  // of the devices from the last request.
  MicrophoneCameraState GetMicrophoneCameraState() const;

  // Returns whether the camera or microphone permission or media device setting
  // has changed since the last permission request.
  bool IsMicrophoneCameraStateChanged() const;

  // Returns the ContentSettingsUsagesState that controls the
  // geolocation API usage on this page.
  const ContentSettingsUsagesState& geolocation_usages_state() const {
    return geolocation_usages_state_;
  }

  // Returns the ContentSettingsUsageState that controls the MIDI usage on
  // this page.
  const ContentSettingsUsagesState& midi_usages_state() const {
    return midi_usages_state_;
  }

  // Call to indicate that there is a protocol handler pending user approval.
  void set_pending_protocol_handler(const ProtocolHandler& handler) {
    pending_protocol_handler_ = handler;
  }

  const ProtocolHandler& pending_protocol_handler() const {
    return pending_protocol_handler_;
  }

  void ClearPendingProtocolHandler() {
    pending_protocol_handler_ = ProtocolHandler::EmptyProtocolHandler();
  }

  // Sets the previous protocol handler which will be replaced by the
  // pending protocol handler.
  void set_previous_protocol_handler(const ProtocolHandler& handler) {
    previous_protocol_handler_ = handler;
  }

  const ProtocolHandler& previous_protocol_handler() const {
    return previous_protocol_handler_;
  }

  // Set whether the setting for the pending handler is DEFAULT (ignore),
  // ALLOW, or DENY.
  void set_pending_protocol_handler_setting(ContentSetting setting) {
    pending_protocol_handler_setting_ = setting;
  }

  ContentSetting pending_protocol_handler_setting() const {
    return pending_protocol_handler_setting_;
  }

  // Returns the |LocalSharedObjectsCounter| instances corresponding to all
  // allowed, and blocked, respectively, local shared objects like cookies,
  // local storage, ... .
  const LocalSharedObjectsCounter& allowed_local_shared_objects() const {
    return allowed_local_shared_objects_;
  }

  const LocalSharedObjectsCounter& blocked_local_shared_objects() const {
    return blocked_local_shared_objects_;
  }

  // Creates a new copy of a CookiesTreeModel for all allowed, and blocked,
  // respectively, local shared objects.
  scoped_ptr<CookiesTreeModel> CreateAllowedCookiesTreeModel() const {
    return allowed_local_shared_objects_.CreateCookiesTreeModel();
  }

  scoped_ptr<CookiesTreeModel> CreateBlockedCookiesTreeModel() const {
    return blocked_local_shared_objects_.CreateCookiesTreeModel();
  }

  bool load_plugins_link_enabled() { return load_plugins_link_enabled_; }
  void set_load_plugins_link_enabled(bool enabled) {
    load_plugins_link_enabled_ = enabled;
  }

  // Called to indicate whether access to the Pepper broker was allowed or
  // blocked.
  void SetPepperBrokerAllowed(bool allowed);

  // Message handlers.
  // TODO(vabr): Only public for tests. Move to a test client.
  void OnContentBlocked(ContentSettingsType type);
  void OnContentBlockedWithDetail(ContentSettingsType type,
                                  const base::string16& details);
  void OnContentAllowed(ContentSettingsType type);

  // These methods are invoked on the UI thread by the static functions above.
  // TODO(vabr): Only public for tests. Move to a test client.
  void OnCookiesRead(const GURL& url,
                     const GURL& first_party_url,
                     const net::CookieList& cookie_list,
                     bool blocked_by_policy);
  void OnCookieChanged(const GURL& url,
                       const GURL& first_party_url,
                       const std::string& cookie_line,
                       const net::CookieOptions& options,
                       bool blocked_by_policy);
  void OnFileSystemAccessed(const GURL& url,
                            bool blocked_by_policy);
  void OnIndexedDBAccessed(const GURL& url,
                           const base::string16& description,
                           bool blocked_by_policy);
  void OnLocalStorageAccessed(const GURL& url,
                              bool local,
                              bool blocked_by_policy);
  void OnServiceWorkerAccessed(const GURL& scope, bool blocked_by_policy);
  void OnWebDatabaseAccessed(const GURL& url,
                             const base::string16& name,
                             const base::string16& display_name,
                             bool blocked_by_policy);
  void OnGeolocationPermissionSet(const GURL& requesting_frame,
                                  bool allowed);
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  void OnProtectedMediaIdentifierPermissionSet(const GURL& requesting_frame,
                                               bool allowed);
#endif

  // This method is called to update the status about the microphone and
  // camera stream access.
  void OnMediaStreamPermissionSet(
      const GURL& request_origin,
      MicrophoneCameraState new_microphone_camera_state,
      const std::string& media_stream_selected_audio_device,
      const std::string& media_stream_selected_video_device,
      const std::string& media_stream_requested_audio_device,
      const std::string& media_stream_requested_video_device);

  // There methods are called to update the status about MIDI access.
  void OnMidiSysExAccessed(const GURL& reqesting_origin);
  void OnMidiSysExAccessBlocked(const GURL& requesting_origin);

  // Adds the given |SiteDataObserver|. The |observer| is notified when a
  // locale shared object, like for example a cookie, is accessed.
  void AddSiteDataObserver(SiteDataObserver* observer);

  // Removes the given |SiteDataObserver|.
  void RemoveSiteDataObserver(SiteDataObserver* observer);

 private:
  friend class content::WebContentsUserData<TabSpecificContentSettings>;

  explicit TabSpecificContentSettings(content::WebContents* tab);

  // content::WebContentsObserver overrides.
  void RenderFrameForInterstitialPageCreated(
      content::RenderFrameHost* render_frame_host) override;
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;
  void DidStartProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc) override;
  void AppCacheAccessed(const GURL& manifest_url,
                        bool blocked_by_policy) override;

  // content_settings::Observer implementation.
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               std::string resource_identifier) override;

  // Notifies all registered |SiteDataObserver|s.
  void NotifySiteDataObservers();

  // Clears the Geolocation settings.
  void ClearGeolocationContentSettings();

  // Clears the MIDI settings.
  void ClearMidiContentSettings();

  // Updates Geolocation settings on navigation.
  void GeolocationDidNavigate(
      const content::LoadCommittedDetails& details);

  // Updates MIDI settings on navigation.
  void MidiDidNavigate(const content::LoadCommittedDetails& details);

  // All currently registered |SiteDataObserver|s.
  base::ObserverList<SiteDataObserver> observer_list_;

  // Stores which content setting types actually have blocked content.
  bool content_blocked_[CONTENT_SETTINGS_NUM_TYPES];

  // Stores if the blocked content was messaged to the user.
  bool content_blockage_indicated_to_user_[CONTENT_SETTINGS_NUM_TYPES];

  // Stores which content setting types actually were allowed.
  bool content_allowed_[CONTENT_SETTINGS_NUM_TYPES];

  // Stores the blocked/allowed cookies.
  LocalSharedObjectsContainer allowed_local_shared_objects_;
  LocalSharedObjectsContainer blocked_local_shared_objects_;

  // Manages information about Geolocation API usage in this page.
  ContentSettingsUsagesState geolocation_usages_state_;

  // Manages information about MIDI usages in this page.
  ContentSettingsUsagesState midi_usages_state_;

  // The pending protocol handler, if any. This can be set if
  // registerProtocolHandler was invoked without user gesture.
  // The |IsEmpty| method will be true if no protocol handler is
  // pending registration.
  ProtocolHandler pending_protocol_handler_;

  // The previous protocol handler to be replaced by
  // the pending_protocol_handler_, if there is one. Empty if
  // there is no handler which would be replaced.
  ProtocolHandler previous_protocol_handler_;

  // The setting on the pending protocol handler registration. Persisted in case
  // the user opens the bubble and makes changes multiple times.
  ContentSetting pending_protocol_handler_setting_;

  // The name(s) of the plugin(s) being blocked.
  std::vector<base::string16> blocked_plugin_names_;

  // Stores whether the user can load blocked plugins on this page.
  bool load_plugins_link_enabled_;

  // The origin of the media stream request. Note that we only support handling
  // settings for one request per tab. The latest request's origin will be
  // stored here. http://crbug.com/259794
  GURL media_stream_access_origin_;

  // The microphone and camera state at the last media stream request.
  MicrophoneCameraState microphone_camera_state_;
  // The selected devices at the last media stream request.
  std::string media_stream_selected_audio_device_;
  std::string media_stream_selected_video_device_;

  // The devices to be displayed in the media bubble when the media stream
  // request is requesting certain specific devices.
  std::string media_stream_requested_audio_device_;
  std::string media_stream_requested_video_device_;

  // Observer to watch for content settings changed.
  ScopedObserver<HostContentSettingsMap, content_settings::Observer> observer_;

  DISALLOW_COPY_AND_ASSIGN(TabSpecificContentSettings);
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_TAB_SPECIFIC_CONTENT_SETTINGS_H_
