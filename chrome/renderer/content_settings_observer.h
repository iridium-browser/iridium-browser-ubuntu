// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CONTENT_SETTINGS_OBSERVER_H_
#define CHROME_RENDERER_CONTENT_SETTINGS_OBSERVER_H_

#include <map>
#include <set>

#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "third_party/WebKit/public/web/WebContentSettingsClient.h"

class GURL;

namespace blink {
class WebFrame;
class WebSecurityOrigin;
class WebURL;
}

namespace extensions {
class Dispatcher;
class Extension;
}

// Handles blocking content per content settings for each RenderFrame.
class ContentSettingsObserver
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<ContentSettingsObserver>,
      public blink::WebContentSettingsClient {
 public:
  // Set |should_whitelist| to true if |render_frame()| contains content that
  // should be whitelisted for content settings.
  ContentSettingsObserver(content::RenderFrame* render_frame,
                          extensions::Dispatcher* extension_dispatcher,
                          bool should_whitelist);
  ~ContentSettingsObserver() override;

  // Sets the content setting rules which back |AllowImage()|, |AllowScript()|,
  // and |AllowScriptFromSource()|. |content_setting_rules| must outlive this
  // |ContentSettingsObserver|.
  void SetContentSettingRules(
      const RendererContentSettingRules* content_setting_rules);

  bool IsPluginTemporarilyAllowed(const std::string& identifier);

  // Sends an IPC notification that the specified content type was blocked.
  void DidBlockContentType(ContentSettingsType settings_type);

  // Sends an IPC notification that the specified content type was blocked
  // with additional metadata.
  void DidBlockContentType(ContentSettingsType settings_type,
                           const base::string16& details);

  // blink::WebContentSettingsClient implementation.
  virtual bool allowDatabase(const blink::WebString& name,
                             const blink::WebString& display_name,
                             unsigned long estimated_size);
  virtual void requestFileSystemAccessAsync(
      const blink::WebContentSettingCallbacks& callbacks);
  virtual bool allowImage(bool enabled_per_settings,
                          const blink::WebURL& image_url);
  virtual bool allowIndexedDB(const blink::WebString& name,
                              const blink::WebSecurityOrigin& origin);
  virtual bool allowPlugins(bool enabled_per_settings);
  virtual bool allowScript(bool enabled_per_settings);
  virtual bool allowScriptFromSource(bool enabled_per_settings,
                                     const blink::WebURL& script_url);
  virtual bool allowStorage(bool local);
  virtual bool allowReadFromClipboard(bool default_value);
  virtual bool allowWriteToClipboard(bool default_value);
  virtual bool allowMutationEvents(bool default_value);
  virtual void didNotAllowPlugins();
  virtual void didNotAllowScript();
  virtual bool allowDisplayingInsecureContent(
      bool allowed_per_settings,
      const blink::WebSecurityOrigin& context,
      const blink::WebURL& url);
  virtual bool allowRunningInsecureContent(
      bool allowed_per_settings,
      const blink::WebSecurityOrigin& context,
      const blink::WebURL& url);

  // This is used for cases when the NPAPI plugins malfunction if used.
  bool AreNPAPIPluginsBlocked() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(ContentSettingsObserverTest, WhitelistedSchemes);
  FRIEND_TEST_ALL_PREFIXES(ChromeRenderViewTest,
                           ContentSettingsInterstitialPages);
  FRIEND_TEST_ALL_PREFIXES(ChromeRenderViewTest, PluginsTemporarilyAllowed);

  // RenderFrameObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void DidCommitProvisionalLoad(bool is_new_navigation,
                                bool is_same_page_navigation) override;

  // Message handlers.
  void OnLoadBlockedPlugins(const std::string& identifier);
  void OnSetAsInterstitial();
  void OnNPAPINotSupported();
  void OnSetAllowDisplayingInsecureContent(bool allow);
  void OnSetAllowRunningInsecureContent(bool allow);
  void OnReloadFrame();
  void OnRequestFileSystemAccessAsyncResponse(int request_id, bool allowed);

  // Resets the |content_blocked_| array.
  void ClearBlockedContentSettings();

  // Whether the observed RenderFrame is for a platform app.
  bool IsPlatformApp();

#if defined(ENABLE_EXTENSIONS)
  // If |origin| corresponds to an installed extension, returns that extension.
  // Otherwise returns NULL.
  const extensions::Extension* GetExtension(
      const blink::WebSecurityOrigin& origin) const;
#endif

  // Helpers.
  // True if |render_frame()| contains content that is white-listed for content
  // settings.
  bool IsWhitelistedForContentSettings() const;
  static bool IsWhitelistedForContentSettings(
      const blink::WebSecurityOrigin& origin,
      const GURL& document_url);

#if defined(ENABLE_EXTENSIONS)
  // Owned by ChromeContentRendererClient and outlive us.
  extensions::Dispatcher* extension_dispatcher_;
#endif

  // Insecure content may be permitted for the duration of this render view.
  bool allow_displaying_insecure_content_;
  bool allow_running_insecure_content_;

  // A pointer to content setting rules stored by the renderer. Normally, the
  // |RendererContentSettingRules| object is owned by
  // |ChromeRenderProcessObserver|. In the tests it is owned by the caller of
  // |SetContentSettingRules|.
  const RendererContentSettingRules* content_setting_rules_;

  // Stores if images, scripts, and plugins have actually been blocked.
  bool content_blocked_[CONTENT_SETTINGS_NUM_TYPES];

  // Caches the result of AllowStorage.
  typedef std::pair<GURL, bool> StoragePermissionsKey;
  std::map<StoragePermissionsKey, bool> cached_storage_permissions_;

  // Caches the result of |AllowScript|.
  std::map<blink::WebFrame*, bool> cached_script_permissions_;

  std::set<std::string> temporarily_allowed_plugins_;
  bool is_interstitial_page_;
  bool npapi_plugins_blocked_;

  int current_request_id_;
  typedef std::map<int, blink::WebContentSettingCallbacks> PermissionRequestMap;
  PermissionRequestMap permission_requests_;

  // If true, IsWhitelistedForContentSettings will always return true.
  const bool should_whitelist_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingsObserver);
};

#endif  // CHROME_RENDERER_CONTENT_SETTINGS_OBSERVER_H_
